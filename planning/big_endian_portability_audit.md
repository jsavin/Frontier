# Big-Endian Portability Audit (v7)
**Last Updated:** 2025-11-23 — Codex  
**Purpose:** Lock down v7 on-disk byte order (big-endian) across all writers/readers so arm64/x86 outputs match bit-for-bit.

## Scope & Goals
- Enforce big-endian encoding for all v7 database writes (header, tables, records, avail list, block metadata).
- Keep readers symmetric with writers; avoid mixed host-order fields.
- Add regression coverage to catch cross-arch drift (goldens/log comparisons).
- Document the final on-disk layout and migration expectations.

## Work Items
1. **Avail list serialization (P0)**
   - Decision: use BE 64-bit `adr` + BE 64-bit `size` (no 4 GB cap) to avoid future format bumps and protect large roots (6–10 GB). Rationale: single-maintainer risk; don’t want to revisit v7 → v8 for the free list.
   - Progress: block headers/trailers now store 64-bit sizes in BE, avail links read/write in BE64. Next: audit any remaining 32-bit size math and shadow flush paths to ensure end-to-end 64-bit.
   - Regression: added in-memory >4 GB free-span encode/decode in `tests/db_format_tests.c` (no artifacts). If a file-based variant is added later, use sparse temp files, unlink on success, and log loudly with path on failure for cleanup.
2. **Record/block metadata audit (P0)**
   - Find any remaining `memtodisklong`/host-order writes for record lengths/addresses in save paths (`dbwritedatablock`, packers).
   - Apply BE helpers uniformly; confirm header/trailer variance paths already BE.
3. **Reader parity (P0)**
   - Ensure all v7 read paths mirror the BE writes (headers, tables, avail list, block metadata).
   - Add defensive logging where mixed-endian data is encountered.
4. **CLI/runtime validation (P0)**
   - Re-enable `tests/cli_runtime_tests` `clock.now()` once v7 reads/writes match across arch.
   - Re-run migrator/runtime suites and stash logs under `/tmp` with timestamps.
5. **Documentation (P0)**
   - Update `docs/database_architecture.md` with the finalized v7 BE layout (field widths, endianness rules).
   - Note the portability guarantees and test commands in `planning/_CURRENT_STATUS.md` and `planning/TODO_future_improvements.md`.
6. **Cross-arch regression coverage (P1)**
   - Add a golden byte-level check: write a minimal v7 DB on host, compare header/table/avail bytes to a saved BE reference.
   - Add a free-list exercise: allocate/free blocks, persist, reopen, and verify avail list bytes unchanged on arm64/x86.

## Risks / Notes
- Avoid partial endian flips: change writers/readers together to prevent free-list corruption.
- Runtime-only fields must stay zeroed on disk (`releasestack`, `fnumdatabase`, in-memory shadows).
- v7 artifacts in-repo are ephemeral; regenerate after changes to validate byte-for-byte parity.
