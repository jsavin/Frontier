# ADR 0010 — EFP Routing in Headless

Status
- Proposed
- Date: 2025-10-01

Context
- In classic Frontier, `system.verbs.<family>.<verb>` scripts call `kernelcall`, which dispatches to the EFP in `efptable`.
- Headless doesn’t load `system.verbs` yet. Tests use a thin shim so dotted names like `file.open` resolve directly to EFPs.

Decision
- Treat the EFP dotted‑name shim as a bootstrap fallback only. Default behavior should load `system.verbs` and route via `kernelcall`.
- Do NOT create a persisted “minimal shim DB” if avoidable. Prefer code‑generated wrappers (from an exported manifest) that register `system.verbs` programmatically for headless bootstrap.
- Gate the shim behind a runtime flag/env (e.g., `FRONTIER_HEADLESS_BOOTSTRAP_EFP=1`) and keep it OFF by default in shipping builds.
- Provide an explicit internal test hook (e.g., `kernel.call(family, verb, args)`) for harnesses to exercise EFPs directly without relying on the dotted‑name shortcut.

Consequences
- Keeps a single semantic source of truth (`system.verbs -> kernelcall -> EFP`), reducing drift between headless and UI builds.
- Supports early testing before `system.verbs` loads, without baking in alternate name resolution rules.
- Requires a minimal `system.verbs` load in headless (from a stub Frontier.root or generated wrappers) for default operation.

Implementation Notes
- Phase A (Now): Keep current shim; add feature flag and tests that assert parity between shim and `kernelcall`.
- Phase B (Bootstrap, Temporary): Use a code‑generated wrapper layer to register a minimal `system.verbs` at startup using an exported manifest (no persisted DB). Disable shim by default.
- Phase C (Target): Load the real Frontier.root; remove the wrapper layer and delete the shim.
- Phase D (Cleanup): Keep `kernel.call` test API for direct EFP testing; enforce that dotted‑name shim is fully removed.

Temporary Scope & Removal Criteria
- Wrapper layer is explicitly temporary. Remove it when:
  1) Frontier.root loads in headless and initializes `system.verbs`, and
  2) Parity tests pass via `kernelcall` routing for covered EFP families.
- Document the flag and mark the shim/wrappers as deprecated in release notes and AGENTS.md.

Links
- planning/EFP_HEADLESS_NOTES.md
- planning/DECISIONS.md
