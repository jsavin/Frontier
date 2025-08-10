/*
 * test_runner.c - UserTalk Objects Test Runner
 * 
 * Main test runner that includes all UserTalk object type tests
 * and provides a unified interface for running the complete test suite.
 */

#include "../../framework/test_framework.h"

// Include test modules that do not rely on CLI/runtime yet
#include "test_basic_types.c"
#include "test_collections.c"
// Temporarily disable complex/DB-dependent tests until portable core grows
//#include "test_complex_objects.c"
//#include "test_database_persistence.c"
#include "test_control_flow.c"
#include "test_operators.c"
#include "test_error_handling.c"
#include "test_addressing_and_handlers.c"
#include "test_portable_handles.c"

// Test function declarations
bool test_string_objects(void);
bool test_number_objects(void);
bool test_boolean_objects(void);
bool test_increment_decrement(void);
bool test_string_concatenation(void);

bool test_list_objects(void);
bool test_record_objects(void);
bool test_heterogeneous_collections(void);
bool test_nested_collections(void);

// Temporarily exclude complex/db tests declarations

bool test_if_else_branching(void);
bool test_loops_with_break_continue(void);
bool test_arithmetic_and_comparison(void);
bool test_logical_and_string_ops(void);
bool test_try_else_error_capture(void);
bool test_status_return_handling(void);
bool test_address_and_dereference(void);
bool test_on_handler_named_params(void);

// Main test runner function
bool run_all_usertalk_object_tests(void) {
    printf("Running UserTalk Object Tests...\n");
    printf("================================\n");
    
    bool all_passed = true;
    
    // Basic types tests
    printf("\n--- Basic Types Tests ---\n");
    all_passed &= test_string_objects();
    all_passed &= test_number_objects();
    all_passed &= test_boolean_objects();
    all_passed &= test_increment_decrement();
    all_passed &= test_string_concatenation();
    
    // Control flow tests
    printf("\n--- Control Flow Tests ---\n");
    all_passed &= test_if_else_branching();
    all_passed &= test_loops_with_break_continue();

    // Operators tests
    printf("\n--- Operators Tests ---\n");
    all_passed &= test_arithmetic_and_comparison();
    all_passed &= test_logical_and_string_ops();

    // Error handling tests
    printf("\n--- Error Handling Tests ---\n");
    all_passed &= test_try_else_error_capture();
    all_passed &= test_status_return_handling();

    // Addressing and handlers
    printf("\n--- Addressing and Handlers Tests ---\n");
    all_passed &= test_address_and_dereference();
    all_passed &= test_on_handler_named_params();

    // Portable handle smoke tests
    printf("\n--- Portable Handle Tests ---\n");
    all_passed &= test_handle_alloc_lock_resize_dup();

    // Collection types tests
    printf("\n--- Collection Types Tests ---\n");
    all_passed &= test_list_objects();
    all_passed &= test_record_objects();
    all_passed &= test_heterogeneous_collections();
    all_passed &= test_nested_collections();
    
    // Complex objects tests (disabled for now)
    // printf("\n--- Complex Objects Tests ---\n");
    
    // Database persistence tests (disabled for now)
    // printf("\n--- Database Persistence Tests ---\n");
    
    printf("\n================================\n");
    if (all_passed) {
        printf("✅ All UserTalk Object Tests PASSED\n");
    } else {
        printf("❌ Some UserTalk Object Tests FAILED\n");
    }
    printf("================================\n");
    
    return all_passed;
}

// Individual test category runners
bool run_basic_type_tests(void) {
    printf("Running Basic Type Tests...\n");
    bool all_passed = true;
    all_passed &= test_string_objects();
    all_passed &= test_number_objects();
    all_passed &= test_boolean_objects();
    all_passed &= test_increment_decrement();
    all_passed &= test_string_concatenation();
    return all_passed;
}

bool run_collection_tests(void) {
    printf("Running Collection Tests...\n");
    bool all_passed = true;
    all_passed &= test_list_objects();
    all_passed &= test_record_objects();
    all_passed &= test_heterogeneous_collections();
    all_passed &= test_nested_collections();
    return all_passed;
}

bool run_complex_object_tests(void) {
    printf("Running Complex Object Tests... (disabled)\n");
    return true;
}

bool run_database_tests(void) {
    printf("Running Database Persistence Tests... (disabled)\n");
    return true;
}
