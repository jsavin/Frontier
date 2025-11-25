# Big-Endian Portability Audit (v7)
**Last Updated:** 2025-11-23 — Codex  
2025-11-23 23:59 CST (Codex): Added a detailed roadmap to reach full BE/64-bit parity.
**Purpose:** Lock down v7 on-disk byte order (big-endian) across all writers/readers so arm64/x86 outputs match bit-for-bit.

## Scope & Goals
- Enforce big-endian encoding for all v7 database writes (header, tables, records, avail list, block metadata).
- Keep readers symmetric with writers; avoid mixed host-order fields.
- Add regression coverage to catch cross-arch drift (goldens/log comparisons).
- Document the final on-disk layout and migration expectations.

## Work Items
1. **Avail list serialization (P0) — Done**
   - Decision: use BE 64-bit `adr` + BE 64-bit `size` (no 4 GB cap) to avoid future format bumps and protect large roots (6–10 GB). Block headers/trailers now store 64-bit sizes in BE; avail links read/write in BE64. Regression: in-memory >4 GB free-span encode/decode lives in `tests/db_format_tests.c` (no artifacts).
   - Next: audit any remaining 32-bit size math and shadow flush paths to ensure end-to-end 64-bit.
2. **Record/block metadata audit (P0)**
   - Find any remaining `memtodisklong`/host-order writes for record lengths/addresses in save paths (`dbwritedatablock`, packers).
   - Apply BE helpers uniformly; confirm header/trailer variance paths already BE.
3. **Reader parity (P0)** — **Done (for v7 headers/trailers/avail)**; verify remaining packers as cleanup lands.
   - Ensure all v7 read paths mirror the BE writes (headers, tables, avail list, block metadata).
   - Add defensive logging where mixed-endian data is encountered.
4. **CLI/runtime validation (P0)**
   - Re-enable `tests/cli_runtime_tests` `clock.now()` once v7 reads/writes match across arch.
   - Re-run migrator/runtime suites and stash logs under `/tmp` with timestamps.
5. **Documentation (P0)** — **In progress**
   - Update `docs/database_architecture.md` with the finalized v7 BE layout (field widths, endianness rules).
   - Note the portability guarantees and test commands in `planning/_CURRENT_STATUS.md` and `planning/TODO_future_improvements.md`.
6. **Cross-arch regression coverage (P1)**
   - Add a golden byte-level check: write a minimal v7 DB on host, compare header/table/avail bytes to a saved BE reference.
   - Add a free-list exercise: allocate/free blocks, persist, reopen, and verify avail list bytes unchanged on arm64/x86.

## Outstanding 64-bit/BE cleanup
- **Legacy packers still 32-bit:** Outline/OP packing (`oppack.c` `header.sizetext/sizelinetable`), lang tree packers, and regex/langpack metadata still use `memtodisklong`/32-bit sizes. Convert these to explicit BE helpers with fixed-width fields so all v7-era disk writes are 64-bit clean, even if practical payloads stay <4 GB.
- 2025-11-23 Codex: Outline/OP, langpack, langtree, and regexp packers now use explicit BE helpers for length/type fields; continue sweeping remaining packers and writers that still rely on `memtodisklong`/host-order paths (see `rg memtodisklong` for stragglers).
- **Tables/records:** Verify record-length writers (tablepack/oppack/langpack) aren’t leaking host-endian or 32-bit sizes. Replace remaining `memtodisklong` with `db_format_write_be32/64` as appropriate.
- **Shadow avail cache:** Confirm any shadow flush paths use 64-bit size/links after the header/trailer and cache struct widening (int64_t).
- **Cross-arch goldens:** Add a minimal v7 root written on one arch and assert byte-for-byte equality on another; include a >4 GB free-span simulation in the suite.
- **Docs/status:** Once the above lands, record the “fully 64-bit/BE” milestone in `_CURRENT_STATUS.md` and update `docs/database_architecture.md` with the final field widths.

## Detailed plan to reach “no 32-bit/endianness worries” for v7
## BE/64-bit Sweep TODO List
- [x] Outline/OP packers: `oppack.c` header sizes → BE32.
- [x] Lang packers: `langpack.c` packed value type/long fields → BE32.
- [x] Lang tree packers: `langtree.c` node sizes/ctnodes/flags → BE32.
- [x] Regexp packer: `langregexp.c` compiled pattern type → BE32.
- [x] PICT packer: `pict.c` pictbytes length → BE32; payload untouched.
- [x] Core DB header fields: `db.c` (availlist, extensions.availlistblock, views[], headerLength) → BE helpers; mirror readers.
- [x] Menu packers: `menupack.c` linked script addr/adroutline → BE helpers.
- [x] Language primitives: `langvalue.c`, `langhash.c`, `langscan.c`, `strings.c` (typeid/osvalue/number/temp) → BE helpers.
- [x] ODB/Cancoon: `cancoon.c`, `odbengine.c` (adrroottable/adrscriptstring) → BE helpers.
- [x] WP engine: `wpengine.c` (header.ctsaves/header.maxpos) → BE helpers.
- [x] Memory serialization: `memory.c`, `memory.track.c` (legacy disk32/x) → BE helpers or document as diagnostics if unused on disk.
- [x] Re-run `db_format_tests` and `runtime_tests` after each chunk; add targeted regressions where fields change (consider a PICT length round-trip check).
- [x] Cross-arch goldens: add byte-for-byte v7 reference + free-list exercise; keep >4 GB span simulation. (Procedural golden added to db_format_tests; run on other arch when available.)

- **Sweep writers for host-order/32-bit fields (P0):** In save paths (`dbwritedatablock`, table/record packers), replace any `memtodisklong` or host-order writes for lengths/addresses with explicit BE helpers (`db_format_write_be32/64` per field bounds). Re-check header/trailer variance writes while sweeping.
- **Widen legacy packers to fixed-width BE (P0):** Update outline/OP packing (`oppack.c` `header.sizetext/sizelinetable`), lang tree packers, and regex/langpack metadata to fixed-width BE fields (BE64 where sizes can grow; BE32 only when structurally bounded). Remove residual 32-bit size math in these packers.
- **Shadow avail/cache parity (P0):** Ensure any shadow flush/cache structs use 64-bit size/links and the same BE64 helpers as primary header/trailer writers.
- **Reader symmetry and guardrails (P0):** Mirror the above changes in read paths; add defensive logs when mixed-endian data is encountered to flag stale artifacts during migration testing.
- **Cross-arch golden coverage (P1 confidence gate):** Add a minimal v7 root writer test that asserts header/table/avail bytes match a saved BE reference across arm64/x86. Include a free-list exercise (allocate/free/persist/reopen) and keep the >4 GB free-span simulation in the suite.
- **CLI/runtime unblock (P0 after writer fixes):** Once BE/64-bit writers/readers align, re-enable `tests/cli_runtime_tests` `clock.now()` by verifying v7 opens succeed. Run `make -C tests runtime_tests` and migrator smoke (`FRONTIER_REGEN_ROOT=… ./tests/runtime_tests`), stashing logs in `/tmp` with timestamps.
- **Docs and milestone logging (P0):** After the above lands, mark the “fully 64-bit/BE” milestone in `planning/_CURRENT_STATUS.md`, and document final v7 field widths/endianness rules and test commands in `docs/database_architecture.md` (cross-link from `planning/TODO_future_improvements.md`).
- **Prereq for reader refactor:** Completing this BE/64-bit sweep is required before splitting the code into a clean v7 reader/writer path and a legacy v6 adapter (for migration only). Once this audit is green, create a dedicated legacy adapter that widens v6 payloads for the migrator, and keep the primary v7 path strictly BE/64-bit. Plan to do that refactor before resuming tests that execute migrated scripts in the v7 root, since it will surface any remaining migrator bugs more cleanly.

## Risks / Notes
- Avoid partial endian flips: change writers/readers together to prevent free-list corruption.
- Runtime-only fields must stay zeroed on disk (`releasestack`, `fnumdatabase`, in-memory shadows).
- v7 artifacts in-repo are ephemeral; regenerate after changes to validate byte-for-byte parity.

- 2025-11-24 Codex: Version-based reader router/header decode guard merged; procedural goldens remain. Legacy widening + strict v7 reader planned on feature/legacy_adapter_widening_and_v7_reader.
