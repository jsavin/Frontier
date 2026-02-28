# `databasedata` Global Elimination Roadmap

**Status**: Tier 1+2 in progress
**Last Updated**: 2026-02-27
**Related**: Phase 4 Global State Elimination (this is a focused sub-workstream)

---

## Background

`databasedata` is a global `hdldatabaserecord` that most database functions read for file number, available-block list, and format information. The save/swap/restore pattern used to temporarily redirect it to guest databases causes instability when:
1. Error paths skip the restore step, leaving the global pointing at the wrong database
2. GIL yield points allow other threads to see the swapped state

**Near-term goal**: Eliminate all runtime save/swap/restore patterns so multiple .root files (guest databases) work reliably.

**North-star goal**: Make the runtime fully thread-safe for real pthreads on multi-core machines — eventually remove the GIL.

---

## Completed Work (Phases 1-7)

| Phase | PR | Summary |
|-------|----|---------|
| Phases 1-3 | #448 | Pack/save path — all five `*verbpack_internal` functions use explicit `db_context` |
| Phase 4 | #451 | Unpack/load path — `tableverbinmemory_common` last `dbpushdatabase`/`dbpopdatabase` eliminated |
| Phases 5-6 | #452 | Removed push/pop stack infrastructure; added `_fnum` variants (`dbread_fnum`, `dbwrite_fnum`, `dbgeteof_fnum`, `dbreference_fnum`); converted 4 simple `_context()` wrappers |
| Phase 7 | #453 | Added `_hdb` variants for allocator + header I/O; converted all 5 complex `_context()` wrappers — **no `databasedata` mutation in wrapper layer** |

**After Phase 7**: The `_context()` wrapper layer in `db_format.c` is fully clean. No save/swap/restore remains there.

---

## Remaining Work — Tiered Approach

### Tier 1: Finish db.c Internals (Small, Mechanical)

**Scope**: ~200 lines. Convert leaf functions that still read `databasedata` so upper layers can avoid save/swap/restore.

| Item | File | Function | What's Needed |
|------|------|----------|---------------|
| 1a | db.c | `dbclearshadowavaillist_hdb` | Still swaps `databasedata` for `dbflushheader()` and `dbrelease_internal()`. Create `dbflushheader_hdb` and use existing `dbrelease_hdb`. |
| 1b | db.c | `dbnormalizeaddress` | Reads from `databasedata` via `dbfindblockforaddress()`. Create `dbnormalizeaddress_hdb` that threads `hdb` through to `dbgeteof_fnum`/`dbreadheader_fnum`/`dbreadtrailer_fnum` (all already exist). |
| 1c | db.c | `db_context_fnum` runtime guard | Add `log_error` + early return for production builds (currently only `assert` in debug). |

**Prerequisite for**: Tier 2 (both `tableverbinmemory_common` and `langexternaldisposevalue` need these)

### Tier 2: Eliminate Runtime Save/Swap/Restore in Callers (Medium)

**Scope**: ~300 lines. Convert the remaining call sites that do inline save/swap/restore of `databasedata` during normal runtime.

| Item | File | Function | What's Needed |
|------|------|----------|---------------|
| 2a | tableexternal_common.c | `tableverbinmemory_common` | Inline swap for `dbnormalizeaddress`. Replace with `dbnormalizeaddress_hdb` from Tier 1b. |
| 2b | langexternal.c | `langexternaldisposevalue` / `externaldispose` | Inline swap for variable disposal. Thread `hdb` through disposal chain; may need `dbpushreleasestack_hdb`. |
| 2c | opverbs.c | `opverbinmemory` | Already guards against guest DB mismatch, but could use `dbnormalizeaddress_hdb` for clean path. |

**Hardening tasks** (fold into Tier 1+2 PR):
- Save As explicit test — validate `dbcopy_hdb` fix from Phase 7
- Callee-saves tests for all new `_hdb` functions

### Tier 3: System-Level / Endgame (DEFERRED)

**Rationale for deferral**: These sites are initialization-only or represent the legitimate "current database" concept. Converting them requires threading `hdb` through the entire script evaluation chain — a much larger architectural change with diminishing returns until the GIL is actually being removed.

| Item | File | Function | Why Deferred |
|------|------|----------|--------------|
| 3a | cancoon.c | `setcancoonglobals` / `getcancoonglobals` | This IS the "switch current database" operation — it's the correct place to set the global. Converting requires `hdb` threaded through eval chain. |
| 3b | frontier-cli/main.c | `dbinitialize_headless` + hydration | Init-time only, single-threaded. No runtime risk. |
| 3c | odbengine.c | `dbnotifyopen` / `dbnotifyclose` | Notification callbacks, no concurrent risk under GIL. |
| 3d | — | Remove `databasedata` global entirely | Requires all reads to go through explicit params. This is the north-star goal, blocked on Tier 3a-3c. |

**When to revisit Tier 3**: When beginning GIL removal / real multi-threaded execution. At that point, `databasedata` should become thread-local or be replaced entirely with explicit context threading through the evaluation chain.

---

## State After Tier 1+2 Completion

- **No save/swap/restore anywhere in runtime code paths** (guest DB or system root)
- `databasedata` still exists as "the current database" global, set only at init and window-switch time
- All `_context()` wrappers in `db_format.c` are clean (Phase 7)
- Remaining `_context()` wrappers in `db.c` (`dbrelease_context`, `dballocate_context`, etc.) still use `db_context_guard` internally — safe because the operations they wrap don't contain yield points
- Multiple .root files (guest databases) can be active without save/swap/restore corruption risk
- Path to GIL removal is clear: convert `databasedata` to thread-local, then gradually thread `hdb` through evaluation chain

---

## Key Gotchas Discovered

1. **Save As split**: `dbcopy_hdb` must read from `dbsaveas_source` (the source), not the destination `hdb`. The `_hdb` approach of "use hdb for everything" breaks Save As contract. (Fixed in Phase 7, PR #453)
2. **`fldatabasesaveas` in `dbassign_hdb`**: Must force fresh allocation in destination during Save As. (Fixed in Phase 7)
3. **Shadow avail list** (`#ifdef dbshadow`): Bounds checks must be inside `#ifdef` guards. (Fixed in Phase 7)
4. **GIL safety**: All remaining save/swap/restore sites (Tier 2) are safe under the GIL because they don't contain yield points (`langbackgroundtask()` or `thread.sleepTicks()`). But eliminating them is still important for error-path safety and future threading.

---

## Navigation

- [← Phase 4 Index](INDEX.md)
- [Phase 4 Progress](PROGRESS.md)
- [Phase 4 Overview](00-overview/README.md)
