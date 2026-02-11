/*
 * test_stringerrorlist.c - Verify stringerrorlist (resource 263) is available
 *
 * The stringerrorlist provides text encoding conversion error messages.
 * These were missing in headless mode, causing setTextEncodingConversionError()
 * to call langerrormessage() with an empty string and poison fllangerror globally.
 *
 * This test verifies the root cause fix: the string resources now load correctly.
 */

#include "framework/test_framework.h"
#include "../Common/headers/logging.h"

/* Include frontier.h for bigstring, boolean, etc. */
#include "../Common/headers/frontier.h"

/* Provided by headless_lang_runtime_more_stubs.c */
extern boolean getstringlist(short listid, short index, bigstring bs);

/* Stub for tcp_process_callbacks referenced by processsleep in stubs */
int tcp_process_callbacks(void) { return 0; }

#define stringerrorlist_id 263

static bool test_all_stringerrorlist_entries_load(void) {
    g_test_stats.current_test_name = "All stringerrorlist entries load";

    bigstring bs;

    /* All 7 entries (indices 1-7) should load successfully */
    for (int i = 1; i <= 7; i++) {
        TEST_ASSERT(getstringlist(stringerrorlist_id, i, bs),
                    "entry %d should load", i);
        TEST_ASSERT(bs[0] > 0,
                    "entry %d should be non-empty", i);
    }

    TEST_PASS("all 7 stringerrorlist entries loaded");
}

static bool test_stringerrorlist_content(void) {
    g_test_stats.current_test_name = "stringerrorlist content verification";

    bigstring bs;
    char cstr[256];

    /* Entry 2 should mention "unknown error" */
    TEST_ASSERT(getstringlist(stringerrorlist_id, 2, bs), "entry 2 loads");
    {
        int len = bs[0];
        memcpy(cstr, bs + 1, len);
        cstr[len] = '\0';
        TEST_ASSERT(strstr(cstr, "unknown error") != NULL,
                    "entry 2 should mention 'unknown error', got: %s", cstr);
    }

    /* Entry 6 should have two substitution parameters (^0 and ^1) */
    TEST_ASSERT(getstringlist(stringerrorlist_id, 6, bs), "entry 6 loads");
    {
        int len = bs[0];
        memcpy(cstr, bs + 1, len);
        cstr[len] = '\0';
        TEST_ASSERT(strstr(cstr, "^0") != NULL,
                    "entry 6 should contain ^0, got: %s", cstr);
        TEST_ASSERT(strstr(cstr, "^1") != NULL,
                    "entry 6 should contain ^1, got: %s", cstr);
    }

    /* Entry 7 should mention "multibyte" */
    TEST_ASSERT(getstringlist(stringerrorlist_id, 7, bs), "entry 7 loads");
    {
        int len = bs[0];
        memcpy(cstr, bs + 1, len);
        cstr[len] = '\0';
        TEST_ASSERT(strstr(cstr, "multibyte") != NULL,
                    "entry 7 should mention 'multibyte', got: %s", cstr);
    }

    TEST_PASS("stringerrorlist content verified");
}

static bool test_stringerrorlist_invalid_index(void) {
    g_test_stats.current_test_name = "stringerrorlist invalid index";

    bigstring bs;

    /* Index 0 should fail */
    TEST_ASSERT(getstringlist(stringerrorlist_id, 0, bs) == false,
                "index 0 should not exist");

    /* Index 8 should fail (only 7 entries) */
    TEST_ASSERT(getstringlist(stringerrorlist_id, 8, bs) == false,
                "index 8 should not exist");

    /* Index 99 should fail */
    TEST_ASSERT(getstringlist(stringerrorlist_id, 99, bs) == false,
                "index 99 should not exist");

    TEST_PASS("invalid indices correctly rejected");
}

static test_case_t test_cases[] = {
    {"All entries load", test_all_stringerrorlist_entries_load},
    {"Content verification", test_stringerrorlist_content},
    {"Invalid index handling", test_stringerrorlist_invalid_index},
};

int main(void) {
    printf("=== String Error List (263) Tests ===\n");
    log_init();
    test_init();
    bool all_passed = run_test_suite(test_cases, sizeof(test_cases) / sizeof(test_cases[0]));
    test_summary();
    return all_passed ? 0 : 1;
}
