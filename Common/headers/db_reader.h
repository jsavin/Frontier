// 2025-11-26 Codex: Split legacy vs modern reader entry points into a dedicated header.
/*
 * db_reader.h - Version-specific database header readers and loader hooks.
 */

#ifndef FRONTIER_DB_READER_H
#define FRONTIER_DB_READER_H

#include <stddef.h>

#include "db.h"

#ifdef __cplusplus
extern "C" {
#endif

boolean db_format_header_version(const unsigned char *rawheader, size_t raw_len, int *out_version);
boolean db_read_legacy(const unsigned char *rawheader, size_t raw_len, tydatabaserecord *out);
boolean db_read_modern(const unsigned char *rawheader, size_t raw_len, tydatabaserecord *out);
boolean db_format_load_legacy_adapter(const tydatabaserecord *decoded_header, boolean flreadonly, tydatabaserecord_64 *widened_out);
boolean db_format_load_v7_reader(const tydatabaserecord *decoded_header, boolean flreadonly);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FRONTIER_DB_READER_H */
