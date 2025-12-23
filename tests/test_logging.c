/*
 * Frontier Logging System Unit Tests
 * Phase 3: Logging Infrastructure Implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../Common/headers/logging.h"

// Test counters
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    do { \
        tests_run++; \
        printf("Testing: %s ... ", name); \
        fflush(stdout); \
    } while (0)

#define ASSERT(condition) \
    do { \
        if (!(condition)) { \
            printf("FAILED\n"); \
            printf("  Assertion failed: %s\n", #condition); \
            printf("  File: %s, Line: %d\n", __FILE__, __LINE__); \
            tests_failed++; \
            return; \
        } \
    } while (0)

#define PASS() \
    do { \
        printf("PASSED\n"); \
        tests_passed++; \
    } while (0)

// Test functions

void test_log_init_defaults(void) {
    TEST("log_init with defaults");

    // Clear environment variables to test defaults
    unsetenv("FRONTIER_LOG_LEVEL");
    unsetenv("FRONTIER_LOG_COMPONENT");
    unsetenv("FRONTIER_LOG_FORMAT");

    log_init();

    // Default level is WARN
    ASSERT(log_is_enabled(LOG_LEVEL_ERROR, LOG_COMP_DB) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_WARN, LOG_COMP_DB) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_INFO, LOG_COMP_DB) == false);
    ASSERT(log_is_enabled(LOG_LEVEL_DEBUG, LOG_COMP_DB) == false);
    ASSERT(log_is_enabled(LOG_LEVEL_TRACE, LOG_COMP_DB) == false);

    // All components enabled by default
    ASSERT(log_is_enabled(LOG_LEVEL_WARN, LOG_COMP_DB) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_WARN, LOG_COMP_HASH) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_WARN, LOG_COMP_TABLE) == true);

    PASS();
}

void test_log_set_level(void) {
    TEST("log_set_level");

    log_set_level(LOG_LEVEL_DEBUG);

    ASSERT(log_is_enabled(LOG_LEVEL_ERROR, LOG_COMP_DB) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_WARN, LOG_COMP_DB) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_INFO, LOG_COMP_DB) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_DEBUG, LOG_COMP_DB) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_TRACE, LOG_COMP_DB) == false);

    // Reset to default
    log_set_level(LOG_LEVEL_WARN);

    PASS();
}

void test_log_set_level_trace(void) {
    TEST("log_set_level with TRACE");

    log_set_level(LOG_LEVEL_TRACE);

    ASSERT(log_is_enabled(LOG_LEVEL_ERROR, LOG_COMP_DB) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_WARN, LOG_COMP_DB) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_INFO, LOG_COMP_DB) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_DEBUG, LOG_COMP_DB) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_TRACE, LOG_COMP_DB) == true);

    // Reset to default
    log_set_level(LOG_LEVEL_WARN);

    PASS();
}

void test_component_filtering(void) {
    TEST("component filtering");

    log_set_level(LOG_LEVEL_DEBUG);

    // Enable only DB component
    log_set_component_enabled(LOG_COMP_DB, true);
    log_set_component_enabled(LOG_COMP_HASH, false);
    log_set_component_enabled(LOG_COMP_TABLE, false);

    ASSERT(log_is_enabled(LOG_LEVEL_DEBUG, LOG_COMP_DB) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_DEBUG, LOG_COMP_HASH) == false);
    ASSERT(log_is_enabled(LOG_LEVEL_DEBUG, LOG_COMP_TABLE) == false);

    // Re-enable all components
    log_set_component_enabled(LOG_COMP_HASH, true);
    log_set_component_enabled(LOG_COMP_TABLE, true);

    // Reset to default
    log_set_level(LOG_LEVEL_WARN);

    PASS();
}

void test_error_always_enabled(void) {
    TEST("errors always enabled regardless of level");

    // Set level to ERROR only
    log_set_level(LOG_LEVEL_ERROR);

    ASSERT(log_is_enabled(LOG_LEVEL_ERROR, LOG_COMP_DB) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_WARN, LOG_COMP_DB) == false);
    ASSERT(log_is_enabled(LOG_LEVEL_INFO, LOG_COMP_DB) == false);
    ASSERT(log_is_enabled(LOG_LEVEL_DEBUG, LOG_COMP_DB) == false);
    ASSERT(log_is_enabled(LOG_LEVEL_TRACE, LOG_COMP_DB) == false);

    // Reset to default
    log_set_level(LOG_LEVEL_WARN);

    PASS();
}

void test_all_components(void) {
    TEST("all components exist");

    log_set_level(LOG_LEVEL_DEBUG);

    // Verify all component enums work
    ASSERT(log_is_enabled(LOG_LEVEL_DEBUG, LOG_COMP_DB) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_DEBUG, LOG_COMP_HASH) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_DEBUG, LOG_COMP_TABLE) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_DEBUG, LOG_COMP_PACK) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_DEBUG, LOG_COMP_PARSE) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_DEBUG, LOG_COMP_EVAL) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_DEBUG, LOG_COMP_OP) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_DEBUG, LOG_COMP_LANG) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_DEBUG, LOG_COMP_EXTERNAL) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_DEBUG, LOG_COMP_STARTUP) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_DEBUG, LOG_COMP_GENERAL) == true);

    // Reset to default
    log_set_level(LOG_LEVEL_WARN);

    PASS();
}

void test_level_hierarchy(void) {
    TEST("log level hierarchy");

    // WARN level should show ERROR and WARN, but not INFO/DEBUG/TRACE
    log_set_level(LOG_LEVEL_WARN);
    ASSERT(log_is_enabled(LOG_LEVEL_ERROR, LOG_COMP_DB) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_WARN, LOG_COMP_DB) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_INFO, LOG_COMP_DB) == false);
    ASSERT(log_is_enabled(LOG_LEVEL_DEBUG, LOG_COMP_DB) == false);
    ASSERT(log_is_enabled(LOG_LEVEL_TRACE, LOG_COMP_DB) == false);

    // INFO level should show ERROR, WARN, and INFO, but not DEBUG/TRACE
    log_set_level(LOG_LEVEL_INFO);
    ASSERT(log_is_enabled(LOG_LEVEL_ERROR, LOG_COMP_DB) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_WARN, LOG_COMP_DB) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_INFO, LOG_COMP_DB) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_DEBUG, LOG_COMP_DB) == false);
    ASSERT(log_is_enabled(LOG_LEVEL_TRACE, LOG_COMP_DB) == false);

    // DEBUG level should show everything except TRACE
    log_set_level(LOG_LEVEL_DEBUG);
    ASSERT(log_is_enabled(LOG_LEVEL_ERROR, LOG_COMP_DB) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_WARN, LOG_COMP_DB) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_INFO, LOG_COMP_DB) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_DEBUG, LOG_COMP_DB) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_TRACE, LOG_COMP_DB) == false);

    // TRACE level should show everything
    log_set_level(LOG_LEVEL_TRACE);
    ASSERT(log_is_enabled(LOG_LEVEL_ERROR, LOG_COMP_DB) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_WARN, LOG_COMP_DB) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_INFO, LOG_COMP_DB) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_DEBUG, LOG_COMP_DB) == true);
    ASSERT(log_is_enabled(LOG_LEVEL_TRACE, LOG_COMP_DB) == true);

    // Reset to default
    log_set_level(LOG_LEVEL_WARN);

    PASS();
}

void test_macro_interface(void) {
    TEST("macro interface compiles and runs");

    log_set_level(LOG_LEVEL_DEBUG);

    // These should not crash or cause errors
    // (We can't easily verify output in unit tests, but we can verify they compile and run)
    log_error(LOG_COMP_DB, "Test error message");
    log_warn(LOG_COMP_DB, "Test warning message");
    log_info(LOG_COMP_DB, "Test info message");
    log_debug(LOG_COMP_DB, "Test debug message");
    log_trace(LOG_COMP_DB, "Test trace message");

    // Reset to default
    log_set_level(LOG_LEVEL_WARN);

    PASS();
}

void test_disabled_logging_has_no_effect(void) {
    TEST("disabled logging has no effect");

    // Set level to ERROR only
    log_set_level(LOG_LEVEL_ERROR);

    // These should be no-ops (not shown)
    log_warn(LOG_COMP_DB, "This should not appear");
    log_info(LOG_COMP_DB, "This should not appear");
    log_debug(LOG_COMP_DB, "This should not appear");
    log_trace(LOG_COMP_DB, "This should not appear");

    // Only errors should go through
    log_error(LOG_COMP_DB, "This error should appear");

    // Reset to default
    log_set_level(LOG_LEVEL_WARN);

    PASS();
}

int main(void) {
    printf("=== Frontier Logging System Unit Tests ===\n\n");

    // Run all tests
    test_log_init_defaults();
    test_log_set_level();
    test_log_set_level_trace();
    test_component_filtering();
    test_error_always_enabled();
    test_all_components();
    test_level_hierarchy();
    test_macro_interface();
    test_disabled_logging_has_no_effect();

    // Print summary
    printf("\n=== Test Summary ===\n");
    printf("Total tests: %d\n", tests_run);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);

    if (tests_failed == 0) {
        printf("\n✓ All logging tests passed!\n");
        return 0;
    } else {
        printf("\n✗ Some tests failed\n");
        return 1;
    }
}
