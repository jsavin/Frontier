/*
 * db_format.h - Helpers for detecting and migrating Frontier database headers.
 */

#ifndef FRONTIER_DB_FORMAT_H
#define FRONTIER_DB_FORMAT_H

#include "db.h"

#ifdef __cplusplus
extern "C" {
#endif

extern boolean use_64bit_format;

#define LEGACY_DB_HEADER_BYTES 88

boolean db_format_prepare_runtime(void);
boolean detect_database_format(const tydatabaserecord *header);
boolean convert_32bit_header_to_64bit(const unsigned char *legacy_header, tydatabaserecord_64 *new_header);
boolean create_root_backup(const char *original_path);
boolean migrate_32bit_to_64bit(const char *db_path);
boolean ensure_database_modern(const char *db_path, boolean *migrated, char *output_path, size_t output_path_size);
boolean db_format_last_backup_path(char *buffer, size_t length);
void db_format_clear_last_backup_path(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FRONTIER_DB_FORMAT_H */
