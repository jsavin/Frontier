# ADR-002: Context-Based Format Versioning for Database Serialization

**Status**: `[IN PROGRESS]` - Picture/WPText externals done, ongoing hash/table refactoring
**Date**: 2025-12-23
**Author**: System Architecture Analysis
**Related**:
- ADR-001 (Multi-Database Context Management)
- `planning/architectural_decision_records/mode_management_single_decision_point.md`
- `planning/phase3/mode_stack_refactor/MODE_STACK_REFACTOR_PHASE1_DETAILED_v2.md`
- `planning/phase3/mode_stack_refactor/MODE_STACK_REFACTOR_PROGRESS.md`

---

## Context

### The Problem: Global Mode Stack Anti-Pattern

Frontier's v6→v7 migration requires reading 32-bit little-endian v6 databases and writing 64-bit big-endian v7 databases. The original implementation used a global mode stack (`db_format_mode_push/pop`) to manage format state.

**Critical Bug**: During migration, code would push legacy (v6) mode to read source tables, but recursive child operations inherited this mode implicitly through global state. This caused child operations to write v4 table headers instead of v5, producing a v7 database with inconsistent format. When later code tried to read these tables, it segfaulted because the v7 reader encountered unexpected v4 headers.

**Example of the failure**:
```c
// Migration code (WRONG)
db_format_mode legacy_mode = {false, false, false};
db_format_mode_push(&legacy_mode);  // Set v6 read mode
load_source_table();                 // Read v6 data

// Somewhere deep in the call chain:
pack_child_table();  // BUG: Inherits v6 mode, writes v4 header in v7 database!

db_format_mode_pop();  // Too late - damage done
```

**Root Causes**:
1. **Implicit mode inheritance**: Child functions didn't know what mode to use
2. **Global state mutation**: Multiple functions pushing/popping created stack corruption
3. **Temporal coupling**: Mode had to be set "before" operation, creating order dependencies
4. **Non-determinism**: Depending on call sequence, same function produced different output

### Four Address Spaces in Migration

The migration process involves four distinct address/format spaces:

1. **On-disk v6 addresses** (32-bit LE): Legacy database files
2. **In-memory pointers** (64-bit): When `flinmemory=1`, objects stored as RAM pointers
3. **Expanded structures**: Objects in memory prepared for v7 format
4. **On-disk v7 addresses** (64-bit BE): Modern database files

**The semantic truth**: Format is a property of the **destination**, not the operation. When packing data, the format should be determined by "where is this going?" not "where did it come from?"

### Alternative Approaches Considered

We evaluated three approaches for managing format versioning:

#### Alternative A: Property-Based Format on Data Structures
Add format fields to every data structure that needs versioning:

```c
typedef struct tyhashtable {
    db_format_mode output_format;  // Each structure knows its target format
    // ... existing fields
} tyhashtable;

boolean hashpacktable(hdlhashtable ht, Handle *hpacked) {
    // Use ht's embedded format
    if ((**ht).output_format.use_64bit_format) {
        pack_v7(...);
    } else {
        pack_v6(...);
    }
}
```

**Pros**:
- Format explicitly tied to data
- No need for parameter plumbing

**Cons**:
- Violates separation of concerns (data knows about serialization)
- Every data structure needs format field (memory overhead)
- Format is property of data, but semantically it's property of destination
- Migration creates mismatch: v6 data structure with v7 format field?
- Doesn't solve the "where is this going?" question

#### Alternative B: Keep Global Mode Stack, Fix Stack Management
Improve the push/pop pattern with better stack discipline:

```c
// Better stack management (still has problems)
#define MODE_GUARD_PUSH(mode) \
    db_format_mode __saved = db_format_mode_current(); \
    db_format_mode_push(mode);

#define MODE_GUARD_POP() \
    db_format_mode_pop(); \
    assert(__saved == db_format_mode_current());
```

**Pros**:
- Minimal code changes
- Familiar pattern

**Cons**:
- Still has implicit inheritance
- Still susceptible to stack corruption
- Doesn't fix fundamental problem of global state
- Hard to debug: mode is invisible in debugger
- Non-deterministic: same function, different mode, different output

#### Alternative C: Context-Based Format Versioning (CHOSEN)
Pass explicit context through all serialization functions:

```c
boolean hashpacktable_internal(const db_context *ctx, hdlhashtable ht,
                                Handle *hpacked) {
    // Format is explicit from context
    if (ctx->mode.use_64bit_format) {
        pack_v7(...);
    } else {
        pack_v6(...);
    }
}
```

**Pros**:
- Format is explicit and visible
- No global state mutation
- Deterministic: same inputs → same outputs
- Debuggable: context visible in stack traces
- Semantically correct: format is property of destination (passed as context)
- Thread-safe by design
- Separates concerns: data structures don't know about serialization format

**Cons**:
- Requires parameter threading through call chains
- Larger API surface (old + new functions during transition)
- Refactoring effort required

---

## Decision

**We adopt context-based format versioning (Alternative C) as the standard pattern for all database serialization operations.**

Format state is passed explicitly as a `const db_context *` parameter through all pack/unpack functions. This makes format decisions explicit, deterministic, and semantically correct.

### Rationale

1. **Alignment with ADR-001 (Multi-Database Context)**: Database handle is already passed via context. Format versioning naturally belongs in the same structure since it answers "how should I write to this database?"

2. **Semantic Correctness**: Format is a property of the destination, not the source. Context explicitly represents the destination, making the code's intent clear.

3. **Single Decision Point Principle**: Format is decided once by the caller and inherited explicitly (not implicitly) by callees. See `mode_management_single_decision_point.md`.

4. **Separation of Concerns**: Data structures remain pure (no serialization metadata). Serialization concerns are isolated to context parameter.

5. **Debuggability**: Context is visible in stack traces, debugger watch windows, and log messages. No hidden global state.

6. **Determinism**: Given the same context, a function always produces the same output. Critical for testing and reproducibility.

7. **Thread Safety**: No global state means inherent thread safety (future-proofing).

---

## Implementation Pattern

### The `_internal(const db_context *ctx, ...)` Pattern

All serialization functions follow this pattern:

```c
// New context-aware version (internal)
boolean <operation>_internal(const db_context *ctx, <params>) {
    // 1. Handle NULL context (backward compatibility)
    if (ctx == NULL) {
        db_context default_ctx;
        db_context_init(&default_ctx);
        return <operation>_internal(&default_ctx, <params>);
    }

    // 2. Use ctx->mode for format decisions
    if (ctx->mode.use_64bit_format) {
        // v7 64-bit BE logic
    } else {
        // v6 32-bit LE logic
    }

    // 3. Pass ctx to child operations (explicit inheritance)
    pack_child_operation(ctx, ...);

    return true;
}

// Old function becomes wrapper (backward compatibility)
boolean <operation>(<params>) {
    db_context ctx;
    db_context_init(&ctx);  // Uses current global mode
    return <operation>_internal(&ctx, <params>);
}
```

### Naming Convention

- **New function**: `<operation>_internal` - takes `const db_context *ctx` as first parameter
- **Old function**: `<operation>` - backward-compatible wrapper
- **Context parameter**: Always first parameter, always `const db_context *ctx`

### Context Initialization Helpers

```c
/* Initialize from current global state */
void db_context_init(db_context *ctx);

/* Initialize with explicit mode */
void db_context_init_with_mode(db_context *ctx, const db_format_mode *mode);

/* Initialize for v6 legacy read */
void db_context_init_legacy_read(db_context *ctx, hdldatabaserecord db);

/* Initialize for v7 modern write */
void db_context_init_modern_write(db_context *ctx, hdldatabaserecord db);

/* Clone context with different mode */
void db_context_clone_with_mode(const db_context *src, db_context *dst,
                                 const db_format_mode *mode);
```

### Mode Management Hierarchy

**Top-Level Caller (ONE place that manages mode)**:
```c
boolean langexternalpack_internal(const db_context *ctx, hdlexternalhandle h,
                                   Handle *hpacked, boolean *flnewdbaddress) {
    db_context working_context, legacy_context;
    boolean adapter_repack;

    if (ctx != NULL) {
        working_context = *ctx;  // Inherit output format from caller
    } else {
        db_context_init(&working_context);
    }

    adapter_repack = db_format_adapter_force_repack();

    // ================================================================
    // SINGLE DECISION POINT FOR READING
    // ================================================================
    if (adapter_repack && !(**hv).flinmemory) {
        // Crossing address space boundary: load from v6 source
        legacy_context = working_context;
        legacy_context.mode.use_64bit_format = false;
        db_context_apply(&legacy_context);

        // Load from v6 (caller manages mode switch)
        load_external_from_disk(...);

        // Switch back to output format
        db_context_apply(&working_context);
    }

    // ================================================================
    // SINGLE DECISION POINT FOR WRITING
    // ================================================================
    // Mode is set to output format - children use this mode
    switch ((**hv).id) {
        case idoutlineprocessor:
            ok = opverbpack_internal(&working_context, hv, hpacked, flnewdbaddress);
            break;
        case idpictprocessor:
            ok = pictverbpack_internal(&working_context, hv, hpacked, flnewdbaddress);
            break;
    }

    return ok;
}
```

**Child Functions (ZERO mode management)**:
```c
boolean opverbpack_internal(const db_context *ctx, hdlexternalvariable h,
                            Handle *hpacked, boolean *flnewdbaddress) {
    /*
     * Pure packing function - NO mode management
     * Preconditions:
     *   - flinmemory=1 (caller loaded external into memory)
     *   - Global mode set to output format
     * Postconditions:
     *   - Packed data appended to *hpacked
     *   - Global mode unchanged
     */

    // NO db_context_apply() calls!
    // NO mode switching!
    // Just operate with current mode

    // Pack outline structure (uses current mode)
    if (!opverbpackoutline(ho, &hpackedoutline))
        return false;

    // Write address (uses current mode)
    return pushlongondiskhandle(adr, *hpacked);
}
```

---

## Code Examples

### Before/After: Picture External Packing

**Before (using mode stack - WRONG)**:
```c
boolean pictverbpack(hdlexternalvariable h, Handle *hpacked, boolean *flnewdbaddress) {
    hdlpictvariable hv = (hdlpictvariable) h;
    const boolean adapter_repack = db_format_adapter_force_repack();
    db_format_mode prev_mode = db_format_mode_current();
    db_format_mode working_mode = prev_mode;

    if (!(**hv).flinmemory) {
        if (adapter_repack) {
            // BUG: Push v6 mode for reading
            working_mode.use_64bit_format = false;
            db_format_mode_push(&working_mode);
        }

        if (!pictverbinmemory(hv)) {
            if (adapter_repack)
                db_format_mode_pop();
            return false;
        }

        if (adapter_repack)
            db_format_mode_pop();  // Mode restored - but what about children?
    }

    // Pack picture - what mode is this using? Depends on call history!
    adr = (**hv).oldaddress;
    if (!dbassignhandle(hpackedpict, &adr))  // Mode is implicit, error-prone
        return false;

    return pushlongondiskhandle(adr, *hpacked);  // Also uses implicit mode
}
```

**After (using explicit context - CORRECT)**:
```c
boolean pictverbpack_internal(const db_context *ctx, hdlexternalvariable h,
                               Handle *hpacked, boolean *flnewdbaddress) {
    /*
    Pure packing function with explicit context

    Preconditions:
      - flinmemory=1 (caller has loaded external into memory)
      - ctx specifies the output format mode

    Postconditions:
      - Picture packed and address written to *hpacked
      - Returns true on success, false on failure
    */

    hdlpictvariable hv = (hdlpictvariable) h;

    // Format is explicit from ctx - no guessing!
    if (ctx != NULL) {
        if (ctx->database != nil)
            databasedata = ctx->database;

        db_context_apply(ctx);  // Set mode once, at top of function
    }

    // All subsequent operations use the mode from ctx
    adr = (**hv).oldaddress;
    if (!dbassignhandle(hpackedpict, &adr))  // Uses ctx->mode
        return false;

    return pushlongondiskhandle(adr, *hpacked);  // Uses ctx->mode
}

// Backward-compatible wrapper
boolean pictverbpack(hdlexternalvariable h, Handle *hpacked, boolean *flnewdbaddress) {
    return pictverbpack_internal(NULL, h, hpacked, flnewdbaddress);
}
```

### Migration Code: The Orchestrator

**How migration uses context to manage format**:
```c
boolean db_migrate_v6_to_v7(const char *source_path, const char *dest_path) {
    db_context source_ctx, dest_ctx;

    // Open source database (v6)
    db_context_init_legacy_read(&source_ctx, source_db);

    // Create destination database (v7)
    db_context_init_modern_write(&dest_ctx, dest_db);

    // Set global mode to v7 for writing
    dest_ctx.mode.use_64bit_format = true;
    dest_ctx.mode.adapter_repack = true;
    db_context_apply(&dest_ctx);

    // Pack external uses dest_ctx - writes in v7 format
    langexternalpack_internal(&dest_ctx, hexternal, &hpacked, &flnew);

    // Context explicitly controls format - no surprises
    return true;
}
```

### Helper Functions: Context Propagation

**Pattern for helper functions**:
```c
// Helper function that needs format awareness
static boolean pack_hash_value(const db_context *ctx, tyvaluerecord *val,
                                Handle hpacked) {
    if (!ctx) {
        db_context default_ctx;
        db_context_init(&default_ctx);
        return pack_hash_value(&default_ctx, val, hpacked);
    }

    // Use ctx->mode for decisions
    if (ctx->mode.use_64bit_format) {
        // Pack as v7
        return pack_long_v7(val->data.longvalue, hpacked);
    } else {
        // Pack as v6
        return pack_long_v6(val->data.longvalue, hpacked);
    }
}
```

---

## Common Pitfalls

### Pitfall 1: Don't Use Global Mode in `_internal` Functions

**WRONG**:
```c
boolean hashpacktable_internal(const db_context *ctx, ...) {
    db_format_mode mode = db_format_mode_current();  // ❌ WRONG!

    if (mode.use_64bit_format) {  // Using global instead of ctx
        pack_v7(...);
    }
}
```

**CORRECT**:
```c
boolean hashpacktable_internal(const db_context *ctx, ...) {
    if (!ctx) {
        db_context default_ctx;
        db_context_init(&default_ctx);
        return hashpacktable_internal(&default_ctx, ...);
    }

    if (ctx->mode.use_64bit_format) {  // ✅ Using explicit ctx
        pack_v7(...);
    }
}
```

### Pitfall 2: Don't Add Format Properties to Data Structures

**WRONG**:
```c
typedef struct tyhashtable {
    db_format_mode target_format;  // ❌ WRONG! Violates separation of concerns
    // ...
} tyhashtable;
```

**CORRECT**:
```c
// Data structure is pure - no serialization metadata
typedef struct tyhashtable {
    // Just data fields
} tyhashtable;

// Format is property of context, not data
boolean hashpacktable_internal(const db_context *ctx, hdlhashtable ht, ...) {
    // Use ctx->mode, not (**ht).target_format
}
```

### Pitfall 3: Context Must Be First Parameter

**WRONG**:
```c
boolean hashpacktable_internal(hdlhashtable ht, Handle *hpacked,
                                const db_context *ctx);  // ❌ ctx last
```

**CORRECT**:
```c
boolean hashpacktable_internal(const db_context *ctx, hdlhashtable ht,
                                Handle *hpacked);  // ✅ ctx first
```

**Rationale**: Consistent parameter ordering makes APIs predictable and easier to refactor.

### Pitfall 4: Don't Call `db_context_apply()` in Child Functions

**WRONG**:
```c
boolean pack_child_table(const db_context *ctx, hdlhashtable ht) {
    db_context_apply(ctx);  // ❌ WRONG! Child shouldn't manage global mode
    // ...
}
```

**CORRECT**:
```c
// TOP-LEVEL caller manages mode (ONCE)
boolean pack_parent_table(const db_context *ctx, hdlhashtable ht) {
    db_context_apply(ctx);  // ✅ Set mode once at top level

    // Children just use whatever mode is set
    pack_child_table(ctx, child_ht);  // Child doesn't call apply()
}

// CHILD function - pure operation, no mode management
boolean pack_child_table(const db_context *ctx, hdlhashtable ht) {
    // NO db_context_apply()!
    // Just operate with current mode
    if (ctx->mode.use_64bit_format) {
        pack_v7(...);
    }
}
```

**Why**: Only top-level orchestrators should manage global mode. Child functions are pure operations.

### Pitfall 5: Always Handle NULL Context

**WRONG**:
```c
boolean hashpacktable_internal(const db_context *ctx, ...) {
    // Assumes ctx is never NULL
    if (ctx->mode.use_64bit_format) {  // ❌ Segfault if ctx is NULL!
        pack_v7(...);
    }
}
```

**CORRECT**:
```c
boolean hashpacktable_internal(const db_context *ctx, ...) {
    if (!ctx) {
        db_context default_ctx;
        db_context_init(&default_ctx);
        return hashpacktable_internal(&default_ctx, ...);
    }

    if (ctx->mode.use_64bit_format) {  // ✅ Safe - ctx is never NULL here
        pack_v7(...);
    }
}
```

---

## Success Criteria

### When Is Refactoring Complete?

Phase 1 (Core Serialization) is complete when:

- [ ] All pack/unpack functions in `langhash.c` use `_internal(ctx, ...)` pattern
- [ ] All pack/unpack functions in `tablepack.c` use `_internal(ctx, ...)` pattern
- [ ] All external pack functions in `langexternal.c` use `_internal(ctx, ...)` pattern
- [ ] Zero `db_format_mode_push/pop` calls in `_internal` functions
- [ ] Migration produces deterministic output (5 runs = identical md5)
- [ ] All table headers in v7 database are v5 (not v4)
- [ ] External tables accessible after migration (`sizeOf(system.verbs.colors) > 0`)
- [ ] All tests pass
- [ ] No mode warnings in migration logs

### Validation Tests

```bash
# 1. Determinism (5 runs)
for i in {1..5}; do
  rm -f tests/test_save_migration-v7.root
  ./tests/save_migration_tests
  cp tests/test_save_migration-v7.root /tmp/migration-run-$i.root
done
md5 /tmp/migration-run-*.root  # All identical

# 2. External table access
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  --system-root tests/test_save_migration-v7.root \
  -e "sizeOf(system.verbs.colors) > 0"
# Expected: "true"

# 3. No mode stack in _internal functions
for file in Common/source/{langhash,tablepack,langexternal}.c; do
  grep "^[a-z_]*_internal(" $file | cut -d'(' -f1 | while read func; do
    if grep -A 50 "^.*$func(" $file | grep -q "db_format_mode_push\|db_format_mode_pop"; then
      echo "ERROR: $func still uses mode stack!"
      exit 1
    fi
  done
done

# 4. All tests pass
./tools/run_headless_tests.sh
```

---

## Related Work

### Related ADRs
- **ADR-001**: Multi-Database Context Management - Established `db_context` pattern for database handles
- **ADR-003** (future): Eliminating Global Database State - Will extend context to cover more global state

### Planning Documents
- `planning/architectural_decision_records/mode_management_single_decision_point.md` - Single Decision Point Principle
- `planning/phase3/mode_stack_refactor/MODE_STACK_REFACTOR_PHASE1_DETAILED_v2.md` - Detailed implementation plan
- `planning/phase3/mode_stack_refactor/MODE_STACK_REFACTOR_PROGRESS.md` - Current progress tracking

### Key Commits
- `77ea2c33` - Mode stack corruption fix + external materialization
- `89621aaf` - Picture external refactoring (context pattern established)
- `08e147e1` - WPText push/pop removal
- `b815ec0c` - LangExternal pack v7 write mode fix
- `2b82f47e` - Phase 1 - Explicit context passing through hashpackexternal

### Documentation
- `docs/external_table_variable_management.md` - External table address handling
- `planning/archive/phase3/mode_stack_refactor/` - Historical planning materials

---

## Consequences

### Positive

1. **Deterministic Serialization**: Same inputs + same context = same output (always)
2. **Debuggable**: Context visible in stack traces and debugger
3. **Testable**: Easy to test different format modes by passing different contexts
4. **Thread-Safe**: No global state mutation (future-proofing)
5. **Semantically Correct**: Format is property of destination (context), not source (data)
6. **Single Decision Point**: Format decided once by caller, inherited explicitly by callees
7. **Separation of Concerns**: Data structures remain pure, serialization isolated to context

### Negative

1. **Parameter Plumbing**: Context must be threaded through call chains
2. **API Surface**: Temporary duplication during transition (old + `_internal` functions)
3. **Refactoring Effort**: ~200+ call sites need updating across codebase
4. **Learning Curve**: New pattern for developers unfamiliar with explicit context passing

### Trade-offs

- **Simplicity vs. Safety**: Added complexity of context parameter buys elimination of entire class of mode inheritance bugs
- **Short-term Cost vs. Long-term Benefit**: Refactoring effort now prevents recurring bugs later
- **Backward Compatibility**: Old functions remain as wrappers, ensuring gradual migration path
- **Performance**: Minimal overhead (context copy on stack), acceptable for serialization operations

---

## Implementation Status

### ✅ Completed (as of 2025-12-23)

- Context initialization API (`db_context_init*` functions)
- Picture externals (`pictverbpack_internal`, `pictverbinmemory`)
- WPText externals (`wp_portable_state_dbref`)
- External materialization infrastructure
- Migration orchestration in `langexternalpack_internal`

### 🔄 In Progress

- Hash table pack/unpack (`hashpacktable_internal`, `hashunpacktable_internal`)
- Table pack/unpack (`tablepacktable_internal`, `tableunpacktable_internal`)
- Remaining 22 `dbpushdatabase` calls across codebase

### 📋 Remaining Work

- Menu verb functions
- Additional table operations
- Outline operations not yet refactored
- Database utility functions
- Legacy compatibility functions

**Progress Tracking**: See `planning/phase3/mode_stack_refactor/MODE_STACK_REFACTOR_PROGRESS.md`

---

## Future Work

### Phase 2: Database Operations Context Refactor

After Phase 1 complete, extend pattern to:
- `dbassign()`, `dbreference()`, `dbcopy()` in `Common/source/db.c`
- `dbpushdatabase()` elimination (22 remaining calls)
- Database utility functions

### Phase 3: Global State Elimination

Eventually:
- Thread-local context instead of global `databasedata`
- Fully explicit context passing throughout codebase
- Complete elimination of global mode state

---

## Questions for Future Review

1. Should context include error accumulation state for better error reporting?
2. Should we add recursion depth tracking to context for debugging?
3. Should context be heap-allocated for very deep call chains?
4. Should we add context validation asserts in debug builds?
5. Should migration have separate source/dest contexts to prevent accidental mixing?

---

**Approval**: Actively implementing, pattern validated by production use

**Next Steps**: Continue Phase 1 refactoring per `MODE_STACK_REFACTOR_PHASE1_DETAILED_v2.md`
