# Thread-Local Globals Migration Pattern

**Quick Reference**: Template for migrating global variables to thread-local storage using Frontier's `tythreadglobals` infrastructure.

**Status**: Established pattern as of ADR-005 (2026-01-02)
**Applies to**: Per-thread execution state that causes cross-call contamination or thread-safety issues
**See Also**: `planning/architectural_decision_records/ADR-005-parameter-state-thread-safety.md`

---

## When to Use This Pattern

Migrate a global variable to thread-local storage when **ANY** of these conditions are true:

### 1. Cross-Call Contamination
Global state set in one function call persists to the next call, causing incorrect behavior.

**Example**: `flnextparamislast` flag set in `file.open()` causes "too many parameters" error in subsequent unrelated verb call.

```c
// BAD: Global persists across calls
boolean flnextparamislast = false;

void verb_A() {
    flnextparamislast = true;  // Set for optional param
    // ... but param not consumed
}

void verb_B() {
    // Sees stale flag from verb_A!
    if (flnextparamislast) { /* Wrong! */ }
}
```

### 2. Thread-Safety Violations
Multiple threads executing concurrently would corrupt shared state.

**Example**: Two users executing scripts simultaneously both modify `bsfunctionname` for error messages.

```c
// BAD: Race condition in multi-user environment
bigstring bsfunctionname;

// Thread 1
strcpy(bsfunctionname, "file.open");
langerror(...);  // Uses bsfunctionname

// Thread 2 (concurrent)
strcpy(bsfunctionname, "string.lower");  // Corrupts Thread 1!
```

### 3. Per-Thread Execution Context
Variable tracks state specific to current thread's execution, not shared across threads.

**Examples**:
- `flscriptrunning` - Is THIS thread running a script?
- `currentprocess` - Which process is running in THIS thread?
- `outlinedata` - What outline is THIS thread editing?

---

## When NOT to Use This Pattern

**DO NOT** migrate globals that are:

### 1. Truly Shared Data
Globals that MUST be the same across all threads.

**Examples**:
- `hbuiltinfunctions` - Built-in function table (read-only after init)
- Database file handles - Managed by `db_context` pattern (ADR-001)
- Reference-counted caches - Use locks, not thread-local copies

### 2. Static Buffers (Case-by-Case)
Static buffers may be OK if:
- Read-only after initialization
- Protected by locks
- Explicitly designed for shared access

**Requires Analysis**: Audit usage patterns before deciding.

### 3. Configuration Values
Global settings that apply to entire process.

**Examples**:
- Feature flags (unless per-user feature flags in Phase 6+)
- System-wide limits and thresholds

---

## Migration Steps (Checklist)

Use this checklist for EVERY global migration:

### Step 1: Add Field to tythreadglobals

**File**: `Common/headers/processinternal.h`

```c
typedef struct tythreadglobals {
    // ... existing fields ...

    // Your migration (ADR-XXX: Description)
    <type> <fieldname>;

    // Reserved for future expansion (if major migration)
    void *<component>_reserved[4];

} tythreadglobals;
```

**Notes**:
- Add comment referencing ADR number and purpose
- Preserve field alignment (use `#pragma pack(2)` if needed)
- Group related fields together (e.g., all parameter state)

---

### Step 2: Update copythreadglobals()

**File**: `Common/source/process.c` (around line 1500)

```c
void copythreadglobals (hdlthreadglobals hglobals) {
    register hdlthreadglobals hg = hglobals;

    // ... existing code ...

    // ADR-XXX: Save thread-local state
    (**hg).<fieldname> = <fieldname>;

    // For strings:
    moveleft(<fieldname>, (**hg).<fieldname>, sizeof(<type>));

    // For arrays:
    moveleft(<fieldname>, (**hg).<fieldname>, sizeof(<type>) * <count>);
}
```

**Notes**:
- This function SAVES current thread state into the thread globals handle
- Called when swapping OUT of a thread
- Use `moveleft()` for strings, arrays, structs (not scalar types)

---

### Step 3: Update swapinthreadglobals()

**File**: `Common/source/process.c` (around line 1620)

```c
void swapinthreadglobals (hdlthreadglobals hglobals) {
    register hdlthreadglobals hg = hglobals;

    // ... existing code ...

    // ADR-XXX: Restore thread-local state
    <fieldname> = (**hg).<fieldname>;

    // For strings:
    moveleft((**hg).<fieldname>, <fieldname>, sizeof(<type>));

    // For arrays:
    moveleft((**hg).<fieldname>, <fieldname>, sizeof(<type>) * <count>);
}
```

**Notes**:
- This function RESTORES thread state from the handle
- Called when swapping IN to a thread
- Must be SYMMETRIC with copythreadglobals()

---

### Step 4: Update newthreadglobals()

**File**: `Common/source/process.c` (find initialization section)

```c
boolean newthreadglobals (hdlthreadglobals *hglobals) {
    register hdlthreadglobals hg;

    // ... allocate and clear handle ...

    // ADR-XXX: Initialize thread-local state
    (**hg).<fieldname> = <initial_value>;

    // For strings:
    setemptystring((**hg).<fieldname>);

    // For boolean flags:
    (**hg).<fieldname> = false;  // or true, depending on default

    return (true);
}
```

**Notes**:
- This function initializes NEW thread globals when thread is created
- Set sensible defaults (usually false for booleans, empty for strings)

---

### Step 5: Replace Global Declaration with Macro

**File**: `Common/source/<component>.c` (where global was declared)

**BEFORE**:
```c
boolean flnextparamislast = false;
```

**AFTER**:
```c
// ADR-XXX: Thread-local state (macro expands to thread globals)
// Legacy declaration removed, now accessed via tythreadglobals
// #define flnextparamislast ((**hthreadglobals).flnextparamislast)
```

**Notes**:
- Remove the actual global variable declaration
- Add comment explaining it's now thread-local
- Reference the macro definition location (usually in header)

---

### Step 6: Add Macro Definition to Header

**File**: `Common/headers/<component>.h` (where `extern` declaration was)

**BEFORE**:
```c
extern boolean flnextparamislast;
```

**AFTER**:
```c
// ADR-XXX: Thread-local state (backward-compatible macro)
#define flnextparamislast ((**hthreadglobals).flnextparamislast)
```

**Notes**:
- Macro provides transparent backward-compatible access
- Existing code using `flnextparamislast` works without changes
- Compiler expands to thread globals access automatically

---

### Step 7: Test Migration

**Required Tests**:

1. **Functional Tests**: Run full test suite
   ```bash
   ./tools/run_headless_tests.sh
   ```

2. **Thread Swap Test**: Verify state persists across thread swaps
   ```c
   // In test code:
   flnextparamislast = true;
   // Trigger thread swap (yield, process switch, etc.)
   assert(flnextparamislast == true);  // Should still be true!
   ```

3. **Isolation Test**: Verify threads don't see each other's state
   ```c
   // Thread 1 sets flag
   flnextparamislast = true;

   // Thread 2 checks flag (should be false, not true!)
   assert(flnextparamislast == false);
   ```

4. **Initialization Test**: New threads get correct defaults
   ```c
   // Create new thread
   // Check that flag has default value (false or true as designed)
   ```

**Success Criteria**:
- All existing tests pass (no regressions)
- Thread-specific state doesn't leak across threads
- State survives thread context swaps
- No compilation warnings or errors

---

## Complete Example: flnextparamislast Migration

### Before Migration

**Common/source/langvalue.c**:
```c
boolean flnextparamislast = false;  // Global variable
```

**Common/headers/lang.h**:
```c
extern boolean flnextparamislast;  // External declaration
```

**Usage in verb processors**:
```c
flnextparamislast = true;
getstringvalue(hparam1, 2, filename);
```

### After Migration

**Common/headers/processinternal.h**:
```c
typedef struct tythreadglobals {
    // ... existing fields ...

    // ADR-005: Parameter state migration (Phase 3)
    boolean flnextparamislast;
} tythreadglobals;
```

**Common/source/process.c** (copythreadglobals):
```c
(**hg).flnextparamislast = flnextparamislast;
```

**Common/source/process.c** (swapinthreadglobals):
```c
flnextparamislast = (**hg).flnextparamislast;
```

**Common/source/process.c** (newthreadglobals):
```c
(**hg).flnextparamislast = false;
```

**Common/source/langvalue.c**:
```c
// ADR-005: Thread-local parameter state (macro expands to thread globals)
// Legacy declaration removed, now accessed via tythreadglobals
```

**Common/headers/lang.h**:
```c
// ADR-005: Thread-local parameter state (backward-compatible macro)
#define flnextparamislast ((**hthreadglobals).flnextparamislast)
```

**Usage** (UNCHANGED):
```c
flnextparamislast = true;  // Macro expands to (**hthreadglobals).flnextparamislast
getstringvalue(hparam1, 2, filename);
```

---

## Common Pitfalls

### Pitfall 1: Forgetting to Initialize in newthreadglobals()

**Symptom**: New threads start with garbage values.

**Fix**: Always initialize fields in `newthreadglobals()`.

```c
// BAD: Field not initialized
boolean newthreadglobals(...) {
    // ... allocate handle ...
    // (**hg).mynewfield not set - GARBAGE VALUE!
    return true;
}

// GOOD: Field initialized
boolean newthreadglobals(...) {
    // ... allocate handle ...
    (**hg).mynewfield = false;  // Explicit default
    return true;
}
```

---

### Pitfall 2: Asymmetric Swap Functions

**Symptom**: State lost or corrupted across thread swaps.

**Fix**: Ensure `copythreadglobals()` and `swapinthreadglobals()` are mirrors.

```c
// BAD: Copy saves but swap doesn't restore
void copythreadglobals(...) {
    (**hg).myfield = myfield;  // Saves
}

void swapinthreadglobals(...) {
    // MISSING: myfield = (**hg).myfield;  // Should restore!
}

// GOOD: Symmetric save/restore
void copythreadglobals(...) {
    (**hg).myfield = myfield;
}

void swapinthreadglobals(...) {
    myfield = (**hg).myfield;  // Symmetric!
}
```

---

### Pitfall 3: Using moveleft() for Scalars

**Symptom**: Compilation warning or performance issue.

**Fix**: Use direct assignment for scalars, `moveleft()` for arrays/strings.

```c
// BAD: Overkill for boolean
moveleft(&flnextparamislast, &(**hg).flnextparamislast, sizeof(boolean));

// GOOD: Direct assignment for scalar
(**hg).flnextparamislast = flnextparamislast;

// GOOD: moveleft() for string
moveleft(bsfunctionname, (**hg).bsfunctionname, sizeof(bigstring));
```

---

### Pitfall 4: Macro Definition Order

**Symptom**: Compiler error "hthreadglobals undefined".

**Fix**: Ensure `hthreadglobals` is declared before macro is used.

```c
// Header inclusion order matters:
#include "processinternal.h"  // Defines tythreadglobals
#include "lang.h"              // Uses macro referencing hthreadglobals

// If reversed, macro won't compile!
```

---

## Performance Considerations

### Thread Swap Overhead

Adding fields to `tythreadglobals` increases thread swap time:
- **Booleans**: ~1-2 CPU cycles per field (negligible)
- **Strings**: ~10-50 cycles (depends on string length)
- **Arrays**: Proportional to array size

**Guideline**: Thread swaps are already expensive (hundreds of cycles). Adding a few booleans/strings is <1% overhead.

### Macro Expansion Overhead

Accessing thread-local via macro:
```c
flnextparamislast = true;
// Expands to:
(**hthreadglobals).flnextparamislast = true;
```

**Cost**:
- 1 handle dereference (`*hthreadglobals`)
- 1 struct field access (`.flnextparamislast`)
- Total: ~2-3 CPU cycles vs direct global (1 cycle)

**Guideline**: Negligible overhead. Compiler optimizations (register caching) often eliminate the difference.

### Memory Overhead

Each thread gets its own copy of all thread-local fields.

**Example**: Migrating 8 booleans + 2 bigstrings = ~40 bytes per thread.

**Guideline**: Frontier typically has <10 threads. 40 bytes × 10 threads = 400 bytes total. Acceptable overhead.

---

## Troubleshooting

### "hthreadglobals undefined" Error

**Cause**: Macro used before `hthreadglobals` is declared.

**Fix**: Ensure proper header inclusion order. `processinternal.h` must be included before headers that use thread-local macros.

---

### State Not Persisting Across Thread Swaps

**Cause**: Forgot to update `copythreadglobals()` or `swapinthreadglobals()`.

**Fix**: Add symmetric save/restore code to both functions.

**Test**:
```c
flnextparamislast = true;
threadyield();  // Force swap
assert(flnextparamislast == true);  // Should still be true!
```

---

### New Threads Start with Garbage Values

**Cause**: Forgot to initialize field in `newthreadglobals()`.

**Fix**: Add explicit initialization.

**Test**:
```c
// Create new thread
// Verify default value
assert(flnextparamislast == false);  // Should be initialized!
```

---

### Threads Seeing Each Other's State

**Cause**: Global variable not fully replaced (direct reference still exists somewhere).

**Fix**: Search codebase for direct references:
```bash
grep -r "flnextparamislast" --exclude="*.md" --exclude-dir=planning
# Should only find macro definition and usage, not global declaration
```

---

## Migration Checklist (Quick Reference)

Use this checklist for each global:

- [ ] **Step 1**: Add field to `tythreadglobals` (processinternal.h)
- [ ] **Step 2**: Update `copythreadglobals()` to save state (process.c)
- [ ] **Step 3**: Update `swapinthreadglobals()` to restore state (process.c)
- [ ] **Step 4**: Update `newthreadglobals()` to initialize (process.c)
- [ ] **Step 5**: Remove global declaration from source file
- [ ] **Step 6**: Replace `extern` with macro in header
- [ ] **Step 7**: Test full suite (`./tools/run_headless_tests.sh`)
- [ ] **Step 8**: Verify thread isolation (threads don't see each other's state)
- [ ] **Step 9**: Document in ADR and commit

---

## References

- **ADR-005**: Parameter state thread-safety (reference implementation)
- **processinternal.h**: `tythreadglobals` structure definition
- **process.c**: Thread swap function implementations
- **CLAUDE.md**: "BURN THE GLOBALS WITH FIRE" (line 498) - Strategic context

---

## Future Work

### Phase 6+: Per-User Context

When implementing collaborative ODB, may need additional thread-local fields:
- `user_id` - Which user owns this thread's operations?
- `session_id` - Session identifier for auditing
- `optimistic_version` - Version tracking for conflict detection

These will follow the same migration pattern established here.

### Global State Elimination

Long-term goal: NO mutable globals. Every piece of state is either:
1. **Thread-local** (this pattern) - Per-thread execution state
2. **Explicitly passed** (context parameters) - Per-operation state
3. **Reference-counted** (handles with locks) - Shared mutable data

See ADR-005 for strategic roadmap.
