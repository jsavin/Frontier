// 2025-11-26 Codex: Dedicated header for modern (v7) writer helpers.
/*
 * db_writer_modern.h - Big-endian 64-bit block/header writers for v7+ roots.
 */

#ifndef FRONTIER_DB_WRITER_MODERN_H
#define FRONTIER_DB_WRITER_MODERN_H

#include <stddef.h>

#include "db.h"
#include "dbinternal.h"

#ifdef __cplusplus
extern "C" {
#endif

boolean db_write_v7_header(const tydatabaserecord *in, unsigned char *outbuf, size_t out_len);
boolean db_write_v7_block_header(dbaddress adr, boolean flfree, long ctbytes, tyvariance variance);
boolean db_write_v7_block_trailer(dbaddress adr, boolean flfree, long ctbytes);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FRONTIER_DB_WRITER_MODERN_H */
