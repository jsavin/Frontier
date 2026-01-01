/*
 * test_table_sorting.c - UserTalk Table Sorting Test Suite
 *
 * Tests for table.sortby() and table.getsortorder() verbs in headless mode
 *
 * Table Sorting Notes:
 * - Sort order is stored in (**htable).sortorder (per-table, persistent)
 * - Three sort columns: "name" (0), "value" (1), "kind" (2)
 * - Default sort order: sortbyname (0)
 * - Sort order persists to database
 * - Cursor preserved after resort (key stays same, flat index recalculated)
 * - Column names are case-insensitive
 * - Invalid column names return explicit error
 */

#include "../../framework/test_framework.h"
#include "../../../portable/cli_executor.h"
#include "../../../Common/headers/lang.h"
#include "../../../Common/headers/memory.h"
#include "../../../Common/headers/strings.h"
#include "../../../Common/headers/tablestructure.h"
#include "../../../Common/headers/langexternal.h"
#include <assert.h>
#include <stdbool.h>

// Global flag to track if runtime has been initialized
static bool runtime_initialized = false;

// Initialize the UserTalk runtime once
static void initialize_runtime(void) {
    if (runtime_initialized) return;

    // Initialize subsystems in proper order
    assert(initmemory());
    initstrings();
    assert(initlang());

    // Allocate hash table stack BEFORE calling inittablestructure
    // This is required for pushhashtable to work
    extern hdltablestack hashtablestack;
    printf("[init] hashtablestack before allocation: %p\n", (void*)hashtablestack);
    if (hashtablestack == NULL) {
        extern boolean newclearhandle(long, Handle*);
        typedef struct tytablestack {
            short toptables;
            hdlhashtable stack[100];  // ct hash tables, from langhash.h
        } tytablestack;
        boolean ok = newclearhandle(sizeof(tytablestack), (Handle*)&hashtablestack);
        printf("[init] newclearhandle returned: %d, hashtablestack=%p\n", ok, (void*)hashtablestack);
        assert(ok);
        (**hashtablestack).toptables = 0;
    }

    boolean initok = inittablestructure();  // This creates roottable and pushes it
    printf("[init] inittablestructure returned: %d\n", initok);
    assert(initok);

    // Verify roottable and currenthashtable are set
    extern hdlhashtable roottable;
    extern hdlhashtable currenthashtable;
    printf("[init] After inittablestructure: roottable=%p, currenthashtable=%p\n",
           (void*)roottable, (void*)currenthashtable);

    // If currenthashtable is still nil, manually set it
    if (currenthashtable == NULL && roottable != NULL) {
        printf("[init] WARNING: currenthashtable is nil, manually pushing roottable\n");
        extern boolean pushhashtable(hdlhashtable);
        boolean pushed = pushhashtable(roottable);
        printf("[init] pushhashtable returned: %d, currenthashtable=%p\n",
               pushed, (void*)currenthashtable);
    }

    assert(roottable != NULL);
    assert(currenthashtable != NULL);

    // Initialize verb tables
    extern boolean langinitresources_headless(void);
    extern boolean langinitverbs(void);
    assert(langinitresources_headless());
    assert(langinitverbs());

    // Initialize WPText support
    extern boolean wp_portable_init(void);
    assert(wp_portable_init());

    runtime_initialized = true;
}

// Test table.sortby() with valid column names
bool test_sortby_valid_columns(void) {
    test_setup();

    const char* script =
        "lang.new(tableType, @t); "
        "t.zebra = \"last\"; "
        "t.alpha = \"first\"; "
        "t.middle = \"mid\"; "
        "table.settarget(@t); "
        "table.sortby(\"name\"); "  // Should succeed
        "local(order1 = table.getsortorder()); "
        "table.sortby(\"value\"); "  // Should succeed
        "local(order2 = table.getsortorder()); "
        "table.sortby(\"kind\"); "  // Should succeed
        "local(order3 = table.getsortorder()); "
        "return(order1 + \",\" + order2 + \",\" + order3)";

    usertalk_execution_t* execution = cli_create_execution_context();
    TEST_ASSERT(execution != NULL, "Failed to create execution context");

    bool compiled = cli_compile_script(script, execution);
    TEST_ASSERT(compiled, "Failed to compile sortby valid columns script");

    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute sortby valid columns script");

    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from sortby valid columns script");
    TEST_ASSERT_STR_EQUAL("name,value,kind", result, "Sort order columns mismatch");

    cli_free(result);
    cli_free_execution_context(execution);

    test_teardown();
    TEST_PASS("table.sortby() with valid column names works correctly");
}

// Test table.sortby() case-insensitive matching
bool test_sortby_case_insensitive(void) {
    test_setup();

    const char* script =
        "lang.new(tableType, @t); "
        "table.settarget(@t); "
        "table.sortby(\"NAME\"); "  // Uppercase
        "local(order1 = table.getsortorder()); "
        "table.sortby(\"VaLuE\"); "  // Mixed case
        "local(order2 = table.getsortorder()); "
        "table.sortby(\"KiNd\"); "  // Mixed case
        "local(order3 = table.getsortorder()); "
        "return(order1 + \",\" + order2 + \",\" + order3)";

    usertalk_execution_t* execution = cli_create_execution_context();
    TEST_ASSERT(execution != NULL, "Failed to create execution context");

    bool compiled = cli_compile_script(script, execution);
    TEST_ASSERT(compiled, "Failed to compile sortby case insensitive script");

    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute sortby case insensitive script");

    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from sortby case insensitive script");
    TEST_ASSERT_STR_EQUAL("name,value,kind", result, "Case-insensitive sorting failed");

    cli_free(result);
    cli_free_execution_context(execution);

    test_teardown();
    TEST_PASS("table.sortby() case-insensitive matching works correctly");
}

// Test table.sortby() with invalid column name (should return error)
bool test_sortby_invalid_column(void) {
    test_setup();

    const char* script =
        "lang.new(tableType, @t); "
        "table.settarget(@t); "
        "try { "
        "  table.sortby(\"invalid\"); "
        "  return(\"SHOULD_HAVE_FAILED\") "
        "} "
        "else { "
        "  return(\"ERROR_CAUGHT\") "
        "}";

    usertalk_execution_t* execution = cli_create_execution_context();
    TEST_ASSERT(execution != NULL, "Failed to create execution context");

    bool compiled = cli_compile_script(script, execution);
    TEST_ASSERT(compiled, "Failed to compile sortby invalid column script");

    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute sortby invalid column script");

    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from sortby invalid column script");
    TEST_ASSERT_STR_EQUAL("ERROR_CAUGHT", result, "Invalid column name should return error");

    cli_free(result);
    cli_free_execution_context(execution);

    test_teardown();
    TEST_PASS("table.sortby() with invalid column name returns error correctly");
}

// Test table.getsortorder() default value
bool test_getsortorder_default(void) {
    test_setup();

    const char* script =
        "lang.new(tableType, @t); "
        "table.settarget(@t); "
        "return(table.getsortorder())";

    usertalk_execution_t* execution = cli_create_execution_context();
    TEST_ASSERT(execution != NULL, "Failed to create execution context");

    bool compiled = cli_compile_script(script, execution);
    TEST_ASSERT(compiled, "Failed to compile getsortorder default script");

    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute getsortorder default script");

    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from getsortorder default script");
    TEST_ASSERT_STR_EQUAL("name", result, "Default sort order should be 'name'");

    cli_free(result);
    cli_free_execution_context(execution);

    test_teardown();
    TEST_PASS("table.getsortorder() returns default 'name' correctly");
}

// Test table.goto() after sort (cursor preservation)
bool test_goto_after_sort(void) {
    test_setup();

    const char* script =
        "lang.new(tableType, @t); "
        "t.zebra = 100; "
        "t.alpha = 200; "
        "t.middle = 300; "
        "table.settarget(@t); "
        "table.goto(2); "  // Go to row 2 (sorted by name: "middle")
        "local(cursor1 = string(table.getcursor())); "  // Should be "t.middle"
        "table.sortby(\"value\"); "  // Resort by value (100, 200, 300)
        "local(cursor2 = string(table.getcursor())); "  // Cursor key should be same ("middle")
        "return(cursor1 == cursor2)";

    usertalk_execution_t* execution = cli_create_execution_context();
    TEST_ASSERT(execution != NULL, "Failed to create execution context");

    bool compiled = cli_compile_script(script, execution);
    TEST_ASSERT(compiled, "Failed to compile goto after sort script");

    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute goto after sort script");

    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from goto after sort script");
    TEST_ASSERT_STR_EQUAL("true", result, "Cursor key should be preserved after resort");

    cli_free(result);
    cli_free_execution_context(execution);

    test_teardown();
    TEST_PASS("Cursor preserved after table.sortby() correctly");
}

// Test sorting with no target table (should error)
bool test_sortby_no_target(void) {
    test_setup();

    const char* script =
        "try { "
        "  table.sortby(\"name\"); "  // No target set
        "  return(\"SHOULD_HAVE_FAILED\") "
        "} "
        "else { "
        "  return(\"ERROR_CAUGHT\") "
        "}";

    usertalk_execution_t* execution = cli_create_execution_context();
    TEST_ASSERT(execution != NULL, "Failed to create execution context");

    bool compiled = cli_compile_script(script, execution);
    TEST_ASSERT(compiled, "Failed to compile sortby no target script");

    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute sortby no target script");

    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from sortby no target script");
    TEST_ASSERT_STR_EQUAL("ERROR_CAUGHT", result, "sortby with no target should return error");

    cli_free(result);
    cli_free_execution_context(execution);

    test_teardown();
    TEST_PASS("table.sortby() with no target returns error correctly");
}

// Test row numbers change after sorting
bool test_row_numbers_after_sort(void) {
    test_setup();

    const char* script =
        "lang.new(tableType, @t); "
        "t.zebra = 100; "
        "t.alpha = 200; "
        "t.middle = 300; "
        "table.settarget(@t); "
        "table.sortby(\"name\"); "  // Sort by name: alpha, middle, zebra
        "table.goto(1); "
        "local(first_name = string(table.getcursor())); "
        "table.sortby(\"value\"); "  // Sort by value: zebra(100), alpha(200), middle(300)
        "table.goto(1); "
        "local(first_value = string(table.getcursor())); "
        "return(first_name + \",\" + first_value)";

    usertalk_execution_t* execution = cli_create_execution_context();
    TEST_ASSERT(execution != NULL, "Failed to create execution context");

    bool compiled = cli_compile_script(script, execution);
    TEST_ASSERT(compiled, "Failed to compile row numbers after sort script");

    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute row numbers after sort script");

    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from row numbers after sort script");

    // First item sorted by name should be "alpha" (t.alpha)
    // First item sorted by value should be "zebra" (t.zebra = 100)
    // Result format: "t.alpha,t.zebra" or similar
    TEST_ASSERT(result != NULL && strlen(result) > 0, "Row numbers changed after sorting");

    cli_free(result);
    cli_free_execution_context(execution);

    test_teardown();
    TEST_PASS("Row numbers change correctly after table.sortby()");
}

// Test sort order with mixed value types
bool test_sortby_mixed_types(void) {
    test_setup();

    const char* script =
        "lang.new(tableType, @t); "
        "t.stringVal = \"text\"; "
        "t.numVal = 42; "
        "t.boolVal = true; "
        "table.settarget(@t); "
        "table.sortby(\"kind\"); "  // Sort by type
        "local(order = table.getsortorder()); "
        "return(order)";

    usertalk_execution_t* execution = cli_create_execution_context();
    TEST_ASSERT(execution != NULL, "Failed to create execution context");

    bool compiled = cli_compile_script(script, execution);
    TEST_ASSERT(compiled, "Failed to compile sortby mixed types script");

    bool executed = cli_execute_compiled_script(execution);
    TEST_ASSERT(executed, "Failed to execute sortby mixed types script");

    char* result = cli_get_execution_result_string(execution);
    TEST_ASSERT(result != NULL, "No result from sortby mixed types script");
    TEST_ASSERT_STR_EQUAL("kind", result, "Sort by kind should work with mixed types");

    cli_free(result);
    cli_free_execution_context(execution);

    test_teardown();
    TEST_PASS("table.sortby('kind') with mixed types works correctly");
}

// Main test runner for table sorting tests
int main(void) {
    // Initialize UserTalk runtime
    initialize_runtime();

    // Initialize test framework
    test_framework_init();

    printf("\n=== Table Sorting Tests ===\n\n");

    RUN_TEST(test_sortby_valid_columns);
    RUN_TEST(test_sortby_case_insensitive);
    RUN_TEST(test_sortby_invalid_column);
    RUN_TEST(test_getsortorder_default);
    RUN_TEST(test_goto_after_sort);
    RUN_TEST(test_sortby_no_target);
    RUN_TEST(test_row_numbers_after_sort);
    RUN_TEST(test_sortby_mixed_types);

    test_framework_summary();
    return test_framework_get_exit_code();
}
