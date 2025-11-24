# Reader Split Plan – v7 BE/64-bit Path vs Legacy Adapter
**Last Updated:** 2025-11-24 — Codex

## Goals
- Isolate a clean, strict v7 reader/writer path (BE/64-bit only).
- Add a legacy v6 adapter solely for migration (read legacy, widen in-memory, emit v7 via modern packers).
- Retire scattered `use_64bit_format` forks in pack/unpack code where possible.
- Re-enable CLI/runtime regressions (e.g., `clock.now()`) once the v7 reader is strict and migration uses the adapter.

## Proposed PR Breakdown

### PR 1: Introduce Legacy Adapter + V7 Reader Skeleton
- Add a legacy adapter module that:
  - Detects v6 roots, reads legacy payloads, widens addresses/sizes in-memory.
  - Feeds widened structures into existing v7 packers for output (no new format mutations).
- Add a v7-only reader entry point that assumes BE/64-bit payloads (no legacy heuristics), gated by header detection.
- Wire migrator/open paths to choose adapter vs v7 reader based on detected version.
- Tests: unit coverage for adapter (legacy header → widened in-memory), ensure procedural BE goldens still pass.

### PR 2: Route Runtime/CLI to Clean V7 Reader and Drop Forks
- Update runtime/CLI open paths to call the v7 reader for v7 roots; use adapter only during migration.
- Remove/disable `use_64bit_format` forks in pack/unpack that were only keeping legacy payloads alive in v7.
- Re-enable `tests/cli_runtime_tests` `clock.now()` and related runtime tests once v7 reader is in use.
- Tests: runtime/CLI suites, regression logs noted in `_CURRENT_STATUS.md` with timestamps.

### PR 3: Cover and Harden
- Add byte-level regression(s) for adapter output: legacy input → v7 bytes match expectations.
- Expand procedural goldens if needed (e.g., minimal table/record payload through adapter → v7 bytes).
- Document finalized reader split and adapter behavior in `docs/database_architecture.md` and planning docs.
- (Optional) Address any remaining warnings (e.g., `__builtin_return_address`) if still present.

## Execution Notes
- Keep PICT/opaque payloads untouched; only normalize lengths/headers in BE.
- Adapter should reuse modern packers to avoid drift; no dual writers.
- Update `_CURRENT_STATUS.md` after each PR with log paths for runtime/CLI runs.
- Once CI gains x86, run procedural goldens there to validate cross-arch behavior.
