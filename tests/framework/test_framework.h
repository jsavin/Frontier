/*
 * test_framework.h - Minimal testing framework for Frontier refactoring
 * 
 * Phase 0.5.3: Testing Strategy Implementation
 * 
 * This framework provides basic testing capabilities for the component
 * migration phase, with minimal dependencies to maintain our clean build.
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

// Test result tracking
typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
    const char* current_test_name;
} test_stats_t;

extern test_stats_t g_test_stats;

// Test assertion macros
#define TEST_ASSERT(condition, message, ...) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s - ", g_test_stats.current_test_name); \
            printf(message, ##__VA_ARGS__); \
            printf("\n"); \
            g_test_stats.failed_tests++; \
            return false; \
        } \
    } while(0)

#define TEST_ASSERT_EQUAL(expected, actual, message) \
    do { \
        if ((expected) != (actual)) { \
            printf("FAIL: %s - %s (expected %d, got %d)\n", \
                   g_test_stats.current_test_name, message, (int)(expected), (int)(actual)); \
            g_test_stats.failed_tests++; \
            return false; \
        } \
    } while(0)

#define TEST_ASSERT_STR_EQUAL(expected, actual, message) \
    do { \
        if (strcmp((expected), (actual)) != 0) { \
            printf("FAIL: %s - %s (expected '%s', got '%s')\n", \
                   g_test_stats.current_test_name, message, (expected), (actual)); \
            g_test_stats.failed_tests++; \
            return false; \
        } \
    } while(0)

#define TEST_PASS(message) \
    do { \
        printf("PASS: %s - %s\n", g_test_stats.current_test_name, message); \
        g_test_stats.passed_tests++; \
        return true; \
    } while(0)

// Test function type
typedef bool (*test_function_t)(void);

// Test case structure
typedef struct {
    const char* name;
    test_function_t test_function;
} test_case_t;

// Test runner functions
void test_init(void);
bool run_test_case(const test_case_t* test_case);
bool run_test_suite(const test_case_t* tests, int count);
void test_summary(void);

// Test utilities
void test_setup(void);
void test_teardown(void);

#endif // TEST_FRAMEWORK_H
