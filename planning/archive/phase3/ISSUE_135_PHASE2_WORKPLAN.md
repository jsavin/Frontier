# Issue #135 Phase 2: API Surface Integration - Detailed Work Plan

**Status**: In Progress
**Scope**: Add `_ctx` variants to all outline mutation functions
**Estimated Effort**: 6-8 hours
**Approach**: Systematic file-by-file refactoring with backward-compat wrappers

---

## Overview

Phase 2 integrates the `op_context_t` structure (from Phase 1) into all outline operation APIs. Every mutation function gets:
1. A `_ctx` variant that takes an `op_context_t *` parameter
2. A version bump at operation start
3. A backward-compat wrapper in the original function name

## Files to Modify (Priority Order)

### Core Mutation Operations (High Priority)

| File | Function | Status | Notes |
|------|----------|--------|-------|
| opexpand.c | `opexpand_ctx()` | pending | Expand/collapse operations |
| opstructure.c | `opinsertstructure_ctx()` | pending | Insert node after/before |
| opstructure.c | `opdeletenode_ctx()` | pending | Delete specific node |
| opstructure.c | `opdelete_ctx()` | pending | Delete current node |
| opstructure.c | `opdeletesubs_ctx()` | pending | Delete children |
| opstructure.c | `oppromote_ctx()` | pending | Promote in hierarchy |
| opstructure.c | `opdemote_ctx()` | pending | Demote in hierarchy |
| opinext.c | `opinsertheadline_ctx()` | pending | Insert with text |
| opverbs.c | `opinserthandle_ctx()` | pending | Insert from handle |

### Attribute Operations (Medium Priority)

| File | Function | Status | Notes |
|------|----------|--------|-------|
| opheadline.c | `opsetlinetext_ctx()` | pending | Change headline text |
| opheadline.c | `opsetrefcon_ctx()` | pending | Set refcon |
| opheadline.c | `opsetdirty_ctx()` | pending | Mark dirty |
| opheadline.c | `opsetmarks_ctx()` | pending | Set/clear marks |

### Display/UI Operations (Lower Priority, Non-Mutating)

| File | Function | Status | Notes |
|------|----------|--------|-------|
| opinext.c | `opgo_ctx()` | pending | Move cursor (no mutation) |
| opinext.c | `opselect_ctx()` | pending | Select range (no mutation) |

## Refactoring Pattern

For each function, apply this pattern:

### Example 1: Simple Mutation (opexpand)

**Before** (opexpand.c, line 184):
```c
boolean opexpand (hdlheadrecord hnode, short level, boolean flmaycreatesubs) {
    register hdloutlinerecord ho = outlinedata;
    // ... operation logic (no version tracking) ...
    opdirtyview();
    return (true);
}
```

**After** (opexpand.c):
```c
/**
 * opexpand_ctx - Expand/collapse outline node to specified level (context-aware)
 *
 * @param ctx - Operation context (required)
 * @param hnode - Node to expand
 * @param level - Expansion level
 * @param flmaycreatesubs - Whether to create subnodes
 * @return true if something expanded, false otherwise
 */
boolean opexpand_ctx (op_context_t *ctx, hdlheadrecord hnode, short level, boolean flmaycreatesubs) {
    assert(ctx != NULL);
    op_context_version_bump(ctx);  // Version bump at start

    register hdloutlinerecord ho = outlinedata;
    // ... existing operation logic unchanged ...
    opdirtyview();
    return (true);
}

/**
 * opexpand - Expand/collapse outline node (backward-compatible wrapper)
 *
 * Wrapper for code that doesn't use operation context yet.
 * Allocates temporary context internally.
 */
boolean opexpand (hdlheadrecord hnode, short level, boolean flmaycreatesubs) {
    op_context_t *ctx = op_context_acquire(OP_CONTEXT_NORMAL);
    boolean result = opexpand_ctx(ctx, hnode, level, flmaycreatesubs);
    op_context_release(ctx);
    return result;
}
```

### Example 2: More Complex Mutation (opdeletenode)

**Pattern is identical**, just with more complex logic inside:
```c
boolean opdeletenode_ctx (op_context_t *ctx, hdlheadrecord hnode) {
    assert(ctx != NULL);
    op_context_version_bump(ctx);

    // ... complex deletion logic ...
    // All existing code remains the same
    // Just add these two lines at the start
}

boolean opdeletenode (hdlheadrecord hnode) {
    op_context_t *ctx = op_context_acquire(OP_CONTEXT_NORMAL);
    boolean result = opdeletenode_ctx(ctx, hnode);
    op_context_release(ctx);
    return result;
}
```

## Implementation Order

### Phase 2A: Core Structure Operations (Day 1, 3-4 hours)

**Files**:
- opstructure.c: Extract/create `_ctx` variants for:
  - `opinsertstructure_ctx()`
  - `opdeletenode_ctx()`
  - `opdeletesubs_ctx()`
  - `opdelete_ctx()`
  - `oppromote_ctx()`
  - `opdemote_ctx()`

**Approach**:
1. Read current function
2. Copy to `_ctx` variant
3. Add `assert(ctx != NULL)` at start
4. Add `op_context_version_bump(ctx)` as second line
5. Create backward-compat wrapper calling `_ctx` variant
6. Update function order (internal first, wrapper after)

**Validation**: Compilation check only (no breaking changes to API)

### Phase 2B: Expansion Operations (Day 1, 1-2 hours)

**Files**:
- opexpand.c: `opexpand_ctx()`, `opfastcollapse_ctx()`, etc.

**Same pattern as 2A**

### Phase 2C: Insert/Text Operations (Day 1-2, 1-2 hours)

**Files**:
- opstructure.c / opinext.c: `opinsertheadline_ctx()`, `opinserthandle_ctx()`
- opheadline.c: `opsetlinetext_ctx()`, `opsetrefcon_ctx()`, etc.

**Same pattern as 2A**

### Phase 2D: Header Updates (Day 2, 30 minutes)

**Files**:
- opentrypoints.h: Add declarations for all new `_ctx` functions
- opverbs.h: Add declarations if verb functions need variants

**Format**:
```c
extern boolean opexpand_ctx (op_context_t *ctx, hdlheadrecord, short, boolean);
extern boolean opdelete_ctx (op_context_t *ctx);
// ... etc for all _ctx variants
```

### Phase 2E: Version Bump Integration Points (Day 2, 1-2 hours)

Ensure version bumps happen at all mutation points:

**Checklist**:
- [ ] opinsertstructure_ctx calls version_bump
- [ ] opdeletenode_ctx calls version_bump
- [ ] opexpand_ctx calls version_bump (expand/collapse is mutating!)
- [ ] opsetlinetext_ctx calls version_bump
- [ ] opsetrefcon_ctx calls version_bump
- [ ] All other mutations call version_bump

**Validation**: Add assertions that version increases after each operation

## Critical Implementation Notes

### 1. Don't Modify Operation Logic

**KEY RULE**: Only add these lines, don't change the operation logic:
```c
assert(ctx != NULL);  // First line
op_context_version_bump(ctx);  // Second line
// ... rest of function unchanged ...
```

This keeps changes surgical and low-risk.

### 2. Backward Compatibility is Essential

Every old API must continue working:
```c
// Old code still works exactly as before
opexpand(hnode, level, TRUE);

// New code can use context
op_context_t *ctx = op_context_acquire(OP_CONTEXT_NORMAL);
opexpand_ctx(ctx, hnode, level, TRUE);
op_context_release(ctx);
```

### 3. Function Ordering in Files

Place `_ctx` variant first, then backward-compat wrapper:
```c
// First: context-aware variant
boolean opexpand_ctx (op_context_t *ctx, ...) { ... }

// Second: backward-compatible wrapper
boolean opexpand (...) { ... }
```

This makes the "real" implementation obvious.

### 4. Version Bumping is Not Conditional

**Always** bump version on mutation, even in error paths:
```c
boolean opinsert_ctx (op_context_t *ctx, ...) {
    assert(ctx != NULL);
    op_context_version_bump(ctx);  // Always bump, even if function returns false

    if (!validate()) {
        return false;  // Still bumped version!
    }

    // ... do work ...
    return true;
}
```

**Why**: Version tracks "operations attempted", not "successful mutations". This provides causality ordering for CRDT.

## Testing Strategy for Phase 2

### Compilation Tests
```bash
make -C tests test 2>&1 | grep -i "error"
# Expected: 0 errors
```

### Link Tests
```bash
# Check all _ctx functions link correctly
nm tests/runtime_tests | grep opexpand_ctx
# Expected: Symbol found
```

### Functional Tests
```bash
./tools/run_headless_tests.sh
# Expected: All tests pass (backward-compat maintained)
```

### Version Tracking Tests
```c
// In unit test (Phase 3):
op_context_t *ctx = op_context_acquire(OP_CONTEXT_NORMAL);
uint64_t v0 = op_context_version_get(ctx);

opexpand_ctx(ctx, hnode, 1, TRUE);
uint64_t v1 = op_context_version_get(ctx);

assert(v1 == v0 + 1);  // Version bumped exactly once
op_context_release(ctx);
```

## File-by-File Checklist

### opstructure.c
- [ ] Create opinsertstructure_ctx
- [ ] Update opinsertstructure wrapper
- [ ] Create opdeletenode_ctx
- [ ] Update opdeletenode wrapper
- [ ] Create opdeletesubs_ctx
- [ ] Update opdeletesubs wrapper
- [ ] Create opdelete_ctx
- [ ] Update opdelete wrapper
- [ ] Create oppromote_ctx
- [ ] Update oppromote wrapper
- [ ] Create opdemote_ctx
- [ ] Update opdemote wrapper

### opexpand.c
- [ ] Create opexpand_ctx
- [ ] Update opexpand wrapper
- [ ] Create opfastcollapse_ctx (if mutating)
- [ ] Check opexpandupdate (if mutating)

### opinext.c / opverbs.c
- [ ] Create opinsertheadline_ctx
- [ ] Update opinsertheadline wrapper
- [ ] Create opinserthandle_ctx
- [ ] Update opinserthandle wrapper

### opheadline.c (if exists)
- [ ] Create opsetlinetext_ctx
- [ ] Update opsetlinetext wrapper
- [ ] Create opsetrefcon_ctx
- [ ] Update opsetrefcon wrapper
- [ ] Other attribute functions

### opentrypoints.h
- [ ] Add extern declarations for all _ctx variants

## Risk Mitigation

### Low Risk: Surgical Changes
- Only adding 2 lines per function
- Backward-compat wrappers maintain old API
- No logic changes to existing operations

### Testing Safety
- Full test suite must pass
- Zero regressions expected
- Can revert single commit if issues

### Rollback Plan
If unexpected issues appear:
```bash
git revert HEAD
git checkout -- Common/source  # Restore to Phase 1A state
./tools/run_headless_tests.sh # Validate rollback
```

## Success Criteria for Phase 2

- [ ] All mutation functions have `_ctx` variants
- [ ] All `_ctx` variants call `op_context_version_bump()` at start
- [ ] All backward-compat wrappers created
- [ ] `./tools/run_headless_tests.sh` passes 100%
- [ ] Zero regressions from Phase 1 baseline
- [ ] No compiler warnings introduced
- [ ] All new `_ctx` functions compile correctly
- [ ] Headers updated with new declarations

## Estimated Timeline

| Phase | Task | Duration | Cumulative |
|-------|------|----------|-----------|
| 2A | Core structure ops | 3-4 hrs | 3-4 hrs |
| 2B | Expand operations | 1-2 hrs | 4-6 hrs |
| 2C | Insert/text ops | 1-2 hrs | 5-8 hrs |
| 2D | Header updates | 30 min | 5.5-8.5 hrs |
| 2E | Validation & testing | 30 min | 6-9 hrs |

**Buffer**: 1 hour for unexpected issues = 7-10 hours total

This is a substantial but straightforward refactoring. The key is discipline (don't change logic) and thoroughness (don't miss any mutation function).
