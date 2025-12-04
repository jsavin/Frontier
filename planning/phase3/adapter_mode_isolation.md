# Adapter Mode Isolation

Status
- State: In Progress
- Phase: Carbon Migration / Runtime Modernization
- Last Updated: 2025-12-01 (Evening)
- Owner: Codex
- Notes: Plan to separate legacy read vs modern write/read adapter configs to stop repack state bleed.

Related Docs
- planning/_CURRENT_STATUS.md
- planning/phase3/modern_reader_writer_split.md
- Common/source/db_format.c (mode stack)

Change Log
- 2025-12-01: Initialized document (Codex).
- 2025-12-02: Added `db_format_mode_enter/push_*` presets and wrapped migrator, Cancoon, ODB, and CLI Save-As entry points with scoped `modern_write_repack` pushes; `make -C tests db_format_tests` and `make -C tests save_migration_tests` green under the new helpers (Codex).

Overview
- Purpose: Eliminate adapter_repack state bleed by using role-specific modes for legacy reads, modern writes (repack), and modern reads.
- Scope: Migration/Save As path, packers/unpackers, CLI/headless runtime loader; no UI changes.
- Audience: Runtime/migration contributors.

Decisions
- Use three explicit mode presets (legacy read, modern write+repack, modern read) instead of a single shared mode/global.
- Keep adapter state thread-local via the existing mode stack; remove reliance on any process-wide repack toggle.
- Assert the write-repack mode at the Save As packing boundary so packers always see adapter_repack=true when emitting BE64 externals.

Details
- Mode presets:
  - mode_legacy_read: use_64bit_format=false, adapter_repack=false (v6 source reads/unpack).
  - mode_modern_write_repack: use_64bit_format=true, adapter_repack=true (Save As packing/writes).
  - mode_modern_read: use_64bit_format=true, adapter_repack=false (v7 opens/reads).
- Boundaries:
  - Legacy open/read/unpack: push legacy_read around legacy reader/unpack calls.
  - Save As/destination write: push modern_write_repack immediately before tablesavesystemtable/dbassign/dbsavehandle.
  - Modern read: push modern_read for v7 opens (strict reader) to keep adapter_repack off.
- Isolation:
  - Each boundary uses push/pop; no long-lived mode reuse across reader/writer stages.
  - Remove any remaining global repack toggles; mode is the sole source of adapter_repack.

Open Questions
- Do any packers still bypass db_format_adapter_force_repack and need direct mode checks?
- Are there other code paths (e.g., direct dbassign callers) that need explicit modern_write_repack wrapping?

Next Steps
- Implement mode helpers (push/pop) for the three presets and swap callers to use them at boundaries.
- Reassert modern_write_repack at Save As pack time (tablesavesystemtable/dbassign) and verify packer logs show adapter_repack=1.
- Remove leftover global repack references and rebuild tests/CLI.
- Re-run migration and CLI clock.now() to confirm system table wiring and valid BE64 externals.
- Capture `frontier-cli --system-root ... clock.now()` crash details under LLDB now that repack scoping is live; confirm adapter logs read as expected (`adapter_repack=1`).

Appendix – Follow-on items from prior plan
- Add byte-level regressions for adapter-widened payloads (table/record/externals) to guard the new split.
- Route runtime/CLI v7 opens through the strict modern reader once payload widening is ready; rerun `make -C tests runtime_tests` and `make -C tests cli_runtime_tests`.
- Re-run migrator/CLI smoke on additional legacy roots after widening; stash logs under /tmp with timestamps.
