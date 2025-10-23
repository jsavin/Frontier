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
- **scriptvaluetype**: Script external metadata (outline structure + source text; compiled code NOT persisted in v7)
- **outlinevaluetype**: Outline/hierarchical data structures
- **wordvaluetype** (wptext): Rich text/word processing objects (32KB limit, 8-bit ASCII, Mac Toolbox format)
- **menuvaluetype**: Menu definitions
- **pictvaluetype**: Picture/image data
- **listvaluetype**: Arrays (can contain any value type including other arrays/records)
- **recordvaluetype**: Records/maps (can contain any value type including nested structures)

Each will need conversion logic for the v6→v7 migration. Complex types (arrays, records) are particularly challenging since they can recursively contain any other type.

## v7 Format Change: Script Storage

**Key Decision**: v7 format does NOT persist compiled code for scripts.

### What's Preserved
- Script external metadata (structure, timestamps, etc.)
- Source text (as outline data structure)
- Script outline hierarchy and attributes

### What's Dropped
- Compiled code (`codevaluetype` attachments)
- Compilation cache/bytecode

### Rationale
- JIT compilation is instant on modern hardware
- Source is canonical; compiled form is ephemeral cache
- Simplifies migration (one less format to convert)
- Already proven: frontier-cli compiles on-demand successfully
- Scripts recompile automatically when dirty/first-called

### Migration Strategy
When encountering scriptvaluetype in v6:
1. Extract script external structure (metadata + outline)
2. Preserve source text and outline hierarchy
3. **Discard** any attached compiled code
4. Let runtime compile on first execution in v7

## Short-term goals
1. **Survey actual types in Frontier.root**
   - Scan v6 database to identify which external types are actually present
   - Prioritize conversion work based on what's used in practice
2. **Reverse-engineer legacy layouts** (for types found)
   - Document byte-level structure for each external type
   - Identify merge patterns vs flat serialization
3. **Implement converters** (prioritized by usage)
   - Start with most common types
   - Handle recursive types (arrays, records containing externals)
4. **Add diagnostics and tests**
   - Unit tests for each converter
   - Integration tests using real v6 data

## Open questions / future work
- How many other payload types (menus, outlines, etc.) rely on the same layout? Enumerate once tables are handled.
- Decide whether to keep the converter headless-only or factor it into a shared migration utility.
- Once stable, consider writing a one-off tool to normalise existing `.root` files to the modern layout.

*This is a living document; update as the loader prototypes evolve.*

