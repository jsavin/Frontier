# Reader Widening Plan (Legacy Adapter → Strict V7)
**Last Updated:** 2025-11-27 — Codex

## Status
- State: In Progress
- Phase: Phase 3 (migration/runtime)
- Owner: Codex
- Notes: **Done:** PR #49 merged router/decode guard; Step 1 audit of legacy touchpoints is complete; reader/writer forks now live in `db_reader_legacy.c`, `db_reader_modern.c`, and `db_writer_modern.c` (builds fixed). Mode/flag cleanup complete (packers/tests/stubs use `db_format_mode_*`; BE64 view0 header regression added); `db_format_tests`/`runtime_tests`/`cli_runtime_tests` pass. **In Progress:** Legacy loader widens headers for v7 packers, caches widened header for wide writes (triggered during Save As/header flush), and keeps legacy reads; strict v7 loader enforces version/size. Table/outline/wp/menu/pict repacks force BE64 writes via adapter; record reference BE64 regression added. Legacy reads stay 32-bit even during Save As (per-handle use64 sync); `langexternalpack` forces legacy reads while materializing externals. Migration now completes with canonical v7 headers (version=7, headerLength=88, view0 set, Cancoon dropped); free-block externals are skipped during packing. Next: payload widening, strict modern reader routing, and clock.now re-enable.

## Objectives
- Legacy adapter: read legacy (v6) payloads, widen addresses/sizes in-memory, and re-pack via v7 packers with `use_64bit_format=true` before writing.
- V7 reader: enforce strict v7 decode (no legacy heuristics) and keep BE/64-bit invariants.
- Preserve current behavior until widening is complete; add targeted tests to prove widening.
- Critical: validate end-to-end round-trip (legacy → widened in-memory → BE64 write → modern read) for payloads and a migrated root; do not declare completion without these checks.
- Naming: modern packers keep canonical names; legacy packers are `_legacy` in `legacy_*/` dirs so modern BE64 remains the default API surface.

## Task Breakdown (granular)
1) **Identify 32-bit legacy unpack/pack touchpoints** (tablepack, record packers, lang packers) used during migration. (**Done**)
2) **Implement legacy→v7 widening path** (header widening + file splits done; payload widening still TODO)
   - Unpack legacy tables/records at 32-bit widths.
   - Widen dbaddresses/lengths to 64-bit in-memory.
   - Re-pack using v7 packers with `use_64bit_format=true`.
   - Ensure runtime-only fields remain zeroed on disk.
3) **Strict v7 reader enforcement** (router/header guards landed; dedicated reader file exists; runtime/CLI routing still pending)
   - Remove/guard legacy heuristics when header version >= 7.
   - Keep BE/64-bit decode only; fail fast on unexpected legacy payloads.
4) **Tests**
   - Add synthetic legacy payload → adapter → v7 bytes test (byte-level check) in `tests/db_format_tests`. (**Header coverage done; still need table/record payload coverage.**)
   - Add table/record payload regression to prove BE64 writes after adapter widening. **Table repack BE64 covered; record payload still pending.**
   - Run `make -C tests runtime_tests` after widening; log results.
   - Plan to re-enable `cli_runtime_tests` `clock.now()` after reader is strict (may be next PR).
5) **Docs/Status**
   - Update `_CURRENT_STATUS.md` and `phase3` plans upon landing.

## Notes
- Keep PICT/opaque payloads untouched; only normalize lengths/headers.
- Procedural goldens remain the cross-arch sentinel.
- Address the `__builtin_return_address` warning separately (not blocking widening).

## Step 1 Audit – 32-bit legacy touchpoints to widen (**Done**)
- **tablepack.c**: `tableverbunpack`/`tableverbpack` switch address width on `use_64bit_format`; adapter must unpack legacy tables at 32-bit, then re-pack via v7 path (8-byte dbaddress, BE).
- **langhash.c**: `write_disk_dbaddress`/`host_to_disk_dbaddress`/`disk_to_host_dbaddress` gate on `sizeof(dbaddress)`; when adapter flips `use_64bit_format=true`, re-packers must emit 64-bit addresses for scalar refs.
- **db_open/router**: `dbopenfile` now routes v6 → legacy adapter, v7 → v7 reader; legacy adapter must widen payloads before flipping `use_64bit_format` to true for re-pack.
- **Tests to add**: synthetic legacy table payload → adapter → v7 bytes (dbaddresses widened, lengths 64-bit BE); ensure decode fails on undersized legacy header (already added). After widening, run `runtime_tests`; `cli_runtime_tests` to re-enable once v7 reader is strict.
