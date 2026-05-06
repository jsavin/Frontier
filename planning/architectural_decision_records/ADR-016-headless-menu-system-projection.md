# ADR-016: Headless Projection of the Frontier Menu System

**Status**: Accepted
**Date**: 2026-05-05
**Author**: Jake Savin (with research support)
**Supersedes**: nothing (new architectural ground)
**Superseded by**: nothing
**Related**: planning/discussions/headless-menus-as-slash-commands.opml, repl-ui-design-plan.opml, repl-slash-menu-implementation-plan.opml

---

## Status note

This ADR establishes the architectural model for projecting the Frontier menu system into headless hosts (the CLI/REPL today, a future native UI host, possible web/chatbot/MCP projections later). It is informed by a deep read of both the current `jsavin/Frontier/` tree and the legacy `tedchoward/Frontier/` tree.

**This ADR does not commit to an implementation.** It commits to a model. Implementation is described downstream in the slash-menu discussion docs and (eventually) the PRs that follow this ADR.

A standalone research note with the full file:line citations lives in the drafts directory: `drafts/ADR-016-research-notes-menu-system-architecture.md` (companion working artifact, kept for reference but not part of the canonical decision record).

The seven open questions identified during drafting were resolved on 2026-05-05; see "Resolved questions" below for outcomes and rationale, including two answers that diverged from the draft's recommendations.

---

## Context

### The original system, in one paragraph

The Frontier menubar is a five-layer composite. Items at the top of the menubar (Apple, File, Edit, Window) are loaded from the application's **resource fork** at startup via `shellinitmenus()` (`Common/source/shellmenu.c:285-328`) — pre-script, pre-ODB, baked into the binary. Hierarchic submenus (Font/Size/Style/Leading/Justify) come from the same resource layer. After that floor, **MBAR objects** — outlines stored in the ODB and wrapped by `tymenurecord` (`Common/headers/menueditor.h:142-193`) — are installed dynamically by `menu.install()` and merged into the menubar by `mebuildmenubar()` (`Common/source/menubar.c:930-989`). External applications publish their menus through the **Suites/IPC mechanism** (`Common/source/langipcmenus.c`). The **Help menu** is a system-provided affordance attached by Carbon. Behavior on top of the structural layer is scripted: the **Pike pattern** (`Common/source/shellmenu.c:1060,1085`) shows how individual menu *items* — even on the resource-loaded File and Edit menus — defer to UserTalk handlers via calls like `pike.isFileMenuItemEnabled("itemname")` and `pike.isFileMenuItemChecked("itemname")`.

### Why this matters for headless hosts

The implementation plan in `planning/discussions/repl-slash-menu-implementation-plan.opml` proposed treating `system.menus.data` as the canonical source of truth for the REPL palette. That's correct for the *dynamic* layer but wrong as a complete model — it ignores the resource-loaded floor and the Pike pattern. A REPL palette that only enumerates `system.menus.data` will not see File, Edit, or Window unless those menus get a headless representation that doesn't exist in the legacy code.

The user's directional goal is parity: the REPL menubar should evolve toward the same shape as the native menubar, so that when a native UI host returns, both consume the same substrate. That requires deciding *now* how the resource layer projects into a world without a resource fork.

### Key facts the research established

A full report with citations is in the companion research note. The decision-relevant findings:

1. **No `'MBAR'` OSType.** A menu in the ODB is `tymenurecord` wrapping `hdloutlinerecord`. The outline hierarchy IS the menu hierarchy; refcons hold script and cmdkey (`Common/headers/menueditor.h:206-214`).
2. **Composition is fixed-order.** Apple → File → Edit → Window → hierarchic submenus → MBAR-installed → Help. The merge function is `mebuildmenubar()` (`Common/source/menubar.c:930-989`).
3. **`shelladjustmenus()` is 415 lines of mostly-data work** (`Common/source/shellmenu.c:946-1361`). It computes enable/disable flags, label rewrites, and checkmarks based on `shellwindowinfo` selection state. The Mac-UI-bound step is the eventual `drawmenubar()`. The computation could run headlessly.
4. **The Pike pattern is the real precedent for "menu items as scripts."** Hard-coded structure, scripted state and behavior. Frontier itself uses it for non-Pike builds via `runfilemenuscript()` and `runeditmenuscript()` (`Common/source/shellmenu.c:1543,1545`).
5. **The current jsavin/ tree is not a rearchitecture of legacy.** It's legacy + db_format context threading + 25 `#ifdef FRONTIER_HEADLESS` stubs. The verb dispatcher `menufunctionvalue()` (`Common/source/menuverbs.c:1953-2174`) is intact; six P0 verbs are no-op'd in headless mode but the structure is unchanged. This means headless persistence is one ifdef-removal-plus-implementation away, not a redesign.
6. **Per-window-type menubar switching is implemented in window-management code, not menu code.** The mechanism is not visible in `Common/source/menubar.c` or `Common/source/shellmenu.c`. It's an OPEN QUESTION whether there's an explicit `system.menus.<windowtype>` convention or whether switching happens via outline-variable activation in window-activate handlers.

---

## Decision

### The 5-layer projection model

Every headless host (REPL today, native UI tomorrow, web/chatbot later) projects the Frontier menubar by composing five layers in fixed order. The substrate is the same; the projection is what's different.

```
+---------------------------------------------------------------------+
| Layer 1: Host-anchored menus                                         |
|   Apple/Frontier menu, File, Edit, Window, Help                      |
|   Source: an MBAR-equivalent outline owned by the host process       |
|   In Mac: resource fork + scripted-item handlers (Pike pattern)      |
|   In headless: ODB outline at system.menus.data.<host>.* installed   |
|     at startup by host's installHostMenubar.ut script                |
+---------------------------------------------------------------------+
| Layer 2: Apps/Tools — dynamic, content-driven                        |
|   Each loaded application's installed menus                          |
|   Source: system.menus.data.<app>.* with .installed = true           |
|   Surfaced when the app calls menu.install()                         |
|   Removed when the app calls menu.remove() or app unloads            |
+---------------------------------------------------------------------+
| Layer 3: Suites/IPC                                                  |
|   Menus published by external processes via the Suites mechanism     |
|   Source: per-process outline transmitted through                    |
|     langipcinstallmenus() (Common/source/langipcmenus.c:562)         |
|   Lifecycle tied to the IPC connection                               |
+---------------------------------------------------------------------+
| Layer 4: Window-type-specific menus                                  |
|   The menubar changes when a wptext / outline / table window is      |
|     frontmost. (OPEN QUESTION: exact mapping mechanism)              |
|   Source: per-window-type outline; activated on window focus         |
|   In headless: deferred — REPL has only one "window" (itself).       |
|     Native host re-enables this when window types come back.         |
+---------------------------------------------------------------------+
| Layer 5: System-provided                                             |
|   Mac OS Help menu (HMGetHelpMenu) and platform affordances          |
|   In headless: replaced by host-owned Help (Layer 1) since there     |
|     is no system menubar to attach to                                |
+---------------------------------------------------------------------+
```

### Composition rules

- **Order is fixed**: Layer 1 (anchored) → Layer 2 (apps/tools) → Layer 3 (suites) → Layer 5 (system) — exactly mirroring the legacy menubar's left-to-right composition. Help is rendered rightmost.
- **Layer 1 is host-owned, not application-owned.** The CLI host owns its anchored menus; a native host owns its anchored menus. Tools and suites are guests.
- **Conflict resolution for menu names**: name collisions across layers are resolved by layer precedence — Layer 1 wins over Layer 2, Layer 2 over Layer 3, etc.
- **Within Layer 2: insertion order.** The first Tool to call `menu.install()` appears leftmost. Same convention as legacy. An explicit `.priority` field can be added later if a real need surfaces.
- **Per-layer hotkey scope**: hotkeys (the auto-derived first-non-duplicated-letter rule) compute **per-layer**, not globally. Each layer's sibling list runs the algorithm independently; cross-layer hotkey collisions are tolerated. Rationale: the user is always navigating inside one layer at a time when picking a hotkey, so cross-layer ambiguity doesn't manifest at the keystroke. Per-layer scope also keeps the rule mechanically simple — no "reserved letters" table threaded across layers.

### What this means for the REPL specifically

The REPL palette's `/` press shows a composed menubar:

- **Layer 1**: Frontier menu, File, Edit, Window, Help — installed at REPL startup by a new `installHostMenubar.ut` (the renamed `installReplMenubar.ut` from the discussion doc). Items inside follow the Pike pattern: structure declared in the outline, behavior in scripts under `system.menus.handlers.<host>.*`.
  - **Edit menu in v1**: present, with linenoise-aware items. Cut/Copy/Paste/Clear operate against the linenoise input buffer where the operations make sense. Undo/Redo/Find can wait for a richer input surface. Real parity now beats dimmed placeholders.
  - **Boot failure mode**: if `installHostMenubar.ut` fails, log a prominent warning and continue. The REPL stays usable via legacy `/foo` commands (`/exit`, `/help`, etc.) even without a Layer 1 menubar — this is a deliberate choice to preserve debuggability when the menubar itself is what's broken.
- **Layer 2**: Whatever's currently in `system.menus.data.<app>.*` with `.installed = true`. None at fresh launch; populated as user scripts call `menu.install()`. The legacy "Tools is dynamic" semantic is preserved here. Within-layer order is insertion order.
- **Layer 3**: Deferred. Cross-process menu publishing was a System 7-era concept tied to Apple Events; it has no current consumers. The modern equivalent (system tray / menu extras / MCP-style shims) is platform-specific and will be re-imagined when there's a real driver, not ported. A future ADR covers it.
- **Layer 4**: Skipped. The REPL has no editor windows yet, so there's no window-type to switch on. The legacy WindowTypes mechanism (Tools framework, ~2002–2003) was a separable hack on top of the core menubar; when editor windows return, Layer 4 returns with them. v1 baseline = "no editor frontmost".
- **Layer 5**: Folded into Layer 1 Help.

### What this means for verbs

`menu.*` verbs operate on Layer 2 and Layer 3 unchanged. The 6 P0 verbs that are currently no-op in headless (`menu.install`, `menu.remove`, `menu.addMenuCommand`, `menu.deleteMenuCommand`, `menu.addSubMenu`, `menu.deleteSubMenu`) become real persistence operations against `system.menus.data`. Layer 1 is just an MBAR like any other; the host installs its own at boot using the same verbs.

`shelladjustmenus()` gets a sibling `meadjustmenus_headless()` that runs the same enable/disable/label-rewrite logic against the composed menubar but writes results into the leaf records (`enabled`, `label_override`, `checked`) instead of calling Mac Menu Manager APIs. The REPL palette reads those leaf fields at render time.

### What this means for `meuserselected`

The dispatch path doesn't change in concept. `meuserselected_headless()` (proposed in the slash-menu discussion) does the parts that don't require Mac UI: `scriptbuildtree` → `langpusherrorcallback` → `newprocess` → `addprocess`. Layer 1 items dispatch through the same path — there's no separate "kernel handlers vs scripts" distinction in the headless world. Even File > Quit goes through `meuserselected_headless()` and calls `repl.exit()` via the leaf's script field.

This is a deliberate simplification of the legacy model. In the Mac code, File > Quit is hard-coded in `shellhandlemenu()` (`Common/source/shellmenu.c:1399+`); in the headless model, it's an MBAR leaf that runs `repl.exit()`. The Pike pattern (scripts everywhere) wins; the kernel-special-cases approach loses. The advantage is uniformity; the cost is that the host's menubar must be installed at startup before the user can do anything menu-driven.

---

## Consequences

### Positive

1. **One substrate, many projections.** The REPL palette, a future native menubar, a chatbot picker, and a web menu UI all render the same composed tree. New consumers don't require menu-system changes.
2. **The legacy "80-90% dynamic" semantic is preserved exactly.** The hard-coded resource floor becomes a host-installed MBAR (which the host happens to install at every startup) — semantically identical to the legacy resource layer, but now data-driven all the way down.
3. **The Pike pattern becomes universal.** Every menu item is a leaf with a script. No kernel special cases. The legacy code's File-menu-handling-in-C is a special case born of needing to bootstrap before scripts could run; headless hosts don't have that constraint.
4. **`shelladjustmenus` becomes useful headlessly.** 415 lines of context-sensitive enable/disable/label logic become a service the REPL palette consumes. Future native hosts consume the same service.
5. **Headless persistence falls out of removing 6 ifdefs.** PR 1 of the slash-menu plan stops being "design a new model" and starts being "implement the obvious model that's already there."
6. **Per-host menubars are clean.** `frontier-cli` declares its own host menubar; a future `frontier-mac` declares its own; a future `frontier-web` declares its own. Each is just an MBAR install script.

### Negative / costs

1. **Bootstrapping order matters.** The host's MBAR install script must run before the user can do anything menu-driven. If install fails, the REPL has no File menu. Mitigation: log a prominent warning and continue; the REPL stays usable via legacy `/foo` commands so the user can debug the failure. `installHostMenubar.ut` is part of the kernel boot sequence, not a user script — so failures should be rare and high-signal.
2. **Layer 4 (window-type switching) is deferred.** The REPL doesn't need it; future native does. We're deciding now to ship without it and re-introduce it when native arrives. That's a coherent decision, but it means the model is incomplete on paper today.
3. **Layer 5 (system Help) won't return.** Mac OS Carbon adds Help to the menubar via `HMGetHelpMenu()`. Headless hosts can't use that; they have to bake Help into Layer 1. Native hosts when they return will need a decision: re-enable system Help (re-introduces Layer 5) or stay with Layer-1 Help (uniform, simpler). Mark as open question for native era.
4. **Scripts now run for every menu action, including primitives like Quit.** Performance impact: negligible (script is ~5 lines, parsed once). Correctness impact: a script bug in File > Quit could prevent quit. Mitigation: the host's MBAR install script is part of the kernel boot — it's tested with the same rigor as the kernel.
5. **The Pike pattern's `runfilemenuscript()` / `runeditmenuscript()` machinery in `shellmenu.c` becomes architecturally aligned but semantically dead code in headless builds.** It's still load-bearing for legacy Mac builds. Don't delete it.

### Headless feasibility per layer (citing research K1-K5)

| Layer | Feasibility | What's required |
|-------|-------------|-----------------|
| 1 (Host-anchored) | **K2** — pure data manipulation, currently no-op'd | Remove ifdefs in 6 P0 verbs; implement `menudata_ensure_root()`; write `installHostMenubar.ut` |
| 2 (Apps/Tools) | **K1** — works as-is for data; **K2** for `menu.install` | Remove `installfunc` ifdef (`menuverbs.c:2020`) |
| 3 (Suites/IPC) | **K4** — load-bearing Mac UI in `langipcinstallmenus` | Sibling `langipcinstallmenus_headless()` if/when needed; defer |
| 4 (Window-type) | **K5** — fundamentally GUI for the switching mechanism; **K1** for the per-type outlines | Defer; re-enable when native UI host returns |
| 5 (System Help) | **K5** — fundamentally Carbon | Replace with Layer-1 Help in headless; revisit for native |

---

## Resolved questions

The following questions were raised during drafting and resolved on 2026-05-05. Two answers (Q4, Q6) diverged from the draft's recommendations; the rationale is preserved here.

1. **Per-window-type menubar switching mechanism (Layer 4).** Resolved: defer. The CLI has no editor windows in v1, so there is no window type to switch on. The legacy WindowTypes mechanism was part of the Tools framework added around 2002–2003 — separable from the core menubar, and acknowledged in retrospect as a slight hack. Per-editor-type menubar callbacks will return when external editor windows return; until then, the v1 baseline is "no editor frontmost." No further research needed before shipping.

2. **Suites vs Tools distinction.** Resolved: different mechanisms; keep both layers. Tools (Layer 2) is the in-process framework that installs MBARs into the host's menubar via `menu.install()`. Suites/IPC (Layer 3) is cross-process menu publishing — a separate program installs menus into the host's menubar through an IPC transport. Legacy Mac used Apple Events for the Suites transport; that's a System 7-era concept and not a current target.

3. **Within-layer ordering for Layer 2.** Resolved: insertion order, matching legacy. First-installed appears leftmost. Add an explicit `.priority` field later if a real need surfaces.

4. **Hotkey collision precedence across layers.** Resolved: **per-layer scope** (diverges from draft recommendation). Each layer's sibling list runs the auto-derive algorithm independently. Cross-layer hotkey collisions are tolerated. Rationale: when navigating, the user is always inside one layer at a time, so cross-layer ambiguity does not manifest at the keystroke. Per-layer scope also keeps the rule mechanically simple — no "reserved letters" table threaded across the composition.
   The draft had recommended Layer 1 hotkeys be reserved globally with Layer 2+ walking past them. That added bookkeeping for a problem that doesn't materialize at the user-experience level.

5. **Host MBAR install failure mode.** Resolved: log a prominent warning and continue (diverges from draft recommendation: fatal). The REPL stays usable via legacy `/foo` commands even when the host MBAR fails to install. Rationale: a REPL that refuses to start when the menubar script breaks is hostile to debugging the menubar script. The pragmatic choice preserves access to the system when something in the menu data is the bug.

6. **Edit menu behavior in v1.** Resolved: present, with linenoise-aware items (diverges from draft recommendation: present-but-all-disabled). Cut, Copy, Paste, and Clear operate against the linenoise input buffer where the operations make sense. Undo/Redo/Find/etc. can wait for a richer input surface or for editor windows to return. Real parity now beats dimmed placeholders.

7. **Suites/IPC in v1.** Resolved: defer. Cross-process menu publishing has no current consumers and the legacy transport (Apple Events) is gone. A modern equivalent — system tray / menu extras / MCP-style shim — will be re-imagined when there's a real driver, not ported. A future ADR covers it. Layer 3 stays in the composition model as a stub for future expansion.

---

## Alternatives considered

### Alternative A: Slack-style flat palette, ignore the menubar model

Treat `/` as a fuzzy-search command picker over a flat namespace. Don't try to mirror the menubar at all.

**Rejected because**: it abandons the directional goal toward native UI parity. Once the slash palette exists with its own UX, retrofitting menubar semantics later means breaking muscle memory or running two systems in parallel. The legacy menu model is genuinely good; the only reason to walk away from it would be if it didn't fit the data, but it does.

### Alternative B: Project only `system.menus.data`, ignore the resource-loaded layer

The original implementation plan. Treat `system.menus.data` as the whole world; the REPL menubar is whatever's there.

**Rejected because**: it doesn't preserve the host-anchored menubar semantic. Without Layer 1, the REPL has no File menu at fresh launch (since `system.menus.data` is empty in a virgin root). Either you require every user to install a File menu (broken UX) or you hard-code something in the REPL (special case that diverges from native). The 5-layer model resolves this cleanly: the host installs Layer 1 at startup as a normal MBAR, no special cases.

### Alternative C: Decide REPL projection only; defer the architectural model

Pick whatever works for the REPL today; revisit when native arrives.

**Rejected because**: the user explicitly asked for the architectural decision now, and the research surfaced enough that the decision is well-informed. Punting means re-litigating when native arrives, which means PR 1 of the slash-menu plan is built on shifting ground.

### Alternative D: Adopt a TUI framework (notcurses, termbox2, libtickit) for menu rendering

Already evaluated in the slash-menu discussion doc; rejected on substrate grounds. Doesn't change at the ADR level.

---

## Implementation implications (downstream of this ADR, not committed here)

The slash-menu implementation plan (`planning/discussions/repl-slash-menu-implementation-plan.opml`) needs updates to align with this ADR:

1. **Rename "REPL menu" to "Frontier menu" / "host menu"**, per the Layer 1 framing. The host owns its anchored menus; "REPL" is a container, not a category.
2. **Replace the proposed REPL menubar with a Layer 1 menubar**: Frontier (About / Documentation / Key Codes / Clear Variables / Quit), File (Open Database / Close), Edit (Cut / Copy / Paste / Clear, wired against the linenoise input buffer), Window (Switch / List / Jump), Help. This matches the legacy menubar shape directionally and gives v1 real Edit functionality, not dimmed placeholders.
3. **Add a `installHostMenubar.ut` implementation note** to PR 6: this script is part of kernel boot. On failure, log a prominent warning and continue; the REPL stays usable via legacy `/foo` commands.
4. **Update PR 1 framing**: it's not "decide headless menu storage" — it's "remove the headless ifdefs from the 6 P0 verbs and implement `menudata_ensure_root()`." The data model decision is THIS ADR.
5. **Add a Layer 2 dynamic enumeration spec** to PR 5: the palette walks `system.menus.data.*` with `installed = true` at every `/` press, in insertion order, plus the host's Layer 1 outline. Layer 3 is reserved as a stub but has no v1 enumeration logic.
6. **Reframe `meuserselected_headless` as universal**, not a special case. Every menu action goes through it, including Layer 1's File > Quit.
7. **Hotkey computation runs per-layer.** PR 5 implements the auto-derive algorithm independently for each layer's sibling list; no cross-layer reservation table.

These changes are clarifying, not structural. The 8-PR phasing in the discussion doc is unchanged; the framing of what each PR does is sharpened.

---

## References

- **Companion research note** (file:line citations for every claim in this ADR): `drafts/ADR-016-research-notes-menu-system-architecture.md`.
- **Discussion docs**: `planning/discussions/headless-menus-as-slash-commands.opml` (data model and product framing), `repl-ui-design-plan.opml` (user-facing UX), `repl-slash-menu-implementation-plan.opml` (implementation plan; this ADR informs revisions).
- **Legacy reference tree**: `/Users/jake/dev/tedchoward/Frontier/` (canonical Mac-era source).
- **Current tree key files**:
  - `Common/source/menuverbs.c` — verb dispatcher (`tymenutoken` enum at lines 71-101; dispatch at 1953-2174)
  - `Common/source/menubar.c` — menubar composition (`mebuildmenubar` at 930-989)
  - `Common/source/shellmenu.c` — host-side menu init and adjust (`shellinitmenus` at 285-328; `shelladjustmenus` at 946-1361)
  - `Common/source/meprograms.c` — script dispatch (`meuserselected` at 256-315)
  - `Common/source/langipcmenus.c` — Suites/IPC mechanism
  - `Common/headers/menueditor.h` — `tymenurecord` (142-193), `tymenuiteminfo` (206-214)
  - `Common/headers/menubar.h` — `tymenubarstack` (72-89)
- **Pike pattern citations** (host-defers-to-script for menu items): `Common/source/shellmenu.c:1060` (enable check), `:1085` (checked check), `:1543` (file menu script), `:1545` (edit menu script).

---

## Sign-off

Accepted on 2026-05-05.

The seven open questions raised during drafting were resolved through a structured walkthrough; outcomes are recorded in the "Resolved questions" section above. Two answers (Q4: per-layer hotkey scope; Q6: linenoise-aware Edit menu items) diverged from the draft's recommendations and the rationale for each divergence is preserved.

This document moved from `drafts/` to the main ADR directory at the time of acceptance.
