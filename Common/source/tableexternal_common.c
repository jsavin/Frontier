#include <stdio.h>
#include <string.h>

#include "frontier.h"
#include "standard.h"

#include "db.h"
#include "lang.h"
#include "langexternal.h"
#include "langinternal.h"
#include "tableinternal.h"
#include "dbinternal.h"
#include "db_format.h"
#include "strings.h"
#include "memory.h"
#include "tableexternal_common.h"

// 2025-10-27 Codex: Log unpack errors while diagnosing headless system table loading.
// 2025-11-14 Codex: Rebuild the legacy table converter so v6 payloads without merge
// prefixes are reconstructed deterministically (header + records + strings + formats).
// 2025-11-28 Codex: Use db_context when dereferencing externals during headless packing.

#if defined(FRONTIER_HEADLESS)
#ifndef TABLE_HEADER_RESERVED_BYTES
#define TABLE_HEADER_RESERVED_BYTES 1024
#endif

#ifndef TABLE_HEADER_RESERVED_VERSION
#define TABLE_HEADER_RESERVED_VERSION 4
#endif

static uint32_t headless_read_be32(const unsigned char *p) {
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |
            (uint32_t)p[3];
}

static size_t headless_calc_header_span(const unsigned char *header, size_t available) {
    size_t span = 16; /* sizeof(tydisktablerecord) */
    if (available >= span + TABLE_HEADER_RESERVED_BYTES) {
        int16_t version = (int16_t)(((int16_t)header[0] << 8) | header[1]);
        if (version >= TABLE_HEADER_RESERVED_VERSION)
            span += TABLE_HEADER_RESERVED_BYTES;
    }
    if (span > available)
        span = available;
    return span;
}

static boolean headless_payload_looks_modern(const unsigned char *payload, size_t payload_len) {
    if ((payload == NULL) || (payload_len < sizeof(uint32_t) * 2))
        return false;

    uint32_t outer_len = headless_read_be32(payload);
    size_t outer_total = sizeof(uint32_t) + (size_t) outer_len;
    if (outer_total > payload_len)
        return false;
    if (outer_len < sizeof(uint32_t))
        return false;

    const unsigned char *outer_first = payload + sizeof(uint32_t);
    uint32_t inner_len = headless_read_be32(outer_first);
    size_t inner_total = sizeof(uint32_t) + (size_t) inner_len;
    if (inner_total > outer_len)
        return false;
    if (inner_len < 16)
        return false;

    return true;
}

static boolean headless_convert_legacy_table_payload(const unsigned char *payload, size_t payload_len, Handle *hout) {
    const size_t legacy_header_size = 16; /* sizeof(tydisktablerecord) */
    const size_t legacy_record_size = 10; /* sizeof(tydisksymbolrecord) */

    if ((payload == NULL) || (hout == NULL))
        return false;

    if (payload_len < legacy_header_size)
        return false;

    const unsigned char *header_src = NULL;
    size_t header_bytes = legacy_header_size;
    const unsigned char *records_only_src = NULL;
    size_t records_only_len = 0;
    const unsigned char *strings_src = NULL;
    size_t strings_len = 0;
    const unsigned char *formats_src = NULL;
    size_t formats_len = 0;
    boolean layout_ready = false;

    const unsigned char *outer_first = payload;
    size_t outer_first_len = payload_len;
    const unsigned char *outer_second = NULL;
    size_t outer_second_len = 0;

    if (payload_len >= sizeof(uint32_t)) {
        uint32_t first_len = headless_read_be32(payload);
        if ((size_t) first_len + sizeof(uint32_t) <= payload_len) {
            outer_first = payload + sizeof(uint32_t);
            outer_first_len = first_len;
            outer_second_len = payload_len - sizeof(uint32_t) - first_len;
            if (outer_second_len > 0)
                outer_second = payload + sizeof(uint32_t) + first_len;
        }
    }

    if (outer_first_len >= sizeof(uint32_t)) {
        uint32_t inner_len = headless_read_be32(outer_first);
        if ((size_t) inner_len + sizeof(uint32_t) <= outer_first_len) {
            const unsigned char *inner_body = outer_first + sizeof(uint32_t);
            size_t strings_available = outer_first_len - sizeof(uint32_t) - inner_len;
            if (inner_len >= legacy_header_size) {
                size_t candidate_header_bytes = headless_calc_header_span(inner_body, inner_len);
                if (inner_len >= candidate_header_bytes) {
                    size_t rec_bytes = inner_len - candidate_header_bytes;
                    if ((rec_bytes % legacy_record_size) == 0) {
                        header_src = inner_body;
                        header_bytes = candidate_header_bytes;
                        records_only_src = inner_body + candidate_header_bytes;
                        records_only_len = rec_bytes;
                        strings_src = inner_body + inner_len;
                        strings_len = strings_available;
                        formats_src = outer_second;
                        formats_len = outer_second_len;
                        layout_ready = true;
#if defined(FRONTIER_HEADLESS)
                        fprintf(stderr, "[headless] legacy table lengths path header=%zu records=%zu strings=%zu formats=%zu\n",
                                header_bytes,
                                records_only_len / legacy_record_size,
                                strings_len,
                                formats_len);
#endif
                    }
                }
            }
        }
    }

    if (!layout_ready) {
        size_t header_span = headless_calc_header_span(payload, payload_len);
        size_t records_start = 0;
        size_t records_end = 0;
        size_t derived_strings_len = 0;
        size_t formats_start = payload_len;

        if (payload_len >= legacy_record_size) {
            for (size_t pos = payload_len - legacy_record_size;;) {
                if (memcmp(payload + pos, "\0\0\0\0\0\0\0\0\0\0", legacy_record_size) == 0) {
                    records_end = pos;
                    formats_start = pos + legacy_record_size;
                    break;
                }
                if (pos == 0)
                    break;
                pos--;
            }
        }

        if (records_end > header_span) {
            size_t strings_upper_bound = records_end - header_span;
            size_t pos = records_end;
            size_t record_count = 0;
            while (pos >= header_span + legacy_record_size) {
                size_t next_pos = pos - legacy_record_size;
                const unsigned char *rec = payload + next_pos;
                uint32_t ixkey = headless_read_be32(rec);
                if (ixkey >= strings_upper_bound)
                    break;
                records_start = next_pos;
                pos = next_pos;
                record_count++;
            }

            if (record_count == 0)
                records_end = 0;
            else
                derived_strings_len = (records_start > header_span) ? (records_start - header_span) : 0;
        }

        if (records_end == 0) {
            for (size_t candidate_strings = 0; candidate_strings <= payload_len - header_span; candidate_strings++) {
                size_t candidate_records_start = header_span + candidate_strings;

                if ((candidate_records_start - header_span) % legacy_record_size != 0)
                    continue;

                if (candidate_records_start + legacy_record_size > payload_len)
                    break;

                boolean valid = true;
                size_t record_count = 0;

                for (size_t rec_offset = candidate_records_start; rec_offset + legacy_record_size <= payload_len; rec_offset += legacy_record_size) {
                    const unsigned char *rec = payload + rec_offset;
                    uint32_t ixkey = headless_read_be32(rec);
                    record_count++;

                    if (ixkey >= candidate_strings) {
                        valid = false;
                        break;
                    }
                }

                if (valid && record_count > 0) {
                    derived_strings_len = candidate_strings;
                    records_start = candidate_records_start;
                    records_end = candidate_records_start + record_count * legacy_record_size;
                    formats_start = records_end;
#if defined(FRONTIER_HEADLESS)
                    fprintf(stderr, "[headless] fallback split strings=%zu records=%zu tail=%zu\n",
                            derived_strings_len,
                            (records_end - records_start) / legacy_record_size,
                            payload_len > formats_start ? payload_len - formats_start : 0);
#endif
                    break;
                }
            }
        }

        if (records_start > header_span && records_end > records_start) {
            header_src = payload;
            header_bytes = header_span;
            records_only_src = payload + records_start;
            records_only_len = records_end - records_start;
            strings_src = payload + header_span;
            strings_len = derived_strings_len;
            formats_src = (formats_start < payload_len) ? (payload + formats_start) : NULL;
            formats_len = (formats_start < payload_len) ? (payload_len - formats_start) : 0;
            layout_ready = true;
        }
    }

    if (!layout_ready)
        return false;

    Handle h1 = nil;
    Handle h2 = nil;

    size_t h1_size = header_bytes + records_only_len;
    if (!newhandle((long)h1_size, &h1))
        return false;

    memcpy(*h1, header_src, header_bytes);
    if (records_only_len > 0)
        memcpy(*h1 + header_bytes, records_only_src, records_only_len);

    if (strings_len > 0) {
        if (!newhandle((long)strings_len, &h2)) {
            disposehandle(h1);
            return false;
        }
        memcpy(*h2, strings_src, strings_len);
    }

    Handle h_inner_merged = nil;
    if (!mergehandles(h1, h2, &h_inner_merged)) {
        if (h1) disposehandle(h1);
        if (h2) disposehandle(h2);
        return false;
    }

    Handle h_formats = nil;
    if (formats_len > 0 && formats_src != NULL) {
        if (!newhandle((long)formats_len, &h_formats)) {
            disposehandle(h_inner_merged);
            return false;
        }
        memcpy(*h_formats, formats_src, formats_len);
    }

    if (!mergehandles(h_inner_merged, h_formats, hout)) {
        if (h_inner_merged) disposehandle(h_inner_merged);
        if (h_formats) disposehandle(h_formats);
        return false;
    }

    return true;
}
#endif /* FRONTIER_HEADLESS */

boolean tableverbinmemory_common(hdlexternalvariable hvariable, hdlhashnode hnode) {
    register hdltablevariable hv = (hdltablevariable) hvariable;
    Handle hpacked;
    hdlhashtable htable = nil;
    dbaddress adr;
    langerrormessagecallback savecallback;
    ptrvoid saverefcon;
    hdlhashtable hparent;
    bigstring bspath, bsunpackerror;
    boolean fl;

    if ((**hv).flinmemory) /* nothing to do, it's already in memory */
        return true;

    if ((hnode == nil) || (hnode == HNoNode))
        hnode = nil;

    dbpushdatabase((**hv).hdatabase);

    adr = (dbaddress) (**hv).variabledata;
    long payload_offset = 0;

#if defined(FRONTIER_HEADLESS)
    if ((**hv).hdatabase == nil) {
        fprintf(stderr, "[headless] tableverbinmemory nil database for variable adr=0x%llx\n",
                (unsigned long long) adr);
    }
#endif

#if defined(FRONTIER_HEADLESS)
    {
        dbaddress normalized = adr;
        if (dbnormalizeaddress(&normalized)) {
            if (normalized != adr) {
                dbaddress data_start = normalized + sizeheader;
                if (adr > data_start)
                    payload_offset = (long) (adr - data_start);
                adr = normalized;
            }
        } else {
            fprintf(stderr, "[headless] dbnormalizeaddress failed for adr=0x%llx\n", (unsigned long long) adr);
        }
    }
#endif

    if (adr == nildbaddress) { /* table has never been allocated */
#if defined(FRONTIER_HEADLESS)
        fprintf(stderr, "[headless] tableverbinmemory nil table address (never saved)\n");
#endif
        shellinternalerror(idniltableaddress, BIGSTRING ("\x2b" "nil table address.  (Creating empty table.)"));
        fl = false;
    } else {
        db_context context;
        db_context_init(&context);
        fl = dbrefhandle_context(&context, adr, &hpacked);

#if defined(FRONTIER_HEADLESS)
        if (!fl) {
            fprintf(stderr, "[headless] dbrefhandle failed adr=0x%llx\n", (unsigned long long)adr);
        } else {
            long hsize_long = gethandlesize(hpacked);
            fprintf(stderr, "[headless] dbrefhandle ok adr=0x%llx size=%ld\n",
                    (unsigned long long)adr, hsize_long);
            if (payload_offset > 0 && payload_offset < hsize_long) {
                pullfromhandle(hpacked, 0, payload_offset, nil);
                fprintf(stderr, "[headless] trimmed leading %ld bytes from packed table\n", payload_offset);
            }
            if (hsize_long > 0) {
                size_t dump = hsize_long < 32 ? (size_t) hsize_long : 32;
                unsigned char *bytes = (unsigned char *) *hpacked;
                fprintf(stderr, "[headless] hpacked first bytes:");
                for (size_t i = 0; i < dump; ++i)
                    fprintf(stderr, " %02x", bytes[i]);
                fprintf(stderr, "\n");
            }

            if (fl) {
                size_t hsize = (size_t) hsize_long;
                unsigned char *bytes = (unsigned char *) *hpacked;
                if (!headless_payload_looks_modern(bytes, hsize)) {
                    fprintf(stderr, "[headless] legacy table payload detected len=%zu\n", hsize);
                    Handle hlegacy = nil;
                    if (headless_convert_legacy_table_payload(bytes, hsize, &hlegacy)) {
                        disposehandle(hpacked);
                        hpacked = hlegacy;
                        fprintf(stderr, "[headless] converted legacy table payload to merged handle\n");
                    } else {
                        fprintf(stderr, "[headless] legacy table conversion failed\n");
                    }
                }
            }
        }
#endif

        if (fl) {
            langtraperrors(bsunpackerror, &savecallback, &saverefcon);

            fl = tableunpacktable(hpacked, false, &htable); /* always disposes of hpackedtable */

            languntraperrors(savecallback, saverefcon, !fl);

            if (!fl) {
                fllangerror = false;

#if defined(FRONTIER_HEADLESS)
                fprintf(stderr, "[headless] tableunpacktable failed adr=0x%llx\n", (unsigned long long)adr);
                {
                    char errbuf[256];
                    short errlen = stringlength(bsunpackerror);
                    short copylen = (errlen < (short)sizeof(errbuf) - 1) ? errlen : (short)sizeof(errbuf) - 1;
                    memmove(errbuf, stringbaseaddress(bsunpackerror), copylen);
                    errbuf[copylen] = '\0';
                    fprintf(stderr, "[headless] tableunpacktable error: %s\n", errbuf);
                }
#endif

                if (langexternalfindvariable((hdlexternalvariable) hv, &hparent, bspath) &&
                    langexternalgetfullpath(hparent, bspath, bspath, nil)) {
                    poptrailingchars(bsunpackerror, '.');
                    lang2paramerror(tableloadingerror, bspath, bsunpackerror);
                } else
                    langerrormessage(bsunpackerror);
            }
#if defined(FRONTIER_HEADLESS)
            else {
                fprintf(stderr, "[headless] tableunpacktable ok adr=0x%llx\n", (unsigned long long)adr);
            }
#endif
        }
    }

    dbpopdatabase();

    if (!fl)
        return false;

    (**hv).flinmemory = true;

    (**hv).variabledata = (long) htable; /* link into variable structure */

    (**hv).oldaddress = adr; /* last place this table was stored */

#if defined(FRONTIER_HEADLESS)
    {
        long ctitems = 0;
        hashcountitems(htable, &ctitems);
        fprintf(stderr, "[headless] tableverbinmemory loaded table with %ld items (adr=0x%llx)%s\n",
                ctitems,
                (unsigned long long)adr,
                (adr == (**hv).oldaddress) ? "" : " *oldaddr mismatch*");
        if (ctitems > 0) {
            hdlhashnode dump = (**htable).hfirstsort;
            int limit = (ctitems < 20) ? (int)ctitems : 20; /*dump everything for small tables*/
            while (dump != nil && limit-- > 0) {
                bigstring bsdump;
                gethashkey(dump, bsdump);
                short len = stringlength(bsdump);
                char cname[256];
                short copylen = (len < (short)sizeof(cname)-1) ? len : (short)sizeof(cname)-1;
                memmove(cname, stringbaseaddress(bsdump), copylen);
                cname[copylen] = '\0';
                fprintf(stderr, "[headless]   entry %s valuetype=%d dontsave=%d\n",
                        cname, (**dump).val.valuetype, (int)(**dump).fldontsave);
                dump = (**dump).sortedlink;
            }
        }
    }
#endif

    if ((**hv).flmayaffectdisplay)
        (**htable).flmayaffectdisplay = true;

#ifdef xmlfeatures
    (**htable).flxml = (**hv).flxml; //5.0.1
#endif

    (**htable).hashtablerefcon = (long) hv; /* we can get from hashtable to variable rec */

    (**htable).thistableshashnode = hnode; /* The var rec is contained in the hashnode... RAB 1/3/00 */

    return true;
}
