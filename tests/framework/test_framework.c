/*
 * test_framework.c - Implementation of minimal testing framework
 * 
 * Phase 0.5.3: Testing Strategy Implementation
 */

#include "test_framework.h"

// Global test statistics
test_stats_t g_test_stats = {0, 0, 0, NULL};

void test_init(void) {
    g_test_stats.total_tests = 0;
    g_test_stats.passed_tests = 0;
    g_test_stats.failed_tests = 0;
    g_test_stats.current_test_name = NULL;
    
    printf("=== Frontier Test Framework Initialized ===\n");
}

bool run_test_case(const test_case_t* test_case) {
    if (!test_case || !test_case->name || !test_case->test_function) {
        printf("ERROR: Invalid test case\n");
        return false;
    }
    
    g_test_stats.current_test_name = test_case->name;
    g_test_stats.total_tests++;
    
    printf("\n--- Running Test: %s ---\n", test_case->name);
    
    // Run the test
    bool result = test_case->test_function();
    
    if (result) {
        g_test_stats.passed_tests++;
    } else {
        g_test_stats.failed_tests++;
    }
    
    return result;
}

bool run_test_suite(const test_case_t* tests, int count) {
    if (!tests || count <= 0) {
        printf("ERROR: Invalid test suite\n");
        return false;
    }
    
    printf("\n=== Running Test Suite (%d tests) ===\n", count);
    
    bool all_passed = true;
    for (int i = 0; i < count; i++) {
        if (!run_test_case(&tests[i])) {
            all_passed = false;
        }
    }
    
    return all_passed;
}

void test_summary(void) {
    printf("\n=== Test Summary ===\n");
    printf("Total Tests: %d\n", g_test_stats.total_tests);
    printf("Passed: %d\n", g_test_stats.passed_tests);
    printf("Failed: %d\n", g_test_stats.failed_tests);
    
    if (g_test_stats.failed_tests == 0) {
        printf("🎉 ALL TESTS PASSED! 🎉\n");
    } else {
        printf("❌ %d TESTS FAILED ❌\n", g_test_stats.failed_tests);
    }
}

void test_setup(void) {
    // Global test setup - can be overridden for specific test suites
    printf("Setting up test environment...\n");
}

void test_teardown(void) {
    // Global test teardown - can be overridden for specific test suites
    printf("Cleaning up test environment...\n");
}

// Wrapper functions for simpler test main() implementations
void test_framework_init(void) {
    test_init();
}

void test_framework_summary(void) {
    test_summary();
}

int test_framework_get_exit_code(void) {
    return (g_test_stats.failed_tests == 0) ? 0 : 1;
}
