# Legacy Outline & Script Payload Layout

Status
- State: In Progress
- Phase: Carbon Migration (Outline Serializer)
- Last Updated: 2025-11-20
- Owner: Codex
- Notes: Reference for legacy outline/script packing; keep synced with oppack.c while modernizing.

**Source of truth**: `../../tedchoward/Frontier/Common/source/oppack.c`

## Overview

Outlines and script bodies are serialized by `oppack()` (see legacy source lines ~200–520). The packed payload is written into an external handle that is then stored in the table record for either `outlinevaluetype` or `scriptvaluetype`. Scripts reuse the identical outline layout; the script flag lives in the surrounding `externalvariable` metadata, so the payload below applies to both objects.

Layout (in file order):

1. **`tyversion2diskheader` (~120 bytes, packed)**  
> 2025-11-15: All fields now use explicit 16/32-bit widths so the header is stable on 64-bit builds.

   Describes editor/UI state plus bookkeeping metadata. Fields (with QuickDraw/Carbon ties in *italics*):
   - `short versionnumber` – currently `2` (`opversionnumber`).
   - `uint32 sizelinetable`, `uint32 sizetext` – byte counts for the sections that follow. Always written big-endian via `memtodisklong`.
   - `short lnumcursor` + hiword – caret line number (split into 16-bit chunks for legacy Pascal code).
   - `linespacing`/`lineindent` – stored as 16-bit values; editor-only metadata.
   - `diskfontstring fontname`, `short fontsize`, `short fontstyle` – *QuickDraw font menu state*.
   - `short vertmin/max/current (+hiword)`, `short horizmin/max/current (+hiword)` – scroll positions.
   - `timecreated`, `timelastsave`, `ctsaves` – 32-bit Mac epoch values.
   - `fltextmode` – stored as a byte (non-zero = true).
   - `diskrect windowrect` – *QuickDraw window frame (top/left/bottom/right)*.
   - `long outlinesignature` – caller-defined cookie.
   - `RGBColor backcolor`, `forecolor` – *QuickDraw color refs* (written via `memtodiskshort`).
   - `OSType platform` – `'mac '` or `'win '`. Controls character mapping during unpacking.
   - `waste[3]` – reserved padding.

   **UI-only section**: `fontname/fontsize/fontstyle`, all scroll rectangles, `windowrect`, `forecolor/backcolor` are the fields we plan to drop (or zero) in headless serialization.

2. **Text block (`sizetext` bytes)**  
   Produced by `opoutlinetotext()`. Each headline is serialized as:
   - `level` tabs (or normalized spaces) → `writehandlestreamstring`.
   - Head text bytes (bigstring up to 255 chars) via `writehandlestreamhandle`.
   - Trailing carriage return (`0x0D`).
   Comments are included unless `flpackcomments` is false.

3. **Line table (`sizelinetable` bytes)**  
   Produced by `opoutlinetotable()` visiting every node. Sequence of `tylinetableitem` records:
   ```c
   typedef struct tylinetableitem {
       short flags;      // see below
       long  lenrefcon;  // big-endian byte count that follows
   } tylinetableitem;
   ```
   - Flags bitfield (`tylinetableitemflags`) encodes expansion state, lock/comment/breakpoint bits, etc. Bits are stored in platform byte order before being written; we must normalize to host endian when reading.
   - Immediately following each entry are `lenrefcon` raw bytes copied from the node’s refcon handle (`(**hn).hrefcon`). Refcons often store compiled script caches or outline-specific metadata; for scripts it is usually empty.

### Related helpers
- `opverbpack()` (legacy `Common/source/opverbs.c:746-875`) calls `oppack()` before writing to disk. When the outline had never been opened in this process, `(**hv).flinmemory` is false, so `opverbpack` must hydrate it before calling the packer.
- `opunpacktexttooutline()` rebuilds the hierarchy when loading. It reads the header to decide whether text needs character remapping (platform bit), then reconstructs the line table and text handles.

## QuickDraw / UI Fields to Drop
When creating the headless serializer we can omit or zero these header members without affecting runtime semantics:
- `fontname`, `fontsize`, `fontstyle`
- `windowrect`
- `vert*` / `horiz*` scroll info
- `linespacing`, `lineindent` (exposed only to the outline editor)
- `forecolor`, `backcolor`

Legacy Windows builds still serialized these fields (they were required for round-tripping through the Mac editor), so we can safely treat them as optional metadata in the new format.

## Portable Header Plan
Rather than writing legacy headers with zeroed QuickDraw fields, headless builds will emit a shortened “portable” header that keeps only runtime-relevant data:

| Field | Keep? | Notes |
| --- | --- | --- |
| `versionnumber` | ✅ | Bump to `4` for the portable layout. |
| `sizelinetable`, `sizetext` | ✅ | Required for unpacking. |
| `lnumcursor` (+hiword) | ✅ | Used by script editor logic; cheap to keep. |
| `linespacing`, `lineindent` | ❌ | Editor-only; drop entirely. |
| `fontname/fontsize/fontstyle` | ❌ | QuickDraw UI settings; omit. |
| Scroll info (`vert*`, `horiz*`) | ❌ | UI state. |
| `timecreated`, `timelastsave`, `ctsaves` | ✅ | True metadata referenced by verbs. |
| `fltextmode` | ✅ | Impacts outline interpretation. |
| `windowrect` | ❌ | UI positioning; drop. |
| `outlinesignature` | ✅ | Used by callers (e.g., script cache keys). |
| `forecolor/backcolor` | ❌ | UI-only. |
| `platform` | ✅ | Legacy scripts key off Mac vs Windows text quirks. |
| `reserved` (1024 B) | ✅ | New zeroed expansion area for future refcon/window metadata. |

Implementation sketch:
1. Define `tyoutlineportableheader` (likely ~32 bytes) containing the “keep” fields.
2. Update headless packer to write the new struct and tag the payload with a format byte so unpackers know whether they’re seeing legacy v2 or portable v3.
3. During migration, detect legacy outlines and rewrite them into the portable header immediately; no UI fields will remain in v7 files. The reserved block ships zeroed for now but gives us room to store future non-scalar metadata (e.g., binarytype blobs, per-window refcons) without another format bump.

## Migration Notes
- Scripts store compiled code in refcon blocks (`lenrefcon` bytes). For v7 we already decided to drop compiled chunks and recompile on demand, so the converter can skip copying refcon data or leave it empty.
- Node-level refcons (`(**hn).hrefcon`) persist as inline `lenrefcon` byte ranges in the line table; they’re typically small handles containing XML attribute tables, nodeType metadata, or other opaque binary blobs. We keep them untouched so script frameworks (e.g., `op.opToXml`, nodeType/windowType) continue to work once rehydrated.
- Outline-level refcons (`(**ho).outlinerefcon`) link the in-memory outline to its owning external variable/host window but were never stored on disk. The new reserved header block gives us space to persist future outline/window metadata so we no longer need hidden parent tables for windowType participation.
- The text + line table sections are platform-neutral once character translation is disabled (`pushdiskchar` no longer maps to MacRoman in headless modes).
- `tylinetableitem.flags` still encodes per-node UI state (expanded/locked/comment). We should keep these bits because they affect runtime traversal and script semantics (e.g., comments, breakpoints).
- Migration now always rewrites outlines/scripts into the portable header (option **b** above). Legacy v2 headers are accepted on read but never written by headless builds.

## Implementation Tasks (Headless Serializer)
1. **Header detection & parsing**
   - Extend `Common/source/opverbs.c` and `Common/source/langexternal.c` to detect the header version (`2` vs `3`) and dispatch to the appropriate unpack path.
2. **Portable packer**
   - Introduce `portable/op_pack_portable.c` (or similar) that walks `hdloutlinerecord` using the existing `opoutlinetotext`/`opoutlinetotable` helpers but writes the new `tyoutlineportableheader`.
   - Strip/discard legacy UI fields instead of copying them.
3. **Refcon trimming**
   - Ensure script outlines drop compiled refcon data during migration; regenerate refcons lazily on first execution.
4. **Migrator wiring**
   - Update `Common/source/db_format.c` so the v6→v7 converter invokes the portable packer whenever `outlinevaluetype`/`scriptvaluetype` appears; delete QuickDraw dependencies from the headless build.
5. **Tests**
   - Add fixtures under `tests/components/` that feed captured v6 outline/script payloads through the converter and verify the emitted portable format round-trips via `opunpack`.
6. **Docs**
   - Document the new header structure in `docs/legacy_frontier_bootstrap.md` and reference it from `_CURRENT_STATUS.md`.

## Outstanding Questions
- Do any runtime verbs inspect `fontname` or scroll positions? A cursory grep suggests they are editor-only, but confirm before stripping.
- Need to confirm whether `lenrefcon` ever stores non-UI data we must keep (e.g., table view filters). Inspect representative v6 outlines (`system.verbs.*`, `examples.*`) to make sure.
- Decide whether we normalize `platform` to `'unix'` / `'posi'` for future builds or just keep `'mac '`/`'win '` for compatibility.
