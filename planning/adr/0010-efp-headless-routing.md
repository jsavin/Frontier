# ADR 0010 — EFP Routing in Headless

Status
- Proposed
- Date: 2025-10-01

Context
- In classic Frontier, `system.verbs.<family>.<verb>` scripts call `kernelcall`, which dispatches to the EFP in `efptable`.
- Headless doesn’t load `system.verbs` yet. Tests use a thin shim so dotted names like `file.open` resolve directly to EFPs.

Decision
- Treat the EFP dotted‑name shim as a bootstrap fallback only. Default behavior should load `system.verbs` (or a minimal subset) and route via `kernelcall`.
- Gate the shim behind a runtime flag/env (e.g., `FRONTIER_HEADLESS_BOOTSTRAP_EFP=1`) and disable by default in shipping headless builds.
- Provide an explicit internal test hook (e.g., `kernel.call(family, verb, args)`) for harnesses to exercise EFPs directly without relying on the dotted‑name shortcut.

Consequences
- Keeps a single semantic source of truth (`system.verbs -> kernelcall -> EFP`), reducing drift between headless and UI builds.
- Supports early testing before `system.verbs` loads, without baking in alternate name resolution rules.
- Requires a minimal `system.verbs` load in headless (from a stub Frontier.root or generated wrappers) for default operation.

Implementation Notes
- Phase A: Keep current shim; add feature flag and tests that assert behavior parity between shim and kernelcall.
- Phase B: Introduce a minimal `system.verbs` for headless init (stub DB or programmatic wrappers). Disable shim by default.
- Phase C: Add `kernel.call` test API; maintain regression tests to ensure identical results between paths.

Links
- planning/EFP_HEADLESS_NOTES.md
- planning/DECISIONS.md
