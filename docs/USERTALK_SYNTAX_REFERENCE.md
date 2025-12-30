# UserTalk Syntax Reference

**Complete guide to UserTalk syntax with focus on differences from modern languages**

## Table of Contents

1. [String Literals](#string-literals)
2. [Character Constants and OSTypes](#character-constants-and-ostypes)
3. [Variables and Assignment](#variables-and-assignment)
4. [Operators](#operators)
5. [Control Flow](#control-flow)
6. [Functions and Scripts](#functions-and-scripts)
7. [Tables (Hash Tables)](#tables-hash-tables)
8. [Common Pitfalls](#common-pitfalls)
9. [Differences from JavaScript/Python](#differences-from-javascriptpython)

---

## String Literals

### Basic Syntax

**UserTalk uses DOUBLE QUOTES for strings:**

```usertalk
"hello"                 // ✓ Correct
"multi word string"     // ✓ Correct
""                      // ✓ Empty string
```

**Single quotes are NOT for strings:**

```usertalk
'hello'                 // ✗ ERROR: Character constant syntax
'test'                  // ✗ ERROR: Character constant syntax
```

### String Operations

```usertalk
// Concatenation
"hello" + " " + "world"           // → "hello world"

// String functions
sizeOf("hello")                   // → 5
string.upper("test")              // → "TEST"
string.lower("TEST")              // → "test"

// Accessing characters (1-based indexing)
"hello"[1]                        // → 'h'
"hello"[5]                        // → 'o'
```

### Escaping

```usertalk
"\"quoted text\""                 // → "quoted text"
"line 1\nline 2"                  // → line 1 (newline) line 2
```

---

## Character Constants and OSTypes

### Character Constants (Single Character)

**Single quotes are for SINGLE characters only:**

```usertalk
'A'                     // ✓ Character constant (single char)
'1'                     // ✓ Character constant (single char)
' '                     // ✓ Space character
```

### OSTypes (Four-Character Codes)

**Four-character codes use single quotes:**

```usertalk
'TEXT'                  // ✓ OSType (Mac file type)
'PICT'                  // ✓ OSType (Mac file type)
'fold'                  // ✓ OSType (Mac file type)
```

### Invalid Usage

```usertalk
'hello'                 // ✗ ERROR: 5 characters (not 1 or 4)
'ab'                    // ✗ ERROR: 2 characters (not 1 or 4)
'abc'                   // ✗ ERROR: 3 characters (not 1 or 4)
'12345'                 // ✗ ERROR: 5 characters (not 1 or 4)
```

**Error message:**
```
Character constant isnt correctly specified. Must be of the form 'c'.
```

---

## Variables and Assignment

### Declaration and Assignment

```usertalk
// Simple assignment (no var/let/const keyword needed)
x = 5
name = "Alice"
flag = true

// Multiple assignments
x = 10
y = 20
z = x + y

// Return value
return x              // Returns value of x
```

### Variable Scope

```usertalk
// Local variables (default scope in scripts)
local x = 5

// Global variables (accessible across scripts)
global.myVar = "value"

// Table addresses (reference to table entry)
@system.verbs.myVerb
```

---

## Operators

### Arithmetic Operators

```usertalk
1 + 1                   // Addition → 2
10 - 3                  // Subtraction → 7
6 * 7                   // Multiplication → 42
100 / 4                 // Division → 25
17 mod 5                // Modulus → 2
```

### Comparison Operators

```usertalk
x == y                  // Equality
x != y                  // Inequality
x > y                   // Greater than
x < y                   // Less than
x >= y                  // Greater than or equal
x <= y                  // Less than or equal
```

### Logical Operators

```usertalk
true and false          // Logical AND → false
true or false           // Logical OR → true
not true                // Logical NOT → false
```

### String Concatenation

```usertalk
"hello" + " " + "world"           // → "hello world"
"count: " + 42                    // → "count: 42"
```

---

## Control Flow

### If Statements

```usertalk
if x > 10 then
    return "large"
else
    return "small"

// Inline if (ternary-like)
if x > 10 then "large" else "small"
```

### Loops

```usertalk
// For loop
for i = 1 to 10
    msg(i)

// While loop
while x < 100
    x = x + 1

// Loop through table
for i = 1 to sizeof(myTable)
    msg(myTable[i])
```

### Break and Continue

```usertalk
for i = 1 to 100
    if i == 50 then
        break         // Exit loop
    if i mod 2 == 0 then
        continue      // Skip to next iteration
```

---

## Functions and Scripts

### Defining Functions

```usertalk
// Script with parameters
on myFunction(x, y)
    return x + y

// Calling the function
myFunction(5, 10)     // → 15
```

### Built-in Functions

```usertalk
sizeOf(x)             // Size of string, table, etc.
defined(x)            // Check if variable is defined
typeof(x)             // Get type of value
msg(x)                // Display message
```

---

## Tables (Hash Tables)

### Creating Tables

```usertalk
// Create new table
lang.new(tableType, @myTable)

// Assign values
myTable.key1 = "hello"
myTable.key2 = 42
myTable.key3 = true
```

### Accessing Table Elements

```usertalk
// Dot notation
myTable.key1          // → "hello"

// Bracket notation (string key)
myTable["key1"]       // → "hello"

// Bracket notation (numeric index - 1-based)
myTable[1]            // First element
```

### Table Operations

```usertalk
sizeOf(myTable)                   // Number of entries
defined(myTable.key1)             // Check if key exists
table.delete(@myTable.key1)       // Delete entry
```

---

## Common Pitfalls

### 1. Single vs Double Quotes

**WRONG:**
```usertalk
sizeOf('hello')        // ✗ ERROR: Character constant
x = 'test'             // ✗ ERROR: Character constant
```

**CORRECT:**
```usertalk
sizeOf("hello")        // ✓ String
x = "test"             // ✓ String
```

### 2. Array Indexing (1-based, not 0-based)

**WRONG:**
```usertalk
"hello"[0]             // ✗ ERROR: Index out of range
myTable[0]             // ✗ ERROR: Index out of range
```

**CORRECT:**
```usertalk
"hello"[1]             // ✓ First character → 'h'
myTable[1]             // ✓ First element
```

### 3. Variable Declaration

**WRONG:**
```usertalk
var x = 5              // ✗ ERROR: 'var' keyword not supported
let x = 5              // ✗ ERROR: 'let' keyword not supported
const x = 5            // ✗ ERROR: 'const' keyword not supported
```

**CORRECT:**
```usertalk
x = 5                  // ✓ Simple assignment
local x = 5            // ✓ Explicit local scope
```

### 4. Boolean Values

```usertalk
true                   // ✓ Boolean true
false                  // ✓ Boolean false

// NOT these (from other languages):
True                   // ✗ ERROR (Python-style)
TRUE                   // ✗ ERROR (C-style)
```

---

## Differences from JavaScript/Python

### String Literals

| Feature | UserTalk | JavaScript | Python |
|---------|----------|------------|--------|
| String syntax | `"hello"` only | `"hello"` or `'hello'` | `"hello"` or `'hello'` |
| Single quotes | Character/OSType | String | String |
| Multi-char single quotes | ERROR | String | String |

### Variable Declaration

| Feature | UserTalk | JavaScript | Python |
|---------|----------|------------|--------|
| Declaration | `x = 5` | `var x = 5` or `let x = 5` | `x = 5` |
| Scope keywords | `local` / `global` | `var` / `let` / `const` | `global` / `nonlocal` |

### Array/Table Indexing

| Feature | UserTalk | JavaScript | Python |
|---------|----------|------------|--------|
| First element | `arr[1]` | `arr[0]` | `arr[0]` |
| Index base | 1-based | 0-based | 0-based |

### Boolean Operators

| Feature | UserTalk | JavaScript | Python |
|---------|----------|------------|--------|
| AND | `and` | `&&` | `and` |
| OR | `or` | `\|\|` | `or` |
| NOT | `not` | `!` | `not` |

### Type Checking

| Feature | UserTalk | JavaScript | Python |
|---------|----------|------------|--------|
| Get type | `typeof(x)` | `typeof x` | `type(x)` |
| Check existence | `defined(x)` | `typeof x !== 'undefined'` | `'x' in dir()` |

---

## Testing UserTalk Code

### Correct Examples

```bash
# String operations (DOUBLE QUOTES)
./frontier-cli -e "sizeOf(\"hello\")"                    # → 5
./frontier-cli -e "string.upper(\"test\")"               # → TEST

# Table operations
./frontier-cli -e "lang.new(tableType, @t); t.key1 = \"hello\"; return t.key1"

# Multi-line scripts
./frontier-cli -e $'x = 5\ny = 10\nreturn x + y'         # → 15
```

### Common Errors to Avoid

```bash
# WRONG: Single quotes for strings
./frontier-cli -e "sizeOf('hello')"                      # ERROR!

# WRONG: 0-based indexing
./frontier-cli -e "\"hello\"[0]"                         # ERROR!

# WRONG: JavaScript-style variable declaration
./frontier-cli -e "var x = 5"                            # ERROR!
```

---

## Quick Reference Card

### Syntax Cheat Sheet

```usertalk
// Strings (DOUBLE QUOTES ONLY)
"hello world"

// Character constant (single char)
'A'

// OSType (4 chars)
'TEXT'

// Variables
x = 5
local y = 10

// Tables
lang.new(tableType, @t)
t.key = "value"

// Control flow
if condition then
    // code
else
    // code

for i = 1 to 10
    // code

// Functions
on myFunc(param)
    return param * 2
```

---

## Additional Resources

- **Project Documentation**: `/Users/jake/dev/jsavin/Frontier/docs/`
- **Planning Docs**: `/Users/jake/dev/jsavin/Frontier/planning/`
- **Legacy Reference**: `/Users/jake/dev/tedchoward/Frontier/`
- **CLAUDE.md**: Project-specific patterns and conventions

---

## See Also

- `CLAUDE.md` - UserTalk syntax quirks section
- `planning/` - Design documentation
- `tests/usertalk_basic_operations.sh` - Regression tests with examples
