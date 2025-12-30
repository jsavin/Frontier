# Carbon Migration Tracer Bullets

Status
- State: In Progress
- Phase: Carbon Migration
- Last Updated: 2025-11-20
- Owner: Codex
- Notes: Lightweight milestones for each branch; update as bullets land.

Tracer bullets (lightweight, verifiable milestones) for each branch in the execution sequence captured in [`_CURRENT_STATUS.md`](../_CURRENT_STATUS.md). Hitting these checkpoints keeps the work incremental and reviewable.

---

## 1. Memory Layer Cleanup
1. **Shim parity test:** Extend `portable/classic_handle.c` so `MemError`/`MaxBlock` queries return deterministic values; add a lightweight unit test under `tests/portable_handles_tests.c` that exercises growth/shrink paths.
2. **Headless build smoke:** Recompile `make -C tests test_migration` with classic Mac Memory Manager headers removed from the include path; success equals zero Carbon warnings/errors from `memory.c`.
3. **CLI regression:** Run `frontier-cli/frontier-cli --system-root databases/Frontier-v7.root -e "defined(clock.now)"` to confirm the loader still works after handle refactor.

   **Exit criteria:** Portable handle layer exposes `classic_mem_error()/classic_max_block()` equivalents, unit test passes, headless build completes without Carbon includes, and CLI regression command succeeds.

## 2. Strings / Resource Fork Replacement
1. **Prototype table:** Create a generated C array (or JSON/INI input) for the `langerrorlist` strings and switch `langerror.c` to use it; add sanity assertions in unit tests.
2. **STR# purge tracer:** Remove `resources.c:getstringlist` from the headless build and ensure the error reporting path in `langregexp.c` still returns meaningful text.
3. **Serialization handshake:** Document the new format in `docs/` and add a script (e.g., `scripts/export_strings.py`) that regenerates the tables from source files.

   **Exit criteria:** Headless build no longer links any STR#/Resource Manager code, automated tests validate the new string tables, documentation and regeneration script exist and are referenced in `_CURRENT_STATUS.md`.

## 3. Filespec / Alias Abstraction
1. **API draft:** Introduce a `tyfilepath` struct and helper prototypes in a new header; refactor `langhash.c` comparison logic to compile with the new abstraction (behind a feature flag).
2. **Round-trip test:** Implement portable pack/unpack helpers and add regression coverage in `tests/runtime_tests.c` that writes/reads a file value without invoking Alias Manager.
3. **Legacy shim retirement:** Delete the stub implementations from `portable/runtime_stubs*.c` and confirm both headless CLI and Windows builds link without `filespectoalias`.

   **Exit criteria:** All shared modules consume the new `tyfilepath` API, runtime tests prove alias-free round-trips, headless/Windows builds succeed without alias stubs, and `langsystem7.c` alias helpers are desktop-only.

## 4. QuickDraw Extraction
1. **String width heuristic:** Replace `stringpixels` usage in `strings.c` with a multi-byte safe character count; add unit tests covering truncation with wide characters.
2. **Desktop isolates:** Move `quickdraw.c` into a desktop-only target and confirm headless builds no longer compile it (link-time removal check).
3. **UI sanity check:** Launch the desktop build (macOS) and visually verify at least one table window still renders titles correctly after the split.

   **Exit criteria:** `strings.c` compiles without QuickDraw headers, headless builds exclude `quickdraw.c`, unit tests cover new heuristics, and desktop smoke test confirms visual parity.

## 5. AppleEvent Isolation
1. **Headless dispatcher:** Introduce a headless-only command path (inspired by `langwinipc.c`) and prove it by running `frontier-cli --system-root ... -e "system.startup.startupScript()"` without touching AppleEvents.
2. **Compilation gate:** Ensure `langipc.c` no longer includes `<AppleEvents.h>` when `FRONTIER_HEADLESS` is set; build both headless and desktop targets.
3. **Integration test:** Add a CLI test that triggers a verb previously routed through AppleEvents (e.g., `defined(system.verbs)`) and confirm the new dispatcher handles it.

   **Exit criteria:** Headless builds compile without AppleEvent headers, automated CLI test suite passes using the new dispatcher, and desktop path still routes through AppleEvents without regression.

## 6. Header / Include Cleanup
1. **Audit script:** Write a simple checker (`scripts/check_includes.py`) that fails if `Carbon`/`QuickDraw` headers appear in a headless compile unit.
2. **Include tree pass:** Run `ninja -t deps` or `clang -M` equivalent to confirm no portable source includes `osincludes_portable.h` redundantly; document results in `status_log.md`.
3. **Final sign-off:** After all phases, update `_CURRENT_STATUS.md` with the completion summary and archive `tracer_bullets.md` in `planning/archive/` with a “Completed” status banner.

   **Exit criteria:** Include audit script passes cleanly, dependency scan shows only intended headers in headless builds, status docs record completion, and this tracer file moves to the archive with a completed banner.

---

Revisit this document whenever a branch scope shifts or a tracer bullet needs adjustment, and log those decisions in `planning/phase3/carbon_migration/decision_log.md`.
