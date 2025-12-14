# Carbon Migration / Runtime Modernization – Active TODO

Status: In Progress (Updated 2025-12-13)  
Owner: Codex

## Runtime / CLI Stabilization
- [ ] Implement headless/kernel clock verbs (`clock.now`, `clock.ticks`, related date coercions) so `cli_runtime_tests` stop exiting 1.
- [ ] Re-run `make -C tests cli_runtime_tests` after clock/date wiring and confirm real pass/fail instead of exit=1 skips.

## Hash / Serialization Follow-Ups
- [ ] Add corruption/bounds tests for hash unpack (OOB name index, truncated records, header edge cases). Ref: Issue #76.
- [ ] Refactor BE pack/unpack into helpers/macros to reduce manual memcpy repetition. Ref: Issue #77.
- [ ] Cross-arch BE64 verification for v7 hash/table records (x86_64 vs arm64 golden blobs). Ref: Issue #78.
- [ ] Consider small gating for verbose hash unpack logging to keep perf predictable when enabled.

## Numeric Type System (mostly done)
- [x] 64-bit widening for int/long/date in `tyvaluedata`; arithmetic/bitwise paths operate on 64-bit.
- [x] Infinity constant set to INT64_MAX; constants seeded at startup.
- [ ] Add targeted tests for 64-bit infinity usage (e.g., `string.mid(..., infinity)`), and date range around 2040+ per `planning/phase3/date_time_format_standard.md`.
- [ ] (Deferred) Drop double handle indirection; only when confident it won’t affect runtime semantics.

## Docs / Planning
- [x] Document v7 hash record layout and logging env var in `docs/database_architecture.md`.
- [ ] Keep `_STATUS_ARCHIVE.md` and `_CURRENT_STATUS.md` in sync with future milestones; note any new env vars or tooling expectations.

## Nice-to-Haves / Future
- [ ] Add small helper macros for BE packing in other packers if duplication grows.
- [ ] Add path validation tests for logging env vars to guard against unsafe paths (headless only).
