# ADR 0005 — Networking Architecture & Security

Status
- Proposed
- Date: 2025-09-30

Context
- CLI includes HTTP/WebSocket modes. Modern environments require secure defaults.

Decision
- Provide a minimal HTTP/WebSocket core with middleware hooks for auth, rate limiting, logging, and CORS.
- Default to safe bind (`127.0.0.1`), explicit `--bind` needed for external exposure; reasonable timeouts and request body limits.
- Recommend TLS termination via a reverse proxy initially; revisit in‑process TLS once core stabilizes.
- Configuration via flags/env with explicit precedence; ship secure defaults.

Consequences
- Security posture and non-goals are explicit; enables test scaffolding.

Links
- planning/1.0_phase1_cli_implementation_plan.md
- planning/1.1_phase1_implementation_summary.md
