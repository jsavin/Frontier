# REPL slash-menu implementation plan

**Date**: 2026-05-04
**Author**: Jake Savin
**Companion to**: [headless-menus-as-slash-commands.opml](headless-menus-as-slash-commands.opml) (data model / product framing) and [repl-ui-design-plan.opml](repl-ui-design-plan.opml) (user-facing UX). This document is the implementation plan: how to actually build it.

> The OPML version of this document ([repl-slash-menu-implementation-plan.opml](repl-slash-menu-implementation-plan.opml)) is the canonical outline form, intended for [Drummer](https://drummer.scripting.com/). This Markdown is the GitHub web-view companion.

---

## Headline decisions in this plan (where I want pushback before code lands)

1. **Canonical headless menu storage** is `system.menus.data.<app>.<menu>.<item>`. Same shape the legacy code already used; lazy-create on first verb call to fix the v7-migrated-root gap.
2. **Leaf record fields**: `label`, `script`, `description`, `enabled`, `hidden`, `shortcut`, `accepts_args`, plus `cmdkey`/`cmdmodifiers` preserved for legacy compat.
3. **Build a hand-rolled pane compositor (~600-800 LOC)** rather than adopt a TUI framework. notcurses fits some of the requirements but its menu widget is single-level only, and it brings CMake + libunistring + libdeflate + an optional GPM broker for mouse-over-ssh.
4. **Submenus cascade visually** — parent menu stays on screen, child opens to the right at the selected row. This is the VisiCalc / classic Mac model, not a Slack-style single-pane palette.
5. **Mouse via xterm SGR 1006 protocol.** Click on menubar / item / scroll wheel all supported. Disable cleanly on exit so terminals don't get stuck.
6. **The 6 existing slash commands** (`/exit`, `/help`, `/clear`, `/keycodes`, `/list`, `/jump`) move into a "REPL" menu in the menubar. The leading-`/` branch in `process_line()` becomes a fallback resolver against `system.menus.data`, so `/exit` ENTER continues to work as muscle memory.

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

## Headless menu storage — what's broken today

- 14 menu verbs in `tymenutoken` (`Common/source/menuverbs.c:71-101`).
- The structural verbs (install, addMenuCommand, addSubMenu, etc.) return true in headless but DON'T persist anything. `menuverbs.c:2042-2046` is the smoking gun: returns true, no side effect.
- The reading verbs (getScript, getCommandKey) return empty string in headless.
- 20 of 49 tests in `menu_data_verbs.yaml` are skipped because of this. `menu_data_verbs.yaml:1032` names the gap exactly: *"system.menus.data may not exist in migrated root7 - needs investigation."*
- **Plan**: lazy `menudata_ensure_root()` at the top of every menu verb. Promotes the 6 P0 verbs from no-op to real persistence, plus 4 P1 verbs (cmdkey, clearMenubar, buildMenubar, isInstalled), plus 2 new verbs (`menu.list`, `menu.describe`) for palette enumeration.

---

## Headless menu execution

- `meuserselected` (`Common/source/meprograms.c:256-315`) is Mac-UI-bound. It calls `op_get_outlinedata` and `shellforcemenuadjust` which don't exist in headless.
- Don't try to make it headless-clean. Add a sibling: `meuserselected_headless(Handle hScript)`.
- It does the parts headless needs: `scriptbuildtree`, `langpusherrorcallback`, `newprocess` (with `mescripterrorroutine`), `addprocess`. No window-only calls.
- The palette calls this with the script string fetched from the ODB leaf. ~30 lines of C.

---

## The REPL integration point

- Single insertion at `repl.c:2123-2145`, inside the `linenoiseEditFeed` branch. Not at `process_line` level.
- After `linenoiseEditFeed` returns `linenoiseEditMore`, check `ls.len == 1 && ls.buf[0] == '/'`. If yes: backspace the `/` out of the buffer, call `palette_open(&ls)`, feed subsequent bytes to `palette_feed_byte` instead.
- The "column 1 of empty input" rule is enforced by checking `ls.len == 1` immediately after the byte was fed.
- Mid-expression `/` does nothing special — the buffer already has >1 char.
- On palette execute: `meuserselected_headless(hscript)` directly, then resume linenoise. On palette cancel (ESC at top): just resume linenoise with empty buffer.

---

## UserTalk side: default REPL menubar

- New script: `usertalk_scripts/Frontier.root/system/menus/installReplMenubar.ut`
- Builds the menubar at startup: REPL menu (Help/Clear/List/Jump/Key codes/Exit with hotkeys H/C/L/J/K/X), Help menu, plus File/Edit/View as reserved hotkey placeholders for future PRs.
- Six handler scripts under `system.menus.handlers.repl.*` reproduce the existing C-side slash-command behavior. Five new kernel verbs (in `frontier-cli/repl_verbs.c`) bridge the parts that today live in C: `repl.exit`, `repl.clearVariables`, `repl.jumpPath`, `repl.printKeyCodes`, `repl.list`.
- Called from REPL init after `repl_variables_init` at `repl.c:2058`. Guarantees the palette has content on first `/` press.

---

## Phasing — 8 PRs, mostly independent

| # | Title | Dependencies |
|---|-------|--------------|
| 1 | Headless menu storage — canonical home + lazy-create | gates everything below |
| 2 | `meuserselected_headless` + `menu.list` / `menu.describe` verbs | parallel with PR 1 |
| 3 | `terminal_control` extensions + SIGWINCH + mouse mode toggles | parallel with 1+2 |
| 4 | Pane compositor — `pane_t` + z-order + diff-renderer + SGR 1006 mouse parser | parallel with 1+2 |
| 5 | `palette.c` on top of pane compositor — cascade rendering, kbd nav, mouse routing | depends on 1-4 |
| 6 | Default REPL menubar + handler scripts + startup install | depends on 5 |
| 7 | Migrate the 6 slash commands to menubar; remove `repl_commands.c` hardcoded paths | depends on 6 |
| 8 | Palette Rung 2 — filter, ANSI 16-color, accepts_args input row, scrollable submenus | independent |

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

- The 20 skipped tests in `tests/integration/test_cases/menu_data_verbs.yaml` become the kernel-side acceptance suite. Goal: 0 skipped after PR 1+2.
- New `tests/integration/test_cases/repl_palette.yaml` drives the palette via scripted keypresses (pexpect, like `repl_basic.yaml`). Covers cascade open/close, hotkey jump, ESC.
- New `tests/integration/test_cases/repl_palette_mouse.yaml` covers click-on-menubar, click-on-cascaded-item, click-outside-to-close.
- New `tests/unit/pane_compositor_test.c` snapshots the framebuffer after register/move/render to verify the diff-renderer.
- New `tests/unit/mouse_parser_test.c` verifies SGR 1006 sequence parsing.
- Existing `repl_commands.yaml` continues green throughout — `/exit` etc. resolve via the menu, but the user-visible behavior is unchanged.

---

## What I want from this discussion

1. **Schema**: is the leaf-record shape the right one for v7? Anything missing? Anything that should be split out (e.g. typed argument schema for `accepts_args`, vs the v1 "one string, parse it yourself" choice)?
2. **Substrate**: is hand-rolling the pane compositor the right call, or is there a framework I missed? notcurses fans, speak now.
3. **Cascade vs single-pane palette**: any reason to prefer Slack-style? My read is that VisiCalc-faithful is the better long-term shape because it's also what we'll want for a future GUI rendering of the same menu tree.
4. **Phasing**: 8 PRs feels right but I could split or merge. The kernel work (1+2) and the UI substrate (3+4) are independent and could land in either order.

---

## Appendix: where the full plan lives

The detailed implementation plan with file:line citations is at `~/.claude/plans/humming-painting-hejlsberg.md` (private to Jake's working tree). This document is the public-facing summary.
