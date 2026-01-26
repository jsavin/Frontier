# Name Resolution System Specification

**Document Type**: Technical Specification for Implementation
**Audience**: system-architect agent
**Purpose**: Complete specification for fixing `string(123)` bug and understanding UserTalk name resolution
**Last Updated**: 2026-01-25

---

## Executive Summary

This document provides a complete specification of Frontier's UserTalk name resolution system, including the bug causing `string(123)` to fail after PR #336.

**The Bug**: `langgettableval()` uses `tablevaltotable()` which rejects non-table types (scripts, scalars). When resolving `string`, it finds the SCRIPT at `system.verbs.globals.string` but rejects it, then continues searching and finds the TABLE at `system.verbs.builtins.string`, causing a "Can't call string because it isn't a script" error.

**The Fix**: Modify `langgettableval()` to accept ANY type for final components while maintaining table-only requirement for intermediate components.

---

## Table of Contents

1. [Foundational Principles](#foundational-principles)
2. [Name Resolution Decision Tree](#name-resolution-decision-tree)
3. [Usage Dispatch After Resolution](#usage-dispatch-after-resolution)
4. [Special Cases & Edge Conditions](#special-cases--edge-conditions)
5. [Root Cause Analysis](#root-cause-analysis)
6. [Implementation Strategy](#implementation-strategy)
7. [Validation Test Cases](#validation-test-cases)

---

## Foundational Principles

### 1. Copy Semantics
- **ALL assignment is by copy**, not by reference
- `x = system.verbs.globals` → Copies entire table into `x`
- `y = x` → Creates independent copy of `x`
- Modifying `x` does NOT affect `y` or the database

### 2. Database Modification
- **Direct path assignment** modifies the database: `system.verbs.globals.newItem = 5`
- All tables in the path are loaded into memory
- Modified tables are marked dirty
- Changes written to `.root` file on database save

### 3. Script Context
- Scripts have **no implicit access** to their parent table members
- Must use `with parentOf(this)^ {...}` or explicit paths
- In practice, this pattern is rarely used

### 4. Value Types
- **typeof() returns the type of the stored value**, not the dereferenced object
- `x = @system.verbs.globals.string` → `typeof(x)` returns `'addr'` (not `'scpt'`)
- Address values store the path, not the object

---

## Name Resolution Decision Tree

### Overview

```
INPUT: Identifier to resolve (name, full path, or partial path)
CONTEXT: Current script execution, local stack, database state

┌─────────────────────────────────────────────────────────────┐
│ STEP 1: Find the Object                                    │
└─────────────────────────────────────────────────────────────┘

Parse identifier type → Apply appropriate resolution algorithm
├─ Simple name → Simple Name Resolution
├─ Full dot-path → Full Path Resolution (via Simple Name + traversal)
└─ Partial dot-path → Partial Path Resolution (via system.paths search)

Result: Object located (or error if not found)

┌─────────────────────────────────────────────────────────────┐
│ STEP 2: Use Found Object                                   │
└─────────────────────────────────────────────────────────────┘

Determine usage from syntax → Dispatch to appropriate handler
├─ Function call → Execute if callable, error otherwise
├─ Assignment → Store copy
├─ Value reference → Return copy
├─ Address reference → Create address value
├─ Property access → Continue resolution
└─ Type check → Return type code
```

### Simple Name Resolution

**Input**: Single identifier with no dots (e.g., `string`, `i`, `myVar`)

**Search Order** (First Match Wins):

```
1. LOCAL STACK
   ├─ Search current script's local variables
   ├─ Includes explicitly declared: local(x = 5)
   ├─ Includes implicitly declared: for i = 1 to 100
   ├─ Includes inline functions: on myFunction() {...}
   └─ If FOUND → GO TO: Use Found Object

2. WITH CONTEXT (if active)
   ├─ Search parent table from with statement
   ├─ Example: with system.verbs.globals { string(123) }
   ├─ Behaves like local stack (frame-based)
   └─ If FOUND → GO TO: Use Found Object

3. TOP-LEVEL SYSTEM ROOT TABLES
   ├─ Search top-level tables in system root database
   ├─ Example: "system", "Frontier", "examples", etc.
   └─ If FOUND → GO TO: Use Found Object
   └─ If NOT FOUND → Continue to step 4

4. system.paths ENTRIES (alphabetical order)
   ├─ Sort system.paths by entry name (path01, path02, path03...)
   ├─ For each entry (path01, path02, ...):
   │  ├─ Get address value (e.g., @system.verbs.globals)
   │  ├─ Navigate to target table (load structure, not values)
   │  ├─ Look up identifier in target table
   │  └─ If FOUND:
   │     ├─ Check type:
   │     │  ├─ ANY TYPE → ACCEPT (script, table, scalar, etc.)
   │     │  └─ GO TO: Use Found Object
   │     └─ If NOT FOUND → Continue to next path
   └─ If ALL PATHS EXHAUSTED → Continue to step 5

5. GUEST DATABASE TABLES (via path14 → system.compiler.files)
   ├─ path14 points to @system.compiler.files
   ├─ system.compiler.files contains entries like @["FULL_PATH_TO_ROOT"]
   ├─ For each guest database (alphabetical order):
   │  ├─ Search top-level tables in guest database
   │  └─ If FOUND → GO TO: Use Found Object
   └─ If ALL GUEST DATABASES EXHAUSTED → Continue to step 6

6. NOT FOUND
   ├─ If identifier is being ASSIGNED TO (simple name):
   │  └─> Implicit local creation (if allowed by context)
   │     Example: for i = 1 to 100  // Creates local(i)
   │     Value: nil (default for new locals)
   │
   ├─ If identifier is in an ADDRESS expression:
   │  ├─ SIMPLE NAME (@name):
   │  │  └─> Create address value with the name (no validation)
   │  │     Example: @name → Creates address, but dereferencing fails
   │  │     Note: adr = @name works, but adr^ fails with "name hasn't been defined"
   │  │
   │  └─ DOT-PATH (@path.to.newItem):
   │     ├─ Traverse path components (path, to, newItem)
   │     ├─ For each INTERMEDIATE component (not the final one):
   │     │  ├─ If component DOES NOT EXIST:
   │     │  │  └─> Runtime Error: "Can't evaluate the expression because the table \"[component]\" does not exist"
   │     │  └─ If component EXISTS but is NOT a table/record:
   │     │     └─> Runtime Error: "Can't evaluate the expression because the table \"[component]\" is not a table"
   │     └─ If all intermediate components valid:
   │        └─> Create address value pointing to not-yet-created final object
   │           Example: @system.verbs.doesNotExist (valid if system.verbs exists and is a table)
   │
   ├─ If identifier is part of a PATH being assigned (path.to.newItem = value):
   │  ├─ Traverse path components (path, to, newItem)
   │  ├─ For each INTERMEDIATE component (not the final one):
   │  │  ├─ If component DOES NOT EXIST:
   │  │  │  └─> Runtime Error: "Can't evaluate the expression because the table \"[component]\" does not exist"
   │  │  └─ If component EXISTS but is NOT a table/record:
   │  │     └─> Runtime Error: "Can't evaluate the expression because the table \"[component]\" is not a table"
   │  └─ If all intermediate components valid:
   │     └─> Create new entry in parent table
   │
   └─ If identifier is being READ (not assignment, not address):
      └─> Runtime Error: "Can't evaluate the name [identifier] because it hasn't been defined."
```

### Full Path Resolution

**Input**: Multi-component dot-path starting from root (e.g., `system.verbs.globals.string`)

**Note**: This is NOT a separate code path - implemented via Simple Name Resolution + table traversal.

```
FIRST COMPONENT:
├─ Use Simple Name Resolution for first component (Steps 1-6)
│  Example: "system" in "system.verbs.globals.string"
└─ If NOT FOUND → Runtime Error

INTERMEDIATE COMPONENTS:
├─ For each component between first and last:
│  ├─ Previous component MUST be a table or record
│  ├─ Load table structure (not values)
│  ├─ Look up component name in table
│  ├─ If FOUND and is TABLE/RECORD → Continue
│  └─ If NOT FOUND or NOT TABLE/RECORD → Runtime Error
│     └─ Error: "Can't evaluate the expression because the table \"[component]\" [does not exist|is not a table]"

FINAL COMPONENT:
├─ Parent component MUST be a table/record (from previous step)
├─ Look up final component name in parent table
├─ If FOUND:
│  ├─ Accept ANY TYPE (script, table, scalar, outline, etc.)
│  └─ GO TO: Use Found Object
└─ If NOT FOUND:
   ├─ If READING → Runtime Error
   └─ If ASSIGNING → Create new entry in parent table
```

### Partial Path Resolution

**Input**: Dot-path starting with simple name (e.g., `string.mid`, `file.exists`)

**Note**: This is NOT a separate code path - implemented via system.paths search (step #4).

```
ALGORITHM:
For each system.paths entry (path01, path02, ...):
   ├─ Navigate to target table
   ├─ Attempt to resolve ENTIRE PATH from this table
   │
   │  Example: Resolving "string.mid"
   │
   │  TRY path01 (@system.verbs.globals):
   │  ├─ Look up "string" → FOUND (SCRIPT)
   │  ├─ Try to look up "mid" in SCRIPT → FAIL (scripts have no children)
   │  └─ Continue to next path
   │
   │  TRY path03 (@system.verbs.builtins):
   │  ├─ Look up "string" → FOUND (TABLE)
   │  ├─ Try to look up "mid" in TABLE → SUCCESS
   │  └─ GO TO: Use Found Object
   │
   └─ If entire path NOT resolved → Continue to next path entry

RULES:
├─ INTERMEDIATE components (e.g., "string" in "string.mid"):
│  └─ MUST be TABLE or RECORD (to allow traversal)
│
└─ FINAL component (e.g., "mid" in "string.mid"):
   └─ Can be ANY TYPE

If NO paths resolve entire path:
└─> Runtime Error: "[path] can't be evaluated because it doesn't exist"
```

---

## Usage Dispatch After Resolution

**Context**: After an object is found, determine what to do with it based on syntax.

**IMPORTANT**: This logic is EXTERNAL to `langgettableval()`. The resolution functions only find objects; higher-level code determines usage.

```
USAGE TYPE 1: FUNCTION/SCRIPT CALL
├─ Syntax: identifier(...) or path.to.script(...)
├─ Example: string(123), file.exists(path)
│
├─ If found object is SCRIPT (scriptType):
│  ├─ Load script into memory if not already loaded
│  ├─ Compile script if not already compiled
│  ├─ Execute script with provided arguments
│  └─ Return script's return value
│
├─ If found object is KERNEL VERB:
│  ├─ Execute kernel verb handler
│  └─ Return result
│
└─ If found object is NOT callable (table, scalar, etc.):
   └─> Runtime Error: "Can't call [name] because it isn't a script"

USAGE TYPE 2: ASSIGNMENT TARGET
├─ Syntax: identifier = value or path.to.item = value
├─ If assigning to LOCAL: Store copy in local stack
└─ If assigning to DATABASE PATH: Create/update entry, mark dirty

USAGE TYPE 3: VALUE REFERENCE
├─ Syntax: identifier or path.to.value
├─ Load object into memory if needed
└─ Return copy of the object

USAGE TYPE 4: ADDRESS REFERENCE
├─ Syntax: @identifier or @path.to.item
├─ Create address value containing the path (NOT the object)
└─ Return address value (typeof = 'addr')

USAGE TYPE 5: PROPERTY ACCESS (dot-path continuation)
├─ Syntax: identifier.property
├─ If found object is TABLE or RECORD: Continue resolution
└─ If found object is NOT a table/record: Runtime Error

USAGE TYPE 6: TYPE CHECK
├─ Syntax: typeof(identifier) or defined(identifier)
├─ For typeof(): Return type code ('scpt', 'tabl', 'addr', etc.)
└─ For defined(): Return true if found, false otherwise (no error)
```

---

## Special Cases & Edge Conditions

### Implicit Local Variable Creation

```usertalk
for i = 1 to 100 { ... }  // No local(i) declared
```

**Behavior**:
1. Runtime searches for `i` using Simple Name Resolution
2. If NOT FOUND anywhere → Create implicit local variable on stack with value `nil`
3. **WARNING**: If `i` exists elsewhere (e.g., in parent table), it will be OVERWRITTEN
   - This can cause bugs and database corruption
   - Best practice: Always use explicit `local(i)`

### Address Values (@operator)

**Simple Name** (`@name`):
- ✅ Always succeeds - Creates address value with the name (no validation)
- ❌ Dereferencing fails - `adr^` errors because "name" doesn't exist

**Dot-Path** (`@path.to.newItem`):
- Traverses intermediate components (must be tables)
- ✅ Succeeds if parent exists - Creates address to not-yet-created object
- ❌ Fails if parent doesn't exist or isn't a table

### Assignment to Undefined Paths

**Final component doesn't exist**:
```usertalk
system.verbs.globals.newScript = "on test() { return 123 }"
```
- All intermediate components MUST exist
- Creates `newScript` as STRING (not script - limitation)

**Intermediate component doesn't exist**:
```usertalk
system.verbs.newTable.newScript = "..."
```
- Runtime Error at first missing/non-table component

### Local Variable Masking

```usertalk
local(string = "myString")
x = string        // Uses local (returns "myString")
y = string(123)   // Error: Can't call "myString" because it isn't a script
```

**Behavior**:
- Locals ALWAYS have priority (step 1 in search order)
- To access masked object: Use full path (`system.verbs.globals.string`)

---

## Root Cause Analysis

### What SHOULD Happen: string(123)

```usertalk
string(123)
```

**Resolution Flow**:
1. **Simple Name Resolution**: Search for "string"
   - Search locals → Not found
   - Search system.paths:
     - path01 = @system.verbs.globals
     - Navigate to system.verbs.globals
     - Look up "string" → FOUND (SCRIPT)
     - **ACCEPT** (final component can be ANY type)
     - GO TO: Use Found Object

2. **Use Found Object**: Detected as FUNCTION CALL (has arguments)
   - Found object is SCRIPT → ✅ Valid for function call
   - Load, compile, execute with argument (123)
   - ✅ Returns "123"

### What IS Happening: string(123) Bug

```usertalk
string(123)
```

**Resolution Flow (BROKEN)**:
1. **Simple Name Resolution**: Search for "string"
   - Search locals → Not found
   - Search system.paths:
     - path01 = @system.verbs.globals
     - Navigate to system.verbs.globals
     - Look up "string" → FOUND (SCRIPT)
     - **REJECT** because `langgettableval()` uses `tablevaltotable()` which only accepts TABLES
     - Continue searching...
     - path03 = @system.verbs.builtins
     - Navigate to system.verbs.builtins
     - Look up "string" → FOUND (TABLE)
     - **ACCEPT** (it's a table)
     - GO TO: Use Found Object

2. **Use Found Object**: Detected as FUNCTION CALL (has arguments)
   - Found object is TABLE → ❌ Invalid for function call
   - ❌ Runtime Error: "Can't call string because it isn't a script"

### The Problem

**Location**: `langgettableval()` in `Common/source/langvalue.c:3651-3673`

**Issue**: Uses `tablevaltotable()` which REJECTS non-table types (scripts, scalars, etc.).

**Current Code**:
```c
static boolean langgettableval(hdlhashtable htable, bigstring bsname, hdlhashtable *hval) {
    pushhashtable(htable);

    // First: search INSIDE the provided table
    if (hashtablelookup(htable, bsname, &val, &hnode)) {
        fl = tablevaltotable(val, hval, hnode);  // ❌ REJECTS non-tables!
    }
    else {
        // Fallback: external lookup
        fl = langexternalgettable(bsname, hval);
    }

    pophashtable();
    return fl;
}
```

**Why It's Wrong**:
- `tablevaltotable()` only accepts TABLE types
- When resolving `string`, finds SCRIPT but rejects it
- Continues searching and finds TABLE instead
- Returns wrong object

---

## Implementation Strategy

### Key Functions

| Function | File | Line | Purpose |
|----------|------|------|---------|
| `langgettableval()` | Common/source/langvalue.c | 3651-3673 | **BUG LOCATION** - Rejects non-table types |
| `langgetdotparams()` | Common/source/langvalue.c | 3941-4100+ | **ONLY CALLER** of langgettableval() (line 4047) |
| `langsearchpathvisit()` | Common/source/langvalue.c | ~3800+ | Iterates through system.paths entries |
| `langdirecttablelookup()` | Common/source/langvalue.c | 3759-3789 | Direct table lookup for path resolution |
| `tablevaltotable()` | Common/source/langvalue.c | ~2500+ | **PROBLEM FUNCTION** - Only accepts table types |

### Critical Context

**`langgettableval()` has ONLY ONE call site** at line 4047 in `langgetdotparams()`:

```c
// langgetdotparams() context (line 4047):
else
    fl = langgettableval (hsubtable, bsname, htable);
```

**Calling Scenario**:
- `langgetdotparams()` recursively traverses dot-paths
- When `hsubtable != nil`, it calls `langgettableval()` to find the next component
- This is the **intermediate/final component traversal** scenario
- The caller determines usage context
- `langgettableval()` should ONLY find and return the object

### Fix Options

**Option 1: Add parameter to indicate final vs intermediate**
```c
boolean langgettableval(hdlhashtable htable, bigstring bsname,
                        boolean acceptAnyType, hdlhashtable *hval)
```

**Option 2: Create separate functions**
```c
boolean langgettableval_anytime(...)  // For final components
boolean langgettableval_tableonly(...) // For intermediate components
```

**Option 3: Check at call site**
- Caller determines if final component
- Accepts any type without using `tablevaltotable()`

### Implementation Steps

1. **Analyze `langgetdotparams()` recursion**
   - Understand how it determines final vs intermediate components
   - Identify where the decision can be made

2. **Choose fix approach**
   - Consider maintainability, clarity, and minimal code change
   - Ensure PR #336 fixes remain intact

3. **Implement the fix**
   - Modify `langgettableval()` or calling logic
   - Accept ANY type for final components
   - Maintain table-only for intermediate components

4. **Test thoroughly**
   - All validation test cases (see below)
   - Ensure PR #336 functionality preserved

### PR #336 Compatibility

**Critical Requirements**:
- ✅ Nested table lookups (e.g., `defined(webserver.init)`) must still work
- ✅ Address value resolution must remain correct
- ✅ system.paths initialization must not regress

---

## Validation Test Cases

Once the fix is implemented, verify with these test cases:

```bash
# Simple name resolution (final component = script)
./frontier-cli/frontier-cli -e "string(123)"
# Expected: "123"

# Simple name resolution (final component = script)
./frontier-cli/frontier-cli -e "number(456)"
# Expected: 456

# Simple name resolution (final component = script)
./frontier-cli/frontier-cli -e "boolean(0)"
# Expected: false

# Partial path resolution (final component = script in table)
./frontier-cli/frontier-cli -e "string.mid('hello', 2, 3)"
# Expected: "ell"

# Partial path resolution (tests PR #336 fix for nested lookups)
./frontier-cli/frontier-cli -e "defined(webserver.init)"
# Expected: true (if webserver.init exists)

# Local masking
./frontier-cli/frontier-cli -e "local(string = 'test'); string"
# Expected: "test"

# Local masking with full path
./frontier-cli/frontier-cli -e "local(string = 'test'); system.verbs.globals.string(123)"
# Expected: "123"

# Address creation
./frontier-cli/frontier-cli -e "typeof(@system.verbs.globals.string)"
# Expected: 'addr'

# Address dereferencing
./frontier-cli/frontier-cli -e "x = @system.verbs.globals.string; typeof(x^)"
# Expected: 'scpt'
```

---

## Recommended Investigation Approach

### Phase 1: Understand the Code Flow

1. **Read `langgetdotparams()` completely**
   - Understand the recursion pattern
   - Identify where final vs intermediate is determined
   - Trace the flow for both `string(123)` and `string.mid()`

2. **Read `langgettableval()` completely**
   - Understand current logic
   - Identify where type checking happens
   - Note the use of `tablevaltotable()`

3. **Read `tablevaltotable()`**
   - Understand why it rejects non-tables
   - Determine if it's the right function to use

### Phase 2: Design the Fix

1. **Determine how to distinguish final vs intermediate**
   - Can the caller pass a flag?
   - Can we detect it within `langgettableval()`?
   - What's the cleanest approach?

2. **Choose implementation strategy**
   - Parameter vs separate functions vs call-site logic
   - Consider maintainability and clarity

3. **Plan for testing**
   - Ensure all validation cases covered
   - Verify PR #336 compatibility

### Phase 3: Implement and Test

1. **Make the code change**
   - Follow chosen strategy
   - Minimize code churn
   - Add comments explaining the logic

2. **Run all validation tests**
   - Verify `string(123)` works
   - Verify `string.mid()` works
   - Verify PR #336 cases work

3. **Review and refine**
   - Check for edge cases
   - Ensure error messages remain clear
   - Verify no regressions

---

## Success Criteria

The fix is complete when:

1. ✅ `string(123)` returns `"123"`
2. ✅ `string.mid('hello', 2, 3)` returns `"ell"`
3. ✅ `defined(webserver.init)` works (PR #336 preserved)
4. ✅ All validation test cases pass
5. ✅ No new test failures introduced
6. ✅ Code is maintainable and well-commented
