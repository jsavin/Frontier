# Frontier Code Patterns Catalog

Status
- State: Draft
- Phase: Cross-Cutting
- Last Updated: 2025-10-11
- Notes: Initial catalog capturing recurring modernization patterns.

Related Docs
- planning/DOCS_CONVENTIONS.md
- planning/adr/0002-ui-boundary-ports-and-adapters.md
- planning/adr/0006-global-state-boundaries.md
- docs/portable_handles.md

Change Log
- 2025-10-11: Initial skeleton with pattern template and seed entries.

## Purpose

Document the architectural and implementation patterns that recur across the Frontier modernization effort. Each entry summarizes why the pattern exists, when to apply it, and where to find live code references and tests. Use this catalog as a companion to ADRs and planning notes when proposing new work or reviewing contributions.

## How to Use This Catalog

- Browse the pattern list to understand existing guardrails before editing core modules.
- Follow the template when adding new entries: supply context, rules, representative code, and validation hooks.
- Cross-link to ADRs, planning docs, and tests so future refactors can trace intent back to authoritative sources.

## Pattern Entry Template

For each new pattern, create a section using the format below:

### Pattern Name
- **Context**: Short description of the problem the pattern addresses.
- **Rules**: Bullet list of conventions or constraints contributors should follow.
- **Example**: Path to representative code (e.g., `Common/source/example.c:42`).
- **Tests**: Existing test binaries or suites that cover the behavior.
- **References**: ADRs, planning docs, or design notes that motivated the pattern.
- **Follow-up Work**: Known improvements or modernization opportunities.

## Seed Patterns (Draft)

These sections should be expanded with full details as we curate the catalog.

### Runtime Context & Scheduler Globals
- **Context**: Interpreter state is managed through module-level globals in `Common/source/lang.c` and `Common/source/process.c`, predating the planned `FrontierContext` abstraction.
- **Example**: `portable/runtime_context.c` snapshots/restores the legacy globals and is covered by `tests/runtime_context_tests.c`.
- **Follow-up Work**: Promote ADR 0006 by mapping these globals to a concrete context struct and updating tests to construct interpreter instances without global state.

### Capability Adapter (Headless vs. UI)
- **Context**: Headless builds refuse UI-only verbs via capability checks implemented in `Common/source/shell_api_headless.c` and backed by test stubs in `tests/headless_shell.c`.
- **Follow-up Work**: Align this adapter with the UIServices boundary from ADR 0002 and document required hooks for future UI ports.

### Portable Handle Allocator
- **Context**: Classic Mac handle semantics are emulated in `Common/source/portable_handles.c`, keeping handle pointers stable while buffers move.
- **Tests**: `tests/handle_tests.c` exercises allocation, resize, and lock semantics (see `handle_tests` target).
- **Follow-up Work**: Expand unit coverage and document lock-count diagnostics before reworking the allocator for reference-counted arenas or sanitizer integration.

### Portability Facade (`FRONTIER_PORTABLE`)
- **Context**: Conditional compilation swaps Carbon-era headers for portable shims (see `Common/source/lang.c` and related modules).
- **Follow-up Work**: Identify hotspots where adapters can replace `#ifdef` usage and record the sequencing needed to migrate toward platform-specific ports.

### Length-Prefixed String Handling
- **Context**: `bigstring` values (length-prefixed Pascal strings) remain the lingua franca across scripting and UI glue.
- **Follow-up Work**: Use this entry to track the UTF-8 modernization plan and link to the relevant Phase 4 string upgrade docs.
