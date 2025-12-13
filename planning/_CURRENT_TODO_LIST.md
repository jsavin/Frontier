# Numeric Type System Modernization – Execution Checklist

Status: In Progress (branch `docs/numeric-type-system-modernization`)
Last Updated: 2025-12-09
Owner: Codex

## Runtime bootstrap / CLI regressions (develop)
- [ ] Diagnose symbol-name corruption and CLI segfault on `Frontier-v6.root`
  - [x] Capture failing traversal: first bad name is `scriptibutes` immediately after `opattributes` during `langfindsymbol` walk; crash follows in same table.
  - [ ] Force logging of legacy `hashunpacktable` name indices/bytes (headless) so we can see the exact length/index in the v6 path before corruption.
  - [ ] Compare legacy `hashunpacktable` against the last known-good (PR #59) for string offset/length handling; audit refcon/handle lifetime for string pool reuse.
  - [ ] Confirm modern packer is not reusing stale string lengths when emitting v7 tables; once fixed, regenerate v7 root and re-run CLI.
- [ ] Restore headless CLI baseline (exit=1/segfaults)
  - [ ] Re-run `./frontier-cli/frontier-cli --system-root databases/Frontier-v6.root -e "3+4"` after fixes; ensure `cli_runtime_tests` no longer exit 139 and startup script dependencies are satisfied.
- [x] Migration crash (Paige text conversion) — fixed
  - [x] Guarded `converttextencoding` to only dispose TEC converters when creation succeeded; reran `SANITIZE=1 make -C tests save_migration_tests && ./tests/save_migration_tests` cleanly.

## Work Items
- [x] Align in-memory numeric storage to 64-bit
  - [x] Widen `tyvaluedata` int/long/date fields to 64-bit (legacy+modern hydration paths)
  - [ ] Convert `double` storage to plain `double` (drop handle indirection) — deferred; optional cleanup, not required for BE64 runtime/disk
  - [x] Update setters/coercion paths (`setintvalue`, `setlongvalue`, `coercetoint`, `coercetolong`, binary-number coercions) to operate on 64-bit values
  - [x] Remove implicit 16-bit clamping; only clamp when explicitly converting to short-sized slots

- [x] Constants
  - [x] Set `longinfinity` to INT64_MAX in `standard.h` and `standard_portable.h`
  - [x] Confirm `langstartup` seeds `infinity` constant from the 64-bit value

- [x] Modern (v7) pack/unpack to BE64
  - [x] Add BE64 helpers for ints/doubles
  - [x] Introduce modern disk layout (e.g., `tydiskvaluedata_v7`, `tydisksymbolrecord_v7`) with 8-byte ints/doubles; keep legacy layout untouched
  - [x] Rename legacy scalar pack/unpack helpers to `*_legacy` and add `*_modern` counterparts using modern structs
  - [x] Add dispatcher to route modern saves/loads through modern pack/unpack when `use_64bit_format` is true; legacy path otherwise
  - [x] Ensure `datevalue` stored as 64-bit Mac epoch in modern path; directions remain 32-bit

- [x] Runtime arithmetic/bitwise paths
  - [x] Ensure arithmetic ops use 64-bit ints by default (add/sub/mul/div/mod)
  - [x] Bitwise verbs now operate on 64-bit values (set/clear/and/or/xor/shifts, limit to bit 63)
  - [x] Removed legacy 32-bit clamping from int coercion (intvaluetype now full 64-bit)

- [x] Numeric conversions/printing
  - [x] Verify numeric-to-string conversions use 64-bit values; `stringaddcommas` is string-only and needs no change
  - [x] Ensure date/time conversions use 64-bit Mac-epoch timestamps per `planning/phase3/date_time_format_standard.md`

- [ ] Tests
  - [x] Add edge-case tests for INT64_MAX/INT64_MIN arithmetic and bitwise ops
  - [ ] Add tests for 64-bit `infinity` in verbs (e.g., `string.mid(..., infinity)`)
  - [x] Add v7 numeric pack/unpack round-trip tests (BE64) and confirm legacy v6 reader behavior unchanged
  - [ ] Add date/time tests for 64-bit Mac epoch (migration from v6, range around 2040+, cross-platform)
  - [x] Adjust `save_migration_tests` to validate the generated v7 artifact (`*-v7.root`) or update the migrator to overwrite the source path once pack/unpack is stable

- [x] Headless UX parity
  - [x] Headless `notifydialog` writes to stdout and blocks for Enter to mirror GUI blocking semantics

## Open Choices to Confirm
- [x] Widen `datevalue` to 64-bit Mac epoch (per `planning/phase3/date_time_format_standard.md`); keep direction/fixed 32-bit unless otherwise specified.
- [x] Keep `intvaluetype` tag (UserTalk `shortType`) but store/operate as 64-bit; clamp only when explicitly coercing to short.

## Notes on record layouts (modern vs legacy)
- Legacy scalar record (`tydiskvaluedata` in `tydisksymbolrecord`): 16-bit int, 32-bit long/single, 32-bit date, 16-bit point coords; used only in legacy/v≤6 paths.
- Modern scalar record (`tydiskvaluedata_v7` in `tydisksymbolrecord_v7`): 64-bit int/long/token, 64-bit Mac-epoch date, 64-bit double bits, 32-bit direction/ostype/enum/fixed/token, 16-bit point coords; used only in modern (v7+) paths.
- Modern symbol layout target: `int32 ixkey`, `uint8 valuetype`, `uint8 version`, `uint16 pad`, followed by `tydiskvaluedata_v7` (aligned for BE64 fields).
