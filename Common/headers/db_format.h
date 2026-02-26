// 2025-11-20 Codex: Add shared big-endian helpers and a serializer for v7 headers to keep disk format portable.
// 2025-11-26 Codex: Move reader/writer entry points into dedicated headers to simplify version splits.
// 2025-11-29 Codex: Add context wrappers for adapter wide writes and Save As completion.
// 2025-11-30 Codex: Extend db_context with Save As state and expose scoped helpers.
/*
 * db_format.h - Helpers for detecting and migrating Frontier database headers.
 */

#ifndef FRONTIER_DB_FORMAT_H
#define FRONTIER_DB_FORMAT_H

#include <assert.h>
#include <stdint.h>
#include <stddef.h>

#include "db.h"
#include "lang.h" /* for hdlhashtable */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct db_format_mode {
    boolean use_64bit_format;
    boolean adapter_repack;
    boolean drop_cancoon;
} db_format_mode;

typedef struct db_saveas_state {
    boolean active;
    hdldatabaserecord destination;
    hdldatabaserecord source;
} db_saveas_state;

struct db_context {
    db_format_mode mode;
    hdldatabaserecord database;
    db_saveas_state saveas;
};

/* True if the database handle points to a v7 (64-bit) format database.
   Safe when hdb is nil (returns false). Used to choose between v7 and
   legacy read/write contexts throughout the packing code. */
#define db_is_v7(hdb) ((hdb) != nil && (**(hdb)).versionnumber >= 7)

#define LEGACY_DB_HEADER_BYTES 88

static inline uint16_t db_format_read_be16(const unsigned char *p) {
    return (uint16_t) ((p[0] << 8) | p[1]);
}

static inline uint32_t db_format_read_be32(const unsigned char *p) {
    return ((uint32_t) p[0] << 24) |
           ((uint32_t) p[1] << 16) |
           ((uint32_t) p[2] << 8)  |
            (uint32_t) p[3];
}

static inline uint64_t db_format_read_be64(const unsigned char *p) {
    return ((uint64_t) p[0] << 56) |
           ((uint64_t) p[1] << 48) |
           ((uint64_t) p[2] << 40) |
           ((uint64_t) p[3] << 32) |
           ((uint64_t) p[4] << 24) |
           ((uint64_t) p[5] << 16) |
           ((uint64_t) p[6] << 8)  |
            (uint64_t) p[7];
}

static inline void db_format_write_be16(void *ptr, uint16_t value) {
    unsigned char *p = (unsigned char *) ptr;
    p[0] = (unsigned char) ((value >> 8) & 0xFF);
    p[1] = (unsigned char) (value & 0xFF);
}

static inline void db_format_write_be32(void *ptr, uint32_t value) {
    unsigned char *p = (unsigned char *) ptr;
    p[0] = (unsigned char) ((value >> 24) & 0xFF);
    p[1] = (unsigned char) ((value >> 16) & 0xFF);
    p[2] = (unsigned char) ((value >> 8) & 0xFF);
    p[3] = (unsigned char) (value & 0xFF);
}

static inline void db_format_write_be64(void *ptr, uint64_t value) {
    unsigned char *p = (unsigned char *) ptr;
    p[0] = (unsigned char) ((value >> 56) & 0xFF);
    p[1] = (unsigned char) ((value >> 48) & 0xFF);
    p[2] = (unsigned char) ((value >> 40) & 0xFF);
    p[3] = (unsigned char) ((value >> 32) & 0xFF);
    p[4] = (unsigned char) ((value >> 24) & 0xFF);
    p[5] = (unsigned char) ((value >> 16) & 0xFF);
    p[6] = (unsigned char) ((value >> 8) & 0xFF);
    p[7] = (unsigned char) (value & 0xFF);
}

static inline void db_format_write_dbaddress64(void *ptr, dbaddress value) {
    db_format_write_be64(ptr, (uint64_t) value);
}

boolean db_format_prepare_runtime(void);
boolean detect_database_format(const tydatabaserecord *header);
boolean convert_32bit_header_to_64bit(const unsigned char *legacy_header, tydatabaserecord_64 *new_header);
boolean db_format_widen_legacy_header(const tydatabaserecord *decoded_header, boolean flreadonly, tydatabaserecord_64 *widened_out);
boolean db_format_write_header64(const tydatabaserecord_64 *src, unsigned char *dest, size_t dest_size);
boolean db_format_decode_header(const unsigned char *rawheader, size_t raw_len, boolean *header_is_v7, tydatabaserecord *out);
/* 2025-11-24 Codex: Entry points for v7 reader vs legacy adapter. */
boolean db_format_load_legacy_adapter(const tydatabaserecord *decoded_header, boolean flreadonly, tydatabaserecord_64 *widened_out);
boolean db_format_load_v7_reader(const tydatabaserecord *decoded_header, boolean flreadonly);
boolean db_format_header_version(const unsigned char *rawheader, size_t raw_len, int *out_version);
/* 2025-11-25 Codex: Legacy adapter state helpers for widening + wide writes. */
boolean db_format_adapter_enable_wide_writes(const tydatabaserecord_64 **widened_header_out);
boolean db_format_adapter_enable_wide_writes_context(const db_context *context, const tydatabaserecord_64 **widened_header_out);
boolean db_format_adapter_force_repack(void);
void db_format_adapter_mark_address(dbaddress *adr_out);
boolean db_format_adapter_is_active(void);
void db_format_adapter_reset(void);
void db_format_set_legacy_source_db(hdldatabaserecord hdb);
boolean db_format_is_legacy_db(hdldatabaserecord hdb);
boolean create_root_backup(const char *original_path);
boolean migrate_32bit_to_64bit(const char *db_path);
boolean migrate_32bit_to_64bit_drop_cancoon(const char *db_path);
boolean ensure_database_v7(const char *db_path, boolean *migrated, char *output_path, size_t output_path_size);
boolean db_format_last_backup_path(char *buffer, size_t length);
void db_format_clear_last_backup_path(void);
void db_format_force_strict_v7_reader(void);
/* Scoped mode helpers (thread-local) to avoid global races. */
void db_format_mode_push(const db_format_mode *mode);
void db_format_mode_pop(void);
db_format_mode db_format_mode_current(void);
void db_format_mode_apply(const db_format_mode *mode);
void db_saveas_state_snapshot(db_saveas_state *state);
void db_saveas_state_apply(const db_saveas_state *state);

/* Extract the file number from a context's database handle.
   Centralises the (hdlfilenum)((**hdb).fnumdatabase) cast so callers
   don't repeat the dereference chain.
   Precondition: context != NULL && context->database != nil. */
static inline hdlfilenum db_context_fnum(const db_context *context) {
    assert(context != NULL && context->database != nil);
    return (hdlfilenum)((**context->database).fnumdatabase);
}

/* Context initialization API */
void db_context_init(db_context *context);
void db_context_init_with_mode(db_context *context, const db_format_mode *mode);
void db_context_init_legacy_read(db_context *context, hdldatabaserecord db);
void db_context_init_v7_read(db_context *context, hdldatabaserecord db);
void db_context_init_v7_write(db_context *context, hdldatabaserecord db);
void db_context_clone_with_mode(const db_context *src, db_context *dst, const db_format_mode *mode);
void db_context_apply(const db_context *context);
boolean hashpacktable_context(const db_context *context, hdlhashtable ht, boolean flsave, Handle *hpacked, boolean *flmustsave);
boolean hashunpacktable_context(const db_context *context, Handle hpacked, boolean flmemory, hdlhashtable htable);
boolean dbassignhandle_context(const db_context *context, Handle h, dbaddress *adr);
boolean dbrefhandle_context(const db_context *context, dbaddress adr, Handle *h);
boolean dbcopy_context(const db_context *context, dbaddress src, dbaddress *dest);
boolean dbassign_context(const db_context *context, dbaddress *padr, long newsize, ptrvoid pdata);
/* Note: unlike other _context() wrappers, dbreference_context does NOT
   save/restore db_format_mode — it passes header_size explicitly. */
boolean dbreference_context(const db_context *context, dbaddress adr, long ctbytes, ptrvoid pdata);
boolean dballocate_context(const db_context *context, long databytes, ptrvoid pdata, dbaddress *paddress);
boolean dbreference_handle_context(const db_context *context, dbaddress adr, Handle *h);
boolean dbendsaveas_context(db_context *context);
boolean dbstartsaveas_context(db_context *context, hdlfilenum fnum);
boolean dbflushreleasestack_context(const db_context *context);
boolean dbzeroreleasestack_context(const db_context *context);
boolean dbrelease_context(const db_context *context, dbaddress adr);
boolean dbwriteshadowavaillist_context(const db_context *context);
boolean dbclearshadowavaillist_context(const db_context *context);
void dbswapglobals_context(db_context *context);
boolean dbassign_internal(dbaddress *padr, long newsize, ptrvoid pdata);
boolean dbcopy_internal(dbaddress adrorig, dbaddress *adrcopy);
boolean dbreference_internal(dbaddress adr, long maxbytes, ptrvoid pdata);

/* Phase 2: Context-aware wrappers for core DB I/O primitives.
   These temporarily apply the context's database, call the legacy function,
   and restore databasedata. Stepping stones toward eliminating the global.
   Intended callers: Phase 3+ pack/save/unpack paths replacing direct dbread/dbwrite. */
boolean dbread_context(const db_context *context, dbaddress adr, long ctbytes, ptrvoid pdata);
boolean dbwrite_context(const db_context *context, dbaddress adr, long ctbytes, ptrvoid pdata);
boolean dbsavehandle_context(const db_context *context, Handle h, dbaddress *adr);
boolean dbgeteof_context(const db_context *context, long *eof);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FRONTIER_DB_FORMAT_H */
