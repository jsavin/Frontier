# Frontier Progress Report: databasedata Elimination & Startup Stabilization

**Date:** February 27, 2026
**Status:** Active Development
**Milestone:** Zero Runtime databasedata Mutation, Startup Bootstrap Stabilization
**Period Covered:** February 16-27, 2026 (11 days)

---

## Executive Summary

This 11-day period delivered **two major outcomes**. First, the **databasedata global elimination** project (Phases 1-10) reached its milestone: zero runtime save/swap/restore of the `databasedata` global in any pack, unpack, save, or load code path. All database I/O wrapper functions now use explicit handle threading, eliminating a class of thread-safety bugs and making the ODB layer ready for concurrent access. Second, **startup stabilization** fixed 11 bugs in the guest database and bootstrap paths, bringing the first-run experience significantly closer to working end-to-end.

---

## Major Accomplishments

### TIER 1: databasedata Global Elimination -- Phases 1-10

**The Headline:** All runtime save/swap/restore of the `databasedata` global eliminated from pack/unpack/save/load paths across 10 phases and 12 PRs.

**What Was Built (by phase):**

- **Pre-work (PR #447):** Save/restore databasedata during recursive packing -- identified the problem and temporary fix
- **Phases 1-3 (PR #448):** Explicit `db_context` for all five `*verbpack_internal` functions on the pack/save path
- **Phase 4 (PR #451):** Eliminated last `dbpushdatabase`/`dbpopdatabase` call in `tableverbinmemory_common` -- the unpack/load path
- **Phases 5-6 (PR #452):** Removed dead DB push/pop stack infrastructure entirely. Added `_fnum` variants (`dbread_fnum`, `dbwrite_fnum`, `dbgeteof_fnum`, `dbreference_fnum`) for thread-safe I/O without global state
- **Phase 7 (PR #453):** Explicit DB handle threading for complex `_context()` wrappers. Added Layer 2 (`dbreadheader_fnum`, `dbwriteheader_fnum`, `dbwritetrailer_fnum`, `dbseteof_fnum`), Layer 3 (`dballocate_hdb`, `dbrelease_hdb` + ~15 `_hdb` helpers), Layer 4 (`dbassign_hdb`, `dbcopy_hdb`, `dbsavehandle_hdb`, `dbrefhandle_hdb`, `dbassignhandle_hdb`). Converted all 5 complex `_context()` wrappers -- no databasedata mutation in wrapper layer.
- **Phase 8 (PR #454):** Converted 3 remaining runtime swap sites (`dbclearshadowavaillist_hdb`, `langexternaldisposevariable`, `tableverbinmemory_common`). New functions: `dbflushheader_hdb`, `dbfindblockforaddress_hdb`, `dbnormalizeaddress_hdb`, `dbpushreleasestack_hdb`.
- **Phase 9 (PR #459):** Converted `tablesortedinversesearch` -- the last runtime databasedata swap. Used refcon wrapper pattern (`tablesortedsearchctx`). Converted 4 visit callbacks to explicit `db_context`.
- **PR #460:** Thread explicit hdb through unpack chain for guest DB externals
- **Phase 10 (PR #461):** Extracted shared `dbflushheader_core()` (80+ line dedup), moved `db_context_fnum` to .c with unconditional `log_error`, named `DB_BACKWARD_SCAN_MAX_BYTES` constant, documented FRONTIER_HEADLESS divergence. Closes #455, #456, #457, #458.

**Key Technical Details:**
- `_fnum` variants take an explicit file number parameter instead of reading from `databasedata`
- `_hdb` variants take an explicit database handle instead of reading from `databasedata`
- `db_context` struct bundles database handle + format mode for pack/unpack operations
- Refcon wrapper pattern used for callback-based APIs (sort, visit) that need to pass context through void* refcon

**Why This Matters:**
- **Thread safety** -- No database I/O wrapper mutates global state, enabling future concurrent access
- **Correctness** -- Guest database operations can no longer accidentally read/write the wrong file
- **Maintainability** -- Explicit parameters make data flow visible; no more implicit global coupling
- **Foundation for removal** -- The `databasedata` global can eventually be removed entirely once ODB engine API is converted to explicit handle passing

**What Remains:**
- ODB engine (`odbengine.c`) still sets `databasedata` on every `odbOpenFile`/`odbGetValue` call (live code for guest databases)
- `odb_context_guard` legitimately saves/restores for guest DB context switching
- Bootstrap/shutdown sets once at startup
- Full removal requires converting ODB API to explicit handle passing (larger architectural change)

---

### TIER 1: Startup Stabilization (PRs #434-#444, #446)

**The Headline:** 11 bugs fixed in guest database operations and startup bootstrap, bringing the first-run experience significantly closer to working.

**What Was Fixed:**

- **PR #434:** Guest database script execution -- address normalization was scanning the wrong file, causing scripts in guest DBs to fail
- **PR #435:** WP text extraction for guest database values -- was reading from system root instead of the guest DB
- **PR #436:** Database context when packing guest database externals -- was using system root format info instead of guest DB's
- **PR #438:** Database format when packing cross-database externals -- format mismatch caused silent data corruption
- **PR #439:** Startup stabilization -- heap corruption from out-of-bounds write, file operation failures, Pascal string logging garbage (PSTR display)
- **PR #440:** Stabilize UserTalk startup bootstrap -- fixed script errors that prevented startup completion
- **PR #441:** TCP callback AST -- was building malformed function call trees, causing crashes when callbacks fired
- **PR #442:** TCP callbacks in headless mode -- execute directly instead of routing through UI event loop
- **PR #443:** GIL yield in REPL event loop -- REPL was holding GIL continuously, starving background threads; fixed headless thread identity stubs
- **PR #444:** Broken Pascal string prefixes in ALL headless verb registrations -- systematic fix for wrong length bytes
- **PR #446:** PSTRING macro with compile-time length validation -- prevents the class of bug fixed in #444 from recurring

**Why This Matters:**
- Guest database operations (open, read, pack, save) now use correct database context throughout
- TCP callbacks work in headless mode -- unblocks web server functionality
- Pascal string encoding bugs eliminated at compile time via PSTRING macro
- Startup bootstrap completes more reliably

---

### TIER 2: Test Improvements

- **PR #450:** Unskip != and ! operator tests (closes #197)
- Integration test count increased from 1,881 to 1,893
- Unit test count stable at 302
- Zero failures maintained throughout all changes

---

## Quality Metrics

### Code Changes (Feb 16-27)

| Metric | Value |
|--------|-------|
| **Pull Requests Merged** | 27 |
| **Issues Closed** | #197, #455, #456, #457, #458 |

### Integration Tests

| Metric | Start (Feb 16) | End (Feb 27) |
|--------|----------------|--------------|
| Total Tests | 1,881 | 1,893 |
| Passed | 1,692 | 1,704 |
| Skipped | 189 | 189 |
| Failed | 0 | 0 |
| Unit Tests | 302 | 302 |

### Key Technical Achievements

| Achievement | Detail |
|-------------|--------|
| **Zero databasedata mutation** | All pack/unpack/save/load paths use explicit handles |
| **~40 new _fnum/_hdb functions** | Thread-safe database I/O without global state |
| **Dead code removal** | dbpush/dbpop stack, ~200 lines removed |
| **PSTRING macro** | Compile-time Pascal string validation |
| **11 startup bugs fixed** | Guest DB context, heap corruption, TCP callbacks |
| **dbflushheader dedup** | 80+ lines of duplication eliminated |

---

## Strategic Impact

### What This Period Accomplished

The databasedata elimination is the most architecturally significant change since GIL threading (PR #410). By removing all global state mutation from the database I/O wrapper layer, the ODB becomes safe for concurrent access in the wrapper functions. Combined with the startup stabilization work, the system is now ready for end-to-end first-run testing.

### What's Ready to Start

1. **Startup diagnostics** -- Run from dist build, capture failures, fix iteratively
2. **mainResponder & Manila installation** -- The first-run flow that installs these guest databases
3. **Guest DB save verification** -- Confirm the databasedata elimination hasn't broken any save paths

### What Still Blocks Launch

- **Issue #86**: Runtime context architecture
- **Issue #88**: HTTP-level security model
- **Phase 4 P0a**: Global state elimination (depends on #86)

---

## Lessons Learned

### What Worked Well

**Phased approach to databasedata elimination:** Breaking the work into 10 phases with clear boundaries made each PR small, reviewable, and testable. Each phase had a single responsibility (one call site or one layer of the stack), making regressions easy to catch.

**PSTRING macro as systemic fix:** PR #444 found broken Pascal string prefixes across ALL headless verb registrations. Rather than just fixing the instances, PR #446 introduced a compile-time macro that prevents the bug class entirely. This is the right response to a systemic bug.

### Challenges Overcome

**Cross-database externals:** PRs #436 and #438 uncovered a subtle bug where packing guest database externals used the system root's format information instead of the guest database's. This only manifested when databases had different format versions -- a scenario that became more common as v7 migration progressed.

**dbcopy during Save As:** Phase 7 required special handling for `dbcopy_hdb` during Save As operations -- the source must be read from the `dbsaveas_source` handle, not the destination. This was a non-obvious requirement that caused test failures until understood.

---

## Next Steps

### Immediate (This Week)

1. **Startup diagnostics** -- Run from dist build, identify remaining blockers
2. **Fix blockers for mainResponder/Manila installation**
3. **Verify guest DB save round-trip end-to-end**

### Mid-Term (2-4 Weeks)

1. **Complete startup flow** -- HTTP server starts, setupFrontier page opens
2. **Begin GUI protocol layer implementation**
3. **Verify long-running HTTP process stability**

### Long-Term (1-2 Months)

1. **GUI Alpha Release**
2. **Issue #86 Resolution**
3. **Phase 4 P0a** -- Global state elimination

---

## Recognition

**Co-Authored-By:** Claude Opus 4.6 <noreply@anthropic.com>

---

**Period Status:** COMPLETE -- databasedata elimination achieved (Phases 1-10), startup bootstrap stabilized, zero test failures maintained
