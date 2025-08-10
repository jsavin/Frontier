# UserTalk Objects Test Suite

This directory contains comprehensive tests for all UserTalk object types, organized by category for better maintainability and extensibility.

## Test Structure

### `test_basic_types.c`
Tests for fundamental UserTalk types:
- **Strings**: Creation, manipulation, concatenation
- **Numbers**: Arithmetic operations, increment/decrement
- **Booleans**: Logical operations, comparisons

### `test_collections.c`
Tests for UserTalk collection types:
- **Arrays**: Heterogeneous lists with zero-based indexing
- **Records**: Key-value associative storage
- **Heterogeneous Collections**: Mixed type storage
- **Nested Collections**: Complex nested structures

### `test_complex_objects.c`
Tests for complex UserTalk objects:
- **Tables**: Object creation, field access, storage
- **Scripts**: Function creation, execution, storage
- **Outlines**: Hierarchical structure creation
- **Wptext**: Rich text object handling
- **Complex Interactions**: Objects containing other objects

### `test_database_persistence.c`
Tests for database operations:
- **Object Persistence**: Storing/retrieving all object types
- **Complex Object Storage**: Tables, scripts in databases
- **Error Handling**: Database access error scenarios

### `test_runner.c`
Main test runner that:
- Includes all test modules
- Provides unified test execution
- Offers individual category runners
- Reports comprehensive results

## UserTalk Syntax Patterns

All tests follow proper UserTalk syntax conventions:

### Variable Declaration
```usertalk
local(var1, var2, var3)
```

### String Conversion
```usertalk
string(myText)      // Convert WPText to plain text (loses formatting)
string(myOutline)   // Convert outline to text (loses refcons)
string(myArray)     // Convert array to text representation
string(myRecord)    // Convert record to text representation
myScript()          // Execute script and return result
string(myScript)    // Get script source code
```

### Object Creation
```usertalk
new(tableType, @myTable)
new(outlineType, @myOutline)
new(wptextType, @myText)
script.newScriptObject("return(42)", @myScript)
wp.newTextObject("Sample text", @myText)
```

### Direct Table Assignment (Optimized)
```usertalk
// Instead of separate creation and assignment:
local(myScript); script.newScriptObject("return(42)", @myScript); myTable.handler = myScript

// Use direct creation in table:
script.newScriptObject("return(42)", @myTable.handler)
```

### String Literals
```usertalk
"Hello, World!"  // Double quotes for strings
'c'              // Single quotes for characters
```

### String Concatenation
```usertalk
"hello" + " " + "world"  // + operator
```

### Explicit Returns
```usertalk
return(expression)
```

### Database Operations
```usertalk
db.open("database.root")
db.setValue("database.root", "key", value)
db.getValue("database.root", "key")
db.save("database.root")
db.close("database.root")
```

## Building and Running

### Build All Tests
```bash
make all
```

### Run Complete Suite
```bash
make test-usertalk
```

### Run Individual Categories
```bash
make test-basic      # Basic types only
make test-collections # Collections only
make test-complex    # Complex objects only
make test-db-persistence # Database persistence only
```

### Run Specific Test Executable
```bash
./test_basic_types
./test_collections
./test_complex_objects
./test_database_persistence
./test_usertalk_runner
```

## Test Categories

### Basic Types (5 tests)
- String object creation and manipulation
- Number arithmetic and operations
- Boolean logic and comparisons
- Increment/decrement operators
- String concatenation

### Collections (4 tests)
- Array creation and operations
- Record creation and field access
- Heterogeneous collections
- Nested collection structures

### Complex Objects (5 tests)
- Table object creation and operations
- Script object creation and execution
- Outline object creation
- Wptext object handling
- Complex object interactions

### Database Persistence (3 tests)
- Basic object persistence
- Complex object persistence
- Database error handling

## Adding New Tests

To add new tests:

1. **Choose appropriate file** based on object type
2. **Follow naming convention**: `test_descriptive_name()`
3. **Use proper UserTalk syntax** as documented above
4. **Include comprehensive assertions** for validation
5. **Add to test runner** if creating new category

### Example Test Structure
```c
bool test_new_feature(void) {
    test_setup();
    
    const char* script = "local(x); x = 42; return(x)";
    usertalk_execution_t* execution = cli_create_execution_context();
    
    // ... test implementation ...
    
    test_teardown();
    TEST_PASS("New feature works correctly");
}
```

## Notes

- All tests use the CLI execution context for consistency
- Database tests create temporary `.root` files for testing
- Complex object tests validate object creation and interaction
- Error handling tests ensure graceful failure modes
- Tests follow UserTalk conventions for syntax and structure

### UserTalk Memory Management Quirks

#### Implicit Return Behavior
UserTalk automatically returns the last expression in a script block:

```usertalk
on myHandler()
    local(x, y)
    x = 5
    y = 10
    x + y  // Implicitly returned
```

#### Memory Optimization with Semicolons
For side-effect-only operations, use semicolons to prevent implicit return memory allocation:

```usertalk
// Inefficient (allocates memory for implicit return):
htmltext = htmltext + "<div>"
htmltext = htmltext + content

// Optimized (prevents implicit return allocation):
htmltext = htmltext + "<div>";
htmltext = htmltext + content;
```

This optimization was critical for production web templating systems but is not implemented by default due to potential side effects.
