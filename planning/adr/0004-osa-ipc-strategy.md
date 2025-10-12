# ADR 0004 — OSA/IPC Strategy

Status
- Proposed
- Date: 2025-09-30

Context
- Classic AppleEvents/OSA integrations are UI/macOS-specific and disabled in headless.
- Need a cross-platform automation/IPC story for CLI/server modes.

Decision
- Headless builds: do not link OSA/AppleEvents. Provide JSON‑RPC over HTTP/WebSocket for automation.
- UI builds: keep legacy OSA behind an adapter boundary that can be stubbed in tests; no direct calls from core.
- Long‑term: unify on a transport‑agnostic RPC interface (stdio/socket) with HTTP/WebSocket adapters.

Consequences
- Clear contract for what is supported headless vs. UI builds.
- Testability via mock transports.

Links
- planning/headless_stubbed_behavior_matrix.md
- planning/no_ui_linkage_policy.md
