# Frontier Test Suite

Status
- State: In Progress
- Phase: 1–2
- Last Updated: 2025-09-29
- Notes: Headless-first testing; see Quickstart and behavior matrix.

Related Docs
- planning/DEVELOPER_QUICKSTART_HEADLESS.md
- planning/headless_stubbed_behavior_matrix.md
- planning/no_ui_linkage_policy.md
- planning/0.5.23_runtime_test_plan.md
- Parser regeneration (maintainers): planning/phase5/bison3_migration_plan.md
  - You do not need Bison to run tests; the generated parser C is committed.
  - To regenerate locally (optional): see `scripts/gen_langparser.sh` usage in the Quickstart’s “Parser Regeneration” section.

Change Log
- 2025-09-29: Initialized template sections (Status/Related Docs/Change Log).

## Table of Contents
- [Overview](#overview)
- [Directory Structure](#directory-structure)
- [Test Hierarchy](#test-hierarchy)
- [Usage](#usage)
  - [Building and Running Tests](#building-and-running-tests)
  - [Test Runner Usage](#test-runner-usage)
- [Test Framework Features](#test-framework-features)
  - [Assertions](#assertions)
  - [Test Suite Definition](#test-suite-definition)
  - [Test Runner Integration](#test-runner-integration)
- [Adding New Test Suites](#adding-new-test-suites)
- [Test Categories](#test-categories)
- [Benefits for Project Priorities](#benefits-for-project-priorities)
- [Current Test Suites](#current-test-suites)
- [Future Test Suites](#future-test-suites)
- [Architecture Support](#architecture-support)
- [Contributing](#contributing)
  - [Headless Helpers (`headless_shell.c`)](#headless-helpers-headless_shellc)
  - [Runtime (`db_format_tests.c`)](#runtime-db_format_testsc)

## Overview

This directory contains the organized test suite for the Frontier refactoring project. The test structure is designed to provide comprehensive testing for component migration while maintaining clear organization and long-term maintainability.

## Directory Structure

```
tests/
├── framework/          # Test framework core
│   ├── test_framework.h    # Test framework header
│   └── test_framework.c    # Test framework implementation
├── headless_shell.c    # UI stubs used by headless harnesses
├── components/         # Component test suites
│   └── test_database.c     # Database component tests
├── db_format_tests.c   # Header conversion/detection unit tests
├── examples/           # Example test suites
│   └── test_example.c      # Framework usage examples
├── test_runner.c       # Main test runner
├── Makefile           # Test build system
└── README.md          # This file
```

## Test Hierarchy

### Framework (`framework/`)
- **Purpose**: Core testing infrastructure
- **Files**: `test_framework.h`, `test_framework.c`
- **Features**: Test assertions, test runner, statistics tracking
- **Usage**: Included by all test suites

### Components (`components/`)
- **Purpose**: Component-specific test suites
- **Current**: `test_database.c` - Database component tests
- **Future**: Additional component tests (engine, UI, etc.)
- **Pattern**: One test file per major component

### Examples (`examples/`)
- **Purpose**: Framework usage examples and basic tests
- **Current**: `test_example.c` - Framework demonstration
- **Usage**: Reference for writing new test suites

### Test Runner (`test_runner.c`)
- **Purpose**: Unified test execution
- **Features**: Run all tests, specific suites, or list available suites
- **Usage**: Main entry point for test execution

## Usage

### Building and Running Tests

```bash
# Build test runner
make

# Run all test suites
make test

# Run specific test suite
make test_examples
make test_database

# List available test suites
make list

# Test both architectures
make test_architectures

# Clean build artifacts
make clean

# Show documentation
make docs
```

### Test Runner Usage

```bash
# Run all test suites
./test_runner

# List available test suites
./test_runner --list

# Run specific test suite
./test_runner examples
./test_runner database

# Run all test suites (explicit)
./test_runner --all
```

## Test Framework Features

### Assertions
```c
TEST_ASSERT(condition, message);
TEST_ASSERT_EQUAL(expected, actual, message);
TEST_ASSERT_STR_EQUAL(expected, actual, message);
TEST_PASS(message);
```

### Test Suite Definition
```c
test_case_t my_tests[] = {
    {"Test Name", test_function},
    // ...
};
```

### Test Runner Integration
```c
// In test file
int main(void) {
    test_init();
    bool all_passed = run_test_suite(my_tests, count);
    test_summary();
    return all_passed ? 0 : 1;
}
```

## Adding New Test Suites

### 1. Create Test File
Create a new `.c` file in the appropriate directory:
- `components/` for component tests
- `examples/` for example/demonstration tests

### 2. Include Framework
```c
#include "../framework/test_framework.h"
```

### 3. Define Test Functions
```c
bool test_my_function(void) {
    TEST_ASSERT(condition, "Test message");
    TEST_PASS("Test passed");
}
```

### 4. Define Test Suite
```c
test_case_t my_tests[] = {
    {"My Test", test_my_function},
    // ...
};
```

### 5. Update Test Runner
Add your test suite to `test_runner.c`:
```c
extern test_case_t my_tests[];
extern int my_test_count;

test_suite_info_t available_suites[] = {
    {"mycomponent", "My component test suite", my_tests, my_test_count},
    // ...
};
```

## Test Categories

### Unit Tests
- Individual function behavior
- Edge cases and error conditions
- Memory management validation

### Integration Tests
- Component interaction testing
- Database ↔ Engine communication
- UI ↔ Engine integration

### Regression Tests
- Legacy behavior preservation
- Cross-architecture consistency
- Performance benchmarks

## Benefits for Project Priorities

### Code Cleanup
- **Safety Net**: Tests provide confidence during refactoring
- **Regression Detection**: Immediate feedback on component changes
- **Documentation**: Tests serve as living documentation

### Forward Compatibility
- **Cross-Architecture Testing**: ARM64 and x86_64 support
- **Modern Build Integration**: Works with clean build approach
- **Future-Proof**: Framework can evolve with component migration

### Long-term Maintainability
- **Clear Organization**: Logical test hierarchy
- **Comprehensive Coverage**: Framework supports all test types
- **Documented Approach**: Clear patterns for team consistency

## Current Test Suites

### Examples Suite
- **Tests**: 4
- **Purpose**: Framework demonstration and basic validation
- **Categories**: Basic assertions, type definitions, framework functionality, legacy compatibility

### Database Suite
- **Tests**: 5
- **Purpose**: Database component validation
- **Categories**: File creation, header structure, file operations, address operations, compatibility

## Future Test Suites

### Engine Component
- UserTalk verb testing
- Memory management validation
- Thread safety testing
- Error handling verification

### UI Component
- Window management tests
- Event handling validation
- Cross-platform compatibility
- Accessibility testing

## Architecture Support

All test suites support both ARM64 and x86_64 architectures:
- Universal binary compilation
- Architecture-specific testing
- Cross-architecture validation
- Performance comparison

## Contributing

When adding new tests:
1. Follow the established patterns
2. Include comprehensive assertions
3. Test both success and failure cases
4. Document test purpose and scope
5. Ensure cross-architecture compatibility
### Headless Helpers (`headless_shell.c`)
- **Purpose**: Provide minimal `shellvisittypedwindows`/`shellerrormessage`
  implementations so database/runtime tests can run without linking the UI
  layer.
- **Usage**: Linked into headless harnesses (e.g. `core_tests`).

### Runtime (`db_format_tests.c`)
- **Purpose**: Validate database header detection and 32→64-bit conversion
  helpers without invoking the UI stack.
- **Usage**: Build with `make db_format_tests` and run `./db_format_tests`.
