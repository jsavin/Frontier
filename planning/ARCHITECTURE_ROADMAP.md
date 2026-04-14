# Architecture Roadmap: Global State Elimination

These 4 issues must be resolved before Frontier can safely run in multi-tenant
or long-running production scenarios. They all share a root cause: global
mutable state that is unsafe for concurrent access.

| Issue | Title | Effort | Status |
|-------|-------|--------|--------|
| #262 | Migrate `currenthashtable` to thread-local storage | Small (3-4h) | Field exists, global not yet removed |
| #296 | Outline context global state (`oppushoutline`/`oppopoutline`) | Small-Medium | ADR-006 implemented, issue open |
| #274 | Eliminate global database state with explicit context | Large (40-80h) | Tiers 1-7 done, Tiers 1b-2 remain |
| #292 | Processor table (`efptable`) lifecycle refactor | Medium (8-16h) | Workaround in place, no structural fix |

## Current State

Frontier's runtime manages execution context through a handful of C globals that
date back to the original single-threaded Mac application (circa 1992). Under
the GIL (ADR-014), these globals are *technically* safe today because only the
GIL holder touches them. But they are fundamentally incompatible with:

1. **Real multi-threading** (GIL removal is on the roadmap)
2. **Concurrent script execution** across yield points (`langbackgroundtask()`)
3. **Multi-user collaborative ODB** (the Phase 6+ north star)

The four remaining globals form two clusters:

- **Execution scope globals**: `currenthashtable` (#262) and `outlinedata` (#296)
  track which hash table / outline a running script is operating on. Nested verb
  calls push/pop these on stacks. If a yield point fires mid-stack, another
  thread sees the wrong scope.

- **Resource lifecycle globals**: `databasedata` (#274) and `efptable` (#292)
  track which database and processor table are "current." Database loading
  overwrites these, and save/swap/restore patterns are fragile on error paths.

### What Has Already Been Migrated

ADR-005 established the thread-local storage pattern. Completed migrations:

- `flscriptrunning`, `flscriptresting`, `currentprocess` -- already in `tythreadglobals`
- `outlinedata` + outline stack -- migrated in PR #261 (ADR-006), 677 call sites across 61 files
- `databasedata` runtime mutation -- Phases 1-7 eliminated all save/swap/restore in the wrapper layer; Tiers 1-2 of leaf functions remain
- `hcurrenthashtable` -- field added to `tythreadglobals` (processinternal.h:139), but the global in langhash.c:770 still exists and is the one actually used at runtime

## Dependencies

```
#262 currenthashtable ──────────────────┐
                                        ├──► #274 database state (uses hashtable context)
#296 outline context (mostly done) ─────┘         │
                                                  │
                                        #292 efptable (blocked on #262)
```

- **#262 has zero prerequisites.** The `tythreadglobals` field already exists; this is wiring + removal of the old global.
- **#296 is nearly complete** (ADR-006 implemented). The issue may just need verification and closure, or minor remaining work.
- **#274 depends on #262** because database verbs chain/unchain hash tables. If `currenthashtable` is still global, threading `db_context` through those paths is fragile.
- **#292 depends on #262** directly (issue text: "blocking on #262 completion"). Processor lookup walks the hash table chain, so `currenthashtable` must be thread-local first.

## Phase 1: Migrate `currenthashtable` to Thread-Local (#262)

**The keystone.** Smallest issue, zero dependencies, unblocks the other two large efforts.

### Problem

`currenthashtable` (langhash.c:770) is a global `hdlhashtable` tracking the
current name-resolution scope. `chainhashtable()` / `unchainhashtable()` push
and pop it. Nested verb calls that modify the chain without proper save/restore
corrupt the parent's scope -- a 1997-era bug that still manifests as assertion
failures in `langevaluate.c:1955`.

### Approach

Follow ADR-005 pattern exactly (proven at scale by ADR-006):

1. The `hcurrenthashtable` field already exists in `tythreadglobals` (processinternal.h:139).
2. Wire up `copythreadglobals()` and `swapinthreadglobals()` in process.c to save/restore it.
3. Wire up `newthreadglobals()` to initialize it to `nil`.
4. Replace the global declaration in langhash.c with a macro accessor: `#define currenthashtable ((**hthreadglobals).hcurrenthashtable)`.
5. Grep for any remaining direct references and update.

### Files Affected

| File | Change |
|------|--------|
| `Common/source/langhash.c` | Remove `hdlhashtable currenthashtable = nil;`, add macro |
| `Common/source/process.c` | Add save/restore/init in thread swap functions |
| `Common/headers/processinternal.h` | Field already present -- no change needed |
| `Common/headers/langhash.h` | Add `#define currenthashtable` macro if not already there |

### Risks

- **Low.** This is the same mechanical pattern used for 677 call sites in ADR-006. The field is already allocated; the remaining work is wiring.
- Potential subtlety: `hashtablestack` (`hdltablestack`) is a companion global. It may need to move at the same time to avoid a split-brain where the stack is global but the current pointer is thread-local.

### Estimated Effort

3-4 hours. Mostly verification (full test suite) rather than code changes.

## Phase 2: Verify/Close Outline Context (#296)

**Likely already done.** ADR-006 is marked IMPLEMENTED (PR #261). This phase is a verification pass.

### Problem

`outlinedata` and `outlinestack` were globals managing the current outline
context for op verbs. Nested `oppushoutline()`/`oppopoutline()` calls created
the same class of corruption risk as `currenthashtable`.

### Approach

1. Verify ADR-006 implementation is complete: `outlinedata` in `tythreadglobals`, accessor functions (`op_get_outlinedata()`, `op_set_outlinedata()`), stack migrated.
2. Confirm no remaining global declarations of `outlinedata` or `outlinestack` outside of backward-compat macros.
3. Run full test suite to confirm.
4. If all checks pass, close #296 with a reference to PR #261 and ADR-006.

### Files Affected

Likely none -- this is a verification phase. If gaps are found:

| File | Potential Change |
|------|-----------------|
| `Common/headers/opinternal.h` | Remove any remaining global `outlinestack` declarations |
| `Common/source/op.c` | Remove any remaining global `outlinedata` declarations |

### Risks

- **Minimal.** If ADR-006 was fully implemented, this is just paperwork.
- Risk: the outline *stack* (`outlinestack[ctoutlinestack]`) may not have been migrated even though `outlinedata` was. Need to verify.

### Estimated Effort

1-2 hours (verification + issue closure).

## Phase 3: Eliminate Global Database State (#274)

**The largest effort.** The `databasedata` global and its companion globals
(`rootvariable`, `roottable`, `currenthashtable`, `hashtablestack`) form the
core of the database context system.

### Problem

Five globals manage all database runtime state:

```c
hdldatabaserecord databasedata = nil;    // db.c — current database handle
Handle rootvariable = nil;               // tablestructure.c — current root table variable
hdlhashtable roottable = nil;            // tablestructure.c — current root hash table
hdlhashtable currenthashtable = nil;     // langhash.c — current scope (Phase 1 target)
hdltablestack hashtablestack = nil;      // langhash.c — scope stack
```

The cancoon record snapshots these when a database is opened, but the snapshot
can become stale. Database verbs restore globals from the cancoon before each
operation -- a fragile save/swap/restore pattern that breaks on error paths and
at GIL yield points.

Phases 1-7 of the `databasedata` elimination roadmap (PRs #448, #451, #452,
#453) already removed all save/swap/restore from the wrapper layer. Remaining
work is in Tiers 1-2 of `planning/phase4/DATABASEDATA_ELIMINATION_ROADMAP.md`.

### Approach

The existing execution plan (`planning/phase3/DB_CONTEXT_REFACTORING_EXECUTION_PLAN.md`, 1850+ lines) lays out four sub-phases. Given the progress already made, the remaining work maps to:

**3A: Finish db.c leaf functions (Tier 1)**
- Create `dbflushheader_hdb()` so `dbclearshadowavaillist_hdb` stops swapping `databasedata`.
- Create `dbnormalizeaddress_hdb()` threading `hdb` through to existing `_fnum` variants.
- Add runtime guard (`log_error` + early return) in `db_context_fnum()`.

**3B: Eliminate runtime save/swap/restore (Tier 2)**
- `tableverbinmemory_common` -- replace inline swap with `dbnormalizeaddress_hdb`.
- `langexternaldisposevalue` / `externaldispose` -- thread `hdb` through disposal chain.
- `opverbinmemory` -- use `dbnormalizeaddress_hdb` for clean path.

**3C: Migrate remaining globals to thread-local or explicit context**
- `rootvariable` and `roottable` -- add to `tythreadglobals` or embed in `odb_runtime_context`.
- `hashtablestack` -- must move alongside `currenthashtable` (Phase 1).
- Cancoon record refactored to embed/reference the runtime context directly.

**3D: Remove globals and deprecated wrappers** (Tier 3 -- deferred until GIL removal begins)
- `setcancoonglobals` / `getcancoonglobals` -- the legitimate "switch current database" operation. Requires `hdb` threaded through the entire eval chain.
- Init-time code in `frontier-cli/main.c` -- single-threaded, low risk.
- Remove `databasedata` global declaration entirely.

### Files Affected

| Sub-phase | Files |
|-----------|-------|
| 3A | `Common/source/db.c` |
| 3B | `Common/source/tableexternal_common.c`, `Common/source/langexternal.c`, `Common/source/opverbs.c` |
| 3C | `Common/headers/processinternal.h`, `Common/source/process.c`, `Common/source/tablestructure.c`, `Common/source/cancoon.c` |
| 3D | `Common/source/db.c`, `Common/source/odbengine.c`, `frontier-cli/main.c` |

### Risks

- **High complexity.** 100+ functions across 6+ core files use these globals. Each change must be verified against the full test suite.
- **Cancoon refactoring** (3C) touches the database open/close lifecycle, which is subtle and has historically been a source of bugs.
- **Tier 3 / 3D deferral is intentional.** Converting `setcancoonglobals` requires threading `hdb` through the entire script evaluation chain -- a much larger change with diminishing returns until GIL removal.

### Estimated Effort

- Tiers 1-2 (3A + 3B): 8-16 hours
- 3C (rootvariable/roottable migration): 8-16 hours
- 3D (full elimination, deferred): 16-40 hours

Total: 40-80 hours as estimated in #274, but front-loaded work (3A+3B) delivers the most safety value.

## Phase 4: Processor Table (`efptable`) Lifecycle (#292)

**Depends on Phase 1 (#262).** The processor table is a global that gets
clobbered during database loading, breaking runtime-registered verb dispatch.

### Problem

`efptable` (tablestructure.c:119) is a global `hdlhashtable` pointing to the
processor table (`system.compiler.kernel`). When a database is loaded from disk,
its serialized processor entries overwrite the runtime-created entries that have
working `valueroutine` function pointers. PR #291 introduced a workaround: a
*second* global (`headless_efptable_original`, tablestructure.c:125) that saves
the pre-load state and is retrieved via `get_headless_efptable()`.

This doubles the global state problem instead of solving it.

### Approach

**4A: Create `processor_context` structure**
- `hdlhashtable processor_table` -- the runtime processor table
- Reference counting for lifecycle management
- Thread ID for debugging concurrent access
- Add as a field in `tythreadglobals` (following ADR-005 pattern)

**4B: Refactor processor lookup**
- Update `tablefindnode()` in langvalue.c to accept an optional `processor_context` parameter.
- Update callers in langtree.c, langops.c, langexternal.c.
- Fall back to thread-local context if parameter is NULL (backward compat).

**4C: Fix database loading**
- Database loading code must not overwrite the runtime processor table.
- Create a separate context for disk-loaded processor entries (tokenvaluetype data).
- Keep runtime context (with live `valueroutine` pointers) separate from serialized data.

**4D: Remove workaround code**
- Delete `get_headless_efptable()`, `save_headless_efptable()`, `headless_efptable_original`.
- Delete global `efptable` declaration from tablestructure.c.
- All processor access goes through explicit context.

### Files Affected

| Sub-phase | Files |
|-----------|-------|
| 4A | `Common/headers/processinternal.h` (or new `processor.h`), `Common/source/process.c` |
| 4B | `Common/source/langvalue.c`, `Common/source/langtree.c`, `Common/source/langexternal.c` |
| 4C | `Common/source/tablestructure.c`, `Common/source/db*.c` |
| 4D | `Common/source/tablestructure.c` (remove globals + workaround functions) |

### Risks

- **Medium.** Processor lookup is on the critical path for every verb dispatch. Performance regression here would be immediately visible.
- The separation of runtime vs. disk-loaded processor entries (4C) requires understanding the database hydration lifecycle. Getting this wrong breaks all verb dispatch.
- `tablefindnode()` is called from many sites; the backward-compat NULL fallback is essential to avoid a big-bang migration.

### Estimated Effort

8-16 hours. The workaround code makes the problem and solution clearly visible; the main effort is threading context through the lookup chain.

## Sequencing Rationale

```
Phase 1 (#262)  ─►  Phase 3 (#274)
  currenthashtable     database state
  [3-4 hours]          [40-80 hours, front-load 3A+3B]
       │
       └──────────►  Phase 4 (#292)
                       efptable lifecycle
                       [8-16 hours]

Phase 2 (#296)  ─►  (independent, likely just verification)
  outline context
  [1-2 hours]
```

1. **Phase 1 first** because it is the smallest change with the most downstream impact. Both #274 and #292 explicitly depend on `currenthashtable` being thread-local. Completing this unblocks all other work.

2. **Phase 2 in parallel with Phase 1** -- it is independent verification of already-implemented work (ADR-006). Can be done by a different person or agent simultaneously.

3. **Phase 3 after Phase 1** because database context threading needs `currenthashtable` and `hashtablestack` to be thread-local before the cancoon refactoring makes sense. The front-loaded Tiers 1-2 (3A+3B) can start as soon as Phase 1 lands.

4. **Phase 4 after Phase 1** because processor lookup walks the hash table chain. It can run in parallel with Phase 3's early tiers (3A+3B) since they touch different files. Phase 4 should complete before Phase 3C (cancoon refactoring) since the cancoon also manages processor table state.

### Critical Path

Phase 1 (4h) --> Phase 3A+3B (16h) --> Phase 3C (16h) --> Phase 3D (deferred)

Total critical-path effort to production safety: ~36 hours of focused work, assuming Phase 2 and Phase 4 run in parallel with Phase 3.

## References

- **ADR-005**: Parameter state thread-safety -- establishes the thread-local migration pattern
- **ADR-006**: Outline push/pop elimination -- largest completed migration (677 sites, PR #261)
- **ADR-014**: GIL cooperative threading -- documents current threading model constraints
- **ADR-001**: Multi-database context -- context threading pattern for database operations
- `planning/phase3/GLOBAL_STATE_AUDIT.md` -- full inventory of global mutable state
- `planning/phase3/DB_CONTEXT_REFACTORING_EXECUTION_PLAN.md` -- detailed 1850-line execution plan for #274
- `planning/phase4/DATABASEDATA_ELIMINATION_ROADMAP.md` -- tier-by-tier progress tracker for `databasedata`
- `planning/phase3/VERB_GLOBAL_STATE_ANALYSIS.md` -- which verb families depend on global state
