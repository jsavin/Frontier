# Mode Stack Refactor - Progress Report

**Last Updated**: 2025-12-23
**Branch**: `refactor/explicit-context-no-mode-stack`
**Status**: Active refactoring - significant progress made

---

## Overview

This document tracks progress on eliminating the `db_format_mode_push/pop` anti-pattern that causes mode inheritance bugs during migration. The goal is to replace global mode stack with explicit `db_context` passed through all functions.

## ✅ Completed Work

### 1. Critical Bug Fixes (Migration Blockers)

#### Mode Stack Corruption Fix (commit `77ea2c33`)
**Problem**: `dest_context.mode = source_context.mode` copied v6 mode to v7 destination, and stacked v6 modes overrode the applied v7 mode.

**Fix**:
- Line 1681: Changed to use explicit v7 modern mode
- Line 1835: Added `g_mode_depth = 0` before applying v7 mode to clear stack
- Result: Mode lock correctly blocks v7→v6 downgrades

**Impact**: Migration now writes v5 table headers instead of v4

---

#### External Materialization Recursion Bug (commit `89621aaf`)
**Problem**: `db_format_force_materialize_external_tables_recursive()` was recursing into ALL externals (pictures, outlines, scripts, wptext) as if they were tables, causing segfaults.

**Root Cause**: Code treated `variabledata` as `hdlhashtable` for all external types, but only table externals (id=3) actually store hash tables. Pictures/outlines/etc. are leaf nodes.

**Fix**: Added conditional at line 1565:
```c
if (var_id == idtableprocessor) {
    hdlhashtable child = (hdlhashtable) (**hv).variabledata;
    // recurse into child table
} else {
    log_debug(LOG_COMP_DB, "not recursing - external type %d is a leaf node", var_id);
}
```

**Impact**: Eliminated segfaults on non-table externals during materialization

---

#### LangExternal Pack Mode Logic (commit `b815ec0c`)
**Problem**: `langexternalpack_internal()` only set v7 write mode when LOADING externals from v6 disk. If externals were already in memory from materialization, v7 write mode was never set.

**Root Cause**: Line 854 check was:
```c
if (adapter_repack && !(**hv).flinmemory)
```
This meant v7 write mode was ONLY set when loading from disk.

**Fix**: Restructured logic:
```c
if (adapter_repack) {
    if (!(**hv).flinmemory) {
        // Load from v6 source with legacy read context
    }
    // ALWAYS set v7 write context when adapter_repack is true
    working_context.mode.use_64bit_format = true;
    working_context.mode.adapter_repack = true;
}
```

**Impact**: Eliminates warnings about invalid mode combination for pre-materialized externals

---

### 2. External Type Refactoring (Explicit Context Pattern)

#### Picture Externals (commits `89621aaf`, `b815ec0c`)
**Before**: Used push/pop and global mode
**After**:
- `pictverbinmemory(const db_context *ctx, hdlexternalvariable hv)` - explicit context for reading
- `pictverbpack_internal(const db_context *ctx, ...)` - explicit context for writing
- Removed all `dbpushdatabase/dbpopdatabase` calls
- Updated 6 call sites in `pictverbs.c`

**Files Modified**:
- `Common/source/pictverbs.c` - refactored functions
- `Common/headers/pictverbs.h` - updated declarations
- `Common/source/langexternal.c` - calls `pictverbpack_internal()` with context
- `Common/source/db_format.c` - calls `pictverbinmemory()` during materialization

---

#### Script Externals (commit `3cfaf238`)
**Discovery**: Scripts ARE outlines with `flscript=true` flag set. They share infrastructure.

**Fix**: Added `|| var_id == idscriptprocessor` to outline materialization conditional

**Files Modified**:
- `Common/source/db_format.c` - materialization loop

---

#### WPText Externals (commit `08e147e1`)
**Before**: `wp_portable_state_dbref()` used `dbpushdatabase/dbrefhandle/dbpopdatabase`

**After**:
- Changed signature: `wp_portable_state_dbref(const db_context *ctx, ...)`
- Uses `dbrefhandle_context(ctx, ...)` instead
- Updated 3 call sites to pass NULL context

**Files Modified**:
- `portable/wptext_runtime.c`

---

### 3. Hash Table and Database Operations (commits `a54c5c56`, `e044b489`)

#### langexternal.c Refactoring (commit `a54c5c56`)
**Function**: `langexternalrefdata()`

**Before**:
```c
dbpushdatabase((**hv).hdatabase);
fl = dbrefhandle((dbaddress)(**hv).variabledata, hdata);
dbpopdatabase();
```

**After**:
```c
boolean langexternalrefdata(hdlexternalvariable hv, Handle *hdata) {
    return langexternalrefdata_context(NULL, hv, hdata);
}
```

**Files Modified**: `Common/source/langexternal.c`

---

#### opverbs.c Refactoring (commit `a54c5c56`)
**Function**: `opverbcopyvalue()` - Direct refactor (single usage path)

**Before**:
```c
dbpushdatabase((**hv).hdatabase);
fl = dbrefhandle(adr, &hpackedoutline);
dbpopdatabase();
```

**After**:
```c
fl = dbrefhandle_context(NULL, adr, &hpackedoutline);
```

**Impact**: Removed unnecessary push/pop wrapper around single database read

**Files Modified**: `Common/source/opverbs.c`

---

#### langhash.c Refactoring (commit `a54c5c56`)
**Functions refactored**: 4 patterns eliminated

1. **disposehashnode()** - Simplified (no refactor needed)
   - Analysis: `disposevaluerecord` uses release stack, not db read operations
   - Action: Removed unnecessary push/pop wrapper entirely

2. **hashassign()** - Simplified (no refactor needed)
   - Same as disposehashnode - only calls `disposevaluerecord` which uses release stack

3. **hashresolvevalue()** - Created `_context` variant + wrapper
   - Before: `dbpushdatabase/dbrefhandle/dbpopdatabase`
   - After: `dbrefhandle_context(ctx, ...)`
   - Wrapper: `hashresolvevalue()` calls `hashresolvevalue_context(NULL, ...)`
   - Call sites: 11 total (kept wrapper for backward compatibility)

4. **hashpackscalar()** - Direct refactor (static function)
   - Refactored `flexternalmemorypack` branch to use explicit context
   - Created local `db_context` when needed
   - No wrapper needed (static function, only 3 internal call sites)

**Files Modified**: `Common/source/langhash.c`

---

#### Menu Functions Refactoring (commit `e044b489`)

1. **claycallbacks.c**: Created `claycopyfile_context(ctx_source, ctx_dest, ...)` variant
2. **menueditor.c**: Created `_context` variants for `meloadscriptoutline`, `medisposemenurecord`
3. **menupack.c**: Created `mereleaserefconroutine_context` variant

All follow the established `_internal(const db_context *ctx, ...)` pattern with backward-compatible wrappers.

**Files Modified**:
- `Common/source/claycallbacks.c`
- `Common/source/menueditor.c`
- `Common/source/menupack.c`

---

#### Phase 2 Deferrals (commit `08391fe7`)

**Files marked for Phase 2** (require creating underlying context-aware helpers first):
- **menuverbs.c** (`menuverbinmemory`) - Requires `meloadmenurecord_context`, `meloadoutline_context`
- **dbstats.c** - Requires context-aware variants for multiple db.c functions
- **langxml.c** - Requires `copyvaluerecord_context`
- **langhtml.c** - Requires `copyvaluerecord_context`

Added TODO comments documenting deferral reasons.

**Files Modified**: `Common/source/menuverbs.c`

---

### 4. External Materialization Implementation (commit `77ea2c33`)

**Feature**: Added comprehensive external materialization during migration:
- Tables (`idtableprocessor`) → `tableverbinmemory()`
- Outlines (`idoutlineprocessor`) → `opverbinmemory()`
- Scripts (`idscriptprocessor`) → `opverbinmemory()`
- WPText (`idwordprocessor`) → `wpverbinmemory()`
- Pictures (`idpictprocessor`) → `pictverbinmemory()`

**Purpose**: Load all externals into memory and clear `oldaddress` to prevent v6 address reuse in v7 database.

**Known Issue**: See Issue #136 - loads ALL externals into memory, which could be problematic for large databases (3GB+). Optimization deferred until all push/pop patterns eliminated.

---

## 🔄 Commits Summary

| Commit | Date | Description |
|--------|------|-------------|
| `77ea2c33` | 2025-12-23 | Mode stack corruption fix + external materialization |
| `3cfaf238` | 2025-12-23 | Script external materialization fix |
| `89621aaf` | 2025-12-23 | Picture external refactoring (inmemory + pack_internal) |
| `b815ec0c` | 2025-12-23 | LangExternal pack v7 write mode fix |
| `08e147e1` | 2025-12-23 | WPText dbref push/pop removal |
| `a54c5c56` | 2025-12-23 | Eliminate dbpushdatabase from langexternal, opverbs, langhash + ADR-002 |
| `e044b489` | 2025-12-23 | Eliminate dbpushdatabase from menu/clay functions |
| `08391fe7` | 2025-12-23 | Add TODO for menuverbs.c Phase 2 refactoring |

---

## ⚠️ Known Issues

### Mode Warnings During Migration
**Status**: Cosmetic only - does not block migration

**Symptoms**:
```
[db-WARN] WARNING: db_format_mode_apply use_64bit=0 but adapter_repack=1!
```

**Root Cause**:
- 22 remaining `dbpushdatabase` calls in codebase push modes with `use_64bit=0`
- `db_format_mode_current()` combines stack mode with global `adapter_repack=1` flag
- Creates technically invalid but harmless combination
- Mode lock prevents actual v7→v6 downgrades (see "BLOCKED v7->v6 downgrade" messages)

**Impact**: None - warnings are defensive checks, actual downgrades are blocked

**Resolution**: Will be eliminated as remaining push/pop patterns are refactored

---

### Memory Usage During Migration
**Status**: Tracked in Issue #136

**Problem**: `db_format_force_materialize_external_tables_recursive()` loads ALL externals into memory, not just tables. For a 3GB database, this could exhaust memory.

**Better Approach**:
1. **For table externals**: Load (needed for recursion), clear oldaddress
2. **For non-table externals**: Just clear oldaddress WITHOUT loading

**Resolution**: Deferred until all push/pop patterns eliminated

---

## 📋 Remaining Work

### Critical Path: Eliminate 22 `dbpushdatabase` Calls

Current count: `22 remaining dbpushdatabase calls`

**Locations to refactor** (found via `grep -r "dbpushdatabase"`):
- Menu verb functions
- Additional table operations
- Outline operations not yet refactored
- Database utility functions
- Legacy compatibility functions

**Pattern for refactoring**:
```c
// BEFORE:
dbpushdatabase((**hv).hdatabase);
boolean ok = dbrefhandle(adr, &h);
dbpopdatabase();

// AFTER:
boolean ok = dbrefhandle_context(ctx, adr, &h);
```

**For each refactoring**:
1. Add `const db_context *ctx` parameter to function signature
2. Replace push/pop with `dbrefhandle_context(ctx, ...)`
3. Update call sites to pass context
4. Add declaration to header file if public
5. Test migration still works

---

## 🎯 Success Metrics

### Current Status
- ✅ Migration runs to completion (no segfaults on materialized externals)
- ✅ External tables accessible after migration
- ✅ V5 table headers written (not v4)
- ✅ Mode lock prevents v7→v6 downgrades
- ⚠️ Mode warnings appear (cosmetic - 22 push/pop calls remaining)
- ⚠️ High memory usage during migration (Issue #136)

### Completion Criteria (Phase 1)
- [ ] Zero `dbpushdatabase` calls in refactored code paths
- [ ] Zero mode warnings during migration
- [ ] Deterministic migration (byte-identical on repeated runs)
- [ ] All tests pass
- [ ] Memory-efficient materialization (Issue #136)

---

## 📖 Related Documentation

### Architectural Decision Records
- **`planning/architectural_decision_records/ADR-002-context-based-format-versioning.md`** - Complete rationale for context-based approach vs. alternatives (property-based, improved stack). Includes implementation patterns, common pitfalls, code examples, and success criteria.
- `planning/architectural_decision_records/MODE_SINGLE_DECISION_POINT.md` - Mode management architecture
- `planning/architectural_decision_records/ADR-001-multi-database-context.md` - Original db_context pattern

### Planning Documents
- `planning/phase3/MODE_STACK_REFACTOR_PHASE1_DETAILED_v2.md` - Detailed Phase 1 plan

### Historical Context
- `planning/archive/phase3/mode_stack_refactor/` - Original planning materials
- `docs/mode_stack_refactor_learnings.md` - Lessons learned
- `docs/external_table_variable_management.md` - External table address handling

### Issues
- GitHub Issue #136 - Memory optimization for materialization

---

## 🔬 Testing Strategy

### Before/After Validation
```bash
# Run before any refactoring
./tools/run_headless_tests.sh > /tmp/before_tests.log 2>&1

# Run after each commit
./tools/run_headless_tests.sh > /tmp/after_tests.log 2>&1

# Compare
diff /tmp/before_tests.log /tmp/after_tests.log
```

### Migration Testing
```bash
# Run migration
make -C tests clean && make -C tests save_migration_tests
./tests/save_migration_tests

# Check for warnings
./tests/save_migration_tests 2>&1 | grep -E "WARNING|ERROR|FAIL"

# Verify determinism (run twice, compare)
./tests/save_migration_tests
md5 tests/test_save_migration-v7.root > /tmp/run1.md5
rm tests/test_save_migration-v7.root
./tests/save_migration_tests
md5 tests/test_save_migration-v7.root > /tmp/run2.md5
diff /tmp/run1.md5 /tmp/run2.md5  # Should be identical
```

---

## 📝 Notes for Future Work

### Patterns Established
1. **Context parameter**: Always first parameter, `const db_context *ctx`
2. **NULL context**: Backward compatibility wrappers pass NULL (uses global mode)
3. **_internal suffix**: New context-aware functions use `_internal` suffix
4. **Wrapper functions**: Keep old signatures for compatibility, call `_internal` version

### Anti-Patterns to Avoid
1. **Don't copy source mode to destination**: `dest.mode = source.mode` is wrong during migration
2. **Don't rely on mode stack state**: Explicitly set mode, don't assume it's what you set earlier
3. **Don't materialize everything**: Only load what you need (tables for recursion, leaf nodes on-demand)
4. **Don't push/pop in _internal functions**: Context-aware functions should NEVER manage global state

---

**Next Session**: Continue systematically eliminating the 22 remaining `dbpushdatabase` calls, following the patterns established in picture/wptext refactoring.
