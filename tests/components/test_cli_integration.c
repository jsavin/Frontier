/*
 * test_cli_integration.c - CLI Integration Test Suite
 * 
 * Phase 1.1: Comprehensive Testing Implementation
 * 
 * This test suite validates the CLI functionality, including script execution,
 * database operations, and network server capabilities.
 */

#include "../framework/test_framework.h"
#include "../../frontier-cli/cli_executor.h"
#include "../../frontier-cli/cli_database.h"
#include "../../frontier-cli/cli_network.h"
#include "../../frontier-cli/cli_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Test database filename
static const char* test_db_filename = "test_cli_integration.root";

// Test helper functions
static bool create_test_database(void) {
    return cli_create_database(test_db_filename);
}

static bool cleanup_test_database(void) {
    if (unlink(test_db_filename) != 0) {
        printf("WARNING: Could not delete test database file\n");
    }
    return true;
}

// Test CLI script execution
bool test_cli_script_execution(void) {
    test_setup();
    
    // Test basic script execution
    const char* script = "local(x, y); x = 5; y = 3; return(x * y + 2)";
    usertalk_execution_t* execution = cli_create_execution_context();
    
    TEST_ASSERT(execution != NULL, "Failed to create execution context");
    
    bool compiled = cli_compile_script(script, execution);
    TEST_ASSERT(compiled, "Failed to compile script");
    
    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute script");
    
    long result = cli_get_execution_result_number(execution);
    TEST_ASSERT_EQUAL(17, result, "Script execution result mismatch");
    
    cli_free_execution_context(execution);
    
    test_teardown();
    TEST_PASS("CLI script execution works correctly");
}

// Test CLI database operations
bool test_cli_database_operations(void) {
    test_setup();
    
    // Create test database
    TEST_ASSERT(create_test_database(), "Failed to create test database");
    
    // Test database operations through CLI
    const char* db_script = "db.open(\"test_cli_integration.root\"); db.setValue(\"test_cli_integration.root\", \"testKey\", \"testValue\"); db.save(\"test_cli_integration.root\"); db.close(\"test_cli_integration.root\")";
    
    usertalk_execution_t* execution = cli_create_execution_context();
    TEST_ASSERT(execution != NULL, "Failed to create execution context");
    
    bool compiled = cli_compile_script(db_script, execution);
    TEST_ASSERT(compiled, "Failed to compile database script");
    
    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute database script");
    
    cli_free_execution_context(execution);
    
    // Test retrieving the value
    const char* retrieve_script = "db.open(\"test_cli_integration.root\"); local(value); value = db.getValue(\"test_cli_integration.root\", \"testKey\"); db.close(\"test_cli_integration.root\"); return(value)";
    
    execution = cli_create_execution_context();
    TEST_ASSERT(execution != NULL, "Failed to create execution context for retrieval");
    
    compiled = cli_compile_script(retrieve_script, execution);
    TEST_ASSERT(compiled, "Failed to compile retrieval script");
    
    executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute retrieval script");
    
    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from database retrieval");
    TEST_ASSERT_STR_EQUAL("testValue", result, "Database retrieval result mismatch");
    
    cli_free(result);
    cli_free_execution_context(execution);
    
    cleanup_test_database();
    test_teardown();
    TEST_PASS("CLI database operations work correctly");
}

// Test CLI error handling
bool test_cli_error_handling(void) {
    test_setup();
    
    // Test invalid script - missing value in assignment
    const char* invalid_script = "local(x, y); x = ; y = 5; return(x + y)";
    usertalk_execution_t* execution = cli_create_execution_context();
    
    TEST_ASSERT(execution != NULL, "Failed to create execution context");
    
    bool compiled = cli_compile_script(invalid_script, execution);
    TEST_ASSERT(!compiled, "Invalid script should not compile");
    
    const char* error = cli_get_execution_error(execution);
    TEST_ASSERT(error != NULL, "Should have error message for invalid script");
    TEST_ASSERT(strlen(error) > 0, "Error message should not be empty");
    
    cli_free_execution_context(execution);
    
    test_teardown();
    TEST_PASS("CLI error handling works correctly");
}

// Test CLI file operations
bool test_cli_file_operations(void) {
    test_setup();
    
    const char* test_filename = "test_cli_file.txt";
    const char* test_content = "Hello, CLI file operations!";
    
    // Test file writing
    bool written = cli_write_file(test_filename, test_content, strlen(test_content));
    TEST_ASSERT(written, "Failed to write test file");
    
    // Test file reading
    long file_size;
    char* read_content = cli_read_file(test_filename, &file_size);
    TEST_ASSERT(read_content != NULL, "Failed to read test file");
    TEST_ASSERT_EQUAL(strlen(test_content), file_size, "File size mismatch");
    TEST_ASSERT_STR_EQUAL(test_content, read_content, "File content mismatch");
    
    cli_free(read_content);
    
    // Cleanup
    unlink(test_filename);
    
    test_teardown();
    TEST_PASS("CLI file operations work correctly");
}

// Test CLI logging
bool test_cli_logging(void) {
    test_setup();
    
    // Initialize logging
    bool initialized = cli_init_logging(true, true);
    TEST_ASSERT(initialized, "Failed to initialize logging");
    
    // Test different log levels
    cli_log_info("Test info message");
    cli_log_warn("Test warning message");
    cli_log_error("Test error message");
    cli_log_debug("Test debug message");
    
    // Cleanup logging
    cli_cleanup_logging();
    
    test_teardown();
    TEST_PASS("CLI logging works correctly");
}

// Test CLI memory management
bool test_cli_memory_management(void) {
    test_setup();
    
    // Test memory allocation
    void* ptr1 = cli_malloc(100);
    TEST_ASSERT(ptr1 != NULL, "Failed to allocate memory");
    
    void* ptr2 = cli_calloc(10, 10);
    TEST_ASSERT(ptr2 != NULL, "Failed to allocate zeroed memory");
    
    void* ptr3 = cli_realloc(ptr1, 200);
    TEST_ASSERT(ptr3 != NULL, "Failed to reallocate memory");
    
    // Test string operations
    char* str1 = cli_strdup("test string");
    TEST_ASSERT(str1 != NULL, "Failed to duplicate string");
    TEST_ASSERT_STR_EQUAL("test string", str1, "String duplication failed");
    
    char* str2 = cli_concat_strings("Hello, ", "World!");
    TEST_ASSERT(str2 != NULL, "Failed to concatenate strings");
    TEST_ASSERT_STR_EQUAL("Hello, World!", str2, "String concatenation failed");
    
    // Cleanup
    cli_free(ptr3);
    cli_free(ptr2);
    cli_free(str1);
    cli_free(str2);
    
    test_teardown();
    TEST_PASS("CLI memory management works correctly");
}

// Test CLI network server framework
bool test_cli_network_framework(void) {
    test_setup();
    
    // Test server creation
    network_server_t* server = cli_create_network_server(8080);
    TEST_ASSERT(server != NULL, "Failed to create network server");
    TEST_ASSERT_EQUAL(8080, server->port, "Server port mismatch");
    TEST_ASSERT(!server->flrunning, "Server should not be running initially");
    
    // Test server start/stop
    bool started = cli_start_server(server);
    TEST_ASSERT(started, "Failed to start server");
    TEST_ASSERT(server->flrunning, "Server should be running after start");
    
    bool stopped = cli_stop_server(server);
    TEST_ASSERT(stopped, "Failed to stop server");
    TEST_ASSERT(!server->flrunning, "Server should not be running after stop");
    
    // Cleanup
    cli_free_network_server(server);
    
    test_teardown();
    TEST_PASS("CLI network server framework works correctly");
}

// Test CLI database migration
bool test_cli_database_migration(void) {
    test_setup();
    
    // Create test database
    TEST_ASSERT(create_test_database(), "Failed to create test database");
    
    // Test database migration
    bool migrated = cli_migrate_database(test_db_filename);
    TEST_ASSERT(migrated, "Failed to migrate database");
    
    // Verify migration created backup
    char* backup_path = cli_database_backup_path(test_db_filename);
    TEST_ASSERT(backup_path != NULL, "Failed to generate backup path");
    
    bool backup_exists = cli_file_exists(backup_path);
    TEST_ASSERT(backup_exists, "Database backup should exist after migration");
    
    cli_free(backup_path);
    cleanup_test_database();
    
    test_teardown();
    TEST_PASS("CLI database migration works correctly");
}

// Test CLI argument parsing
bool test_cli_argument_parsing(void) {
    test_setup();
    
    // Test valid arguments
    const char* valid_args[] = {"frontier-cli", "-e", "local(x); x = 5; return(x * 2)", NULL};
    cli_options_t options = {0};
    
    bool parsed = cli_parse_arguments(3, (char**)valid_args, &options);
    TEST_ASSERT(parsed, "Failed to parse valid arguments");
    TEST_ASSERT(options.inline_script != NULL, "Inline script should be set");
    TEST_ASSERT_STR_EQUAL("local(x); x = 5; return(x * 2)", options.inline_script, "Inline script content mismatch");
    
    cli_free_options(&options);
    
    // Test invalid arguments
    const char* invalid_args[] = {"frontier-cli", "--invalid-option", NULL};
    options = (cli_options_t){0};
    
    bool invalid_parsed = cli_parse_arguments(2, (char**)invalid_args, &options);
    TEST_ASSERT(!invalid_parsed, "Should fail to parse invalid arguments");
    
    test_teardown();
    TEST_PASS("CLI argument parsing works correctly");
}

// Test CLI execution context lifecycle
bool test_cli_execution_context_lifecycle(void) {
    test_setup();
    
    // Test context creation
    usertalk_execution_t* execution = cli_create_execution_context();
    TEST_ASSERT(execution != NULL, "Failed to create execution context");
    TEST_ASSERT(!execution->flcompiled, "Execution context should not be compiled initially");
    TEST_ASSERT(!execution->flsuccess, "Execution context should not be successful initially");
    
    // Test context compilation
    const char* script = "local(x); x = 42; return(x)";
    bool compiled = cli_compile_script(script, execution);
    TEST_ASSERT(compiled, "Failed to compile script");
    TEST_ASSERT(execution->flcompiled, "Execution context should be compiled");
    
    // Test context execution
    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute script");
    TEST_ASSERT(execution->flsuccess, "Execution context should be successful");
    
    // Test result retrieval
    long result = cli_get_execution_result_number(execution);
    TEST_ASSERT_EQUAL(42, result, "Execution result mismatch");
    
    // Test context cleanup
    cli_free_execution_context(execution);
    
    test_teardown();
    TEST_PASS("CLI execution context lifecycle works correctly");
}

// Test suite runner
test_case_t cli_integration_tests[] = {
    {"CLI Script Execution", test_cli_script_execution},
    {"CLI Database Operations", test_cli_database_operations},
    {"CLI Error Handling", test_cli_error_handling},
    {"CLI File Operations", test_cli_file_operations},
    {"CLI Logging", test_cli_logging},
    {"CLI Memory Management", test_cli_memory_management},
    {"CLI Network Server Framework", test_cli_network_framework},
    {"CLI Database Migration", test_cli_database_migration},
    {"CLI Argument Parsing", test_cli_argument_parsing},
    {"CLI Execution Context Lifecycle", test_cli_execution_context_lifecycle},
};

int main(void) {
    test_init();
    
    printf("=== CLI Integration Test Suite ===\n");
    printf("Testing CLI functionality and integration\n\n");
    
    bool success = run_test_suite(cli_integration_tests, 
                                 sizeof(cli_integration_tests) / sizeof(test_case_t));
    
    test_summary();
    
    return success ? 0 : 1;
}
