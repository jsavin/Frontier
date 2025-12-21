# Mode Stack Refactor Learnings - December 2025

**Date**: 2025-12-20
**Duration**: 1 day (intensive effort from context carry-over)
**Status**: ✅ Complete - Comprehensive documentation of refactor decisions and patterns

---

## Why This Document Matters

The global mode stack (`db_format_mode_push/pop/current`) was the root cause of **every single bug in Issue #123** across multiple phases. This document captures:

1. **Why the original pattern failed**
2. **How to identify similar problems in the future**
3. **The architectural patterns that fix it**
4. **Specific patterns to AVOID**
5. **Key insights about explicit state management**

Read this before touching database layer code!

---

## The Problem: Global Mode Stack Pattern

### What Was Happening (Before 2025-12-20)

```c
// ANTI-PATTERN: Mode stack push/pop
void somefunction() {
    db_format_mode old_mode = db_format_mode_current();

    // Push a different mode for reading v6 data
    db_format_mode read_v6_mode = {.use_64bit_format = false};
    db_format_mode_push(&read_v6_mode);

    // Call child function - it inherits the pushed mode!
    childfunction();  // ← Child is now in v6 read mode

    // Pop mode back to original
    db_format_mode_pop();
}
```

**The hidden issue**: ANY recursive call to `childfunction()` would see the mode state left on the stack. If the child called ANOTHER function that expected v7 mode, it would get v6 mode instead.

### Root Causes of Issue #123

#### Phase 1: Root Table Unpacked with Wrong Reader
```
Migration starts:
  1. database is v6 format
  2. Code checks: "Is database v7?" → NO
  3. Pushes legacy_read mode (use_64bit = false)
  4. Loads tables from v6 database ✓
  5. Pops mode back to v7

BUT:
  - If a child table was ALSO loaded, it might inherit the mode stack
  - Some code paths checked mode AFTER pop, getting wrong mode
  - Result: Some tables read with v6 reader, some with v7 reader
  - Inconsistent data in memory
```

#### Phase 2: Child Tables Packed with Wrong Writer
```
Pack root table:
  1. Sets v7 write mode
  2. Calls tablepacktable_internal()
  3. That function calls child table pack
  4. BUT: If child table pack does mode_push/pop, it changes global state
  5. Root table finishes packing with DIFFERENT mode than it started with

Result: Root table headers written in one format, child table data in another
```

#### Phase 3: WP Text Migration Segfault
```
During migration, pack functions do:
  1. db_format_fixup_external_handles() - PREMATURELY UPDATES HANDLES
  2. Then calls wp_portable_state_dbref() to read Paige data
  3. But handles now point to DESTINATION database
  4. Trying to read v6 Paige data from v7 database → invalid pointers
  5. Segmentation fault
```

### Why Guards Didn't Help

The code had `db_context_guard` pattern:

```c
// GUARD PATTERN: Save/restore context
db_context_guard guard;
db_context_guard_enter(&new_context, &guard);  // Save old state
db_format_mode_apply(&new_context.mode);       // Set new mode
// ... do work ...
db_context_guard_exit(&guard);                 // Restore old state
```

**But this is STILL global state manipulation**! The problem:
- If another thread (or recursive call) is in between guard_enter and guard_exit
- That code sees the changed global state
- After guard_exit, global state is restored, but child was already corrupted

**Guard pattern = False sense of safety while still having global state bugs**

---

## The Solution: Explicit Context Passing

### Core Principle: Single Decision Point

**CRITICAL ARCHITECTURAL RULE**:

> There must be ONE and ONLY ONE place where the code decides:
> - "Use v6 reader mode" vs "Use v7 reader mode"
> - "Use v6 writer mode" vs "Use v7 writer mode"

If multiple functions independently decide the mode, **the design is wrong**.

### The Single Decision Point Pattern

```c
// langexternalpack_internal() - THE ONLY PLACE THAT DECIDES
boolean langexternalpack_internal(const db_context *ctx, ...) {
    db_context working_context;

    // Decision Point: Should we use v6 read mode for loading?
    if (adapter_repack && !(**hv).flinmemory) {
        // For migration: load from v6 with v6 reader
        db_context legacy_read_context = {
            .mode = {.use_64bit_format = false, .adapter_repack = false},
            .database = source_database
        };
        ensure_external_in_memory(&legacy_read_context, hv);

        // Switch to v7 write mode for output
        working_context.mode = {.use_64bit_format = true, .adapter_repack = true};
        working_context.database = dest_database;
    } else {
        // Normal path: just use caller's context
        working_context = *ctx;
    }

    // Call child functions with explicit context
    // Children DO NOT make mode decisions - they just use the passed context
    opverbpack_internal(&working_context, hv, hpacked, flnew);
}
```

### How Child Functions Work (Pure Operations)

```c
// Child function: Pure operation, no mode management
boolean opverbpack_internal(const db_context *ctx, ...) {
    // Precondition: external is in memory (caller ensured this)
    if (!(**hv).flinmemory) {
        return false;  // Precondition violated
    }

    // Set mode from passed context (for C compiler register state)
    if (ctx != NULL) {
        databasedata = ctx->database;  // Set global for I/O operations
        db_format_mode_apply(&ctx->mode);  // Set mode registers
    }

    // Pack the outline - uses global state we just set
    fl = opverbpackoutline(ho, &hpackedoutline);

    // Store packed data using globals
    fl = dbassignhandle(hpackedoutline, &adr);

    // NO PUSH/POP, NO GUARDS, NO MODE CHANGES
    // Function is deterministic - same inputs → same outputs

    return fl;
}
```

**Key: Child functions**
- Take context as parameter
- Set global registers from context (needed for C)
- Perform operations
- Don't change mode/context
- Don't call guard functions
- Don't make decisions about format

---

## Four Address Spaces During Migration

Understanding these spaces is critical for data migration:

### 1. On-Disk v6 Addresses (32-bit Little-Endian)
- Stored in v6 database files on disk
- Read by v6 reader: `db_reader_legacy.c`
- Format: `uint32_t` in little-endian byte order
- Example: `0x0010FE00` on disk = `0x00FE1000` in memory after endian conversion

### 2. In-Memory Pointers (64-bit)
- When `flinmemory=1`, external's `variabledata` is a malloc'd pointer
- These are direct memory addresses from malloc()
- NOT database addresses - they're real C pointers
- Used during WP packing to reference Paige format data in memory

### 3. Expanded/Aligned Structures
- Structures loaded from v6 (packed for 32-bit) need expansion for v7 (64-bit)
- Fields may need padding/reordering for 64-bit alignment
- Data sizes change (e.g., `long` was 32-bit, now 64-bit)
- NOT the same as raw v6 addresses

### 4. On-Disk v7 Addresses (64-bit Big-Endian)
- Written to v7 database files on disk
- Written by v7 writer: `db_writer_v7.c`
- Format: `uint64_t` in big-endian byte order (for portability)
- Example: `0x0102030405060708` in big-endian = first 8 bytes are address

### Migration Data Flow

```
v6 Database on Disk (32-bit LE)
    ↓ (db_reader_legacy reads)
In-Memory Pointers (malloc'd)
    ↓ (WP extracts Paige data, converts to RTF)
Expanded Structures (padded for 64-bit)
    ↓ (db_writer_v7 writes)
v7 Database on Disk (64-bit BE)
```

### Critical Insight: Handle Updates Must Wait

```c
// WRONG: Update handles BEFORE packing
db_format_fixup_external_handles(hroot, dest_db);  // ❌
tablesavesystemtable(hroot);  // Now tries to read but handles wrong

// RIGHT: Update handles AFTER packing
tablesavesystemtable(hroot);  // Reads from source with source handles ✓
// Handles will be refreshed when migrated database is opened later
```

Why? Because:
1. Root table packing loads child externals
2. Child externals need to read from SOURCE database
3. Their handles must point to SOURCE
4. After packing completes, table is disposed
5. When database is opened later, fresh handles are loaded automatically

---

## WP Text Migration Specifics

### WP Packing Process (Paige → RTF)

During migration, WP packing does:

1. **Extract Paige data from memory**
   - External's variabledata contains pointer to Paige engine data
   - This data is in SOURCE database address space
   - Uses v6 addresses to navigate Paige structures

2. **Convert Paige to RTF (UTF-8)**
   - Extracts plain text from Paige format
   - Extracts inline style information
   - Creates RTF styling tags
   - Outputs UTF-8 text with RTF markup

3. **Write RTF to destination**
   - Packs RTF into Handle (memory buffer)
   - dbassignhandle() stores buffer to DESTINATION database
   - Uses v7 write mode

### Two Legacy WP Formats

The code handles TWO different v6 WP formats:

1. **Paige Format** (newer, more common)
   - Detected by Paige header structure
   - Full rich text with styles
   - Extraction: `wp_portable_extract_paige_text()`

2. **Word Solutions Format** (older, legacy)
   - Pre-Paige format from ancient Frontier
   - Plain text only (formatting dropped)
   - Detected when Paige header is invalid
   - Extraction: `wp_portable_extract_ws_text()`

### Error Messages During Migration

```
"Malformed Paige key header"
```

This is **NOT an error** - it means:
- External is in Word Solutions format, not Paige
- Code attempts to read Paige header, fails
- Falls back to Word Solutions extraction
- Migration continues successfully

---

## How to Identify Mode Stack Bugs in the Future

### Red Flags

Watch for these patterns:

1. **Push/Pop patterns that don't match in structure**
   ```c
   // BAD: If parent does push/pop, child shouldn't also do push/pop
   db_format_mode_push(&mode1);
   childfunction();  // ← If child also does push/pop, you have stacking issues
   db_format_mode_pop();
   ```

2. **Recursive calls with mode changes**
   ```c
   // BAD: Function changes mode, then calls itself
   void pack_recursive(table) {
       db_format_mode_push(read_mode);
       pack_recursive(child_table);  // ← Recursive call sees wrong mode
       db_format_mode_pop();
   }
   ```

3. **Mode decisions in multiple places**
   ```c
   // BAD: Multiple code paths deciding format
   if (is_migration) {
       db_format_mode mode1 = migration_mode;
       db_format_mode_apply(&mode1);
   } else if (is_load) {
       db_format_mode mode2 = load_mode;
       db_format_mode_apply(&mode2);
   }
   // Now both migrations and loads made independent decisions
   ```

4. **Guards used "for safety"**
   ```c
   // BAD: Guards give false sense of safety
   db_context_guard guard;
   db_context_guard_enter(..., &guard);
   // If another function reads global state here, it gets wrong state
   db_context_guard_exit(&guard);
   ```

### How to Fix

Replace with:
1. **Single decision point** that decides format once
2. **Explicit context parameter** passed to all children
3. **Child functions are pure operations** that don't change global mode
4. **Preconditions** that assert invariants (e.g., `flinmemory=1`)

---

## Key Principles to Remember

### 1. Global State is Evil
- Mode stack is global state
- Guards are false safety - still global state
- Every global state dependency makes code less deterministic
- Solution: Pass state explicitly as parameters

### 2. Explicit is Better Than Implicit
- Don't rely on callers to push mode before calling your function
- Don't assume call stack context
- Take parameters for everything you need
- Make dependencies visible

### 3. Single Responsibility
- One function decides format: `langexternalpack_internal()`
- Other functions perform operations: `opverbpack_internal()`, `wpverbpack_internal()`, etc.
- Don't mix decision-making with operation execution

### 4. Preconditions and Postconditions
- Assert requirements: "external must be in memory"
- Verify results: "address is valid"
- Make contract explicit

### 5. Determinism Matters
- Same inputs → same outputs (every time)
- No hidden dependencies on call stack depth
- No mode state from previous calls affecting current call
- Critical for testing and debugging

---

## Testing Implications

### What Tests Should Verify

1. **Context isn't modified during operation**
   ```c
   db_context before = ctx;
   operation(&ctx, data);
   assert(ctx == before);  // Operation didn't change context
   ```

2. **Operation produces same output with same context**
   ```c
   result1 = opverbpack(&ctx1, data);
   result2 = opverbpack(&ctx1, data);
   assert(result1 == result2);  // Deterministic
   ```

3. **Different contexts produce appropriate different results**
   ```c
   v6_ctx = {...use_64bit=false...};
   v7_ctx = {...use_64bit=true...};

   result_v6 = pack(&v6_ctx, data);
   result_v7 = pack(&v7_ctx, data);
   assert(result_v6 != result_v7);  // Format matters
   ```

### What NOT to Test

- ❌ Don't test that mode stack returns to previous state
- ❌ Don't test that guards save/restore state
- ❌ Don't test call-stack-dependent behavior
- If you're testing these, the design is wrong

---

## Code Examples: Before and After

### Before (Mode Stack, Broken)

```c
// opverbpack - version with mode stack
void opverbpack_internal(hdlexternalvariable h, ...) {
    db_format_mode prev_mode = db_format_mode_current();
    db_format_mode mode = {.use_64bit_format = true};
    db_format_mode_push(&mode);  // ← Sets global state

    // Child function might also push/pop, corrupting mode stack
    opverbpackoutline(ho, &hpacked);

    db_format_mode_pop();  // ← Restores global state

    // BUT: What if packoutline called something that changed mode?
    // We just popped assuming mode was still what we set, but it might not be!
}
```

### After (Explicit Context, Working)

```c
// opverbpack - version with explicit context
boolean opverbpack_internal(const db_context *ctx, hdlexternalvariable h, ...) {
    // Precondition check
    if (!(**hv).flinmemory) {
        return false;
    }

    // Set mode registers from context (for C)
    if (ctx != NULL) {
        databasedata = ctx->database;
        db_format_mode_apply(&ctx->mode);
    }

    // Pack the data - uses global state we just set
    fl = opverbpackoutline(ho, &hpackedoutline);

    // NO push/pop, NO guards, NO assumptions about call stack
    // Function is deterministic - same ctx, same data → same result

    return fl;
}
```

---

## References

- **Mode Stack Refactor Plan**: `planning/phase3/MODE_STACK_REFACTOR_PLAN.md`
- **Single Decision Point**: `planning/phase3/MODE_SINGLE_DECISION_POINT.md`
- **External Table Variables**: `docs/external_table_variable_management.md`
- **Migration Validation**: `planning/phase3/MIGRATION_VALIDATION_REPORT.md`

---

## Summary Checklist for Future Developers

When touching database code:

- [ ] Is there exactly ONE decision point for format selection?
- [ ] Are all child functions taking context as a parameter?
- [ ] Are child functions pure operations (no mode/context changes)?
- [ ] Is there any push/pop or guard pattern in the code? (If yes, fix it)
- [ ] Are preconditions explicit and checked?
- [ ] Could the same inputs produce different outputs? (If yes, design is wrong)
- [ ] Are all global state dependencies explicitly passed as parameters?
- [ ] Is the code deterministic regardless of call stack depth?

If you answer "no" to any of these, refactor before proceeding.
