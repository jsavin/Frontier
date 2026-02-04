# Menu Type v6 to v7 Migration Plan

**Status**: Planning
**Last Updated**: 2026-02-03
**Branch**: feature/menu-migration-fix

---

## 1. Overview

### Goal

Migrate `menubarType` objects from v6 database format to v7 format, simplifying the storage to just be an outline with table-based refcons. The v7 format discards unused GUI metadata (window positions, fonts, scroll state) and stores command key bindings and script references in a structured table format.

### Approach Summary

1. **Read v6 format**: Parse existing `tymenuiteminfo` structures from node refcons
2. **Transform**: Convert to v7 table-based refcon format
3. **Write v7 format**: Pack as v7 outline with table refcons

### Key Simplification

- **v6**: Complex `tymenuiteminfo` struct with byte-level packed fields, linked scripts via `dbaddress`
- **v7**: Each node refcon is a packed table with named fields (`keyBinding`, `modifiers`, `handlerScript`)

---

## 2. V6 Format Investigation

### 2.1 Menu Node Refcon Structure (`tymenuiteminfo`)

Location: `/Users/jake/dev/jsavin/Frontier-menu-migration-fix/Common/headers/menueditor.h` (lines 193-209)

```c
#pragma pack(2)
typedef struct tylinkeditem {
    dbaddress adrlink;           // Address of packed script on disk
    hdloutlinerecord houtline;   // In-memory handle (nil when on disk)
} tylinkeditem;

typedef struct tymenuiteminfo {  // Linked into hrefcon of each menu node
    byte cmdkey;                 // Command key character (e.g., 'S' for Cmd+S)
    byte cmdmodifiers;           // Modifier flags (bit field)
    tylinkeditem linkedscript;   // Script address/handle
} tymenuiteminfo;
#pragma options align=reset
```

**Total size**: 2 (cmdkey+modifiers) + 4 (adrlink) + 4/8 (houtline) = ~10-14 bytes packed

### 2.2 Modifier Flags (`tykeyflags`)

Location: `/Users/jake/dev/jsavin/Frontier-menu-migration-fix/Common/headers/shelltypes.h` (lines 132-144)

```c
typedef enum tykeyflags {
    keyshift   = 0x0001,  // Shift key
    keycontrol = 0x0200,  // Control key
    keyoption  = 0x0400,  // Option/Alt key
    keycommand = 0x0800   // Command key (Mac) / Windows key
} tykeyflags;
```

**Note**: Stored in `cmdmodifiers` byte, but constants are 16-bit. Only the lower byte is meaningful in the packed structure.

### 2.3 Where Refcons Are Read/Written

| Operation | File | Function | Notes |
|-----------|------|----------|-------|
| Read refcon | menupack.c | `megetmenuiteminfo()` | Calls `opgetrefcon()`, converts `adrlink` from BE |
| Write refcon | menupack.c | `mesetmenuiteminfo()` | Converts `adrlink` to BE, calls `opsetrefcon()` |
| Pack scripts | menupack.c | `mepackscriptvisit()` | Visits each node, packs attached scripts |
| Unpack scripts | menupack.c | `meunpackscriptvisit()` | Visits each node, unpacks scripts from handle |
| Release scripts | menupack.c | `mereleaserefconroutine_context()` | Disposes in-memory outline, releases db node |
| Copy refcon | menupack.c | `mecopyrefconroutine()` | Copies script outline for clipboard ops |

### 2.4 Menu Structure Storage

The menubar is stored as a two-level data structure:

1. **Menu metadata record** (`tysavedmenuinfo` / `tysavedmenuinfo_v7`)
   - Address of menu outline
   - Scroll position, window rects, fonts (v6 only)
   - Stored at the menu external's database address

2. **Menu outline**
   - Hierarchical outline where each node is a menu item
   - Level 0 = menu names ("File", "Edit", etc.)
   - Level 1+ = menu items and submenus
   - Each node's refcon contains `tymenuiteminfo`

3. **Script outlines** (per-node)
   - Separate packed outlines for each menu item's handler script
   - Referenced by `adrlink` in the node's refcon
   - Loaded on demand

### 2.5 V6 Disk Layout

```
Menu External (in parent table):
  [4 bytes: dbaddress of tysavedmenuinfo_disk_legacy]

tysavedmenuinfo_disk_legacy (at that address):
  [2 bytes: versionnumber]
  [4 bytes: adroutline (BE32)]
  [2 bytes: vertmin]
  [2 bytes: vertmax]
  [2 bytes: vertcurrent]
  [8 bytes: scriptwindowrect]
  [2 bytes: flags]
  [2 bytes: menuactivelayer]
  [2 bytes: lnumcursor]
  [32 bytes: defaultscriptfontname]
  [2 bytes: defaultscriptfontsize]
  [8 bytes: menuwindowrect]
  [42 bytes: waste/padding]

Menu Outline (at adroutline):
  [standard outline pack format - v2/v3/v4 header + text + linetable]
  Each node's linetable entry includes refcon length
  Refcons are tymenuiteminfo structures

Script Outlines (at each node's adrlink):
  [standard outline pack format]
```

---

## 3. V7 Format Specification

### 3.1 Design Goals

1. **Drop GUI metadata**: No font info, window positions, scroll state
2. **Structured refcons**: Replace binary `tymenuiteminfo` with table-based format
3. **Portable**: All fields in big-endian with explicit byte order helpers
4. **Extensible**: Table format allows adding new fields without format changes

### 3.2 V7 Menu Metadata (`tysavedmenuinfo_v7`)

Already implemented in menupack.c (lines 744-752):

```c
typedef struct tysavedmenuinfo_v7 {
    uint16_t versionnumber;     // 2 = v7 marker
    uint16_t _pad;              // Alignment
    uint64_t adroutline;        // BE64 address of menubar outline
    uint64_t lnumcursor;        // BE64 line number of selection
    uint32_t flags;             // autosmash flag, etc.
    uint32_t menuactiveitem;    // Active item enum
    uint8_t  reserved[1024];    // Future expansion
} tysavedmenuinfo_v7;
```

### 3.3 V7 Node Refcon Format

Each menu node's refcon becomes a **packed table** with these keys:

| Key | Type | Description |
|-----|------|-------------|
| `keyBinding` | char | Command key character (e.g., 'S') or 0 if none |
| `modifiers` | table | Sub-table with boolean keys: `shift`, `control`, `option`, `command` |
| `handlerScript` | script | The script object (outline type with script flag) |

**Example in UserTalk notation**:
```
{
  keyBinding: 'S',
  modifiers: {
    shift: false,
    control: false,
    option: false,
    command: true
  },
  handlerScript: script("msg(\"Hello\")")
}
```

**Storage format**: Standard packed table value (same format as table cell values).

### 3.4 V7 Disk Layout

```
Menu External (in parent table):
  [4/8 bytes: dbaddress of tysavedmenuinfo_v7]

tysavedmenuinfo_v7 (at that address):
  [2 bytes: versionnumber = 2]
  [2 bytes: _pad]
  [8 bytes: adroutline (BE64)]
  [8 bytes: lnumcursor (BE64)]
  [4 bytes: flags (BE32)]
  [4 bytes: menuactiveitem (BE32)]
  [1024 bytes: reserved]

Menu Outline (at adroutline):
  [v4 portable outline header]
  [text section]
  [linetable section]
  Each node's refcon is a packed table (langunpackvalue format)

Script Outlines (embedded in refcon tables):
  Stored as script values within each node's refcon table
```

---

## 4. Implementation Phases

### Phase 1: V6 Reader (Extract data from old format)

**Goal**: Create functions to fully materialize a v6 menu into memory with all scripts loaded.

#### 4.1.1 New Function: `meloadmenurecord_v6_full()`

```c
/**
 * Load a v6 menu with all scripts materialized into memory.
 * Unlike meloadmenurecord(), this loads ALL linked scripts,
 * not just on-demand.
 *
 * Used for migration to ensure complete data extraction.
 */
boolean meloadmenurecord_v6_full(const db_context *ctx, dbaddress adr,
                                  hdlmenurecord *hmenurecord);
```

**Implementation**:
1. Call existing `meloadmenurecord_internal()` with v6 context
2. Push menu outline
3. Visit all nodes with `opsiblingvisiter()`
4. For each node, call `meloadscriptoutline()` to force-load the script
5. Keep scripts in memory (set `fldirty = true` to prevent unload)

#### 4.1.2 New Function: `meextractmenuiteminfo_v6()`

```c
/**
 * Extract v6 menu item info into C structure.
 * Handles byte order conversion from disk format.
 */
boolean meextractmenuiteminfo_v6(hdlheadrecord hnode,
                                  byte *cmdkey,
                                  tykeyflags *modifiers,
                                  hdloutlinerecord *hscript);
```

**Implementation**:
1. Call `megetmenuiteminfo()` (existing)
2. Ensure script is loaded via `item.linkedscript.houtline`
3. Return decomposed fields

### Phase 2: V7 Writer (Create table refcons, pack as outline)

**Goal**: Convert in-memory menu to v7 format with table-based refcons.

#### 4.2.1 New Function: `mecreaterefcontable_v7()`

```c
/**
 * Create a v7 table refcon from v6 menu item data.
 * Returns a packed table handle ready to be set as node refcon.
 */
boolean mecreaterefcontable_v7(byte cmdkey, tykeyflags modifiers,
                                hdloutlinerecord hscript,
                                Handle *hpackedtable);
```

**Implementation**:
1. Create temporary in-memory hashtable
2. Set `keyBinding` = cmdkey (char value)
3. Create `modifiers` sub-table with:
   - `shift` = (modifiers & keyshift) != 0
   - `control` = (modifiers & keycontrol) != 0
   - `option` = (modifiers & keyoption) != 0
   - `command` = (modifiers & keycommand) != 0
4. If hscript != nil:
   - Create script external value from hscript
   - Set `handlerScript` = script value
5. Pack table using `langpackvalue()`
6. Return packed handle

#### 4.2.2 New Visitor Function: `meconvertrefcon_v6_to_v7_visit()`

```c
/**
 * Node visitor that converts v6 refcon to v7 table format.
 * Called during migration traversal.
 */
static boolean meconvertrefcon_v6_to_v7_visit(hdlheadrecord hnode, ptrvoid refcon);
```

**Implementation**:
1. Extract v6 data via `meextractmenuiteminfo_v6()`
2. If no refcon data, skip (return true)
3. Create v7 table via `mecreaterefcontable_v7()`
4. Dispose old refcon
5. Set new refcon via `opsetrefcon()`

#### 4.2.3 New Function: `mepackmenustructure_v7_new()`

Update existing `mepackmenustructure_v7()` to use table refcons:

1. Fill `tysavedmenuinfo_v7` header
2. Visit all nodes, converting refcons to v7 table format
3. Pack outline using `oppack()` (which packs refcons as-is)
4. Scripts are embedded in table refcons, not separate

### Phase 3: Update menuverb Functions

**Goal**: Make menu pack/unpack functions handle both formats correctly.

#### 4.3.1 Update `mepackmenustructure()`

Already dispatches based on `db_format_mode_current()`:
- v6: calls `mepackmenustructure_legacy()`
- v7: calls `mepackmenustructure_v7()`

**Change**: Ensure v7 path uses new table refcon logic.

#### 4.3.2 Update `meunpackmenustructure()`

Already dispatches based on format:
- v6: calls `meunpackmenustructure_legacy()`
- v7: calls `meunpackmenustructure_v7()`

**Change**: v7 path must unpack table refcons correctly.

#### 4.3.3 New Function: `meunpackrefcontable_v7()`

```c
/**
 * Unpack a v7 table refcon and extract menu item info.
 * Inverse of mecreaterefcontable_v7().
 */
boolean meunpackrefcontable_v7(Handle hpackedtable,
                                byte *cmdkey,
                                tykeyflags *modifiers,
                                hdloutlinerecord *hscript);
```

**Implementation**:
1. Unpack table via `langunpackvalue()`
2. Look up `keyBinding` → cmdkey
3. Look up `modifiers` sub-table → extract flags
4. Look up `handlerScript` → extract script outline
5. Return decomposed fields

### Phase 4: Testing

#### 4.4.1 Unit Tests

Create `tests/menu_v7_migration_tests.c`:

| Test | Description |
|------|-------------|
| `test_menu_refcon_roundtrip` | v6 refcon → v7 table → v6 refcon |
| `test_menu_empty_refcon` | Node with no script or keybinding |
| `test_menu_keybinding_only` | Node with Cmd+K but no script |
| `test_menu_script_only` | Node with script but no keybinding |
| `test_menu_full_refcon` | Node with script + all modifier combos |
| `test_menu_modifiers_encoding` | Test each modifier flag individually |
| `test_menu_nested_structure` | Menu with submenus |

#### 4.4.2 Integration Tests

Add YAML test cases in `tests/integration/test_cases/menu_migration/`:

```yaml
# test_menu_simple.yaml
name: "Simple menu migration"
script: |
  local (m = menu.new())
  menu.addCommand(m, "File", "Open", "dialog.alert(\"open\")")
  menu.setCommandKey(@m["File"]["Open"], 'O')
  return typeOf(m) == menuType
expected_result: true
```

---

## 5. File Changes

### 5.1 Files to Modify

| File | Changes |
|------|---------|
| `Common/source/menupack.c` | Add v7 refcon conversion functions, update v7 pack/unpack |
| `Common/source/menuverbs.c` | Update `menuverbinmemory_context()` for v7 table refcons |
| `Common/headers/menueditor.h` | Add function declarations for new v7 helpers |
| `tests/headless_menu_stubs.c` | Update stubs for v7 format |

### 5.2 New Files

| File | Purpose |
|------|---------|
| `tests/menu_v7_migration_tests.c` | Unit tests for refcon conversion |
| `tests/integration/test_cases/menu_migration/*.yaml` | Integration tests |

### 5.3 Detailed Changes to menupack.c

#### Add after line ~750 (after tysavedmenuinfo_v7 typedef):

```c
/**
 * V7 Table Refcon Format
 *
 * Each menu node's refcon is a packed table with these fields:
 * - keyBinding (char): Command key character or 0
 * - modifiers (table): {shift, control, option, command} booleans
 * - handlerScript (script): The attached script or nil
 */

boolean mecreaterefcontable_v7(byte cmdkey, tykeyflags modifiers,
                                hdloutlinerecord hscript,
                                Handle *hpackedtable);

boolean meunpackrefcontable_v7(Handle hpackedtable,
                                byte *cmdkey,
                                tykeyflags *modifiers,
                                hdloutlinerecord *hscript);

static boolean meconvertrefcon_v6_to_v7_visit(hdlheadrecord hnode, ptrvoid refcon);
```

#### Update mepackmenustructure_v7():

Replace the direct refcon packing with conversion visitor call before packing.

---

## 6. Test Cases

### 6.1 Data Migration Tests

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Empty menu | Menu with no items | Packs to v7 with empty outline |
| 2 | Single item, no script | One menu item, no keybinding | Table refcon with nil handlerScript |
| 3 | Single item with Cmd+K | Item with command key | keyBinding='K', modifiers.command=true |
| 4 | Item with Shift+Cmd+K | Complex modifier combo | All modifier flags set correctly |
| 5 | Item with script | Simple script attached | handlerScript contains script outline |
| 6 | Nested menu | File > Recent > item1, item2 | Correct outline hierarchy preserved |
| 7 | Round-trip v6→v7→v6 | Full menu structure | Data integrity preserved |

### 6.2 Edge Cases

| # | Edge Case | Test Strategy |
|---|-----------|---------------|
| 1 | Corrupt refcon | Handle gracefully, don't crash |
| 2 | Script load failure | Log error, continue with nil script |
| 3 | Very large script | Memory handling |
| 4 | Unicode menu item names | Preserved through migration |
| 5 | Maximum nesting depth | Stress test with deep submenus |

### 6.3 Regression Tests

- Ensure existing v6 databases still load correctly
- Ensure GUI builds can still edit menus (when applicable)
- Ensure script execution works from migrated menus

---

## 7. Implementation Notes

### 7.1 Memory Management

- Scripts loaded for migration must be disposed after packing
- Use `fldirty = true` to prevent premature unload during traversal
- Track and release all temporary tables created for refcons

### 7.2 Byte Order

- All v7 fields use explicit BE helpers (`db_format_write_be64`, etc.)
- Table packing uses standard `langpackvalue()` which handles byte order
- Refcon data stored as packed table bytes (platform-independent)

### 7.3 Backward Compatibility

- v7 reader must handle v6 databases (detect via versionnumber)
- v6 reader (in legacy Frontier) won't understand v7 format
- Migration is one-way (v6 → v7)

### 7.4 Error Handling

- Log all conversion errors with context
- Don't abort migration on single node failure
- Track and report error count at end

---

## 8. Risks and Mitigations

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Script data loss | Medium | High | Comprehensive testing, backup v6 before migration |
| Keybinding corruption | Low | Medium | Unit tests for all modifier combos |
| Memory exhaustion on large menus | Low | High | Incremental processing, release after each node |
| Table packing incompatibility | Low | Medium | Use existing langpackvalue(), well-tested |

---

## 9. Success Criteria

- [ ] All v6 menu features preserved in v7 format
- [ ] Unit tests pass for refcon conversion
- [ ] Integration tests pass for menu verbs
- [ ] Real-world menu data migrates correctly (Frontier.root menus)
- [ ] No memory leaks in conversion code
- [ ] GUI builds still work (when applicable)

---

## 10. References

- V6 to V7 Migration Gaps: `planning/phase3/v6_to_v7_migration_gaps.md`
- Outline Pack Format: `Common/source/oppack_v7.c`
- Refcon Handling: `Common/source/oprefcon.c`
- Menu Editor Header: `Common/headers/menueditor.h`
- Menu Pack Implementation: `Common/source/menupack.c`
- Headless Menu Stubs: `tests/headless_menu_stubs.c`
