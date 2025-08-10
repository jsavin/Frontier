/*
 * test_operators.c - UserTalk Operators Test Suite
 */

#include "../../framework/test_framework.h"
#include "../../../portable/cli_executor.h"

// Arithmetic and comparison operators
bool test_arithmetic_and_comparison(void) {
    test_setup();

    const char* script = "local(a, b); a = 5; b = 2; return((a + b == 7) and (a - b == 3) and (a * b == 10) and (a % b == 1) and (a / b > 2))";

    usertalk_execution_t* execution = cli_create_execution_context();
    TEST_ASSERT(execution != NULL, "Failed to create execution context");

    TEST_ASSERT(cli_compile_script(script, execution), "Failed to compile operators script");
    TEST_ASSERT(cli_execute_compiled_script(execution), "Failed to execute operators script");

    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from operators script");
    TEST_ASSERT_STR_EQUAL("true", result, "Arithmetic/comparison evaluation mismatch");

    cli_free(result);
    cli_free_execution_context(execution);
    test_teardown();
    TEST_PASS("Arithmetic and comparison operators work correctly");
}

// Logical and string operators
bool test_logical_and_string_ops(void) {
    test_setup();

    const char* script = "local(s); s = \"Frontier\"; return((\"Front\" beginsWith s) or (s beginsWith \"Front\"))";

    usertalk_execution_t* execution = cli_create_execution_context();
    TEST_ASSERT(execution != NULL, "Failed to create execution context");

    TEST_ASSERT(cli_compile_script(script, execution), "Failed to compile string ops script");
    TEST_ASSERT(cli_execute_compiled_script(execution), "Failed to execute string ops script");

    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from string ops script");
    TEST_ASSERT_STR_EQUAL("true", result, "String operator evaluation mismatch");

    cli_free(result);
    cli_free_execution_context(execution);
    test_teardown();
    TEST_PASS("Logical and string operators work correctly");
}
