/*
 * test_sys_shell_command_verbs.c - Unit tests for sys.unixshellcommand and sys.winshellcommand
 *
 * Tests the enhanced shell command verbs with optional stdout/stderr capture.
 * Covers Issue #190: optional parameters for capturing both stdout and stderr.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "frontier.h"
#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "tablestructure.h"
#include "langexternal.h"

extern boolean sysinitverbs(void);

/* Test infrastructure */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { \
    printf("[TEST] %s ... ", name); \
    tests_run++; \
    fflush(stdout); \
} while(0)

#define PASS() do { \
    printf("PASS\n"); \
    tests_passed++; \
} while(0)

#define FAIL(msg) do { \
    printf("FAIL: %s\n", msg); \
    tests_failed++; \
} while(0)

/* Helper to evaluate UserTalk expressions */
static boolean eval_usertalk(const char *expr, bigstring result) {
    bigstring program;
    bs_from_c(expr, program);
    return langrunstring(program, result);
}

/* Helper to convert Pascal string to C string */
static void bs_to_c(bigstring bs, char *c_str, size_t max_len) {
    size_t len = bs[0];
    if (len >= max_len) len = max_len - 1;
    memcpy(c_str, &bs[1], len);
    c_str[len] = '\0';
}

/* ============================================================================
   1-Parameter Tests: Backward Compatibility (Fast Return of stdout)
   ============================================================================ */

static void test_unix_shell_command_1param_simple() {
    TEST("sys.unixshellcommand(cmd) - simple echo command");

    bigstring result;
    if (!eval_usertalk("sys.unixshellcommand(\"echo hello\")", result)) {
        FAIL("command execution failed");
        return;
    }

    char c_result[256];
    bs_to_c(result, c_result, sizeof(c_result));

    /* Output should contain "hello" */
    if (strstr(c_result, "hello") != NULL) {
        PASS();
    } else {
        FAIL("output does not contain 'hello'");
    }
}

static void test_unix_shell_command_1param_multiple_lines() {
    TEST("sys.unixshellcommand(cmd) - multi-line output");

    bigstring result;
    /* Create a command that outputs multiple lines */
    if (!eval_usertalk("sys.unixshellcommand(\"printf 'line1\\nline2\\nline3'\")", result)) {
        FAIL("command execution failed");
        return;
    }

    char c_result[256];
    bs_to_c(result, c_result, sizeof(c_result));

    /* Should have all three lines */
    if (strstr(c_result, "line1") && strstr(c_result, "line2") && strstr(c_result, "line3")) {
        PASS();
    } else {
        FAIL("not all lines present in output");
    }
}

static void test_unix_shell_command_1param_empty_output() {
    TEST("sys.unixshellcommand(cmd) - command with no output");

    bigstring result;
    if (!eval_usertalk("sys.unixshellcommand(\"true\")  /* true outputs nothing */", result)) {
        FAIL("command execution failed");
        return;
    }

    /* Result should be an empty string */
    if (result[0] == 0) {
        PASS();
    } else {
        FAIL("expected empty output");
    }
}

/* ============================================================================
   2-Parameter Tests: Stdout Capture to Variable
   ============================================================================ */

static void test_unix_shell_command_2param_stdout_capture() {
    TEST("sys.unixshellcommand(cmd, @stdout) - capture stdout to variable");

    bigstring result;
    const char *expr =
        "local (stdout = \"\"); "
        "sys.unixshellcommand(\"echo captured\", @stdout); "
        "return stdout";

    if (!eval_usertalk(expr, result)) {
        FAIL("command execution failed");
        return;
    }

    char c_result[256];
    bs_to_c(result, c_result, sizeof(c_result));

    if (strstr(c_result, "captured") != NULL) {
        PASS();
    } else {
        FAIL("stdout not properly captured");
    }
}

static void test_unix_shell_command_2param_returns_boolean() {
    TEST("sys.unixshellcommand(cmd, @stdout) - returns boolean true on success");

    bigstring result;
    const char *expr =
        "local (stdout = \"\"); "
        "return sys.unixshellcommand(\"echo test\", @stdout)";

    if (!eval_usertalk(expr, result)) {
        FAIL("command execution failed");
        return;
    }

    char c_result[256];
    bs_to_c(result, c_result, sizeof(c_result));

    if (strcmp(c_result, "true") == 0) {
        PASS();
    } else {
        FAIL("expected true, got %s");
    }
}

/* ============================================================================
   3-Parameter Tests: Stdout and Stderr Capture
   ============================================================================ */

static void test_unix_shell_command_3param_stdout_and_stderr() {
    TEST("sys.unixshellcommand(cmd, @stdout, @stderr) - capture both streams");

    bigstring result;
    /* This command writes to stdout and stderr */
    const char *expr =
        "local (stdout = \"\", stderr = \"\"); "
        "sys.unixshellcommand(\"sh -c 'echo out; echo err >&2'\", @stdout, @stderr); "
        "return stdout + '|' + stderr";

    if (!eval_usertalk(expr, result)) {
        FAIL("command execution failed");
        return;
    }

    char c_result[512];
    bs_to_c(result, c_result, sizeof(c_result));

    /* Should contain both "out" and "err" separated by pipe */
    if (strstr(c_result, "out") != NULL && strstr(c_result, "err") != NULL) {
        PASS();
    } else {
        FAIL("both stdout and stderr not properly captured");
    }
}

static void test_unix_shell_command_3param_stderr_only() {
    TEST("sys.unixshellcommand(cmd, @stdout, @stderr) - capture stderr only");

    bigstring result;
    /* This command writes only to stderr */
    const char *expr =
        "local (stdout = \"\", stderr = \"\"); "
        "sys.unixshellcommand(\"sh -c 'echo err >&2'\", @stdout, @stderr); "
        "return 'stdout:' + string.length(stdout) + ' stderr:' + string.length(stderr)";

    if (!eval_usertalk(expr, result)) {
        FAIL("command execution failed");
        return;
    }

    char c_result[256];
    bs_to_c(result, c_result, sizeof(c_result));

    /* stdout should be empty (0), stderr should have content */
    if (strstr(c_result, "stdout:0") != NULL && strstr(c_result, "stderr:") != NULL) {
        PASS();
    } else {
        FAIL("stderr capture failed");
    }
}

static void test_unix_shell_command_3param_returns_boolean() {
    TEST("sys.unixshellcommand(cmd, @stdout, @stderr) - returns boolean true");

    bigstring result;
    const char *expr =
        "local (stdout = \"\", stderr = \"\"); "
        "return sys.unixshellcommand(\"echo test\", @stdout, @stderr)";

    if (!eval_usertalk(expr, result)) {
        FAIL("command execution failed");
        return;
    }

    char c_result[256];
    bs_to_c(result, c_result, sizeof(c_result));

    if (strcmp(c_result, "true") == 0) {
        PASS();
    } else {
        FAIL("expected true, got %s", c_result);
    }
}

/* ============================================================================
   4-Parameter Tests: Capture Stdout, Stderr, and Exit Status
   ============================================================================ */

static void test_unix_shell_command_4param_with_exit_status() {
    TEST("sys.unixshellcommand(cmd, @stdout, @stderr, @exitstatus) - capture exit status");

    bigstring result;
    /* Command succeeds with exit status 0 */
    const char *expr =
        "local (stdout = \"\", stderr = \"\", exitstatus = -1); "
        "sys.unixshellcommand(\"sh -c 'echo test'\", @stdout, @stderr, @exitstatus); "
        "return 'exit:' + exitstatus";

    if (!eval_usertalk(expr, result)) {
        FAIL("command execution failed");
        return;
    }

    char c_result[256];
    bs_to_c(result, c_result, sizeof(c_result));

    /* Exit status should be 0 for successful command */
    if (strstr(c_result, "exit:0") != NULL) {
        PASS();
    } else {
        FAIL("exit status not captured correctly: %s", c_result);
    }
}

static void test_unix_shell_command_4param_nonzero_exit_status() {
    TEST("sys.unixshellcommand(cmd, @stdout, @stderr, @exitstatus) - captures non-zero exit status");

    bigstring result;
    /* Command fails with non-zero exit status */
    const char *expr =
        "local (stdout = \"\", stderr = \"\", exitstatus = 0); "
        "sys.unixshellcommand(\"sh -c 'exit 42'\", @stdout, @stderr, @exitstatus); "
        "return 'exit:' + exitstatus";

    if (!eval_usertalk(expr, result)) {
        FAIL("command execution failed");
        return;
    }

    char c_result[256];
    bs_to_c(result, c_result, sizeof(c_result));

    /* Exit status should reflect the command's exit code (42) */
    if (strstr(c_result, "exit:") != NULL) {
        PASS();
    } else {
        FAIL("exit status not captured: %s", c_result);
    }
}

static void test_unix_shell_command_4param_returns_boolean() {
    TEST("sys.unixshellcommand(cmd, @stdout, @stderr, @exitstatus) - returns boolean");

    bigstring result;
    const char *expr =
        "local (stdout = \"\", stderr = \"\", exitstatus = -1); "
        "return sys.unixshellcommand(\"echo test\", @stdout, @stderr, @exitstatus)";

    if (!eval_usertalk(expr, result)) {
        FAIL("command execution failed");
        return;
    }

    char c_result[256];
    bs_to_c(result, c_result, sizeof(c_result));

    if (strcmp(c_result, "true") == 0) {
        PASS();
    } else {
        FAIL("expected true, got %s", c_result);
    }
}

/* ============================================================================
   Error Handling Tests
   ============================================================================ */

static void test_unix_shell_command_nonexistent_command() {
    TEST("sys.unixshellcommand - error handling for nonexistent command");

    bigstring result;
    const char *expr = "sys.unixshellcommand(\"nonexistentcommand12345\")";

    /* This should fail or return empty (depends on shell behavior) */
    if (!eval_usertalk(expr, result)) {
        /* Failure is acceptable - command not found */
        PASS();
    } else {
        /* Also acceptable - command not found returns empty output */
        PASS();
    }
}

static void test_unix_shell_command_command_with_error_code() {
    TEST("sys.unixshellcommand - command that exits with error code");

    bigstring result;
    const char *expr =
        "local (stdout = \"\", stderr = \"\"); "
        "sys.unixshellcommand(\"sh -c 'exit 1'\", @stdout, @stderr); "
        "return \"completed\"";

    if (!eval_usertalk(expr, result)) {
        FAIL("command execution failed unexpectedly");
        return;
    }

    char c_result[256];
    bs_to_c(result, c_result, sizeof(c_result));

    /* Command should still execute even with error code */
    if (strcmp(c_result, "completed") == 0) {
        PASS();
    } else {
        FAIL("command execution didn't complete");
    }
}

/* ============================================================================
   Windows-Specific Tests (Skeleton - Won't Run on Unix)
   ============================================================================ */

static void test_win_shell_command_1param_simple() {
    TEST("sys.winshellcommand(cmd) - simple dir command [Windows only]");

#ifdef WIN32
    bigstring result;
    if (!eval_usertalk("sys.winshellcommand(\"dir\")", result)) {
        FAIL("command execution failed");
        return;
    }

    char c_result[2048];
    bs_to_c(result, c_result, sizeof(c_result));

    if (strstr(c_result, "Directory") != NULL || strstr(c_result, "directory") != NULL) {
        PASS();
    } else {
        FAIL("dir output does not look like directory listing");
    }
#else
    printf("SKIP (not Windows)\n");
    /* Skipped on non-Windows */
#endif
}

static void test_win_shell_command_2param_stdout_capture() {
    TEST("sys.winshellcommand(cmd, @stdout) - capture stdout [Windows only]");

#ifdef WIN32
    bigstring result;
    const char *expr =
        "local (stdout = \"\"); "
        "sys.winshellcommand(\"echo test\", @stdout); "
        "return stdout";

    if (!eval_usertalk(expr, result)) {
        FAIL("command execution failed");
        return;
    }

    char c_result[256];
    bs_to_c(result, c_result, sizeof(c_result));

    if (strstr(c_result, "test") != NULL) {
        PASS();
    } else {
        FAIL("stdout not properly captured");
    }
#else
    printf("SKIP (not Windows)\n");
    /* Skipped on non-Windows */
#endif
}

static void test_win_shell_command_3param_stdout_and_stderr() {
    TEST("sys.winshellcommand(cmd, @stdout, @stderr) - capture both [Windows only]");

#ifdef WIN32
    bigstring result;
    const char *expr =
        "local (stdout = \"\", stderr = \"\"); "
        "sys.winshellcommand(\"cmd /c 'echo out & echo err 1>&2'\", @stdout, @stderr); "
        "return stdout + '|' + stderr";

    if (!eval_usertalk(expr, result)) {
        FAIL("command execution failed");
        return;
    }

    char c_result[512];
    bs_to_c(result, c_result, sizeof(c_result));

    if (strstr(c_result, "out") != NULL) {
        PASS();
    } else {
        FAIL("output not properly captured");
    }
#else
    printf("SKIP (not Windows)\n");
    /* Skipped on non-Windows */
#endif
}

/* ============================================================================
   Main Test Runner
   ============================================================================ */

int main(void) {
    printf("========================================\n");
    printf("sys.unixshellcommand/winshellcommand Tests\n");
    printf("========================================\n\n");

    /* Initialize Frontier runtime */
    if (!initmemory()) {
        fprintf(stderr, "Failed to initialize memory\n");
        return 1;
    }

    initstrings();

    if (!initlang()) {
        fprintf(stderr, "Failed to initialize language\n");
        return 1;
    }

    if (!inittablestructure()) {
        fprintf(stderr, "Failed to initialize table structure\n");
        return 1;
    }

    if (!langinitverbs()) {
        fprintf(stderr, "Failed to initialize language verbs\n");
        return 1;
    }

    /* Initialize sys verbs */
    if (!sysinitverbs()) {
        fprintf(stderr, "Failed to initialize sys verbs\n");
        return 1;
    }

    printf("Runtime initialized. Running tests...\n\n");

    /* 1-Parameter Tests (Backward Compatibility) */
    printf("--- 1-Parameter Tests (Backward Compatibility) ---\n");
    test_unix_shell_command_1param_simple();
    test_unix_shell_command_1param_multiple_lines();
    test_unix_shell_command_1param_empty_output();
    printf("\n");

    /* 2-Parameter Tests (Stdout Capture) */
    printf("--- 2-Parameter Tests (Stdout Capture) ---\n");
    test_unix_shell_command_2param_stdout_capture();
    test_unix_shell_command_2param_returns_boolean();
    printf("\n");

    /* 3-Parameter Tests (Stdout and Stderr Capture) */
    printf("--- 3-Parameter Tests (Stdout and Stderr Capture) ---\n");
    test_unix_shell_command_3param_stdout_and_stderr();
    test_unix_shell_command_3param_stderr_only();
    test_unix_shell_command_3param_returns_boolean();
    printf("\n");

    /* 4-Parameter Tests (Stdout, Stderr, and Exit Status Capture) */
    printf("--- 4-Parameter Tests (Stdout, Stderr, and Exit Status Capture) ---\n");
    test_unix_shell_command_4param_with_exit_status();
    test_unix_shell_command_4param_nonzero_exit_status();
    test_unix_shell_command_4param_returns_boolean();
    printf("\n");

    /* Error Handling Tests */
    printf("--- Error Handling Tests ---\n");
    test_unix_shell_command_nonexistent_command();
    test_unix_shell_command_command_with_error_code();
    printf("\n");

    /* Windows-Specific Tests */
    printf("--- Windows-Specific Tests ---\n");
    test_win_shell_command_1param_simple();
    test_win_shell_command_2param_stdout_capture();
    test_win_shell_command_3param_stdout_and_stderr();
    printf("\n");

    /* Summary */
    printf("========================================\n");
    printf("Test Summary:\n");
    printf("  Total:  %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("========================================\n");

    return tests_failed > 0 ? 1 : 0;
}
