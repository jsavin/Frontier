# Decision Needed: OSA/IPC Strategy (ADR-0004)

Context
- Legacy OSA/AppleEvents are macOS/UI specific and disabled in headless; we need a portable automation story.

Proposed Decision
- Headless: do not link OSA; expose JSON-RPC over HTTP/WebSocket.
- UI builds: keep OSA behind an adapter boundary; no direct core calls.
- Long-term: transport-agnostic RPC with HTTP/WebSocket/stdio adapters.

Alternatives
- Keep OSA everywhere (non-portable, harder to test).
- Shell-only automation (limits remote control).

Inputs Needed
- Minimal method surface for automation in Phase 1/2.
- Security constraints (authz/authn) for remote calls.

Acceptance Criteria
- Documented RPC schema and versioning (even minimal).
- Tests mocking transports for key verbs.

Links
- planning/adr/0004-osa-ipc-strategy.md
- planning/DECISIONS.md
