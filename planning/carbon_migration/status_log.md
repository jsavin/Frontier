# 2025-10-31
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
