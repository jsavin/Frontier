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
#include "strings.h"
#include "memory.h"
#include "tableexternal_common.h"

#if defined(FRONTIER_HEADLESS)
static uint32_t headless_read_be32(const unsigned char *p) {
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |
            (uint32_t)p[3];
}

static boolean headless_convert_legacy_table_payload(const unsigned char *payload, size_t payload_len, Handle *hout) {
    const size_t legacy_header_size = 16; /* sizeof(tydisktablerecord) */
    const size_t legacy_record_size = 10; /* sizeof(tydisksymbolrecord) */

    if ((payload == NULL) || (hout == NULL))
        return false;

    if (payload_len < legacy_header_size + legacy_record_size)
        return false;

    /* The legacy format is [header][strings][records][sentinel] WITHOUT the merge prefix.
     * The modern format expects [size][header+records][strings].
     * We need to find where strings end and records begin, then reorganize.
     * Records are 10 bytes each. Each record has an ixkey field (first 4 bytes, big-endian)
     * that's an offset into the string pool.
     */

    size_t strings_len = 0;
    size_t records_start = 0;

    /* Try different string pool sizes. Records start after strings and are 10-byte aligned. */
    for (size_t candidate_strings = 0; candidate_strings <= payload_len - legacy_header_size; candidate_strings++) {
        size_t candidate_records_start = legacy_header_size + candidate_strings;

        /* Records must be aligned */
        if ((candidate_records_start - legacy_header_size) % legacy_record_size != 0)
            continue;

        if (candidate_records_start + legacy_record_size > payload_len)
            break;  /* Not enough room for even one record */

        boolean valid = true;
        boolean found_sentinel = false;
        size_t record_count = 0;

        /* Check all potential records from candidate_records_start to end */
        for (size_t rec_offset = candidate_records_start; rec_offset + legacy_record_size <= payload_len; rec_offset += legacy_record_size) {
            const unsigned char *rec = payload + rec_offset;
            uint32_t ixkey = headless_read_be32(rec);
            uint8_t valuetype = rec[4];
            uint8_t version = rec[5];
            uint32_t dataval = headless_read_be32(rec + 6);

            record_count++;

            /* Check for sentinel (all zeros) */
            if ((ixkey == 0) && (valuetype == 0) && (version == 0) && (dataval == 0)) {
                found_sentinel = true;
                continue;  /* Sentinel is valid, keep checking following records */
            }

            /* ixkey must be a valid offset into the string pool */
            if (ixkey >= candidate_strings) {
                valid = false;
                break;
            }
        }

        if (valid && found_sentinel && record_count > 0) {
            strings_len = candidate_strings;
            records_start = candidate_records_start;
            fprintf(stderr, "[headless] found valid split: strings=%zu records_start=%zu records=%zu\n",
                    strings_len, records_start, record_count);
            break;
        }
    }

    if (records_start == 0)
        return false;  /* Couldn't find valid split point */

    /* Now create handles in the modern format: h1=[header+records], h2=[strings] */
    size_t records_len = payload_len - records_start;
    size_t h1_size = legacy_header_size + records_len;

    Handle h1 = nil;
    Handle h2 = nil;

    if (!newhandle((long)h1_size, &h1))
        return false;

    /* h1 = header + records */
    memcpy(*h1, payload, legacy_header_size);  /* Copy header */
    memcpy(*h1 + legacy_header_size, payload + records_start, records_len);  /* Copy records */

    /* h2 = strings */
    if (strings_len > 0) {
        if (!newhandle((long)strings_len, &h2)) {
            disposehandle(h1);
            return false;
        }
        memcpy(*h2, payload + legacy_header_size, strings_len);  /* Copy strings */
    }

    /* Use mergehandles to create the inner merged table (what hashpacktable creates) */
    Handle h_inner_merged = nil;
    if (!mergehandles(h1, h2, &h_inner_merged)) {
        /* mergehandles consumes h1 and h2 on success, but we need to clean up on failure */
        if (h1) disposehandle(h1);
        if (h2) disposehandle(h2);
        return false;
    }

    /* Now merge again with empty formats to create the outer structure (what tablepacktable creates) */
    Handle h_formats = nil;  /* No formats in legacy payload */
    if (!mergehandles(h_inner_merged, h_formats, hout)) {
        if (h_inner_merged) disposehandle(h_inner_merged);
        return false;
    }

    fprintf(stderr, "[headless] created two-level merged handle\n");
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
        shellinternalerror(idniltableaddress, BIGSTRING ("\x2b" "nil table address.  (Creating empty table.)"));
        fl = false;
    } else {
        fl = dbrefhandle(adr, &hpacked);

#if defined(FRONTIER_HEADLESS)
        if (!fl) {
            fprintf(stderr, "[headless] dbrefhandle failed adr=0x%llx\n", (unsigned long long)adr);
        } else {
            fprintf(stderr, "[headless] dbrefhandle ok adr=0x%llx size=%ld\n",
                    (unsigned long long)adr, gethandlesize(hpacked));
            if (payload_offset > 0 && payload_offset < gethandlesize(hpacked)) {
                pullfromhandle(hpacked, 0, payload_offset, nil);
                fprintf(stderr, "[headless] trimmed leading %ld bytes from packed table\n", payload_offset);
            }
            if (gethandlesize(hpacked) > 0) {
                size_t dump = gethandlesize(hpacked) < 32 ? gethandlesize(hpacked) : 32;
                unsigned char *bytes = (unsigned char *) *hpacked;
                fprintf(stderr, "[headless] hpacked first bytes:");
                for (size_t i = 0; i < dump; ++i)
                    fprintf(stderr, " %02x", bytes[i]);
                fprintf(stderr, "\n");
            }

            if (fl) {
                size_t hsize = (size_t) gethandlesize(hpacked);
                if (hsize >= 4) {
                    unsigned char *bytes = (unsigned char *) *hpacked;
                    uint32_t prefix = headless_read_be32(bytes);
                    if ((prefix < 16) || (prefix > (hsize - 4))) {
                        fprintf(stderr, "[headless] detected legacy table payload prefix=0x%08x len=%zu\n", prefix, hsize);
                        Handle hlegacy = nil;
                        if (headless_convert_legacy_table_payload(bytes, hsize, &hlegacy)) {
                            disposehandle(hpacked);
                            hpacked = hlegacy;
                            fprintf(stderr, "[headless] converted legacy table payload to merged handle\n");
                            size_t merged_size = (size_t) gethandlesize(hpacked);
                            unsigned char *merged_bytes = (unsigned char *) *hpacked;
                            uint32_t merged_len = headless_read_be32(merged_bytes);
                            fprintf(stderr, "[headless] merged len prefix=%u total=%zu\n", merged_len, merged_size);
                            size_t dump = merged_size < 32 ? merged_size : 32;
                            fprintf(stderr, "[headless] merged first bytes:");
                            for (size_t i = 0; i < dump; ++i)
                                fprintf(stderr, " %02x", merged_bytes[i]);
                            fprintf(stderr, "\n");
                        } else {
                            fprintf(stderr, "[headless] legacy table conversion failed\n");
                        }
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

    if ((**hv).flmayaffectdisplay)
        (**htable).flmayaffectdisplay = true;

#ifdef xmlfeatures
    (**htable).flxml = (**hv).flxml; //5.0.1
#endif

    (**htable).hashtablerefcon = (long) hv; /* we can get from hashtable to variable rec */

    (**htable).thistableshashnode = hnode; /* The var rec is contained in the hashnode... RAB 1/3/00 */

    return true;
}
