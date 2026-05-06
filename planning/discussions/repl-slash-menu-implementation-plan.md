# REPL slash-menu implementation plan

**Date**: 2026-05-04 (revised 2026-05-05 to align with ADR-016)
**Author**: Jake Savin
**Companion to**: [headless-menus-as-slash-commands.opml](headless-menus-as-slash-commands.opml) (data model / product framing) and [repl-ui-design-plan.opml](repl-ui-design-plan.opml) (user-facing UX). This document is the implementation plan: how to actually build it.
**Architectural foundation**: [ADR-016](../architectural_decision_records/ADR-016-headless-menu-system-projection.md) — Headless Projection of the Frontier Menu System (5-layer model).

> The OPML version of this document ([repl-slash-menu-implementation-plan.opml](repl-slash-menu-implementation-plan.opml)) is the canonical outline form, intended for [Drummer](https://drummer.scripting.com/). This Markdown is the GitHub web-view companion.

---

## Headline decisions in this plan

These follow from ADR-016. Recorded here so the implementation can be read standalone.

1. **Canonical headless menu storage** is `system.menus.data.<app>.<menu>.<item>`. Same shape the legacy code already used; lazy-create on first verb call to fix the v7-migrated-root gap.
2. **Leaf record fields**: `label`, `script`, `description`, `enabled`, `hidden`, `shortcut`, `accepts_args`, plus `cmdkey`/`cmdmodifiers` preserved for legacy compat. `shortcut` is auto-derived (first non-duplicated letter of the label) at install time — only set explicitly when overriding.
3. **Build a hand-rolled pane compositor (~600-800 LOC)** rather than adopt a TUI framework. notcurses fits some of the requirements but its menu widget is single-level only, and it brings CMake + libunistring + libdeflate + an optional GPM broker for mouse-over-ssh.
4. **Submenus cascade visually** — parent menu stays on screen, child opens to the right at the selected row. VisiCalc / classic Mac model, not a Slack-style single-pane palette.
5. **Mouse via xterm SGR 1006 protocol.** Click on menubar / item / scroll wheel all supported. Disable cleanly on exit so terminals don't get stuck.
6. **The 6 existing slash commands** (`/exit`, `/help`, `/clear`, `/keycodes`, `/list`, `/jump`) move into the host menubar. The leading-`/` branch in `process_line()` becomes a fallback resolver against `system.menus.data`, so `/exit` ENTER continues to work as muscle memory.
7. **Hotkey rule**: within a sibling list, each item's hotkey is the first letter of its label that hasn't already been claimed by an earlier sibling at the same level. **Per-layer scope** — Layer 1's File menu and a Layer 2 Tool's "File Tools" menu compute their hotkeys independently. Resolved at install/build time, surfaced in the leaf's `shortcut` field, rendered with an underline.
8. **`meuserselected_headless` is universal.** Every menu action goes through it, including Layer 1's Quit / Open / etc. No kernel-special-cases distinction between "host menu items" and "user-installed menu items."

---

## ADR-016 alignment: the 5-layer projection model

This implementation projects the menubar through five layers in fixed order:

- **Layer 1: Host-anchored** — Frontier / File / Edit / Window / Help. Owned by the CLI host. Installed at startup by `installHostMenubar.ut`. In v1: this is what fresh-launch shows.
- **Layer 2: Apps/Tools** — Whatever's currently in `system.menus.data.<app>.*` with `.installed = true`. Populated as user scripts call `menu.install()`. **Insertion order** (left-to-right). Empty at fresh launch.
- **Layer 3: Suites/IPC** — Cross-process menu publishing. Deferred to a future ADR; not implemented in v1. Stub in the composition model.
- **Layer 4: Window-type-specific** — The menubar swaps when wptext / outline / table windows become frontmost. Deferred until editor windows return to the CLI; v1 baseline is "no editor frontmost."
- **Layer 5: System-provided** — Mac OS Help via `HMGetHelpMenu`. Folded into Layer 1 Help in headless.

**Hotkey scope is per-layer.** Each layer's sibling list runs the auto-derive algorithm independently. Cross-layer collisions are tolerated (the user is always navigating inside one layer at a time when picking a hotkey, so the ambiguity doesn't manifest at the keystroke).

**Boot failure mode is warning, not fatal.** If `installHostMenubar.ut` fails, log a prominent warning and continue. The REPL stays usable via legacy `/foo` commands so the user can debug the failure.

---

## Why hand-roll instead of a TUI framework

- **No maintained C framework ships a cascading-menu widget.** notcurses `ncmenu` is single-level. termbox2 has no widgets. libtickit has windows but no menu. We'd hand-roll the cascade in any of them.
- **notcurses's strongest feature (planes with z-order) is its biggest fit risk.** CMake 3.21+, mandatory libunistring + libdeflate, mouse-over-ssh requires a GPM broker per docs. Inverts Frontier's `make`-driven C17 / hand-rolled-CSI build model.
- **Existing infrastructure already aligns with hand-roll.** linenoise puts tty in raw mode. `terminal_control.c` emits CSI. The 100Hz poll multiplexer is the same loop shape `tb_get_fds` / `notcurses_inputready_fd` would expose. The GIL discipline (ADR-014) already constrains who writes to the screen.
- **Total scope: ~1000-1500 LOC** of well-understood C. No new build-system surface, no license/dep audit, fits the existing ANSI/raw-mode/poll architecture.
- **Plan B if the hand-rolled compositor becomes load-bearing**: vendor `termbox2.h` (single header, MIT) for the cell-buffer/double-buffer layer. Reserve as escape hatch — not adopted up front.

---

## The pane compositor (the substrate)

- A pane is `{x, y, w, h, z, title, has_border, focused, cell_buf, mouse/key callbacks}`.
- The compositor walks a z-sorted list back-to-front, copies each pane's buffer into a screen-sized framebuffer at `(x,y)`, diffs against previous frame, emits minimal CSI. Same discipline that ncplane uses; ~600 LOC in C.
- **Mouse via SGR 1006**: enable on entry (`\e[?1006h\e[?1000h`), parse `\e[<b;x;yM/m`, route hits via `compositor_pane_at(x,y)`. ~50 LOC.
- **Async output safety drops out for free**: log lines route to a designated "scrollback" pane, render thread drains queue at frame boundary, background threads NEVER write directly to stdout. Same pattern as the GIL render-thread discipline.
- This compositor is also the substrate for the future Frontier UI (concurrent script-editor / file-browser / REPL panes with a menubar floating above). The palette is the first consumer.

---

## Cascading menu rendering

- Top menu opens as a pane positioned beneath its menubar entry: `(menubar_x_of_menu, 1)`.
- Submenu opens as a **new pane to the right** of the parent, vertically aligned to the selected parent row: `(parent.x + parent.w, parent.y + parent.cursor)`.
- Each pane has its own border + cursor + selection. Parent panes remain visible — this is the visual depth requirement.
- If a submenu would overflow the right edge, it opens to the LEFT instead. Same for vertical overflow. Same pattern the Mac menu manager used.
- ESC closes the deepest open pane (pops one cascade level), not the whole palette. ESC at top closes the palette.

---

## PR 1 framing: ifdef removal + `menudata_ensure_root()`

PR 1 is **not** "decide headless menu storage." That decision is ADR-016. PR 1 is the mechanical implementation:

- 14 menu verbs in `tymenutoken` (`Common/source/menuverbs.c:71-101`).
- The structural verbs (install, addMenuCommand, addSubMenu, etc.) currently return true in headless but DON'T persist anything. `menuverbs.c:2042-2046` is the smoking gun: returns true, no side effect.
- The reading verbs (getScript, getCommandKey) return empty string in headless.
- 16 of 49 tests in `menu_data_verbs.yaml` are skipped because of this. `menu_data_verbs.yaml:1032` names the gap exactly: *"system.menus.data may not exist in migrated root7 - needs investigation."*
- **Plan**: lazy `menudata_ensure_root()` at the top of every menu verb. Remove the `FRONTIER_HEADLESS` ifdefs from the 6 P0 verbs (install, remove, addMenuCommand, addSubMenu, deleteMenuCommand, deleteSubMenu, getScript, setScript, isInstalled). 4 P1 verbs (cmdkey, clearMenubar, buildMenubar, isInstalled) follow. 2 new verbs (`menu.list`, `menu.describe`) for palette enumeration ship in PR 2.

---

## Headless menu execution

- `meuserselected` (`Common/source/meprograms.c:256-315`) is Mac-UI-bound. It calls `op_get_outlinedata` and `shellforcemenuadjust` which don't exist in headless.
- Don't try to make it headless-clean. Add a sibling: `meuserselected_headless(Handle hScript)`.
- It does the parts headless needs: `scriptbuildtree`, `langpusherrorcallback`, `newprocess` (with `mescripterrorroutine`), `addprocess`. No window-only calls.
- The palette calls this with the script string fetched from the ODB leaf. ~30 lines of C.
- **Universal dispatch**: every menu action — Layer 1's Quit, Layer 2 Tool actions, anything — goes through this path. No kernel special cases.

---

## The REPL integration point

- Single insertion at `repl.c:2123-2145`, inside the `linenoiseEditFeed` branch. Not at `process_line` level.
- After `linenoiseEditFeed` returns `linenoiseEditMore`, check `ls.len == 1 && ls.buf[0] == '/'`. If yes: backspace the `/` out of the buffer, call `palette_open(&ls)`, feed subsequent bytes to `palette_feed_byte` instead.
- The "column 1 of empty input" rule is enforced by checking `ls.len == 1` immediately after the byte was fed.
- Mid-expression `/` does nothing special — the buffer already has >1 char.
- On palette execute: `meuserselected_headless(hscript)` directly, then resume linenoise. On palette cancel (ESC at top): just resume linenoise with empty buffer.

---

## UserTalk side: default host menubar (Layer 1)

- New script: `usertalk_scripts/Frontier.root/system/menus/installHostMenubar.ut` (renamed from `installReplMenubar.ut` — the host owns its anchored menus; "REPL" is a container, not a category).
- Builds the menubar at startup with the legacy menubar shape directionally:
  - **Frontier** menu: About / Documentation / Key codes / Clear variables / Quit
  - **File** menu: Open database / Close
  - **Edit** menu: **Cut / Copy / Paste / Clear**, wired against the linenoise input buffer (real parity now, not dimmed placeholders). Undo/Redo/Find can wait for a richer input surface.
  - **Window** menu: Switch / List / Jump
  - **Help** menu: About / Documentation
- All hotkeys auto-derived at install — no `setCommandKey` calls needed. **Per-layer scope**: each menu's items run the algorithm independently against the menubar's letters and against each other.
- Handler scripts under `system.menus.handlers.host.*` reproduce the existing C-side slash-command behavior. New kernel verbs (in `frontier-cli/repl_verbs.c`) bridge the parts that today live in C: `repl.exit`, `repl.clearVariables`, `repl.jumpPath`, `repl.printKeyCodes`, `repl.list`. Plus new linenoise-buffer verbs for Edit menu wiring: `linenoise.cut`, `linenoise.copy`, `linenoise.paste`, `linenoise.clear`.
- Called from REPL init after `repl_variables_init` at `repl.c:2058`. Guarantees the palette has content on first `/` press.
- **Boot failure mode**: if `installHostMenubar.ut` fails, log a prominent warning and continue. The REPL stays usable via legacy `/foo` commands (`/exit`, `/help`, etc.) so the user can debug the failure.

---

## Hotkey auto-derivation rule

- **Scope is per-layer, per-sibling-list.** Each menu has its own hotkey namespace, and so does the menubar itself. Layer 2 Tools compute their hotkeys independently from Layer 1. Cross-layer collisions are tolerated; the user is always inside one layer at a time when picking a hotkey, so the ambiguity doesn't manifest at the keystroke.
- **Algorithm at install/build time**: walk siblings in declared order. For each label, find the first character whose uppercase form is not yet claimed in this scope. Claim it, store as the leaf's `shortcut`, and mark for underline rendering.
- **No-letter-available fallback**: if every letter in a label is already claimed, the item gets no hotkey — still navigable by arrow keys + ENTER. Log a warning at install.
- **Explicit overrides win**: authors set `shortcut` directly in the leaf (or via `menu.setCommandKey`). Explicit overrides are honored first, before the auto-walk runs, so they reserve their letter against later siblings.
- **Rendering**: hotkey character is underlined in the rendered label (ANSI SGR 4). On terminals without underline support, fall back to first-char highlight via inverse video on that single cell.

---

## Phasing — 8 PRs, mostly independent

| # | Title | Dependencies |
|---|-------|--------------|
| 1 | Headless menu storage — remove 6 P0 ifdefs + `menudata_ensure_root()` | gates everything below |
| 2 | `meuserselected_headless` (universal dispatch) + `menu.list` / `menu.describe` verbs | parallel with PR 1 |
| 3 | `terminal_control` extensions + SIGWINCH + mouse mode toggles | parallel with 1+2 |
| 4 | Pane compositor — `pane_t` + z-order + diff-renderer + SGR 1006 mouse parser | parallel with 1+2 |
| 5 | `palette.c` on top of pane compositor — cascade rendering, kbd nav, mouse routing, per-layer hotkey scope | depends on 1–4 |
| 6 | Default host menubar (Layer 1) + handler scripts + `installHostMenubar.ut` boot | depends on 5 |
| 7 | Migrate the 6 slash commands to menubar; remove `repl_commands.c` hardcoded paths | depends on 6 |
| 8 | Palette Rung 2 — filter, ANSI 16-color, accepts_args input row, scrollable submenus | independent |

**Test harness (Plan 1, written before PR 4)**: design and ship the snapshot-test infrastructure for the palette as a first-class PR. Sits between PRs 1–3 (kernel and terminal_control work) and PR 4 (compositor). Without it, every UI PR ends up validated by manual smoke tests and `/auto` can't honestly claim convergence.

---

## Risk register (top 5)

| Risk | Mitigation |
|------|------------|
| `meuserselected` being Mac-UI-bound | New sibling function, don't patch |
| Mouse leak on exit (terminal stuck in tracking mode) | `atexit` handler emits the disable sequence, plus SIGINT/SIGTERM handlers |
| Pane compositor scope creep — easy to grow into a full TUI library | Lock the API in PR 4. Rect panes, z-order, mouse hit-test, diff-render. No tabs, splits, layouts, themes, animations |
| v7 migrated roots may lack `system.menus.data` | `menudata_ensure_root()` at top of every verb. No separate migration step |
| Async output corrupting palette | Structural — log lines route to a scrollback pane, the compositor's diff-render handles it. No special-case code in `repl_async_output` |

---

## Acceptance suite

- **Kernel-side**: the 16 skipped tests in `tests/integration/test_cases/menu_data_verbs.yaml` become the PR 1+2 acceptance suite. Goal: 0 skipped after PR 2 lands.
- **Test harness layer** (Plan 1): designed and built before PR 4. Specifies cell-buffer snapshot infrastructure, synthetic input feeding, golden-file format, integration-vs-unit boundaries.
- **Palette UX**: new `tests/integration/test_cases/repl_palette.yaml` drives the palette via scripted keypresses (pexpect, like `repl_basic.yaml`). Covers cascade open/close, hotkey jump, ESC.
- **Palette mouse**: new `tests/integration/test_cases/repl_palette_mouse.yaml` covers click-on-menubar, click-on-cascaded-item, click-outside-to-close.
- **Compositor unit**: new `tests/unit/pane_compositor_test.c` snapshots the framebuffer after register/move/render to verify the diff-renderer.
- **Mouse parser unit**: new `tests/unit/mouse_parser_test.c` verifies SGR 1006 sequence parsing.
- **Regression**: existing `repl_commands.yaml` continues green throughout — `/exit` etc. resolve via the menu, but the user-visible behavior is unchanged.

---

## What I want from this discussion

1. **Schema**: is the leaf-record shape the right one for v7? Anything missing? Anything that should be split out (e.g. typed argument schema for `accepts_args`, vs the v1 "one string, parse it yourself" choice)?
2. **Substrate**: is hand-rolling the pane compositor the right call, or is there a framework I missed? notcurses fans, speak now.
3. **Cascade vs single-pane palette**: any reason to prefer Slack-style? My read is that VisiCalc-faithful is the better long-term shape because it's also what we'll want for a future GUI rendering of the same menu tree.
4. **Phasing**: 8 PRs feels right but I could split or merge. The kernel work (1+2) and the UI substrate (3+4) are independent and could land in either order.

---

## Appendix: where the full plan lives

The detailed implementation plan with file:line citations is at `~/.claude/plans/humming-painting-hejlsberg.md` (private to Jake's working tree). This document is the public-facing summary, aligned with ADR-016.
