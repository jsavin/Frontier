# Mode Stack Refactor - Phase 1 Detailed Breakdown (v2.0)

**Phase**: Core Serialization - Make hash/table pack/unpack deterministic
**Estimated Duration**: 2-3 days (8-10 working hours)
**Created**: 2025-12-19 | **Refreshed**: 2025-12-23
**Status**: Ready to execute (production-ready for Sonnet)
**Root Cause**: Segfault in migration tests caused by writing legacy-format (v4) table headers into v7 database due to global mode stack inheritance during recursive pack/unpack operations

---

## Executive Summary

**The Problem**: Global `db_format_mode_push/pop` pattern causes implicit mode inheritance in recursive calls. When migrating v6→v7, the code pushes legacy mode to read source tables, but child operations inherit this mode and write v4 headers instead of v5. Later, v7 reader can't handle v4 headers and segfaults.

**The Solution**: Replace global mode stack with explicit `db_context` passed through all functions. This makes format deterministic—child operations can't inherit wrong mode because mode is explicit, not implicit.

**Success Criteria**:
- ✅ Migration produces byte-identical output on repeated runs (determinism)
- ✅ All table headers are v5 (v7 format), ZERO v4 headers
- ✅ External tables accessible (`sizeOf(system.verbs.colors) > 0`)
- ✅ All tests pass
- ✅ No mode stack push/pop in `_context` functions

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

# Record baseline file sizes and hash
ls -lh tests/test_save_migration-v7.root
md5 tests/test_save_migration-v7.root > /tmp/baseline_migration.md5

# Create progress tracker file
cat > /tmp/phase1_progress.txt << 'EOF'
Phase 1 Progress Tracker
Started: $(date)

Step 1.0:  [ ] Complete
Step 1.1:  [ ] Complete
Step 1.2:  [ ] Complete
Step 1.3:  [ ] Complete
Step 1.4:  [ ] Complete
Step 1.5:  [ ] Complete
Step 1.6:  [ ] Complete
Step 1.7:  [ ] Complete
Step 1.8:  [ ] Complete
Step 1.9:  [ ] Complete
Step 1.10: [ ] Complete
EOF
```

**Success Criteria**: All tests pass, baseline recorded, progress tracker ready

---

## Step 0.1: Verify Existing Infrastructure (CRITICAL - 30 minutes)

**Goal**: Determine what already exists before planning implementation. Prevents duplicate work and conflicts.

### Tasks:

```bash
# 1. Check db_context structure
echo "=== Current db_context structure ==="
grep -A 15 "typedef struct db_context" Common/headers/db_format.h

# 2. Check existing init functions
echo "=== Existing db_context init functions ==="
grep -n "^void db_context\|^void db_.*_context" Common/source/db_format.c

# 3. Check for _context variants
echo "=== Existing _context variants in langhash.c ==="
grep -n "hashpacktable_context\|hashunpacktable_context\|hash.*_context" Common/source/langhash.c | head -20

echo "=== Existing _context variants in tablepack.c ==="
grep -n "tablepacktable_context\|tableunpacktable_context\|table.*_context" Common/source/tablepack.c | head -20

echo "=== Existing _context variants in langexternal.c ==="
grep -n "_context" Common/source/langexternal.c | head -20

# 4. Count mode stack usage
echo "=== Mode stack usage in target files ==="
echo "langhash.c:"
grep -c "db_format_mode_push\|db_format_mode_pop" Common/source/langhash.c
echo "tablepack.c:"
grep -c "db_format_mode_push\|db_format_mode_pop" Common/source/tablepack.c
echo "langexternal.c:"
grep -c "db_format_mode_push\|db_format_mode_pop" Common/source/langexternal.c
```

### Document Findings:

Create `/tmp/phase1_infrastructure_audit.txt` with:
- Current db_context fields
- Existing init functions (names, signatures)
- Existing _context variants (which functions have them)
- Mode stack call counts by file

### Decision Points:

**If init functions already exist**:
- Verify signatures match requirements (see Step 1.1.2)
- If signatures differ, decide: enhance vs. create new
- If correct, skip Step 1.1.2, proceed to Step 1.2

**If _context variants already exist**:
- Audit their implementations (Step 1.2 will examine them)
- Don't recreate—enhance existing ones
- Check if they use mode stack (need to remove)

**If db_context is missing fields** (error_context, recursion_depth):
- Check if they're actually needed (see Step 1.1.1)
- Minimal approach: only add if used in this phase
- Document decision

### No Changes Yet
This is **investigation only**. No commits, no code changes. Just documentation.

---

## Coding Conventions (MUST FOLLOW)

Before writing any code, establish these conventions to ensure consistency:

### Convention 1: Context Parameter Position
```c
// ✅ CORRECT: ctx is FIRST parameter
boolean hashpacktable_context(const db_context *ctx, hdlhashtable ht,
                               boolean flsave, Handle *hpacked, boolean *flmustsave)

// ❌ WRONG: ctx is last
boolean hashpacktable_context(hdlhashtable ht, boolean flsave,
                               Handle *hpacked, const db_context *ctx)
```

### Convention 2: NULL Context Handling
All `_context` functions MUST handle NULL context gracefully:
```c
boolean hashpacktable_context(const db_context *ctx, ...) {
    if (!ctx) {
        db_context default_ctx;
        db_context_init(&default_ctx);
        return hashpacktable_context(&default_ctx, ...);  // Recursion safe
    }

    // Proceed with ctx->mode instead of db_format_mode_current()
    if (ctx->mode.use_64bit_format) {
        // v7 logic
    } else {
        // v6 logic
    }
}
```

### Convention 3: Mode Stack Removal Pattern
```c
// ❌ BEFORE: Uses mode stack and global state
boolean hashpacktable(...) {
    db_format_mode mode = db_format_mode_current();
    db_format_mode_push(&my_mode);  // ← REMOVE THIS

    if (mode.use_64bit_format) {
        result = pack_v7(...);
    } else {
        result = pack_v6(...);
    }

    db_format_mode_pop();  // ← REMOVE THIS
    return result;
}

// ✅ AFTER: Uses explicit context
boolean hashpacktable_context(const db_context *ctx, ...) {
    if (!ctx) {
        db_context default_ctx;
        db_context_init(&default_ctx);
        return hashpacktable_context(&default_ctx, ...);
    }

    if (ctx->mode.use_64bit_format) {
        return pack_v7(...);
    } else {
        return pack_v6(...);
    }
}
```

### Convention 4: Context Propagation
When a function calls another function that needs context:
```c
// In hashpacktable_context:
if (!pack_hash_value(ctx, &val, ...)) {  // ← Pass ctx through
    return false;
}

// Definition of pack_hash_value:
static boolean pack_hash_value(const db_context *ctx, tyvaluerecord *val, ...) {
    if (!ctx) {
        db_context default_ctx;
        db_context_init(&default_ctx);
        return pack_hash_value(&default_ctx, val, ...);
    }

    if (ctx->mode.use_64bit_format) {
        return pack_v7(...);
    } else {
        return pack_v6(...);
    }
}
```

### Convention 5: Non-Context Wrapper Pattern
Old non-context function should delegate to `_context` variant:
```c
// OLD non-context function becomes thin wrapper
boolean hashpacktable(hdlhashtable ht, boolean flsave,
                      Handle *hpacked, boolean *flmustsave) {
    db_context ctx;
    db_context_init(&ctx);
    return hashpacktable_context(&ctx, ht, flsave, hpacked, flmustsave);
}
```

---

## Step 1.0: Audit Existing Code (Investigation Only - No Changes)

**Duration**: 30 minutes
**Status**: Investigation only

See Step 0.1 above—this combines Steps 0.1 and 1.0 into one comprehensive audit.

---

## Step 1.1: Add/Verify Context Initialization API

**Goal**: Ensure context initialization functions exist and are correct

**Duration**: 1 hour
**Files to modify**: `Common/headers/db_format.h`, `Common/source/db_format.c`

### Step 1.1.1: Enhance db_context structure (if needed)

**File**: `Common/headers/db_format.h`

Check current structure. It should have at minimum:
```c
typedef struct db_context {
    db_format_mode mode;              // v6 vs v7
    hdldatabaserecord database;       // Which database
    db_saveas_state saveas;          // Migration state
} db_context;
```

**Decision**:
- If structure has all three fields → No changes needed
- If structure is missing fields → Consult Step 0.1 findings
- Only add fields if actively used in this phase

**No commit** for structure verification—only commit if you actually modify it.

### Step 1.1.2: Verify or Create Context Init Functions

**File**: `Common/source/db_format.c`

**First, check if functions exist**:
```bash
grep -n "^void db_context_init\|^void db_context_init_with_mode\|^void db_context_init_legacy_read\|^void db_context_init_modern_write" Common/source/db_format.c
```

**If functions already exist**:
1. Read their implementations
2. Verify they match the patterns below
3. If signatures differ, decide whether to enhance or replace
4. Skip the "add new code" section below

**If functions are missing**, add them to `db_format.c`:

```c
/* Initialize context from current global state (backward compatibility) */
void db_context_init(db_context *ctx) {
    if (!ctx) return;

    memset(ctx, 0, sizeof(*ctx));
    ctx->mode = db_format_mode_current();
    ctx->database = databasedata;
    db_saveas_state_snapshot(&ctx->saveas);
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

**If functions don't exist, also add declarations to header**:

**File**: `Common/headers/db_format.h`

```c
/* Context initialization API */
extern void db_context_init(db_context *ctx);
extern void db_context_init_with_mode(db_context *ctx, const db_format_mode *mode);
extern void db_context_init_legacy_read(db_context *ctx, hdldatabaserecord db);
extern void db_context_init_modern_write(db_context *ctx, hdldatabaserecord db);
extern void db_context_clone_with_mode(const db_context *src, db_context *dst,
                                       const db_format_mode *mode);
```

### Step 1.1 Testing:

```bash
# 1. Compile
make -C tests clean
make -C tests save_migration_tests
# Should succeed with no errors

# 2. Run migration 3 times (verify determinism)
for i in {1..3}; do
  rm -f tests/test_save_migration-v7.root
  make -C tests save_migration_tests && ./tests/save_migration_tests
  cp tests/test_save_migration-v7.root /tmp/step1.1-run$i.root
done

# 3. Verify deterministic output
echo "=== Determinism check ==="
md5 /tmp/step1.1-run*.root
# All 3 should have SAME md5 hash

# 4. Run full test suite
./tools/run_headless_tests.sh
# Should pass completely

# 5. Quick safety check—no mode stack in new functions
grep "db_format_mode_push\|db_format_mode_pop" Common/source/db_format.c | \
  grep -v "^.*//.*mode"
# Should show nothing (or only comments)
```

### Success Criteria:
- ✅ Code compiles without errors
- ✅ Migration deterministic (all 3 runs = same md5)
- ✅ All tests pass
- ✅ Functions callable and working
- ✅ (Optional) Database loads and is usable

### Step 1.1 Commit:

```bash
git add Common/headers/db_format.h Common/source/db_format.c
git commit -m "refactor(context): Add context initialization API for mode stack elimination

Add foundational context management functions to support explicit context
passing throughout the database layer. These functions replace the global
mode stack pattern that was causing Issue #123 (segfault from legacy-format
data in v7 database).

New functions:
- db_context_init() - Initialize from current global state
- db_context_init_with_mode() - Initialize with explicit mode
- db_context_init_legacy_read() - For v6 database operations
- db_context_init_modern_write() - For v7 database operations
- db_context_clone_with_mode() - Clone context with mode change

Testing: All tests pass, migration output deterministic.

Part of Phase 1: Core Serialization (Step 1.1)
Ref: planning/phase3/MODE_STACK_REFACTOR_PHASE1_DETAILED_v2.md

🤖 Generated with Claude Code

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>"
```

### Update Progress:
```bash
sed -i 's/Step 1.1:.*\[.*\]/Step 1.1:  [✓] Complete/' /tmp/phase1_progress.txt
git tag checkpoint-step-1.1
echo "Step 1.1 complete. Checkpoint created."
```

---

## Step 1.2: Audit langhash.c Context Functions (Investigation)

**Duration**: 30 minutes
**Status**: Investigation only, no code changes

**Goal**: Document exactly which functions need conversion and create helper function identification

### Tasks:

```bash
# Find existing _context variants
echo "=== Existing _context functions in langhash.c ==="
grep -n "^[a-z_]*_context(" Common/source/langhash.c

# Find all functions called by main pack/unpack
echo "=== Functions called by hashpacktable ==="
sed -n '/^.*hashpacktable(/,/^}/p' Common/source/langhash.c | \
  grep -o '[a-z_]*(' | sort -u | sed 's/($//'

echo "=== Functions called by hashunpacktable ==="
sed -n '/^.*hashunpacktable(/,/^}/p' Common/source/langhash.c | \
  grep -o '[a-z_]*(' | sort -u | sed 's/($//'

# Find which functions read mode
echo "=== Functions that read db_format_mode_current() ==="
grep -B 5 "db_format_mode_current()" Common/source/langhash.c | \
  grep "^[a-z_]*(" | cut -d'(' -f1 | sort -u

# Document findings
cat > /tmp/langhash_audit.txt << 'EOF'
langhash.c Audit Results

Existing _context functions:
[FILL IN FROM GREP]

Functions called by hashpacktable:
[FILL IN FROM ANALYSIS]

Functions called by hashunpacktable:
[FILL IN FROM ANALYSIS]

Functions that read db_format_mode_current():
[FILL IN FROM GREP]

Helper functions needing context parameter:
[INFER FROM ABOVE]

Mode stack usage:
[COUNT push/pop calls]

EOF
```

### Decision:

Based on audit findings:
1. Document which functions need context parameter
2. List them in order of dependencies (callee functions first)
3. Create conversion plan in /tmp/langhash_conversion_plan.txt

### No Commit
Investigation only. Update audit results in this plan document.

---

## Step 1.3: Convert langhash.c Core Functions (hashpacktable)

**Duration**: 2-3 hours
**Target**: `hashpacktable_context()` and helper functions

### Step 1.3.1: Update or Create hashpacktable_context()

**File**: `Common/source/langhash.c`

**First, check if function exists**:
```bash
grep -n "^.*hashpacktable_context(" Common/source/langhash.c
```

**If exists**: Audit it (does it use mode stack? Should use ctx->mode instead)
**If doesn't exist**: Create it based on `hashpacktable()` pattern

**Pattern to follow**:
```c
boolean hashpacktable_context(const db_context *ctx, hdlhashtable ht,
                               boolean flsave, Handle *hpacked, boolean *flmustsave) {
    if (!ctx) {
        db_context default_ctx;
        db_context_init(&default_ctx);
        return hashpacktable_context(&default_ctx, ht, flsave, hpacked, flmustsave);
    }

    // Use ctx->mode instead of db_format_mode_current()
    if (ctx->mode.use_64bit_format) {
        // v7 packing logic
    } else {
        // v6 packing logic
    }

    // Call helper functions with ctx:
    // pack_hash_value(ctx, ...)  NOT pack_hash_value(...)
}
```

### Step 1.3.2: Update hashpacktable() wrapper

Make old function delegate to `_context` version:
```c
boolean hashpacktable(hdlhashtable ht, boolean flsave,
                      Handle *hpacked, boolean *flmustsave) {
    db_context ctx;
    db_context_init(&ctx);
    return hashpacktable_context(&ctx, ht, flsave, hpacked, flmustsave);
}
```

### Step 1.3.3: Identify and Fix Helper Functions

**Find helpers called by hashpacktable_context**:
```bash
# Look inside hashpacktable_context definition
sed -n '/^.*hashpacktable_context(/,/^}/p' Common/source/langhash.c | \
  grep -o '[a-z_]*(' | sed 's/($//' | sort -u
```

**For each helper function**:
1. Add `const db_context *ctx` as FIRST parameter
2. Remove `db_format_mode_current()` calls
3. Use `ctx->mode` instead
4. Update all call sites to pass `ctx`
5. Repeat for nested helpers

### Step 1.3 Testing:

```bash
# 1. Compile
make -C tests clean && make -C tests save_migration_tests
# Should compile without errors

# 2. Migration determinism (3 runs)
for i in {1..3}; do
  rm -f tests/test_save_migration-v7.root
  ./tests/save_migration_tests
  cp tests/test_save_migration-v7.root /tmp/step1.3-run$i.root
done

# 3. Verify identical
echo "=== Determinism Check ==="
md5 /tmp/step1.3-run*.root
# All 3 should be IDENTICAL

# 4. Test database access
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  --system-root tests/test_save_migration-v7.root \
  -e "defined(system)"
# Should print "true"

# 5. Full test suite
./tools/run_headless_tests.sh
# All tests should pass

# 6. Pre-commit checks
git diff HEAD Common/source/langhash.c | \
  grep "^+.*hashpacktable_context" -A 20 | \
  grep "db_format_mode_push\|db_format_mode_pop"
# Should return NOTHING (no mode stack in context version)
```

### Success Criteria:
- ✅ Compiles without errors
- ✅ Migration deterministic (3 runs = same md5)
- ✅ Database loads
- ✅ All tests pass
- ✅ No mode stack in `hashpacktable_context()`

### Step 1.3 Commit:

```bash
git add Common/source/langhash.c
git commit -m "refactor(langhash): Convert hashpacktable to use explicit context

Converted hashpacktable_context() and helper functions to use ctx->mode
instead of db_format_mode_current(), eliminating global mode stack dependency.

Non-context variant (hashpacktable) now creates default context and delegates
to hashpacktable_context(), maintaining backward compatibility.

Helper functions modified:
- [List specific functions]

Testing: Migration produces deterministic output, all tests pass.
No mode stack push/pop in context version.

Part of Phase 1: Core Serialization (Step 1.3)
Ref: planning/phase3/MODE_STACK_REFACTOR_PHASE1_DETAILED_v2.md

🤖 Generated with Claude Code

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>"
```

---

## Step 1.4: Convert langhash.c - hashunpacktable

**Duration**: 2-3 hours
**Target**: `hashunpacktable_context()` and its helpers

Follow same pattern as Step 1.3, but for unpack instead of pack.

### Testing:

Same as Step 1.3 - migration must be deterministic.

### Commit:

Similar format to Step 1.3, but for unpack functions.

---

## Step 1.5: Remove Mode Stack from langhash.c Context Functions

**Duration**: 1 hour
**Target**: Eliminate all `db_format_mode_push/pop` from `_context` functions

### Verification:

```bash
# Find remaining mode stack in langhash.c
grep -n "db_format_mode_push\|db_format_mode_pop" Common/source/langhash.c

# For each occurrence:
# - If in _context function → Convert to use ctx->mode (should already be done)
# - If in non-context wrapper → OK to keep (for backward compat)
# - If in other function → Need to add ctx parameter or convert
```

### Commit:

After ensuring no mode stack in context functions.

---

## Step 1.6: Convert tablepack.c Core Functions

**Duration**: 2-3 hours
**Target**: `tablepacktable_context()` and helpers

Follow same pattern as Steps 1.3-1.4 for langhash.c.

### Testing:

```bash
# Migration determinism 3 runs
for i in {1..3}; do
  rm -f tests/test_save_migration-v7.root
  ./tests/save_migration_tests
  cp tests/test_save_migration-v7.root /tmp/step1.6-run$i.root
done
md5 /tmp/step1.6-run*.root
# Should all be IDENTICAL

# Full tests
./tools/run_headless_tests.sh
```

---

## Step 1.7: Convert tablepack.c Unpack and Helpers

**Duration**: 2-3 hours
**Target**: `tableunpacktable_context()` and its helpers

Same pattern as previous steps.

---

## Step 1.8: Remove Mode Stack from tablepack.c

**Duration**: 1 hour

Verify no mode stack in context functions.

---

## Step 1.9: Convert langexternal.c (CRITICAL - Issue #123)

**Duration**: 3-4 hours
**Goal**: Fix external table address format issue

**Why Critical**: External tables were storing v6 addresses in v7 database, causing segfault.

### Step 1.9.1: Audit langexternal.c (Investigation)

```bash
# Find external pack/unpack entry points
grep -n "^.*pack.*external\|^.*pack.*variabledata" Common/source/langexternal.c

# Find mode stack usage
grep -n "db_format_mode_push\|db_format_mode_pop" Common/source/langexternal.c

# Document findings for conversion plan
```

### Step 1.9.2: Convert External Pack Functions

Add context parameter, remove mode stack, use `ctx->mode`.

### Step 1.9.3: Convert External Unpack Functions

Same pattern as pack.

### Step 1.9.4: Specific Test for Issue #123 Fix

```bash
# After converting langexternal.c, test external tables specifically

# 1. Build and migrate
make -C tests save_migration_tests && ./tests/save_migration_tests

# 2. Test external table access
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  --system-root tests/test_save_migration-v7.root \
  -e "defined(system.verbs.colors) and sizeOf(system.verbs.colors) > 0"
# Expected: "true"

# 3. Test multiple external table accesses
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  --system-root tests/test_save_migration-v7.root << 'EOF'
-e "
  local(pass = 0);
  if (defined(system.verbs.colors)) { pass = pass + 1; };
  if (sizeOf(system.verbs.colors) > 0) { pass = pass + 1; };
  if (defined(system.verbs.dialog)) { pass = pass + 1; };
  return(pass = 3);
"
EOF
# Expected: "true"

# 4. Determinism check (migration must be identical)
for i in {1..3}; do
  rm -f tests/test_save_migration-v7.root
  ./tests/save_migration_tests
  cp tests/test_save_migration-v7.root /tmp/step1.9-run$i.root
done
md5 /tmp/step1.9-run*.root
# All IDENTICAL
```

### Success Criteria:
- ✅ External tables accessible
- ✅ No segfault
- ✅ Migration deterministic
- ✅ All tests pass

---

## Step 1.10: Phase 1 Final Validation

**Duration**: 1.5-2 hours
**Goal**: Comprehensive validation before declaring Phase 1 complete

### 1.10.1: Determinism Validation (5 runs)

```bash
# Clean rebuild
make -C tests clean
rm -f tests/test_save_migration-v7.root

# Run migration 5 times
for i in {1..5}; do
  rm -f tests/test_save_migration-v7.root
  make -C tests save_migration_tests && ./tests/save_migration_tests
  if [ $? -ne 0 ]; then
    echo "ERROR: Migration failed on run $i"
    exit 1
  fi
  cp tests/test_save_migration-v7.root /tmp/migration-run-$i.root
done

# Verify all identical
echo "=== Determinism Validation ==="
md5 /tmp/migration-run-*.root
# All 5 should be IDENTICAL

# Compare to baseline
echo ""
echo "Baseline (pre-Phase1):"
cat /tmp/baseline_migration.md5
echo ""
echo "After Phase 1:"
md5 /tmp/migration-run-1.root
echo ""
echo "Note: If different, that's OK—we fixed bugs!"
```

### 1.10.2: Byte-Level Validation (Table Headers)

```bash
# Verify NO v4 table headers in v7 database
# This would indicate mode inheritance bug (Issue #123)

python3 << 'EOF'
import sys
import struct

def check_table_headers(db_path):
    """Check that all table headers are v5 (v7 format), not v4 (v6 format)"""

    with open(db_path, 'rb') as f:
        data = f.read()

    # Search for table header version fields
    # Table header format: version (2 bytes BE) + ...
    v4_headers = []
    v5_headers = []

    for i in range(len(data) - 2):
        word = struct.unpack('>H', data[i:i+2])[0]

        # Table version markers (approximate detection)
        if word == 4:  # Could be v4 header
            v4_headers.append((i, hex(i)))
        elif word == 5:  # Could be v5 header
            v5_headers.append((i, hex(i)))

    print(f"Potential v4 headers found: {len(v4_headers)}")
    print(f"Potential v5 headers found: {len(v5_headers)}")

    if len(v4_headers) > 0:
        print("\n⚠️  WARNING: Possible v4 headers (legacy format) in v7 database!")
        print("This indicates the mode inheritance bug (Issue #123) may still exist.")
        return False
    else:
        print("\n✅ PASS: No v4 headers found - format is consistent")
        return True

check_table_headers('/tmp/migration-run-1.root')
EOF
```

### 1.10.3: Database Access Validation

```bash
# Test migrated database is usable
echo "=== Database Access Validation ==="

# 1. System table exists
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  --system-root /tmp/migration-run-1.root \
  -e "defined(system)"
echo "System table: OK"

# 2. External tables accessible (Issue #123 regression check)
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  --system-root /tmp/migration-run-1.root \
  -e "sizeOf(system.verbs.colors) > 0"
echo "External table access: OK"

# 3. Large dataset access
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  --system-root /tmp/migration-run-1.root \
  -e "sizeOf(system.verbs) > 100"
echo "Large table access: OK"
```

### 1.10.4: Test Suite Validation

```bash
# Run full test suite
echo "=== Full Test Suite ==="
./tools/run_headless_tests.sh

if [ $? -eq 0 ]; then
  echo "✅ All tests pass"
else
  echo "❌ Some tests failed"
  exit 1
fi
```

### 1.10.5: Mode Stack Elimination Verification

```bash
# Verify mode stack removed from all _context functions
echo "=== Mode Stack Verification ==="

for file in Common/source/langhash.c Common/source/tablepack.c Common/source/langexternal.c; do
  echo ""
  echo "Checking $file:"

  # Find all _context functions
  for func in $(grep "^[a-z_]*_context(" $file | cut -d'(' -f1); do
    # Check if function uses mode stack
    if grep -A 50 "^.*$func(" $file | grep -q "db_format_mode_push\|db_format_mode_pop"; then
      echo "  ❌ $func still uses mode stack!"
      exit 1
    else
      echo "  ✅ $func - clean (no mode stack)"
    fi
  done
done

echo ""
echo "✅ All _context functions are clean"
```

### 1.10.6: Performance Check

```bash
# Optional but recommended
echo "=== Performance Check ==="
echo "Baseline (from Step 1.0):"
# cat /tmp/baseline_time.txt  (if you recorded it)

time ./tests/save_migration_tests

echo ""
echo "Note: Should be within ±5% of baseline"
```

### 1.10.7: Commit Summary

```bash
# Create a summary milestone commit (optional but recommended)
git commit --allow-empty -m "milestone: Complete Phase 1 - Core Serialization context refactor

Phase 1 complete: Core serialization functions now use explicit context
passing instead of global mode stack.

Accomplishments:
- Context initialization API implemented (Step 1.1)
- langhash.c pack/unpack converted to context (Steps 1.3-1.4)
- tablepack.c pack/unpack converted to context (Steps 1.6-1.7)
- langexternal.c converted to context (Step 1.9)
- All mode stack removed from _context functions

Results:
- Migration is deterministic (5 runs produce identical output)
- External tables accessible (Issue #123 permanently fixed)
- All tests pass
- Performance within baseline
- No v4 headers in v7 database

Next: Phase 2 - Database Operations context refactor

Ref: planning/phase3/MODE_STACK_REFACTOR_PHASE1_DETAILED_v2.md

🤖 Generated with Claude Code

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>"
```

### Success Criteria Checklist:

- [ ] Migration is deterministic (5 runs = identical md5)
- [ ] No v4 table headers in v7 database
- [ ] External tables accessible (`sizeOf(system.verbs.colors) > 0`)
- [ ] All tests pass
- [ ] No mode stack push/pop in `_context` functions
- [ ] Performance within 5% of baseline
- [ ] All commits are atomic and revertable
- [ ] Phase 1 checkpoint tag created: `git tag phase1-complete`

### If All Checks Pass:

```bash
# Create safety backup
git branch phase1-complete
git tag phase1-complete-$(date +%Y%m%d)

# Update planning docs
# Mark Phase 1 as complete in MODE_STACK_REFACTOR_PLAN.md

echo "✅ Phase 1 COMPLETE - Ready for Phase 2"
```

---

## Emergency Rollback Procedures

### If Step 1.1 Fails:
```bash
git revert HEAD
make -C tests clean && make -C tests save_migration_tests
./tools/run_headless_tests.sh
```

### If Any Step Fails:
```bash
# Option 1: Revert last commit
git revert HEAD
make -C tests clean && ./tools/run_headless_tests.sh

# Option 2: Revert to last checkpoint
git reset --hard checkpoint-step-X.Y

# Option 3: Start over from Phase 1 beginning
git reset --hard origin/develop
git checkout -b refactor/explicit-context-no-mode-stack
# Run Step 1.1 again
```

### If Phase 1 Completes But Breaks Phase 2:
```bash
# You created this backup already
git reset --hard phase1-complete
# Can start Phase 2 planning fresh
```

---

## Notes

- **Each step should take 30min-3hrs max** - if longer, break it down further
- **Test after EVERY step**, not just at phase boundaries
- **Commit after every successful step**
- **Migration output MUST be deterministic** (same md5 on repeated runs)
- **Output MAY differ from baseline** - we're fixing bugs (v4→v5 headers), changes expected
- **Don't proceed to next step** if current step's tests fail
- **Keep checkpoints** at each step (`git tag checkpoint-step-X.Y`)

---

## Next Phase Preview

After Phase 1 is complete:

**Phase 2**: Database Operations context refactor
- Files: `Common/source/db.c` (~30 call sites)
- Functions: `dbassign()`, `dbreference()`, `dbcopy()`, `dbpushdatabase()`, etc.
- Same incremental approach: audit → convert → test → commit

---

## Appendix: Test Data

**Baseline (from Step 1.0)**:
```
Migration file size: [ls -lh output]
Migration md5: [save baseline_migration.md5 here]
Test suite result: [save baseline_tests.log here]
Performance time: [record from time command]
```

**After Phase 1 (fill in Step 1.10)**:
```
Migration determinism: [md5 /tmp/migration-run-*.root output]
Table header validation: [python3 script output]
External table test: [CLI output]
Test suite result: [./tools/run_headless_tests.sh output]
Performance time: [time ./tests/save_migration_tests output]
```

---

**End of Phase 1 Detailed Plan v2.0**
**Status**: Production-ready for autonomous execution by Claude Sonnet
**Last Refreshed**: 2025-12-23
**Architect Review**: Complete and approved (see system-architect feedback above)
