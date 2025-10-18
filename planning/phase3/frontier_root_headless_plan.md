# Frontier.root Headless Bring-up & Kernel Glue Integration

Status
- State: Proposed
- Phase: 3
- Last Updated: 2025-10-18
- Notes: Tracks the work required to open Frontier.root headless, hydrate the system table, and compile the kernelcall glue scripts into the runtime.

Related Docs
- planning/phase3/0.5.23_runtime_test_plan.md
- planning/phase3/system_verbs_bootstrap_plan.md
- planning/phase3/DEVELOPER_QUICKSTART_HEADLESS.md
- planning/headless_stubbed_behavior_matrix.md

 Change Log
- 2025-10-12: Draft initial bring-up/export plan.
- 2025-10-12: Tracked initial CLI plumbing for `--system-root` flag (read-only load path).
- 2025-10-13: Added headless fallback that hydrates missing `system.misc`/`system.menus` tables in-memory so sanitized Frontier.root loads with warnings.
- 2025-10-13: Noted follow-up to modernize the database save path so hydrated roots can be persisted as true v7 files.
- 2025-10-16: Landed serializer refactor (fixed-width disk addresses, 32-bit sentinel) and added runtime round-trip tests for legacy/64-bit tables.
- 2025-10-18: Reframed kernelcall work to focus on compiling glue scripts with the runtime instead of exporting text artifacts.

## Objectives

1. Enable the headless runtime/CLI to open the sanitized-but-complete `Frontier.root` (the one we ship with the app) using the real database engine. The root must retain `system.verbs`, `system.agents`, and other bootstrap tables so scripts work exactly as they do in the desktop build.
2. Load the system table programmatically (equivalent of `Frontier.startup` in classic boot).
3. Compile the kernelcall glue scripts into the headless/runtime build so UserTalk can invoke kernel verbs without the export shim.
4. Cover the workflow with automated smoke/regression tests and refreshed documentation.

## Deliverables

- Headless CLI/test build that links required database modules and supports a `--system-root` flag.
- Minimal integration path that opens `Frontier.root`, calls the existing startup verbs, and verifies `system.table` contents.
- Build automation that compiles kernelcall glue scripts alongside the runtime so they can be invoked directly from UserTalk.
- Automated checks: CLI smoke test validating system root hydration + kernelcall invocation; docs updated to describe usage.

## Work Breakdown

1. **Database Module Integration**
   - Pull `db*.c`, `file.c`, and supporting utilities into the headless build list (CLI + tests).
   - Identify/extend headless stubs for any UI-era symbols reached (dialogs, alias resolution, progress UI).
   - Add compile-time guards or no-op implementations where the database layer expects legacy OS services.

2. **Runtime Initialization Enhancements**
   - Add CLI entry points (`--system-root`, `--load-system-table`) that call `dbopenfile`, set database globals, and execute the standard startup call chain (e.g., `dbopenfile`, `dbgetview`, `tableloadsystemtable`, `settablestructureglobals`).
   - Keep the sanitized `Frontier.root` authoritative—avoid code-generated `system.verbs` stubs except as a last resort. When older or trimmed roots are encountered, use a fallback that logs warnings and hydrates missing optional tables (`system.misc`, `system.menus`, `system.macintosh.objectmodel`, `system.paths`) so scripting remains usable.
   - Ensure headless logging surfaces missing dependency errors clearly.
   - Document required sample database locations (`databases/Guest Databases/...`) and any environment knobs.
   - Document how the classic application hydrates `system.root` before any UI comes online so the headless implementation can match that behavior (see Legacy Bootstrapping below).

3. **System Table Validation & Tests**
   - Create a headless integration test that opens `Frontier.root`, resolves a known entry (e.g. `system.verbs.kernelCall`), and exits cleanly.
   - Add lightweight assertions around verb counts/failure modes to catch regressions.

4. **Kernelcall Glue Integration**
   - Rely on the glue scripts stored in `system.verbs` inside the shipped database. The headless runtime should simply load the root and run them through `kernel()` just like the legacy app.
   - Drop the plan to compile glue into the binary unless we hit a hard blocker; document that a “real” root is the contract.

5. **Documentation & Follow-up**
   - Update `planning/phase3/DEVELOPER_QUICKSTART_HEADLESS.md` and `frontier-cli/README.md` once features land.
   - Note in `system_verbs_bootstrap_plan.md` how the export replaces manual UserTalk dumps.
   - Track remaining dependencies (e.g. kernel binding tests) for subsequent PRs.
   - Build or modernize the database save/migration path so hydrated Frontier.root instances can be written back to disk as v7+ without relying on legacy GUI save code.

## Risks & Mitigations

- **UI Dependencies Surface During DB Boot**: Keep extending headless stubs; fall back to CLI flags that skip UI-coded paths until Phase 2 adapters are in place.
- **Compiled Glue Drift**: Add smoke tests that call representative kernel verbs so regressions are caught even without exported artifacts.
- **Large Frontier.root Footprint**: Use the sanitized sample DB already committed; document storage requirements.

## Exit Criteria

- Headless build opens `Frontier.root`, loads `system.table`, and makes compiled kernelcall glue available to UserTalk in an automated run.
- Tests exercise the workflow and guard against regressions.
- Documentation updated so other developers can run the pipeline without relying on UserTalk tooling or manual exports.

## Legacy Bootstrapping (Reference)

The classic application loads `system.root` and primes the script runtime without relying on UI layers. Key steps (all in `Common/source`):

1. **Database open + root table load**
   - `ccloadfile()` → `ccloadsystemtable()` → `tableloadsystemtable()` (`tablestructure.c:386`) reads the root symbol table from disk and returns the table variable + hashtable.

2. **Wire globals for the runtime**
   - `settablestructureglobals()` (`tablestructure.c:657`) clears and reinitializes global pointers (`roottable`, `systemtable`, etc.) and calls `checktablestructure()` to locate/create core children:
     - `system.verbs`, `system.verbs.builtins`, `system.verbs.globals`
     - `system.agents`, `system.paths`, `system.misc`, optional macintosh tables, etc.

3. **Link runtime-owned tables**
   - `linksystemtablestructure()` (`tablestructure.c:233`) links in tables created at runtime (`internaltable`, `environmenttable`, `charsetstable`, temp table). These are not persisted in the DB.

4. **Install language resources**
   - `langinitverbs()` (`langstartup.c:969`) builds the language tables:
     - `langtable.constants` via `langinitconsttable()`
     - `langtable.builtins` via `langinitbuiltintable()` (no-op in headless)
     - `langtable.keywords` via `langinitkeywordtable()`

5. **Load system scripts**
   - `loadsystemscripts()` (`scripts.c:675`) runs `system.startup` and loads agents, compiling any handlers found in `system.agents`.

6. **Fallback for missing tables**
   - Sanitized roots may lack optional tables. Headless `load_system_root_database()` mirrors the app’s behavior by calling `ensure_named_subtable()` to create `system.paths`, `system.misc`, `system.macintosh.objectmodel`, etc., then re-running `checktablestructure()`.

7. **Glue script location**
   - Kernelcall glue such as `system.verbs.builtins.file.open` resides inside `system.verbs`. No C compilation happens; once the tables load, scripts are interpreted via `kernel()`.

### Implications for Headless

- The sanitized root we ship today lacks `system.verbs`, so headless falls back to the EFP shim (`langexternalgettable` HEADLESS block) and dotted calls bypass `system.verbs`.
- To match the legacy bootstrap, we must either:
  - Ship a sanitized root that still includes the `system.verbs` hierarchy; or
  - Generate wrapper tables programmatically at startup (see `planning/phase3/system_verbs_bootstrap_plan.md`).
- Tests that rely on `system.verbs.*` should be deferred until those tables exist in the headless runtime (either via DB or codegen).
