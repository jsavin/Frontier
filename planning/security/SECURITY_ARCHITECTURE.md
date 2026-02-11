# Security Architecture for Frontier

**Status**: Planning (Pre-Implementation)
**Last Updated**: 2026-02-11
**Depends On**: [Phase 4 P0a](../phase4/INDEX.md) (thread-local globals), WebSocket server
**Timeline**: Security Phases 1–5, aligned with project Phases 3–6+

---

## Overview

This document defines Frontier's security architecture from daemon safety through federation and CRDT collaboration. The design follows a progressive enhancement model: each phase adds capabilities without breaking previous ones, and single-user local mode remains unrestricted for backward compatibility.

Security is layered bottom-up, with hard boundaries enforced in the C runtime and flexible policy defined in UserTalk. This separation ensures that security primitives cannot be bypassed by scripts, while giving UserTalk developers full control over policy composition.

---

## Threat Model

### Trust Boundaries

| Boundary | Description | Example |
|----------|-------------|---------|
| Local process | Same user, same machine | Desktop app, CLI in local mode |
| Localhost network | Same machine, different process | GUI client connecting to daemon via WebSocket |
| LAN | Same network, different machine | Development team sharing a runtime |
| Internet | Untrusted network | Federated runtimes, public-facing daemon |

### Threat Actors

| Actor | Motivation | Capability |
|-------|-----------|------------|
| Curious user | Accidental damage, exploring beyond their scope | Runs scripts in REPL, opens guest databases |
| Malicious script | Untrusted UserTalk code | File/network/shell access via built-in verbs |
| Network attacker | Intercept or manipulate traffic | Plaintext sniffing, MITM on exposed daemon |
| Malicious tenant | Cross-tenant data access | Authenticated user exceeding their permissions |

### Attack Surfaces

- **Script execution**: `sys.shellCommand` (shell injection), `file.*` verbs (arbitrary file access), `tcp.*` verbs (outbound connections, port binding)
- **ODB data**: Cross-tenant reads/writes in shared databases, escalation via `root.system` modification
- **Network transport**: Plaintext interception of WebSocket traffic, MITM on daemon connections
- **Authentication**: Brute force against login endpoints, credential theft from plaintext password storage (legacy `suites.people`)
- **Federation**: Runtime impersonation, unauthorized cross-runtime access, delegation abuse

---

## Design Principles

1. **Context-dependent trust** — Local mode is unrestricted (preserves legacy behavior). Daemon and multi-user modes enforce restrictions proportional to exposure.

2. **Security primitives, not opinions** — The C runtime provides enforcement mechanisms (capability gates, ACL checks, identity propagation). UserTalk developers compose their own policies from these primitives.

3. **Defense-in-depth** — Multiple layers protect each resource. A misconfigured ACL doesn't grant shell access; a leaked token doesn't bypass capability gates.

4. **Backward compatible** — Existing single-user scripts run unchanged. No security checks fire in local mode unless explicitly configured.

5. **Progressive enhancement** — Each security phase adds capabilities without requiring changes from previous phases. Phase 1 scripts work identically after Phase 3 deploys.

6. **Local-first** — All security works offline with no cloud dependency. Federation is additive, not required.

7. **C enforces hard boundaries; UserTalk provides flexible policy** — The runtime guarantees that gates cannot be bypassed. UserTalk defines who gets what access.

---

## Security Layers (Bottom-Up)

| Layer | Enforced By | Scope |
|-------|------------|-------|
| Transport | C runtime (TLS, bind restrictions) | Network encryption, localhost-only default |
| Execution Identity | C runtime (`typrocessrecord`) | Who is running this script |
| Capability Gates | C runtime (verb-level checks) | File/network/shell access per-context |
| ODB Access Control | C runtime (path-based checks) | Read/write/execute per ODB path |
| Authentication | C + UserTalk hybrid | Token validation, session management |
| Authorization (RBAC) | UserTalk layer | Roles, groups, tenant policies |
| Audit | C runtime (event emission) | Security event logging |

Each layer operates independently. Transport encryption protects the wire regardless of whether authentication is configured. Capability gates block shell access regardless of the caller's role. This layering ensures that a failure at one level doesn't cascade.

---

## Execution Identity Model

### Core Concept

Every script execution carries a **security principal** identifying who initiated it. The principal is set when execution begins and propagates through all nested calls.

### Principal Types

| Principal | When Used | Permissions |
|-----------|----------|-------------|
| `OWNER` | Local mode, all scripts | Unrestricted (legacy behavior) |
| `ANONYMOUS` | Unauthenticated daemon connections | Minimal (read-only public data, if any) |
| `AUTHENTICATED(user_id, tenant_id, roles[])` | Authenticated daemon connections | Determined by capability config + ACLs |

### Implementation

- Add `security_principal_t*` to `typrocessrecord` / `tythreadglobals`
- Single-user local mode: all scripts run as `OWNER` — no checks, no overhead
- Daemon mode: scripts from connections run as `AUTHENTICATED` or `ANONYMOUS` based on session state
- Principal propagates through `langrunscript`, `langruncode`, and thread spawning
- Phase 4 P0a (thread-local globals) provides the infrastructure for per-thread principal storage

### UserTalk API

```
sys.getCurrentUser()      // Returns current principal's user_id (or "owner" in local mode)
sys.getCurrentRoles()     // Returns list of roles for current principal
security.getCurrentPrincipal()  // Returns full principal record
```

---

## Capability Gates (Sandboxing)

### Context-Dependent Enforcement

Capability gates are check functions called at the verb dispatch level, before the underlying C implementation executes. They consult the current execution principal and the capability configuration.

| Capability | OWNER | AUTHENTICATED | ANONYMOUS |
|-----------|-------|---------------|-----------|
| File read (any path) | ✅ | Configured | ❌ |
| File write (any path) | ✅ | Configured | ❌ |
| Shell execution (`sys.shellCommand`) | ✅ | ❌ (default) | ❌ |
| Network connect (outbound TCP) | ✅ | Configured | ❌ |
| Network listen (bind port) | ✅ | ❌ (default) | ❌ |
| ODB system areas (`root.system`) | ✅ | ❌ | ❌ |
| ODB user data | ✅ | ACL-based | ❌ |
| Process/thread management | ✅ | ✅ | ❌ |

"Configured" means the capability is denied by default but can be granted via `system.security.capabilities` in the ODB.

### Implementation

- Check functions called before `fileverbs`, `tcpverbs`, `sysshellcall` execute
- Capability configuration stored in ODB at `system.security.capabilities`
- UserTalk-layer can grant/revoke: `security.grantCapability()` / `security.revokeCapability()`
- **Critical first target**: `sysshellcall.c` — `popen()` is the highest-risk vector (shell injection)
- In `OWNER` mode, all checks return true immediately (zero overhead for local use)

---

## Phase Roadmap

### Security Phase 1: Daemon Safety

**Aligns with**: Phase 3/4 — Launch Blocking
**Depends on**: Phase 4 P0a (thread-local globals, for per-thread principal)
**Priority**: Highest — required before any network exposure

#### C Runtime Changes

- Add `security_principal_t` struct to process/thread context
- Add runtime mode flag (`--mode local|daemon`) to `cli_options_t` in `cli_parser.h`
- Capability gate on `sysshellcall.c` — block shell execution for non-OWNER principals
- Capability gate on file verbs (`fileverbs_portable.c`) — path allowlist in restricted mode
- Capability gate on network verbs (`tcpverbs.c`) — outbound connect restrictions
- Localhost-only bind default for TCP listeners in daemon mode (`INADDR_LOOPBACK` instead of `INADDR_ANY`)
- Add `--bind-address` CLI flag to override bind address

#### UserTalk Changes

- New `system.security` table for capability configuration
- `security.getMode()` — returns current runtime mode (`"local"` or `"daemon"`)
- `security.getCurrentPrincipal()` — returns identity of current execution context

#### Key Files

| File | Change |
|------|--------|
| `frontier-cli/main.c` | Mode flag parsing, principal initialization |
| `frontier-cli/cli_parser.h` | `cli_options_t` additions (`mode`, `bind_address`) |
| `Common/headers/process.h` | `security_principal_t` in thread context |
| `Common/headers/processinternal.h` | Principal access macros |
| `Common/source/sysshellcall.c` | Capability gate (highest priority) |
| `portable/fileverbs_portable.c` | Path restriction gate |
| `Common/source/tcpverbs.c` | Bind restriction, connect gate |

---

### Security Phase 2: Authentication & Sessions

**Aligns with**: GUI Protocol (WebSocket server)
**Depends on**: Security Phase 1, WebSocket server implementation

#### C Runtime Changes

- TLS support for WebSocket server (OpenSSL or LibreSSL linkage)
- Token validation in protocol handler (opaque bearer tokens)
- Session-to-principal binding: WebSocket connection → security principal
- Rate limiting on authentication endpoints (token bucket per IP)
- Add Argon2id or bcrypt to `langcrypt.c` for modern password hashing

#### UserTalk Changes (Hybrid mainResponder-Style API)

```
security.authenticate(username, password)     // → returns opaque token
security.validateToken(token)                 // → returns principal
security.createUser(username, password, email)  // wraps people.newUser + hashing
security.changePassword(username, oldPw, newPw)
security.createSession()                      // → session record
security.destroySession()                     // invalidate current session
```

- `mainResponder.members.*` verbs continue working but delegate to new infrastructure
- Password storage migration: detect plaintext in `suites.people`, upgrade to Argon2id on next login

#### Key Architectural Decision: Opaque Tokens (Not JWT)

Tokens are opaque identifiers. The server stores a token→principal mapping.

**Rationale**: Opaque tokens are revocable immediately (delete the mapping). JWTs require short expiry or revocation lists. For a local-first architecture, server-side state is acceptable — there is no need for stateless verification across distributed systems until federation. If federation later requires portable tokens, JWTs can be layered on top.

---

### Security Phase 3: Authorization & Multi-Tenancy

**Aligns with**: Multi-User support
**Depends on**: Security Phase 2, Phase 4 P0b (multi-user ready)

#### C Runtime Changes

- Path-based ACL checks in ODB access layer (`db.c`, `tableverbs.c`)
- ACL cache for performance (avoid re-checking on every ODB access)
- Tenant isolation enforcement (database-level separation for guest databases)
- Protected system paths hardcoded: `root.system`, `root.suites` → OWNER only

#### UserTalk Changes

```
security.roles                            // table: define roles with permission sets
security.acl.set(path, role, permissions) // set ACL on ODB path
security.acl.get(path)                    // get ACL for path
security.acl.check(path, action)          // check current principal's access
security.tenants                          // table: tenant definitions, admin assignments
security.groups                           // table: user groups within tenants
```

#### Role & Permission Model

**Built-in roles** (extensible by developers):

| Role | Typical Use |
|------|------------|
| `owner` | Full system access, local user |
| `admin` | Tenant administration, user management |
| `editor` | Read/write access to tenant data |
| `viewer` | Read-only access to tenant data |
| `anonymous` | Unauthenticated, minimal access |

**Permissions** (per ODB path): `read`, `write`, `execute`, `admin`

#### ACL Inheritance Model

ACLs inherit down the ODB tree, similar to filesystem permissions:

- An ACL set on `user.data` applies to `user.data.projects`, `user.data.projects.foo`, etc.
- An explicit ACL at a deeper path overrides the inherited one
- `root` defaults to OWNER-only in daemon mode
- No ACL means "inherit from parent" — the root default propagates everywhere unless overridden

---

### Security Phase 4: Federation Trust

**Aligns with**: Phase 6+ (CRDT collaboration)
**Depends on**: Security Phase 3, CRDT foundation

#### C Runtime Changes

- Mutual TLS (mTLS) for runtime-to-runtime connections
- Federation token exchange protocol
- Cross-runtime principal mapping (remote user → local delegated principal)

#### UserTalk Changes

```
security.federation.trustRuntime(url, publicKey)   // establish trust
security.federation.revokeRuntime(url)             // revoke trust
security.federation.listTrusted()                  // list trusted runtimes
```

#### Delegation Model

1. User on Runtime A requests access to Runtime B
2. Runtime B contacts Runtime A to validate the user's identity
3. Runtime B creates a local **delegated principal** with restricted permissions
4. Delegated principal inherits the intersection of: user's roles on A, trust policy on B

#### Discovery (Sketch)

Full federation discovery protocol will be designed in a separate document. Initial sketch:

- DNS SRV records for runtime discovery (`_frontier._tcp`)
- `.well-known/frontier` endpoint for capability advertisement
- Manual trust establishment first; automated discovery later

---

### Security Phase 5: Audit & Observability

**Aligns with**: All phases (can start incrementally after Phase 1)
**Depends on**: Security Phase 1 (for principal context in events)

#### C Runtime Changes

- Security event emission via existing structured logging infrastructure (`log_*` macros)
- New logging component: `LOG_COMP_SECURITY`
- Event types:

| Event | Trigger |
|-------|---------|
| `auth_attempt` | Login request received |
| `auth_success` | Successful authentication |
| `auth_failure` | Failed authentication (with IP, no password logged) |
| `access_denied` | Capability gate or ACL blocked an operation |
| `capability_blocked` | Specific capability gate fired |
| `acl_check` | ACL evaluation (configurable verbosity) |
| `session_created` | New session established |
| `session_destroyed` | Session invalidated or expired |

#### UserTalk Changes

```
security.audit.getLog(filter)          // query security events
security.audit.onEvent(callback)       // register callback for security events
system.security.audit.retentionDays    // configurable retention (default: 90)
```

---

## C vs UserTalk Layer Assignment

The division between C and UserTalk follows one rule: **if bypassing it would be a security vulnerability, it must be in C.**

| Concern | Layer | Rationale |
|---------|-------|-----------|
| Execution identity propagation | C | Must be tamper-proof, available in all contexts |
| Capability gates (file/network/shell) | C | Enforced before verb execution, not bypassable |
| ODB path ACL enforcement | C | Enforced at database layer, not bypassable |
| TLS / transport encryption | C | OpenSSL integration, performance-critical |
| Password hashing (Argon2id) | C | Crypto must be correct, not reimplementable in UserTalk |
| Rate limiting | C | Must protect auth endpoints before UserTalk processes request |
| Token validation | C | Must happen before any UserTalk execution for the request |
| Role/group definitions | UserTalk | Flexible, site-specific, extensible by developers |
| ACL policy configuration | UserTalk | Developers define their own access patterns |
| Tenant management | UserTalk | Application-level concern, builds on C primitives |
| Audit event callbacks | UserTalk | Developers define what to do with security events |
| Agent permission policies | UserTalk | Explicitly left to developers to compose |
| Federation trust policies | UserTalk | Which runtimes to trust is a policy decision |

---

## Integration with Existing Systems

### Process/Thread Context (`typrocessrecord` / `tythreadglobals`)

Add `security_principal_t*` field. Phase 4 P0a is already migrating these to thread-local storage — the security principal rides the same infrastructure. In local mode, the principal pointer is set once at startup to a static `OWNER` principal and never changes.

### `currentprocess` Global

Principal accessed via `currentprocess->principal` in local mode. In multi-threaded daemon mode, accessed via thread-local storage (same mechanism as other thread globals after P0a).

### mainResponder.members

Existing `mainResponder.members.*` verbs continue working unchanged. New `security.*` verbs provide modern infrastructure underneath. Migration path: `mainResponder.members.checkMembership` internally calls `security.validateToken` when the new infrastructure is available, falls back to legacy behavior otherwise.

### suites.people

`people.newUser` is wrapped by `security.createUser`, which adds Argon2id password hashing. Direct `people.*` calls still work for backward compatibility but store plaintext (deprecated, with log warning). On login via `security.authenticate`, plaintext passwords are detected and upgraded to Argon2id automatically.

### Guest Databases

Tenant data is naturally isolated in separate `.root` files. ACL enforcement (Phase 3) adds path-level protection within shared databases. Guest databases opened in daemon mode inherit the daemon's security mode — scripts in guest databases respect capability gates.

### Structured Logging

Security events use existing `log_*` macros with new `LOG_COMP_SECURITY` component. No new logging infrastructure required — the security layer plugs into the existing structured logging system defined in [Logging Standards](../../docs/LOGGING_STANDARDS.md).

---

## Dependencies & Sequencing

```
Phase 4 P0a (thread-local globals)
    └──→ Security Phase 1 (daemon safety)
              ├──→ Security Phase 5 (audit — can start incrementally)
              └──→ Security Phase 2 (auth) ←── WebSocket server
                        └──→ Security Phase 3 (authz) ←── Phase 4 P0b (multi-user)
                                  └──→ Security Phase 4 (federation) ←── CRDT foundation
```

### Critical Path

The critical path to a secure daemon is: **Phase 4 P0a → Security Phase 1 → Security Phase 2**. This sequence must be complete before any daemon mode is exposed beyond localhost.

### Parallel Work

- Security Phase 5 (audit) can begin as soon as Phase 1 lands — it only needs the principal context
- TLS research and OpenSSL/LibreSSL integration can begin before Phase 2 (no runtime dependencies)
- UserTalk API design for Phases 2–4 can be designed in parallel with Phase 1 implementation

---

## ADRs Needed

These ADRs should be written as each phase begins implementation:

| ADR | Topic | Write Before |
|-----|-------|-------------|
| ADR-014 | Security Principal Model — identity propagation, principal types, thread-local storage | Security Phase 1 |
| ADR-015 | Capability-Based Sandboxing — gate implementation, configuration, OWNER bypass | Security Phase 1 |
| ADR-016 | Authentication Token Architecture — opaque tokens, storage, revocation, session binding | Security Phase 2 |
| ADR-017 | ODB Path-Based ACL Model — inheritance, caching, enforcement layer, protected paths | Security Phase 3 |
| ADR-018 | Federation Trust Protocol — mutual TLS, token exchange, delegation model | Security Phase 4 |

---

## Open Questions

These should be resolved during ADR authoring for each phase:

1. **Capability gate granularity**: Should file verbs distinguish read vs write vs delete, or use a single "filesystem access" gate? (Recommendation: separate read/write at minimum.)
2. **ACL cache invalidation**: When an ACL changes, how quickly must the cache reflect it? (Recommendation: immediate invalidation, lazy repopulation.)
3. **Token storage format**: In-memory hash table vs ODB table for token→principal mapping? (Recommendation: in-memory for performance, with ODB persistence for session survival across restart.)
4. **Password hashing library**: Argon2id (modern, memory-hard) vs bcrypt (proven, widely available)? (Recommendation: Argon2id with bcrypt as fallback if Argon2 library integration is problematic.)
5. **Federation scope**: How much federation design belongs in this document vs a separate federation architecture doc? (Current approach: sketch here, full design in separate doc.)
