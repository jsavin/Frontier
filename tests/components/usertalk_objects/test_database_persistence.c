/*
 * test_database_persistence.c - Database Persistence Test Suite
 * 
 * Tests for UserTalk object persistence in Frontier databases
 * 
 * Database Operations:
 * - db.open(databaseName) - opens database for operations
 * - db.setValue(databaseName, key, value) - stores value in database
 * - db.getValue(databaseName, key) - retrieves value from database
 * - db.save(databaseName) - saves changes to database
 * - db.close(databaseName) - closes database connection
 */

#include "../framework/test_framework.h"
#include "../../frontier-cli/cli_executor.h"
#include "../../frontier-cli/cli_database.h"

// Helper function to create test database
bool create_test_database(void) {
    // Create a simple test database with basic objects
    const char* create_script = "db.open(\"test_usertalk_objects.root\"); db.setValue(\"test_usertalk_objects.root\", \"myString\", \"Persisted String\"); db.setValue(\"test_usertalk_objects.root\", \"myNumber\", 123); db.setValue(\"test_usertalk_objects.root\", \"myBoolean\", true); db.save(\"test_usertalk_objects.root\"); db.close(\"test_usertalk_objects.root\")";
    
    usertalk_execution_t* execution = cli_create_execution_context();
    if (!execution) {
        return false;
    }
    
    bool compiled = cli_compile_script(create_script, execution);
    if (!compiled) {
        cli_free_execution_context(execution);
        return false;
    }
    
    bool executed = cli_execute_compiled_script(execution);
    cli_free_execution_context(execution);
    
    return executed;
}

// Helper function to cleanup test database
void cleanup_test_database(void) {
    // Remove test database file
    remove("test_usertalk_objects.root");
}

// Test database object persistence
bool test_database_object_persistence(void) {
    test_setup();
    
    // Create test database
    TEST_ASSERT(create_test_database(), "Failed to create test database");
    
    // Test storing and retrieving objects from database
    const char* store_script = "db.open(\"test_usertalk_objects.root\"); db.setValue(\"test_usertalk_objects.root\", \"myString\", \"Persisted String\"); db.setValue(\"test_usertalk_objects.root\", \"myNumber\", 123); db.setValue(\"test_usertalk_objects.root\", \"myBoolean\", true); db.save(\"test_usertalk_objects.root\"); db.close(\"test_usertalk_objects.root\")";
    
    usertalk_execution_t* execution = cli_create_execution_context();
    TEST_ASSERT(execution != NULL, "Failed to create execution context");
    
    bool compiled = cli_compile_script(store_script, execution);
    TEST_ASSERT(compiled, "Failed to compile database storage script");
    
    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute database storage script");
    
    cli_free_execution_context(execution);
    
    // Test retrieving objects from database
    const char* retrieve_script = "db.open(\"test_usertalk_objects.root\"); local(stringValue, numberValue, booleanValue); stringValue = db.getValue(\"test_usertalk_objects.root\", \"myString\"); numberValue = db.getValue(\"test_usertalk_objects.root\", \"myNumber\"); booleanValue = db.getValue(\"test_usertalk_objects.root\", \"myBoolean\"); db.close(\"test_usertalk_objects.root\"); return(stringValue + \":\" + numberValue + \":\" + booleanValue)";
    
    execution = cli_create_execution_context();
    TEST_ASSERT(execution != NULL, "Failed to create execution context for retrieval");
    
    compiled = cli_compile_script(retrieve_script, execution);
    TEST_ASSERT(compiled, "Failed to compile database retrieval script");
    
    executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute database retrieval script");
    
    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from database retrieval script");
    TEST_ASSERT(strstr(result, "Persisted String") != NULL, "Database string retrieval failed");
    TEST_ASSERT(strstr(result, "123") != NULL, "Database number retrieval failed");
    TEST_ASSERT(strstr(result, "true") != NULL, "Database boolean retrieval failed");
    
    cli_free(result);
    cli_free_execution_context(execution);
    
    cleanup_test_database();
    test_teardown();
    TEST_PASS("Database object persistence works correctly");
}

// Test complex object persistence
bool test_complex_object_persistence(void) {
    test_setup();
    
    // Test storing complex objects in database
    const char* store_complex_script = "db.open(\"test_complex_objects.root\"); local(myTable, myScript); new(tableType, @myTable); myTable.testValue = 42; myScript = on testHandler() { return(\"persisted\") }; db.setValue(\"test_complex_objects.root\", \"myTable\", myTable); db.setValue(\"test_complex_objects.root\", \"myScript\", myScript); db.save(\"test_complex_objects.root\"); db.close(\"test_complex_objects.root\")";
    
    usertalk_execution_t* execution = cli_create_execution_context();
    TEST_ASSERT(execution != NULL, "Failed to create execution context");
    
    bool compiled = cli_compile_script(store_complex_script, execution);
    TEST_ASSERT(compiled, "Failed to compile complex object storage script");
    
    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute complex object storage script");
    
    cli_free_execution_context(execution);
    
    // Test retrieving complex objects from database
    const char* retrieve_complex_script = "db.open(\"test_complex_objects.root\"); local(retrievedTable, retrievedScript); retrievedTable = db.getValue(\"test_complex_objects.root\", \"myTable\"); retrievedScript = db.getValue(\"test_complex_objects.root\", \"myScript\"); db.close(\"test_complex_objects.root\"); return(retrievedTable.testValue + \":\" + retrievedScript())";
    
    execution = cli_create_execution_context();
    TEST_ASSERT(execution != NULL, "Failed to create execution context for complex retrieval");
    
    compiled = cli_compile_script(retrieve_complex_script, execution);
    TEST_ASSERT(compiled, "Failed to compile complex object retrieval script");
    
    executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute complex object retrieval script");
    
    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from complex object retrieval script");
    TEST_ASSERT(strstr(result, "42") != NULL, "Complex object table retrieval failed");
    TEST_ASSERT(strstr(result, "persisted") != NULL, "Complex object script retrieval failed");
    
    cli_free(result);
    cli_free_execution_context(execution);
    
    // Cleanup
    remove("test_complex_objects.root");
    test_teardown();
    TEST_PASS("Complex object persistence works correctly");
}

// Test database error handling
bool test_database_error_handling(void) {
    test_setup();
    
    // Test accessing non-existent database
    const char* error_script = "db.open(\"nonexistent.root\"); local(value); value = db.getValue(\"nonexistent.root\", \"nonexistent\"); return(value)";
    
    usertalk_execution_t* execution = cli_create_execution_context();
    TEST_ASSERT(execution != NULL, "Failed to create execution context");
    
    bool compiled = cli_compile_script(error_script, execution);
    TEST_ASSERT(compiled, "Failed to compile error handling script");
    
    bool executed = cli_execute_compiled_script(execution);
    // This should handle the error gracefully
    TEST_ASSERT(executed, "Failed to execute error handling script");
    
    cli_free_execution_context(execution);
    
    test_teardown();
    TEST_PASS("Database error handling works correctly");
}
