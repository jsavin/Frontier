# GUI Architecture

## Overview

This document describes the architecture for GUI applications that communicate with `frontier-cli` — the headless core runtime. The design prioritizes:

- **Thin client model** — GUIs are presentation layers; the runtime is authoritative
- **Protocol-first** — Well-documented APIs enable anyone to build a GUI
- **Pluggable editors** — Users can choose their preferred editing tools
- **Multi-user, multi-tenant** — Single runtime serves multiple users and workspaces
- **Federation-ready** — A frontier-cli instance can be a client to another instance

## Architecture Model

### Phase 1: Thin Client (Current Target)

```
┌─────────────────┐     ┌─────────────────────────────────┐
│   Browser GUI   │────▶│                                 │
└─────────────────┘     │                                 │
                        │        frontier-cli             │
┌─────────────────┐     │     (remote runtime)            │
│  Electron App   │────▶│                                 │
└─────────────────┘     │  - Execution engine             │
                        │  - ODB storage                  │
┌─────────────────┐     │  - User data                    │
│  Native Client  │────▶│  - Authentication               │
└─────────────────┘     │                                 │
                        └─────────────────────────────────┘
```

- Single remote runtime, authoritative for all execution
- GUIs are stateless presentation layers
- All data (system and user) resides on the server
- Well-documented protocol enables third-party GUIs

### Future: Local Data with Remote Runtime

A future evolution allows users to connect a local .root file to a remote runtime:

- Runtime executes scripts and verbs
- User's data stays local
- Runtime accesses local data via reverse channel or mounted storage
- Details TBD; protocol design should not preclude this

### Future: Federation

A frontier-cli instance can act as a client to another instance:

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│   Browser GUI   │────▶│  Local Frontier │────▶│ Remote Frontier │
└─────────────────┘     │  (user's data)  │     │ (shared runtime)│
                        └─────────────────┘     └─────────────────┘
```

The same protocol supports GUI↔Runtime and Runtime↔Runtime communication.

---

## Protocol Stack

### Request/Response: JSON-RPC 2.0

All commands use [JSON-RPC 2.0](https://www.jsonrpc.org/specification) over WebSocket.

**Example request:**
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "odb.getValue",
  "params": {
    "path": "workspace.scratchpad.helloWorld"
  }
}
```

**Example response:**
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "type": "script",
    "value": "dialog.alert(\"Hello, World!\")"
  }
}
```

**Why JSON-RPC:**
- Simple, well-specified
- Excellent library support across languages
- Natural fit for "call a verb, get a result"
- Familiar to developers who know XML-RPC

### Server-Initiated Events: WebSocket Push

State changes and UI commands are broadcast to connected clients as JSON messages (not JSON-RPC, since they have no request ID):

```json
{
  "event": "odb.updated",
  "data": {
    "path": "workspace.scratchpad.helloWorld",
    "type": "script",
    "changedBy": "user:alice"
  }
}
```

### Event Categories

| Category | Events | Description |
|----------|--------|-------------|
| ODB mutations | `odb.created`, `odb.updated`, `odb.deleted`, `odb.moved` | Object database changes |
| UI commands | `editor.open`, `dialog.show`, `quickscript.run` | Runtime requests GUI action |
| Execution | `script.started`, `script.completed`, `script.error` | Script lifecycle |
| System | `session.authenticated`, `server.shutdown` | Connection/server state |

Clients receive all events and filter locally for relevance. Subscription-based filtering may be added later if bandwidth becomes a concern.

---

## Transport Layers

frontier-cli owns all listeners directly (no separate bridge process).

### Priority Order

| Transport | Platform | Use Case | Priority |
|-----------|----------|----------|----------|
| WebSocket | All | Web GUIs, Electron, remote access | Phase 1 |
| Unix domain socket | macOS, Linux | Native clients, low overhead | Phase 2 |
| TCP localhost | Windows | Native clients | Phase 2 |

### Internal Abstraction

```c
// Transport-agnostic interface
typedef struct {
    void (*send)(client_id, const char *json, size_t len);
    void (*broadcast)(const char *json, size_t len);
    void (*close)(client_id);
} transport_ops;

// All transports deliver to one handler
void handle_client_message(client_id, const char *json, size_t len);
```

Adding a new transport means implementing ~4 functions. The protocol layer doesn't know which transport delivered the message.

---

## Editor Architecture

### Design Principles

1. **One editor per window type** — Each legacy Frontier editor (table, script, outline, etc.) becomes a distinct component
2. **Shared UI components** — Common elements (path breadcrumb, type indicators, connection status) are reusable
3. **Pluggable at multiple levels:**
   - **Component-level** — Swap a different text editor widget into the GUI shell
   - **App-level** — External editors (VS Code, etc.) connect directly to frontier-cli

### Window Types (from legacy Frontier)

| Type | Description | Starting Point |
|------|-------------|----------------|
| table | Object/hash table browser | **Phase 1** |
| script | UserTalk code editor | Phase 2 |
| outline | Hierarchical outliner | Phase 2 |
| wptext | Word processing / styled text | Phase 3 |
| menubar | Menu editor | Phase 3 |
| pictwindow | Image viewer | Phase 3 |

### Shared Components

- Object path breadcrumb / navigator
- Type indicator badges
- Dirty/saved state indicator
- Connection status
- Toolbar (common actions)
- Error/status message display

### Pluggable Editor Protocol

External editors that implement the protocol can serve as frontends:

1. Connect via WebSocket
2. Authenticate
3. Request object contents (`odb.getValue`)
4. Display/edit in native UI
5. Save changes (`odb.setValue`)
6. Receive update events, refresh as needed

This enables using Monaco, CodeMirror, VS Code, vim, emacs, or any editor that can speak WebSocket + JSON.

---

## Multi-User Model

### Architecture

```
frontier-cli (single instance)
    │
    ├── Tenant A (Manila community / workspace)
    │     ├── User alice (managing editor)
    │     └── User bob (contributor)
    │
    └── Tenant B (Manila community / workspace)
          ├── User carol (managing editor)
          └── User alice (contributor)  ← same user, different role
```

- **Runtime** — Single frontier-cli process
- **Tenant** — Independent community/workspace with its own membership (maps to Manila site concept)
- **User** — Authenticated identity, may belong to multiple tenants with different roles in each
- **Roles** — Per-tenant permissions (managing editor, contributor, read-only, etc.)
- **Groups** — Per-tenant user groupings (to be added; mainResponder lacked this)

**Key insight from Manila:** A single Frontier server hosted many independent Manila communities. Each community had its own membership pool and role assignments. User alice might be a managing editor on one site and a contributor on another. The multi-tenancy was handled transparently by mainResponder — applications above that layer didn't need to think about it.

### Existing Foundation

The authentication and multi-tenancy system builds on existing Frontier infrastructure:

| Layer | Location | Provides |
|-------|----------|----------|
| suites.people | system tables | Identity primitives |
| mainResponder | mainResponder.root | Membership system, user profiles, multi-tenancy |
| Manila | Manila.root | Role-based access (example of layering on mainResponder) |

**Goal:** Modernize mainResponder's membership model for API auth while preserving its "you don't have to think about multi-tenancy" quality.

### Authentication Flow

1. Client connects via WebSocket
2. Client sends `auth.login` with credentials (or token)
3. Server validates, returns session token
4. Subsequent requests include token in header or params
5. Server resolves user identity and tenant context for ACL checks

**Token format:** TBD (likely JWT or opaque bearer tokens)

**Tenant context:** May be part of connection URL (`wss://server/tenant-id/`) or established post-auth

### Request Context

Every request carries user identity for ACL evaluation:

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "odb.setValue",
  "params": {
    "path": "workspace.myScript",
    "value": "..."
  },
  "_auth": {
    "token": "...",
    "tenant": "workspace-a"
  }
}
```

When frontier-cli acts as a client to another instance (federation), it passes through the end-user identity — it's a proxy, not the principal.

---

## Access Control (ACL)

### Protected Areas

| Path | Access |
|------|--------|
| root.system | Runtime admins only |
| root.suites | Runtime admins only |
| root.examples | Read-only for regular users |
| root.user (server config) | Runtime admins only |
| User data areas | Owner + permitted users/groups |

### Privilege Levels

| Level | Can Do |
|-------|--------|
| Anonymous | Connect, possibly read-only access (TBD) |
| Authenticated user | Access own data, read shared resources |
| Tenant admin | Manage users/roles within tenant |
| Runtime admin | Modify system areas, manage tenants |

### ACL Evaluation

```
Request arrives with (user, tenant, path, operation)
    │
    ├── Is path in protected system area?
    │     └── Require runtime admin
    │
    ├── Is path in tenant-specific area?
    │     └── Check tenant membership + role
    │
    └── Is path in user's data area?
          └── Check ownership or explicit grant
```

Details TBD. The protocol carries identity; the runtime evaluates permissions.

---

## Session Model

### Principles

- **Client owns UI state** — What's open, scroll position, selection, etc.
- **Server is stateless per-request** — Each JSON-RPC request stands alone
- **WebSocket enables push** — Server broadcasts events; clients filter for relevance
- **Separate tabs = separate sessions** — Fine, no coordination required

### What Server Tracks

- Active WebSocket connections (for broadcasting)
- Authentication tokens (for validation)
- Nothing about what the client has open

### Implications

- Server cannot command "close all windows showing X" directly
- Instead: server broadcasts `odb.deleted` event, clients that have X open react
- This is cleaner — clients own their state, server doesn't need to track it

---

## Menubar and Callbacks

### Challenge

Legacy Frontier: Menu items trigger UserTalk scripts. Scripts can open windows, modify menus, show dialogs.

In the thin client model:
- Menu is rendered by GUI
- Selection triggers request to runtime
- Script runs on runtime
- Script may need to command GUI (open editor, show dialog)

### Solution

Bidirectional commands:

**GUI → Runtime (user selects menu item):**
```json
{
  "method": "menu.invoke",
  "params": { "path": "File.New.Script" }
}
```

**Runtime → GUI (script opens editor):**
```json
{
  "event": "editor.open",
  "data": {
    "path": "workspace.scratchpad.newScript",
    "type": "script"
  }
}
```

### Menu Definition

Menus may come from multiple sources:
- System menus (from runtime, defined in root.system)
- Tenant menus (from tenant's data)
- User menus (from user's customizations)

GUI renders merged result. Selection routes to appropriate handler.

---

## Phased Roadmap

### Phase 1: Foundation

- [ ] WebSocket server in frontier-cli
- [ ] JSON-RPC message handling
- [ ] Event broadcast infrastructure
- [ ] Basic auth (local-only mode, simple tokens)
- [ ] Table editor (ODB browser) — reference GUI implementation
- [ ] Protocol documentation

### Phase 2: Core Editors

- [ ] Script editor
- [ ] Outline editor
- [ ] Unix domain socket transport
- [ ] Improved auth (mainResponder integration)

### Phase 3: Full Feature Set

- [ ] wptext editor
- [ ] Menubar editor
- [ ] Groups in membership model
- [ ] ACL enforcement
- [ ] Multi-tenant support

### Phase 4: Advanced

- [ ] Local data with remote runtime
- [ ] Federation (frontier-cli as client)
- [ ] Subscription-based event filtering
- [ ] HTTPS/WSS with proper certificates

---

## Parking Lot

Items that are important but not blocking initial design:

- **OOBE for remote setup** — How to bootstrap admin credentials on a remote instance
- **Federated identity** — Trust model when frontier-cli proxies for users
- **Per-user data isolation** — Separate .root files? Namespaced within one .root?
- **Offline mode** — Can GUI cache data for offline viewing/editing?
- **Conflict resolution** — If local data model is adopted, how to handle conflicts
- **Rate limiting / abuse prevention** — For public-facing instances
- **Audit logging** — Who did what, when

---

## Related Documents

- `planning/gui/README.md` — Purpose of this planning area
- `docs/CLI_USAGE_GUIDE.md` — Current frontier-cli capabilities
- Legacy Frontier source: `/Users/jake/dev/tedchoward/Frontier`
- mainResponder documentation (location TBD)
