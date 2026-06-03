# Legacy Menu System: Architecture + Headless Port Roadmap

**Audience**: internal technical (Claude, JES, future agents). Investigation produced from reading `/Users/jake/dev/tedchoward/Frontier/Common/source/`, the UserTalk `.ut` corpus under `usertalk_scripts/Frontier.root/`, and the `system.menus.buildMenubar` script body (provided by JES from the live legacy ODB), cross-referenced against the headless port work in `/Users/jake/dev/jsavin/Frontier/`.

**Status**: Part 1 (how the legacy worked) is from C source + UserTalk reading. Part 2 (port roadmap) is grounded in Part 1 + the PR #563 / #608 / #674 work that exists today.

**Correction 2026-06-03 — the menubar builder is a UserTalk script, not the windowTypes framework**: earlier revisions of this doc claimed the windowTypes framework composes the menubar via window-focus hooks. That is wrong. The authoritative menubar builder is the UserTalk script **`system.menus.buildMenubar`** (see the dedicated section below). The windowTypes framework is a separate, narrower thing: it handles per-window open/close/save/cursor callbacks and File/Edit menu-command dispatch, and it contributes at most the *single modal menu* keyed off the frontmost window's type. It does not build the bar. The bulk of the bar (base menubar, html menu, bookmark/custom menus, every Tool-contributed menu, the Help menu) is assembled by `system.menus.buildMenubar` independent of any window-focus event.

**Corpus/ODB drift note**: `system.menus.buildMenubar` (the real builder body) is **not** present in this repo's `.ut` corpus. The only `buildMenubar.ut` in the corpus — `usertalk_scripts/Frontier.root/system/verbs/builtins/menu/buildMenubar.ut` — is a thin kernel wrapper (`on buildMenubar () { kernel (menu.buildmenubar)}`), a *different verb with the same short name*. The real builder lives only in the live legacy Frontier.root. This is a concrete instance of the open `.root`/`.ut` sync problem.

**windowTypes framework location**: `usertalk_scripts/Frontier.root/system/verbs/builtins/Frontier/tools/windowTypes/` (UserTalk path `system.verbs.builtins.Frontier.tools.windowTypes`). Files: `init.ut`, `findWindowType.ut`, `callWindowType.ut`, `newWindow.ut`, `openWindow.ut`, `runFileMenuScript.ut`, `runEditMenuScript.ut`, `findWindowWithMatchingAtts.ut`, `isFileMenuItemChecked.ut`, `isFileMenuItemEnabled.ut`, `isWindowDirty.ut`, `getDefaultFilename.ut`, plus `callbacks/` and `commands/` subtables. Read the `.ut` files for ground truth on the framework's shape.

---

## Part 1: How the Legacy Menu System Works

### The big picture

Legacy Frontier's menu system has **three layers**, but the composition layer is a UserTalk *script*, not the windowTypes framework:

1. **C kernel** — owns menubar data structures, dispatch from OS-level menu hit to script execution, storage format (the `menubarType` / `'mbar'` ODB record), and the low-level "turn an outline into OS menu handles" builder (`mebuildmenubar`, exposed to UserTalk as the kernel verb `menu.buildmenubar`). Knows nothing about "window types" by name.
2. **UserTalk verb surface** — 13 verbs in `system.verbs.menu.*` that let scripts manipulate menubars (`install`, `remove`, `addMenuCommand`, `setScript`, `clearMenuBar`, `buildMenubar`, etc.). Built into the kernel via `menuverbs.c`.
3. **UserTalk-side composition** — driven primarily by the **`system.menus.buildMenubar`** script (and the smaller scripts it calls: `html.menu.install`, the `user.menus` loop, the modal-menu lookup). This is the authority for *what the menubar contains*. The windowTypes framework is a peer UserTalk-side subsystem that handles per-window-open behavior and File/Edit command dispatch; it feeds the builder only indirectly (its commands can be reached from menu items the builder installed), and contributes the single modal menu's content.

The kernel is **deliberately unaware** of both windowTypes and `system.menus.buildMenubar`. It provides the verb surface and the OS-handle builder; the UserTalk layer decides composition. `system.menus.buildMenubar` is called explicitly (e.g. by `system.menus.installMainMenu` at startup, and again whenever the frontmost window changes so the modal menu can be swapped) — it is not fired by a kernel window-event hook.

### The authoritative builder: `system.menus.buildMenubar`

This UserTalk script is **the** menubar composer. Its body is not in this repo's corpus (live ODB only); the logic below is from the script body JES provided. In order, it:

1. **Defines a local helper** `on add (adrmenu)` — installs one menu into the bar being built.
2. **`menu.clearMenuBar ()`** — tears down the current bar so composition starts from a clean slate.
3. **Installs the base menubar** from `user.menus.menubar` (falling back to `system.menus.menubar`). This base bar is **ODB data** (a `menubarType` / `'mbar'` record), *not* a set of Mac `MENU` resources. This is the File/Edit/Window/etc. skeleton.
4. **`html.menu.install ()`** — if the HTML/web-editor menu is enabled, folds it in (see `usertalk_scripts/.../html/menu/install.ut`).
5. **Adds the bookmark menu and the custom menu.**
6. **Loops over `user.menus`**, installing every entry whose type is `menuBarType`, *skipping* the reserved entries `bookmarkMenu`, `customMenu`, and `menubar` (those were handled in steps 3 and 5). **This loop is the channel by which the Tools Framework contributes menus** — a Tool drops a `menuBarType` record under `user.menus` and it appears as a top-level menu here.
7. **Adds `system.menus.helpMenu`.**
8. **Installs exactly one modal menu**, chosen by the frontmost window's *window type*: `winType = window.getType (window.frontmost ())`, recorded in `system.menus.data.currentMenuType`, looked up under `system.menus.modals.[winType]`. This is the **only** point where the frontmost window influences the bar, and it contributes a single menu.
9. **`menu.buildMenuBar ()`** — calls the kernel verb (`menu.buildmenubar` → `mebuildmenubar`, `menubar.c`) to turn the assembled outline(s) into live OS menu handles and force the menubar to redraw.

Key consequences for the mental model:

- **The bar is data-driven.** `user.menus.menubar` / `system.menus.menubar`, `user.menus.*`, `system.menus.helpMenu`, `system.menus.modals.*` are all ODB records. Editing the menubar means editing ODB data, then re-running the builder.
- **Window type touches only step 8.** Everything else (base bar, html, bookmarks, custom, all Tool menus, help) is window-type-independent. The frontmost window selects one modal menu; it does not compose the bar.
- **"window type" here is a meta-type**, distinct from an ODB external value type. `window.getType()` returns a string like `"ScriptEditor"` / `"OutlineEditor"`; that is *not* the `typeOf` of any ODB value. (Contrast: `menuBarType` in step 6 *is* an ODB external value type — `'mbar'`. Same word "type", two different notions.)
- **Tools and window types are independent capabilities.** A Tool can contribute a top-level menu (step 6, via `user.menus`) *and/or* declare its own window types (consumed by the windowTypes framework and step 8). Neither implies the other.

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

### Kernel→UserTalk callback registry (window-event substrate)

`tablestructure.h:47-138` defines an enum `idsystemtablescripts = 139` indexing **40+ well-known kernel→UserTalk callback scripts**. The kernel resolves each ID to a UserTalk path via `getsystemtablescript(idscript, bspath)` (`tablestructure.c:155`), which reads from string resource list 139 in the Mac OS resource fork.

This registry is what the **windowTypes framework** hooks — but note its scope: window open/close/save/cursor events, *not* "build the menubar." The framework reacts to a window opening by running that window type's `openWindow` handler (which may set the window title, or install/remove a type-specific menu), and it routes File/Edit menu *commands* to per-window-type scripts. The whole-bar composition still belongs to `system.menus.buildMenubar`. When the frontmost window changes, the modal-menu swap is achieved by re-running `system.menus.buildMenubar` (step 8 re-reads `window.getType(window.frontmost())`), not by a windowTypes hook rebuilding the bar.

Selected hooks relevant to window events:

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

**windowTypes framework — what it actually does** (confirmed from the `.ut` corpus):

The framework registers handlers under `idopenwindowscript` / `idclosewindowscript` / `idsavewindowscript` / `idopcursormovedscript` (its `callbacks/` subtable: `openWindow.ut`, `closeWindow.ut`, `saveWindow.ut`, `opCursorMoved.ut`, `opReturnKey.ut`). The open-window path is:

1. `callbacks/openWindow.ut` reads the window's `"type"` attribute via `window.attributes.getOne("type", @windowType, adr)`.
2. It calls `findWindowType.ut` to resolve that type string to a table — first checking `user.tools.windowTypes.[type]`, then `Frontier.tools.data.windowTypes.[type]` (`findWindowType.ut:7-19`).
3. If that type table has an `openWindow` script, it runs it (`callbacks/openWindow.ut:11-13`). That per-type handler is free to install a type-specific menu, set the title, etc.

File/Edit menu *commands* route through `runFileMenuScript.ut` / `runEditMenuScript.ut`, which `thread.callScript` into `Frontier.tools.windowTypes.commands.[itemname]` (e.g. `new`, `open`, `save`, `find`).

**What it does NOT do**: none of the framework scripts call `menu.install` to compose the whole bar, and none call `menu.buildMenuBar`. The framework is per-window behavior + command dispatch. The bar is built by `system.menus.buildMenubar`. The framework's only contribution to *bar content* is upstream of step 8 of the builder: a window's type determines which modal menu the builder installs.

This is why the headless port (Part 2) had to be careful: it reuses the windowTypes framework as the dispatch substrate but composes the headless bar through `installReplMenubar` + a `ReplWindow.openWindow` handler that installs the `frontier` bar directly — **a deliberate divergence from the legacy `system.menus.buildMenubar` single-builder model**, made because the real builder script was never in the corpus to port.

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

This is the verb `system.menus.buildMenubar`'s `add` helper calls (once per menu it folds into the bar), and the verb a window type's `openWindow` handler may call to install a type-specific menu.

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
   a. Allocates a new Script Editor window (type attribute "ScriptEditor")
   b. Calls window-creation framework, which fires idopenwindowscript
   c. The windowTypes framework's callbacks/openWindow handler runs:
      - Reads the new window's "type" attribute (= "ScriptEditor")
      - findWindowType resolves it to the ScriptEditor type table
      - If that table has an openWindow script, runs it (may set title,
        may install a type-specific menu) -- per-window behavior, NOT
        a full-bar rebuild
   d. The window becomes frontmost. To reflect that in the bar, the
      app re-runs system.menus.buildMenubar:
      - steps 1-7 reassemble the base bar + html + bookmark/custom +
        every user.menus menuBarType (Tool menus) + helpMenu
      - step 8: winType = window.getType(window.frontmost()) is now
        "ScriptEditor"; it installs system.menus.modals.ScriptEditor
        as the single modal menu and records currentMenuType
      - step 9: menu.buildMenuBar() rebuilds the OS handles
   e. Window is shown
7. Next time user clicks a menu, memenu walks the rebuilt bar.
```

The kernel never knows the window is "ScriptEditor-typed." That label is a window *attribute* read by the windowTypes framework (`window.attributes.getOne("type", ...)`) and by the builder (`window.getType(window.frontmost())`). The windowTypes framework reacts to the open event (per-window behavior); the *bar content* change is `system.menus.buildMenubar`'s job — specifically its step-8 modal-menu swap.

### Why `idmenubarscript` is declared but unreferenced in source

The header declares `idmenubarscript = 1` but the kernel sources don't call `getsystemtablescript(idmenubarscript, ...)` directly. Two possibilities:

1. **Vestigial.** Earlier Frontier versions called it; later ones removed the call. The id stayed in the header to preserve the ordering of subsequent IDs (changing it would break the resource fork mapping).
2. **Called from somewhere I haven't grepped.** A different naming convention, an indirect reference, or a callsite in Mac-specific code outside `Common/source/`.

For port purposes, this is mostly trivia. The likely answer: `idmenubarscript` is vestigial — modern Frontier composes the bar by *calling* `system.menus.buildMenubar` explicitly (from `installMainMenu` at startup and on frontmost-window changes), not by the kernel firing a menubar-build hook. The active window-event hooks (`idopenwindowscript`, etc.) are load-bearing for the windowTypes framework's per-window behavior, but they are not how the bar gets built.

### Per-database vs. frontmost-window axes

Two axes of menubar variation exist in legacy Frontier:

1. **Database swap** (`setcancoonglobals` → `setcurrentmenubarlist`): each open Frontier database has its own menubarlist. Switching between databases is a coarse-grained swap. Driven entirely by C-side cancoon machinery.
2. **Frontmost-window swap** (`system.menus.buildMenubar` step 8): within a database, the *modal menu* changes as windows of different types become frontmost. This is a single-menu swap, performed by re-running the builder (which re-reads `window.getType(window.frontmost())`), not by the windowTypes framework swapping whole menubars.

The two axes compose: the database swap establishes the menubarlist; re-running `system.menus.buildMenubar` recomposes the bar (including the frontmost-driven modal menu) within it. The windowTypes framework rides alongside as the per-window-open/command-dispatch layer; it is not one of the menubar-*composition* axes.

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

#### (1) Window-type registry (UserTalk-side) + a builder analog

The windowTypes registry already exists in the corpus at `Frontier.tools.data.windowTypes.[type]` (resolved by `findWindowType.ut`, with `user.tools.windowTypes.[type]` as the user-override layer). Each type table can carry an `openWindow` handler (run on open) plus File/Edit command tables. The headless port reuses this.

But note what the registry is *for*: per-window behavior, and (in legacy) selecting the step-8 modal menu via `system.menus.modals.[winType]`. It is **not** a menubar manifest. Legacy did not have "windowType X → install menubars A,B,C"; legacy had `system.menus.buildMenubar` assembling base + html + bookmarks + custom + all `user.menus` Tool menus + help + one modal menu.

Two honest options for the headless port:

- **(a) Port the builder.** Bring `system.menus.buildMenubar` (and the `user.menus.menubar` / `system.menus.menubar` / `system.menus.helpMenu` / `system.menus.modals.*` data it reads) into the corpus and call it as legacy does. This is the high-fidelity path but requires the builder script body (live ODB only today — see the drift note at the top).
- **(b) The current headless divergence.** `installReplMenubar` installs the `repl` bar directly, and a `ReplWindow.openWindow` manifest handler (in `Frontier.tools.data.windowTypes.ReplWindow`) installs the `frontier` bar (File/Edit/View). The palette then unions the installed bars. This works and is shipped (Phase E), but it composes the bar through the windowTypes per-window-open path rather than through a `buildMenubar` builder — a deliberate divergence, documented in MENU_PORT_PLAN.md.

If full legacy fidelity is the goal, option (a) is the eventual target and depends on first getting `system.menus.buildMenubar` into the corpus.

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

The UserTalk windowTypes framework's `callbacks/openWindow` handler reads the new window's `"type"` attribute, resolves it via `findWindowType`, and runs that type's `openWindow` script (per-window behavior — title, optional type-specific menu). It does **not** install a menubar manifest; legacy bar composition is `system.menus.buildMenubar`'s job. In the headless port, the `ReplWindow.openWindow` handler installs the `frontier` host bar directly — that is the port's divergence from the legacy builder model, not a faithful reproduction of the legacy openWindow handler.

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

4. **Window-type registry** — already present: `Frontier.tools.data.windowTypes.[type]` (corpus), with `user.tools.windowTypes.[type]` as the override layer, resolved by `findWindowType.ut`. Populated for `ReplWindow` (Phase C/E). NOTE: this registry drives per-window behavior + the modal-menu selection; it is not a menubar manifest. Faithful legacy composition additionally needs `system.menus.buildMenubar` + its data tables (gap #1).

5. **Editor windows** — when these arrive, each editor's window record has a windowType, focus changes fire the events, the framework drives menubar composition.

The first three are doable BEFORE editors arrive and would clear the path for them. Editor work would then plug in via clean interfaces rather than retrofitting.

### File > Quit vs REPL > Exit (task #21, resolved in Phase E)

In legacy, File lives in the base menubar (`user.menus.menubar`, installed by `system.menus.buildMenubar` step 3) and is present whenever the host bar is up. There is no per-window-type "File menu appears only for editors" mechanism — File is part of the always-present base bar; window type selects only the single modal menu (step 8).

In the headless port, both the `repl` bar (REPL menu, incl. Exit) and the `frontier` bar (File menu, incl. Quit) are installed and the palette unions them, so both are visible at once. Phase E resolved the apparent collision by wiring **File > Quit and REPL > Exit to the same exit path** (`repl.exit ()`): they are dispatch-target-identical, not redundant. See MENU_PORT_PLAN.md Phase E for details.

---

## Gaps + open questions

These remain unresolved in this investigation and should be tracked:

1. **`system.menus.buildMenubar` is not in the corpus.** The authoritative builder body lives only in the live legacy Frontier.root. Until it is exported into `usertalk_scripts/`, the legacy composition model can be described (this doc) but not run/ported faithfully. This is the single biggest gap, and a concrete instance of the broader `.root`/`.ut` sync problem. **Action: export `system.menus.buildMenubar` (and the data tables it reads — `user.menus.menubar`/`system.menus.menubar`, `system.menus.helpMenu`, `system.menus.modals.*`) into the corpus.**

2. **`idmenubarscript` purpose.** Declared (`tablestructure.h`) but not referenced in `Common/source/`. Almost certainly vestigial — modern Frontier composes the bar by explicitly *calling* `system.menus.buildMenubar`, not via a kernel menubar-build hook. Worth a final confirmation before porting, but low risk.

3. **Per-database vs. per-window menubar scope.** Legacy had per-database (cancoon) menubarlists and a per-frontmost-window modal-menu swap. Headless port currently articulates neither explicitly. Design choice when editors arrive: do menubars belong to a database, a window, both, or neither?

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
