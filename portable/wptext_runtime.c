#include "frontier.h"
#include "langexternal.h"
#include "memory.h"
#include "db.h"
#include "strings.h"
#include "byteorder.h"
#include "timedate.h"
#include "wpverbs.h"
#include "wptext_portable.h"
#include "db_format.h"
#include "logging.h"
#include "processinternal.h"  /* wp_sel_start/wp_sel_end macros (thread-local via GIL) */

extern boolean flconvertingolddatabase;

#ifndef BlockMoveData
#define BlockMoveData(src, dst, size) memmove((dst), (src), (size))
#endif

#if defined(FRONTIER_HEADLESS) && defined(FRONTIER_TESTS)
static boolean wp_portable_utf8_to_rtf(Handle hutf8, Handle *hrtf, long *out_chars);
#endif
static boolean wp_portable_rtf_to_utf8(const uint8_t *rtf, long len, Handle *hout_utf8);

#ifdef FRONTIER_HEADLESS

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "paige_text_extractor.h"

/* 2025-11-19 Codex: Replace Paige-linked headless runtime with a pure extractor + RTF writer pipeline. */

/* 2025-11-10 Codex: Replace the stub headless WP runtime with a Paige-backed loader/packer.
   2025-11-11 Codex: Add portable RTF serialization with 'WPRT' header support.
   2025-11-17 Codex: Implement wp_portable_external_* helpers so headless builds avoid null callbacks.
   2025-11-19 Codex: Remove Paige dependencies; rely on paige_text_extractor + RTF helpers entirely. */

#define WP_FLAG_ONELINE 0x8000
#define WP_FLAG_RULERON 0x4000

#define WP_PORTABLE_MAGIC 'WPRT'
#define WP_PORTABLE_VERSION 1
#define WP_PORTABLE_FLAG_UTF8 0x0001

typedef struct {
    short top;
    short left;
    short bottom;
    short right;
} wp_diskrect;

typedef struct {
    short versionnumber;
    long timecreated;
    long timelastsave;
    long ctsaves;
    char fontname[32];
    short fontsize;
    short fontstyle;
    long maxpos;
    short unused[4];
    short flags;
    wp_diskrect windowrect;
    long varlistsize;
    long buffersize;
    char waste[52];
} wp_diskheader;

typedef struct {
    OSType magic;
    uint16_t version;
    uint16_t flags;
    uint32_t timecreated;
    uint32_t timelastsave;
    uint32_t ctsaves;
    uint32_t textlength;
    uint32_t utf8bytelen;
    uint32_t reservedlength;
    uint8_t reserved[1024];
} wp_portable_diskheader;

typedef struct {
    uint16_t version;
    uint16_t flags;
    uint32_t timecreated;
    uint32_t timelastsave;
    uint32_t ctsaves;
    uint32_t textlength;
    uint32_t utf8bytelen;
    uint32_t reservedlength;
} wp_portable_header;

typedef struct wp_portable_state {
    dbaddress address;
    frontier_time_t timecreated;      /* 64-bit timestamp per frontier_time_t standard */
    frontier_time_t timelastsave;     /* 64-bit timestamp per frontier_time_t standard */
    frontier_time_t ctsaves;          /* Save count - using frontier_time_t for consistency */
    long maxpos;
    long buffersize;
    short flags;
    wp_diskheader raw_header;
    boolean header_valid;
    boolean dirty;
    boolean portable_format;
    wp_portable_header portable_header;
    long portable_payload_size;
    Handle portable_rtf_cache;
} wp_portable_state;

/* wp_sel_start/wp_sel_end are now thread-local macros from processinternal.h */

static wp_portable_state *wp_portable_state_alloc(void) {
    wp_portable_state *state = (wp_portable_state *)calloc(1, sizeof(wp_portable_state));
    return state;
}

static void wp_portable_state_free(wp_portable_state *state) {
    if (state == NULL)
        return;

    if (state->portable_rtf_cache != nil) {
        disposehandle(state->portable_rtf_cache);
        state->portable_rtf_cache = nil;
    }
    free(state);
}

static wp_portable_state *wp_portable_state_from_external(hdlexternalvariable hv) {
    return (wp_portable_state *)(intptr_t)(**hv).variabledata;
}

static void wp_portable_state_attach(hdlexternalvariable hv, wp_portable_state *state, dbaddress adr) {
    (**hv).flinmemory = true;
    (**hv).flpacked = false;
    (**hv).variabledata = (long)(intptr_t)state;
    (**hv).oldaddress = adr;
    state->address = adr;
    state->header_valid = false;
    state->portable_format = false;
    state->portable_payload_size = 0;
    state->portable_rtf_cache = nil;
    memset(&state->portable_header, 0, sizeof(state->portable_header));
}

static boolean wp_portable_state_dbref(const db_context *ctx, hdlexternalvariable hv, wp_portable_state *state, Handle *hpacked) {
    /*
    2025-12-23: Refactored to use explicit context instead of push/pop pattern
    */
    if (hv == nil || state == NULL || hpacked == NULL)
        return false;

    if (state->address == nildbaddress)
        return false;

    /* Use explicit context for reading - NO global state changes */
    boolean ok = dbrefhandle_context(ctx, state->address, hpacked);
    return ok;
}

static boolean wp_portable_parse_legacy_header(const wp_diskheader *header, wp_portable_state *state) {
    if (header == NULL || state == NULL)
        return false;

    state->raw_header = *header;
    state->timecreated = conditionallongswap(header->timecreated);
    state->timelastsave = conditionallongswap(header->timelastsave);
    state->ctsaves = conditionallongswap(header->ctsaves);
    state->maxpos = conditionallongswap(header->maxpos);
    state->buffersize = conditionallongswap(header->buffersize);
    state->flags = conditionalshortswap(header->flags);
    state->header_valid = true;
    state->portable_format = false;
    state->portable_payload_size = 0;
    memset(&state->portable_header, 0, sizeof(state->portable_header));
    return true;
}

static boolean wp_portable_parse_portable_header(const wp_portable_diskheader *disk, long payload_len, wp_portable_state *state) {
    if (disk == NULL || state == NULL)
        return false;

    OSType magic = conditionallongswap(disk->magic);
    if (magic != WP_PORTABLE_MAGIC)
        return false;

    wp_portable_header header;
    header.version = conditionalshortswap(disk->version);
    header.flags = conditionalshortswap(disk->flags);
    header.timecreated = conditionallongswap(disk->timecreated);
    header.timelastsave = conditionallongswap(disk->timelastsave);
    header.ctsaves = conditionallongswap(disk->ctsaves);
    header.textlength = conditionallongswap(disk->textlength);
    header.utf8bytelen = conditionallongswap(disk->utf8bytelen);
    header.reservedlength = conditionallongswap(disk->reservedlength);

    if ((long)header.utf8bytelen != payload_len)
        return false;

    state->timecreated = header.timecreated;
    state->timelastsave = header.timelastsave;
    state->ctsaves = header.ctsaves;
    state->maxpos = header.textlength;
    state->buffersize = payload_len;
    state->flags = 0;
    state->portable_format = true;
    state->portable_payload_size = payload_len;
    state->portable_header = header;
    state->header_valid = true;
    return true;
}

static boolean wp_portable_state_refresh_metadata(hdlexternalvariable hv, wp_portable_state *state) {
    if (state == NULL)
        return false;
    if (state->header_valid && (!state->portable_format || state->portable_payload_size > 0))
        return true;

    Handle hpacked = nil;
    if (!wp_portable_state_dbref(NULL, hv, state, &hpacked))
        return false;

    long size = gethandlesize(hpacked);
    boolean ok = false;

    if (size >= (long)sizeof(wp_portable_diskheader)) {
        wp_portable_diskheader disk;
        BlockMoveData(*hpacked, &disk, sizeof(wp_portable_diskheader));
        long payload_len = size - (long)sizeof(wp_portable_diskheader);
        ok = wp_portable_parse_portable_header(&disk, payload_len, state);
    }

    if (!ok) {
        if (size < (long)sizeof(wp_diskheader)) {
            disposehandle(hpacked);
            return false;
        }
        wp_diskheader legacy;
        long ix = size - (long)sizeof(legacy);
        if (!loadfromhandle(hpacked, &ix, sizeof(legacy), &legacy)) {
            disposehandle(hpacked);
            return false;
        }
        ok = wp_portable_parse_legacy_header(&legacy, state);
    }

    disposehandle(hpacked);
    return ok;
}

static wp_portable_state *wp_portable_state_require(hdlexternalvariable hv) {
    wp_portable_state *state = NULL;
    /* Only treat variabledata as pointer if external is in memory (Phase 3 migration optimization) */
    if ((**hv).flinmemory) {
        state = wp_portable_state_from_external(hv);
    }
    if (state == NULL) {
        state = wp_portable_state_alloc();
        if (state != NULL)
            wp_portable_state_attach(hv, state, (**hv).oldaddress);
    }
    return state;
}

static void wp_portable_fill_portable_header(wp_portable_state *state, long utf8_len) {
    state->portable_header.version = WP_PORTABLE_VERSION;
    state->portable_header.flags = WP_PORTABLE_FLAG_UTF8;
    /* Explicit cast from frontier_time_t (64-bit) to uint32_t (32-bit disk format) */
    state->portable_header.timecreated = (uint32_t)state->timecreated;
    state->portable_header.timelastsave = (uint32_t)state->timelastsave;
    state->portable_header.ctsaves = (uint32_t)state->ctsaves;
    state->portable_header.textlength = state->maxpos;
    state->portable_header.utf8bytelen = utf8_len;
    state->portable_header.reservedlength = 0;
}

static boolean wp_portable_wrap_rtf_payload(wp_portable_state *state, Handle hrtf, Handle *out_payload) {
    if (state == NULL || hrtf == nil || out_payload == NULL)
        return false;

    long utf8_len = gethandlesize(hrtf);
    size_t header_size = sizeof(wp_portable_diskheader);
    long total = (long)(header_size + (size_t)utf8_len);
    Handle hblob = nil;
    if (!newclearhandle(total, &hblob))
        return false;

    wp_portable_fill_portable_header(state, utf8_len);

    wp_portable_diskheader disk;
    memset(&disk, 0, sizeof(disk));
    disk.magic = conditionallongswap(WP_PORTABLE_MAGIC);
    disk.version = conditionalshortswap(state->portable_header.version);
    disk.flags = conditionalshortswap(state->portable_header.flags);
    disk.timecreated = conditionallongswap(state->portable_header.timecreated);
    disk.timelastsave = conditionallongswap(state->portable_header.timelastsave);
    disk.ctsaves = conditionallongswap(state->portable_header.ctsaves);
    disk.textlength = conditionallongswap(state->portable_header.textlength);
    disk.utf8bytelen = conditionallongswap(state->portable_header.utf8bytelen);
    disk.reservedlength = conditionallongswap(state->portable_header.reservedlength);

    BlockMoveData(&disk, *hblob, header_size);
    BlockMoveData(*hrtf, ((uint8_t *)*hblob) + header_size, utf8_len);

    state->portable_format = true;
    state->portable_payload_size = utf8_len;
    state->header_valid = true;

    *out_payload = hblob;
    return true;
}

static boolean wp_portable_state_cache_rtf(hdlexternalvariable hv, wp_portable_state *state) {
    if (state == NULL)
        return false;
    if (state->portable_rtf_cache != nil)
        return true;
    if (!wp_portable_state_refresh_metadata(hv, state))
        return false;

#if defined(FRONTIER_HEADLESS)
    log_debug(LOG_COMP_GENERAL, "wp-plain: cache state=%p portable=%d address=0x%llx cache=%s",
        (void *)state,
        state->portable_format ? 1 : 0,
        (unsigned long long)state->address,
        state->portable_rtf_cache ? "yes" : "no");
#endif

    Handle hpacked = nil;
    if (!wp_portable_state_dbref(NULL, hv, state, &hpacked))
        return false;

    long size = gethandlesize(hpacked);
    boolean ok = false;

    if (state->portable_format) {
        long header_len = (long)sizeof(wp_portable_diskheader);
        if (size >= header_len) {
            long payload_len = size - header_len;
            Handle hcopy = nil;
            if (newclearhandle(payload_len, &hcopy)) {
                BlockMoveData(((const uint8_t *)*hpacked) + header_len, *hcopy, payload_len);
                state->portable_rtf_cache = hcopy;
                ok = true;
#if defined(FRONTIER_HEADLESS)
                log_debug(LOG_COMP_GENERAL, "wp-plain: cache copied portable payload bytes=%ld", (long)payload_len);
#endif
            }
        }
    } else {
        long payload_len = size - (long)sizeof(wp_diskheader);
        const uint8_t *payload = (const uint8_t *)*hpacked;
        Handle hrtf = nil;
        long char_count = 0;
        if (payload_len > 0) {
            paige_extract_stats stats = {0};
            char errbuf[256] = {0};
            if (wptext_emit_rtf_from_paige_blob(payload, payload_len, &hrtf, &char_count, &stats,
                    errbuf, sizeof(errbuf))) {
                state->portable_rtf_cache = hrtf;
                if (char_count > 0)
                    state->maxpos = char_count;
                ok = true;
#if defined(FRONTIER_HEADLESS)
                log_debug(LOG_COMP_GENERAL, "wp-plain: cache built RTF bytes=%ld chars=%ld",
                    gethandlesize(hrtf), char_count);
#endif
            } else {
                log_error(LOG_COMP_GENERAL, "[wp-plain] RTF emit failed: %s", errbuf[0] ? errbuf : "<unknown>");
            }
        }
    }

    disposehandle(hpacked);
#if defined(FRONTIER_HEADLESS)
    log_debug(LOG_COMP_GENERAL, "wp-plain: cache state=%p result=%d", (void *)state, ok ? 1 : 0);
#endif
    return ok;
}

static boolean wp_portable_state_pack_portable(hdlexternalvariable hv, wp_portable_state *state, Handle *out_payload) {
    if (state == NULL || out_payload == NULL)
        return false;
    if (!wp_portable_state_cache_rtf(hv, state))
        return false;
    return wp_portable_wrap_rtf_payload(state, state->portable_rtf_cache, out_payload);
}

boolean wpverbgetdisplaystring(hdlexternalvariable h, bigstring bs) {
    (void)h;
    copyctopstring("wp", bs);
    return true;
}

boolean wpverbgettypestring(hdlexternalvariable h, bigstring bs) {
    (void)h;
    copyctopstring("wp", bs);
    return true;
}

boolean wpverbdispose(hdlexternalvariable h, boolean fldisk) {
    (void)fldisk;
    /* Only free state if external is in memory (Phase 3 migration optimization) */
    if ((**h).flinmemory) {
        wp_portable_state *state = wp_portable_state_from_external(h);
        wp_portable_state_free(state);
    }
    (**h).variabledata = 0;
    return true;
}

boolean wpverbisdirty(hdlexternalvariable h) {
    if (!(**h).flinmemory)
        return false;
    wp_portable_state *state = wp_portable_state_from_external(h);
    return state != NULL && state->dirty;
}

boolean wpverbsetdirty(hdlexternalvariable h, boolean fldirty) {
    if (!(**h).flinmemory)
        return false;
    wp_portable_state *state = wp_portable_state_from_external(h);
    if (state == NULL)
        return false;
    state->dirty = fldirty;
    return true;
}

boolean wpverbnew(Handle h, hdlexternalvariable *hv) {
    wp_portable_state *state;

    if (hv == NULL)
        return false;

    state = wp_portable_state_alloc();
    if (state == NULL)
        return false;

    if (!langnewexternalvariable(true, 0, hv)) {
        wp_portable_state_free(state);
        return false;
    }

    wp_portable_state_attach(*hv, state, nildbaddress);
    state->dirty = true;
    state->portable_format = true;
    state->timecreated = timenow();
    state->timelastsave = state->timecreated;

    if (h != nil) {
        state->portable_rtf_cache = h;
    }

    return true;
}

boolean wpverbmemorypack(hdlexternalvariable h, Handle *hpacked) {

    if (h == nil || hpacked == NULL || *hpacked == nil)
        return false;

    boolean fltempload = !(**h).flinmemory;

    wp_portable_state *state = wp_portable_state_require(h);
    if (state == NULL)
        return false;

    Handle hpayload = nil;
    boolean ok = wp_portable_state_pack_portable(h, state, &hpayload);

    if (fltempload) {
        wp_portable_state_free(state);
        (**h).variabledata = 0;
        (**h).flinmemory = false;
    }

    if (!ok) {
        if (hpayload != nil)
            disposehandle(hpayload);
        return false;
    }

    ok = pushhandle(hpayload, *hpacked);
    disposehandle(hpayload);

    return ok;
}

boolean wpverbmemoryunpack(Handle hpacked, long *ixload, hdlexternalvariable *h) {

    if (h == NULL)
        return false;

    Handle hdata = nil;

    if (!loadhandleremains(*ixload, hpacked, &hdata))
        return false;

    wp_portable_state *state = wp_portable_state_alloc();
    if (state == NULL) {
        disposehandle(hdata);
        return false;
    }

    hdlexternalvariable hv = nil;
    if (!langnewexternalvariable(true, (long)(intptr_t)state, &hv)) {
        wp_portable_state_free(state);
        disposehandle(hdata);
        return false;
    }

    wp_portable_state_attach(hv, state, nildbaddress);
    state->portable_format = true;
    state->portable_rtf_cache = hdata;

    *h = hv;
    return true;
}

boolean wpverbpack(hdlexternalvariable hv, Handle *hpacked, boolean *flnewdbaddress) {
    if (hv == nil || hpacked == NULL || *hpacked == nil)
        return false;

    wp_portable_state *state = wp_portable_state_require(hv);
    if (state == NULL)
        return false;

    dbaddress adr = state->address;
    boolean flforcepack = true;

    if (!flforcepack) {
        if (fldatabasesaveas) {
            if (!dbcopy(adr, &adr))
                return false;
            state->address = adr;
        }

        if (flnewdbaddress != NULL)
            *flnewdbaddress = fldatabasesaveas ? true : ((**hv).oldaddress != adr);

        (**hv).oldaddress = adr;
        state->address = adr;
        return pushlongondiskhandle((long)adr, *hpacked);
    }

    Handle hpayload = nil;
    boolean ok = wp_portable_state_pack_portable(hv, state, &hpayload);

    if (!ok)
        return false;

    state->ctsaves += 1;

    dbaddress previous = (**hv).oldaddress;
    boolean assigned = dbassignhandle(hpayload, &adr);
    disposehandle(hpayload);

    if (!assigned)
        return false;

    state->address = adr;
    (**hv).oldaddress = adr;
    state->dirty = false;

    if (flnewdbaddress != NULL)
        *flnewdbaddress = fldatabasesaveas ? true : (previous != adr);

    return pushlongondiskhandle((long)adr, *hpacked);
}

boolean wpverbunpack(Handle hpacked, long *ixload, hdlexternalvariable *h) {
    long rawadr = 0;

    if (!loadlongfromdiskhandle(hpacked, ixload, &rawadr))
        return false;

    wp_portable_state *state = wp_portable_state_alloc();
    if (state == NULL)
        return false;

    hdlexternalvariable hv = nil;
    if (!langnewexternalvariable(true, (long)(intptr_t)state, &hv)) {
        wp_portable_state_free(state);
        return false;
    }

    wp_portable_state_attach(hv, state, (dbaddress)rawadr);
    *h = hv;
    return true;
}

boolean wpverbinmemory(const db_context *ctx, hdlexternalvariable h) {
    (void) ctx;  /* Context not used in portable implementation */
    wp_portable_state *state = wp_portable_state_require(h);
    if (state == NULL)
        return false;
    return wp_portable_state_refresh_metadata(h, state);
}

boolean wpverbpacktotext(hdlexternalvariable h, Handle htext) {
    if (h == NULL || htext == NULL)
        return false;

    Handle hplain = nil;
    if (!wp_portable_extract_plaintext(h, &hplain))
        return false;

    boolean ok = pushhandle(hplain, htext);
    disposehandle(hplain);
    return ok;
}

boolean wpverbgetsize(hdlexternalvariable h, long *size) {
    if (size == NULL)
        return false;

    wp_portable_state *state = wp_portable_state_require(h);
    if (state == NULL)
        return false;

    if (!wp_portable_state_refresh_metadata(h, state))
        return false;

    *size = state->maxpos;
    return true;
}

boolean wpverbgettimes(hdlexternalvariable h, int64_t *timecreated, int64_t *timemodified) {
    wp_portable_state *state = wp_portable_state_require(h);
    if (state == NULL)
        return false;

    if (!wp_portable_state_refresh_metadata(h, state))
        return false;

    if (timecreated)
        *timecreated = state->timecreated;
    if (timemodified)
        *timemodified = state->timelastsave;
    return true;
}

boolean wpverbsettimes(hdlexternalvariable h, int64_t timecreated, int64_t timemodified) {
    wp_portable_state *state = wp_portable_state_require(h);
    if (state == NULL)
        return false;

    state->timecreated = timecreated;
    state->timelastsave = timemodified;
    state->header_valid = true;
    state->dirty = true;
    return true;
}

boolean wpwindowopen(hdlexternalvariable h, hdlwindowinfo *hinfo) {
    (void)h;
    if (hinfo)
        *hinfo = nil;
    return false;
}

boolean wpedit(hdlexternalvariable h, hdlwindowinfo win, ptrfilespec fs, bigstring bs, rectparam rzoom) {
    (void)h;
    (void)win;
    (void)fs;
    (void)bs;
    (void)rzoom;
    return false;
}

boolean wpverbfind(hdlexternalvariable h, boolean *flzoom) {
    (void)h;
    if (flzoom)
        *flzoom = false;
    return false;
}

boolean wpstart(void) {
    return true;
}

boolean wpgetselection(long *startsel, long *endsel) {
    if (startsel)
        *startsel = wp_sel_start;
    if (endsel)
        *endsel = wp_sel_end;
    return true;
}

boolean wpsetselection(long startsel, long endsel) {
    wp_sel_start = startsel;
    wp_sel_end = endsel;
    return true;
}

Boolean wp_portable_extract_plaintext(hdlexternalvariable hv, Handle *hout_utf8) {
    if (hv == NULL || hout_utf8 == NULL)
        return false;

    wp_portable_state *state = wp_portable_state_require(hv);
    if (state == NULL)
        return false;

    if (!wp_portable_state_refresh_metadata(hv, state))
        return false;

#if defined(FRONTIER_HEADLESS)
    log_debug(LOG_COMP_GENERAL, "wp-plain: extract hv=%p state=%p portable=%d address=0x%llx cache=%s",
        (void *)hv,
        (void *)state,
        state->portable_format ? 1 : 0,
        (unsigned long long)state->address,
        (state->portable_rtf_cache != nil) ? "yes" : "no");
#endif

    Handle hpacked = nil;
    if (!wp_portable_state_dbref(NULL, hv, state, &hpacked))
        return false;

    const uint8_t *bytes = (const uint8_t *)*hpacked;
    long packed_size = gethandlesize(hpacked);

    if (state->portable_format) {
        long header_size = (long)sizeof(wp_portable_diskheader);
        if (packed_size < header_size) {
            disposehandle(hpacked);
            return false;
        }

        long payload_len = packed_size - header_size;
        const uint8_t *payload = bytes + header_size;
        Handle hutf8 = nil;
        if (wp_portable_rtf_to_utf8(payload, payload_len, &hutf8)) {
            disposehandle(hpacked);
            *hout_utf8 = hutf8;
            return true;
        }

        log_warn(LOG_COMP_GENERAL, "[wp-plain] unable to decode RTF payload, returning raw bytes");

        if (!newclearhandle(payload_len, &hutf8)) {
            disposehandle(hpacked);
            return false;
        }
        BlockMoveData(payload, *hutf8, payload_len);
        disposehandle(hpacked);
        *hout_utf8 = hutf8;
        return true;
    }

    paige_extract_stats stats = {0};
    char errbuf[256] = {0};
    boolean ok = paige_extract_text_and_styles(bytes, packed_size, hout_utf8, &stats, errbuf, sizeof(errbuf));
    disposehandle(hpacked);

    if (!ok) {
        log_error(LOG_COMP_GENERAL, "[wp-plain] extractor failed: %s", errbuf[0] ? errbuf : "<unknown>");
        return false;
    }

    if (stats.returned_macroman) {
        log_debug(LOG_COMP_GENERAL, "[wp-plain] macromantoutf8 conversion failed, returning MacRoman bytes");
    }

#if defined(FRONTIER_HEADLESS)
    if (hout_utf8 != NULL && *hout_utf8 != nil) {
        log_debug(LOG_COMP_GENERAL, "wp-plain: extract ok hv=%p state=%p bytes=%ld portable=%d",
            (void *)hv,
            (void *)state,
            gethandlesize(*hout_utf8),
            state->portable_format ? 1 : 0);
    }
#endif

    return true;
}

Boolean wp_portable_external_should_drop(hdlexternalvariable hv) {
    if (!(**hv).flinmemory)
        return false;
    wp_portable_state *state = wp_portable_state_from_external(hv);
    if (state == NULL)
        return false;
    /*
        For now we never drop automatically; callers decide after attempting
        to load/convert the document.
    */
    return false;
}

Boolean wp_portable_external_was_legacy_ws(hdlexternalvariable hv) {
    wp_portable_state *state = wp_portable_state_require(hv);
    if (state == NULL)
        return false;
    return !state->portable_format;
}

static void wp_portable_log_event(const char *tag, hdlexternalvariable hv, const char *name_hint) {
    const char *path = (name_hint != NULL) ? name_hint : "<unknown>";
    log_debug(LOG_COMP_GENERAL, "[wp-plain] %s external=%p path=%s", tag, (void *)hv, path);
}

void wp_portable_note_drop_logged(hdlexternalvariable hv, const char *name_hint) {
    wp_portable_log_event("drop", hv, name_hint);
}

void wp_portable_note_conversion_logged(hdlexternalvariable hv, const char *name_hint) {
    wp_portable_log_event("converted-to-plain-text", hv, name_hint);
}

#ifdef FRONTIER_TESTS
Boolean wp_portable_pack_text_for_test(const char *utf8text, Handle *hpacked) {
    if (utf8text == NULL || hpacked == NULL)
        return false;

    long textlen = (long)strlen(utf8text);
    Handle hutf8 = nil;
    if (!newclearhandle(textlen, &hutf8))
        return false;
    if (textlen > 0)
        BlockMoveData(utf8text, *hutf8, (size_t)textlen);

    Handle hrtf = nil;
    long char_count = 0;
    if (!wp_portable_utf8_to_rtf(hutf8, &hrtf, &char_count)) {
        disposehandle(hutf8);
        return false;
    }
    disposehandle(hutf8);

    wp_portable_state temp = {0};
    temp.maxpos = char_count;
    temp.timecreated = 0;
    temp.timelastsave = 0;
    temp.ctsaves = 0;
    temp.portable_format = true;
    temp.header_valid = true;

    Handle payload = nil;
    boolean ok = wp_portable_wrap_rtf_payload(&temp, hrtf, &payload);
    disposehandle(hrtf);
    if (!ok)
        return false;

    *hpacked = payload;
    return true;
}

Boolean wp_portable_load_portable_blob_for_test(const unsigned char *blob, long len) {
    if (blob == NULL || len < (long)sizeof(wp_portable_diskheader))
        return false;

    wp_portable_diskheader disk;
    BlockMoveData(blob, &disk, sizeof(disk));

    if (conditionallongswap(disk.magic) != WP_PORTABLE_MAGIC)
        return false;
    if (conditionalshortswap(disk.version) != WP_PORTABLE_VERSION)
        return false;

    uint32_t utf8len = conditionallongswap(disk.utf8bytelen);
    uint32_t reserved_len = conditionallongswap(disk.reservedlength);
    if (reserved_len != 0)
        return false;

    size_t header_size = sizeof(wp_portable_diskheader);
    if ((long)header_size + (long)utf8len != len)
        return false;

    const uint8_t *payload = blob + header_size;
    Handle hutf8 = nil;
    boolean ok = wp_portable_rtf_to_utf8(payload, (long)utf8len, &hutf8);
    if (ok && hutf8 != nil)
        disposehandle(hutf8);
    return ok;
}

#define RTF_PREFIX "{\\rtf1\\ansi\\deff0\\pard "
#define RTF_SUFFIX "}"

static boolean wp_portable_append_bytes_unconditionals(Handle h, const void *data, size_t len) {
    if (h == nil || data == NULL || len == 0)
        return true;
    long old_size = gethandlesize(h);
    long new_size = old_size + (long)len;
    if (!sethandlesize(h, new_size))
        return false;
    BlockMoveData(data, *h + old_size, len);
    return true;
}

static boolean wp_portable_append_cstr(Handle h, const char *literal) {
    if (literal == NULL)
        return true;
    return wp_portable_append_bytes_unconditionals(h, literal, strlen(literal));
}

static long wp_portable_count_utf8_chars(const unsigned char *data, long len) {
    if (data == NULL || len <= 0)
        return 0;
    long count = 0;
    for (long i = 0; i < len; ++i) {
        unsigned char c = data[i];
        if ((c & 0xC0) != 0x80)
            ++count;
    }
    return count;
}

static boolean wp_portable_utf8_to_rtf(Handle hutf8, Handle *hrtf, long *out_chars) {
    if (hutf8 == nil || hrtf == NULL)
        return false;

    Handle hrtf_local = nil;
    if (!newclearhandle(0, &hrtf_local))
        return false;

    if (!wp_portable_append_cstr(hrtf_local, RTF_PREFIX)) {
        disposehandle(hrtf_local);
        return false;
    }

    const unsigned char *src = (const unsigned char *)*hutf8;
    long len = gethandlesize(hutf8);
    for (long i = 0; i < len; ++i) {
        unsigned char c = src[i];
        if (c == '\r' || c == '\n') {
            if (c == '\r' && (i + 1) < len && src[i + 1] == '\n')
                ++i;
            if (!wp_portable_append_cstr(hrtf_local, "\\par "))
            {
                disposehandle(hrtf_local);
                return false;
            }
            continue;
        }
        if (c == '\t') {
            if (!wp_portable_append_cstr(hrtf_local, "\\tab "))
            {
                disposehandle(hrtf_local);
                return false;
            }
            continue;
        }
        if (c == '\\' || c == '{' || c == '}') {
            char esc[2] = {'\\', (char)c};
            if (!wp_portable_append_bytes_unconditionals(hrtf_local, esc, sizeof(esc))) {
                disposehandle(hrtf_local);
                return false;
            }
            continue;
        }
        if (c < 0x20 || c == 0x7F) {
            char buf[6];
            snprintf(buf, sizeof(buf), "\\'%02x", c);
            if (!wp_portable_append_cstr(hrtf_local, buf)) {
                disposehandle(hrtf_local);
                return false;
            }
            continue;
        }
        if (c < 0x80) {
            if (!wp_portable_append_bytes_unconditionals(hrtf_local, &c, 1)) {
                disposehandle(hrtf_local);
                return false;
            }
        } else {
            char buf[6];
            snprintf(buf, sizeof(buf), "\\'%02x", c);
            if (!wp_portable_append_cstr(hrtf_local, buf)) {
                disposehandle(hrtf_local);
                return false;
            }
        }
    }

    if (!wp_portable_append_cstr(hrtf_local, RTF_SUFFIX)) {
        disposehandle(hrtf_local);
        return false;
    }

    if (out_chars)
        *out_chars = wp_portable_count_utf8_chars(src, len);

    *hrtf = hrtf_local;
    return true;
}
#endif /* FRONTIER_TESTS */

#endif /* FRONTIER_HEADLESS */

/*
 * Helper function for RTF-to-UTF8 conversion (wp_portable_rtf_to_utf8).
 * Defined outside FRONTIER_HEADLESS since wp_portable_rtf_to_utf8 is used unconditionally.
 * Note: wp_portable_append_bytes_unconditionals (inside FRONTIER_HEADLESS) serves the same purpose for
 * wp_portable_utf8_to_rtf, but that function is test-only.
 */
static boolean wp_portable_append_bytes_unconditional(Handle h, const void *data, size_t len) {
    if (h == nil || data == NULL || len == 0)
        return true;
    long old_size = gethandlesize(h);
    long new_size = old_size + (long)len;
    if (!sethandlesize(h, new_size))
        return false;
    BlockMoveData(data, *h + old_size, len);
    return true;
}

static int wp_portable_hex_value(unsigned char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static boolean wp_portable_rtf_to_utf8(const uint8_t *rtf, long len, Handle *hout_utf8) {
    if (rtf == NULL || len <= 0 || hout_utf8 == NULL)
        return false;

    Handle hout = nil;
    if (!newclearhandle(0, &hout))
        return false;

    long i = 0;
    while (i < len) {
        unsigned char c = rtf[i];
        if (c == '\\') {
            ++i;
            if (i >= len)
                goto fail;
            unsigned char next = rtf[i];
            if (next == '\\' || next == '{' || next == '}') {
                if (!wp_portable_append_bytes_unconditional(hout, &next, 1))
                    goto fail;
                ++i;
                continue;
            }
            if (next == '\'') {
                if ((i + 2) >= len)
                    goto fail;
                int hi = wp_portable_hex_value(rtf[i + 1]);
                int lo = wp_portable_hex_value(rtf[i + 2]);
                if (hi < 0 || lo < 0)
                    goto fail;
                unsigned char byte = (unsigned char)((hi << 4) | lo);
                if (!wp_portable_append_bytes_unconditional(hout, &byte, 1))
                    goto fail;
                i += 3;
                continue;
            }
            long word_start = i;
            while (i < len && ((rtf[i] >= 'a' && rtf[i] <= 'z') || (rtf[i] >= 'A' && rtf[i] <= 'Z')))
                ++i;
            long word_len = i - word_start;
            if (word_len <= 0)
                goto fail;

            const char *word = (const char *)(rtf + word_start);
            boolean handled = false;
            if (word_len == 3 && strncmp(word, "par", 3) == 0) {
                unsigned char newline = '\r';
                handled = wp_portable_append_bytes_unconditional(hout, &newline, 1);
            } else if (word_len == 3 && strncmp(word, "tab", 3) == 0) {
                unsigned char tab = '\t';
                handled = wp_portable_append_bytes_unconditional(hout, &tab, 1);
            } else if ((word_len == 3 && strncmp(word, "rtf", 3) == 0) ||
                       (word_len == 4 && strncmp(word, "ansi", 4) == 0) ||
                       (word_len == 4 && strncmp(word, "deff", 4) == 0) ||
                       (word_len == 4 && strncmp(word, "pard", 4) == 0)) {
                handled = true;
            }

            if (!handled)
                goto fail;

            while (i < len && (rtf[i] == '-' || (rtf[i] >= '0' && rtf[i] <= '9')))
                ++i;
            if (i < len && rtf[i] == ' ')
                ++i;
            continue;
        }

        if (c == '{' || c == '}') {
            ++i;
            continue;
        }

        if (!wp_portable_append_bytes_unconditional(hout, &c, 1))
            goto fail;
        ++i;
    }

    *hout_utf8 = hout;
    return true;

fail:
    if (hout != nil)
        disposehandle(hout);
    return false;
}
