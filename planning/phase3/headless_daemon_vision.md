# Headless Daemon Vision

Status
- State: Proposed
- Phase: 3
- Last Updated: 2025-10-18
- Notes: Defines the long-term target for running Frontier as a headless service and how the CLI participates.

Related Docs
- planning/phase3/frontier_root_headless_plan.md
- planning/phase3/headless_stubbed_behavior_matrix.md
- planning/phase3/DEVELOPER_QUICKSTART_HEADLESS.md
- planning/phase3/ui_abstraction/PHASES.md

## Overview

The modernization effort ultimately needs a headless Frontier kernel that can serve both
daemon-style deployments and local “God Mode” development sessions. This document captures the
desired end state so Phase 3 planning can converge on the correct long-term contracts.

At a high level:
- The kernel can run as a daemon with a single canonical `system.root` opened read-write as the owning user,
  but read-only when any other user connects to it (unless they have explicit write access via TBD mechanism).
- The daemon exposes both HTTP and stdio endpoints so a UI process (local or remote) can drive it.
- Each user can launch long-lived scripts/threads under their own identity with read-only access to
  `system.root` and read/write access to their personal databases (e.g., `User.root`).
- Administrators can attach via the UI and gain read/write access to the daemon’s runtime
  (promoting ops workflows).
- A “God Mode” desktop runtime allows developers to run a local kernel instance with full read/write
  access to all databases, effectively turning the new UI (not yet built) into an IDE for the system itself.
- One-off UserTalk scripts can still run via the CLI, automatically choosing the best execution
  target (short-lived process, local daemon, or remote daemon).

## Execution Modes & CLI Contract

The CLI remains the universal entry point:

1. **Short-lived local runtime** – default when no daemon hints are provided and a local daemon sin't found.
   The CLI hydrates the    caller’s runtime (system + user databases), runs the script, prints results to stdio,
   and exits.
2. **Attach to local daemon** – discovered automatically (well-known socket/stdio channel) or
   selected via CLI flag/environment variable. Scripts run against the daemon’s shared state.
3. **Attach to remote daemon** – controlled via explicit flags/env variables (e.g.
   `FRONTIER_DAEMON_URL`). Enables distributed workflows and container deployments.
4. **God Mode application** – the same CLI can bootstrap a local kernel with unrestricted write
   access when run in a privileged context (e.g. developer UI).

The CLI should provide consistent flag/environment resolution so scripts can be dropped into shell
pipelines, cron jobs, or container entrypoints without bespoke wrappers.

## Runtime Requirements

- **Single canonical system root**: opened read-write inside the daemon as the daemon user, and read-only
  during short-lived runs by the local user. The file name/location can be configured but defaults to a
  sanitized `system.root` on first run, and opens existing `system.root` from a well-known location if it
  already exists in the current user's context.
- **User-owned databases**: each user’s and each application's mutable state lives in separate `.root` files
  (e.g. `User.root`). User-owned worker processes open these per-user with read/write permissions.
- **Long-lived threads**: user scripts can spawn arbitrarily long-running threads on the daemon,
  constrained by ACLs but not by session lifetimes.
- **Administrator access**: the UI can connect with elevated rights to perform migrations, inspect
  state, promote changes, or otherwise modify the daemon's runtime state and databases.

## Data & Configuration Layout

- **Workspace directory**: when the CLI or daemon starts, it looks for `~/.frontier` (name TBD). If
  missing, it creates the directory, seeds configuration, and places any per-user databases there. In
  "God Mode", a sanitized `system.root` owned by the current user is also placed in the directory.
- **Config in `.root` files**: wherever possible, configuration should live in `.root` databases so
  the classic UI or God Mode runtime can edit settings directly.
- **Environment variables / flags**: runtime mode and daemon endpoints are controlled via
  environment variables (`FRONTIER_DAEMON_URL`, `FRONTIER_RUNTIME_MODE`, credential hints) and CLI
  flags. These inputs make the system container-friendly and scriptable.
- **Bootstrap order**: configuration is resolved first, then databases are hydrated (system root in
  read-only mode as needed, user roots with write access as appropriate), followed by transport
  initialization (HTTP + stdio endpoints).

## Roadmap Alignment

To reach this vision, the Phase 3 roadmap needs an explicit milestone for the “headless service
core” before the overarching project declares headless runtime “complete.” Recommended actions:

1. **Introduce Phase 3b – Headless Service Core**: 
   - Stabilize CLI hydration and table separation.
   - Implement the long-lived daemon host with HTTP/stdio transports.
   - Define mode resolution rules and authentication/ACL layers (ideally stored in a .root file).
   - Provide regression tests for daemon lifecycle and CLI attach/detach paths.
2. **Schedule database restructuring work**:
   - Draft an ADR covering the migration of `user` tables to a separate `User.root`.
   - Document multi-root hydration and ownership semantics.
3. **Expand test coverage**:
   - Create smoke tests that exercise each CLI execution mode.
   - Add daemon integration tests (local loopback first, remote later).

These items should be linked into the existing `phase3` backlog and reflected in `phase_overview.md`
so the team has a shared definition of success.

## Open Questions

- Authentication model for daemon connections (API keys, TLS, OS accounts?).
- How to sandbox user threads while preserving long-running workflows.
- Coordination between multiple daemons (federation, replication, or eventual multi-master setups).
- Lifecycle management for per-user databases in containerized deployments (volume mounts, backups).

Capturing answers in future ADRs will ensure the implementation remains aligned with this vision.
