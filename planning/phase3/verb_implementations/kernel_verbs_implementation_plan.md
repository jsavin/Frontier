# Phase 3: Kernel Verb Implementation & Validation Plan

## Overview

This plan outlines a systematic approach to pull in remaining kernel verb implementations from stub status and validate them with comprehensive unit tests. The goal is to move the 37 stub processors (407 verbs) toward real implementations, categorizing them as "working", "gui-dependent", "platform-specific", or "incompatible" in headless mode.

**Current State:**
- 51/51 processors initialized with stubs
- 14 processors with real implementations (~300 verbs)
- 37 processors with stub implementations (~407 verbs)
- All auto-generated and linked

---

## Critical Reference: UserTalk Documentation

**Source:** `docs/usertalk/docserver.userland.com/` (70+ processor categories with comprehensive verb documentation)

The DocServer documentation is **essential** for all verb implementations. It provides:
- Complete verb signatures with all parameter types
- Return value specifications and behavior
- Usage examples and best practices
- Platform-specific considerations (Windows vs. Mac)
- Character encoding standards and conversions
- RFC compliance notes where applicable
- Warning about edge cases and limitations

### Key Patterns from Documentation

**Verb Naming Conventions:**
- Accessor pairs: `processor.get()` / `processor.set()`
- Type converters: `processor.toType()` / `processor.fromType()`
- Action verbs: Simple names like `delete`, `create`, `replace`
- Boolean queries: Simple names returning true/false (e.g., `file.exists()`)

**Implementation Considerations from Docs:**

1. **Base64**: RFC 1521 compliant; `encode()` accepts any data type with optional line-wrapping (0 = no wrap)
2. **Bit**: 32-bit operations only (bits 0-31); all operations work with 32-bit numbers
3. **Clock**: Includes both duration waits (`sleepFor()`) and tick/timer operations; `ticks()` returns system ticks since startup
4. **Date**: 35+ verbs; extensive component access (day, month, year, dayOfWeek); conversions to string formats; calculations (tomorrow, nextMonth, etc.); ISO 8601 support
5. **File**: `exists()` works for both files and folders; always check existence before operations; platform-specific path handling
6. **String**: 70+ verbs covering case conversion, substring ops, text modification, encoding/decoding, validation, formatting, and parsing
7. **Character Encoding**: Verbs support multiple standards - ANSI, UTF-8, UTF-16, Mac Roman, Latin-Windows; critical for string operations
8. **GUI Processors**: Dialog, window, menu, target verbs fail without GUI context - cannot execute in headless mode

**Documentation Location Map:**
- Root index: `docs/usertalk/docserver.userland.com/index.html`
- Alphabetical reference: `docs/usertalk/docserver.userland.com/alphabeticalIndex.html`
- Individual processors: `docs/usertalk/docserver.userland.com/{processor}/index.html`
- Individual verbs: `docs/usertalk/docserver.userland.com/{processor}/{verb}.html`

---

## Frontier Runtime Architecture & Design

**Source:** Matt Neuberg's "The Definitive Guide - Frontier" (47 chapters, comprehensive reference)
**Location:** `docs/Frontier - The Definitive Guide - by Matt Neuberg/`

Understanding how Frontier's runtime works is essential for implementing verbs that integrate properly. Key concepts:

### The Persistent Database Model

**Frontier.root (The Core Database)**
- Central to all operations; Frontier is nearly non-functional without it
- Hierarchical, hash-indexed name-value storage for rapid access
- Contains: system verbs, scripts, configuration, all user-defined code and data
- Every verb execution operates in context of this persistent database

**Guest Databases**
- Additional databases can be opened alongside Frontier.root
- Managed by kernel via in-memory table at `system.compiler.files`
- All top-level items in guest databases exist in global scope in UserTalk domain
- Per CLAUDE.md instructions: This is critical for understanding context switching

### Variable Scope & Lifetime

**Local Variables** (within scripts)
- Created when script starts, destroyed when script ends
- Only accessible within the containing script
- Temporary and not visible to other scripts unless explicitly passed

**Database Entries** (persistent globals)
- Paths like `workspace.myVar` or `system.config.value`
- Persist across script execution and survive restarts
- Globally accessible from any script
- Stored permanently in database on disk

**Critical for Implementation:** Verbs need to understand whether they're operating on temporary local state or persistent global state, and manage both appropriately.

### The Type System & Coercion

**31 UserTalk Datatypes** including:
- Scalars: String, Integer, Long, Boolean, Point, Rectangle, Date, Keyword, Enum
- Collections: Table (hash-indexed), Record, List, Outline (hierarchical)
- Special: Handle, Pointer, Verb, Font, Script, WPText
- Type designation: Internally via string4 codes; user-facing via `system.compiler.language.constants`

**Coercion Rules**
- Automatic conversion between compatible types
- No strong typing: any object can change types mid-execution
- Parameter mismatch resolved via coercion, not type error
- Integer division yields integer (not float)
- String coercion produces displayable representation

**For Verbs:** Must handle flexible input types, apply coercion intelligently, and validate type assumptions where needed.

### Verb Execution & Parameter Passing

**Verb Definition Pattern (Eponymous Handler)**
```
on verbName (param1, param2, ...)
  // handler code
  return value
```
Requirements:
- Handler name must match script object's final path element
- Handler must use `on` keyword and be at script's top level (summit level)
- Parameters received are part of the handler signature

**Parameter Passing Conventions**
- **By value (default):** Changes to parameters don't affect caller's variables
- **By reference (using @):** Caller passes `@variable` to allow verb to modify caller's variable
  - Example: `date.get(clockValue, @day, @month, @year)` - three parameters passed by address
  - Critical pattern: many verbs use this to return multiple values

**Return Values**
- Implicit: Last expression evaluated (if no explicit return)
- Explicit: `return value` statement
- All verbs return a value (nil if none specified)

**For Implementation:** Understand when verbs should accept addresses vs. values; common pattern is returning multiple values via address parameters.

### Parameter Default Values

**Declaring Default Values in Handler Definition**
```
on myVerb (param1=defaultValue1, param2=defaultValue2)
  // implementation
```

- Defaults specified with `=` syntax in the `on` line parameter list
- Can be any valid expression
- Any parameter with a default can be omitted from the call
- If a value is provided in the call, it overrides the default

**Calling with Default Parameters**
```
myVerb()                    → uses all defaults
myVerb(10)                  → uses 10 for param1, default for param2
myVerb(10, 20)              → uses 10 for param1, 20 for param2
```

**Real Examples from Documentation:**
```
on addThree (a=1, b=2, c=3)
  return (a + b + c)

addThree()              → 6     [1 + 2 + 3]
addThree(10)            → 15    [10 + 2 + 3]
addThree(10, 20)        → 33    [10 + 20 + 3]
addThree(10, 20, 30)    → 60    [10 + 20 + 30]
```

**Critical for Implementation:** Default parameters make verbs more flexible and user-friendly. Many built-in verbs use them for optional configuration parameters.

### Named Parameters

**Syntax for Named Parameter Calls**
```
verb(paramName1:value1, paramName2:value2)
```

- Parameter names match the handler's parameter definitions
- Can be called in any order (not positional)
- Makes code self-documenting

**Real Examples from Documentation:**
```
on addThree (a=1, b=2, c=3)
  return (a + b + c)

addThree(c:30)                → 33    [defaults for a, b; 30 for c: 1+2+30]
addThree(c:30, b:20)          → 51    [default for a; 20 for b, 30 for c: 1+20+30]
addThree(c:30, a:5)           → 38    [5 for a, default for b, 30 for c: 5+2+30]
```

**Mixing Positional and Named Parameters**

You can mix both syntaxes with **specific rules:**
- **Ordered (positional) parameters come FIRST** and must start with the first parameter
- Ordered parameters can continue in sequence for any number of parameters (1st, 2nd, 3rd, etc.)
- **Named parameters come AFTER** all ordered parameters
- Named parameters can consist of any number of name:value pairs in any order
- **No ordered parameters can follow named parameters**

**Examples:**
```
on addThreeAndMultiplyToo (a, b, c)
  return (a + (2 * b) + (3 * c))

addThreeAndMultiplyToo(5, b:32, c:1)
  → 72    [a=5 (ordered), b=32 (named), c=1 (named)]

addThreeAndMultiplyToo(5, 32, c:1)
  → 72    [a=5, b=32 (ordered), c=1 (named)]

addThreeAndMultiplyToo(5, 32, 1)
  → 107   [a=5, b=32, c=1 (all ordered)]

addThreeAndMultiplyToo(c:1, a:5, b:32)
  → 72    [c=1, a=5, b=32 (all named, any order)]
```

**For Implementation:** Named parameters allow:
- More readable code (parameter purpose is explicit)
- Flexible calling: can use ordered params, named params, or both
- Adding new parameters to existing handlers safely (can be omitted by callers)
- Combining flexibility with clarity

**Important Note on Built-in Verbs:**
When looking up verbs in documentation, parameter names shown are descriptive (for clarity), not necessarily the actual parameter names in the implementation. To call a built-in verb with named parameters, check the database to find the actual parameter names.

### Special Evaluation Rules

Four verbs treat parameters specially (DO NOT evaluate them):
- `defined(objectReference)` - Check object existence without error
- `parentOf(objectReference)` - Get parent table
- `sizeOf(objectReference)` - Get object size
- `nameOf(objectReference)` - Get object name

These allow safe introspection of the database structure without causing errors if objects don't exist.

**For Implementation:** These patterns inform how to safely check for object existence and metadata within verbs.

### Threading Model

**Frontier is Multithreaded**
- Main agent thread runs background processes
- Each script execution creates a temporary script thread (destroyed on completion)
- Status shows thread count ("1 thread" = idle)
- Implies: Scripts can run simultaneously; verbs should be thread-safe where applicable

**Yielding & Concurrency**
- `sys.systemTask()` yields time to other processes
- `clock.waitSixtieths()` or `clock.waitSeconds()` for pausing
- Semaphores available for preventing collisions (`semaphore.lock()`, `semaphore.unlock()`)

**For Implementation:** Some verbs may need to coordinate with concurrency model; timing/blocking operations must yield properly.

### The Target Concept

**What is the Target**
- Current window or editor context (database, script editor, outline editor, etc.)
- Some verbs operate on implicit target if not explicitly addressed
- Maintained across script executions based on user interaction
- Persists in headless mode as a logical concept (though no visible window)

**For Verb Implementation:** Understand that target can be queried/set and affects scope of some operations.

### Key Implementation Patterns (from The Definitive Guide)

1. **Database Access Pattern:**
   - Check existence with `defined()` before accessing
   - Use hierarchical paths: `workspace.category.item`
   - Understand persistence implications

2. **Parameter Address Pattern:**
   - For multiple returns: Pass parameters by address
   - Caller uses `@variable` syntax
   - Handler uses address parameter to modify caller's variables

3. **Error Handling Pattern:**
   - Check preconditions with `defined()`
   - Use `dialog.notify()` for headless-safe user messages (queues for GUI mode, logs in headless)
   - Return boolean success/failure status

4. **Type Coercion Pattern:**
   - Accept flexible input types
   - Apply coercion rules appropriately
   - Document expected types and coercion behavior

5. **Scope Management Pattern:**
   - Local variables for temporary computation
   - Database entries for persistent state
   - Pass addresses for outgoing values

### Critical 1-Based Array Indexing

Unlike C and most modern languages, Frontier uses **1-based array indexing:**
- First element is `array[1]` (not `array[0]`)
- Second element is `array[2]`, etc.
- Must be remembered when implementing any list/string operations

### Operator Precedence & Evaluation

**Precedence Hierarchy (Highest to Lowest):**

1. **Function/Verb calls** - parentheses
2. **Unary operators** - `++` (prefix), `--` (prefix), `!`, `@`, `^`
3. **Multiplication/Division** - `*`, `/`, `%`
4. **Addition/Subtraction** - `+`, `-`
5. **Comparison operators** - `<`, `<=`, `>`, `>=`, `==`, `!=`
6. **String/List operators** - `beginsWith`, `contains`, `endsWith`
7. **Logical AND** - `&&` (short-circuits)
8. **Logical OR** - `||` (short-circuits)
9. **Assignment** - `=` (lowest precedence)

**Key Rule from The Definitive Guide:**
> "If an expression contains more than one arithmetic operator, the pairs are evaluated in left-to-right order, except that multiplication and division are evaluated before addition and subtraction. To override this order of evaluation, enclose in parentheses any expressions to be evaluated first."

**Examples:**
- `2 + 1 * 3` = 5 (multiplication first: 1 * 3 = 3, then 2 + 3 = 5)
- `(2 + 1) * 3` = 9 (parentheses force: 2 + 1 = 3, then 3 * 3 = 9)
- `x = y + 1` (addition happens before assignment, as intended)

**Associativity:**
- **Left-to-right:** All binary arithmetic operators, comparison operators, logical AND/OR
- **Right-to-left:** Assignment operator, unary operators

**Short-Circuit Evaluation (Critical for Implementation):**
- `&&` (AND): If first operand is false, second operand is never evaluated
- `||` (OR): If first operand is true, second operand is never evaluated
- Implications: Code in second operand might not execute; side effects may not occur

**Type Coercion During Evaluation:**
- Pairwise evaluation: `3 + 4 + "5"` evaluates as `(3 + 4) + "5"` = `7 + "5"` = `"75"`
- Each operation coerces types as needed for next operation
- Type precedence affects coercion: boolean < short < point/long/char/string4/direction < date < single/fixed < double < rect/rgb/alias/address < string < binary < list < record

**Increment/Decrement Operators:**
- **Prefix** (`++x`, `--x`): Operation happens before surrounding expression
- **Postfix** (`x++`, `x--`): Operation happens after surrounding expression

**Special Evaluation Notes:**
- `defined()`, `parentOf()`, `sizeOf()`, `nameOf()` don't evaluate their parameters (special forms)
- `evaluate()` verb takes a string and evaluates it as a UserTalk expression at runtime
- Square brackets `[]` in object references force extra evaluation

**Recommendation for Verbs:**
- Always use parentheses in complex expressions for clarity, even when not strictly necessary
- Be aware of short-circuit evaluation when implementing verbs that produce side effects
- Test type coercion behavior experimentally for edge cases (guide notes this can be surprising)
- Document any assumptions about evaluation order in verb implementations

### SCNS: Simple Cross-Network Scripting

**Overview**
SCNS is a kernel feature that enables calling remote scripting endpoints as if they were local verbs. This allows UserTalk scripts to transparently invoke RPC services (XML-RPC, JSON-RPC, etc.) on remote servers.

**Syntax**
```
local (endpoint = "protocol://server:port/path");
[endpoint].function.name (param1, param2);
```

**How It Works**
1. **URL Parsing:** The bracketed expression `[endpoint]` is evaluated as a URL string
2. **Remote Detection:** Code detects the bracketed expression is followed by dot operators (dotted name chain)
3. **Protocol Handler Lookup:** Extracts protocol (xmlrpc, json-rpc, etc.) from URL
4. **Handler Discovery:**
   - First looks in `user.remoteCallers.[protocol]` for custom handler
   - Falls back to `Frontier.remoteCallers.[protocol]` for built-in handler
5. **Function Name Construction:** Walks the dotted name chain to build procedure name
6. **Handler Invocation:** Calls the protocol handler with parameters: (server, procedureName, paramList)
7. **Response Handling:** Handler parses protocol-specific response and returns result

**Real Examples**
```usertalk
// XML-RPC call to betty.userland.com
local (endpoint = "xmlrpc://betty.userland.com/RPC2");
[endpoint].examples.getStateName (40);
// → Calls examples.getStateName(40) on XML-RPC server
// → Server returns state name for index 40

// Local testing endpoint
local (endpoint = "xmlrpc://127.0.0.1:5335/RPC2");
[endpoint].radio.helloworld ("Dave");
// → Calls radio.helloworld("Dave") on local XML-RPC server on port 5335
```

**Implementation Details (from source code)**

The SCNS implementation in `Common/source/langxml.c` includes:

- **langisremotefunction()**: Detects if a code tree represents a remote function call by checking for:
  - Bracket operator containing URL-like string
  - Followed by dot operators (dotted name chain)

- **parseremotefunction()**: Parses protocol URLs:
  - Extracts protocol name (before `:`)
  - Validates `://` prefix format
  - Extracts server/path information
  - Handles optional port numbers (commented out in current code)

- **findprotocolhandler()**: Locates handler script by protocol:
  - Checks user's custom handlers in `user.remoteCallers.[protocol]`
  - Falls back to system handlers in `Frontier.remoteCallers.[protocol]`
  - Returns error if protocol not supported

- **langremotefunctioncall()**: Executes remote call:
  - Decompiles the dotted name chain to build procedure name
  - Creates parameter list: [server, procedureName, originalParams]
  - Invokes protocol handler script
  - Returns handler's response to caller

**Protocol Handler Requirements**

A protocol handler is a UserTalk script that must:
1. Accept parameters: (server, procedureName, params)
2. Establish connection to remote server using specified protocol
3. Format and send request (XML-RPC, JSON-RPC, etc.)
4. Parse response according to protocol specification
5. Return the parsed result or error

**Example Handler Structure**
```usertalk
on xmlrpc (server, procedureName, params)
  // 1. Parse server URL to get host:port/path
  // 2. Format XML-RPC request with procedureName and params
  // 3. Send HTTP POST request
  // 4. Parse XML response
  // 5. Return result or error
```

**Current Implementation Status**
- **Core mechanism:** Fully implemented in kernel (`langxml.c`)
- **Protocol handlers:** Need to be registered in database at `user.remoteCallers` or `Frontier.remoteCallers`
- **Built-in protocols:** Would typically include xmlrpc, json-rpc, etc.
- **Port handling:** Code exists but commented out (likely for future use)

**For Headless Mode Considerations**
- SCNS requires `tcp.*` verbs to be implemented for network connectivity
- HTTP client functionality needed for protocol handlers
- May require special handling for timeouts/connection failures in headless environment

**Key Source Files**
- `Common/source/langxml.c` - SCNS core implementation
- `Common/source/langvalue.c` - Integration with verb call evaluation

---

## Phase 3 Structure: 5 Major Stages

### Stage 1: Categorization & Assessment (Foundation)

**Goal:** Create a comprehensive inventory of what each stub processor needs.

#### 1.1 Audit Remaining 37 Stub Processors
- **Create:** `planning/phase3/processor_audit.md`
- **For each processor:** Document
  - Processor name and EFP ID
  - Verb count
  - **Expected implementation status:**
    - ✅ Core functionality available
    - ⚠️ Partial (some verbs work, others need GUI/OS)
    - ❌ Requires GUI (dialog, menu, window, etc.)
    - ❌ Requires OS-specific code (file metadata, volumes, etc.)
  - **Dependencies on other verbs or systems**
  - **Complexity estimate** (low/medium/high)
  - **Known issues or blockers**
  - **Documentation reference:** Link to UserTalk docs for processor overview

**Key References:**

1. **UserTalk Verb Documentation** (ESSENTIAL):
   - `docs/usertalk/docserver.userland.com/{processor}/index.html` - Complete processor documentation with all verbs
   - Read processor overview and verb listings during audit
   - Note character encoding requirements, platform-specific concerns, and RFC compliance notes

2. **Implementation Patterns:**
   - `tests/headless_string_verbs.c` - String operations (70+ verbs, completed)
   - `tests/headless_file_verbs.c` - File I/O (~86 verbs, completed)
   - `tests/headless_table_verbs.c` - Table operations (~18 verbs, completed)
   - `tests/headless_db_verbs.c` - Database ops (~13 verbs, completed)

#### 1.2 Identify Low-Hanging Fruit (Quick Wins)
- Processors with simple, non-GUI logic
- No external dependencies
- Clear implementation from Frontier source
- Target: 5-10 processors suitable for quick implementation

**Candidates to evaluate:**
- `base64` (2 verbs) - Encoding/decoding
- `rgb` (2 verbs) - Color values
- `bit` (8 verbs) - Bitwise operations
- `semaphore` (2 verbs) - Synchronization primitives
- `clock` (7 verbs) - Time operations
- `speaker` (3 verbs) - Audio (may require OS bindings)

#### 1.3 Create Implementation Priority Matrix
- **Create:** `planning/phase3/implementation_priority.md`
- Rank processors by:
  - **Impact:** How many other systems depend on this?
  - **Effort:** Time to implement properly
  - **Feasibility:** Can it work in headless mode?
  - **Testing:** How easy is it to test?

---

### Stage 2: Unit Testing Framework Setup

**Goal:** Establish comprehensive testing patterns for verb implementations.

#### 2.1 Create Test Infrastructure
- **Create directory:** `tests/unit/` if not exists
- **Baseline:** Review existing test patterns
  - Look at `tests/unit/test_*.c` files for existing patterns
  - Document conventions for:
    - Test function naming (`test_<processor>_<verb_name>`)
    - Setup/teardown patterns
    - Error case handling
    - Edge case coverage

#### 2.2 Establish Testing Standards Document
- **Create:** `planning/phase3/verb_testing_standards.md`
- Define for each verb:
  - **Input validation tests:**
    - Valid inputs (multiple cases)
    - Invalid types/ranges
    - Boundary conditions
  - **Output validation tests:**
    - Return value correctness
    - Side effects (file creation, DB changes, etc.)
    - State consistency
  - **Error handling tests:**
    - Error messages
    - Graceful failure
    - Resource cleanup
  - **Integration tests:**
    - Interaction with other verbs
    - State dependencies

#### 2.3 Create Test Template Generator
- **Create:** `tools/test_generator/generate_verb_tests.py`
- **Purpose:** Auto-generate test file skeleton for each processor
- **Output:** `tests/unit/test_<processor>_verbs.c`
- **Includes:**
  - Test stubs for each verb
  - Common setup/teardown for processor
  - Example assertions and patterns

#### 2.4 Set Up Test Runner Integration
- **Update:** CI/CD pipeline or Makefile
- Commands:
  - `make test-verbs` - Run all verb tests
  - `make test-verbs-verbose` - With detailed output
  - `make test-coverage` - Coverage report
- **Integration:** With existing test infrastructure

---

### Stage 3: Implementation of Low-Hanging Fruit (Quick Wins)

**Goal:** Implement 5-10 simpler processors to establish patterns and build momentum.

#### 3.1 Implement Priority Group 1 (0-2 complexity)

**Target processors:** base64, rgb, bit, semaphore, clock

**For each processor:**

1. **Read UserTalk documentation** (CRITICAL FIRST STEP):
   - Open `docs/usertalk/docserver.userland.com/{processor}/index.html`
   - Read processor overview
   - Read each verb's documentation page to understand:
     - Exact parameter types and counts
     - Return value type and semantics
     - Usage examples from documentation
     - Edge cases and platform-specific notes
   - Note RFC compliance requirements (e.g., base64 = RFC 1521)
   - Understand character encoding implications if applicable

2. **Review implementation sources:**
   - Legacy Frontier at `/Users/jake/dev/tedchoward/Frontier`
   - Existing stub in `tests/headless_<processor>_verbs.c`
   - Look for patterns in similar completed processors

3. **Implement verbs:**
   - Replace stub function bodies with real logic following documentation
   - Use utility functions from existing codebase
   - Handle errors consistently
   - Match return types and behavior exactly as documented

4. **Write comprehensive tests:**
   - Use test template from Stage 2.3
   - Minimum 3 test cases per verb
   - Cover happy path + error cases from documentation
   - Test parameter boundary conditions mentioned in docs

5. **Validate:**
   - Compile without warnings
   - Tests pass 100%
   - No memory leaks (valgrind check)
   - Behavior matches documentation examples

6. **Document:**
   - Code comments for non-obvious logic
   - Update `planning/phase3/processor_audit.md` status
   - Note any deviations from documentation and why

#### 3.2 Code Review & Merge
- Self-review using code-review-bar-raiser agent
- Check for:
  - Style consistency
  - Error handling completeness
  - Test coverage
  - No new TODOs or FIXMEs

#### 3.3 Create Implementation Pattern Document
- **Create:** `planning/phase3/implementation_patterns.md`
- Document recurring patterns:
  - How to call utility functions
  - Error handling conventions
  - Memory management patterns
  - Type conversions

---

### Stage 4: Medium-Complexity Implementations

**Goal:** Implement 15-20 processors with moderate complexity.

#### 4.1 Categorize Medium-Complexity Processors

**Examples to evaluate:**
- `math` (3 verbs) - Mathematical operations
- `point`, `rectangle` (2-2 verbs) - Geometric types
- `rgb` continuation - Color operations
- `date` (30 verbs) - Date/time operations
- `keyboard`, `mouse` (4-2 verbs) - Input (may be GUI-dependent)
- `httpcontrol` (8 verbs) - HTTP client
- `tcp` (23 verbs) - Network operations

#### 4.2 Implementation Process (Batch Workflow)
- Break into batches of 3-4 processors
- Each batch follows same workflow:
  1. Implement all verbs in batch
  2. Write all tests
  3. Review as batch
  4. Merge as batch

#### 4.3 Establish Continuous Testing
- Run full test suite after each batch
- Ensure no regressions in previous implementations
- Maintain coverage percentage

---

### Stage 5: High-Complexity & Categorization

**Goal:** Handle remaining processors, categorizing those that can't be fully implemented.

#### 5.1 High-Complexity Processors (Real Implementation Needed)

**Examples:**
- `window` (31 verbs) - GUI, mostly incompatible
- `dialog`, `menu` (19, 14 verbs) - GUI-only
- `target` (3 verbs) - GUI context
- `pict` (4 verbs) - Image handling
- `htmlcontrol` (8 verbs) - GUI HTML renderer
- `statusbar` (5 verbs) - GUI element
- `mainwindow` (31 verbs) - GUI window management
- `search` (18 verbs) - May need UI for search dialogs
- `filemenu`, `editmenu` (14, 14 verbs) - GUI menus
- `inetd` (8 verbs) - Network daemon (may be feasible)
- `webserver` (10+ verbs) - HTTP server (feasible)

#### 5.2 Classification Process

For each remaining processor:

1. **Determine category:**
   - ✅ **Implementable:** Has no GUI dependencies, clear logic
   - ⚠️ **Partial:** Some verbs work, others need GUI
   - ❌ **GUI-Only:** All or most verbs require interactive UI
   - ❌ **Platform-Specific:** Needs deep OS integration
   - ❌ **External Service:** Requires third-party service

1. **Document in:** `planning/phase3/processor_classification.md`
   - Why classification decision was made
   - Which specific verbs are blockers (if partial)
   - Suggested workarounds

3. **Implement workarounds** (if applicable):
   - Stub functions that gracefully fail
   - Return meaningful error messages
   - Document in `planning/phase3/headless_limitations.md`

#### 5.3 Create Headless Mode Documentation

**Create:** `planning/phase3/headless_limitations.md`

Document for users/developers:
- Which processors/verbs work in headless mode
- Which don't and why
- Suggested alternatives or workarounds
- How to detect headless mode and adapt code

---

### Stage 6: Comprehensive Validation

**Goal:** Ensure all implementations are robust and well-tested.

#### 6.1 Test Suite Completion
- Unit tests for all 300+ implemented verbs
- Minimum coverage: 85% of new code
- Integration tests for verb interactions

#### 6.2 Regression Testing
- Test suite runs cleanly
- No memory leaks across all verbs
- Performance acceptable (document benchmarks)

#### 6.3 Documentation Completion
- **Create:** `planning/phase3/kernel_verbs_reference.md`
  - For each implemented processor
  - List of verbs with brief descriptions
  - Links to unit tests
  - Known limitations or caveats

#### 6.4 Update Main README
- Document verb implementation status
- Link to comprehensive reference
- Note about headless limitations

---

## Implementation Timeline Guidance

**Recommended approach** (no time estimates, just sequence):

1. **First:** Complete Stage 1 (categorization) - establishes the full picture
2. **Parallel:** Stage 2 (testing framework) - prepare infrastructure
3. **Then:** Stage 3 (quick wins) - build momentum with simple wins
4. **Next:** Stage 4 (medium complexity) - systematic batch processing
5. **Final:** Stage 5 & 6 (hard problems & validation) - finish strong

---

## Key Files & Locations

**Frontier Runtime Architecture** (ESSENTIAL FOUNDATIONAL KNOWLEDGE):
- Book: `docs/Frontier - The Definitive Guide - by Matt Neuberg/`
- Key chapters for verb implementation:
  - ch03.html: The Database (persistent model, scope, lifetime)
  - ch04.html: What a UserTalk Script Is Like
  - ch05.html: Handlers and Parameters (verb signature patterns, defaults, named parameters)
  - ch06.html: Referring to Database Entries (path syntax)
  - ch07.html: The Scope of Variables and Handlers
  - ch08.html: Addresses (parameter passing by reference with @)
  - ch09.html: Special Evaluation (defined, parentOf, sizeOf, nameOf, evaluate)
  - ch10.html: Datatypes (31 types, coercion rules, 1-based indexing, type precedence)
  - ch15.html: Math (arithmetic operators, operator precedence, unary operators)
  - ch21.html: Threading & Semaphores
  - ch44.html: Operators (comprehensive operator reference, short-circuit evaluation)
  - ch46.html: Verbs Reference (comprehensive verb listing)

**UserTalk Verb Documentation** (ESSENTIAL REFERENCE):
- `docs/usertalk/docserver.userland.com/` - Complete processor documentation (70+ processors)
- Index: `docs/usertalk/docserver.userland.com/index.html`
- By processor: `docs/usertalk/docserver.userland.com/{processor}/` (e.g., `base64/`, `string/`, `date/`)
- By verb: `docs/usertalk/docserver.userland.com/{processor}/{verb}.html`
- Alphabetical index: `docs/usertalk/docserver.userland.com/alphabeticalIndex.html`

**Existing implementations** (reference patterns):
- `tests/headless_string_verbs.c` - String operations (70+ verbs)
- `tests/headless_file_verbs.c` - File I/O (~86 verbs)
- `tests/headless_table_verbs.c` - Table operations (~18 verbs)
- `tests/headless_db_verbs.c` - Database operations (~13 verbs)
- `tests/headless_xml_verbs.c` - XML operations (~14 verbs)
- `tests/headless_html_verbs.c` - HTML operations (~23 verbs)

**Verb definitions:**
- `Common/resources/Win32/kernelverbs.rc` - Master verb list (707 verbs across 51 processors)

**Test templates:**
- `tests/unit/test_*.c` - Existing test patterns

**Parser & generation:**
- `tools/kernelverbs_parser/parse_kernelverbs.py`
- `tools/kernelverbs_parser/generate_processor_stubs.py`

**Legacy reference:**
- `/Users/jake/dev/tedchoward/Frontier` - Original implementations

---

## Success Criteria

✅ **Phase 2 Complete When:**
- All 37 stub processors have been reviewed and categorized
- 20+ processors have real implementations
- All implementations have comprehensive unit tests (85%+ coverage)
- All unit tests pass
- No memory leaks detected
- Headless limitations documented for users
- README and reference docs updated
- No compiler warnings for verb-related code

---

## Appendix: Processor Categories

### Likely Implementable (20+ processors)
`base64`, `bit`, `clock`, `date`, `rgb`, `point`, `rectangle`, `math`, `string` (completed), `file` (completed), `table` (completed), `db` (completed), `xml` (completed), `html` (completed), `re`, `crypt`, `sqlite` (completed), `mysql` (completed), `tcp`, `inetd`, `webserver`, `python`, `dll`, `rez`, `launch`, `clipboard`, `thread`, `mrcalendar`, `searchengine`

### Likely GUI-Dependent (7 processors)
`dialog`, `menu`, `window`, `target`, `pict`, `filemenu`, `editmenu`, `htmlcontrol`, `mainwindow`, `statusbar`

### Partially Feasible (10 processors)
`search` (may need UI), `kb`/`mouse`/`speaker` (input-related), `osa` (Apple events), `lang` (partially done), `sys`, `launch`, `frontier`, `script`

### Under Review
`op`, `opattributes` (core operations), others pending detailed analysis

