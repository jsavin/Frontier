# Menubar Serialization Reference

Status
- State: In Progress
- Phase: Carbon Migration (UI Extraction)
- Last Updated: 2025-11-20
- Owner: Codex
- Notes: Documenting legacy menubar serialization for future UI refactor; keep sources list updated.

**Sources**: `Common/headers/menueditor.h`, `Common/source/menupack.c`, `Common/source/menuverbs.c`

## 1. High-Level Model

Menubar objects are stored as outlines:

1. The menubar outline (`hdloutlinerecord`) contains one head per menu item. Headline **text** drives the UI label. Special characters are evaluated by the legacy UI:
   - `"-"` becomes a separator line.
   - Leading `"("` greys out the item (disabled).
   - Leading `"!"` shows the item as checked/toggled.
2. Each headline may link to:
   - **Scripts**: stored via `tymenuiteminfo.linkedscript`. Double-click launches the script editor.
   - **Sub-menus**: represented by child nodes in the outline (recursively packed).
3. Global menubar state (window rectangles, default script font, etc.) lives in `tysavedmenuinfo`. This struct is serialized alongside the outline and re-applied when the menubar is edited.

On disk, the serialized payload consists of:

```
[tysavedmenuinfo (~112 bytes)][packed menubar outline][concatenated script outlines]
```

`mepackmenustructure()` (`Common/source/menupack.c:378`) builds this layout. Scripts are packed first (`mepackscriptvisit`), then appended to the packed menubar outline handle.

## 2. `tysavedmenuinfo` — Legacy vs Portable

- **Legacy (v1)** — stored every UI preference (scroll positions, script/menu window rects, default script font, cursor line). These fields leaked per-user state into shared roots and were the source of the 64-bit widening bugs.
- **Portable (v2)** — keeps only the data we actually need at runtime (`adroutline` plus the bitfield for `flautosmash`). Everything else is dropped, and a 1KB zero-filled slab is reserved for any future metadata.

| Field | Type | Notes |
| --- | --- | --- |
| `versionnumber` | `int16` | `1` for legacy saves, `2` for the portable header. |
| `adroutline` | `int32` | Database address of the packed menubar outline (big-endian). |
| `flags` | `int16` | Bitmask (`flautosmash_mask`, future bits). |
| `reserved[1024]` | `uint8[1024]` | Always zero. |

Migrating a v6/v7 root now reads the v1 struct, copies just `adroutline`/`flags`, and immediately rewrites it as v2 so all new `.root` files share the lean layout.

## 3. Menu Item Refcons (`tymenuiteminfo`)

Each outline node’s `hrefcon` stores a fixed-size record describing the command key and linked script. Previously we copied the in-memory struct (which included raw pointers), leading to 64-bit corruption. We now serialise through a dedicated disk struct:

```c
typedef struct {
    uint8_t  cmdkey;
    uint8_t  cmdmodifiers;
    int32_t adrlink;          // database address of packed script outline
    int32_t outline_reserved; // must be zero (future use)
} tymenuiteminfo_disk;
```

- `megetmenuiteminfo()` (`Common/source/menupack.c:59-72`) reads the disk struct, swaps `adrlink`, and clears `linkedscript.houtline`.
- `mesetmenuiteminfo()` writes back the disk struct with `adrlink` swapped to big-endian.
- `tylinkeditem` in memory still tracks both `adrlink` and `houtline`; only the address is persisted.

### Script Packing Flow

1. `mepackscriptvisit()` iterates each node, packing its linked script outline with `oppackoutline()` if the script is in memory, otherwise copying the existing packed script at `linkedscript.adrlink`.
2. Handles are appended to `packinfo.hpackedscripts`.
3. After the menubar outline is packed, `mergehandles()` concatenates the outline and script handle arrays, so `meunpackmenustructure()` can rebuild outline + scripts in one pass.

## 4. Headless Adjustments (2025)

- `tymenuiteminfo_disk` ensures we only ever read/write 12 bytes per menu refcon; this mirrors the legacy 32-bit layout regardless of host pointer size.
- `tysavedmenuinfo` now uses fixed-width fields and zeroed reserved space, eliminating padding differences between 32- and 64-bit builds.
- Documentation is updated (`AGENTS.md`, this file) so future refactors don’t rely on `sizeof(long)` when touching menubar data.

## 5. Compatibility Notes & TODOs

1. **Legacy roots**: If a v7 root was produced by an older headless build, its menu refcons may already contain 64-bit garbage in `lenrefcon`. Re-running the migrator after these fixes will repack refcons, restoring the correct 12-byte disk layout.
2. **Script metadata**: The `outline_reserved` field is always zero today. If we need to store script hashes or metadata (e.g., to detect stale compiled caches), we can reuse this slot once both read/write paths understand the extension.
3. **Documentation gaps**: Menu command key semantics (modifiers, script linking) are codified in `menuverbs.c` but not fully described here. Future iterations should add a table covering modifier bits and how `menuverbs` consumes them.

## 6. References

- `Common/headers/menueditor.h`: `tysavedmenuinfo`, `tymenuiteminfo`, `tylinkeditem`.
- `Common/source/menupack.c`: pack/unpack logic, new disk struct, script visitors.
- `Common/source/menuverbs.c`: runtime verbs (`menu.setScript`, etc.) that rely on these structures.
