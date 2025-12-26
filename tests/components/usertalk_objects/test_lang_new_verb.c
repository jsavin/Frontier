/*
 * test_lang_new_verb.c - lang.new() Verb Test Suite
 *
 * Tests for the lang.new() verb which creates tables in headless mode.
 * This is foundational work for Issue #166 (UserTalk integration tests).
 */

#include "../../framework/test_framework.h"
#include "../../../portable/cli_executor.h"

// Test basic table creation with lang.new()
bool test_lang_new_creates_table(void) {
    test_setup();

    const char* script = "local (t); lang.new(tableType, @t); return (defined(t))";

    usertalk_execution_t* execution = cli_create_execution_context();
    if (!execution) {
        TEST_ASSERT(false, "Failed to create execution context");
    }

    bool compiled = cli_compile_script(script, execution);
    TEST_ASSERT(compiled, "Failed to compile lang.new() script");

    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute lang.new() script");

    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from lang.new() script");
    TEST_ASSERT_STR_EQUAL("true", result, "lang.new() should create a defined table");

    cli_free(result);
    cli_free_execution_context(execution);
    test_teardown();
    TEST_PASS("lang.new() creates table successfully");
}

// Test table mutation after creation
bool test_lang_new_table_mutation(void) {
    test_setup();

    const char* script = "local (t); lang.new(tableType, @t); t.val=1; return (t.val + sizeOf(t))";

    usertalk_execution_t* execution = cli_create_execution_context();
    if (!execution) {
        TEST_ASSERT(false, "Failed to create execution context");
    }

    bool compiled = cli_compile_script(script, execution);
    TEST_ASSERT(compiled, "Failed to compile table mutation script");

    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute table mutation script");

    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from table mutation script");
    TEST_ASSERT_STR_EQUAL("2", result, "Table mutation should work (t.val=1, sizeOf(t)=1, sum=2)");

    cli_free(result);
    cli_free_execution_context(execution);
    test_teardown();
    TEST_PASS("Table mutation after lang.new() works correctly");
}

// Test multiple table members
bool test_lang_new_multiple_members(void) {
    test_setup();

    const char* script = "local (t); lang.new(tableType, @t); t.a=1; t.b=2; t.c=3; return (sizeOf(t))";

    usertalk_execution_t* execution = cli_create_execution_context();
    if (!execution) {
        TEST_ASSERT(false, "Failed to create execution context");
    }

    bool compiled = cli_compile_script(script, execution);
    TEST_ASSERT(compiled, "Failed to compile multiple members script");

    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute multiple members script");

    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from multiple members script");
    TEST_ASSERT_STR_EQUAL("3", result, "Table should have 3 members");

    cli_free(result);
    cli_free_execution_context(execution);
    test_teardown();
    TEST_PASS("Multiple table members work correctly");
}

// Test string members in table
bool test_lang_new_string_members(void) {
    test_setup();

    const char* script = "local (t); lang.new(tableType, @t); t.x=\"hello\"; return (sizeOf(t.x))";

    usertalk_execution_t* execution = cli_create_execution_context();
    if (!execution) {
        TEST_ASSERT(false, "Failed to create execution context");
    }

    bool compiled = cli_compile_script(script, execution);
    TEST_ASSERT(compiled, "Failed to compile string members script");

    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute string members script");

    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from string members script");
    TEST_ASSERT_STR_EQUAL("5", result, "String \"hello\" should have size 5");

    cli_free(result);
    cli_free_execution_context(execution);
    test_teardown();
    TEST_PASS("String members in tables work correctly");
}
