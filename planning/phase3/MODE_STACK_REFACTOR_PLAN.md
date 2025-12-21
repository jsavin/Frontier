# Mode Stack to Explicit Context Refactor - Complete Plan

**Status**: Ready for Implementation
**Owner**: Codex/Claude
**Created**: 2025-12-19
**Estimated Duration**: 8-13 days (2-3 weeks)
**Priority**: CRITICAL - Stability Blocker

## Executive Summary

The global mode stack pattern (`db_format_mode_push/pop/current`) has caused every Issue #123 bug and continues to produce non-deterministic behavior. This plan completely eliminates the mode stack in favor of explicit context passing throughout the database layer.

**Why This Matters**: The mode stack is the root cause of:
- Issue #123 Phase 1: Root table unpacked with wrong reader
- Issue #123 Phase 2: Child tables packed with wrong writer
- Issue #123 Phase 3: Migration produces invalid addresses
- Non-deterministic behavior depending on call stack depth
- Hidden state that makes debugging impossible

**Decision**: Full refactor (Option B) chosen over incremental approach. Stability is paramount.

---

## Current State Analysis

### Mode Stack Usage
- **95 call sites** across the codebase use `db_format_mode_push/pop/current`
- **Critical files**:
  - `Common/source/db.c` - Primary DB operations
  - `Common/source/db_format.c` - Mode management, migration
  - `Common/source/langhash.c` - Hash pack/unpack (15+ sites)
  - `Common/source/tablepack.c` - Table serialization (10+ sites)
  - `Common/source/db_reader_legacy.c`, `db_reader_v7.c`, `db_writer_v7.c` - Format handlers

### Existing Context Infrastructure
- `db_context` structure already exists in `db_format.h` (lines 34-38)
- Several `_context` variants already implemented:
  - `hashpacktable_context()`, `hashunpacktable_context()`
  - `dbassignhandle_context()`, `dbrefhandle_context()`
  - `dbstartsaveas_context()`, `dbendsaveas_context()`
- `db_context_guard` pattern exists for scoped mode changes

### What's Missing
- No initialization functions for context
- No clone/copy functions for creating derived contexts
- Non-context functions still used in ~90% of call sites
- Mode stack still primary mechanism (context is secondary)

---

## Architecture Design

### 0. Fundamental Principle: Single Decision Point

**CRITICAL**: The entire refactor is built on this architectural principle:

**Four Address Spaces During Migration**:
1. **On-disk v6 addresses** (32-bit LE): Addresses stored in v6 database files
2. **In-memory pointers** (64-bit): When `flinmemory=1`, variabledata is a memory pointer
3. **Expanded/aligned structures**: Objects in memory prepared for v7 format
4. **On-disk v7 addresses** (64-bit BE): New addresses written to v7 database

**Single Decision Point Rule**:
- **ONE decision point for reading**: One place decides "use v6 32-bit LE reader" vs "use v7 64-bit BE reader"
- **ONE decision point for writing**: One place decides "use v6 writer" vs "use v7 writer"
- **If there are multiple code branches making these decisions, the design is WRONG**

**Mode Management Hierarchy**:
1. **Top-level caller** (e.g., migration driver, langexternalpack_internal): Sets mode ONCE
2. **All child functions** inherit and use that mode - they DO NOT call db_context_apply()
3. **NO mode switching** inside individual pack/unpack functions (opverbpack_internal, wpverbpack_internal, etc.)

**Example During Migration**:
```c
// langexternalpack_internal - THE ONLY PLACE THAT MANAGES MODE
boolean langexternalpack_internal(const db_context *ctx, ...) {
    db_context working_context;
    if (ctx != NULL) {
        working_context = *ctx;  // Inherit output format from caller
    } else {
        db_context_init(&working_context);
    }

    // If need to load from v6 during migration, switch mode temporarily
    if (adapter_repack && !flinmemory) {
        db_context legacy_read_ctx = working_context;
        legacy_read_ctx.mode.use_64bit_format = false;
        legacy_read_ctx.mode.adapter_repack = false;
        db_context_apply(&legacy_read_ctx);

        // Load from v6
        load_external_from_disk(...);

        // Restore output format
        db_context_apply(&working_context);
    }

    // Now pack - all child pack functions use the current global mode
    // NO child function should call db_context_apply()
    switch (type) {
        case idoutlineprocessor:
            opverbpack_internal(&working_context, ...);  // Just passes context, doesn't apply it
            break;
    }
}

// opverbpack_internal - NEVER CALLS db_context_apply()
boolean opverbpack_internal(const db_context *ctx, ...) {
    // Just use the context for information, don't apply it
    // The global mode is already set correctly by the caller

    // NO db_context_apply() calls here!
    pack_outline(...);
    return pushlongondiskhandle(adr, *hpacked);  // Uses current global mode
}
```

**Why This Matters**:
- Prevents mode flip-flopping during recursive operations
- Makes debugging trivial: mode is set once at top level
- Eliminates race conditions and hidden state bugs
- Each function has clear responsibility: caller manages mode, children operate with it

### 1. Enhanced Context Structure

Located in `Common/headers/db_format.h`:

```c
typedef struct db_context {
    // Format mode (read/write version selection)
    db_format_mode mode;

    // Database handle (which database we're operating on)
    hdldatabaserecord database;

    // Save As state (source/destination for migration)
    db_saveas_state saveas;

    // Error tracking (optional, for future debugging)
    tyerrorcode last_error;
    const char *error_context;  // For diagnostics

    // Recursion depth tracking (prevent infinite loops)
    int recursion_depth;
    int max_recursion_depth;  // Default 50
} db_context;
```

**Design Principles**:
- **Immutable by default**: Pass as `const db_context *`
- **Stack-allocated**: Never heap-allocated, tied to function scope
- **Self-contained**: All state needed for DB operation is in context
- **Copy-on-modify**: Create new context for mode changes

### 2. Context Initialization API

Add to `Common/source/db_format.c`:

```c
// Initialize from current global state (backward compatibility)
void db_context_init(db_context *ctx);

// Initialize with explicit mode
void db_context_init_with_mode(db_context *ctx, const db_format_mode *mode);

// Initialize for reading v6 database
void db_context_init_legacy_read(db_context *ctx, hdldatabaserecord db);

// Initialize for writing v7 database
void db_context_init_modern_write(db_context *ctx, hdldatabaserecord db);

// Initialize for migration (legacy source, modern destination)
void db_context_init_migration(db_context *ctx,
                                hdldatabaserecord source_db,
                                hdldatabaserecord dest_db);

// Clone context with modified mode
void db_context_clone_with_mode(const db_context *src,
                                 db_context *dst,
                                 const db_format_mode *mode);
```

### 3. Context Guard Pattern (RAII-style)

Already exists but needs refinement in `Common/source/db_format.c`:

```c
typedef struct db_context_guard {
    db_format_mode prev_mode;
    db_saveas_state prev_saveas;
    hdldatabaserecord prev_db;
    boolean active;  // Safety flag to prevent double-exit
} db_context_guard;

void db_context_guard_enter(const db_context *ctx, db_context_guard *guard);
void db_context_guard_exit(db_context_guard *guard);

// Usage:
void some_function(const db_context *ctx) {
    db_context_guard guard = {0};
    db_context_guard_enter(ctx, &guard);

    // Call legacy code that reads global state
    legacy_function_without_context(...);

    db_context_guard_exit(&guard);  // Always restore
}
```

### 4. Propagation Rules

**Rule 1: Stack-Allocated Contexts**
- Contexts are stack variables, never heap-allocated
- Lifetime tied to function scope
- No malloc/free needed

**Rule 2: Pass by Const Pointer**
```c
// Good
boolean some_operation(const db_context *ctx, ...);

// Bad (forces copy)
boolean some_operation(db_context ctx, ...);
```

**Rule 3: Create New Context for Mode Changes**
```c
void parent_function(const db_context *ctx) {
    // Need legacy read mode for this operation
    db_context legacy_ctx;
    db_context_clone_with_mode(ctx, &legacy_ctx, &legacy_mode);

    child_function(&legacy_ctx);
    // Original ctx unchanged
}
```

**Rule 4: Pass Context Through Recursion**
```c
static void recursive_walk(const db_context *ctx, hdlhashtable ht, int depth) {
    if (depth >= ctx->max_recursion_depth)
        return;

    for (each child table) {
        recursive_walk(ctx, child, depth + 1);  // Same context
    }
}
```

**Rule 5: Use Guards for Global State Compatibility**
- During transition, use guards to sync context → globals
- After transition, remove guards entirely

---

## Implementation Plan

### Phase 1: Core Serialization (2-3 days)
**Goal**: Make hash/table pack/unpack deterministic

**Files to modify**:
- `Common/source/langhash.c` (~15 call sites)
- `Common/source/tablepack.c` (~10 call sites)
- `Common/source/langexternal.c` (~5 call sites)

**Tasks**:
1. Add context initialization functions to `db_format.c`
2. Convert all `hashpacktable()` callers to `hashpacktable_context()`
3. Convert all `hashunpacktable()` callers to `hashunpacktable_context()`
4. Convert `tablepacktable()` and `tableunpacktable()` callers
5. Remove `db_format_mode_push/pop` from these files
6. Add deprecation warnings to non-context variants

**Testing**:
```bash
./tools/run_headless_tests.sh
make -C tests save_migration_tests && ./tests/save_migration_tests
```

**Success Criteria**:
- Migration test produces identical output
- No mode stack push/pop in serialization paths
- All tests pass

### Phase 2: Database Operations (2-3 days)
**Goal**: Make DB read/write deterministic

**Files to modify**:
- `Common/source/db.c` (~30 call sites)

**Tasks**:
1. Convert `dbassign()` callers to `dbassign_context()`
2. Convert `dbreference()` callers to `dbreference_context()`
3. Convert `dbcopy()` callers to `dbcopy_context()`
4. Convert `dbpushdatabase/dbpopdatabase` to context variants
5. Update save-as operations to use context
6. Remove mode stack from `db.c`

**Testing**:
```bash
./tools/run_headless_tests.sh
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  --system-root databases/Frontier-v6-v7.root -e "defined(system)"
```

**Success Criteria**:
- Database loads correctly
- No mode stack in DB operations
- All tests pass

### Phase 3: Format Readers/Writers (1-2 days)
**Goal**: Make format-specific code deterministic

**Files to modify**:
- `Common/source/db_reader_legacy.c`
- `Common/source/db_reader_v7.c`
- `Common/source/db_writer_v7.c`

**Tasks**:
1. Add context parameter to all reader functions
2. Add context parameter to all writer functions
3. Update `dbopenfile()` to create appropriate context
4. Remove mode stack from reader/writer code

**Testing**:
```bash
# Test v6 → v7 migration
make -C tests save_migration_tests && ./tests/save_migration_tests

# Test loading v7 database
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  --system-root tests/test_save_migration-v7.root \
  -e "sizeOf(system.verbs.colors)"
```

**Success Criteria**:
- Migration works deterministically
- Loading v7 databases works
- No mode stack in format-specific code

### Phase 4: Migration Code (1-2 days)
**Goal**: Make migration completely deterministic

**Files to modify**:
- `Common/source/db_format.c`

**Tasks**:
1. Update `migrate_internal()` to use two explicit contexts
2. Remove ALL `db_format_mode_push/pop` from migration
3. Add `db_context_init_migration()` helper
4. Validate migration produces identical output

**Testing**:
```bash
# Run migration multiple times, compare output
for i in {1..5}; do
  make -C tests save_migration_tests
  mv tests/test_save_migration-v7.root tests/migration-run-$i.root
done

# All should be binary identical
md5 tests/migration-run-*.root
```

**Success Criteria**:
- Migration is deterministic (identical output every time)
- No mode stack in migration code
- External tables accessible post-migration

### Phase 5: Remaining Callers (2-3 days)
**Goal**: Eliminate all mode stack usage

**Files to modify**:
- `Common/source/tablestructure.c`
- `Common/source/tableverbs.c`
- `Common/source/cancoon.c`
- `Common/source/menupack.c`
- Any other files using mode stack

**Tasks**:
1. Convert all remaining callers to context variants
2. Remove all non-context wrappers
3. Delete `db_format_mode_push/pop/current` functions
4. Remove mode stack global variables

**Testing**:
```bash
# Full test suite
./tools/run_headless_tests.sh

# Verify no mode stack references remain
grep -r "db_format_mode_push\|db_format_mode_pop" Common/source/*.c
# Should return empty
```

**Success Criteria**:
- Zero mode stack calls in entire codebase
- All tests pass
- No mode stack infrastructure remains

---

## Backward Compatibility Strategy

### During Transition (Phases 1-4)

Create legacy wrappers in `Common/source/db_format.c`:

```c
// Global default context (replaces mode stack during transition)
static _Thread_local db_context g_default_context;
static _Thread_local boolean g_default_context_initialized = false;

static db_context *get_default_context(void) {
    if (!g_default_context_initialized) {
        db_context_init(&g_default_context);
        g_default_context_initialized = true;
    }
    // Refresh from current global state
    g_default_context.mode = db_format_mode_current();
    g_default_context.database = databasedata;
    db_saveas_state_snapshot(&g_default_context.saveas);
    return &g_default_context;
}

// Legacy wrapper (deprecated)
boolean hashpacktable(hdlhashtable ht, boolean flsave,
                      Handle *hpacked, boolean *flmustsave) {
    return hashpacktable_context(get_default_context(), ht, flsave,
                                 hpacked, flmustsave);
}
```

**Mark deprecated**:
```c
#ifdef FRONTIER_WARN_DEPRECATED_CONTEXT
#warning "hashpacktable() is deprecated, use hashpacktable_context()"
#endif
```

### After Phase 5

- Remove all legacy wrappers
- Delete mode stack infrastructure
- Update documentation

---

## Testing Strategy

### Unit Tests (Add to `tests/db_context_tests.c`)

**Test 1: Context Isolation**
```c
void test_context_isolation(void) {
    db_context ctx1, ctx2;
    db_format_mode v6_mode = {false, false, false};
    db_format_mode v7_mode = {true, false, false};

    db_context_init_with_mode(&ctx1, &v6_mode);
    db_context_init_with_mode(&ctx2, &v7_mode);

    // Pack same table with both contexts
    Handle h1, h2;
    hashpacktable_context(&ctx1, ht, false, &h1, NULL);
    hashpacktable_context(&ctx2, ht, false, &h2, NULL);

    // h1 should have v6 format, h2 should have v7 format
    assert_different_formats(h1, h2);
}
```

**Test 2: Recursive Mode Preservation**
```c
void test_recursive_mode_preservation(void) {
    db_context ctx;
    db_format_mode v7_mode = {true, false, false};
    db_context_init_with_mode(&ctx, &v7_mode);

    // Pack nested tables - all should use v7
    Handle h;
    hashpacktable_context(&ctx, nested_table, false, &h, NULL);

    // Verify all child tables use v7 format
    assert_all_tables_v7_format(h);
}
```

**Test 3: Guard Restore**
```c
void test_guard_restore(void) {
    db_context ctx;
    db_format_mode v7_mode = {true, false, false};
    db_context_init_with_mode(&ctx, &v7_mode);

    db_context_guard guard;
    db_context_guard_enter(&ctx, &guard);

    // Global state should reflect v7
    assert(db_format_mode_current().use_64bit_format);

    db_context_guard_exit(&guard);

    // Global state should be restored
}
```

### Integration Tests

**Test 4: Issue #123 No Regression**
```c
void test_issue_123_no_regression(void) {
    // Root cause: push legacy mode for read,
    // forget to pop before packing child tables

    db_context ctx;
    db_format_mode v7_mode = {true, false, false};
    db_context_init_with_mode(&ctx, &v7_mode);

    // Load v6 table (creates child context internally)
    hdlhashtable ht;
    load_legacy_table(&ctx, v6_adr, &ht);

    // Pack child table - should use v7, not inherited v6
    Handle h;
    hashpacktable_context(&ctx, child_table, false, &h, NULL);

    // Verify table header is v5 (v7 format), not v4
    assert_table_header_version(h, 5);
}
```

**Test 5: Migration Determinism**
```c
void test_migration_deterministic(void) {
    char output1[1024], output2[1024];

    // Run migration twice
    migrate_32bit_to_64bit("databases/Frontier-v6.root");
    strcpy(output1, db_format_last_backup_path());

    migrate_32bit_to_64bit("databases/Frontier-v6.root");
    strcpy(output2, db_format_last_backup_path());

    // Outputs should be binary identical
    assert_files_identical(output1, output2);
}
```

**Test 6: No Mode Stack Usage**
```c
void test_no_mode_stack_calls(void) {
    // After Phase 5, this should pass
    extern int g_mode_depth;
    int initial_depth = g_mode_depth;

    // Run full migration
    migrate_32bit_to_64bit("test.root");

    // Mode stack should be unchanged
    assert(g_mode_depth == initial_depth);
}
```

### Performance Benchmarks

```c
void benchmark_context_overhead(void) {
    db_context ctx;
    db_context_init(&ctx);

    // Baseline: current implementation with mode stack
    uint64_t start = get_time_ns();
    for (int i = 0; i < 10000; i++) {
        db_format_mode_push(&mode);
        hashpacktable(ht, false, &h, NULL);
        db_format_mode_pop();
    }
    uint64_t baseline = get_time_ns() - start;

    // New: explicit context passing
    start = get_time_ns();
    for (int i = 0; i < 10000; i++) {
        hashpacktable_context(&ctx, ht, false, &h, NULL);
    }
    uint64_t new_time = get_time_ns() - start;

    // Should be within 5% of baseline
    assert(new_time < baseline * 1.05);
}
```

---

## Validation Checklist

After each phase:

- [ ] All existing tests pass (`./tools/run_headless_tests.sh`)
- [ ] Migration test passes (`make -C tests save_migration_tests`)
- [ ] Migrated database accessible (`frontier-cli --system-root ... -e "defined(system)"`)
- [ ] External tables accessible (`-e "sizeOf(system.verbs.colors)"`)
- [ ] No mode stack calls in modified files (`grep -r "db_format_mode_push"`)
- [ ] New unit tests pass
- [ ] Performance within 5% of baseline

Final validation (after Phase 5):

- [ ] Zero mode stack references in codebase
- [ ] Mode stack infrastructure deleted
- [ ] Documentation updated
- [ ] All tests pass
- [ ] Migration is deterministic (identical output every run)

---

## Risk Assessment

### High Risks

**Risk 1: Incomplete Migration - Mixed State**
- **Description**: Some paths use mode stack, others use context
- **Mitigation**:
  - Migrate complete phases (all serialization, then all DB ops)
  - Add compile-time warnings for deprecated APIs
  - Comprehensive testing at each boundary
- **Rollback**: Revert to branch before phase started

**Risk 2: Performance Regression**
- **Description**: Context passing adds overhead
- **Mitigation**:
  - Context is small (<64 bytes), pointer passing is cheap
  - Benchmark at each phase
  - Target: <5% overhead acceptable
- **Rollback**: If >10% overhead, redesign context structure

**Risk 3: Breaking Existing Callers**
- **Description**: External code breaks when signatures change
- **Mitigation**:
  - Maintain legacy wrappers during transition
  - Deprecation warnings, not errors
  - Provide migration examples in docs
- **Rollback**: Keep legacy wrappers indefinitely if needed

### Medium Risks

**Risk 4: Context Cloning Errors**
- **Description**: Forgetting to clone before modifying
- **Mitigation**:
  - Provide clear helper functions
  - Add assertions in context setters
  - Code review guidelines
- **Rollback**: N/A (design issue, not implementation)

**Risk 5: Thread Safety Edge Cases**
- **Description**: Global state during transition
- **Mitigation**:
  - Already have thread-local mode stack
  - Explicit context is inherently thread-safe
  - Document: no sharing contexts between threads
- **Rollback**: N/A (already thread-safe)

---

## Rollback Strategy

### Immediate Rollback (<1 hour)
If critical issues arise during implementation:

1. Revert to branch before context changes
2. Document issue in GitHub issue
3. Plan fix offline

### Partial Rollback (<4 hours)
If only one phase is problematic:

1. Keep completed phases (e.g., Phase 1-2 done)
2. Revert problematic phase (e.g., Phase 3)
3. Add legacy wrappers back for reverted phase
4. Fix issue, re-test, re-land

### No Rollback After Phase 3
- Once serialization, DB ops, and readers/writers converted, core is stable
- Remaining phases are lower risk

---

## Success Metrics

### Code Quality
- **0 mode stack references** in codebase after Phase 5
- **<5% performance overhead** vs baseline
- **100% test coverage** for context operations

### Stability
- **Deterministic migration**: Same input → same output every time
- **No Issue #123 regressions**: External tables always accessible
- **Zero hidden state**: Every function knows its mode explicitly

### Maintainability
- **Clear propagation**: Context flow visible in call chain
- **Easy debugging**: Can inspect context at any point
- **Future-proof**: Thread-safe, no global state

---

## Current Status (as of 2025-12-19)

- **Phase**: Not started
- **Branch**: `fix/issue-123-migration-validation`
- **Blockers**: None
- **Dependencies**: None
- **Next Steps**: Begin Phase 1 implementation

---

## References

- **Planning Agent Output**: See terminal output from `subagent_type=Plan` execution
- **Issue #123 History**: See `planning/phase3/ISSUE_123_SOLUTION_DESIGN.md`
- **Mode Stack Documentation**: See `planning/phase3/modern_reader_writer_split.md`
- **External Table Docs**: See `docs/external_table_variable_management.md`

---

## Notes for Resuming Work

### Where We Left Off
1. User requested Option B (full refactor) instead of Option A (incremental)
2. Planning agent created comprehensive architecture design
3. Created this document before starting implementation
4. Ready to begin Phase 1: Core Serialization

### Quick Start Commands
```bash
# Create branch for refactor
git checkout -b refactor/explicit-context-no-mode-stack

# Run baseline tests
./tools/run_headless_tests.sh

# Start Phase 1
# Edit Common/source/db_format.c - add context init functions
# Edit Common/source/langhash.c - convert to context
# Edit Common/source/tablepack.c - convert to context
```

### Key Files to Start With
1. `Common/source/db_format.c` - Add context initialization API
2. `Common/headers/db_format.h` - Enhance context structure
3. `Common/source/langhash.c` - First conversion target
4. `Common/source/tablepack.c` - Second conversion target

### Testing After Phase 1
```bash
make -C tests clean
make -C tests save_migration_tests
./tests/save_migration_tests

# Should produce identical output to current migration
# All external tables should be accessible
```
