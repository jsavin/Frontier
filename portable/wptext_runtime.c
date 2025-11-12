#include "frontier.h"
#include "langexternal.h"
#include "memory.h"
#include "db.h"
#include "strings.h"
#include "byteorder.h"
#include "wpverbs.h"
#include "wptext_portable.h"
#include "db_format.h"

extern boolean flconvertingolddatabase;

#ifdef FRONTIER_HEADLESS

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define Rect PaigeRect
#define Point PaigePoint
#define RgnHandle PaigeRgnHandle
#include "PAIGE.H"
#include "PGMEMMGR.H"
#include "PGTRAPS.H"
#include "PGEXCEPS.H"
#include "PGIO.H"
#include "DEFPROCS.H"
#include "PGTXR.H"
#undef Rect
#undef Point
#undef RgnHandle

/* 2025-11-10 Codex: Replace the stub headless WP runtime with a Paige-backed loader/packer.
   2025-11-11 Codex: Add portable RTF serialization with 'WPRT' header support. */

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
    unsigned short version;
    unsigned short flags;
    unsigned long timecreated;
    unsigned long timelastsave;
    unsigned long ctsaves;
    unsigned long textlength;
    unsigned long utf8bytelen;
    unsigned long reservedlength;
    unsigned char reserved[1024];
} wp_portable_diskheader;

typedef struct {
    unsigned short version;
    unsigned short flags;
    unsigned long timecreated;
    unsigned long timelastsave;
    unsigned long ctsaves;
    unsigned long textlength;
    unsigned long utf8bytelen;
    unsigned long reservedlength;
} wp_portable_header;

typedef struct wp_portable_state {
    dbaddress address;
    long timecreated;
    long timelastsave;
    long ctsaves;
    long maxpos;
    long buffersize;
    short flags;
    wp_diskheader raw_header;
    boolean header_valid;
    boolean dirty;
    boolean doc_loaded;
    pg_ref doc;
    boolean portable_format;
    wp_portable_header portable_header;
    long portable_payload_size;
} wp_portable_state;

static long headless_wp_sel_start = 0;
static long headless_wp_sel_end = 0;

static wp_portable_state *wp_portable_state_alloc(void) {
    wp_portable_state *state = (wp_portable_state *)calloc(1, sizeof(wp_portable_state));
    return state;
}

static void wp_portable_state_dispose_doc(wp_portable_state *state) {
    if (state == NULL || !state->doc_loaded)
        return;

    if (state->doc != MEM_NULL)
        pgDispose(state->doc);

    state->doc = MEM_NULL;
    state->doc_loaded = false;
}

static void wp_portable_state_free(wp_portable_state *state) {
    if (state == NULL)
        return;

    wp_portable_state_dispose_doc(state);
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
    state->doc_loaded = false;
    state->portable_format = false;
    state->portable_payload_size = 0;
    memset(&state->portable_header, 0, sizeof(state->portable_header));
}

static boolean wp_portable_require_runtime(void) {
    return wp_portable_init();
}

static boolean wp_portable_state_dbref(hdlexternalvariable hv, wp_portable_state *state, Handle *hpacked) {
    if (hv == nil || state == NULL || hpacked == NULL)
        return false;

    if (state->address == nildbaddress)
        return false;

    dbpushdatabase((**hv).hdatabase);
    boolean ok = dbrefhandle(state->address, hpacked);
    dbpopdatabase();
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
    if (!wp_portable_state_dbref(hv, state, &hpacked))
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
    wp_portable_state *state = wp_portable_state_from_external(hv);
    if (state == NULL) {
        state = wp_portable_state_alloc();
        if (state != NULL)
            wp_portable_state_attach(hv, state, (**hv).oldaddress);
    }
    return state;
}

static boolean wp_portable_unpack_legacy_doc(Handle hpacked, long payload_len, pg_ref *out_doc) {
    if (hpacked == nil || out_doc == NULL)
        return false;
    if (!wp_portable_require_runtime())
        return false;

    pgm_globals *mem_globals = wp_portable_mem_globals();
    pg_globals *pg_globals = wp_portable_pg_globals();

    file_ref filemap = (file_ref)MemoryAlloc(mem_globals, sizeof(pg_byte), payload_len, 0);
    if (filemap == MEM_NULL)
        return false;

    pg_byte *dest = (pg_byte *)UseMemory(filemap);
    pgBlockMove(*hpacked, dest, payload_len);
    UnuseMemory(filemap);

    size_t position = 0;
    pg_error ec = pgVerifyFile(filemap, pgScrapMemoryRead, position);

    pg_ref doc = MEM_NULL;
    if (ec == noErr) {
        PG_TRY(mem_globals) {
            doc = pgNewShell(pg_globals);
            ec = pgReadDoc(doc, &position, NULL, 0, pgScrapMemoryRead, filemap);
            if (ec == noErr)
                pgSetHiliteStates(doc, deactivate_verb, no_change_verb, false);
        }
        PG_CATCH {
            if (doc != MEM_NULL)
                pgFailureDispose(doc);
            ec = 1;
        }
        PG_ENDTRY;
    }

    UnuseAndDispose((memory_ref)filemap);

    if (ec != noErr) {
        if (doc != MEM_NULL)
            pgDispose(doc);
        return false;
    }

    *out_doc = doc;
    return true;
}

static boolean wp_portable_import_rtf_from_bytes(const uint8_t *data, long len, pg_ref *out_doc) {
    if (out_doc == NULL || len < 0)
        return false;
    if (len > 0 && data == NULL)
        return false;
    if (!wp_portable_require_runtime())
        return false;

    pgm_globals *mem_globals = wp_portable_mem_globals();
    pg_globals *pg_globals = wp_portable_pg_globals();

    pg_file_unit temp_unit;
    pg_file_desc_ref temp_desc = pgOpenTempFile(mem_globals, &temp_unit);
    if (temp_desc == MEM_NULL || temp_unit < 0)
        return false;

    pg_error err = NO_ERROR;
    long remaining = len;
    const uint8_t *cursor = data;
    while (remaining > 0 && err == NO_ERROR) {
        size_t chunk = (remaining > 32768) ? 32768 : (size_t)remaining;
        size_t chunk_copy = chunk;
        err = pgWriteFileBytes(temp_unit, &chunk_copy, cursor);
        remaining -= (long)chunk_copy;
        cursor += chunk_copy;
    }

    if (err == NO_ERROR)
        err = pgSetFilePos(temp_unit, 0);

    pg_ref doc = MEM_NULL;
    if (err == NO_ERROR) {
        doc = pgNewShell(pg_globals);
        if (doc == MEM_NULL)
            err = 1;
    }

    if (err == NO_ERROR)
        err = pgImportFileFromC(doc, pg_rtf_type, IMPORT_EVERYTHING_FLAG, 0, temp_unit);

    if (err == NO_ERROR)
        pgSetHiliteStates(doc, deactivate_verb, no_change_verb, false);

    pgCloseFile(temp_unit);
    pgDeleteFile(temp_desc);
    pgDisposeFileDescriptor(temp_desc);

    if (err != NO_ERROR) {
        if (doc != MEM_NULL)
            pgDispose(doc);
        return false;
    }

    *out_doc = doc;
    return true;
}

static boolean wp_portable_state_load_doc(hdlexternalvariable hv, wp_portable_state *state) {
    if (state == NULL)
        return false;
    if (state->doc_loaded)
        return true;
    if (!wp_portable_state_refresh_metadata(hv, state))
        return false;

    Handle hpacked = nil;
    if (!wp_portable_state_dbref(hv, state, &hpacked))
        return false;

    long size = gethandlesize(hpacked);
    boolean ok = false;

    if (state->portable_format) {
        long header_len = (long)sizeof(wp_portable_diskheader);
        if (size < header_len) {
            disposehandle(hpacked);
            return false;
        }
        long payload_len = size - header_len;
        const uint8_t *payload = ((const uint8_t *)*hpacked) + header_len;
        ok = wp_portable_import_rtf_from_bytes(payload, payload_len, &state->doc);
    } else {
        long payload_len = size - (long)sizeof(wp_diskheader);
        if (payload_len < 0) {
            disposehandle(hpacked);
            return false;
        }
        ok = wp_portable_unpack_legacy_doc(hpacked, payload_len, &state->doc);
    }

    disposehandle(hpacked);

    if (ok)
        state->doc_loaded = true;

    return ok;
}

static boolean wp_portable_use_portable_format(const wp_portable_state *state) {
    if (state != NULL && state->portable_format)
        return true;
    if (flconvertingolddatabase || use_64bit_format)
        return true;
    return false;
}

static boolean wp_portable_state_pack_legacy(hdlexternalvariable hv, wp_portable_state *state, Handle *out_payload) {
    if (state == NULL || out_payload == NULL)
        return false;

    if (!wp_portable_state_load_doc(hv, state))
        return false;

    state->maxpos = pgTextSize(state->doc);
    state->raw_header.maxpos = conditionallongswap(state->maxpos);

    Handle hpackedtext = nil;
    file_ref filemap;
    long pos = 0;
    pg_error ec;
    pgm_globals *mem_globals = wp_portable_mem_globals();

    filemap = (file_ref)MemoryAlloc(mem_globals, sizeof(pg_byte), 0, 0);
    if (filemap == MEM_NULL)
        return false;

    ec = pgSaveDoc(state->doc, &pos, NULL, 0, pgScrapMemoryWrite, filemap, 0);
    if (ec == noErr)
        ec = pgTerminateFile(state->doc, &pos, pgScrapMemoryWrite, filemap);

    if (ec != noErr) {
        UnuseAndDispose((memory_ref)filemap);
        return false;
    }

    pg_byte *ptext = (pg_byte *)UseMemory(filemap);
    long len = GetMemorySize(filemap);
    boolean ok = newfilledhandle(ptext, len, &hpackedtext);
    UnuseAndDispose((memory_ref)filemap);
    if (!ok)
        return false;

    state->buffersize = len;
    state->raw_header.buffersize = conditionallongswap(len);

    Handle hheader = nil;
    if (!newfilledhandle(&state->raw_header, sizeof(state->raw_header), &hheader)) {
        disposehandle(hpackedtext);
        return false;
    }

    ok = pushhandle(hheader, hpackedtext);
    disposehandle(hheader);

    if (ok) {
        *out_payload = hpackedtext;
        state->header_valid = true;
    } else
        disposehandle(hpackedtext);

    return ok;
}

static void wp_portable_fill_portable_header(wp_portable_state *state, long utf8_len) {
    state->portable_header.version = WP_PORTABLE_VERSION;
    state->portable_header.flags = WP_PORTABLE_FLAG_UTF8;
    state->portable_header.timecreated = state->timecreated;
    state->portable_header.timelastsave = state->timelastsave;
    state->portable_header.ctsaves = state->ctsaves;
    state->portable_header.textlength = state->maxpos;
    state->portable_header.utf8bytelen = utf8_len;
    state->portable_header.reservedlength = 0;
}

static boolean wp_portable_state_pack_portable(hdlexternalvariable hv, wp_portable_state *state, Handle *out_payload) {
    if (state == NULL || out_payload == NULL)
        return false;
    if (!wp_portable_state_load_doc(hv, state))
        return false;
    if (!wp_portable_require_runtime())
        return false;

    state->maxpos = pgTextSize(state->doc);

    Handle hrtf = nil;
    pg_file_unit temp_unit;
    pgm_globals *mem_globals = wp_portable_mem_globals();
    pg_file_desc_ref temp_desc = pgOpenTempFile(mem_globals, &temp_unit);
    if (temp_desc == MEM_NULL || temp_unit < 0)
        return false;

    pg_error err = pgExportFileFromC(state->doc, pg_rtf_type, EXPORT_EVERYTHING_FLAG | EXPORT_UNICODE_FLAG, 0, NULL, false, temp_unit);
    long eof = 0;
    if (err == NO_ERROR)
        err = pgGetFileEOF(temp_unit, &eof);

    if (err == NO_ERROR)
        err = pgSetFilePos(temp_unit, 0);

    if (err == NO_ERROR) {
        if (!newclearhandle(eof, &hrtf))
            err = 1;
    }

    if (err == NO_ERROR) {
        long remaining = eof;
        char *dest = *hrtf;
        while (remaining > 0 && err == NO_ERROR) {
            size_t chunk = (remaining > 32768) ? 32768 : (size_t)remaining;
            size_t chunk_read = chunk;
            err = pgReadFileBytes(temp_unit, &chunk_read, dest);
            remaining -= (long)chunk_read;
            dest += chunk_read;
        }
    }

    pgCloseFile(temp_unit);
    pgDeleteFile(temp_desc);
    pgDisposeFileDescriptor(temp_desc);

    if (err != NO_ERROR) {
        if (hrtf != nil)
            disposehandle(hrtf);
        return false;
    }

    long utf8_len = gethandlesize(hrtf);
    state->buffersize = utf8_len;

    Handle hblob = nil;
    size_t header_size = sizeof(wp_portable_diskheader);
    size_t total = header_size + (size_t)utf8_len;
    if (!newclearhandle((long)total, &hblob)) {
        disposehandle(hrtf);
        return false;
    }

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

    disposehandle(hrtf);

    state->portable_format = true;
    state->portable_payload_size = utf8_len;
    state->header_valid = true;

    *out_payload = hblob;
    return true;
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
    wp_portable_state *state = wp_portable_state_from_external(h);
    wp_portable_state_free(state);
    (**h).variabledata = 0;
    return true;
}

boolean wpverbisdirty(hdlexternalvariable h) {
    wp_portable_state *state = wp_portable_state_from_external(h);
    return state != NULL && state->dirty;
}

boolean wpverbsetdirty(hdlexternalvariable h, boolean fldirty) {
    wp_portable_state *state = wp_portable_state_from_external(h);
    if (state == NULL)
        return false;
    state->dirty = fldirty;
    return true;
}

boolean wpverbnew(Handle h, hdlexternalvariable *hv) {
    (void)h;
    (void)hv;
    return false;
}

boolean wpverbmemorypack(hdlexternalvariable h, Handle *hpacked) {
    (void)h;
    (void)hpacked;
    return false;
}

boolean wpverbmemoryunpack(Handle hpacked, long *ixload, hdlexternalvariable *h) {
    (void)hpacked;
    (void)ixload;
    (void)h;
    return false;
}

boolean wpverbpack(hdlexternalvariable hv, Handle *hpacked, boolean *flnewdbaddress) {
    if (hv == nil || hpacked == NULL || *hpacked == nil)
        return false;

    wp_portable_state *state = wp_portable_state_require(hv);
    if (state == NULL)
        return false;

    boolean use_portable = wp_portable_use_portable_format(state);

    dbaddress adr = state->address;
    boolean flforcepack = flconvertingolddatabase || fldatabasesaveas || state->dirty || use_portable;

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
    boolean ok = use_portable ?
        wp_portable_state_pack_portable(hv, state, &hpayload) :
        wp_portable_state_pack_legacy(hv, state, &hpayload);

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

boolean wpverbinmemory(hdlexternalvariable h) {
    wp_portable_state *state = wp_portable_state_require(h);
    if (state == NULL)
        return false;
    return wp_portable_state_load_doc(h, state);
}

boolean wpverbpacktotext(hdlexternalvariable h, Handle htext) {
    (void)h;
    (void)htext;
    return false;
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

boolean wpverbgettimes(hdlexternalvariable h, long *timecreated, long *timemodified) {
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

boolean wpverbsettimes(hdlexternalvariable h, long timecreated, long timemodified) {
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
        *startsel = headless_wp_sel_start;
    if (endsel)
        *endsel = headless_wp_sel_end;
    return true;
}

boolean wpsetselection(long startsel, long endsel) {
    headless_wp_sel_start = startsel;
    headless_wp_sel_end = endsel;
    return true;
}

#endif /* FRONTIER_HEADLESS */
