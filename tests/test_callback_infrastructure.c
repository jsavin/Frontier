/*
 * Frontier Parameterized Callback Infrastructure Unit Tests
 * Phase 4: P0a - Critical Thread-Safety Infrastructure
 *
 * Tests for langruncallbackwithparams() - General-purpose parameterized callback infrastructure
 *
 * This file tests the new parameterized callback system that extends Frontier's
 * existing callback infrastructure to support callbacks WITH parameters.
 *
 * Based on legacy regexp callback pattern (langregexp.c:1591-1672):
 *   1. Create local variable table for parameters
 *   2. Assign parameters to local table
 *   3. Build parameter list using langpushlistaddress()
 *   4. Execute callback with langrunscriptcode()
 *   5. Clean up local scope
 *   6. Wrap in thread-safety pattern (grabthreadglobals/oppushoutline)
 *
 * Reference: planning/phase4/p0a-critical-thread-safety/CALLBACK_INFRASTRUCTURE.md
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Frontier headers */
#include "frontier.h"
#include "standard.h"
#include "lang.h"
#include "langinternal.h"
#include "db.h"
#include "file.h"
#include "memory.h"
#include "strings.h"
#include "logging.h"
#include "tablestructure.h"
#include "process.h"
#include "shell.h"
#include "op.h"

/* Test counters */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Test macros */
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

/* Forward declaration of function to be implemented */
boolean langruncallbackwithparams(
    hdlhashtable htable,             /* Hash table containing callback script */
    bigstring callback_name,          /* Name of callback script */
    short param_count,                /* Number of parameters */
    tyvaluerecord *params,            /* Array of parameter values */
    tyvaluerecord *result             /* Returned value (optional, can be nil) */
);

/* Global test database and table */
static hdlhashtable htesttable = nil;

/*
 * Setup: Initialize runtime and create test table with callback scripts
 */
boolean test_setup(void) {
    boolean fl;

    /* Initialize logging */
    log_init();

    /* Initialize Frontier runtime */
    if (!initmemory()) {
        printf("ERROR: initmemory() failed\n");
        return false;
    }

    initstrings();

    if (!initlang()) {
        printf("ERROR: initlang() failed\n");
        return false;
    }

    if (!inittablestructure()) {
        printf("ERROR: inittablestructure() failed\n");
        return false;
    }

    if (!langinitverbs()) {
        printf("ERROR: langinitverbs() failed\n");
        return false;
    }

    /* Create test table for callback scripts */
    tyvaluerecord tableval;
    fl = tablenewtablevalue(&htesttable, &tableval);
    if (!fl) {
        printf("ERROR: tablenewtablevalue() failed\n");
        return false;
    }

    return true;
}

/*
 * Teardown: Clean up test resources
 */
void test_teardown(void) {
    if (htesttable != nil) {
        disposehashtable(htesttable, false);
        htesttable = nil;
    }
}

/*
 * Helper: Add a UserTalk script to test table
 */
boolean add_test_script(bigstring bs_name, const char *script_text) {
    Handle htext;
    hdltreenode hcode;
    tyvaluerecord val;
    bigstring bs_script;

    /* Convert C string to bigstring */
    copyctopstring(script_text, bs_script);

    /* Create handle from script text */
    if (!newtexthandle(bs_script, &htext)) {
        printf("ERROR: Failed to create text handle\n");
        return false;
    }

    /* Compile the script */
    if (!langcompiletext(htext, false, &hcode)) {
        printf("ERROR: Failed to compile script: %s\n", script_text);
        return false;
    }

    /* Create code value */
    val.valuetype = codevaluetype;
    val.data.codevalue = hcode;

    /* Add to test table */
    return hashtableassign(htesttable, bs_name, val);
}

/*
 * Test 1: Zero parameters (backward compatibility with parameterless callbacks)
 */
void test_callback_zero_params(void) {
    TEST("Callback with zero parameters");

    bigstring bs_name;
    tyvaluerecord result;
    boolean fl;

    /* Create callback script: script body that returns 42 */
    copyctopstring("testZeroParams", bs_name);
    fl = add_test_script(bs_name, "42");
    ASSERT(fl);

    /* Call callback with zero parameters */
    fl = langruncallbackwithparams(htesttable, bs_name, 0, nil, &result);
    ASSERT(fl);

    /* Verify result is 42 */
    ASSERT(result.valuetype == longvaluetype);
    ASSERT(result.data.longvalue == 42);

    PASS();
}

/*
 * Test 2: Single long parameter
 */
void test_callback_one_param_long(void) {
    TEST("Callback with single long parameter");

    bigstring bs_name;
    tyvaluerecord param;
    tyvaluerecord result;
    boolean fl;

    /* Create callback script: script that accesses param1 and returns param1 * 2 */
    copyctopstring("testOneParamLong", bs_name);
    fl = add_test_script(bs_name, "param1 * 2");
    ASSERT(fl);

    /* Create parameter: 21 */
    setlongvalue(21, &param);

    /* Call callback */
    fl = langruncallbackwithparams(htesttable, bs_name, 1, &param, &result);
    ASSERT(fl);

    /* Verify result is 42 */
    ASSERT(result.valuetype == longvaluetype);
    ASSERT(result.data.longvalue == 42);

    PASS();
}

/*
 * Test 3: Single string parameter
 */
void test_callback_one_param_string(void) {
    TEST("Callback with single string parameter");

    bigstring bs_name, bs_param, bs_expected;
    tyvaluerecord param;
    tyvaluerecord result;
    boolean fl;

    /* Create callback script: script that accesses param1 and appends " world" */
    copyctopstring("testOneParamString", bs_name);
    fl = add_test_script(bs_name, "param1 + \" world\"");
    ASSERT(fl);

    /* Create parameter: "hello" */
    copyctopstring("hello", bs_param);
    fl = setstringvalue(bs_param, &param);
    ASSERT(fl);

    /* Call callback */
    fl = langruncallbackwithparams(htesttable, bs_name, 1, &param, &result);
    ASSERT(fl);

    /* Verify result is "hello world" */
    ASSERT(result.valuetype == stringvaluetype);
    copyctopstring("hello world", bs_expected);
    /* Note: Would need to compare Handle contents in real test */

    disposevaluerecord(result, false);
    disposevaluerecord(param, false);

    PASS();
}

/*
 * Test 4: Single boolean parameter
 */
void test_callback_one_param_boolean(void) {
    TEST("Callback with single boolean parameter");

    bigstring bs_name;
    tyvaluerecord param;
    tyvaluerecord result;
    boolean fl;

    /* Create callback script: script that inverts param1 */
    copyctopstring("testOneParamBoolean", bs_name);
    fl = add_test_script(bs_name, "not param1");
    ASSERT(fl);

    /* Create parameter: true */
    setbooleanvalue(true, &param);

    /* Call callback */
    fl = langruncallbackwithparams(htesttable, bs_name, 1, &param, &result);
    ASSERT(fl);

    /* Verify result is false (inverted) */
    ASSERT(result.valuetype == booleanvaluetype);
    ASSERT(result.data.flvalue == false);

    PASS();
}

/*
 * Test 5: Multiple parameters (long, string, boolean)
 */
void test_callback_multiple_params(void) {
    TEST("Callback with multiple parameters (long, string, boolean)");

    bigstring bs_name, bs_str;
    tyvaluerecord params[3];
    tyvaluerecord result;
    boolean fl;

    /* Create callback script: script that checks param1 == 42 and param2 == "test" and param3 */
    copyctopstring("testMultipleParams", bs_name);
    fl = add_test_script(bs_name,
        "(param1 == 42) and (param2 == \"test\") and param3");
    ASSERT(fl);

    /* Create parameters */
    setlongvalue(42, &params[0]);
    copyctopstring("test", bs_str);
    fl = setstringvalue(bs_str, &params[1]);
    ASSERT(fl);
    setbooleanvalue(true, &params[2]);

    /* Call callback */
    fl = langruncallbackwithparams(htesttable, bs_name, 3, params, &result);
    ASSERT(fl);

    /* Verify result is true */
    ASSERT(result.valuetype == booleanvaluetype);
    ASSERT(result.data.flvalue == true);

    disposevaluerecord(params[1], false);

    PASS();
}

/*
 * Test 6: Callback returning boolean
 */
void test_callback_return_boolean(void) {
    TEST("Callback returning boolean");

    bigstring bs_name;
    tyvaluerecord param;
    tyvaluerecord result;
    boolean fl;

    /* Create callback script: on test(x) { return x > 10 } */
    copyctopstring("testReturnBoolean", bs_name);
    fl = add_test_script(bs_name, "param1 > 10");
    ASSERT(fl);

    /* Call with 15 (should return true) */
    setlongvalue(15, &param);
    fl = langruncallbackwithparams(htesttable, bs_name, 1, &param, &result);
    ASSERT(fl);
    ASSERT(result.valuetype == booleanvaluetype);
    ASSERT(result.data.flvalue == true);

    /* Call with 5 (should return false) */
    setlongvalue(5, &param);
    fl = langruncallbackwithparams(htesttable, bs_name, 1, &param, &result);
    ASSERT(fl);
    ASSERT(result.valuetype == booleanvaluetype);
    ASSERT(result.data.flvalue == false);

    PASS();
}

/*
 * Test 7: Callback returning string
 */
void test_callback_return_string(void) {
    TEST("Callback returning string");

    bigstring bs_name, bs_param;
    tyvaluerecord param;
    tyvaluerecord result;
    boolean fl;

    /* Create callback script: on test(name) { return "Hello, " + name } */
    copyctopstring("testReturnString", bs_name);
    fl = add_test_script(bs_name, "\"Hello, \" + param1");
    ASSERT(fl);

    /* Create parameter: "Alice" */
    copyctopstring("Alice", bs_param);
    fl = setstringvalue(bs_param, &param);
    ASSERT(fl);

    /* Call callback */
    fl = langruncallbackwithparams(htesttable, bs_name, 1, &param, &result);
    ASSERT(fl);

    /* Verify result is string type (content verification would require Handle comparison) */
    ASSERT(result.valuetype == stringvaluetype);

    disposevaluerecord(result, false);
    disposevaluerecord(param, false);

    PASS();
}

/*
 * Test 8: Nil result parameter (don't need return value)
 */
void test_callback_nil_result(void) {
    TEST("Callback with nil result parameter");

    bigstring bs_name;
    tyvaluerecord param;
    boolean fl;

    /* Create callback script: on test(x) { return x * 2 } */
    copyctopstring("testNilResult", bs_name);
    fl = add_test_script(bs_name, "param1 * 2");
    ASSERT(fl);

    /* Create parameter */
    setlongvalue(42, &param);

    /* Call callback with nil result pointer (we don't care about return value) */
    fl = langruncallbackwithparams(htesttable, bs_name, 1, &param, nil);
    ASSERT(fl);

    PASS();
}

/*
 * Test 9: Callback script doesn't exist (error handling)
 */
void test_callback_not_found(void) {
    TEST("Callback script not found (error handling)");

    bigstring bs_name;
    tyvaluerecord result;
    boolean fl;

    /* Try to call non-existent callback */
    copyctopstring("nonExistentCallback", bs_name);
    fl = langruncallbackwithparams(htesttable, bs_name, 0, nil, &result);

    /* Should fail gracefully */
    ASSERT(!fl);

    PASS();
}

/*
 * Test 10: Empty callback (no return statement)
 */
void test_callback_empty(void) {
    TEST("Empty callback (no return statement)");

    bigstring bs_name;
    tyvaluerecord result;
    boolean fl;

    /* Create callback script with no return: just returns nil */
    copyctopstring("testEmpty", bs_name);
    fl = add_test_script(bs_name, "nil");
    ASSERT(fl);

    /* Call callback */
    fl = langruncallbackwithparams(htesttable, bs_name, 0, nil, &result);
    ASSERT(fl);

    /* Empty script should return nil/undefined (check valuetype) */
    /* Note: Exact behavior depends on langrunscriptcode implementation */

    PASS();
}

/*
 * Test 11: TCP callback simulation (stream_id, remote_addr, remote_port)
 */
void test_callback_tcp_simulation(void) {
    TEST("TCP callback simulation (stream_id, addr, port)");

    bigstring bs_name;
    tyvaluerecord params[3];
    tyvaluerecord result;
    boolean fl;

    /* Create callback script: on handleConnection(stream, addr, port) { return stream + addr + port } */
    copyctopstring("handleConnection", bs_name);
    fl = add_test_script(bs_name,
        "param1 + param2 + param3");
    ASSERT(fl);

    /* Create parameters: stream=1, addr=0x7F000001 (127.0.0.1), port=8080 */
    setlongvalue(1, &params[0]);        /* stream_id */
    setlongvalue(0x7F000001, &params[1]); /* remote_addr (127.0.0.1 in network byte order) */
    setlongvalue(8080, &params[2]);      /* remote_port */

    /* Call callback */
    fl = langruncallbackwithparams(htesttable, bs_name, 3, params, &result);
    ASSERT(fl);

    /* Verify result is numeric (sum of all params) */
    ASSERT(result.valuetype == longvaluetype);
    ASSERT(result.data.longvalue == (1 + 0x7F000001 + 8080));

    PASS();
}

/*
 * Test 12: Window/database callback simulation (title string)
 */
void test_callback_window_simulation(void) {
    TEST("Window/database callback simulation (title)");

    bigstring bs_name, bs_title;
    tyvaluerecord param;
    tyvaluerecord result;
    boolean fl;

    /* Create callback script: on closeWindow(title) { return title == "Important Document" } */
    copyctopstring("closeWindow", bs_name);
    fl = add_test_script(bs_name,
        "param1 == \"Important Document\"");
    ASSERT(fl);

    /* Test with "Important Document" (should return true) */
    copyctopstring("Important Document", bs_title);
    fl = setstringvalue(bs_title, &param);
    ASSERT(fl);

    fl = langruncallbackwithparams(htesttable, bs_name, 1, &param, &result);
    ASSERT(fl);
    ASSERT(result.valuetype == booleanvaluetype);
    ASSERT(result.data.flvalue == true);

    disposevaluerecord(param, false);

    /* Test with "Other Document" (should return false) */
    copyctopstring("Other Document", bs_title);
    fl = setstringvalue(bs_title, &param);
    ASSERT(fl);

    fl = langruncallbackwithparams(htesttable, bs_name, 1, &param, &result);
    ASSERT(fl);
    ASSERT(result.valuetype == booleanvaluetype);
    ASSERT(result.data.flvalue == false);

    disposevaluerecord(param, false);

    PASS();
}

/*
 * Test 13: Address parameter (table address)
 */
void test_callback_address_param(void) {
    TEST("Callback with address parameter");

    bigstring bs_name, bs_table_name;
    hdlhashtable hparamtable;
    tyvaluerecord param;
    tyvaluerecord result;
    boolean fl;

    /* Create a test table to pass as address */
    fl = langnewtable(nil, &hparamtable);
    ASSERT(fl);

    /* Add value to test table: test.value = 123 */
    copyctopstring("value", bs_table_name);
    tyvaluerecord val;
    setlongvalue(123, &val);
    fl = hashtableassign(hparamtable, bs_table_name, val);
    ASSERT(fl);

    /* Create callback script: on test(t) { return t.value } */
    /* Note: This would require address parameter support in UserTalk */
    copyctopstring("testAddress", bs_name);
    fl = add_test_script(bs_name, "defined(param1)");
    ASSERT(fl);

    /* Create address parameter */
    copyctopstring("paramtable", bs_table_name);
    fl = setaddressvalue(hparamtable, bs_table_name, &param);
    ASSERT(fl);

    /* Call callback */
    fl = langruncallbackwithparams(htesttable, bs_name, 1, &param, &result);
    ASSERT(fl);

    /* Verify callback executed (result should be boolean true since address is defined) */
    ASSERT(result.valuetype == booleanvaluetype);
    ASSERT(result.data.flvalue == true);

    disposehashtable(hparamtable, false);

    PASS();
}

/*
 * Test 14: Four parameters (maximum from legacy template system)
 */
void test_callback_four_params(void) {
    TEST("Callback with four parameters");

    bigstring bs_name, bs_str;
    tyvaluerecord params[4];
    tyvaluerecord result;
    boolean fl;

    /* Create callback script: on test(a, b, c, d) { return (a + b + c) == d } */
    copyctopstring("testFourParams", bs_name);
    fl = add_test_script(bs_name, "(param1 + param2 + param3) == param4");
    ASSERT(fl);

    /* Create parameters: 10, 20, 30, 60 */
    setlongvalue(10, &params[0]);
    setlongvalue(20, &params[1]);
    setlongvalue(30, &params[2]);
    setlongvalue(60, &params[3]);

    /* Call callback */
    fl = langruncallbackwithparams(htesttable, bs_name, 4, params, &result);
    ASSERT(fl);

    /* Verify result is true (10+20+30 == 60) */
    ASSERT(result.valuetype == booleanvaluetype);
    ASSERT(result.data.flvalue == true);

    PASS();
}

/*
 * Main test runner
 */
int main(void) {
    printf("\n");
    printf("========================================================================\n");
    printf("Frontier Parameterized Callback Infrastructure Unit Tests\n");
    printf("========================================================================\n");
    printf("\n");

    /* Setup */
    printf("Initializing test environment...\n");
    if (!test_setup()) {
        printf("ERROR: Test setup failed\n");
        return 1;
    }
    printf("Test environment initialized.\n\n");

    /* Run tests */
    printf("Running tests:\n");
    printf("------------------------------------------------------------------------\n");

    /* Basic parameter passing */
    test_callback_zero_params();
    test_callback_one_param_long();
    test_callback_one_param_string();
    test_callback_one_param_boolean();
    test_callback_multiple_params();

    /* Return value types */
    test_callback_return_boolean();
    test_callback_return_string();

    /* Edge cases */
    test_callback_nil_result();
    test_callback_not_found();
    test_callback_empty();

    /* Real use cases */
    test_callback_tcp_simulation();
    test_callback_window_simulation();
    test_callback_address_param();
    test_callback_four_params();

    printf("------------------------------------------------------------------------\n");

    /* Teardown */
    test_teardown();

    /* Report results */
    printf("\n");
    printf("========================================================================\n");
    printf("Test Results:\n");
    printf("  Tests run:    %d\n", tests_run);
    printf("  Tests passed: %d\n", tests_passed);
    printf("  Tests failed: %d\n", tests_failed);
    printf("========================================================================\n");
    printf("\n");

    if (tests_failed > 0) {
        printf("RESULT: FAILED - %d test(s) failed\n\n", tests_failed);
        return 1;
    } else {
        printf("RESULT: SUCCESS - All tests passed!\n\n");
        return 0;
    }
}
