/*
 * test_basic_types.c - Basic UserTalk Types Test Suite
 * 
 * Tests for fundamental UserTalk types: strings, numbers, booleans
 * 
 * UserTalk Syntax Patterns Used:
 * - Variable declaration: local(var1, var2, var3)
 * - Variable assignment: var = value
 * - Explicit returns: return(expression)
 * - String literals: "string" (double quotes, like C)
 * - Character literals: 'c' (single quotes, like C)
 * - String concatenation: + operator (e.g., "hello" + " " + "world")
 * - Number increment/decrement: ++ and -- operators
 * - No trailing semicolons unless multiple statements on one line
 */

#include "../../framework/test_framework.h"
#include "../../../portable/cli_executor.h"

// Test string objects
bool test_string_objects(void) {
    test_setup();
    
    // Test string creation and retrieval
    const char* script = "local(myString); myString = \"Hello, World!\"; return(myString)";
    usertalk_execution_t* execution = cli_create_execution_context();
    
    if (!execution) {
        TEST_ASSERT(false, "Failed to create execution context");
    }
    
    bool compiled = cli_compile_script(script, execution);
    TEST_ASSERT(compiled, "Failed to compile string script");
    
    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute string script");
    
    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from string script");
    TEST_ASSERT_STR_EQUAL("Hello, World!", result, "String result mismatch");
    
    cli_free(result);
    cli_free_execution_context(execution);
    
    test_teardown();
    TEST_PASS("String objects work correctly");
}

// Test number objects
bool test_number_objects(void) {
    test_setup();
    
    // Test number operations
    const char* script = "local(x, y); x = 10; y = 20; return(x + y)";
    usertalk_execution_t* execution = cli_create_execution_context();
    
    if (!execution) {
        TEST_ASSERT(false, "Failed to create execution context");
    }
    
    bool compiled = cli_compile_script(script, execution);
    TEST_ASSERT(compiled, "Failed to compile number script");
    
    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute number script");
    
    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from number script");
    TEST_ASSERT_STR_EQUAL("30", result, "Number result mismatch");
    
    cli_free(result);
    cli_free_execution_context(execution);
    
    test_teardown();
    TEST_PASS("Number objects work correctly");
}

// Test boolean objects
bool test_boolean_objects(void) {
    test_setup();
    
    // Test boolean operations
    const char* script = "local(x, y); x = 5; y = 10; return(x < y)";
    usertalk_execution_t* execution = cli_create_execution_context();
    
    if (!execution) {
        TEST_ASSERT(false, "Failed to create execution context");
    }
    
    bool compiled = cli_compile_script(script, execution);
    TEST_ASSERT(compiled, "Failed to compile boolean script");
    
    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute boolean script");
    
    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from boolean script");
    TEST_ASSERT_STR_EQUAL("true", result, "Boolean result mismatch");
    
    cli_free(result);
    cli_free_execution_context(execution);
    
    test_teardown();
    TEST_PASS("Boolean objects work correctly");
}

// Test increment/decrement operators
bool test_increment_decrement(void) {
    test_setup();
    
    // Test increment and decrement
    const char* script = "local(x); x = 5; x++; return(x)";
    usertalk_execution_t* execution = cli_create_execution_context();
    
    if (!execution) {
        TEST_ASSERT(false, "Failed to create execution context");
    }
    
    bool compiled = cli_compile_script(script, execution);
    TEST_ASSERT(compiled, "Failed to compile increment script");
    
    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute increment script");
    
    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from increment script");
    TEST_ASSERT_STR_EQUAL("6", result, "Increment result mismatch");
    
    cli_free(result);
    cli_free_execution_context(execution);
    
    test_teardown();
    TEST_PASS("Increment/decrement operators work correctly");
}

// Test string concatenation
bool test_string_concatenation(void) {
    test_setup();
    
    // Test string concatenation with + operator
    const char* script = "local(hello, world); hello = \"Hello\"; world = \"World\"; return(hello + \", \" + world + \"!\")";
    usertalk_execution_t* execution = cli_create_execution_context();
    
    if (!execution) {
        TEST_ASSERT(false, "Failed to create execution context");
    }
    
    bool compiled = cli_compile_script(script, execution);
    TEST_ASSERT(compiled, "Failed to compile concatenation script");
    
    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute concatenation script");
    
    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from concatenation script");
    TEST_ASSERT_STR_EQUAL("Hello, World!", result, "String concatenation result mismatch");
    
    cli_free(result);
    cli_free_execution_context(execution);
    
    test_teardown();
    TEST_PASS("String concatenation works correctly");
}
