# Legacy vs Modern Reader/Packer Split Plan
**Last Updated:** 2025-12-08 — Codex  
**Purpose:** Inventory all mixed legacy/v7 code paths, lock in the delegation pattern (`*_legacy` ↔ `*_modern` with a single dispatcher), and sequence the remaining forks so modern BE64 stays branch-free while legacy v≤6 behavior remains untouched.

## Goals
- Keep v7 pack/unpack/read/write free of legacy branches; isolate compatibility to `*_legacy` adapters.
- Ensure headless/portable builds only depend on modern code except when explicitly invoking legacy adapters (migration, v6 open).
- Preserve v≤6 behavior for migration/unpack while guaranteeing modern BE64 storage for all types (ints, dates, doubles, menu state, etc.).

## Current Split Inventory (good)
- **DB header + core reader/writer:** `db_reader_modern.c`, `db_reader_legacy.c`, `db_writer_modern.c`, `db_format_adapter_*`.
- **Tables:** `tablepack.c` (modern) vs `legacy/tablepack_legacy.c`; dispatch in `langhash.c` and table verbs.
- **Outlines (packers):** `oppack_modern.c` vs `legacy/oppack_legacy.c`; outline visit dispatch in `langhash.c`.
- **Menus:** `menupack.c` already has `mepackmenustructure_modern/_legacy` with dispatcher.
- **Kernel verbs init:** generated `kernel_verbs_init.c` (headless-only processors); legacy GUI processors are not compiled in headless.

## Mixed/Unsplit Areas (needs action)
- **Lists/oplists:** `oplist.c` has a single `oppacklist`/`opunpacklist` that mixes legacy layout assumptions and headless diagnostics; no modern fork exists.
- **General value pack/unpack dispatch:** `langpack.c`/`langhash.c` still route some types through legacy helpers without an explicit modern fork (lists, possibly scripts/WPTexts).
- **Scripts/WPText/pict/other externals:** packed via `langexternal.c`/type-specific verb packers; modern vs legacy separation is not explicit outside tables/outlines/menus.
- **CLI/runtime loader:** `db_format_prepare_runtime`/`headless_init_kernel_verbs` rely on generated init; alignment expectations need to be stabilized separately from legacy layout (packed(2)).

## Sequenced Plan (do in order)
1) **Centralize dispatch table for pack/unpack per valuetype**
   - Files: `Common/source/langhash.c`, `Common/source/langpack.c`
   - Actions: Explicitly route each valuetype to `*_modern` or `*_legacy` helpers; add comments noting the split. Ensure migration paths choose legacy; modern writer/reader choose modern.

2) **Fork list pack/unpack**
   - Files: `Common/source/oplist.c`
   - Create `oppacklist_modern`/`opunpacklist_modern` (BE64 counts, no UI globals) and `oppacklist_legacy`/`opunpacklist_legacy` (current behavior, packed(2), outline-handle quirks).
   - Add dispatcher `oppacklist`/`opunpacklist` that selects based on format mode (reuse pattern from tables/outlines).
   - Remove headless debug traps from shared path; keep tracing in legacy or under a debug macro.

3) **Scripts / WPText / pict externals split**
   - Files: `Common/source/langexternal.c`, `Common/source/scriptverbs.c` (if present), `portable/paige_text_extractor.c`, `portable/wptext_runtime.c`.
   - Define `scriptpack_modern/_legacy`, `wptextpack_modern/_legacy`, etc., and dispatch in verb pack/unpack helpers.
   - Modern path: BE64 sizes, UTF-8 RTF blobs, drop UI/font/window metadata. Legacy path: preserve original serialization unchanged.

4) **Hash/outline/list traversal safety**
   - Files: `Common/source/langhash.c`, `Common/source/oplist.c`
   - Ensure `hashpackvisit_modern` never calls legacy visitors; legacy adapter only. Add sanity checks that `valuetype` enums map to correct dispatcher (prevent listtype→29 mishaps).

5) **Menu fork hardening**
   - Files: `Common/source/menupack.c`
   - Verify modern path drops `diskfontstring`/UI state except `menuactive` and `lnumcursor` (preserved, BE64). Confirm dispatcher is branch-free in modern path and legacy untouched.

6) **CLI/runtime build hygiene**
   - Files: `frontier-cli/Makefile`, `Common/headers/lang.h`, `Common/headers/op.h`
   - Ensure headless build links only modern packers plus legacy adapters explicitly (no missing sources). Resolve packed(2) alignment asserts by keeping asserts in modern structs and gating legacy-packed structs with a documented macro or alignment shim.

7) **Tests/regressions**
   - Add headless tests per fork:
     - Lists: round-trip pack/unpack legacy vs modern; mixed element types (string4, script, table).
     - Scripts/WPText: legacy read → modern write → modern read equivalence; verify UI metadata stripped.
     - Menus: modern pack/unpack preserves `menuactive`/`lnumcursor`, drops fonts/window info.
   - Touch files: `tests/langvalue_64_tests.c` (if needed), new targeted suites under `tests/` (e.g., `list_pack_tests.c`, `menu_pack_tests.c`, `script_wptext_pack_tests.c`).

## Implementation Checklist (tick as we go)
- [ ] Dispatch map finalized for all valuetypes (legacy vs modern) — `langhash.c`, `langpack.c`.
- [ ] `oppacklist_modern/_legacy` + dispatcher in `oplist.c`; headless diagnostics contained.
- [ ] Script/WPText/pict externals split with clear modern/legacy helpers; dispatcher wired in `langexternal.c`.
- [ ] Menu modern path validated for BE64 + stripped UI metadata; legacy path intact.
- [ ] CLI/headless build compiles with modern packers and stable alignment strategy.
- [ ] Regression tests added and run for lists, menus, scripts/WPText; legacy-vs-modern round-trips validated.

## Notes on Delegation Pattern
- Keep the public entry points (`oppacklist`, `opunpacklist`, etc.) thin dispatchers that decide based on format mode (v≤6 adapter vs v7).
- Encapsulate legacy-only branches inside `*_legacy` functions; avoid `if (legacy)` inside modern implementations.
- Maintain BE64-only serialization on all modern paths; legacy paths preserve original endianness and packed(2) struct layouts.
