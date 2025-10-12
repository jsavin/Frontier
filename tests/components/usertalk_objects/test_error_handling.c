/*
 * test_error_handling.c - UserTalk Error Handling Test Suite
 */

#include "../../framework/test_framework.h"
#include "../../../portable/cli_executor.h"

// try/else captures error and exposes tryError string
bool test_try_else_error_capture(void) {
    test_setup();

    const char* script =
        "try { file.delete(\"/definitely/does/not/exist\") } "
        "else { return(tryError != \"\") }";

    usertalk_execution_t* execution = cli_create_execution_context();
    TEST_ASSERT(execution != NULL, "Failed to create execution context");

    TEST_ASSERT(cli_compile_script(script, execution), "Failed to compile try/else script");
    TEST_ASSERT(cli_execute_compiled_script(execution), "Failed to execute try/else script");

    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from try/else script");
    TEST_ASSERT_STR_EQUAL("true", result, "tryError not captured as expected");

    cli_free(result);
    cli_free_execution_context(execution);
    test_teardown();
    TEST_PASS("try/else error capture works correctly");
}

// verbs that return status should be checkable under if
bool test_status_return_handling(void) {
    test_setup();

    const char* script =
        "local(f); f = \"/definitely/does/not/exist\"; "
        "if file.open(f) return(\"opened\") else return(\"failed\")";

    usertalk_execution_t* execution = cli_create_execution_context();
    TEST_ASSERT(execution != NULL, "Failed to create execution context");

    TEST_ASSERT(cli_compile_script(script, execution), "Failed to compile status script");
    TEST_ASSERT(cli_execute_compiled_script(execution), "Failed to execute status script");

    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from status script");
    TEST_ASSERT_STR_EQUAL("failed", result, "Status return handling mismatch");

    cli_free(result);
    cli_free_execution_context(execution);
    test_teardown();
    TEST_PASS("Status-return verbs are handled correctly");
}
