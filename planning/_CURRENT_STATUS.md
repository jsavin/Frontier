# Carbon Migration – Current Status

**Last Updated**: November 11, 2025  \
**Branches in flight**: `feature/carbon-migration-plan`, `feature/headless-system-bootstrap`

## Recent Updates — November 11, 2025
- Replaced the headless WP stub with a real Paige-backed runtime (`portable/wptext_runtime.c`). External values now carry a `wp_portable_state` with cached header timestamps plus an optional live `pg_ref`, so `wpverbinmemory`, `wpverbgetsize`, and `wpverbgettimes` behave like the legacy desktop build.
- `wp_portable_init()` exposes `pg_globals`/`pgm_globals` through `wp_portable_pg_globals()` and `wp_portable_mem_globals()`. The CLI and runtime tests initialize Paige before evaluating scripts, which lets us unpack legacy WPText trailers, repack them, and run the migrator path without touching QuickDraw.
- `wpverbpack` mirrors Save/Save As semantics: forced repacks (`flconvertingolddatabase`, `fldatabasesaveas`, dirty docs) hydrate Paige, export the current payload, update timestamps/ctsaves, and call `dbassignhandle` before pushing the db address back on the packed handle. This unblocks the forthcoming RTF serializer because the migrator already routes through the new pipeline.
- Updated `planning/carbon_migration/wptext_rtf_tracker.md` with the completed wiring work and added a detailed RTF conversion plan (portable header spec, exporter/importer steps, runtime hooks, and validation strategy) so the next agent can jump straight into the serializer changes.

## Recent Updates — November 10, 2025
- Added a UNIX/headless platform definition to Paige’s core headers plus a portable CMake configuration. The Paige build now runs with Clang via `third_party/cmake-install/bin/cmake`, stalls only on the missing memory-handle traps, and gives us concrete follow-up items instead of SDK errors.
- `third_party/Paige` now builds end-to-end on the headless toolchain: `pgMTraps` has malloc-backed handles, `pgIO` exports POSIX file I/O, and the CMake target skips `PGWIN.C/PGDLL32.C` when not on Windows. The generated `build-headless/libpaige.a` is a universal archive (arm64 + x86_64) so Intel Macs can run the same bits we test on Apple Silicon.
- Headless builds (CLI + tests) link against `libpaige.a`, add `../third_party/Paige/PGHEADER` to the include path, and trigger the Paige build automatically. With the new runtime layer in place, the tests no longer depend on `headless_wp_stubs.c`.
- Documented in the WPText tracker that the immediate blockers were the memory macros in `pgMTraps.h` and the platform stubs in `PGPLATFO/PGWIN.C`, then resolved those blockers by introducing the UNIX platform module and malloc-backed handles.
- Captured the follow-up items (portable header, serializer, migrator tests) so future agents can focus on behavior instead of rediscovering the build steps.

## Recent Updates — November 9, 2025
- Table serializer now emits a v4 header with a 1 KB reserved block for future metadata (timestamped in `Common/source/langhash.c`). The migrator reads legacy v3 headers, zero-fills the reserved region, and legacy payload converters now pad modern headers accordingly, so we have guaranteed space for refcons/window state before touching outline/scripts.
- Runtime tests now include a regression that unpacks the serialized header and asserts the version/reserved bytes are correct, so future changes can’t accidentally strip the new space.
- Headless outline/script packing moved to a portable header (version 4) that strips QuickDraw UI fields and zero-fills a 1 KB reserved block; `oppack`/`opunpack` detect the new layout while still accepting legacy v2/v3 payloads.
- Legacy table converter now reconstructs modern merged handles using the documented `[outer merge][inner header+records][strings][formats]` layout. We retired the heuristic splitter, so every legacy table (including `system.verbs.*`) hydrates without manual trimming.
- `frontier-cli` no longer relies on `langrunstringnoerror`; we feed every inline script through `langrun`, then render results via `hashgetvaluestring`. That fixes “silent” failures for non-string values and gets `defined(system.verbs.globals)` working end-to-end.
- `tests/cli_system_defined` still fails at `clock.now()` because the script resolution path never reaches the real implementation—`langrun` returns false with an empty error. The logs show repeated `langsearchpathlookup` misses (e.g., `script`, `scriptions`), so the remaining work is to confirm that `pathstable` is populated before evaluation and that `langsearchpathvisit` walks those addresses in headless mode.
- Added headless-only logging inside `langsearchpathvisit/langsearchpathlookup` (see `Common/source/langvalue.c`). Running `script.getText(...)` now shows each `path##` entry visit plus the final hit, proving that the search path wiring works even though deeper script helpers still fail.
- WPText → RTF migration tracker lives at `planning/carbon_migration/wptext_rtf_tracker.md` (Paige build, portable header, serializer, tests). Refer to that document for the current checklist and status before starting any work on WP serialization.
- Captured a dedicated Paige portability TODO (`planning/carbon_migration/paige_portability_todo.md`) so the remaining machine-layer/graf/clipboard shims are tracked separately from the RTF migration work. That document should reach ✅ on items 1–7 before we remove `tests/headless_wp_stubs.c` or rely on the real Paige runtime.
- Added the first headless Paige machine layer (`third_party/Paige/PGPLATFO/PGUNX.C`) and wired it into the Paige build so the static library now resolves `pgMachineInit`, `pgClipGrafDevice`, `pgMeasureText`, etc., without depending on QuickDraw/GDI or the linker’s `-undefined dynamic_lookup` escape hatch. The snapshot is now vendored (not a submodule) and pinned to commit `a2fe9b1`.
- `portable/wptext_portable.c` now calls the real Paige bootstrap (`pgMemStartup` / `pgInit`) and exposes `wp_portable_init()` so headless callers can initialize the engine without touching the UI stack.

## Immediate Next Steps
1. **Portable header + documentation**
   - Finalize the `tywpportableheader` layout (`'WPRT'` magic, version, UTF-8 length flag, 1 KB reserved block) and update `planning/carbon_migration/wptext_format.md` with diagrams/field descriptions.
2. **RTF exporter/importer**
   - Implement the exporter path (`pgExportFileFromC` with UTF-8 RTF) and the companion importer (`pgImportFileFromC` + headless caching) inside `portable/wptext_runtime.c`, gating the new format on `flconvertingolddatabase` (and eventually a build flag).
3. **Migrator + tests**
   - Re-run the v6→v7 migrator once the portable serializer exists, regenerate `databases/Frontier-v7.root`, and extend runtime/CLI tests to assert the `WPRT` header plus successful script execution against the migrated root.

Reference: `planning/carbon_migration/wptext_rtf_tracker.md` for the full checklist and supporting subtasks.
