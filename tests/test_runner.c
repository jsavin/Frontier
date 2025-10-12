/*
 * test_runner.c - Main test runner for Frontier testing framework
 * 
 * Phase 0.5.3: Testing Strategy Implementation - Organized Structure
 * 
 * This file provides a unified test runner that can execute all test suites
 * from the organized test hierarchy.
 */

#include "framework/test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include minimal type definitions for testing
typedef unsigned char Boolean;
typedef unsigned char UInt8;
typedef unsigned short UInt16;
typedef unsigned int UInt32;
typedef signed char SInt8;
typedef signed short SInt16;
typedef signed int SInt32;

typedef char* Ptr;
typedef Ptr* Handle;
typedef Handle RgnHandle;
typedef Handle ControlHandle;
typedef Handle MenuHandle;

typedef struct {
    unsigned char data[8];
} Pattern;

typedef struct {
    unsigned char data[80];
} FSRef;

typedef struct {
    unsigned short length;
    unsigned short data[255];
} HFSUniStr255;

// Test suite information
typedef struct {
    const char* name;
    const char* description;
    int test_count;
} test_suite_info_t;

// Available test suites (placeholder for now)
test_suite_info_t available_suites[] = {
    {"examples", "Example test suite demonstrating framework usage", 4},
    {"database", "Database component test suite", 5},
    {"dbnew", "Database dbnew() function test suite", 6},
    {"structures", "64-bit database structure test suite", 4},
    {"migration", "Database migration test suite", 6},
    {NULL, NULL, 0} // Terminator
};

// Function to list available test suites
void list_available_suites(void) {
    printf("Available Test Suites:\n");
    printf("=====================\n");
    
    for (int i = 0; available_suites[i].name != NULL; i++) {
        printf("%-12s - %s (%d tests)\n", 
               available_suites[i].name, 
               available_suites[i].description,
               available_suites[i].test_count);
    }
    printf("\n");
}

// Function to run a specific test suite
bool run_test_suite_by_name(const char* suite_name) {
    for (int i = 0; available_suites[i].name != NULL; i++) {
        if (strcmp(available_suites[i].name, suite_name) == 0) {
            printf("\n=== Running Test Suite: %s ===\n", available_suites[i].name);
            printf("Description: %s\n", available_suites[i].description);
            printf("Test Count: %d\n\n", available_suites[i].test_count);
            
            // For now, just report that the suite would run
            printf("Test suite '%s' would run %d tests\n", 
                   available_suites[i].name, available_suites[i].test_count);
            printf("(Integration with individual test files pending)\n");
            
            return true;
        }
    }
    
    printf("ERROR: Test suite '%s' not found\n", suite_name);
    return false;
}

// Function to run all test suites
bool run_all_test_suites(void) {
    printf("=== Running All Test Suites ===\n");
    
    bool all_passed = true;
    for (int i = 0; available_suites[i].name != NULL; i++) {
        printf("\n--- Test Suite: %s ---\n", available_suites[i].name);
        if (!run_test_suite_by_name(available_suites[i].name)) {
            all_passed = false;
        }
    }
    
    return all_passed;
}

// Main test runner
int main(int argc, char* argv[]) {
    printf("=== Frontier Test Runner ===\n");
    printf("Phase 0.5.3: Testing Strategy Implementation - Organized Structure\n\n");
    
    // Initialize test framework
    test_init();
    
    bool all_passed = true;
    
    if (argc == 1) {
        // No arguments - run all test suites
        printf("No test suite specified. Running all test suites.\n\n");
        all_passed = run_all_test_suites();
    } else if (argc == 2) {
        if (strcmp(argv[1], "--list") == 0 || strcmp(argv[1], "-l") == 0) {
            // List available test suites
            list_available_suites();
            return 0;
        } else if (strcmp(argv[1], "--all") == 0 || strcmp(argv[1], "-a") == 0) {
            // Run all test suites
            all_passed = run_all_test_suites();
        } else {
            // Run specific test suite
            all_passed = run_test_suite_by_name(argv[1]);
        }
    } else {
        printf("Usage: %s [suite_name|--list|--all]\n", argv[0]);
        printf("  suite_name  - Run specific test suite\n");
        printf("  --list, -l  - List available test suites\n");
        printf("  --all, -a   - Run all test suites\n");
        printf("  (no args)   - Run all test suites\n\n");
        list_available_suites();
        return 1;
    }
    
    // Print final summary
    test_summary();
    
    return all_passed ? 0 : 1;
}
