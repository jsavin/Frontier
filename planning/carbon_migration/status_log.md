# 2025-11-05
- Removed `tests/db_portable_stub.c`; `save_migration_tests` now copies/migrates `Frontier.root`, opens the migrated file through `db_portable`, verifies view/handle reads, and closes via the real shim. Updated `_CURRENT_STATUS.md` and `db_portable_plan.md` accordingly. `make -C tests all` and `make -C frontier-cli` remain green (warnings unchanged).

# 2025-10-31
- Added `portable/appleevent_portable.c`, wired headless builds to the shared file/database shims (`portable/file_portable.c`, `portable/db_portable.c`), trimmed duplicate stubs in `headless_mac_compat.c`, and reran Step D commands: `make -C tools/strings_compiler`, `make -C tests all`, and `make -C frontier-cli` now succeed (warnings noted for legacy pointer-sign conversions). Docs updated (`_CURRENT_STATUS.md`, `header_cleanup_plan.md`) to mark Step D complete and track the remaining warning cleanup.
- Guarded `appleevent_portable_shim.h` so it only defines base types when `osincludes_portable.h` is absent, added `tests/db_portable_stub.c` (with file/alias/table stubs and globals) so `make -C tests db_format_tests && ./db_format_tests` now succeeds; full Step D rerun still fails (`make -C tests all`, `make -C frontier-cli`) on missing classic helpers such as `HLock/HUnlock`, `getstringlist`/`langpackfileval`, TEC shims, and QuickDraw color/rect macros.
- `portable/text_encoding_portable.{h,c}` now returns real data (UTF-8, MacRoman, Windows-1252) so `initCharsetsTable` succeeds in headless builds; added `portable/quickdraw_portable.h` to map rect conversions without pulling QuickDraw.
- Headless builds now provide `getstringlist` via `strings_portable.c`, sourcing data from the generated tables; missing YAML coverage surfaces as stderr warnings and placeholder strings, so we still need to migrate real STR# content.
- Documented the headless-only database shim plan in [`db_portable_plan.md`](db_portable_plan.md) and refreshed README/_CURRENT_STATUS with the headless runtime North Star. The shim will expose migration-friendly wrappers in `portable/db_portable` so `db_format` and tests stop depending on Carbon database APIs.
- AppleEvent shim now self-contained (`appleevent_portable_shim.h`); `headless_stubs.h` and `portable/text_encoding_portable.h` defer to the inline stubs, and `osincludes_portable.h` no longer requires early includes. Ran Step D builds (`make -C tools/strings_compiler`, `make -C tests all`, `make -C frontier-cli`); tests fail at db I/O link symbols and the CLI still references unresolved Carbon helpers (HLock/FixRound/getstringlist). `_CURRENT_STATUS.md` and `header_cleanup_plan.md` updated with blockers.
- Updated `strings_measure_pixels` with a 7 px-per-glyph heuristic for headless builds, dropped `quickdraw.h` from headless stubs, added no-op port/color shims, inlined portable rect/RGB converters in `langhash.c`/`langpack.c`, wrapped `op.c`/`opicons.c`/`opexpand.c`/`opops.c`/`oppack.c`/`opinit.c`/`opverbs.c` to avoid QuickDraw; desktop-only files (e.g., `opdisplay_desktop.c`, `opdraggingmove.c`) are marked Not Needed, and reran `make -C tools/strings_compiler`, `make -C tests handle_tests`.
- Captured call-site maps for STR# string lists, filespec/alias helpers, QuickDraw utilities, and AppleEvent IPC under [`maps/`](maps/). Updated `_CURRENT_STATUS.md` with the completed Step 1 milestone.
- Documented the Carbon refactor branch order (memory → strings/resources → filespec/alias → QuickDraw → AppleEvents → header cleanup) in `_CURRENT_STATUS.md`.
- Added tracer bullet milestones for each branch in [`tracer_bullets.md`](tracer_bullets.md).
- Extended `portable/classic_handle.c` to reuse the shared allocator, track `MemError`/`MaxBlock`, and guarded the headless stubs; `make -C tests handle_tests && ./handle_tests` now covers these paths.
- Completed Win32 parity review; documented IPC reuse, feature flags, and resource table notes in [`windows_parity_review.md`](windows_parity_review.md).
- Drafted STR# replacement plan (`strings_replacement_plan.md`) centered on a bison-driven YAML compiler, outlining source format, tooling, migration steps, and tests.
- Implemented the initial `tools/strings_compiler/` utility (bison/flex) and verified it generates C/H/manifest outputs for sample YAML tables.
- Vendored libyaml 0.2.5 under `third_party/libyaml/` and updated `tools/strings_compiler/` to load YAML via libyaml; bison/flex work deferred to Phase 2. Header cleanup staged in [`header_cleanup_plan.md`](header_cleanup_plan.md): QuickDraw/UI extraction (in progress), TextEncoding shim landed (`portable/text_encoding_portable.h`), AppleEvent split underway (`appleevent_portable.h`), include audit/regression rebuild to follow.

# 2025-10-30
- Documented remaining platform-specific dependencies in [`platform_legacy_audit.md`](platform_legacy_audit.md) and updated `_CURRENT_STATUS.md` with the refactor roadmap.

# Carbon Migration Status Log

Chronological breadcrumbs for this project. Add an entry whenever we complete a meaningful milestone (phase finished, major PR merged, blocker discovered, etc.). Keep entries short and link to PRs when available.

## 2025-10-29
- Created Carbon migration plan (`planning/carbon_migration/`).
- Updated status/index docs to point at the new workstream.
- Inventory seeded from current headless build failures.

<!-- Add newest entries to the top -->
