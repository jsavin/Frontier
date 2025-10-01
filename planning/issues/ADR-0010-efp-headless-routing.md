# Decision Needed: EFP Routing in Headless (ADR-0010)

Context
- Headless currently uses a dotted-name shim (`family.verb`) to dispatch directly to EFPs because `system.verbs` is not loaded.

Proposed Decision
- Make the shim a bootstrap fallback only, gated by a runtime flag/env and disabled by default.
- Default headless init should load `system.verbs` (minimal DB or generated wrappers) so routing is `system.verbs -> kernelcall -> EFP`.
- Add a dedicated test hook (e.g., `kernel.call`) for direct EFP invocation in harnesses.

Alternatives
- Keep the shim permanently (risk: diverging semantics from classic builds).
- Remove the shim immediately (slows early testing until `system.verbs` loads).

Inputs Needed
- Minimum viable `system.verbs` set for headless.
- Flag/env naming and default behavior.

Acceptance Criteria
- Feature flag implemented; default disabled in release builds.
- Headless loads minimal `system.verbs` and routes via `kernelcall`.
- Tests verify parity between shim and kernelcall.

Links
- planning/adr/0010-efp-headless-routing.md
- planning/EFP_HEADLESS_NOTES.md
- planning/DECISIONS.md
