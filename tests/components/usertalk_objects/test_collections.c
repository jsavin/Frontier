/*
 * test_collections.c - UserTalk Collection Types Test Suite
 * 
 * Tests for UserTalk collection types: arrays and records
 * 
 * Data Structure Notes:
 * - Arrays and records are heterogeneous (can contain any type)
 * - Arrays: {item1, item2, item3} - zero-based indexing
 * - Records: [key: value, key2: value2] - associative storage
 * - Complex objects can be stored in arrays/records (tables, scripts, outlines)
 */

#include "../../framework/test_framework.h"
#include "../../../portable/cli_executor.h"

// Test list (array) objects
bool test_list_objects(void) {
    test_setup();
    
    // Test list creation and operations
    const char* script = "local(myList); myList = {1, 2, 3, \"four\"}; return(string(myList))";
    usertalk_execution_t* execution = cli_create_execution_context();
    
    if (!execution) {
        TEST_ASSERT(false, "Failed to create execution context");
    }
    
    bool compiled = cli_compile_script(script, execution);
    TEST_ASSERT(compiled, "Failed to compile list script");
    
    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute list script");
    
    // Lists should return as strings in CLI context
    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from list script");
    TEST_ASSERT(strlen(result) > 0, "Empty result from list script");
    
    cli_free(result);
    cli_free_execution_context(execution);
    
    test_teardown();
    TEST_PASS("List objects work correctly");
}

// Test record objects
bool test_record_objects(void) {
    test_setup();
    
    // Test record creation and access
    const char* script = "local(myRecord); myRecord = [name: \"John\", age: 30]; return(myRecord.name)";
    usertalk_execution_t* execution = cli_create_execution_context();
    
    if (!execution) {
        TEST_ASSERT(false, "Failed to create execution context");
    }
    
    bool compiled = cli_compile_script(script, execution);
    TEST_ASSERT(compiled, "Failed to compile record script");
    
    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute record script");
    
    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from record script");
    TEST_ASSERT_STR_EQUAL("John", result, "Record field access result mismatch");
    
    cli_free(result);
    cli_free_execution_context(execution);
    
    test_teardown();
    TEST_PASS("Record objects work correctly");
}

// Test heterogeneous collections
bool test_heterogeneous_collections(void) {
    test_setup();
    
    // Test array with mixed types
    const char* script = "local(mixedArray); mixedArray = {42, \"string\", true, [key: \"value\"]}; return(string(mixedArray))";
    usertalk_execution_t* execution = cli_create_execution_context();
    
    if (!execution) {
        TEST_ASSERT(false, "Failed to create execution context");
    }
    
    bool compiled = cli_compile_script(script, execution);
    TEST_ASSERT(compiled, "Failed to compile heterogeneous array script");
    
    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute heterogeneous array script");
    
    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from heterogeneous array script");
    TEST_ASSERT(strlen(result) > 0, "Empty result from heterogeneous array script");
    
    cli_free(result);
    cli_free_execution_context(execution);
    
    test_teardown();
    TEST_PASS("Heterogeneous collections work correctly");
}

// Test nested collections
bool test_nested_collections(void) {
    test_setup();
    
    // Test nested arrays and records
    const char* script = "local(nested); nested = {[outer: \"value\"], {inner: \"nested\"}}; return(string(nested))";
    usertalk_execution_t* execution = cli_create_execution_context();
    
    if (!execution) {
        TEST_ASSERT(false, "Failed to create execution context");
    }
    
    bool compiled = cli_compile_script(script, execution);
    TEST_ASSERT(compiled, "Failed to compile nested collections script");
    
    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute nested collections script");
    
    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from nested collections script");
    TEST_ASSERT(strlen(result) > 0, "Empty result from nested collections script");
    
    cli_free(result);
    cli_free_execution_context(execution);
    
    test_teardown();
    TEST_PASS("Nested collections work correctly");
}
