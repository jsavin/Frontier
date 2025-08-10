/*
 * test_complex_objects.c - Complex UserTalk Objects Test Suite
 * 
 * Tests for complex UserTalk objects: tables, scripts, outlines, wptext
 * 
 * Object Creation Notes:
 * - Tables: new(tableType, @var) - creates new table object
 * - Scripts: script.newScriptObject(string, @var) - creates script object from string
 * - Outlines: new(outlineType, @var) - creates outline object
 * - Wptext: new(wptextType, @var) then wp.newTextObject(string, @var) - creates rich text object
 * 
 * String Conversion Notes:
 * - Complex objects need string() conversion: return(string(object))
 * - WPText: string(wptext) returns plain text (loses formatting)
 * - Outlines: string(outline) returns text representation (loses refcons)
 * - Scripts: script() executes and returns result, string(script) returns source
 * - Collections: string(array/record) returns text representation
 * 
 * Optimization Notes:
 * - Scripts can be created directly in tables: script.newScriptObject(string, @table.field)
 * - Avoids separate local variables for better memory efficiency
 * - Direct creation is faster and cleaner than separate assignment
 */

#include "../../framework/test_framework.h"
#include "../../frontier-cli/cli_executor.h"
#include "../../frontier-cli/cli_database.h"

// Test table objects
bool test_table_objects(void) {
    test_setup();
    
    // Test table creation and operations
    const char* script = "local(myTable); new(tableType, @myTable); myTable.testValue = 42; return(myTable.testValue)";
    usertalk_execution_t* execution = cli_create_execution_context();
    
    if (!execution) {
        TEST_ASSERT(false, "Failed to create execution context");
    }
    
    bool compiled = cli_compile_script(script, execution);
    TEST_ASSERT(compiled, "Failed to compile table script");
    
    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute table script");
    
    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from table script");
    TEST_ASSERT_STR_EQUAL("42", result, "Table field access result mismatch");
    
    cli_free(result);
    cli_free_execution_context(execution);
    
    test_teardown();
    TEST_PASS("Table objects work correctly");
}

// Test script objects
bool test_script_objects(void) {
    test_setup();
    
    // Test script creation and execution
    const char* script = "local(myScript); script.newScriptObject(\"return(42)\", @myScript); return(myScript())";
    usertalk_execution_t* execution = cli_create_execution_context();
    
    if (!execution) {
        TEST_ASSERT(false, "Failed to create execution context");
    }
    
    bool compiled = cli_compile_script(script, execution);
    TEST_ASSERT(compiled, "Failed to compile script object script");
    
    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute script object script");
    
    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from script object script");
    TEST_ASSERT_STR_EQUAL("42", result, "Script execution result mismatch");
    
    cli_free(result);
    cli_free_execution_context(execution);
    
    test_teardown();
    TEST_PASS("Script objects work correctly");
}

// Test outline objects
bool test_outline_objects(void) {
    test_setup();
    
    // Test outline creation
    const char* script = "local(myOutline); new(outlineType, @myOutline); return(string(myOutline))";
    usertalk_execution_t* execution = cli_create_execution_context();
    
    if (!execution) {
        TEST_ASSERT(false, "Failed to create execution context");
    }
    
    bool compiled = cli_compile_script(script, execution);
    TEST_ASSERT(compiled, "Failed to compile outline script");
    
    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute outline script");
    
    // Outlines should return as strings in CLI context
    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from outline script");
    TEST_ASSERT(strlen(result) > 0, "Empty result from outline script");
    
    cli_free(result);
    cli_free_execution_context(execution);
    
    test_teardown();
    TEST_PASS("Outline objects work correctly");
}

// Test wptext objects
bool test_wptext_objects(void) {
    test_setup();
    
    // Test wptext creation and operations
    const char* script = "local(myText); new(wptextType, @myText); wp.newTextObject(\"Sample text\", @myText); return(string(myText))";
    usertalk_execution_t* execution = cli_create_execution_context();
    
    if (!execution) {
        TEST_ASSERT(false, "Failed to create execution context");
    }
    
    bool compiled = cli_compile_script(script, execution);
    TEST_ASSERT(compiled, "Failed to compile wptext script");
    
    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute wptext script");
    
    // Wptext should return as strings in CLI context
    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from wptext script");
    TEST_ASSERT_STR_EQUAL("Sample text", result, "Wptext content mismatch");
    
    cli_free(result);
    cli_free_execution_context(execution);
    
    test_teardown();
    TEST_PASS("Wptext objects work correctly");
}

// Test complex object interactions
bool test_complex_object_interactions(void) {
    test_setup();
    
    // Test table containing script
    const char* script = "local(myTable); new(tableType, @myTable); script.newScriptObject(\"return(\\\"success\\\")\", @myTable.handler); return(myTable.handler())";
    usertalk_execution_t* execution = cli_create_execution_context();
    
    if (!execution) {
        TEST_ASSERT(false, "Failed to create execution context");
    }
    
    bool compiled = cli_compile_script(script, execution);
    TEST_ASSERT(compiled, "Failed to compile complex object script");
    
    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute complex object script");
    
    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from complex object script");
    TEST_ASSERT_STR_EQUAL("success", result, "Complex object interaction result mismatch");
    
    cli_free(result);
    cli_free_execution_context(execution);
    
    test_teardown();
    TEST_PASS("Complex object interactions work correctly");
}
