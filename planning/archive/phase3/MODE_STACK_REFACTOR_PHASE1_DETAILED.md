# Mode Stack Refactor - Phase 1 Detailed Breakdown

**Phase**: Core Serialization - Make hash/table pack/unpack deterministic
**Estimated Duration**: 2-3 days
**Created**: 2025-12-19
**Status**: Ready to begin

---

## Pre-Flight Checklist

Before starting any changes:

```bash
# Ensure clean working directory
git status  # Should show no uncommitted changes

# Create feature branch
git checkout -b refactor/explicit-context-no-mode-stack

# Run baseline tests (record results)
./tools/run_headless_tests.sh > /tmp/baseline_tests.log 2>&1
echo $?  # Should be 0

# Run migration test (save baseline output)
make -C tests save_migration_tests && ./tests/save_migration_tests
cp tests/test_save_migration-v7.root /tmp/baseline_migration.root

# Record baseline file sizes for comparison
ls -lh tests/test_save_migration-v7.root
md5 tests/test_save_migration-v7.root > /tmp/baseline_migration.md5

# Note: Baseline is for reference only. As we fix mode stack bugs,
# the v7 output WILL change (and should improve). The key metric is
# DETERMINISM (same output every time), not equality to baseline.
```

**Success Criteria**: All tests pass, migration produces deterministic output (not necessarily identical to baseline)

---

## Step 1.0: Audit Existing Code (Investigation Only - No Changes)

**Goal**: Understand current state before making changes

**Duration**: 30 minutes

### Tasks:

```bash
# 1. Find all _context variants that already exist
grep -n "hashpacktable_context\|hashunpacktable_context" Common/source/langhash.c
grep -n "tablepacktable_context\|tableunpacktable_context" Common/source/tablepack.c

# 2. Find all mode stack push/pop in target files
grep -n "db_format_mode_push\|db_format_mode_pop" Common/source/langhash.c
grep -n "db_format_mode_push\|db_format_mode_pop" Common/source/tablepack.c
grep -n "db_format_mode_push\|db_format_mode_pop" Common/source/langexternal.c

# 3. Count total call sites
grep -c "hashpacktable(" Common/source/langhash.c
grep -c "hashunpacktable(" Common/source/langhash.c

# 4. Check current db_context structure
grep -A20 "typedef struct db_context" Common/headers/db_format.h

# 5. Check for existing context init functions
grep -n "db_context_init" Common/source/db_format.c
```

**Document findings**:
- Which `_context` functions already exist?
- How many mode stack push/pop sites need removal?
- What's the current `db_context` structure?
- Do init functions already exist?

**Output**: Create checklist in this document (scroll to Audit Results section below)

**No commit** - investigation only

---

## Step 1.1: Add Context Initialization API

**Goal**: Create foundational context management functions

**Duration**: 1 hour

**Files to modify**:
- `Common/headers/db_format.h`
- `Common/source/db_format.c`

### Changes:

#### 1.1.1: Enhance db_context structure (if needed)

**File**: `Common/headers/db_format.h`

Check current structure, ensure it has:
```c
typedef struct db_context {
    db_format_mode mode;              // Already exists
    hdldatabaserecord database;       // Already exists
    db_saveas_state saveas;          // Already exists

    // Add these if missing:
    tyerrorcode last_error;          // For debugging
    const char *error_context;       // For diagnostics
    int recursion_depth;             // Safety counter
    int max_recursion_depth;         // Default 50
} db_context;
```

**Test**: `make -C tests clean && make -C tests save_migration_tests`
- Should compile without errors
- No functional changes yet

#### 1.1.2: Add context initialization functions

**File**: `Common/source/db_format.c`

Add these functions (find appropriate location near other db_context code):

```c
/* Initialize context from current global state (for backward compatibility) */
void db_context_init(db_context *ctx) {
    if (!ctx) return;

    memset(ctx, 0, sizeof(*ctx));
    ctx->mode = db_format_mode_current();
    ctx->database = databasedata;
    db_saveas_state_snapshot(&ctx->saveas);
    ctx->recursion_depth = 0;
    ctx->max_recursion_depth = 50;
    ctx->last_error = noErr;
    ctx->error_context = NULL;
}

/* Initialize context with explicit mode */
void db_context_init_with_mode(db_context *ctx, const db_format_mode *mode) {
    if (!ctx || !mode) return;

    db_context_init(ctx);
    ctx->mode = *mode;
}

/* Initialize context for v6 legacy read */
void db_context_init_legacy_read(db_context *ctx, hdldatabaserecord db) {
    db_format_mode legacy_mode = {false, false, false};
    db_context_init_with_mode(ctx, &legacy_mode);
    ctx->database = db;
}

/* Initialize context for v7 modern write */
void db_context_init_modern_write(db_context *ctx, hdldatabaserecord db) {
    db_format_mode modern_mode = {true, false, false};
    db_context_init_with_mode(ctx, &modern_mode);
    ctx->database = db;
}

/* Clone context with different mode */
void db_context_clone_with_mode(const db_context *src, db_context *dst,
                                 const db_format_mode *mode) {
    if (!src || !dst || !mode) return;

    *dst = *src;  // Shallow copy
    dst->mode = *mode;
}
```

**Add function declarations to header**:

**File**: `Common/headers/db_format.h`

Add near other db_context declarations:
```c
/* Context initialization API */
extern void db_context_init(db_context *ctx);
extern void db_context_init_with_mode(db_context *ctx, const db_format_mode *mode);
extern void db_context_init_legacy_read(db_context *ctx, hdldatabaserecord db);
extern void db_context_init_modern_write(db_context *ctx, hdldatabaserecord db);
extern void db_context_clone_with_mode(const db_context *src, db_context *dst,
                                       const db_format_mode *mode);
```

### Testing:

```bash
# 1. Compile
make -C tests clean
make -C tests save_migration_tests

# 2. Run migration test 3 times to verify determinism
for i in {1..3}; do
  rm -f tests/test_save_migration-v7.root
  ./tests/save_migration_tests
  cp tests/test_save_migration-v7.root /tmp/step1.1-run$i.root
done

# 3. Verify deterministic output (all 3 runs identical)
md5 /tmp/step1.1-run*.root
# All 3 should have SAME md5

# 4. Compare to baseline (optional - for information only)
echo "Baseline:"
cat /tmp/baseline_migration.md5
echo "Current:"
md5 tests/test_save_migration-v7.root
# Note: Output MAY differ from baseline if we fixed bugs - that's OK!

# 4. Run full test suite
./tools/run_headless_tests.sh
```

**Success Criteria**:
- ✅ All code compiles without errors
- ✅ Migration test produces deterministic output (3 runs → same md5)
- ✅ Migrated database can be loaded and accessed
- ✅ All tests pass
- ✅ New functions exist and are callable (verified by successful compilation)

**Note**: Output may differ from baseline - that's expected as we fix bugs!

### Commit:

```bash
git add Common/headers/db_format.h Common/source/db_format.c
git commit -m "refactor(context): Add context initialization API for mode stack elimination

Add foundational context management functions to support explicit context
passing throughout the database layer. These functions will replace the
global mode stack pattern.

New functions:
- db_context_init() - Initialize from current global state
- db_context_init_with_mode() - Initialize with explicit mode
- db_context_init_legacy_read() - For v6 database operations
- db_context_init_modern_write() - For v7 database operations
- db_context_clone_with_mode() - Clone context with mode change

Enhanced db_context structure with error tracking and recursion depth
safety counters.

Testing: All tests pass, migration output unchanged (binary identical).

Part of Phase 1: Core Serialization (Step 1.1)
Ref: planning/phase3/MODE_STACK_REFACTOR_PLAN.md

🤖 Generated with Claude Code

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>"
```

**Rollback Plan**: `git revert HEAD` if tests fail

---

## Step 1.2: Audit langhash.c Context Functions

**Goal**: Document which functions already have `_context` variants

**Duration**: 30 minutes

**No code changes** - investigation only

### Tasks:

```bash
# Find existing _context variants in langhash.c
grep -n "_context" Common/source/langhash.c | grep "^[0-9]*:.*("

# Find mode stack usage
grep -n "db_format_mode" Common/source/langhash.c

# List all public hash functions
grep -n "^boolean hash" Common/source/langhash.c
grep -n "^void hash" Common/source/langhash.c
```

**Document findings in checklist below** (Audit Results section)

**No commit** - update this document with findings

---

## Step 1.3: Convert langhash.c - Batch 1 (Core Pack/Unpack)

**Goal**: Convert primary pack/unpack functions to use context exclusively

**Duration**: 2-3 hours

**Target Functions**:
- `hashpacktable()` → ensure `hashpacktable_context()` works correctly
- `hashunpacktable()` → ensure `hashunpacktable_context()` works correctly

**Strategy**:
1. Check if `_context` variants already exist
2. If they exist, update them to use context instead of mode stack
3. If they don't exist, create them
4. Update the non-context versions to call `_context` variants with default context
5. Add deprecation warnings

### Changes:

#### 1.3.1: Update or create hashpacktable_context()

**File**: `Common/source/langhash.c`

**If function already exists**: Remove any `db_format_mode_push/pop` calls, use `ctx->mode` instead

**If function doesn't exist**: Create it based on `hashpacktable()`, add `ctx` parameter

**Pattern**:
```c
// Before (uses mode stack):
boolean hashpacktable_context(...) {
    db_format_mode mode = db_format_mode_current();  // ← Remove this
    if (mode.use_64bit_format) {
        // v7 logic
    } else {
        // v6 logic
    }
}

// After (uses context):
boolean hashpacktable_context(const db_context *ctx, ...) {
    if (!ctx) {
        // Fallback for NULL context
        db_context default_ctx;
        db_context_init(&default_ctx);
        ctx = &default_ctx;
    }

    if (ctx->mode.use_64bit_format) {
        // v7 logic
    } else {
        // v6 logic
    }
}
```

#### 1.3.2: Update hashpacktable() to call _context variant

```c
boolean hashpacktable(hdlhashtable ht, boolean flsave,
                      Handle *hpacked, boolean *flmustsave) {
    db_context ctx;
    db_context_init(&ctx);
    return hashpacktable_context(&ctx, ht, flsave, hpacked, flmustsave);
}
```

#### 1.3.3: Repeat for hashunpacktable_context() and hashunpacktable()

Follow same pattern as above.

### Testing:

```bash
# 1. Compile
make -C tests clean
make -C tests save_migration_tests

# 2. Run migration test 3 times (verify determinism)
for i in {1..3}; do
  rm -f tests/test_save_migration-v7.root
  ./tests/save_migration_tests
  cp tests/test_save_migration-v7.root /tmp/step1.3-run$i.root
done

# 3. Verify deterministic output
md5 /tmp/step1.3-run*.root
# All 3 should be IDENTICAL

# 4. Test loading migrated database
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  --system-root tests/test_save_migration-v7.root \
  -e "defined(system)"

# 5. Run full test suite
./tools/run_headless_tests.sh
```

**Success Criteria**:
- ✅ Compiles without errors
- ✅ Migration produces deterministic output (3 runs → same md5)
- ✅ Migrated database loads successfully
- ✅ All tests pass

**Note**: md5 may differ from baseline - we're fixing bugs, not preserving them!

### Commit:

```bash
git add Common/source/langhash.c
git commit -m "refactor(langhash): Convert core pack/unpack to use explicit context

Updated hashpacktable_context() and hashunpacktable_context() to use
ctx->mode instead of db_format_mode_current(), eliminating dependency
on global mode stack.

Non-context variants now create default context and delegate to _context
versions, maintaining backward compatibility.

Testing: Migration produces deterministic output, all tests pass.

Part of Phase 1: Core Serialization (Step 1.3)
Ref: planning/phase3/MODE_STACK_REFACTOR_PLAN.md

🤖 Generated with Claude Code

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>"
```

**Rollback Plan**: `git revert HEAD`

---

## Step 1.4: Convert langhash.c - Batch 2 (Helper Functions)

**Goal**: Convert hash helper functions that pack/unpack call

**Duration**: 2-3 hours

**Target Functions** (exact list depends on audit results):
- Functions called by `hashpacktable_context()` that use mode stack
- Functions called by `hashunpacktable_context()` that use mode stack

**Strategy**:
1. Identify which helper functions read `db_format_mode_current()`
2. Add `const db_context *ctx` parameter to each
3. Pass `ctx` through call chain
4. Remove mode stack reads

### Example Pattern:

```c
// Before:
static boolean pack_hash_value(tyvaluerecord *val, ...) {
    db_format_mode mode = db_format_mode_current();
    if (mode.use_64bit_format) {
        return pack_value_v7(val, ...);
    } else {
        return pack_value_legacy(val, ...);
    }
}

// After:
static boolean pack_hash_value(const db_context *ctx, tyvaluerecord *val, ...) {
    if (ctx->mode.use_64bit_format) {
        return pack_value_v7(val, ...);
    } else {
        return pack_value_legacy(val, ...);
    }
}

// Update callers:
boolean hashpacktable_context(const db_context *ctx, ...) {
    ...
    pack_hash_value(ctx, &val, ...);  // ← Pass ctx through
    ...
}
```

### Testing:

Same as Step 1.3 - migration must produce identical output.

### Commit:

```bash
git add Common/source/langhash.c
git commit -m "refactor(langhash): Convert hash helper functions to use context

Updated internal helper functions to accept context parameter and use
ctx->mode instead of reading global mode stack.

Functions modified:
- [List specific functions here]

Testing: Migration produces deterministic output, all tests pass.

Part of Phase 1: Core Serialization (Step 1.4)
Ref: planning/phase3/MODE_STACK_REFACTOR_PLAN.md

🤖 Generated with Claude Code

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>"
```

---

## Step 1.5: Remove Mode Stack from langhash.c

**Goal**: Eliminate all `db_format_mode_push/pop` from langhash.c

**Duration**: 1 hour

### Tasks:

```bash
# Verify no mode stack calls remain in core functions
grep -n "db_format_mode_push\|db_format_mode_pop" Common/source/langhash.c

# Each occurrence should be in:
# 1. Non-context wrapper functions (backward compat) - OK to keep for now
# 2. Functions we haven't converted yet - need to convert
# 3. Dead code - can delete
```

**For each remaining mode stack call**:
- If it's in a `_context` function → convert to use ctx parameter
- If it's in a non-context wrapper → leave it (will remove in Phase 5)
- If it's in dead code → delete the code

### Testing:

Same as previous steps - migration must be identical.

### Commit:

```bash
git add Common/source/langhash.c
git commit -m "refactor(langhash): Remove mode stack from context functions

Eliminated all db_format_mode_push/pop calls from _context variant
functions. Mode stack now only used in legacy wrapper functions for
backward compatibility.

Mode stack usage in langhash.c:
- Before: X call sites
- After: Y call sites (all in legacy wrappers)

Testing: Migration produces deterministic output, all tests pass.

Part of Phase 1: Core Serialization (Step 1.5)
Ref: planning/phase3/MODE_STACK_REFACTOR_PLAN.md

🤖 Generated with Claude Code

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>"
```

---

## Step 1.6: Convert tablepack.c - Batch 1 (Core Functions)

**Goal**: Convert table pack/unpack to use context

**Duration**: 2-3 hours

**Target Functions**:
- `tablepacktable()` → create/update `tablepacktable_context()`
- `tableunpacktable()` → create/update `tableunpacktable_context()`

**Strategy**: Same as langhash.c Step 1.3

### Testing:

Same testing protocol - migration must produce deterministic output (3 runs → same md5).

### Commit:

```bash
git add Common/source/tablepack.c
git commit -m "refactor(tablepack): Convert core pack/unpack to use explicit context

Updated tablepacktable_context() and tableunpacktable_context() to use
ctx->mode instead of global mode stack.

Non-context variants delegate to _context versions for backward compatibility.

Testing: Migration produces deterministic output, all tests pass.

Part of Phase 1: Core Serialization (Step 1.6)
Ref: planning/phase3/MODE_STACK_REFACTOR_PLAN.md

🤖 Generated with Claude Code

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>"
```

---

## Step 1.7: Convert tablepack.c - Batch 2 (Helpers)

**Goal**: Convert table helper functions to use context

**Duration**: 2-3 hours

Same pattern as Step 1.4 (langhash helpers)

### Commit:

Similar to Step 1.4 commit message, but for tablepack.c

---

## Step 1.8: Remove Mode Stack from tablepack.c

**Goal**: Eliminate mode stack from context functions

**Duration**: 1 hour

Same pattern as Step 1.5

### Commit:

Similar to Step 1.5 commit message, but for tablepack.c

---

## Step 1.9: Convert langexternal.c (External Value Pack/Unpack)

**Goal**: Convert external value serialization to use context

**Duration**: 2-3 hours

**Target Functions**:
- External value pack/unpack functions (identify during audit)

**Strategy**: Same pattern as previous conversions

### Testing:

Critical - external values are what broke in Issue #123!

```bash
# Migration test
make -C tests save_migration_tests && ./tests/save_migration_tests

# Verify external tables work
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  --system-root tests/test_save_migration-v7.root \
  -e "sizeOf(system.verbs.colors)"

# Should return a number, not an error
```

### Commit:

```bash
git add Common/source/langexternal.c
git commit -m "refactor(langexternal): Convert external value pack/unpack to context

Updated external value serialization to use ctx->mode instead of global
mode stack, ensuring external tables use correct address format during
migration.

This eliminates the root cause of Issue #123 external table access failures.
Output format may change from baseline - this is expected as we fix the
address format bugs.

Testing: Migration produces deterministic output, external tables accessible.

Part of Phase 1: Core Serialization (Step 1.9)
Ref: planning/phase3/MODE_STACK_REFACTOR_PLAN.md

🤖 Generated with Claude Code

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>"
```

---

## Step 1.10: Phase 1 Final Validation

**Goal**: Comprehensive testing before declaring Phase 1 complete

**Duration**: 1 hour

### Validation Checklist:

```bash
# 1. Clean rebuild
make -C tests clean
rm -f tests/test_save_migration-v7.root

# 2. Run migration 5 times, verify deterministic
for i in {1..5}; do
  rm -f tests/test_save_migration-v7.root
  make -C tests save_migration_tests && ./tests/save_migration_tests
  cp tests/test_save_migration-v7.root /tmp/migration-run-$i.root
done

md5 /tmp/migration-run-*.root
# All 5 runs should have IDENTICAL md5 hash (determinism verified)

# Compare to baseline (informational only)
echo "=== Baseline (before Phase 1) ==="
cat /tmp/baseline_migration.md5
echo "=== After Phase 1 ==="
md5 /tmp/migration-run-1.root
echo ""
echo "Note: If md5 differs, that's OK - we fixed bugs!"
echo "What matters: All 5 runs above are identical (deterministic)"

# 3. Test migrated database access
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  --system-root /tmp/migration-run-1.root \
  -e "defined(system) and sizeOf(system.verbs.colors) > 0"

# Should return "true"

# 4. Run full test suite
./tools/run_headless_tests.sh

# 5. Verify mode stack eliminated from target files
grep "db_format_mode_push\|db_format_mode_pop" Common/source/langhash.c
grep "db_format_mode_push\|db_format_mode_pop" Common/source/tablepack.c
grep "db_format_mode_push\|db_format_mode_pop" Common/source/langexternal.c

# Should only appear in non-context wrapper functions (backward compat)

# 6. Performance check (optional but recommended)
time ./tests/save_migration_tests
# Compare to baseline time from Step 1.0
# Should be within 5% of original
```

### Success Criteria:

- [ ] Migration is deterministic (5 runs produce identical md5)
- [ ] Migrated database loads successfully
- [ ] External tables accessible (`sizeOf(system.verbs.colors) > 0`)
- [ ] All tests pass
- [ ] Mode stack removed from `_context` functions
- [ ] Performance within 5% of baseline
- [ ] All commits are clean, atomic, and revertable

**Note**: Output md5 may differ from baseline - that's expected and good (we fixed bugs!)

### Document Phase 1 Completion:

Update `planning/phase3/MODE_STACK_REFACTOR_PLAN.md`:

```markdown
## Current Status (as of YYYY-MM-DD)

- **Phase 1**: ✅ COMPLETE (completed YYYY-MM-DD)
  - Core serialization now uses explicit context
  - langhash.c: X → 0 mode stack calls in context functions
  - tablepack.c: Y → 0 mode stack calls in context functions
  - langexternal.c: Z → 0 mode stack calls in context functions
  - Migration is deterministic
  - All tests pass
- **Phase 2**: Not started
- **Branch**: `refactor/explicit-context-no-mode-stack`
- **Blockers**: None
```

### Final Commit (Optional - Summary):

```bash
git commit --allow-empty -m "milestone: Complete Phase 1 - Core Serialization context refactor

Phase 1 complete: Core serialization functions now use explicit context
passing instead of global mode stack.

Changes:
- Added context initialization API (Step 1.1)
- Converted langhash.c pack/unpack to context (Steps 1.3-1.5)
- Converted tablepack.c pack/unpack to context (Steps 1.6-1.8)
- Converted langexternal.c to context (Step 1.9)

Results:
- Migration is deterministic (5 runs = identical output)
- External tables accessible (Issue #123 root cause eliminated)
- All tests pass
- Performance within baseline

Next: Phase 2 - Database Operations

Ref: planning/phase3/MODE_STACK_REFACTOR_PHASE1_DETAILED.md

🤖 Generated with Claude Code

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>"
```

---

## Audit Results (Fill in during Step 1.0 and 1.2)

### langhash.c

**Existing _context functions**:
- [ ] `hashpacktable_context()` - exists? ___
- [ ] `hashunpacktable_context()` - exists? ___
- [ ] Other: ___

**Mode stack usage**:
- Total `db_format_mode_push` calls: ___
- Total `db_format_mode_pop` calls: ___
- Located at lines: ___

**Public functions to convert**:
- [ ] `hashpacktable()`
- [ ] `hashunpacktable()`
- [ ] Other: ___

### tablepack.c

**Existing _context functions**:
- [ ] `tablepacktable_context()` - exists? ___
- [ ] `tableunpacktable_context()` - exists? ___
- [ ] Other: ___

**Mode stack usage**:
- Total `db_format_mode_push` calls: ___
- Total `db_format_mode_pop` calls: ___
- Located at lines: ___

**Public functions to convert**:
- [ ] `tablepacktable()`
- [ ] `tableunpacktable()`
- [ ] Other: ___

### langexternal.c

**Existing _context functions**:
- [ ] (List any found): ___

**Mode stack usage**:
- Total `db_format_mode_push` calls: ___
- Total `db_format_mode_pop` calls: ___
- Located at lines: ___

**Public functions to convert**:
- [ ] (List all external pack/unpack functions): ___

---

## Emergency Rollback Procedures

### If Step 1.1 fails:
```bash
git revert HEAD
# Rebuild
make -C tests clean && make -C tests save_migration_tests
```

### If any conversion step (1.3-1.9) fails:
```bash
# Option 1: Revert last commit
git revert HEAD
make -C tests clean && ./tools/run_headless_tests.sh

# Option 2: Revert to before Phase 1
git reset --hard origin/develop
git checkout -b refactor/explicit-context-no-mode-stack
# Start over from Step 1.1
```

### If Phase 1 completes but breaks something later:
```bash
# Create rollback branch before proceeding to Phase 2
git branch phase1-complete
git tag phase1-complete-$(date +%Y%m%d)

# If Phase 2 breaks, return here:
git reset --hard phase1-complete
```

---

## Notes

- Each step should take 30min-3hrs max
- Test after EVERY step, not just at phase boundaries
- Commit after every successful step
- If a step takes >3hrs, break it down further
- Migration output MUST be deterministic after each step (same every run)
- Output MAY differ from baseline - we're fixing bugs, expect improvements
- Don't proceed to next step if current step's tests fail
- Keep `/tmp/baseline_migration.root` for the entire phase for comparison

---

## Next Phase Preview

After Phase 1 complete, Phase 2 will convert:
- `Common/source/db.c` (~30 call sites)
- Functions: `dbassign()`, `dbreference()`, `dbcopy()`, `dbpushdatabase()`, etc.
- Same incremental approach: audit → batches → test → commit
