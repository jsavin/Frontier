EFPs and Headless Routing

Overview

- External Function Processors (EFPs) are pluggable verb families (e.g., `file`, `op`, `db`) exposed to UserTalk.
- Each EFP is a hashtable under `efptable` with a `valueroutine` and a set of verb tokens.
- In classic Frontier, `system.verbs.<family>.<verb>` scripts (always in scope) call `kernelcall`, which dispatches to the EFP under `efptable`.

Headless Implementation

- There’s no resource loading or UI/system tables in headless. We programmatically register EFPs with `newfunctionprocessor("file", &valueroutine, …)` and install verbs via `langaddkeyword`.
- To keep behavior working for `file.open` in tests, headless adds a small, temporary routing shim:
  - In `langgethandlercode`, when the lookup table is `efptable` and the dotted name looks like `family.verb`, resolve `family` to a table under `efptable` and `verb` to a token within it.
  - This produces a kernel call (no code node) so `kernelfunctionvalue` executes the verb.

Temporary Test Workaround

- Until the broader UserTalk environment (e.g., `system.verbs`) is loaded in headless, tests link the EFP table (`file`) into the current scope so `file.open` resolves reliably in `langhandlercall`.
- This is a scoped workaround for testability and should be removed once `system.verbs` or equivalent is initialized in headless.

Future Work

- Default to loading `system.verbs` (from a minimal Frontier.root or generated wrappers) in headless so that routing remains `system.verbs.* -> kernelcall -> EFP`.
- Keep the dotted‑name EFP shim as a bootstrap fallback behind a runtime flag (disabled by default in shipping builds).
- Add a test hook (e.g., `kernel.call(family, verb, args)`) to exercise EFPs directly in harnesses.
- Expand tests to cover additional EFP families and ensure parity between shim and kernelcall paths.
