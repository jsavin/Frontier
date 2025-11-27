# Modern Reader/Writer Split Plan
**Last Updated:** 2025-11-26 — Codex  
**State:** Draft / In Progress  
**Scope:** Complete the clean fork between legacy (v6) reads and modern (v7, BE64) reads/writes; retire legacy write code for headless use and ensure v7 outputs drop legacy Cancoon/view blocks.

## Objectives
- Fully separate legacy vs modern read/write paths (no shared branching on 32-bit vs BE64).
- Keep legacy reads isolated; use a modern write context to emit BE64 v7 files without Cancoon/view drift.
- Preserve headless focus: modern writer only; legacy writer retired/stubbed.
- Ensure v7 roots do not carry the legacy Cancoon record present in v≤6 roots; view0 should be the modern root only.
- Maintain a strict fork: modern code should be branch-free for BE64; legacy handling should live in separate functions/files so modern logic stays clean.
- Naming split: modern packers keep canonical names in modern files/dirs; legacy packers live in `legacy_*/` dirs and are suffixed with `_legacy` entry points so the modern path remains the default going forward.

## Plan
1) **File/Module Layout**
   - Move remaining reader logic out of `db_format.c` into `db_reader_legacy.c` / `db_reader_modern.c`; delete reader helpers left in `db_format.c`.
   - Consolidate BE64 header/block writers in `db_writer_modern.c`; remove modern write helpers from `db_format.c`.
   - Keep `db_format.c` for shared utilities only (BE helpers, adapter state, detection, migrate helpers).
   - Seed `Common/source/legacy/` with the 32-bit pack/unpack forks (`tablepack_legacy.c` plus `langexternal_legacy.c` / `opverbs_legacy.c`) so modern canonical names can drop runtime format branching.
   - Update all build targets (tests, frontier-cli) to use the split modules; remove duplicate inclusions.
   - After the split stabilizes, remove the `use_64bit_format` global: switch all consumers to `db_format_mode_current()`/explicit mode args and delete the symbol.
2) **Core Delegation (db.c)**
   - Keep `dbread`/`dbwrite` exported for the split modules; remove unused `db_prepare_modern_header`.
   - Make `dbwriteheader/dbwritetrailer` BE64-only (modern writer) with no legacy branches.
   - Ensure `dbopenfile` uses `db_read_legacy` for v6 and `db_read_modern` for v7+, without flipping `use_64bit_format` during legacy reads.
3) **Migration Flow & View/Cancoon Cleanup**
   - Maintain separate contexts: legacy reader for source, modern writer for destination (no mid-run `use_64bit_format` flip).
   - Drop Cancoon/view0 legacy block entirely when writing v7; set `views[0]` to the new root only.
   - Verify packing uses modern block/header writers and BE64 addresses throughout.
   - Widen payloads (tables/records/externals) to BE64 during migration: legacy read → widen in-memory → write via modern packers with `use_64bit_format=true` and BE endianness for lengths/addresses. No mixed-width writers.
4) **Tests/Validation**
   - Add byte-level regressions: modern header/block write outputs; adapter-widened payload → modern bytes (table/record).
   - Add synthetic round-trip: build a tiny legacy-packed table payload (externals + scalars), decode with legacy reader, widen + repack via modern writer, decode with modern reader, and assert logical equality (types/keys/values; addresses may differ). Fail on any 32-bit or mixed-endian remnant.
   - File-level round-trip: migrate canonical v6 root to v7, reopen with modern reader, and verify sentinel tables/externals logically match legacy reads. Confirm view0/variance and Cancoon removal.
   - Re-enable `runtime_tests`/`cli_runtime_tests` once the modern path is clean; log paths in `_CURRENT_STATUS.md`.
   - Rerun `FRONTIER_REGEN_ROOT=databases/Frontier-v6.root ./tests/runtime_tests`; verify view0 variance and Cancoon drop.
5) **Docs/Tracking**
   - Update `_CURRENT_STATUS.md` and the two phase3 docs with milestones as each slice lands.
   - Track remaining warnings (`__builtin_return_address`, unused helpers) for cleanup after BE64 path is solid.

## Notes
- Headless: only modern writer needed; legacy writer can stay stubbed/unused.
- Keep PICT/opaque payloads untouched; only normalize headers/addresses and record/table metadata lengths in BE64.
- Payload widening + round-trip validation are critical: don’t call the split “done” until synthetic and file-level legacy→modern→modern-read checks pass and `runtime_tests`/`cli_runtime_tests` succeed on a migrated root.
