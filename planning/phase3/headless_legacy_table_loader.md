# Headless Legacy Table Loader Plan

## Purpose
Capture the concrete steps required to decode legacy (Pascal-style) table payloads when running the headless Frontier runtime. This document complements the broader `pascal_runtime_modernization.md` note by focusing on the immediate loader work.

## Current status (2025-10-22)
- ✅ **Header migration fixed**: `views[0]` points correctly to root block in `Frontier-v7.root`
- ✅ **Table payload conversion implemented**: `tableexternal_common.c` detects legacy Pascal format `[header][strings][records]` and converts to modern two-level merged format `[outer_size][inner_merged][formats]`
- ✅ **System root loads successfully**: UserTalk scripts execute correctly with loaded system tables
- ✅ **Format documented**: Comprehensive documentation added to `docs/legacy_frontier_bootstrap.md`

### Next: Other External Value Types
The same legacy format issue affects other external types stored in v6 databases:
- **scriptvaluetype**: Compiled script objects
- **outlinevaluetype**: Outline/hierarchical data structures
- **wordvaluetype** (wptext): Rich text/word processing objects (32KB limit, 8-bit ASCII)
- **menuvaluetype**: Menu definitions
- **pictvaluetype**: Picture/image data

Each may need similar conversion logic when encountered during migration.

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

