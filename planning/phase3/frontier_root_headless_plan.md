# Frontier.root Headless Bring-up & Kernel Export

Status
- State: Proposed
- Phase: 3
- Last Updated: 2025-10-12
- Notes: Tracks the work required to open Frontier.root headless, hydrate the system table, and emit kernelcall glue scripts from C.

Related Docs
- planning/phase3/0.5.23_runtime_test_plan.md
- planning/phase3/system_verbs_bootstrap_plan.md
- planning/phase3/DEVELOPER_QUICKSTART_HEADLESS.md
- planning/headless_stubbed_behavior_matrix.md

Change Log
- 2025-10-12: Draft initial bring-up/export plan.

## Objectives

1. Enable the headless runtime/CLI to open `Frontier.root` using the real database engine.
2. Load the system table programmatically (equivalent of `Frontier.startup` in classic boot).
3. Export the kernelcall glue scripts to deterministic text files so Phase 3 native bindings can consume them.
4. Cover the workflow with automated smoke/regression tests and refreshed documentation.

## Deliverables

- Headless CLI/test build that links required database modules and supports a `--frontier-root` style flag.
- Minimal integration path that opens `Frontier.root`, calls the existing startup verbs, and verifies `system.table` contents.
- Export routine (C) that walks `system.verbs` and writes normalized text artifacts (one file per verb family or similar) to a caller-provided directory.
- Automated checks: CLI smoke test or scripted diff validating the export output; docs updated to describe usage.

## Work Breakdown

1. **Database Module Integration**
   - Pull `db*.c`, `file.c`, and supporting utilities into the headless build list (CLI + tests).
   - Identify/extend headless stubs for any UI-era symbols reached (dialogs, alias resolution, progress UI).
   - Add compile-time guards or no-op implementations where the database layer expects legacy OS services.

2. **Runtime Initialization Enhancements**
   - Add CLI entry points (`--frontier-root`, `--load-system-table`) that call `dbopenfile`, set database globals, and execute the standard startup call chain (`dbstartup`, `langloadsystemtable`, etc.).
   - Ensure headless logging surfaces missing dependency errors clearly.
   - Document required sample database locations (`databases/Guest Databases/...`) and any environment knobs.

3. **System Table Validation & Tests**
   - Create a headless integration test that opens `Frontier.root`, resolves a known entry (e.g. `system.verbs.kernelCall`), and exits cleanly.
   - Add lightweight assertions around verb counts/failure modes to catch regressions.

4. **Kernelcall Export Implementation**
   - Implement a C routine that enumerates `system.verbs`, retrieves script text, normalizes line endings, and writes deterministic files.
   - Expose the routine through the CLI (e.g. `--export-system-verbs <dir>`), returning non-zero on failure.
   - Ensure exports land outside tracked roots by default; allow override for testing.

5. **Documentation & Follow-up**
   - Update `planning/phase3/DEVELOPER_QUICKSTART_HEADLESS.md` and `frontier-cli/README.md` once features land.
   - Note in `system_verbs_bootstrap_plan.md` how the export replaces manual UserTalk dumps.
   - Track remaining dependencies (e.g. kernel binding tests) for subsequent PRs.

## Risks & Mitigations

- **UI Dependencies Surface During DB Boot**: Keep extending headless stubs; fall back to CLI flags that skip UI-coded paths until Phase 2 adapters are in place.
- **Export Drift Without Tests**: Introduce golden files or hash checks so CI flags unexpected diff.
- **Large Frontier.root Footprint**: Use the sanitized sample DB already committed; document storage requirements.

## Exit Criteria

- Headless build opens `Frontier.root`, loads `system.table`, and exports kernelcall glue via CLI in an automated run.
- Tests exercise the workflow and guard against regressions.
- Documentation updated so other developers can run the pipeline without relying on UserTalk tooling.
