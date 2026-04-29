# Discussions

Proposal-shaped documents written for external technical conversation — typically as starting points for design discussions with Dave or other technical collaborators.

Documents here are not decisions or plans. They are arguments and open questions. ADRs in `planning/architectural_decision_records/` are downstream of conversations that start here.

OPML files in this directory are intended to be opened in [Drummer](https://drummer.scripting.com/) for navigable outline review. Markdown companions exist for GitHub web-view readers.

## Current

- **[Headless menus as Frontier's slash command system](headless-menus-as-slash-commands.opml)** (2026-04-29) — Proposes treating the existing menu system as the slash-command surface for REPL, HTTP, chatbots, and AI agents rather than as a UI artifact. The 20 skipped tests in `menu_data_verbs.yaml` become the acceptance suite for Phase 2 of the implementation.
- **[Frontier REPL UI design plan](repl-ui-design-plan.opml)** (2026-04-29) — Companion to the menu proposal: the modal palette UI that sits atop `system.menus.data`. North star is VisiCalc on Apple ][ — text-only, full-screen, modal, instant. Iteration ladder rises from there but every rung must work in 40×24 vt100 over ssh.
