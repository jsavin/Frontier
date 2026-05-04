# Discussions

Proposal-shaped documents written for external technical conversation — typically as starting points for design discussions with Dave or other technical collaborators.

Documents here are not decisions or plans. They are arguments and open questions. ADRs in `planning/architectural_decision_records/` are downstream of conversations that start here.

OPML files in this directory are intended to be opened in [Drummer](https://drummer.scripting.com/) for navigable outline review. Markdown companions exist for GitHub web-view readers.

## Current

- **[Headless menus as Frontier's slash command system](headless-menus-as-slash-commands.opml)** (2026-04-29) — Proposes treating the existing menu system as the scriptable command surface it has always been, with the REPL palette as the first modern projection. Reframes from "Mac UI artifact" to "the cross-host command vocabulary an app exposes to whatever is calling it." The 20 skipped tests in `menu_data_verbs.yaml` become the acceptance suite for Phase 2 of the implementation.
- **[Frontier REPL UI design plan](repl-ui-design-plan.opml)** (2026-04-29) — Companion to the menu proposal: the modal palette UI that sits atop `system.menus.data`. North star is VisiCalc on Apple ][ — text-only, full-screen, modal, instant. Iteration ladder rises from there but every rung must work in 40×24 vt100 over ssh.
- **[REPL slash-menu implementation plan](repl-slash-menu-implementation-plan.opml)** (2026-05-04) — Implementation companion to the two above: how to actually build the palette. Hand-rolled pane compositor (~600-800 LOC) over a TUI framework, cascading submenus that stay visually layered, xterm SGR 1006 mouse, lazy `menudata_ensure_root()` to fix the v7 migration gap. 8-PR phasing. Markdown companion: [repl-slash-menu-implementation-plan.md](repl-slash-menu-implementation-plan.md).
