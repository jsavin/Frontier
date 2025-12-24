/*
 * refcon_tests.c - Phase 1: Refcon Fundamentals Tests
 *
 * Tests basic refcon operations (set/get/has/empty) before moving to
 * serialization, migration, and reference integrity in later phases.
 *
 * Phase 1 Coverage:
 * - Test 1.1: Simple binary blob refcon
 * - Test 1.2: Structured data refcon
 * - Test 1.3: Empty/NULL refcon handling
 * - Test 1.4: Size mismatch handling
 *
 * Part of comprehensive refcon test suite for v6->v7 migration validation.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "frontier.h"
#include "standard.h"
#include "op.h"
#include "opinternal.h"
#include "memory.h"
#include "lang.h"
#include "tablestructure.h"
#include "../portable/wptext_portable.h"

/*
 * Test 1.1: Simple Binary Blob Refcon
 *
 * Create outline and headline, set a simple 16-byte binary blob as refcon,
 * get refcon back and verify byte-for-byte match. Verify ophasrefcon() reports true.
 */
static void test_refcon_simple_binary_blob(void) {
    printf("[refcon] Test 1.1: Simple binary blob... ");
    fflush(stdout);

    hdloutlinerecord houtline;
    hdlheadrecord hnode;

    /* Create outline + headline */
    assert(newoutlinerecord(&houtline));
    assert(opaddheadline((**houtline).hsummit, down, "\pTest Node", &hnode));

    /* Test data: 16-byte binary blob */
    unsigned char test_data[16] = {
        0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
        0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10
    };

    /* Set refcon */
    assert(opsetrefcon(hnode, test_data, 16));
    assert(ophasrefcon(hnode));

    /* Get refcon and verify byte-for-byte match */
    unsigned char retrieved[16];
    memset(retrieved, 0, 16);
    assert(opgetrefcon(hnode, retrieved, 16));
    assert(memcmp(test_data, retrieved, 16) == 0);

    /* Verify handle size */
    assert(gethandlesize((**hnode).hrefcon) == 16);

    opdisposeoutline(houtline, false);

    printf("PASS\n");
    fflush(stdout);
}

/*
 * Test 1.2: Structured Data Refcon
 *
 * Create structured data pattern (simulate simple struct with int32, int16, int16, int64 fields),
 * set as refcon, retrieve and verify field values are preserved.
 * Demonstrates refcons preserve structured byte patterns.
 */
static void test_refcon_structured_data(void) {
    printf("[refcon] Test 1.2: Structured data... ");
    fflush(stdout);

    /* Define test structure - simple pattern to avoid alignment issues */
    typedef struct {
        int32_t magic_number;      /* 0xDEADBEEF */
        int16_t version;           /* 42 */
        int16_t flags;             /* 0x00FF */
        int64_t timestamp;         /* 1234567890LL */
    } test_refcon_struct;

    hdloutlinerecord houtline;
    hdlheadrecord hnode;

    assert(newoutlinerecord(&houtline));
    assert(opaddheadline((**houtline).hsummit, down, "\pTest Node", &hnode));

    /* Create structured data */
    test_refcon_struct original;
    original.magic_number = 0xDEADBEEF;
    original.version = 42;
    original.flags = 0x00FF;
    original.timestamp = 1234567890LL;

    /* Set refcon */
    assert(opsetrefcon(hnode, &original, sizeof(test_refcon_struct)));

    /* Get refcon and verify structure fields */
    test_refcon_struct retrieved;
    memset(&retrieved, 0, sizeof(test_refcon_struct));
    assert(opgetrefcon(hnode, &retrieved, sizeof(test_refcon_struct)));

    assert(retrieved.magic_number == 0xDEADBEEF);
    assert(retrieved.version == 42);
    assert(retrieved.flags == 0x00FF);
    assert(retrieved.timestamp == 1234567890LL);

    opdisposeoutline(houtline, false);

    printf("PASS\n");
    fflush(stdout);
}

/*
 * Test 1.3: Empty and NULL Refcon Handling
 *
 * Verify new headline has no refcon (ophasrefcon returns false),
 * verify opgetrefcon on empty refcon behaves gracefully (no crash),
 * set a refcon then use opemptyrefcon to clear it,
 * verify refcon is gone after empty.
 */
static void test_refcon_empty_and_null(void) {
    printf("[refcon] Test 1.3: Empty/NULL handling... ");
    fflush(stdout);

    hdloutlinerecord houtline;
    hdlheadrecord hnode;

    assert(newoutlinerecord(&houtline));
    assert(opaddheadline((**houtline).hsummit, down, "\pTest Node", &hnode));

    /* Initially no refcon */
    assert(!ophasrefcon(hnode));
    assert((**hnode).hrefcon == NULL);

    /* opgetrefcon on NULL refcon should return false but not crash */
    unsigned char buffer[16];
    memset(buffer, 0xFF, 16);  /* Fill with 0xFF */
    boolean result = opgetrefcon(hnode, buffer, 16);
    assert(result == false);  /* Per oprefcon.c:109-110 */
    assert(buffer[0] == 0x00);  /* Should be zeroed (per oprefcon.c:107) */

    /* Set refcon, then empty it */
    unsigned char test_data[4] = {0x01, 0x02, 0x03, 0x04};
    assert(opsetrefcon(hnode, test_data, 4));
    assert(ophasrefcon(hnode));

    opemptyrefcon(hnode);
    assert(!ophasrefcon(hnode));
    assert((**hnode).hrefcon == NULL);

    opdisposeoutline(houtline, false);

    printf("PASS\n");
    fflush(stdout);
}

/*
 * Test 1.4: Size Mismatch Handling
 *
 * Set 32-byte refcon, try to get with 16-byte buffer (smaller),
 * try to get with 64-byte buffer (larger), verify no crashes on size mismatches.
 * Test resize: replace 32-byte with 64-byte refcon, verify larger refcon
 * is correctly stored and retrieved.
 */
static void test_refcon_size_mismatch(void) {
    printf("[refcon] Test 1.4: Size mismatch handling... ");
    fflush(stdout);

    hdloutlinerecord houtline;
    hdlheadrecord hnode;

    assert(newoutlinerecord(&houtline));
    assert(opaddheadline((**houtline).hsummit, down, "\pTest Node", &hnode));

    /* Set 32-byte refcon */
    unsigned char test_data[32];
    for (int i = 0; i < 32; i++) test_data[i] = (unsigned char)i;
    assert(opsetrefcon(hnode, test_data, 32));

    /* Try to get with smaller buffer (16 bytes) */
    unsigned char small_buffer[16];
    memset(small_buffer, 0xFF, 16);
    assert(opgetrefcon(hnode, small_buffer, 16));
    /* Should get first 16 bytes (per oprefcon.c:112-113) */
    for (int i = 0; i < 16; i++) {
        assert(small_buffer[i] == (unsigned char)i);
    }

    /* Try to get with larger buffer (64 bytes) */
    unsigned char large_buffer[64];
    memset(large_buffer, 0xFF, 64);
    assert(opgetrefcon(hnode, large_buffer, 64));
    /* First 32 bytes should match, rest should be zeroed (per oprefcon.c:106-107) */
    for (int i = 0; i < 32; i++) {
        assert(large_buffer[i] == (unsigned char)i);
    }
    for (int i = 32; i < 64; i++) {
        assert(large_buffer[i] == 0x00);
    }

    /* Test resize: replace 32-byte with 64-byte refcon */
    unsigned char large_test_data[64];
    for (int i = 0; i < 64; i++) large_test_data[i] = (unsigned char)(63 - i);
    assert(opsetrefcon(hnode, large_test_data, 64));

    /* Verify new size */
    assert(gethandlesize((**hnode).hrefcon) == 64);

    /* Verify new data */
    unsigned char retrieved[64];
    memset(retrieved, 0xFF, 64);
    assert(opgetrefcon(hnode, retrieved, 64));
    assert(memcmp(large_test_data, retrieved, 64) == 0);

    opdisposeoutline(houtline, false);

    printf("PASS\n");
    fflush(stdout);
}

/*
 * Main test runner
 */
int main(void) {
    printf("\n=== Phase 1: Refcon Fundamentals Tests ===\n");
    printf("[refcon] Initializing runtime...\n");
    fflush(stdout);

    /* Initialize runtime subsystems */
    assert(initlang());
    assert(inittablestructure());
    assert(langinitverbs());
    assert(wp_portable_init());

    printf("[refcon] Testing basic refcon operations (set/get/has/empty)\n");
    fflush(stdout);

    test_refcon_simple_binary_blob();
    test_refcon_structured_data();
    test_refcon_empty_and_null();
    test_refcon_size_mismatch();

    printf("\n========================================\n");
    printf("[refcon] Phase 1: ALL TESTS PASSED\n");
    printf("========================================\n");
    fflush(stdout);

    wp_portable_shutdown();
    return 0;
}
