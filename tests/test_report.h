/*
 * test_report.h - Lightweight test reporting for Frontier unit tests
 *
 * Instruments existing test executables to report per-function pass/fail
 * results as JSON. Uses setjmp/SIGABRT to catch assert() failures and
 * continue running remaining tests.
 *
 * Usage:
 *   #include "test_report.h"
 *
 *   int main(void) {
 *       TR_INIT("my_tests");
 *       // ... existing init code ...
 *       TR_RUN(test_function_a);
 *       TR_RUN(test_function_b);
 *       TR_SUMMARY();
 *       return TR_EXIT_CODE();
 *   }
 *
 * Output: writes tests/tmp/unit/<name>.json with per-function results.
 */

#ifndef TEST_REPORT_H
#define TEST_REPORT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <time.h>
#include <sys/stat.h>

/* Maximum number of test functions per executable */
#define TR_MAX_TESTS 256

typedef struct {
    const char *name;
    int passed; /* 1 = pass, 0 = fail */
} tr_test_result;

/* Global state for the current test run */
static const char *tr_executable_name = NULL;
static tr_test_result tr_results[TR_MAX_TESTS];
static int tr_count = 0;
static int tr_pass_count = 0;
static int tr_fail_count = 0;
static jmp_buf tr_jump_buf;
static volatile sig_atomic_t tr_in_test = 0;

/* Previous SIGABRT handler, saved/restored around each test */
static struct sigaction tr_prev_sigaction;

static void tr_sigabrt_handler(int sig) {
    (void)sig;
    if (tr_in_test) {
        longjmp(tr_jump_buf, 1);
    }
    /* If not in a test, restore previous handler and re-raise */
    sigaction(SIGABRT, &tr_prev_sigaction, NULL);
    raise(SIGABRT);
}

static void tr_install_handler(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = tr_sigabrt_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGABRT, &sa, &tr_prev_sigaction);
}

static void tr_restore_handler(void) {
    sigaction(SIGABRT, &tr_prev_sigaction, NULL);
}

static void tr_write_json(void) {
    /* Build path: tests/tmp/unit/<name>.json */
    char dir_path[512];
    char json_path[512];

    /* Try to create output directory (ignore errors if exists) */
    snprintf(dir_path, sizeof(dir_path), "tmp/unit");
    /* Use mkdir -p equivalent via two calls */
    (void)mkdir("tmp", 0755);
    (void)mkdir(dir_path, 0755);

    snprintf(json_path, sizeof(json_path), "tmp/unit/%s.json", tr_executable_name);

    FILE *fp = fopen(json_path, "w");
    if (!fp) {
        fprintf(stderr, "[test_report] WARNING: Could not write %s\n", json_path);
        return;
    }

    /* ISO 8601 timestamp */
    time_t now = time(NULL);
    struct tm *utc = gmtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", utc);

    fprintf(fp, "{\n");
    fprintf(fp, "  \"executable\": \"%s\",\n", tr_executable_name);
    fprintf(fp, "  \"timestamp\": \"%s\",\n", timestamp);
    fprintf(fp, "  \"tests\": [\n");
    for (int i = 0; i < tr_count; i++) {
        fprintf(fp, "    {\"name\": \"%s\", \"passed\": %s}%s\n",
                tr_results[i].name,
                tr_results[i].passed ? "true" : "false",
                (i < tr_count - 1) ? "," : "");
    }
    fprintf(fp, "  ],\n");
    fprintf(fp, "  \"total\": %d,\n", tr_count);
    fprintf(fp, "  \"passed\": %d,\n", tr_pass_count);
    fprintf(fp, "  \"failed\": %d\n", tr_fail_count);
    fprintf(fp, "}\n");
    fclose(fp);
}

/* Initialize the test reporter for this executable */
#define TR_INIT(name) \
    do { \
        tr_executable_name = (name); \
        tr_count = 0; \
        tr_pass_count = 0; \
        tr_fail_count = 0; \
    } while (0)

/*
 * Run a test function, catching assert failures via SIGABRT/setjmp.
 * Records pass/fail and continues to the next test either way.
 */
#define TR_RUN(func) \
    do { \
        tr_install_handler(); \
        tr_in_test = 1; \
        if (setjmp(tr_jump_buf) == 0) { \
            (func)(); \
            /* If we get here, the test passed */ \
            tr_in_test = 0; \
            tr_restore_handler(); \
            if (tr_count < TR_MAX_TESTS) { \
                tr_results[tr_count].name = #func; \
                tr_results[tr_count].passed = 1; \
                tr_count++; \
                tr_pass_count++; \
            } \
        } else { \
            /* longjmp landed here: the test failed (assert fired) */ \
            tr_in_test = 0; \
            tr_restore_handler(); \
            fprintf(stderr, "[test_report] FAIL: %s (assert fired)\n", #func); \
            if (tr_count < TR_MAX_TESTS) { \
                tr_results[tr_count].name = #func; \
                tr_results[tr_count].passed = 0; \
                tr_count++; \
                tr_fail_count++; \
            } \
        } \
    } while (0)

/* Print summary and write JSON results file */
#define TR_SUMMARY() \
    do { \
        printf("[test_report] %s: %d/%d tests passed", \
               tr_executable_name, tr_pass_count, tr_count); \
        if (tr_fail_count > 0) \
            printf(" (%d FAILED)", tr_fail_count); \
        printf("\n"); \
        tr_write_json(); \
    } while (0)

/* Return appropriate exit code (0 if all passed, 1 if any failed) */
#define TR_EXIT_CODE() (tr_fail_count > 0 ? 1 : 0)

#endif /* TEST_REPORT_H */
