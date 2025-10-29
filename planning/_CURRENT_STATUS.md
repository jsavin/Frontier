# Frontier Database Format Investigation - Current Status

**Last Updated**: October 27, 2025 (Headless System work queue) <!-- 2025-10-27 Codex: refreshed status -->
**Branch**: feature/headless-system-bootstrap
**Session ID**: 019a2490-f65d-7861-b9c6-a6718394c2e8 <!-- 2025-10-27 Codex: captured current Codex session -->

## Updated Plan — October 25, 2025

1. **Scanner & Docs (done)**: Documented that the 442-byte block at `views[0]` is a serialized Cancoon/About record and taught the scanner to follow its `adrroottable`. Helper script `scripts/dump_tables.py` enumerates actual modern blocks for debugging.
2. **Next Work Items**:
   - Re‑audit the v6→v7 migration path now that we can reliably read the v6 roots. Verify block copying and format conversion are lossless.
   - Resume headless runtime work: load the v7 Frontier.root, bring the system table online, and ensure glue scripts can call through to our kernel implementations.
   - Preserve Cancoon state for future UI re‑hosting (tracked in `planning/TODO_future_improvements.md`).

## Longer-Term Goals
- Lock in the v6→v7 migration (Phase 1/Phase 2 deliverables) with automated verification once the parser is stable.
- Complete headless runtime parity so server deployments can run without GUI dependencies while still exposing necessary UI state (About/Cancoon, msg log, agent status) through new channels.
- Phase 3+ work: database hash-table modernization, headless CLI improvements, and eventual UI layer re-host.

## Quick Status — October 28, 2025
- Replaced the test-only file stubs with a shared stdio-backed implementation (`tests/headless_mac_compat.c`), so `openfile`/`fileseteof`/`headless_readline` now behave like the desktop code during migrations and CLI runs.
- Trimmed the `test_migration` build to use the canonical language/runtime sources (no `FRONTIER_PORTABLE` or stubbed hash/runtime files), exposing the real symbol gaps we still need to solve.
- Added portable guards in `osincludes_portable.h` for AppleEvents, Components, FSRefs, etc., letting headless builds consume Carbon-dependent headers without extra shims.
- `langhash.c` compiles further under headless but now fails on a handful of legacy helpers (`getstringlist`, `recttodiskrect`, `rgbtodiskrgb`, `dtox80`, etc.). We need targeted replacements or feature gates before the portable tests link again.

## Progress — October 26, 2025
- `tests/db_format_tests` builds/runs cleanly, and a lightweight harness built from `Common/source/db_format.c` successfully migrated `databases/test.root` and `databases/Frontier-v6.root` into `<base>-v7.root`, both of which scan correctly via `scripts/scan_database_types.py`.
- `tests/runtime_tests` now runs in the headless portable configuration (after extending the stub set with `dbnormalizeaddress`), exercising `langrunstringnoerror`, constant evaluation, OPML round-trips, and serializer round-trips—confirming kernel verbs execute end-to-end in headless mode.
- `frontier-cli --system-root databases/Frontier-v6.root -e "3 + 4"` brings the runtime up and executes scripts; however, glue lookups such as `clock.now()` still fail because the migrated system table is missing key subtables/built-ins (`system.verbs.builtins` never materializes after `settablestructureglobals`). We removed the old “auto-hydrate” fallback to avoid masking this bug.

## Immediate Next Steps (migration focus)
1. ✅ **Doc & guidance updates** – AGENTS.md and docs now capture the v6→v7 gap and PR template requirements.
2. ✅ **Portable header prep** – refactored `portable/*` headers and shared includes so Carbon vs portable typedef collisions are largely resolved.
3. ✅ **Finish portable file I/O shims** – stdio-backed file routines now live in `tests/headless_mac_compat.c`, and the test harness links against the real implementations.
4. ⏳ **Validate serialized output**: after the build is green, diff `system`/`system.verbs` blocks between v6 and migrated v7 to confirm 64-bit addresses and record sizes.
5. 🔄 **Unblock headless `langhash` build**: provide portable equivalents or guards for `getstringlist`, rect/RGB packing helpers, and other legacy glue so `test_migration` can compile end-to-end again.
6. ⏳ **Re-run headless CLI checks** (`defined(system.verbs)`, `clock.now()`) against the freshly migrated root; capture results and clean up instrumentation once stable.
7. ⏳ **Add regression coverage** in migration/component tests to assert widened addresses so header-only regressions are caught automatically.
