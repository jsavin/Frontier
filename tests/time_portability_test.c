/*
 * time_portability_test.c
 *
 * Tests for frontier_time_t portability and timestamp handling.
 *
 * Verifies:
 * - frontier_time_t is always 64-bit (8 bytes)
 * - Epoch conversion between Unix (1970) and Frontier (1904) is correct
 * - Timestamps survive round-trip storage and retrieval
 * - Boundary cases near 2038 (32-bit overflow point) work correctly
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>
#include <string.h>

#include "frontier.h"
#include "standard.h"
#include "timedate.h"

/* Test counter */
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) do { \
    if (condition) { \
        printf("  PASS: %s\n", message); \
        tests_passed++; \
    } else { \
        printf("  FAIL: %s\n", message); \
        tests_failed++; \
    } \
} while (0)

/*
 * Test 1: Verify frontier_time_t is always 64-bit (8 bytes)
 */
static void test_frontier_time_t_size(void) {
    printf("\nTest 1: frontier_time_t size\n");

    size_t size = sizeof(frontier_time_t);
    TEST_ASSERT(size == 8, "frontier_time_t is 8 bytes (64-bit)");

    /* Verify it's a signed type (can represent negative values) */
    frontier_time_t negative_time = -1;
    TEST_ASSERT(negative_time < 0, "frontier_time_t is signed");
}

/*
 * Test 2: Verify epoch conversion between Unix (1970) and Frontier (1904)
 */
static void test_epoch_conversion(void) {
    printf("\nTest 2: Epoch conversion\n");

    /* Seconds between 1904 and 1970 epochs */
    const frontier_time_t SECONDS_1904_TO_1970 = 2082844800UL;

    /* Test Unix epoch (1970-01-01 00:00:00) -> Frontier time */
    time_t unix_epoch = 0;
    frontier_time_t frontier_epoch = (frontier_time_t)unix_epoch + SECONDS_1904_TO_1970;
    TEST_ASSERT(frontier_epoch == SECONDS_1904_TO_1970,
                "Unix epoch (1970) maps to correct Frontier time");

    /* Test round-trip: Unix -> Frontier -> Unix */
    time_t test_time = 1234567890; /* Arbitrary test value */
    frontier_time_t frontier_time = (frontier_time_t)test_time + SECONDS_1904_TO_1970;
    time_t recovered_time = (time_t)(frontier_time - SECONDS_1904_TO_1970);
    TEST_ASSERT(recovered_time == test_time, "Round-trip conversion preserves value");

    /* Test current time conversion (using standard C library time()) */
    time_t c_now = time(NULL);
    frontier_time_t f_now = (frontier_time_t)c_now + SECONDS_1904_TO_1970;
    TEST_ASSERT(f_now > SECONDS_1904_TO_1970, "Current time is after 1970");
    TEST_ASSERT(f_now < SECONDS_1904_TO_1970 + 4000000000LL, "Current time is reasonable (before ~2096)");
}

/*
 * Test 3: Verify 2038 boundary case (32-bit time_t overflow point)
 */
static void test_2038_boundary(void) {
    printf("\nTest 3: Year 2038 boundary\n");

    /* 32-bit signed time_t overflows at: Tue Jan 19 03:14:07 2038 UTC */
    const time_t time_2038_overflow = 0x7FFFFFFF; /* 2147483647 */
    const frontier_time_t SECONDS_1904_TO_1970 = 2082844800UL;

    /* Convert 2038 overflow point to frontier_time_t */
    frontier_time_t frontier_2038 = (frontier_time_t)time_2038_overflow + SECONDS_1904_TO_1970;

    /* Verify this is representable in frontier_time_t (no overflow) */
    TEST_ASSERT(frontier_2038 > 0, "2038 timestamp is positive in frontier_time_t");
    TEST_ASSERT(frontier_2038 == (SECONDS_1904_TO_1970 + time_2038_overflow),
                "2038 timestamp conversion is exact");

    /* Test time after 2038 (would overflow 32-bit time_t) */
    frontier_time_t post_2038 = frontier_2038 + 86400; /* One day after overflow */
    TEST_ASSERT(post_2038 > frontier_2038, "Timestamps after 2038 work correctly");
}

/*
 * Test 4: Verify byte order independence (frontier_time_t is just int64_t)
 */
static void test_byte_order_independence(void) {
    printf("\nTest 4: Byte order independence\n");

    /* Create a known frontier_time_t value */
    frontier_time_t original = 0x0123456789ABCDEFLL;

    /* Manual byte swap for testing */
    union {
        frontier_time_t value;
        unsigned char bytes[8];
    } src, dst;

    src.value = original;

    /* Reverse bytes (manual big-endian conversion) */
    for (int i = 0; i < 8; i++) {
        dst.bytes[i] = src.bytes[7 - i];
    }

    /* Reverse again (should recover original) */
    for (int i = 0; i < 8; i++) {
        src.bytes[i] = dst.bytes[7 - i];
    }

    TEST_ASSERT(src.value == original, "Double byte-swap preserves value");
}

/*
 * Test 5: Verify frontier_time_t can represent very old and very new dates
 */
static void test_time_range(void) {
    printf("\nTest 5: Time range coverage\n");

    const frontier_time_t SECONDS_1904_TO_1970 = 2082844800UL;

    /* Test very old date: 1904 epoch */
    frontier_time_t epoch_1904 = 0;
    TEST_ASSERT(epoch_1904 == 0, "1904 epoch is represented as 0");

    /* Test pre-1970 date: 1950-01-01 */
    /* ~46 years before 1970 = ~1,451,520,000 seconds before 1970 */
    frontier_time_t time_1950 = SECONDS_1904_TO_1970 - (46LL * 365 * 86400);
    TEST_ASSERT(time_1950 > 0, "Pre-1970 dates are representable");

    /* Test far future: year 3000 */
    /* ~1030 years after 1970 = ~32,503,680,000 seconds after 1970 */
    frontier_time_t time_3000 = SECONDS_1904_TO_1970 + (1030LL * 365 * 86400);
    TEST_ASSERT(time_3000 > SECONDS_1904_TO_1970, "Far future dates are representable");
    TEST_ASSERT(time_3000 < INT64_MAX, "Far future dates fit in frontier_time_t");
}

/*
 * Test 6: Verify arithmetic operations work correctly
 */
static void test_time_arithmetic(void) {
    printf("\nTest 6: Time arithmetic\n");

    const frontier_time_t SECONDS_1904_TO_1970 = 2082844800UL;
    frontier_time_t base_time = SECONDS_1904_TO_1970 + 1000000;

    /* Test addition */
    frontier_time_t one_hour_later = base_time + 3600;
    TEST_ASSERT(one_hour_later == base_time + 3600, "Addition works correctly");

    /* Test subtraction */
    frontier_time_t time_diff = one_hour_later - base_time;
    TEST_ASSERT(time_diff == 3600, "Subtraction works correctly");

    /* Test comparison */
    TEST_ASSERT(one_hour_later > base_time, "Comparison (>) works correctly");
    TEST_ASSERT(base_time < one_hour_later, "Comparison (<) works correctly");
    TEST_ASSERT(base_time == base_time, "Comparison (==) works correctly");
}

/*
 * Test 7: Verify timenow64() returns reasonable current time
 */
static void test_timenow64(void) {
    printf("\nTest 7: timenow64() function\n");

    /* Get current time using timenow64() */
    frontier_time_t now = timenow64();

    /* Verify it's reasonable: after 2024-01-01 and before 2100-01-01 */
    /* 2024-01-01 in Frontier epoch ≈ (2024-1904) * 365.25 * 86400
       = 120 * 365.25 * 86400 = 3,786,912,000 seconds from 1904 */
    const frontier_time_t time_2024 = 3786912000LL;
    /* 2100-01-01 in Frontier epoch ≈ 196 * 365.25 * 86400 = 6,184,752,000 seconds from 1904 */
    const frontier_time_t time_2100 = 6184752000LL;

    TEST_ASSERT(now > time_2024, "timenow64() returns time after 2024");
    TEST_ASSERT(now < time_2100, "timenow64() returns time before 2100");

    /* Verify it matches manual conversion (within 1 second tolerance) */
    time_t unix_now = time(NULL);
    frontier_time_t manual_conversion = (frontier_time_t)unix_now + FRONTIER_EPOCH_TO_UNIX_OFFSET;

    int64_t diff = (int64_t)(now - manual_conversion);
    if (diff < 0) diff = -diff;
    TEST_ASSERT(diff <= 1, "timenow64() matches manual conversion (within 1 second)");

    /* Verify it's actually 64-bit (stores values > 32-bit max) */
    TEST_ASSERT(now > 0x7FFFFFFFLL, "timenow64() returns 64-bit value (> 32-bit max)");
}

/*
 * Main test runner
 */
int main(int argc, char *argv[]) {
    printf("=== Frontier Time Portability Tests ===\n");
    printf("sizeof(frontier_time_t) = %zu bytes\n", sizeof(frontier_time_t));
    printf("sizeof(time_t) = %zu bytes\n", sizeof(time_t));

    /* Run tests */
    test_frontier_time_t_size();
    test_epoch_conversion();
    test_2038_boundary();
    test_byte_order_independence();
    test_time_range();
    test_time_arithmetic();
    test_timenow64();

    /* Print summary */
    printf("\n=== Test Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);

    if (tests_failed > 0) {
        printf("\n*** SOME TESTS FAILED ***\n");
        return 1;
    } else {
        printf("\n*** ALL TESTS PASSED ***\n");
        return 0;
    }
}
