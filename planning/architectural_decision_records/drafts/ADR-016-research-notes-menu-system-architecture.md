# ADR-016 Research Notes: Frontier Menu System Architecture

**Date**: 2026-05-05
**Companion to**: `ADR-016-headless-menu-system-projection.md` (this directory)
**Status**: Findings only — no recommendations.

This document is the source-of-truth research backing ADR-016. It maps the full Frontier menu system across both the current `/Users/jake/dev/jsavin/Frontier/` tree and the legacy `/Users/jake/dev/tedchoward/Frontier/` tree. Every architectural claim in the ADR is cited from this document.

The format follows the 13 questions the investigation was scoped against. Where the two trees differ, both are shown. Where ambiguity remains, it's marked **OPEN QUESTION**.

---

## A. Menubar Construction: Launch Path to First Render

### A1. Entry Point: `shellinit()` to `shellinitmenus()` to `menubarinit()`

**Call chain (current tree, `/Users/jake/dev/jsavin/Frontier/`):**

1. **`shell.c:924-1110` — `shellinit()`** — Main application initialization
   - Calls `initmenusystem()` at line 983
   - Calls `shellinitmenus()` at line 993

2. **`shellmenu.c:285-328` — `shellinitmenus()`** — Resource-based menu initialization
   - Iterates `firstmainmenu` through `lastmainmenu` (`shellmenu.h:41-47`: applemenu=1, filemenu=2, editmenu=3, windowsmenu=4)
   - Calls `installmenu()` for each (line 296)
   - Calls `installhierarchicmenu()` for hierarchic menus (line 300)
   - Installs resource items: ApplMenu desk accessories, font menu fonts (lines 305-309)
   - Sets up parameter substitution via `visitmenuitems(&parammenuitem)` (line 322)
   - Disables all items initially via `visitmenuitems(&menudisablevisit)` (line 325)

3. **`menubar.c:1482` — `menubarinit()`** — Menu data structure initialization
   - Initializes the `menubarlist` global (line 68)

**Legacy comparison (`/Users/jake/dev/tedchoward/Frontier/`):**
- Same file structure and call chain
- Licenses changed (GPLv2 → MIT) but code flow identical
- No `FRONTIER_HEADLESS` conditionals

### A2. Hard-Coded Menu Initialization

**Resource-driven menu loading:**

- File: `Common/source/shellmenu.c:228-262`
- Functions: `installmenu()` (line 228), `installhierarchicmenu()` (line 246)
- Mechanism: Calls `getresourcemenu(idmenu)` to load menus from resource fork, then:
  - `insertmenu(h, insertatend)` for main menus
  - `inserthierarchicmenu(h, idmenu)` for submenus (font, size, style, leading, justify)
- Hard-coded menu IDs in `shellmenu.h:38-62`:
  - Main: applemenu=1, filemenu=2, editmenu=3, windowsmenu=4
  - Hierarchic: fontmenu=128, stylemenu=129, sizemenu=130, leadingmenu=131, justifymenu=132

**DrawMenuBar() call:**
- Implicit via `meupdatemenubar()` at `menubar.c:293-298`, which calls `drawmenubar()`
- Triggered when `fldirtymenubar` flag set (line 287 in `medirtymenubar()`)

---

## B. MBAR Objects: Type, Structure, Serialization

### B1. In-Memory Representation

**Type code:** Not defined as a separate OSType; uses UserLand outline-based storage.

**In-memory struct (`menueditor.h:142-193`):**

```c
typedef struct tymenurecord {
    hdloutlinerecord menuoutline;   /* the display of menubar structure */
    dbaddress adroutline;            /* where stored in database */
    hdlmenubarstack hmenustack;      /* menubar.c's data structure */
    Rect menuwindowrect;             /* editor window position */
    short menuactiveitem;            /* which text item active */
    short menuactivelayer;           /* which layer active */
    WindowPtr scriptwindow;          /* script editor window, might be nil */
    Rect scriptwindowrect;           /* script window position */
    ...
    boolean fldirty;                 /* changes since save */
    boolean flinstalled;             /* explicitly installed in menubar */
} tymenurecord, **hdlmenurecord;
```

**Nested stack structure (`menubar.h:72-89`):**

```c
typedef struct tymenubarstack {
    hdlmenubarstack hnext;           /* linked list */
    hdloutlinerecord menubaroutline; /* outline at one w/menubar */
    boolean flactive;
    boolean flclientowned;           /* actual menus owned by menu sharing client */
    short ixdeletedmenu;
    short topstack;
    long refcon;
    tymenubarstackelement stack[ctmenubarstack];  /* max 50 menus */
} tymenubarstack;
```

**Menu bar element:**

```c
typedef struct tymenubarstackelement {
    hdlmenu hmenu;                   /* Menu Manager data structure */
    short idmenu;                    /* Menu Manager id */
    hdlheadrecord hnode;             /* structure that created this menu */
    boolean flhierarchic;            /* is it a sub-menu */
    boolean flenabled;
    boolean flbuiltin;               /* belongs to Frontier app */
    short ctbaseitems;               /* if builtin, # already there */
} tymenubarstackelement;
```

### B2. On-Disk Serialization

**File:** `menueditor.c:80-103` — `tysavedmenuinfo`

- Saved version number, outline address, scrollbar state, script window rect
- Expanded in v7 to use 64-bit cursor position (line 94)
- Total size: 116 bytes (static assert at line 107)

**Packing/unpacking:**
- `menuverbs.c:313-342` — `menuverbmemorypack()` — packs MBAR into Handle via `mesavemenurecord()`
- `menuverbs.c:345-377` — `menuverbmemoryunpack()` — unpacks Handle back to `hdlmenurecord`

### B3. Where MBAR Objects Live

**ODB location:**
- Typically at `system.menus.<menuname>` (referenced in `tablestructure.c` comment about "system.menus" created in 5.0a16)
- MBAR objects are external variables wrapping outlines
- Outline itself stored as database record (dbaddress in `adroutline`)

**Editor:**
- No separate MBAR editor; uses the outline editor on menu record
- Script attached to each node via `linkedscript` in refcon (`menueditor.h:212`)

**Construction paths:**
- Manual: outline editor on a menu record variable
- Scripted: `menu.install(hmenuvariable)` verb (`menuverbs.c:2016`)
- Programmatic: `menu.addMenuCommand()` verb (`menuverbs.c:2042`)

---

## C. Menu Construction Sources: Complete Enumeration

### C1. Source 1: Hard-Coded C-Side Resource Menus

- Files: `shellmenu.c`, `shellmenu.h`
- What: Apple, File, Edit, Window menus + Font, Size, Style, Leading, Justify submenus
- When: `shellinitmenus()` at startup (`shell.c:993`)
- Control: Menu IDs hardcoded in `shellmenu.h:38-62`; items from resource fork
- Status: Identical across current and legacy trees

### C2. Source 2: MBAR Objects from ODB

- Files: `menuverbs.c`, `menubar.c`, `menueditor.c`
- What: Dynamically-built menus from outline-based records
- When: `menu.install(hmenuvariable)` (`menuverbs.c:1186`); `meinstallmenubar()` (`menubar.c:1501-1513`)
- How:
  1. Load MBAR record into memory via `menuverbinmemory()` (`menuverbs.c:209`)
  2. `mebuildmenubar()` (`menubar.c:930`) walks outline and creates menus
  3. `meinsertmenubar()` attaches to menu manager (`menubar.c:1501`)
  4. Mark as `flinstalled` in record
- Control: outline structure determines menu tree; refcon stores command key + linked script

### C3. Source 3: Suites Mechanism

- File: `langipcmenus.c`
- What: Inter-process menu sharing from plugin/scripted hosts
- Structure: `langipcmenus.c:49-58` — `tymenulistrecord` — tracks per-app menu outline + stack
- When:
  - `langipcinstallmenus()` (`langipcmenus.c:562`) — called when external app registers
  - Builds stack via `buildmenubarstack()` (`langipcmenus.c:229`)
  - Activates via `langipcactivatemenus()` (`langipcmenus.c:606`)
- Control: outline transmitted from external app; can be dynamic

### C4. Source 4: Scripted Dynamic Menu Installation

- Implicit mechanism: UserTalk scripts call `menu.*()` verbs to build outlines
- Pattern: script creates outline under `system.menus.myapp.myname`, calls `menu.install()`, menu appears in menubar

### C5. Source 5: Hard-Coded Edit Menu Adjustments

- File: `shellmenu.c:787-816` — `shelladjustundo()`
- What: Undo item label + enable/disable status
- Mechanism: called from `shelladjustmenus()` (`:1358`); queries `(**shellwindowinfo).hundostring`

### C6. Source 6: File/Edit Menu Scripted Items (Pike pattern)

- Files: `shellmenu.c:845-942` (Pike-specific code)
- What: File/Edit menu items managed by UserTalk scripts
- Mechanism (Pike only):
  - `pikesetfilemenuitemenable()` (`:879`) — calls script `pike.isFileMenuItemEnabled("itemname")`
  - `pikesetfilemenuitemchecked()` (`:847`) — calls script `pike.isFileMenuItemChecked("itemname")`
- Scripts embedded in resources or `system` table (`:865`: `idpikeisfilemenuitemcheckedscript`)

### C7. Source 7: Window Menu (Per-Window Type)

- File: `shellwindowmenu.c` (8235 bytes)
- Mechanism: `shellupdatewindowmenu()` (prototype `shellmenu.h:304`); dynamically populated with open windows

### C8. Source 8: Font, Size, Style, Leading, Justify Submenus

- File: `shellmenu.c:445-784`
- What: Context-sensitive checks based on selection
- Mechanism: `shellcheckfontsizestyle()` (`:741`) iterates checker functions
  - `shellfontmenuchecker()` (`:445`), `shellsizemenuchecker()` (`:470`), etc.
  - Enabled/disabled and checked based on `(**shellwindowinfo).selectioninfo` fields

### C9. Source 9: Help Menu

- Mechanism: Mac OS system help menu via `HMGetHelpMenu()` (`shellmenu.c:198`)

### C10. Source 10: Open Recent Menu

- File: `shellmenu.c:1349-1354`, `shellmenu.h:137`
- Mechanism: `shellupdateopenrecentmenu()` (`shellmenu.c:1394`); dynamically populated; menu id `openrecentmenu=137` (`shellmenu.h:58`)
- Status: Enabled on modern targets (`shellmenu.c:1367` comment)

### C11. Source 11: Dock Menu (macOS)

- File: `dockmenu.c`
- Mechanism: Event handler for `kEventAppGetDockTileMenu` (`shell.c:1097`); separate from main menubar

### C12. Source 12: Dialog Popup Menus

- File: `oppopup.c` (refs `mereducemenucodes` at `menubar.h:182`)
- Mechanism: Popup menus in dialogs can use menu formulas; `mereducemenucodes()` processes enable/check flags

### C13. Source 13: New Object Menu

- File: `shellmenu.h:135` — `newobjectmenu=135`
- Mechanism: `shellmenu.c:1344-1347` — checked/enabled in `shelladjustmenus()`

---

## D. Menubar Merge and Composition

### D1. Order of Composition

**File:** `menubar.c:393-466` — `meactivatemenus()` + stack structure

**Order (fixed):**
1. **Apple menu** — Always first (resource-based)
2. **File menu** — Always second (resource-based)
3. **Edit menu** — Always third (resource-based)
4. **Window menu** — Always fourth (resource-based)
5. **Hierarchic submenus** — Font, Size, Style, etc. (resource-based, `insertmenu(..., rightmainmenu)`)
6. **MBAR-based menus** — User-installed custom menus (`:430`: `insertmenu(item.hmenu, rightmainmenu)`)
7. **Help menu** — Last (system help, added by Carbon/macOS)

**Data structure anchor:** `menubardata` global points to active `hdlmenubarstack`; each stack contains up to 50 menu slots (`ctmenubarstack=50` in `menubar.h:50`).

### D2. Conflict Resolution

**No explicit conflict handling**: if two sources try to define "File" menu, the second silently overwrites or inserts alongside. Order depends on insertion order.

**Practical resolution**: hard-coded menus always come first (`shellinitmenus()` `:295-301`); MBAR menus added after (via outline walk in `meactivatemenus()` `:417-437`).

### D3. Merge Function

**Primary:** `mebuildmenubar()` — `menubar.c:930-989`

```c
boolean mebuildmenubar (hdlmenubarstack hstack) {
    // Walks outline structure in hstack
    // For each top-level node: creates menu via mebuildmenu()
    // Inserts into menubar via insertmenu() or inserthierarchicmenu()
    // Returns true on success
}
```

**Secondary:** `meactivatemenus()` — `menubar.c:393-466` — activates/deactivates stacks when switching contexts.

---

## E. Menubar Adjust / Dynamic Update Path

### E1. Trigger Points

**File:** `shellmenu.c:819-830` — `shellforcemenuadjust()` + `shellupdatemenus()`

**When called:**
1. Idle loop via `shellupdatemenus()` (`:1364-1396`)
2. Process callback after menu item selected (`meprograms.c:307`: `(**hp).processkilledroutine = &meprocesscallback`)
3. Explicit calls from `menubar.c:289` — `medirtymenubar()` after structural changes
4. Modal dialog entry at `shellmenu.c:825`

### E2. State Machine

**File:** `shellmenu.c:59-65`

```c
static tymenustate menustate = dirtymenus;
// Enum: dirtymenus, normalmenus, optionmenus, modaldialogmenus
```

- `shellforcemenuadjust()` sets to `dirtymenus`
- `shellupdatemenus()` checks option key and transitions state
- `shelladjustmenus()` executes all adjustments if state changed

### E3. Primary Adjustment Function

**File:** `shellmenu.c:946-1361` — `shelladjustmenus()`

**What it does:**

1. **Window context detection (`:972-1029`)**
   - Checks if `shellwindow != nil`
   - Gets root window info
   - Sets up `tyselectioninfo` from window selection

2. **File menu adjustments (`:1033-1135`)**
   - Enables/disables: New, Open, Close, Save, Save As, Revert, Print, Quit
   - Changes "Save" label to "Save Database" if root window selected
   - Pike-specific: calls `pike.isFileMenuItemEnabled()` (`:1060`) + `pike.isFileMenuItemChecked()` (`:1085`)

3. **Edit menu adjustments (`:1141-1219`)**
   - Enables/disables: Undo, Cut, Copy, Paste, Clear, Select All, Find & Replace
   - Enables font/size/style/leading/justify submenus based on `x.flcansetfont`, etc.
   - Calls `shelladjustundo()` to update undo label

4. **Font/Size/Style/Leading/Justify submenus (`:1220-1355`)**
   - Calls individual checker functions for each item
   - Sets checkmark on currently-selected font/size/style/etc.

5. **Hook callback (`:1360`)**
   - `shellcallmenuhooks(0, 0)` — allows plugins to adjust their menus

**Pure-data adjustments (headless-feasible)**: enable/disable flags, checkmarks, label changes (e.g., "Undo Typing" vs "Undo Cut")

**Mac-UI-bound adjustments**: none that are strictly load-bearing; all are cosmetic for rendering.

---

## F. Menu Dispatch Path: User Click to Script Execution

### F1. Entry Point

**File:** `shellmenu.c:1399+` — `shellhandlemenu()`

**Call chain:**
1. Mac event arrives at `shellprocessevent()`
2. Menu Manager returns `menucode` (HiWord = menu id, LoWord = item index)
3. `shellhandlemenu(menucode)` called

### F2. Dispatch Table for Hard-Coded Menus

**Apple menu (`:1441-1461`):**
- Item 1: `aboutcommand()` (hard-coded)
- Items 2+: Desk accessories via `shellapplemenu()` (`:1455`)

**File menu (`:1464-1544`):**
- Items handled by kernel: New, Open, Close, Save, Save As, Print, Quit
- Other items: call `runfilemenuscript()` (`:1543`) or Pike script

**Edit menu (`:1545+`):**
- Calls `shelleditcommand()` (via `runeditmenuscript()`) → dispatches to undo/cut/copy/paste/clear/selectall

**Window menu:** `shellwindowmenuselect()` (prototype `shellmenu.h:306`)

**Help menu:** Help menu manager handles internally.

### F3. MBAR-Based Menu Dispatch

**File:** `menubar.c:1450-1500` (implied) or via IPC mechanism

**Mechanism:**
1. `memenu()` or `memenuhit()` looks up node from menu id + item index
2. Retrieves script handle from node's refcon via `megetnodelangtext()` (`meprograms.c:183-216`)
3. Calls `meuserselected()` (`meprograms.c:256`)

### F4. Script Execution

**File:** `meprograms.c:256-315` — `meuserselected()`

**Call chain:**
1. Load script from node via `megetnodelangtext()` (`:282`)
2. Parse script via `scriptbuildtree()` (`:288`)
3. Create process via `newprocess()` (`:297`)
4. Set process callback to `meprocesscallback` (`:307`) — calls `shellforcemenuadjust()` on death (`:249`)
5. Add to run queue via `addprocess()` (`:312`)

### F5. Mac-UI-Only Calls in Dispatch Path

| Call | Location | Classification |
|------|----------|----------------|
| `shellapplemenu()` | `shellmenu.c:1455` | Cosmetic; stub in headless |
| `shelleditcommand()` | `shellmenu.c` via `runeditmenuscript` | Load-bearing if cut/copy/paste involved; headless can skip |
| `shellforcemenuadjust()` | `meprograms.c:249,309` | Cosmetic; no-op in headless |
| `op_get_outlinedata()` | `meprograms.c:279` | Load-bearing; required for script lookup |
| Menu Manager hit test | Implicit in Mac event loop | Load-bearing but native to OS |

**Headless assessment:** script execution path works headlessly; only the trigger (user click) doesn't exist. Dispatch can be driven programmatically.

---

## G. Per-Window-Type Menubar Switching

### G1. Window Type Detection

**Mechanism:**
- Not explicitly visible in `menubar.c`; controlled via outline switching in `menueditor.c`
- Each window type's menubar likely stored separately in ODB (e.g., `system.menus.outline`, `system.menus.table`)
- `shellforcemenuadjust()` is context-sensitive based on `shellwindowinfo` globals

### G2. Switching Trigger

**Implied trigger:** When window type changes (text→outline→database), different `menudata` variable activated.

**Data structure:** `menubarlist` — linked list of `hdlmenubarstack` (one per active window type or app context).

### G3. Window-Type-Specific Adjustments

**File:** `shellmenu.c:946-1361` — `shelladjustmenus()`

**Context-sensitive enabling (`:1020-1029`):**
```c
if (flwindow) {
    getrootwindow (w, &hrootinfo);
    shellcheckfontsizestyle ();
    x = (**shellwindowinfo).selectioninfo;
}
```

**Result:** Font, Size, Style menus only enabled if window supports text formatting (checked via `x.flcansetfont`, etc.)

### G4. Dispatcher

**Likely location:** `meactivatemenus()` (`menubar.c:393`) — manages which stack is active.

**OPEN QUESTION:** Exact window-type-to-menubar mapping not explicit in code; likely controlled by outline variable naming convention (`system.menus.<type>`) or explicit switch in window management layer (not in menu code).

---

## H. Menu Vocabulary: All Verbs and Tables

### H1. Menu Token Verbs (`tymenutoken` enum)

**File:** `menuverbs.c:71-101`

| Token | Verb Name | Signature | Headless Status |
|-------|-----------|-----------|-----------------|
| 0 | zoomscriptfunc | `menu.zoomscript()` | No-op (GUI-only) |
| 1 | buildmenubarfunc | `menu.buildmenubar()` | Returns false (`:1990`) |
| 2 | clearmenubarfunc | `menu.clearmenubar()` | Returns true (`:2003`, no-op) |
| 3 | isinstalledfunc | `menu.isinstalled(path)` | Works (`:2010-2014`) |
| 4 | installfunc | `menu.install(hmenuvar)` | Returns false (`:2020`) |
| 5 | removefunc | `menu.remove(hmenuvar)` | Returns false (`:2033`) |
| 6 | getscriptfunc | `menu.getscript(path)` | Works (`:2142`) |
| 7 | setscriptfunc | `menu.setscript(path, script)` | Works (`:2147`) |
| 8 | addmenucommandfunc | `menu.addmenucommand(path)` | Works (`:2042`) |
| 9 | deletemenucommandfunc | `menu.deletemenucommand(path)` | Works (`:2048`) |
| 10 | addsubmenufunc | `menu.addsubmenu(path)` | Works (`:2054`) |
| 11 | deletesubmenufunc | `menu.deletesubmenu(path)` | Works (`:2060`) |
| 12 | getcommandkeyfunc | `menu.getcommandkey(path)` | Works (`:2152`) |
| 13 | setcommandkeyfunc | `menu.setcommandkey(path, key)` | Works (`:2157`) |

**Registration:** `menuinitverbs()` at `:2177` calls `loadfunctionprocessor(idmenuverbs, &menufunctionvalue)`.

### H2. Verb Subcategories

- **Editor-window-required (`:2078-2093`)**: zoomscript, getscript, setscript, getcommandkey, setcommandkey — fail in headless (no `langfindtargetwindow()`); `:2069-2076` returns early with false
- **GUI-only headless stubs (`:1988-2040`)**: buildmenubar, clearmenubar, install, remove
- **Data-manipulation only (work headlessly)**: isinstalled, addmenucommand, deletemenucommand, addsubmenu, deletesubmenu

### H3. Shell Menu Verbs (`shellmenu.c`)

| Function | Location | Purpose |
|----------|----------|---------|
| `shellinitmenus()` | `:285` | Load resource menus |
| `shellupdatemenus()` | `:1364` | State machine for menu adjust |
| `shelladjustmenus()` | `:946` | Execute all adjustments |
| `shellforcemenuadjust()` | `:819` | Mark menus dirty |
| `shelladjustundo()` | `:787` | Update undo label |
| `shellhandlemenu()` | `:1399` | Dispatch menu selection |
| `shelleditcommand()` | `:389` | Dispatch edit menu commands |

### H4. System Tables: `system.menus.*` Namespace

- `tablestructure.c` comment: "5.0a16 dmb: last version created system.menus"

**Known subtables:**
| Table | Purpose | Location |
|-------|---------|----------|
| `system.menus.data` | Menu outlines for each menu bar | ODB (custom menu records) |
| `system.menus.handlers` | (Hypothetical) Script handlers for menu items | Not found in code; likely outline refcon |

**OPEN QUESTION:** Full namespace mapping not explicit; likely user-extensible and convention-based.

---

## I. Specific Menus: Hard-Coded vs Data-Driven Classification

### I1. Apple Menu
- **Classification:** Hard-coded C-side + dynamic DA list
- Hard-coded via resource fork (`shellinitmenus()` `:305`)
- Item 1: "About Frontier" → `aboutcommand()` (`shellmenu.c:1445`)
- Items 2+: Desk accessories via `installresitems(applemenu, 'DRVR')` (`:305`)

### I2. File Menu
- **Classification:** Hard-coded structure + scripted items (Pike) + kernel handlers
- Hard-coded resource fork: New, Open, Open Recent, Close, Save, Save As, Revert, Print, Quit
- Items 1-15 in `shellmenu.h:115-128` (non-Pike) or `:80-109` (Pike)
- Kernel-handled (Frontier): New, Open, Close, Save, Save As, Revert, Print, Quit (`shellmenu.c:1506-1534`)
- Script-handled (Frontier): others via `runfilemenuscript()` (`:1543`)
- Script-handled (Pike): `pike.isFileMenuItemEnabled()` (`:1060`), `pike.isFileMenuItemChecked()` (`:1085`)

### I3. Edit Menu
- **Classification:** Hard-coded structure + kernel handlers + scripted for Pike
- Hard-coded resource fork: Undo, Cut, Copy, Paste, Clear, Select All, Find & Replace
- Items in `shellmenu.h:161-176` (non-Pike)
- Kernel: Undo (via `shelladjustundo()` + `shelleditcommand(undocommand)`); Cut, Copy, Paste, Clear, Select All
- Script (Pike): via `runeditmenuscript()` (`shellmenu.c:1545`)

### I4. Window Menu
- **Classification:** Data-driven (dynamically populated)
- Hard-coded menu ID `windowsmenu=4` (`shellmenu.h:44`)
- Dynamic items: list of open windows
- Populated by `shellupdatewindowmenu()` (prototype `shellmenu.h:304`)

### I5. Help Menu
- **Classification:** System-provided (Mac OS / Carbon)
- `HMGetHelpMenu()` (`shellmenu.c:198`)

### I6. Suites Menu (Plugins)
- **Classification:** Data-driven, IPC-managed
- `langipcinstallmenus()` (`langipcmenus.c:562`)
- Each plugin app provides outline via IPC; stack built via `buildmenubarstack()` (`:229`)

### I7. Tools / Apps Menu
- **Classification:** Likely data-driven (script-installed)
- Expected: user/startup script creates outline under `system.menus.tools` or similar; installs via `menu.install()`
- Not explicitly handled in `shellmenu.c`; likely user-installed MBAR

### I8. Font Menu
- **Classification:** Hard-coded structure + system-provided items
- Menu ID `fontmenu=128` (`shellmenu.h:49`)
- Items added via `installresitems(fontmenu, 'FONT')` (`shellmenu.c:308`)
- Adjustment: `shellfontmenuchecker()` (`:445`)

### I9. Size Menu
- **Classification:** Hard-coded structure + adjustment
- Menu ID `sizemenu=130` (`shellmenu.h:51`)
- Items 1-6: 9, 10, 12, 14, 18, 24 pt (`shellmenu.h:200-209`)
- Items 8-9: Up, Down (scaling); Item 11: Custom size
- Adjustment: `shellsizemenuchecker()` (`:470`)

### I10. Style Menu
- **Classification:** Hard-coded
- Menu ID `stylemenu=129` (`shellmenu.h:50`)
- Items: Plain, Bold, Italic, Underline, Outline, Shadow, Superscript, Subscript (`:188-199`)
- Adjustment: `shellstylemenuchecker()` (`:584`)

### I11. Leading Menu
- **Classification:** Hard-coded
- Menu ID `leadingmenu=131` (`shellmenu.h:52`)
- Items: 0, 1, 2, 3, 4, 5, Custom (`:212-218`)
- Adjustment: `shellleadingmenuchecker()` (`:647`)

### I12. Justify Menu
- **Classification:** Hard-coded
- Menu ID `justifymenu=132` (`shellmenu.h:53`)
- Items: Left, Right, Center, Full (`:221-224`)
- Adjustment: `shelljustifymenuchecker()` (`:701`)

### I13. Open Recent Menu
- **Classification:** Data-driven (runtime history)
- Menu ID `openrecentmenu=137` (`shellmenu.h:58`)
- Populated by `shellupdateopenrecentmenu()` (`shellmenu.c:1394`)
- Items call `runopenrecentmenuscript()` (prototype `shellmenu.h:302`)

---

## J. Cross-Tree Differences (Current vs Legacy)

### J1. Identical Across Both Trees

**Structural code:**
- Menu dispatch logic (`shellhandlemenu()`, `shelladjustmenus()`)
- Outline-based MBAR structures (`menueditor.h`, `menubar.h`)
- Resource-based hard-coded menus (`shellmenu.c` core lines 228-328)
- IPC menu sharing (`langipcmenus.c`)

**License only change:**
- Legacy: GPLv2 header (`tedchoward/Frontier/Common/source/menuverbs.c:1-33`)
- Current: MIT header + added includes for logging, db_format (`jsavin/Frontier/Common/source/menuverbs.c:1-57`)

### J2. Current Tree Additions (Headless Support)

**`menuverbs.c` — 166 added lines (2517 vs 2351 total)**

1. **Database context awareness (`:165-206`)**
   - `menuverbinmemory_context()` accepts explicit `db_context *ctx`
   - Detects v6 (legacy) vs v7 (64-bit) database format
   - Logs format detection

2. **Pack/unpack context threading (`:380-464`)**
   - `menuverbpack_internal()` — new signature with `db_context *ctx`
   - Maintains `db_format_mode` state machine
   - Adapter repack detection for format migration

3. **Headless stubs (25 locations marked `#ifdef FRONTIER_HEADLESS`)**:
   - `buildmenubarfunc` returns false (`:1990`)
   - `clearmenubarfunc` returns true no-op (`:2003`)
   - `installfunc` returns false (`:2020`)
   - `removefunc` returns false (`:2033`)
   - Editor-window verbs early-exit with false (`:2075`)

4. **Logging (`:181-189, 223-226, 414-417`)**: debug logging for context switches, format detection, pack ops

### J3. No Significant Semantic Changes

Both trees: same menu verb registration, same dispatch path, same adjustment logic, same window-type switching (if any).

**Conclusion:** Current tree is legacy + database context migration + headless stubs, not a rearchitecture.

---

## K. Headless Feasibility Classification

### K1. Pure Data Manipulation (Works Headless, No Change Needed)

| Component | Location | Reason |
|-----------|----------|--------|
| `menu.addmenucommand(path)` | `menuverbs.c:2042` | Only modifies outline/ODB |
| `menu.deletemenucommand(path)` | `menuverbs.c:2048` | Only modifies outline/ODB |
| `menu.addsubmenu(path)` | `menuverbs.c:2054` | Only modifies outline/ODB |
| `menu.deletesubmenu(path)` | `menuverbs.c:2060` | Only modifies outline/ODB |
| `menu.getscript(path)` | `menuverbs.c:2142` | Returns script text from outline |
| `menu.setscript(path, script)` | `menuverbs.c:2147` | Writes script to outline |
| `menu.getcommandkey(path)` | `menuverbs.c:2152` | Reads refcon data |
| `menu.setcommandkey(path, key)` | `menuverbs.c:2157` | Writes refcon data |
| `menu.isinstalled(path)` | `menuverbs.c:2010` | Reads menubarlist linked list |
| Script execution path | `meprograms.c:256-315` | Lookup + compile + execute, no UI |
| Outline-based menu structure | `menueditor.h` | In-memory data structure |
| MBAR packing/unpacking | `menuverbs.c:313-377` | Serialization, no UI |

### K2. Pure Data Manipulation But Currently No-Op'd (Needs Implementation)

| Component | Location | Current | Status |
|-----------|----------|---------|--------|
| `menu.buildmenubar()` | `menuverbs.c:1984-1995` | Returns false (`:1990`) | Could work headlessly |
| `menu.clearmenubar()` | `menuverbs.c:1997-2008` | Returns true (no-op) | Could work headlessly |
| `menu.install(hmenuvar)` | `menuverbs.c:2016-2027` | Returns false (`:2020`) | Could work headlessly if refactored |
| `menu.remove(hmenuvar)` | `menuverbs.c:2029-2040` | Returns false (`:2033`) | Could work headlessly if refactored |

These call `meinstallmenubar()` → `meinsertmenubar()` → `insertmenu()` (Mac Menu Manager). The outline modification step could be extracted.

### K3. Calls Mac UI But Cosmetic (Can Be Stubbed)

| Component | Location | Headless Alternative |
|-----------|----------|----------------------|
| `shellforcemenuadjust()` | `shellmenu.c:819` | No-op (just sets flag) |
| `shellupdatemenus()` | `shellmenu.c:1364` | Call `shelladjustmenus()` but skip `drawmenubar()` |
| Font/Size/Style/Leading menu checks | `shellmenu.c:741-783` | No-op (only affects rendering) |
| Edit menu label "Undo Typing" → "Undo Cut" | `shellmenu.c:787-816` | Set label but don't render |
| Window menu population | `shellmenu.c` (unknown) | Skip drawing but update list |
| Dock menu | `dockmenu.c` | Skip entirely |

### K4. Calls Mac UI and Load-Bearing (Needs Sibling Headless Function)

| Component | Location | Why Load-Bearing | Headless Solution |
|-----------|----------|------------------|-------------------|
| Menu dispatch event loop | `shell.c` (main event loop) | User interaction | REPL/CLI command parser |
| `shellapplemenu()` | `shellmenu.c:340` | Opens desk accessory | Stub; no DAs in headless |
| `shelleditcommand()` | `shellmenu.c:389` | Calls undo/cut/copy/paste | Implement separate for headless |
| Edit command dispatchers | `shell*.c` (window-specific) | Calls window manager | Abstract to headless layer |
| Resource menu loading | `shellmenu.c:228-262` | Calls `getresourcemenu()` | Pre-load resource data or hardcode |

### K5. Fundamentally GUI (Never Headless)

| Component | Reason |
|-----------|--------|
| Menu Manager hit-testing | Requires mouse coordinates + rendered menubar |
| Window menu population (dynamic) | Requires window list management (could be abstracted) |
| Context-sensitive menu enable/disable rendering | Could be abstracted to data, but not in menu code |
| Font menu system resource iteration | Possible to abstract; currently implicit |

---

## L. Minimum Viable Headless Menu System

### L1. What Can Be Projected

**Data model:**
- Menu outline structure (hierarchy, script, command key)
- Script execution (lookup outline → compile → run)
- Enable/disable state (computed from window context, not rendered)
- Label text (dynamically computed)

**Concrete entry points (REPL-able):**
- `/menu <path>` — look up by path, print items
- `/menu <path> <item>` — execute script
- `/menu build <outline_var>` — parse outline into menu structure
- `/menu list` — print menus and items

### L2. What Must Remain Mac-UI-Bound

- **Hard-coded resource menus**: cannot be changed at runtime (resource fork tied to binary)
- **Actual menu rendering**: drawing menubar, mouse hit-testing, font/size/style cosmetic state
- **Window menu (dynamic list)**: feasible headlessly if window list abstracted to data; currently intertwined with Quickdraw

### L3. Deferred / Out of Scope

- REPL-specific menu hierarchy (would need design + implementation)
- Persistence of menu adjustments across sessions (could be added)
- Plugin menu discovery (IPC mechanism exists but GUI-bound)

---

## Open Questions Requiring User Clarification

1. **Window-type-to-menubar mapping (Q7)**: Is there an explicit table mapping window types (wptext, wpoutline, etc.) to menubar outlines in `system.menus`? Or is this handled by window management code outside menu module?

2. **`system.menus.*` full shape (Q9)**: What is the complete namespace under `system.menus`? Is it user-writable? Conventions for user-defined menus (e.g., `system.menus.tools.mytheme.mycommand`)?

3. **Suites vs Tools distinction (Q3/Q10)**: Are "Suites" and "Tools" menus two different mechanisms, or does "Tools" refer to the user-installed menu bar that uses the Suites IPC mechanism?

4. **Edit command routing (Q6)**: Dispatch path shows `shelleditcommand()` for hard-coded menus but `runeditmenuscript()` for Pike menus (`:1545`). Priority order, or override?

5. **Help menu origin (Q10)**: Help menu item count and content fixed by the system, or can Frontier customize it via script?

---

## Reference Map: File:Line Citations

**Key entry points:**
- `shell.c:924-1110` — Main init (`shellinit()`)
- `shell.c:993` — Menu system init call (`shellinitmenus()`)
- `shellmenu.c:285-328` — Resource menu loading (`shellinitmenus()`)
- `menubar.c:68-90` — Menu bar data structures (global `menubarlist`)
- `menubar.c:930-989` — Build menubar from outline (`mebuildmenubar()`)

**Menu dispatch:**
- `shellmenu.c:1399+` — Menu item selection handler (`shellhandlemenu()`)
- `meprograms.c:256-315` — Script lookup & execution (`meuserselected()`)

**Dynamic adjustment:**
- `shellmenu.c:946-1361` — Menu state adjustment (`shelladjustmenus()`)
- `shellmenu.c:1364-1396` — Update loop (`shellupdatemenus()`)

**Data structures:**
- `shellmenu.h:38-62` — Hard-coded menu IDs
- `menueditor.h:142-193` — Menu record struct (`tymenurecord`)
- `menubar.h:52-103` — Menu bar stack structs (`tymenubarstack`, `tymenubarlist`)

**Verbs:**
- `menuverbs.c:71-101` — Menu verb enum (`tymenutoken`)
- `menuverbs.c:1953-2174` — Menu verb dispatcher (`menufunctionvalue()`)

**Headless conditionals:**
- `menuverbs.c:1988, 2001, 2017, 2030, 2069` — 5 major stubs
- `menupack.c:1039-1169` — 13 data pack/unpack stubs
