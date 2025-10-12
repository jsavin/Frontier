# Decision Needed: Networking Architecture & Security (ADR-0005)

Context
- CLI includes HTTP/WebSocket modes; we need secure defaults and a small, testable core.

Proposed Decision
- Minimal HTTP/WebSocket core with middleware hooks (auth, rate limiting, logging, CORS).
- Safe defaults: bind 127.0.0.1, explicit --bind for external; timeouts and body limits.
- TLS via reverse proxy initially; revisit in-process TLS later.

Alternatives
- Rich in-process server now (increases scope and attack surface).
- No network until UI refactor (blocks integration work).

Inputs Needed
- Auth requirements (tokens, local-only, etc.).
- Operational limits (max connections, request size).

Acceptance Criteria
- Documented flags/env config and defaults.
- Tests for limits and bind behavior.

Links
- planning/adr/0005-networking-architecture-and-security.md
- planning/DECISIONS.md
