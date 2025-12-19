/* 2025-12-06 Codex: Minimal test surface for hashpack modern/legacy helpers. */
#ifndef LANGHASH_TEST_H
#define LANGHASH_TEST_H

#include "lang.h"
/* langhash.c has no public header; this file is test-only surface. */

#ifdef __cplusplus
extern "C" {
#endif

/* Modern scalar payload (mirrors langhash.c) */
typedef struct langhash_test_diskvaluedata_v7 {
    uint64_t longvalue;
    uint64_t datevalue;
    uint64_t doublebits;
    int32_t dirvalue;
    struct { int16_t v; int16_t h; } pointvalue;
    uint32_t ostypevalue;
    uint32_t enumvalue;
    int32_t fixedvalue;
    int32_t tokenvalue;
} langhash_test_diskvaluedata_v7;

typedef struct langhash_test_disksymbolrecord_v7 {
    int32_t ixkey;
    uint8_t valuetype;
    uint8_t version;
    uint16_t _pad;
    langhash_test_diskvaluedata_v7 data;
} langhash_test_disksymbolrecord_v7;

/* Expose value pack/unpack helpers for tests (guarded by FRONTIER_TESTS in langhash.c). */
void langhash_test_value_to_disk_v7(const tyvaluerecord *val, langhash_test_disksymbolrecord_v7 *rec);
void langhash_test_value_from_disk_v7(const langhash_test_disksymbolrecord_v7 *rec, tyvaluerecord *val);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LANGHASH_TEST_H */
