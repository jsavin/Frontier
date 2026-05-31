# Legacy Menu System: Architecture + Headless Port Roadmap

**Audience**: internal technical (Claude, JES, future agents). Investigation produced from reading `/Users/jake/dev/tedchoward/Frontier/Common/source/` and cross-referencing against the headless port work in `/Users/jake/dev/jsavin/Frontier/`.

**Status**: Part 1 (how the legacy worked) is from C source reading. Part 2 (port roadmap) is grounded in Part 1 + the PR #563 / #608 / #674 work that exists today. **Gap**: the UserTalk windowTypes framework JES wrote on top of these kernel primitives lives in a Frontier.root that is not in either the legacy `tedchoward` tree or the current `jsavin/Frontier` tree. This doc captures the kernel surface (which is in-source) and the windowTypes framework's contract with that surface (inferred from kernel evidence + JES guidance). When a real windowTypes-bearing .root surfaces, this doc should be updated with the actual UserTalk implementation details.

---

## Part 1: How the Legacy Menu System Works

### The big picture

Legacy Frontier had **three layers** for the menu system:

1. **C kernel** — owns menubar data structures, dispatch from OS-level menu hit to script execution, storage format (the `menubarType` ODB record). Knows nothing about "window types" by name.
2. **UserTalk verb surface** — 13 verbs in `system.verbs.menu.*` that let scripts manipulate menubars (`install`, `remove`, `addMenuCommand`, `setScript`, etc.). Built into the kernel via `menuverbs.c`.
3. **UserTalk-side framework (windowTypes)** — JES-authored, lives entirely in the Frontier.root database. Sits on top of the verb surface to provide window-type-aware menubar composition: "when frontmost window is type X, install menubars A, B, C; when type Y becomes frontmost, swap to D, E, F." Hooks into kernel-fired script callbacks for window focus events.

The kernel is **deliberately unaware** of windowTypes. The kernel fires a small set of well-known scripts at well-known moments (window opened, window closed, app suspended/resumed, etc.) and the windowTypes framework hooks those firing points to drive the menubar swap.

### Source map

| File | Lines | Role |
|------|-------|------|
| `menu.c` | 769 | Single-menu operations |
| `menubar.c` | 1849 | **The core.** Menubar data structures, push/pop, dispatch, menubarlist |
| `menuverbs.c` | 2351 | UserTalk verb implementations for `menu.*` |
| `menueditor.c` | 2160+ | Menu-editing UI; also home to `meinstallmenubar` |
| `menupack.c` | n/a | ODB pack/unpack (v6/v7 serialization format) |
| `menufind.c` | 241 | Search within open menus |
| `menuresize.c` | n/a | Menu editor window resize |
| `meprograms.c` | n/a | **`meuserselected`** — the kernel→UserTalk script dispatch |
| `langipcmenus.c` | 776 | IPC bridge for shared menubars across Frontier instances (older era) |
| `tablestructure.c` | n/a | Global table registry; defines `menubartable = system.menus.sharedmenus` |
| `tablestructure.h` | n/a | The 40+ `id*script` enum of well-known kernel→UserTalk hooks |
| `cancoon.c` | n/a | Per-database menubarlist lifecycle |
| `meprograms.c` | n/a | Script execution wrappers, including `meuserselected` |

### Data structures (kernel-side)

```
hdlmenubarlist           // a linked list of menubars currently "live"
    .hfirst              // -> hdlmenubarstack
    .hnext               // chain to next bar

hdlmenubarstack          // ONE menubar, with a stack of pushed sub-bars
    .menubaroutline      // -> the ODB outline that defines this bar
    .stack[]             // array of tymenubarstackelement
        .hmenu           // the Mac OS menu handle
        .idmenu          // numeric menu ID
        .hnode           // -> the outline node this menu was built from
        .ctbaseitems     // (built-in items count for offset math)
    .topstack            // current top of stack index
    .refcon              // typically the database pointer (for purgefrommenubarlist)
    .hnext               // linked-list pointer (next menubar in the list)

hdlmenurecord            // the ODB menurecord, stored as menubarType in the database
    .menuoutline         // the outline representing the menu structure
    .hmenustack          // backref to the live in-memory stack (nil if not installed)
    .flinstalled         // is this menubar currently installed?
    .flactive            // is it currently visible/active?
```

The mental model: **`menubarlist`** is a linked list. Each node is a **`menubarstack`** (one menubar). When the user clicks a menu item, the kernel walks the entire menubarlist, asking each menubar "is this hit yours?" via `memenuhit`. First match wins.

### Database storage

Top-level table: **`system.menus.sharedmenus`** (literal name: `"sharedmenus"`, defined in `stringdefs.h:STR_menubars = "\x0b" "sharedmenus"`).

```
@system.menus.sharedmenus
    .FRNT            <- menubarType, application id 'FRNT' = Frontier
        .menuoutline <- the outline holding File/Edit/Window/etc.
    .ABCD            <- another app id (e.g. legacy IPC client)
        .menuoutline
    ...
```

Each child is a menurecord (typeOf `menubarType` per the UserTalk type system), keyed by a 4-char OSType app id. The menurecord's outline contains the menu structure: top-level rows = menus (File, Edit, etc.), child rows = items, descriptions = labels, refcons = scripts/commands.

The path `system.menus.sharedmenus.FRNT` is the canonical Frontier-host menubar. Other entries hold menubars for legacy inter-app sharing (`langipcmenus.c`).

### The dispatch path: kernel → UserTalk script

When the user clicks a menu item, the OS calls into Frontier's main event loop. The handler eventually calls `memenu (idmenu, ixmenu)` in `menubar.c:1775`. Trace:

```
OS menu hit
  → memenu (idmenu, ixmenu)                  // menubar.c:1775
    → walks menubarlist  (linked list)
      → for each menubar:
        → pushmenubarglobals (this menubar)  // menubar.c:88 — make this bar's globals current
        → memenuhit (idmenu, ixmenu, &hnode) // menubar.c:1706 — find the clicked node
          → matches: returns true with hnode = the outline node
        → meuserselected (hnode)             // meprograms.c:255 — RUN THE SCRIPT
        → popmenubarglobals ()               // menubar.c:114 — restore previous globals
```

`meuserselected` in `meprograms.c:255` is **the kernel→UserTalk script dispatch**. It:

1. Calls `megetnodelangtext (hnode, &htext, &signature)` to extract the script text + language signature (UserTalk vs OSA/AppleScript)
2. Compiles via `scriptbuildtree (htext, signature, &hcode)` — handles both languages
3. Wraps in a `newprocess (...)` and adds to the runtime process list via `addprocess`
4. Sets `processkilledroutine = meprocesscallback` so it can react when the script exits

**Async, not synchronous.** Menu scripts run as processes; the click handler returns immediately. The script's effect (focus change, file open, etc.) lands on subsequent event loop iterations.

### Per-database menubar lists: the cancoon swap

Each Frontier database (each "cancoon" — the codename for a shell instance, see `cancoon.c`) has its own menubarlist:

```c
hdlcancoonrecord .hmenubarlist     // cancoon.c — one menubarlist per database
```

`setcancoonglobals(hcancoon)` at `cancoon.c:542` calls `setcurrentmenubarlist((**hcancoon).hmenubarlist)` to swap. So **opening a different Frontier database swaps the entire menubarlist** — this is the coarse-grained "menubar swap" axis. The fine-grained axis (window-type-driven) is separate.

### Kernel→UserTalk callback registry (the windowTypes substrate)

`tablestructure.h:47-138` defines an enum `idsystemtablescripts = 139` indexing **40+ well-known kernel→UserTalk callback scripts**. The kernel resolves each ID to a UserTalk path via `getsystemtablescript(idscript, bspath)` (`tablestructure.c:155`), which reads from string resource list 139 in the Mac OS resource fork.

Selected hooks relevant to windowTypes:

| Hook ID | Purpose | Caller |
|---------|---------|--------|
| `idmenubarscript = 1` | Build/customize the menubar | (referenced in header; runtime callers TBD — see Gap note) |
| `idopenwindowscript` | Fires when a window is being opened (UserTalk can veto / customize) | `shellwindow.c:1930` |
| `idclosewindowscript` | Fires when a window is being closed | `cancoon.c:1217` |
| `idsavewindowscript` | Fires on save | `cancoon.c:873` |
| `idcompilewindowscript` | Fires for script-compile windows | `scripts.c:3213` |
| `idsuspendscript` | App suspending (lost focus) | `scripts.c:645` |
| `idresumescript` | App resuming (gained focus) | `scripts.c:669` |
| `idfrontierstartup` | Boot-time install | (used at app startup) |
| `idopcursormovedscript` | Outline cursor moved | (outline editor) |
| `idruneditmenuscript`, `idrunfilemenuscript`, `idrunopenrecentmenuscript` | Per-menu hooks added 2005 ("pike" era) | various |

**windowTypes framework hypothesis** (based on kernel evidence + JES guidance): the UserTalk windowTypes framework registers handlers under (or aliases) `idopenwindowscript` / `idclosewindowscript` / `idsuspendscript` / `idresumescript`. When a window of type X opens, the framework's openwindowscript handler:

1. Looks up the window's declared windowType (a refcon on the window record, or a property on the window's data table)
2. Looks up that windowType's menubar manifest (probably under `system.windowTypes.<TypeName>.menubars` — names speculative)
3. For each manifest entry, calls `menu.install(@some.menubar)` to add the menubar to the current menubarlist

When the window closes / loses focus, the inverse fires: walk the manifest and call `menu.remove` on each.

Meta-types resolve through a level of indirection in the framework: `windowType X.metaOf = "EditorWindow"; windowType EditorWindow.menubars = (@Edit, @View)` means a window declared as X gets both X-specific menubars and EditorWindow-meta-type menubars installed.

**This is hypothesis until a Frontier.root with a populated windowTypes table is available for inspection.** The kernel side is what makes it possible; the framework is what makes it usable.

### The UserTalk verb surface

`menuverbs.c:68-99` enumerates 13 verbs that scripts use to build/modify menubars:

```
menu.zoomScript           (zoomscriptfunc)
menu.buildMenubar         (buildmenubarfunc)
menu.clearMenubar         (clearmenubarfunc)
menu.isInstalled          (isinstalledfunc)
menu.install              (installfunc)         <- the key install verb
menu.remove               (removefunc)          <- the key uninstall verb
menu.getScript            (getscriptfunc)
menu.setScript            (setscriptfunc)
menu.addMenuCommand       (addmenucommandfunc)
menu.deleteMenuCommand    (deletemenucommandfunc)
menu.addSubMenu           (addsubmenufunc)
menu.deleteSubMenu        (deletesubmenufunc)
menu.getCommandKey        (getcommandkeyfunc)
menu.setCommandKey        (setcommandkeyfunc)
```

`menu.install(@some.menubar)` ultimately calls `meinstallmenubar` in `menueditor.c:2055`, which:

1. If the menurecord has no in-memory stack: build one with `menewmenubar(outline)` then `mebuildmenubar(stack)`
2. If not currently active: insert into menubarlist via `meinsertmenubar(stack)`
3. Mark `.flinstalled = true`

This is the pivotal verb the windowTypes framework calls (many times, once per menubar in the type's manifest) when a window of that type becomes frontmost.

### End-to-end worked example: "user clicks File > New Script in legacy Frontier"

Assume frontmost window is a Script Editor; menubarlist currently contains [Frontier-host-bar, Script-bar].

```
1. User clicks "File" menu in OS menubar at screen coordinates (x, y).
2. OS menu manager pops down the File menu, user picks "New Script".
3. OS calls Frontier's event handler with idmenu=<File-menu-id>, ixmenu=<row>.
4. Frontier's event dispatch → memenu(idmenu, ixmenu) [menubar.c:1775]
5. memenu walks menubarlist (Frontier-host-bar first):
   a. pushmenubarglobals(frontier-host-bar)  -- make its globals current
   b. memenuhit(idmenu, ixmenu, &hnode)      -- search this bar's stack
   c. The File menu's id matches Frontier-host-bar's stack entry for File
      → returns true with hnode = the outline node for "New Script"
   d. meuserselected(hnode)                   -- DISPATCH
      - megetnodelangtext extracts the script text:
        "scriptEditor.newWindow ()"          (or whatever the legacy script was)
      - scriptbuildtree compiles it
      - newprocess wraps it; addprocess queues it
   e. popmenubarglobals()
   f. returns true → memenu returns
6. Asynchronously: the runtime picks up the queued process,
   executes scriptEditor.newWindow(), which:
   a. Allocates a new Script Editor window
   b. Calls window-creation framework, which fires idopenwindowscript
   c. The windowTypes framework's openwindowscript handler runs:
      - Reads the new window's windowType (= "ScriptEditor")
      - Looks up ScriptEditor's menubar manifest
      - For each manifest entry: calls menu.install(@<bar>)
        → each menu.install eventually calls meinstallmenubar
        → meinstallmenubar appends to menubarlist
      - menubarlist is now [Frontier-host-bar, Script-bar, ScriptEditor-bar]
   d. Window is shown
7. Next time user clicks a menu, memenu walks the updated 3-bar list.
```

The kernel never knows the window is "ScriptEditor-typed." That label and the menubar-manifest mapping live entirely in the UserTalk-side windowTypes framework, which uses `idopenwindowscript` as its hook point.

### Why `idmenubarscript` is declared but unreferenced in source

The header declares `idmenubarscript = 1` but the kernel sources don't call `getsystemtablescript(idmenubarscript, ...)` directly. Two possibilities:

1. **Vestigial.** Earlier Frontier versions called it; later ones removed the call. The id stayed in the header to preserve the ordering of subsequent IDs (changing it would break the resource fork mapping).
2. **Called from somewhere I haven't grepped.** A different naming convention, an indirect reference, or a callsite in Mac-specific code outside `Common/source/`.

For port purposes, this is mostly trivia — the active window-event hooks (`idopenwindowscript`, etc.) are clearly load-bearing and where the windowTypes framework hangs.

### Per-database vs. windowTypes axes

Two axes of menubar swap exist in legacy Frontier:

1. **Database swap** (`setcancoonglobals` → `setcurrentmenubarlist`): each open Frontier database has its own menubarlist. Switching between databases is a coarse-grained swap. Driven entirely by C-side cancoon machinery.
2. **Window-type swap** (windowTypes framework hooks on `idopenwindowscript` etc.): within a single database's menubarlist, menubars come and go as windows of different types become frontmost. Driven entirely by UserTalk.

The two axes compose: the database swap establishes the menubarlist; the windowTypes framework adds/removes menubars within that list as windows open and close.

---

## Part 2: Headless Port Roadmap

### What survives intact

These pieces of the legacy model carry forward without conceptual change:

- **The menubarType data model.** A menubar is an outline with refcons holding scripts/commands. Already supported in v7 (per `planning/phase3/MENU_V7_MIGRATION_PLAN.md`). The `system.menus.sharedmenus.<appid>` storage shape is fine for the headless CLI; we'd just have one entry (`FRNT` or similar) for the CLI host.
- **The 13 UserTalk verbs** (`menu.install`, `menu.remove`, `menu.addMenuCommand`, etc.). These ARE in the headless port — `menuverbs_headless.c` ships them, with the `menudata_ensure_root()` lazy-create that PR #608 introduced.
- **The kernel→UserTalk callback registry** (`tablestructure.h` script IDs and `getsystemtablescript`). The mechanism is sound; we just need to wire up the headless-relevant hooks (`idopenwindowscript`, `idclosewindowscript`, `idresumescript`, etc.) when the CLI gains windows.
- **`meuserselected` for script dispatch.** Already ported as `meuserselected_headless` in PR #608's consolidation. Async-process semantics may need adapting to the GIL-held REPL modal context, but the structure is intact.
- **menubarlist as a linked list of installed menubars.** Same data structure works in headless. The cascade renderer in `palette.c` already iterates over the data source's `count_menus` (currently 1 — only `repl`); generalizing to N menubars is what issue #677 covers.

### What needs adaptation

These pieces have conceptual analogs but the headless context changes the substrate:

- **OS menu manager handoff.** Legacy got menu hits from the Mac OS menu manager via Carbon events. Headless gets them from our own palette modal (`run_palette_modal` in `repl.c`). The palette's `palette_feed_byte` dispatch and `meuserselected_headless` invocation IS the headless equivalent of `memenu → meuserselected`. Already wired.
- **Per-database menubarlist swap.** Legacy: each cancoon has its own menubarlist; opening a different db swaps. Headless: today there's one "host" menubarlist (the REPL palette source); when guest databases are opened in the headless CLI, each could in principle contribute its own menubarlist segment. **Open question for the port**: do we want database-scoped menubars in the headless CLI, or do all open databases contribute to a single shared list? The current `system.menus.data.<menubar-name>` storage shape is database-shared; if we want per-database scoping, the path needs an app/db key.
- **Menubar visibility (push/pop).** Legacy uses `pushmenubarglobals/popmenubarglobals` to walk the list and check each bar's items. Headless palette already walks the data source's menubars; this maps cleanly. The push/pop of "globals" (current outline being inspected) is a no-op equivalent in headless since we don't have outline-globals state to maintain.

### What new substrate the editors will need

These are the load-bearing pieces that **don't exist in the headless port yet** and need to be built before editor windows can integrate cleanly:

#### (1) Window-type registry (UserTalk-side, mirrors legacy windowTypes framework)

A table at `system.windowTypes` (or `system.windows.types` — naming TBD) with entries like:

```
@system.windowTypes
    .ScriptEditor
        .menubars = {@user.menubars.editMenu, @user.menubars.scriptMenu}
        .metaOf = "EditorWindow"     <- optional, supports meta-types
    .OutlineEditor
        .menubars = {@user.menubars.editMenu, @user.menubars.outlineMenu}
        .metaOf = "EditorWindow"
    .EditorWindow
        .menubars = {@user.menubars.fileMenu, @user.menubars.windowMenu}
        .abstract = true             <- meta-types are abstract; no window has this as concrete type
    .ReplWindow
        .menubars = {@user.menubars.replMenu}
```

JES wrote this on the legacy side; the headless port should adopt the same shape so existing windowTypes definitions are portable.

#### (2) Frontmost-window tracking (headless event loop)

The headless CLI needs to track which window is currently "frontmost" — i.e. the focus target for menu activation. Today there are no editor windows in the CLI, so frontmost is trivially the REPL itself. When editors arrive:

- Each window record gets a `windowType` field (string).
- A global `frontmost_window` pointer tracks the active one.
- When focus changes (TBD how — keystroke, mouse, command), an event fires.

#### (3) Window-event callback bridge (kernel-side, headless)

Today, `palette_modal_smoke.yaml`'s "menubar source" is hardcoded to `repl_palette_source_init_for("repl")`. When editor windows exist and need to contribute menubars, the bridge looks like:

```c
// On frontmost-change (in repl.c event loop, post-window-system support):
void on_frontmost_changed (hdl_window old_window, hdl_window new_window) {
    if (old_window != nil)
        run_user_script (idclosewindowscript, old_window);  // legacy hook
    if (new_window != nil)
        run_user_script (idopenwindowscript, new_window);   // legacy hook
}
```

The UserTalk windowTypes framework's `idopenwindowscript` handler reads the new window's type, looks up the menubar manifest, and calls `menu.install` for each. The framework's `idclosewindowscript` handler does the reverse.

`run_user_script` and the `getsystemtablescript` machinery: port from `tablestructure.c`. Likely needs adaptation for the headless GIL model — we don't have addprocess as a separate runtime queue; scripts run synchronously inline.

#### (4) Palette source multi-menubar enumeration

Issue #677 covers this. Once the menubarlist has more than one entry, `repl_palette_source.c` needs to enumerate them and present them as the cascade's top-level menus. Today it's hardcoded to one source name.

Two design choices for the enumeration:

- (a) Flat: enumerate every installed menubar, each becomes a top-level entry in the cascade strip. Limit ~5-7 menubars before width overflow.
- (b) Composed: walk all installed menubars and aggregate their top-level menus into one combined strip. This matches legacy behavior (the OS menubar shows e.g. `File Edit View Tools Window Help` as one row even though they come from multiple installed menubars).

**Legacy did (b).** The headless port should also do (b) — render the union of all menubars' top-level menus as a single horizontal strip, with the menubarlist composition driving what appears.

### Port sequencing (proposed)

The investigation suggests this dependency order for the menu-system port work:

1. **Issue #677 (multi-menubar enumeration)** — straightforward extension of `repl_palette_source.c` to walk `@system.menus.data.*` rather than hardcoding `.repl`. This is mostly mechanical and the prerequisite for everything else.

2. **`getsystemtablescript` port** — bring the script-ID → path resolution mechanism to headless. Resource-fork-backed in legacy; in headless this becomes a small string table in C (or a UserTalk-side `system.scriptids` table — design call). Without it the windowTypes framework can't hook anything.

3. **Window-event callback bridge stubs** — port `idopenwindowscript` etc. firing points. Even without windows yet, the REPL itself can fire `idresumescript` / `idsuspendscript` on its own activation/deactivation if that's useful. This is the wiring the windowTypes framework needs.

4. **Window-type registry** — port the UserTalk windowTypes framework as `system.windowTypes` table + handler scripts. Can be done before any actual windows exist — the registry can be populated for "ReplWindow" right away to validate the model.

5. **Editor windows** — when these arrive, each editor's window record has a windowType, focus changes fire the events, the framework drives menubar composition.

The first three are doable BEFORE editors arrive and would clear the path for them. Editor work would then plug in via clean interfaces rather than retrofitting.

### File > Exit vs REPL > Exit (task #21 deferral)

The collision question becomes clearer in the windowTypes model:

- File menu lives in `system.windowTypes.EditorWindow.menubars` (or wherever the meta-type lives) — appears when an editor window is frontmost.
- REPL menu lives in `system.windowTypes.ReplWindow.menubars` — appears when the REPL is frontmost.

Today there's no concept of frontmost-window in the headless CLI, so both menubars are always installed (only REPL is implemented). When windowTypes drives composition, **File > Exit and REPL > Exit are visible at different times** — File when an editor is frontmost, REPL when the REPL is. They don't actually collide in any single screen.

This makes option (a) from task #21 (both exist, redundant by design) the right answer — they're not redundant in practice once windowTypes is wired.

---

## Gaps + open questions

These remain unresolved in this investigation and should be tracked:

1. **The UserTalk windowTypes framework's actual implementation.** Hypothetical here. When a Frontier.root with the framework surfaces (or JES exports the relevant scripts), this doc should be updated with concrete UserTalk verb signatures, table shape, and handler logic.

2. **`idmenubarscript` purpose.** Declared but not referenced in `Common/source/`. Either vestigial or called from Mac-specific code outside that tree. Worth checking before porting (might be a hook the windowTypes framework relies on).

3. **Per-database vs. per-window menubar scope.** Legacy had both. Headless port currently has neither articulated explicitly. Design choice when editors arrive: do menubars belong to a database, a window, both, or neither?

4. **Async vs. sync script dispatch.** Legacy `meuserselected` runs scripts as queued processes (async). Headless `meuserselected_headless` runs inline under the GIL. This is a real semantic difference — if a legacy menu script did `wait 5` then continued, the menubar continued to function. Headless equivalent would freeze the REPL for 5 seconds. Worth surfacing in any porting of substantial legacy menu scripts.

5. **Inter-app menubar sharing** (`langipcmenus.c`). Legacy supported one Frontier instance hosting menubars for another application. Likely not relevant for headless CLI; can be deprecated.

---

## See also

- `planning/discussions/repl-slash-menu-implementation-plan.md` — the 8-PR plan and 5-layer projection model
- `planning/phase3/MENU_V7_MIGRATION_PLAN.md` — v6 → v7 menubarType migration
- Issue #677 — multi-menubar enumeration for the palette
- Memory: `project_slash_menu_heritage.md` — `/` for menu has a 45+ year VisiCalc/Excel heritage
- ADR-016 (in planning/) — 5-layer headless menu projection model
- Session task #22 (this investigation), #20 (Phase 6 detailed plan, blocked by this doc)
