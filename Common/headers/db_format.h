// 2025-11-20 Codex: Add shared big-endian helpers and a serializer for v7 headers to keep disk format portable.
// 2025-11-26 Codex: Move reader/writer entry points into dedicated headers to simplify version splits.
/*
 * db_format.h - Helpers for detecting and migrating Frontier database headers.
 */

#ifndef FRONTIER_DB_FORMAT_H
#define FRONTIER_DB_FORMAT_H

#include <stdint.h>
#include <stddef.h>

#include "db.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct db_format_mode {
    boolean use_64bit_format;
    boolean adapter_repack;
    boolean drop_cancoon;
} db_format_mode;

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
boolean db_format_decode_header(const unsigned char *rawheader, size_t raw_len, boolean *header_is_modern, tydatabaserecord *out);
/* 2025-11-24 Codex: Entry points for v7 reader vs legacy adapter. */
boolean db_format_load_legacy_adapter(const tydatabaserecord *decoded_header, boolean flreadonly, tydatabaserecord_64 *widened_out);
boolean db_format_load_v7_reader(const tydatabaserecord *decoded_header, boolean flreadonly);
boolean db_format_header_version(const unsigned char *rawheader, size_t raw_len, int *out_version);
/* 2025-11-25 Codex: Legacy adapter state helpers for widening + wide writes. */
boolean db_format_adapter_enable_wide_writes(const tydatabaserecord_64 **widened_header_out);
boolean db_format_adapter_force_repack(void);
void db_format_adapter_mark_address(dbaddress *adr_out);
boolean db_format_adapter_is_active(void);
void db_format_set_legacy_source_db(hdldatabaserecord hdb);
boolean db_format_is_legacy_db(hdldatabaserecord hdb);
boolean create_root_backup(const char *original_path);
boolean migrate_32bit_to_64bit(const char *db_path);
boolean migrate_32bit_to_64bit_drop_cancoon(const char *db_path);
boolean ensure_database_modern(const char *db_path, boolean *migrated, char *output_path, size_t output_path_size);
boolean db_format_last_backup_path(char *buffer, size_t length);
void db_format_clear_last_backup_path(void);
void db_format_force_strict_v7_reader(void);
/* Scoped mode helpers (thread-local) to avoid global races. */
void db_format_mode_push(const db_format_mode *mode);
void db_format_mode_pop(void);
db_format_mode db_format_mode_current(void);
void db_format_mode_apply(const db_format_mode *mode);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FRONTIER_DB_FORMAT_H */
