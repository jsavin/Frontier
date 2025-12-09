<!-- 2025-12-08 Codex: Initial outline of multi-user/headless north-star problems and rationale. -->

# Phase 4: Multi-User / Headless North-Star Problem Map

This document enumerates the concrete problems we must solve to reach a secure, multi-user, headless Frontier runtime where multiple remote users can run code against shared system code and their own data. It focuses on problem statements, constraints, and justification.

## Path & Location Semantics
- Separate the meanings of executable path, system root path, per-user data root, and guest DB roots; ban implicit use of CWD for persistence.
- Ensure `frontier.getProgramPath` returns the executable path (compat), and expose explicit verbs/globals for system root and per-user data root.
- Normalize folder utilities (`file.folderFromPath`, `file.fileFromPath`, path separators) to be platform-correct and trailing-separator-safe; avoid accidental concatenation bugs.

## Context Isolation (Per-Request / Per-User)
- Introduce an explicit runtime context object (user ID, mounts, ACL flags) threaded through evaluation; remove reliance on global singletons for “current user” or path state.
- Make the search path (`system.paths`) per-context; prevent leakage between users or threads.
- Ensure table/materialization caches respect user context and do not cross-contaminate.

## Mounting & ACLs
- Model three mount classes: system root (admin, read-only), guest DBs (admin-provisioned, policy-driven), user DB (per-user, read/write for owner).
- Enforce ACLs at the verb/DB layer: guard mutations by mount policy; reject writes to system/readonly mounts.
- Provide a deterministic mount order for lookups; avoid “last write wins” across mounts.

## Globals vs User Data
- Relocate `user.*` into per-user DBs; keep `system.*` and core code in system root; define how guest DBs contribute shared data/code.
- Audit verbs and startup scripts that assume a monolithic `user.*`; refactor to use per-user mounts.

## Startup & Initialization
- Update startup scripts to use explicit data roots (not program path/CWD) for “Guest Databases” and scratch areas.
- Define how per-user contexts are initialized on first touch (lazy load user DB, mount guest DBs, hydrate search path).

## Long-Running Tasks & Scheduling
- Tag background tasks with a run-as identity; enforce ACLs during task execution.
- Define lifecycle: how tasks inherit or drop privileges when user sessions end or are revoked.

## Authentication & Identity Hooks
- Add an abstraction for user identity in the headless daemon (pluggable auth); ensure every request/verb runs with an identity.
- Decide how identity maps to per-user data roots (local paths vs remote storage).

## Code Loading & Safety
- Clarify which code can be user-writable (likely user DB scripts) vs system/guest read-only code.
- Ensure dynamic compilation/loading uses the correct mounts and respects ACLs.

## Logging, Errors, and Observability
- Attribute logs/errors to user context; avoid leaking one user’s errors to another.
- Add tracing for mount/ACL decisions to debug access denials without overexposing data.

## Migration & Compatibility
- Maintain a single-user compatibility mode that routes through the new context (one user DB, one system root) to burn down assumptions gradually.
- Plan data migration steps: extract `user.*` into per-user DBs; re-point guest DBs; update search path defaults.

## Testing & Tooling
- Add integration tests that mount system/guest/user DBs with different ACLs and verify read/write behavior per user.
- Provide harnesses to simulate multiple users/sessions concurrently and validate isolation.

## Operational Concerns
- Define configuration for data roots and guest DB locations (env flags/config files), separate from executable deployment.
- Document expected filesystem layout and permissions for system vs user data in headless deployments.
