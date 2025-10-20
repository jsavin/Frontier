# Pascal-Era Data Modernization Plan

## Context
The original Frontier codebase was written for classic Mac OS (68k → PowerPC) and leans heavily on Pascal conventions:
- Pascal strings (length byte followed by inline data) are ubiquitous in on-disk tables.
- Many serialized structures are arranged as handle-based blocks (`mergehandles`/`unmergehandles`) rather than the modern "length + payload" buffers we expect today.
- Database records and table payloads embed Mac-specific layout assumptions (big-endian fields, Pascal string pools, handle offsets) that newer consumers find hard to decode.

Recent work on the headless loader highlighted these issues: while the v6 → v7 header migration now preserves correct pointers, the system table payload is still emitted in the legacy Pascal format. Our quick shim that attempts to reinterpret the payload as a modern merged-handle crashes because the string pool and record offsets follow the classic Pascal contract.

## Why this matters
- **Correctness:** Until the headless loader can decode the legacy Pascal layout, hydration fails inside `tableunpacktable`.
- **Maintainability:** Pascal-era assumptions are scattered through `langhash.c`, `tablepack.c`, and friends; without documentation they’re easy to break.
- **Modernisation path:** Any future format upgrade (UTF-8 strings, little-endian support, streaming) needs a clear translation layer from these legacy constructs.

## Proposed work
1. **Document the current layout.**
   - Capture the exact structure of `tydisktablerecord`, `tydisksymbolrecord`, and the Pascal string pool (`hashpacktable` output).
   - Annotate the headless shim (`tableexternal_common.c`) once the converter is in place so developers know which invariants must hold.

2. **Introduce helper utilities.**
   - Write shared functions to read/write Pascal strings and classic symbol records, rather than sprinkling byte math throughout the loader.
   - Add explicit tests that round-trip a legacy table block using these helpers.

3. **Long-term modernization.**
   - Once the headless loader is stable, plan a dedicated phase to migrate database payloads away from Pascal constructs (e.g., switch to length-prefixed UTF-8, replace handle-based streams with simple buffers).
   - Provide tooling/scripts to rewrite older `.root` files if we decide to change the on-disk format.

## Immediate next step
Focus the current headless work on accurately reconstructing the legacy payload (Pascal strings + records) before calling `tableunpacktable`. Track implementation details in `planning/phase3/headless_legacy_table_loader.md` and update the plan as the converter takes shape.
