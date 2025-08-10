# Frontier Test Suite

## Overview

This directory contains the organized test suite for the Frontier refactoring project. The test structure is designed to provide comprehensive testing for component migration while maintaining clear organization and long-term maintainability.

## Directory Structure

```
tests/
├── framework/          # Test framework core
│   ├── test_framework.h    # Test framework header
│   └── test_framework.c    # Test framework implementation
├── components/         # Component test suites
│   └── test_database.c     # Database component tests
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
