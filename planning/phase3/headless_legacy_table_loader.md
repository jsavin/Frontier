# Headless Legacy Table Loader Plan

## Purpose
Capture the concrete steps required to decode legacy (Pascal-style) table payloads when running the headless Frontier runtime. This document complements the broader `pascal_runtime_modernization.md` note by focusing on the immediate loader work.

## Current status (2025-10-20)
- Header migration is fixed; `views[0]` now points to the correct root block in `Frontier-v7.root`.
- Hydration still crashes inside `tableunpacktable` because the payload returned from disk is in the legacy Pascal format rather than the merged-handle layout the modern code expects.
- A prototype shim in `tableexternal_common.c` can detect legacy payloads and allocate a replacement handle, but the conversion logic is incomplete.

## Short-term goals
1. **Reverse-engineer the legacy layout**
   - Document the byte-level structure of `tydisktablerecord` + `tydisksymbolrecord` + Pascal string pool emitted by `hashpacktable`.
   - Identify how offsets into the string pool are represented (Pascal length byte vs. classic handles).
2. **Implement a faithful converter**
   - Build helper routines to parse the legacy block and synthesise correct `hrecords` and `hstrings` buffers.
   - Reproduce `mergehandles` semantics so the merged handle matches what desktop Frontier would have produced.
3. **Add diagnostics and tests**
   - Instrument the conversion with sanity checks (record count, string bounds, sentinel validation).
   - Add unit/integration coverage that feeds a known legacy block through the converter and into `tableunpacktable`.

## Open questions / future work
- How many other payload types (menus, outlines, etc.) rely on the same layout? Enumerate once tables are handled.
- Decide whether to keep the converter headless-only or factor it into a shared migration utility.
- Once stable, consider writing a one-off tool to normalise existing `.root` files to the modern layout.

*This is a living document; update as the loader prototypes evolve.*

