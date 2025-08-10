/*
 * test_addressing_and_handlers.c - UserTalk Addressing and Handlers Test Suite
 */

#include "../../framework/test_framework.h"
#include "../../../portable/cli_executor.h"

// Test address-of @ and dereference ^ with a table
bool test_address_and_dereference(void) {
    test_setup();

    const char* script =
        "local(t, adr); new(tableType, @t); t.value = 99; adr = @t.value; return(adr^)";

    usertalk_execution_t* execution = cli_create_execution_context();
    TEST_ASSERT(execution != NULL, "Failed to create execution context");

    TEST_ASSERT(cli_compile_script(script, execution), "Failed to compile addressing script");
    TEST_ASSERT(cli_execute_compiled_script(execution), "Failed to execute addressing script");

    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from addressing script");
    TEST_ASSERT_STR_EQUAL("99", result, "Dereference result mismatch");

    cli_free(result);
    cli_free_execution_context(execution);
    test_teardown();
    TEST_PASS("Address-of and dereference work correctly");
}

// Test defining and invoking a handler with defaults and named parameter override
bool test_on_handler_named_params(void) {
    test_setup();

    // Define a handler and call it with a named parameter override
    const char* script =
        "on greet(name=\"World\") { return(\"Hello, \" + name) } "
        "return(greet(name: \"Jake\"))";

    usertalk_execution_t* execution = cli_create_execution_context();
    TEST_ASSERT(execution != NULL, "Failed to create execution context");

    TEST_ASSERT(cli_compile_script(script, execution), "Failed to compile handler script");
    TEST_ASSERT(cli_execute_compiled_script(execution), "Failed to execute handler script");

    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from handler script");
    TEST_ASSERT_STR_EQUAL("Hello, Jake", result, "Handler named param result mismatch");

    cli_free(result);
    cli_free_execution_context(execution);
    test_teardown();
    TEST_PASS("on-handler with defaults and named params works correctly");
}
