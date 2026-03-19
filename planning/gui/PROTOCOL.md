# Frontier GUI Protocol Specification

| | |
|---|---|
| **Version** | 1.0.1 |
| **Status** | Draft |
| **Last Updated** | 2026-02-04 |

## Change History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0.1 | 2026-02-04 | Jake Savin, Claude | Added odb/move and odb/copy operations |
| 1.0.0 | 2026-02-04 | Jake Savin, Claude | Initial specification |

---

## 1. Overview

This document specifies the JSON-based protocol for communication between GUI clients and `frontier-cli`. The protocol supports both HTTP and WebSocket transports, enabling stateless request/response patterns as well as real-time event subscriptions.

### 1.1 Design Principles

1. **REST-ish with JSON** - URL paths for routing (no body parsing needed for dispatch), all operations use POST with JSON bodies
2. **Transport Agnostic** - Same message format works over HTTP and WebSocket
3. **Stateless Friendly** - Clients can just start making requests; no mandatory handshake
4. **Small Pieces Loosely Joined** - Optional capability discovery, unknown fields ignored but preserved
5. **Graceful Degradation** - Human-readable error messages as fallback for unknown codes

### 1.2 Scope

**Version 1.0 (this document):**
- Session/capabilities
- ODB read operations (get, children)
- ODB write operations (create, setValue, delete, rename, move, copy)
- Event subscriptions

**Future versions:**
- Context menus
- Script execution
- Editor-specific protocols

### 1.3 Related Documents

- [`ARCHITECTURE.md`](./ARCHITECTURE.md) - Overall GUI architecture and multi-user model
- [`TABLE_BROWSER.md`](./TABLE_BROWSER.md) - Table browser specification and UI requirements

---

## 2. Transport Layer

The protocol supports two transport mechanisms. Both use the same JSON message format.

### 2.1 HTTP Transport

HTTP transport is stateless and suitable for simple integrations, scripting, and environments where WebSocket is unavailable.

**Base URL:** `http://localhost:5000/api/` (configurable)

**Request format:**
- Method: `POST` for all operations (except `GET /api/capabilities`)
- Content-Type: `application/json`
- Body: JSON object with operation parameters

**Example:**
```http
POST /api/odb/children HTTP/1.1
Host: localhost:5000
Content-Type: application/json
X-Frontier-Protocol-Version: 1.0

{"path": "workspace.scratchpad", "offset": 0, "limit": 100}
```

**Response format:**
- Content-Type: `application/json`
- Body: JSON object with result or error

```http
HTTP/1.1 200 OK
Content-Type: application/json
X-Frontier-Protocol-Version: 1.0

{"result": {...}, "total": 42}
```

**HTTP Status Codes:**
- `200 OK` - Successful operation (check response body for application-level errors)
- `400 Bad Request` - Malformed JSON or missing required fields
- `401 Unauthorized` - Authentication required or invalid token
- `403 Forbidden` - Authenticated but not authorized
- `404 Not Found` - Unknown endpoint
- `500 Internal Server Error` - Server-side error

**Note:** HTTP transport cannot receive server-initiated events. Use WebSocket for real-time updates.

### 2.2 WebSocket Transport

WebSocket provides bidirectional communication, enabling server-initiated events for real-time updates.

**Endpoint:** `ws://localhost:5000/ws` (configurable)

**Message format:** JSON objects with `op` field replacing URL path

**Request:**
```json
{
  "op": "odb/children",
  "id": 1,
  "params": {
    "path": "workspace.scratchpad",
    "offset": 0,
    "limit": 100
  }
}
```

**Response:**
```json
{
  "id": 1,
  "result": {...},
  "total": 42
}
```

**Error response:**
```json
{
  "id": 1,
  "error": {
    "code": 2001,
    "message": "Object not found",
    "category": "odb",
    "details": {"path": "workspace.doesNotExist"}
  }
}
```

**Server-initiated event (no `id`):**
```json
{
  "event": "odb/updated",
  "subscriptionId": "sub_abc123",
  "data": {...}
}
```

### 2.3 Message ID Requirements

- **HTTP:** No `id` field needed (one request = one response)
- **WebSocket requests:** `id` field required (integer or string)
- **WebSocket responses:** Echo back the request `id`
- **Server events:** No `id` field; include `subscriptionId` for subscription-triggered events

### 2.4 Legacy Compatibility

The existing XML-RPC endpoint at `/RPC2` remains unchanged. This protocol operates alongside it without interference.

---

## 3. Message Format

### 3.1 Request Structure (WebSocket)

```json
{
  "op": "string",           // Required: Operation name (e.g., "odb/get")
  "id": "string|number",    // Required: Request identifier for correlation
  "params": {}              // Optional: Operation-specific parameters
}
```

### 3.2 Success Response Structure

```json
{
  "id": "string|number",    // Echo of request id
  "result": {},             // Operation-specific result
  // Additional top-level fields as needed (e.g., "total" for pagination)
}
```

### 3.3 Error Response Structure

```json
{
  "id": "string|number",    // Echo of request id
  "error": {
    "code": 2001,           // Numeric error code
    "message": "Human-readable description",
    "category": "odb",      // Error category
    "details": {}           // Optional: Additional context
  }
}
```

### 3.4 Event Structure

```json
{
  "event": "string",        // Event type (e.g., "odb/updated")
  "subscriptionId": "string", // Optional: Which subscription triggered this
  "data": {}                // Event-specific payload
}
```

### 3.5 Response Schema: `result` vs `results`

- `script/eval` responses use `"result"` (singular object with `value` and `type` fields)
- ODB operations use `"results"` (plural array of per-item result objects)

This is a deliberate design decision: `script/eval` returns a single value while ODB operations are batch-capable and return one result per item.

---

## 4. Versioning

### 4.1 Header-Based Versioning

Version negotiation uses HTTP headers (for both transports).

**Request header:** `X-Frontier-Protocol-Version: 1.0`

**Response header:** `X-Frontier-Protocol-Version: 1.0`

### 4.2 Version Rules

- Absent header implies version `1.0` (backward compatible)
- Server responds with its supported version
- Minor version differences (1.0 vs 1.1) should be compatible
- Major version differences may require negotiation or rejection

### 4.3 No Version in URLs

URLs do not contain version numbers. Rationale:
- Security: Prevents probing for old, potentially vulnerable versions
- Cleaner URLs
- Header-based versioning is sufficient

---

## 5. Capabilities Endpoint

### 5.1 Purpose

Optional endpoint for feature discovery. Clients can start making requests immediately without checking capabilities first.

### 5.2 Request

```http
GET /api/capabilities HTTP/1.1
Host: localhost:5000
```

WebSocket equivalent:
```json
{"op": "capabilities", "id": 1}
```

### 5.3 Response

```json
{
  "protocolVersion": "1.0",
  "serverVersion": "0.9.0",
  "features": {
    "odb": {
      "read": true,
      "write": true,
      "move": true,
      "copy": true
    },
    "subscriptions": {
      "supported": true,
      "channels": ["odb", "system"],
      "maxSubscriptions": 100
    },
    "contextMenus": false,
    "scriptExecution": false
  },
  "limits": {
    "maxPageSize": 1000,
    "maxSubscriptionDepth": 10,
    "subscriptionTTL": 3600
  }
}
```

### 5.4 Usage Patterns

**Graceful feature detection:**
1. Try the operation
2. If it fails with "unsupported" error, fall back or disable feature
3. Use capabilities endpoint only when needed for UI hints (e.g., hiding buttons)

---

## 6. ODB Operations

### 6.1 Path Format

ODB paths use dot notation: `workspace.scratchpad.myScript`

- Root is implied (paths do not start with `root.`)
- Names containing dots must be escaped (TBD: escaping scheme)
- Empty string `""` refers to root table

### 6.2 Type Codes

| Type | Description |
|------|-------------|
| `nil` | No value / uninitialized |
| `boolean` | `true` or `false` |
| `char` | Single character |
| `short` | 16-bit signed integer |
| `long` | 32-bit signed integer |
| `float` | 32-bit floating point (deprecated, use double) |
| `double` | 64-bit floating point |
| `string` | Text string |
| `binary` | Binary data (base64 encoded in JSON) |
| `date` | Date/time value |
| `address` | Reference to ODB location |
| `table` | Hash table / nested object |
| `script` | UserTalk script |
| `outline` | Hierarchical outline |
| `wptext` | Word processor text |
| `menu` | Menu definition |
| `picture` | Image data |
| `filespec` | File system reference |
| `alias` | Symlink-like reference |
| `list` | Ordered list |
| `record` | Ordered key-value pairs |
| `objspec` | Object specifier |

### 6.3 odb/get - Get Single Object

Retrieves a single object's metadata and value.

**HTTP:**
```http
POST /api/odb/get
{"path": "workspace.scratchpad.greeting"}
```

**WebSocket:**
```json
{"op": "odb/get", "id": 1, "params": {"path": "workspace.scratchpad.greeting"}}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Dot-path to object |
| `includeValue` | boolean | No | Include value in response (default: true) |

**Response:**
```json
{
  "id": 1,
  "result": {
    "name": "greeting",
    "path": "workspace.scratchpad.greeting",
    "type": "string",
    "value": "Hello, World!",
    "created": "2026-01-15T10:30:00Z",
    "modified": "2026-02-01T14:22:00Z"
  }
}
```

**Type-specific value formats:**

| Type | JSON Representation |
|------|---------------------|
| `nil` | `null` |
| `boolean` | `true` / `false` |
| `short`, `long` | JSON number |
| `double` | JSON number |
| `string` | JSON string |
| `binary` | `{"encoding": "base64", "data": "..."}` |
| `date` | ISO 8601 string: `"2026-02-04T12:00:00Z"` |
| `address` | `{"address": "path.to.object"}` |
| `table` | `{"childCount": 42}` (value is metadata, not contents) |
| `script` | `{"lineCount": 12, "source": "..."}` or just line count |
| `outline` | `{"lineCount": 50}` |
| `wptext` | `{"length": 1500}` |
| `list` | `[item1, item2, ...]` |
| `record` | `[{"key": "k1", "value": v1}, ...]` |

**For externals (script, outline, wptext):** Value is summary only. Use type-specific endpoints (future) for full content.

### 6.4 odb/children - Get Children of Table

Retrieves children of a table with pagination and sorting.

**HTTP:**
```http
POST /api/odb/children
{
  "path": "workspace.scratchpad",
  "offset": 0,
  "limit": 100,
  "sort": "name",
  "order": "asc"
}
```

**WebSocket:**
```json
{
  "op": "odb/children",
  "id": 2,
  "params": {
    "path": "workspace.scratchpad",
    "offset": 0,
    "limit": 100,
    "sort": "name",
    "order": "asc"
  }
}
```

**Parameters:**

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `path` | string | Yes | - | Dot-path to parent table |
| `offset` | integer | No | 0 | Starting index for pagination |
| `limit` | integer | No | 100 | Maximum items to return (max: 1000) |
| `sort` | string | No | `"name"` | Sort field: `"name"`, `"type"`, `"modified"` |
| `order` | string | No | `"asc"` | Sort order: `"asc"` or `"desc"` |

**Response:**
```json
{
  "id": 2,
  "result": {
    "path": "workspace.scratchpad",
    "children": [
      {
        "name": "config",
        "type": "table",
        "value": {"childCount": 3},
        "modified": "2026-02-01T10:00:00Z"
      },
      {
        "name": "count",
        "type": "long",
        "value": 42,
        "modified": "2026-01-20T08:15:00Z"
      },
      {
        "name": "greeting",
        "type": "string",
        "value": "Hello, World!",
        "modified": "2026-02-01T14:22:00Z"
      }
    ]
  },
  "total": 3,
  "offset": 0,
  "limit": 100
}
```

**Notes:**
- `total` is the complete child count (for scroll bar sizing)
- Children include enough info to render a table row
- For tables with 100K+ items, clients should use pagination

### 6.5 odb/create - Create Object

Creates a new object in the ODB.

**HTTP:**
```http
POST /api/odb/create
{
  "path": "workspace.scratchpad",
  "name": "newItem",
  "type": "string",
  "value": "initial value"
}
```

**WebSocket:**
```json
{
  "op": "odb/create",
  "id": 3,
  "params": {
    "path": "workspace.scratchpad",
    "name": "newItem",
    "type": "string",
    "value": "initial value"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Parent table path |
| `name` | string | Yes | Name for new object |
| `type` | string | No | Type code (default: `"nil"`) |
| `value` | any | No | Initial value (type-appropriate) |
| `position` | string | No | `"first"`, `"last"` (default), or sibling name to insert after |

**Response:**
```json
{
  "id": 3,
  "result": {
    "path": "workspace.scratchpad.newItem",
    "name": "newItem",
    "type": "string",
    "created": "2026-02-04T12:00:00Z"
  }
}
```

**Errors:**
- `2002` - Parent not found
- `2003` - Name already exists
- `2004` - Invalid type
- `2010` - Permission denied

### 6.6 odb/setValue - Update Object Value

Updates an existing object's value.

**HTTP:**
```http
POST /api/odb/setValue
{
  "path": "workspace.scratchpad.greeting",
  "value": "Hello, Frontier!"
}
```

**WebSocket:**
```json
{
  "op": "odb/setValue",
  "id": 4,
  "params": {
    "path": "workspace.scratchpad.greeting",
    "value": "Hello, Frontier!"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Path to object |
| `value` | any | Yes | New value (must be compatible with object type) |

**Response:**
```json
{
  "id": 4,
  "result": {
    "path": "workspace.scratchpad.greeting",
    "modified": "2026-02-04T12:05:00Z"
  }
}
```

**Errors:**
- `2001` - Object not found
- `2005` - Type mismatch (value incompatible with object type)
- `2010` - Permission denied

### 6.7 odb/delete - Delete Object

Deletes an object from the ODB.

**HTTP:**
```http
POST /api/odb/delete
{"path": "workspace.scratchpad.oldItem"}
```

**WebSocket:**
```json
{
  "op": "odb/delete",
  "id": 5,
  "params": {"path": "workspace.scratchpad.oldItem"}
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Path to object to delete |
| `recursive` | boolean | No | Delete non-empty tables (default: false) |

**Response:**
```json
{
  "id": 5,
  "result": {
    "deleted": "workspace.scratchpad.oldItem"
  }
}
```

**Errors:**
- `2001` - Object not found
- `2006` - Table not empty (when recursive=false)
- `2010` - Permission denied
- `2011` - Cannot delete system object

### 6.8 odb/rename - Rename Object

Renames an object within its parent table.

**HTTP:**
```http
POST /api/odb/rename
{
  "path": "workspace.scratchpad.oldName",
  "newName": "newName"
}
```

**WebSocket:**
```json
{
  "op": "odb/rename",
  "id": 6,
  "params": {
    "path": "workspace.scratchpad.oldName",
    "newName": "newName"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Current path to object |
| `newName` | string | Yes | New name (must be unique in parent) |

**Response:**
```json
{
  "id": 6,
  "result": {
    "oldPath": "workspace.scratchpad.oldName",
    "newPath": "workspace.scratchpad.newName"
  }
}
```

**Errors:**
- `2001` - Object not found
- `2003` - Name already exists
- `2007` - Invalid name (empty, contains invalid characters)
- `2010` - Permission denied

### 6.9 odb/move - Move Object

Moves an object to a new location in the ODB.

**HTTP:**
```http
POST /api/odb/move
{
  "path": "workspace.scratchpad.item",
  "destination": "workspace.archive",
  "newName": "archivedItem"
}
```

**WebSocket:**
```json
{
  "op": "odb/move",
  "id": 7,
  "params": {
    "path": "workspace.scratchpad.item",
    "destination": "workspace.archive",
    "newName": "archivedItem"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Current path to object |
| `destination` | string | Yes | Path to destination table |
| `newName` | string | No | New name at destination (default: keep current name) |
| `position` | string | No | `"first"`, `"last"` (default), or sibling name to insert after |

**Response:**
```json
{
  "id": 7,
  "result": {
    "oldPath": "workspace.scratchpad.item",
    "newPath": "workspace.archive.archivedItem"
  }
}
```

**Errors:**
- `2001` - Object not found
- `2002` - Destination table not found
- `2003` - Name already exists at destination
- `2010` - Permission denied
- `2012` - Circular reference (cannot move table into itself or descendant)

### 6.10 odb/copy - Copy Object

Copies an object to a new location in the ODB.

**HTTP:**
```http
POST /api/odb/copy
{
  "path": "workspace.scratchpad.template",
  "destination": "workspace.projects.newProject",
  "newName": "config"
}
```

**WebSocket:**
```json
{
  "op": "odb/copy",
  "id": 8,
  "params": {
    "path": "workspace.scratchpad.template",
    "destination": "workspace.projects.newProject",
    "newName": "config"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Path to object to copy |
| `destination` | string | Yes | Path to destination table |
| `newName` | string | No | Name for copy (default: same as original) |
| `position` | string | No | `"first"`, `"last"` (default), or sibling name to insert after |
| `deep` | boolean | No | For tables: copy all descendants (default: true) |

**Response:**
```json
{
  "id": 8,
  "result": {
    "sourcePath": "workspace.scratchpad.template",
    "newPath": "workspace.projects.newProject.config",
    "created": "2026-02-04T12:15:00Z"
  }
}
```

**Errors:**
- `2001` - Object not found
- `2002` - Destination table not found
- `2003` - Name already exists at destination
- `2010` - Permission denied

---

## 7. Event Subscription System

### 7.1 Overview

Subscriptions enable clients to receive real-time notifications about changes. Subscriptions are explicit (opt-in) and auto-expire after a timeout.

### 7.2 Subscription Channels

| Channel | Description | Path Required |
|---------|-------------|---------------|
| `odb` | Object database changes | Yes |
| `system` | Global events (About Window, shutdown, etc.) | No |

### 7.3 subscribe - Create Subscription

**HTTP:**
```http
POST /api/subscribe
{
  "channel": "odb",
  "path": "workspace.scratchpad",
  "depth": 1,
  "events": ["created", "updated", "deleted", "moved"]
}
```

**WebSocket:**
```json
{
  "op": "subscribe",
  "id": 10,
  "params": {
    "channel": "odb",
    "path": "workspace.scratchpad",
    "depth": 1,
    "events": ["created", "updated", "deleted", "moved"]
  }
}
```

**Parameters:**

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `channel` | string | Yes | - | Subscription channel |
| `path` | string | Conditional | - | Required for `odb` channel |
| `depth` | integer | No | 1 | How deep to watch: 0=exact path only, 1=direct children, -1=all descendants |
| `events` | array | No | all | Event types to receive |

**ODB event types:**
- `created` - New object created
- `updated` - Object value changed
- `deleted` - Object deleted
- `moved` - Object renamed or moved (future)

**System event types:**
- `aboutWindow` - About Window triggered
- `shutdown` - Server shutting down
- `configChanged` - Server configuration changed

**Response:**
```json
{
  "id": 10,
  "result": {
    "subscriptionId": "sub_abc123",
    "channel": "odb",
    "path": "workspace.scratchpad",
    "expires": "2026-02-04T13:00:00Z"
  }
}
```

**Notes:**
- `subscriptionId` is used to identify events and to unsubscribe
- `expires` indicates when subscription auto-expires (typically 1 hour)
- Clients should renew subscriptions before expiry

### 7.4 unsubscribe - Remove Subscription

**HTTP:**
```http
POST /api/unsubscribe
{"subscriptionId": "sub_abc123"}
```

**WebSocket:**
```json
{
  "op": "unsubscribe",
  "id": 11,
  "params": {"subscriptionId": "sub_abc123"}
}
```

**Response:**
```json
{
  "id": 11,
  "result": {
    "unsubscribed": "sub_abc123"
  }
}
```

### 7.5 Event Delivery

Events are delivered over WebSocket only. HTTP clients must poll or use WebSocket for events.

**ODB event example:**
```json
{
  "event": "odb/updated",
  "subscriptionId": "sub_abc123",
  "data": {
    "path": "workspace.scratchpad.greeting",
    "type": "string",
    "value": "Hello, Frontier!",
    "modified": "2026-02-04T12:05:00Z",
    "changedBy": "user:alice"
  }
}
```

**ODB created event:**
```json
{
  "event": "odb/created",
  "subscriptionId": "sub_abc123",
  "data": {
    "path": "workspace.scratchpad.newItem",
    "name": "newItem",
    "type": "string",
    "value": "initial",
    "created": "2026-02-04T12:10:00Z",
    "createdBy": "user:bob"
  }
}
```

**ODB deleted event:**
```json
{
  "event": "odb/deleted",
  "subscriptionId": "sub_abc123",
  "data": {
    "path": "workspace.scratchpad.oldItem",
    "deletedBy": "user:alice"
  }
}
```

**System event example:**
```json
{
  "event": "system/shutdown",
  "data": {
    "reason": "scheduled maintenance",
    "shutdownAt": "2026-02-04T14:00:00Z"
  }
}
```

### 7.6 Subscription Renewal

To prevent expiry, clients should renew subscriptions:

```json
{
  "op": "subscribe/renew",
  "id": 12,
  "params": {"subscriptionId": "sub_abc123"}
}
```

**Response:**
```json
{
  "id": 12,
  "result": {
    "subscriptionId": "sub_abc123",
    "expires": "2026-02-04T14:00:00Z"
  }
}
```

### 7.7 Subscription Limits

- Maximum subscriptions per connection: 100 (configurable)
- Maximum depth: 10 levels
- Subscription TTL: 3600 seconds (1 hour, configurable)

---

## 8. Error Codes and Handling

### 8.1 Error Response Format

```json
{
  "error": {
    "code": 2001,
    "message": "Object not found: workspace.doesNotExist",
    "category": "odb",
    "details": {
      "path": "workspace.doesNotExist"
    }
  }
}
```

### 8.2 Error Code Ranges

| Range | Category | Description |
|-------|----------|-------------|
| 1000-1999 | Session/Auth | Authentication and session errors |
| 2000-2999 | ODB | Object database errors |
| 3000-3999 | Editor | Editor-specific errors (future) |
| 4000-4999 | Subscription | Event subscription errors |
| 5000-5999 | Script | Script execution errors (future) |

### 8.3 Session/Auth Errors (1000-1999)

| Code | Message | Description |
|------|---------|-------------|
| 1001 | Authentication required | No credentials provided |
| 1002 | Invalid credentials | Username/password incorrect |
| 1003 | Token expired | Session token has expired |
| 1004 | Token invalid | Malformed or revoked token |
| 1005 | Session not found | Session ID unknown |
| 1010 | Rate limited | Too many requests |

### 8.4 ODB Errors (2000-2999)

| Code | Message | Description |
|------|---------|-------------|
| 2001 | Object not found | Path does not exist |
| 2002 | Parent not found | Parent table does not exist |
| 2003 | Name already exists | Duplicate name in table |
| 2004 | Invalid type | Unknown or unsupported type code |
| 2005 | Type mismatch | Value incompatible with object type |
| 2006 | Table not empty | Cannot delete non-empty table |
| 2007 | Invalid name | Name is empty or contains invalid chars |
| 2008 | Invalid path | Malformed path syntax |
| 2009 | Database locked | Database is read-only or locked |
| 2010 | Permission denied | User lacks required permission |
| 2011 | Cannot modify system object | Protected system area |
| 2012 | Circular reference | Operation would create cycle |

### 8.5 Subscription Errors (4000-4999)

| Code | Message | Description |
|------|---------|-------------|
| 4001 | Subscription not found | Unknown subscription ID |
| 4002 | Subscription expired | Subscription timed out |
| 4003 | Too many subscriptions | Limit exceeded |
| 4004 | Invalid channel | Unknown subscription channel |
| 4005 | Path required | Channel requires path parameter |
| 4006 | Depth exceeds limit | Requested depth too deep |

### 8.6 Graceful Degradation

Clients should:
1. Check `code` field for programmatic handling
2. Fall back to `message` for display if code is unknown
3. Use `category` for grouping/routing errors
4. Log `details` for debugging

Unknown error codes in a category should be treated as generic errors in that category.

---

## 9. Extensibility

### 9.1 Unknown Field Handling

**Rule:** Ignore unknown fields; preserve them on round-trip.

**Rationale:** Enables forward compatibility. New protocol versions can add fields without breaking older clients.

**Example:**
```json
// Request with unknown field
{
  "op": "odb/get",
  "id": 1,
  "params": {"path": "workspace.x", "futureOption": true}
}

// Server ignores futureOption, processes normally
```

### 9.2 Adding New Operations

New operations can be added without breaking existing clients:
- Use new `op` names (e.g., `odb/move`)
- Old clients simply don't call them
- Capability discovery helps clients know what's available

### 9.3 Adding New Event Types

New event types can be added to existing channels:
- Clients that don't recognize an event type ignore it
- Event-specific data is in the `data` field

---

## 10. Security Considerations

### 10.1 Transport Security

**Production deployments MUST use:**
- HTTPS instead of HTTP
- WSS (WebSocket Secure) instead of WS

**Development/local:** Plain HTTP/WS acceptable for localhost only.

### 10.2 Authentication

Authentication details are specified in [`ARCHITECTURE.md`](./ARCHITECTURE.md).

**Summary:**
- Token-based authentication
- Token passed in header (`Authorization: Bearer <token>`) or params
- Tokens have expiration
- Failed auth returns 1001-1004 errors

### 10.3 Authorization

All operations check user permissions against the target path:
- System areas require admin privileges
- User data areas require ownership or explicit grant
- Permission denied returns error 2010

### 10.4 Input Validation

Servers MUST validate:
- Path syntax (no path traversal attacks)
- Value types match declared types
- Name uniqueness
- Size limits (value size, name length)

### 10.5 Rate Limiting

Servers SHOULD implement rate limiting:
- Per-connection request limits
- Per-user subscription limits
- Error 1010 when limits exceeded

### 10.6 No Version in URLs

URL paths do not contain version numbers:
- Prevents probing for old, vulnerable versions
- Version negotiation via headers only

---

## 11. Implementation Notes

### 11.1 Client Implementation Checklist

1. **Connection setup**
   - Support both HTTP and WebSocket transports
   - Include protocol version header
   - Handle connection failures gracefully

2. **Request handling**
   - Generate unique request IDs for WebSocket
   - Set reasonable timeouts
   - Handle partial/chunked responses

3. **Error handling**
   - Parse error responses
   - Display message to user
   - Log details for debugging
   - Handle unknown error codes gracefully

4. **Subscriptions (WebSocket)**
   - Track active subscriptions
   - Renew before expiry
   - Handle subscription expiry events
   - Clean up on disconnect

5. **Extensibility**
   - Ignore unknown fields in responses
   - Ignore unknown event types

### 11.2 Server Implementation Checklist

1. **Transport**
   - HTTP endpoint at `/api/*`
   - WebSocket endpoint at `/ws`
   - XML-RPC unchanged at `/RPC2`

2. **Message handling**
   - Parse JSON with error handling
   - Validate required fields
   - Route by path (HTTP) or `op` (WebSocket)
   - Echo request `id` in response

3. **ODB operations**
   - Validate paths
   - Check permissions
   - Return consistent response format

4. **Subscriptions**
   - Generate unique subscription IDs
   - Track per-connection subscriptions
   - Broadcast events to matching subscriptions
   - Expire stale subscriptions

5. **Events**
   - Trigger on ODB mutations
   - Include subscription ID in delivery
   - Include changed-by user info

---

## Appendix A: Quick Reference

### A.1 HTTP Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/capabilities` | Feature discovery |
| POST | `/api/odb/get` | Get single object |
| POST | `/api/odb/children` | Get children (paginated) |
| POST | `/api/odb/create` | Create object |
| POST | `/api/odb/setValue` | Update value |
| POST | `/api/odb/delete` | Delete object |
| POST | `/api/odb/rename` | Rename object |
| POST | `/api/odb/move` | Move object |
| POST | `/api/odb/copy` | Copy object |
| POST | `/api/subscribe` | Create subscription |
| POST | `/api/unsubscribe` | Remove subscription |

### A.2 WebSocket Operations

| Operation | Description |
|-----------|-------------|
| `capabilities` | Feature discovery |
| `odb/get` | Get single object |
| `odb/children` | Get children (paginated) |
| `odb/create` | Create object |
| `odb/setValue` | Update value |
| `odb/delete` | Delete object |
| `odb/rename` | Rename object |
| `odb/move` | Move object |
| `odb/copy` | Copy object |
| `subscribe` | Create subscription |
| `subscribe/renew` | Renew subscription |
| `unsubscribe` | Remove subscription |

### A.3 Event Types

| Event | Channel | Description |
|-------|---------|-------------|
| `odb/created` | odb | Object created |
| `odb/updated` | odb | Object value changed |
| `odb/deleted` | odb | Object deleted |
| `odb/moved` | odb | Object moved/renamed (future) |
| `system/shutdown` | system | Server shutting down |
| `system/aboutWindow` | system | About Window triggered |

---

## Appendix B: Example Session

### B.1 WebSocket Session Example

```
CLIENT: Connect to ws://localhost:5000/ws

CLIENT: {"op": "capabilities", "id": 1}
SERVER: {"id": 1, "result": {"protocolVersion": "1.0", ...}}

CLIENT: {"op": "subscribe", "id": 2, "params": {"channel": "odb", "path": "workspace.scratchpad", "depth": 1}}
SERVER: {"id": 2, "result": {"subscriptionId": "sub_abc123", "expires": "2026-02-04T13:00:00Z"}}

CLIENT: {"op": "odb/children", "id": 3, "params": {"path": "workspace.scratchpad"}}
SERVER: {"id": 3, "result": {"children": [...]}, "total": 5}

CLIENT: {"op": "odb/setValue", "id": 4, "params": {"path": "workspace.scratchpad.greeting", "value": "Hello!"}}
SERVER: {"id": 4, "result": {"path": "workspace.scratchpad.greeting", "modified": "..."}}

// Another client modifies the same path
SERVER: {"event": "odb/updated", "subscriptionId": "sub_abc123", "data": {"path": "workspace.scratchpad.count", ...}}

CLIENT: {"op": "unsubscribe", "id": 5, "params": {"subscriptionId": "sub_abc123"}}
SERVER: {"id": 5, "result": {"unsubscribed": "sub_abc123"}}
```

### B.2 HTTP Session Example

```http
POST /api/odb/children HTTP/1.1
Host: localhost:5000
Content-Type: application/json
X-Frontier-Protocol-Version: 1.0
Authorization: Bearer eyJ...

{"path": "workspace.scratchpad", "limit": 50}
```

```http
HTTP/1.1 200 OK
Content-Type: application/json
X-Frontier-Protocol-Version: 1.0

{
  "result": {
    "path": "workspace.scratchpad",
    "children": [...]
  },
  "total": 5,
  "offset": 0,
  "limit": 50
}
```

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0.1 | 2026-02-04 | Added odb/move and odb/copy operations |
| 1.0.0 | 2026-02-04 | Initial specification |
