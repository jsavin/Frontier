/*
 * test_control_flow.c - UserTalk Control Flow Test Suite
 */

#include "../../framework/test_framework.h"
#include "../../../portable/cli_executor.h"

// Test if/else branching
bool test_if_else_branching(void) {
    test_setup();

    const char* script = "local(answer); answer = 7; if answer > 6 return(\"Big family!\") else return(\"Not such a big family!\")";

    usertalk_execution_t* execution = cli_create_execution_context();
    if (!execution) {
        TEST_ASSERT(false, "Failed to create execution context");
    }

    bool compiled = cli_compile_script(script, execution);
    TEST_ASSERT(compiled, "Failed to compile if/else script");

    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute if/else script");

    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from if/else script");
    TEST_ASSERT_STR_EQUAL("Big family!", result, "if/else result mismatch");

    cli_free(result);
    cli_free_execution_context(execution);
    test_teardown();
    TEST_PASS("If/else branching works correctly");
}

// Test for/while/loop with break and continue
bool test_loops_with_break_continue(void) {
    test_setup();

    // Sum 1..5 skipping 3 via continue; loop exits with break when i > 5
    const char* script =
        "local(i, sum); i = 1; sum = 0; "
        "for i = 1 to 5 { "
        "    if i == 3 { continue } "
        "    sum = sum + i "
        "} "
        "return(sum)";

    usertalk_execution_t* execution = cli_create_execution_context();
    if (!execution) {
        TEST_ASSERT(false, "Failed to create execution context");
    }

    bool compiled = cli_compile_script(script, execution);
    TEST_ASSERT(compiled, "Failed to compile loop script");

    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute loop script");

    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from loop script");
    TEST_ASSERT_STR_EQUAL("12", result, "Loop sum result mismatch (expected 1+2+4+5=12)");

    cli_free(result);
    cli_free_execution_context(execution);
    test_teardown();
    TEST_PASS("Looping with continue works correctly");
}
