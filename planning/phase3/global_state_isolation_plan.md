# Global State Isolation and Thread-Safe Parameterization

## Status
- State: In Progress - DB/serializer (P0) partially complete; dbverbs/db.c (P0) pending
- Phase: 3 (Global State Isolation)
- Last Updated: 2025-12-22
- Notes: db_context infrastructure in place for pack/unpack and core DB APIs; remaining work on dbverbs/db.c stack and headless adoption pending

## Goals
- Remove or encapsulate global mutable state so components can be used in isolation (per-thread/per-context) without cross-talk.
- Allow tests/headless tools to instantiate only what they need without dragging a full app-global environment.
- Prevent state leakage between threads and pave the way for true multi-threading.

## Scope
- Format/DB mode flags (e.g., `use_64bit_format`, serializer options).
- Outline/script/menu globals (`outlinedata`, menupack/editor globals).
- Language/runtime globals (current database, target tables, error state).
- Handle/allocator singletons only where truly process-wide.

## Inventory & Work Tracks (living list)
- **DB + serializer (P0)**  
  - Globals: `use_64bit_format`, mode stack, `databasedata`/`databasedestination`, avail list shadows.  
  - Ordering: first. Break out `db_context` + explicit `db_format_mode` parameters for pack/unpack and DB read/write (`dbassignhandle/dbrefhandle`).  
  - Status: In progress; `db_context` defined and context-aware wrappers added for pack/unpack + dbassign/ref/copy. Table pack/unpack callers now go through the context shims; remaining DB/serializer call sites still lean on the TLS mode stack. Migrator Save As now runs under explicit source/dest contexts via a destination accessor so adapter mode flips don’t leak; refcon/adrlink test was blocked by global mode flips and is tracked below.  
  - Tasks:  
    - [x] Define `db_context` struct (format mode, avail shadow, DB handle, error/log refcon).  
    - [x] Add context-aware shims for serializer + DB APIs (`hashpack/unpack`, `dbassignhandle`, `dbrefhandle`, `dbcopy`).  
    - [x] Adopt context shims in table pack/unpack callers (modern + legacy forks, tableverbs, tablestructure HASH load, tableexternal externals, langhash external deref).  
    - [x] Convert migrator sanitize/assign/ref paths to use `db_context`; verified with `make -C tests db_format_tests`.  
      - [ ] Convert DB read/write callers to context wrappers instead of direct calls:  
        - [x] Migrator sanitize/assign/ref paths (db_format.c).  
        - [ ] Remaining db_format helpers (adapter utilities, free-block handling); Save As migrator + packers/dbflush/dbstartsaveas now call adapter enable via context wrappers.  
        - [ ] dbverbs/db.c stack/helpers (`dbassign`, `dbcopy`, `dbreference`, `dbrelease`, Save As swap/release stack) to context; new destination accessor + push/pop/release-stack wrappers ready to thread through.  
        - [ ] Headless stubs/tests updated to new helpers (remove `databasedata`/`use_64bit_format` assumptions).  
    - [ ] Provide legacy wrappers using a default context; mark deprecated. *(Wrapper shims exist; adoption pending.)*  
    - [ ] Update headless/tests to use explicit context (incl. refcon/adrlink migration test).  
    - [x] Add a two-context regression (legacy DB + modern pack) to prove no cross-talk.  
    - [x] Remove the TLS `use_64bit_format` shim once all callers are migrated. *(Shim deleted; mode state now tracks via `g_mode_state`.)*
- **Assumptions to preserve during rollout**  
  - Save As swap order (`databasedata`/`databasedestination`, releasestack) must remain intact; adapter-enabled writes must never mutate the legacy source.  
  - Avail list/shadow writes only when writable and not mid Save As; adapter/widening must not touch legacy avail metadata.  
  - DB stack helpers (`dbpushdatabase`/`dbpopdatabase`) currently couple database handle and format mode; maintain that behavior until explicit contexts replace them.  
  - ABI shims (`use_64bit_format`, `db_format_mode_current`, legacy signatures) stay until every caller is on explicit contexts to avoid mid-rollout breakage.
- **Outline/OP + menupack (P0)**  
  - Globals: `outlinedata`, hoist/selection state, menudata/menuwindow/menuwindowinfo/editor callbacks.  
  - Ordering: after DB context exists. Introduce `op_context` (outlinedata/hoists/callbacks) and menu context injected into menupack.  
  - Status: Not started; headless menupack shim still fills globals.  
  - Tasks:  
    - [ ] Define `op_context` (outlinedata, hoists, callbacks) and menu context (menudata/menuwindow/editor hooks).  
    - [ ] Thread contexts through outline pack/unpack and menupack; remove dependence on globals.  
    - [ ] Adjust headless stubs/shims to accept injected contexts.  
    - [ ] Add headless test with two contexts (menu + script refcons) to verify isolation.
- **Lang/runtime surface (P1)**  
  - Globals: current target tables, error state, DB stack hooks used by tests/headless tools.  
  - Ordering: after DB/OP contexts land; keep surface minimal for headless/tests.  
  - Status: Not started.  
  - Tasks:  
    - [ ] Identify minimal `lang_context` used by tests/headless tools (targets/error state/DB hooks).  
    - [ ] Thread through exposed APIs used in tests; keep deeper refactor deferred.  
    - [ ] Add a headless regression ensuring two lang contexts don’t cross-talk.
- **UI/legacy stubs (P2)**  
  - Globals: display/window stubs, editor state not needed in headless mode.  
  - Ordering: last; only as needed for headless menupack/opdisplay.  
  - Status: Not started.  
  - Tasks:  
    - [ ] Inventory remaining UI globals used by headless paths.  
    - [ ] Isolate or stub per-context as needed (defer if not required by headless tests).
- **Debugger/thread stacks, future UI parity (P2)**  
  - Requirement: preserve the historical UserTalk debugger behavior where each managed thread has a temp table stack, breakpoints can halt execution, and Cmd-2-click on a variable opens/highlights that variable in its stack frame.  
  - Ordering: later, but keep in mind while isolating globals so we don’t block this UI/stack introspection model.  
  - Tasks:  
    - [ ] Document how thread stacks are currently tracked (temp tables per thread) and which globals they use.  
    - [ ] Ensure context-aware thread state leaves room for per-thread stack tables and breakpoint callbacks.  
    - [ ] Note UI hooks needed to surface stack tables per thread; avoid design choices that forbid per-thread debugger views.

## Plan
1) **Inventory globals**
   - Enumerate globals by subsystem (DB, serializer, outline/op*, menupack, lang/runtime, UI stubs).
   - Classify: true singleton vs per-db/per-outline/per-thread state.
   - Capture in a checklist table with proposed owners/contexts.
2) **Define context structs**
   - DB: `db_context` holding format mode, avail list shadow, current DB handle, error/log refcon.
   - Serializer: pass `db_format_mode` and endianness through APIs instead of a global.
   - Outline/OP: `op_context` holding `outlinedata`, display/hoist state, callbacks; menupack gets its own context for editor-ish state.
   - Lang/runtime: minimal `lang_context` for symbols/targets/error state (focus on headless/test surfaces first).
3) **Plumb context parameters**
   - Convert public APIs to accept a context pointer (or lightweight mode struct) instead of reading globals.
   - Start with DB/serializer: `hashpacktable`, `hashunpacktable`, `dbassignhandle`, `dbrefhandle` take a `db_context`/`db_format_mode`.
   - Outline pack/unpack: pass `op_context` (or explicit outlinedata) instead of globals.
   - Menupack: inject menu context instead of global menudata/menuwindow/editor state.
4) **Legacy shims**
   - Provide thin wrappers using a process-global default context to keep existing callers compiling; mark deprecated.
   - Shift tests and new code to explicit contexts first.
5) **Thread-safe init/teardown**
   - Add `*_init_context`/`*_free_context` helpers; ensure temp buffers/shadows live in the context.
   - No shared globals for mode stacks; contexts own their stacks.
6) **Refactor critical paths first**
   - DB read/write & serializer mode handling (remove `use_64bit_format` global).
   - Outline pack/unpack (menus/scripts/refcon paths) to stop depending on global outlinedata/menudata.
   - Menupack editor-state globals replaced with an injected headless context for tests.
   - Optional if needed: convert menu/script refcon helpers to carry context instead of assuming global outlinedata.
7) **Testing**
   - Add headless tests instantiating multiple contexts concurrently to assert no cross-talk.
   - Refcon/adrlink migration test uses explicit legacy DB context and separate modern pack context.
8) **Docs & breadcrumbs**
   - Track progress here and note milestones in `_CURRENT_STATUS.md`.
   - Keep TODO/TODO entries pointing here for future work.

## Risks / Mitigations
- Signature churn: mitigate via wrappers and staged rollout (DB/serializer first).
- Hidden globals in legacy code: use `rg`/`nm` to surface; add asserts when globals are touched in context builds.
- Perf: pass context by pointer; structs are small.

## Immediate Next Steps
1) Convert pack/unpack + DB read/write callers to the new `db_context`/`db_format_mode` wrappers; stop relying on the TLS global shim.
2) Build a minimal `op_context` for outlinedata/menudata and thread it through menupack and outline pack/unpack in headless tests.
3) Add a concurrency-style test harness to ensure two contexts can pack/unpack without shared globals.
