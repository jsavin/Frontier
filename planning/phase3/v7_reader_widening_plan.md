# V7 Reader Widening Plan (Legacy Adapter → Strict V7)
**Last Updated:** 2025-11-24 — Codex

## Status
- State: Draft
- Phase: Phase 3 (migration/runtime)
- Owner: Codex
- Notes: PR #49 merged router/decode guard; this plan covers the real legacy widening and strict v7 reader.

## Objectives
- Legacy adapter: read legacy (v6) payloads, widen addresses/sizes in-memory, and re-pack via v7 packers with `use_64bit_format=true` before writing.
- V7 reader: enforce strict v7 decode (no legacy heuristics) and keep BE/64-bit invariants.
- Preserve current behavior until widening is complete; add targeted tests to prove widening.

## Task Breakdown (granular)
1) **Identify 32-bit legacy unpack/pack touchpoints** (tablepack, record packers, lang packers) used during migration.
2) **Implement legacy→v7 widening path**
   - Unpack legacy tables/records at 32-bit widths.
   - Widen dbaddresses/lengths to 64-bit in-memory.
   - Re-pack using v7 packers with `use_64bit_format=true`.
   - Ensure runtime-only fields remain zeroed on disk.
3) **Strict v7 reader enforcement**
   - Remove/guard legacy heuristics when header version >= 7.
   - Keep BE/64-bit decode only; fail fast on unexpected legacy payloads.
4) **Tests**
   - Add synthetic legacy payload → adapter → v7 bytes test (byte-level check) in `tests/db_format_tests`.
   - Run `make -C tests runtime_tests` after widening; log results.
   - Plan to re-enable `cli_runtime_tests` `clock.now()` after reader is strict (may be next PR).
5) **Docs/Status**
   - Update `_CURRENT_STATUS.md` and `phase3` plans upon landing.

## Notes
- Keep PICT/opaque payloads untouched; only normalize lengths/headers.
- Procedural goldens remain the cross-arch sentinel.
- Address the `__builtin_return_address` warning separately (not blocking widening).
