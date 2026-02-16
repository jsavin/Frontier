# Frontier CLI — NDJSON Stdio Protocol

| | |
|---|---|
| **Version** | 1.0.0 |
| **Status** | Implemented |
| **Last Updated** | 2026-02-15 |
| **Implementation** | `frontier-cli/protocol_handler.c` |

## 1. Overview

The NDJSON stdio protocol is a lightweight JSON-over-stdin/stdout protocol for driving a long-lived `frontier-cli` process. It was originally built to accelerate integration tests (one process per worker instead of per-test spawning), but its design intentionally aligns with the future GUI protocol (see [`PROTOCOL.md`](./PROTOCOL.md)) for reuse as a GUI↔CLI communication channel.

### 1.1 Design Principles

1. **One JSON object per line** — [NDJSON](https://github.com/ndjson/ndjson-spec) format for simple framing
2. **Request/response correlation** — Every request has an `id`; the response echoes it
3. **`op` field for routing** — Same dispatch pattern as the WebSocket GUI protocol
4. **Stdout isolation** — Protocol output goes to a saved copy of stdout; the C library's `stdout` is redirected to stderr via `dup2(STDERR_FILENO, STDOUT_FILENO)` to prevent stray `printf` from contaminating the protocol stream
5. **Stateful process** — The process keeps its database open and REPL state across evaluations; `clearContext` resets between logical sessions

### 1.2 Relationship to GUI Protocol

The GUI protocol ([`PROTOCOL.md`](./PROTOCOL.md)) specifies HTTP and WebSocket transports with the same JSON message structure (`op`, `id`, `params`, `result`, `error`). This stdio protocol uses an identical message shape over a simpler transport (stdin/stdout pipes), making it straightforward to:

- **Reuse the same dispatch logic** when the GUI connects via stdio instead of WebSocket
- **Add new operations** (e.g., `odb/get`, `odb/children`) to the stdio handler as they're implemented in the kernel
- **Bridge to WebSocket** by wrapping the stdio process in a thin proxy that translates between WebSocket frames and NDJSON lines

### 1.3 Related Documents

| Document | Description |
|----------|-------------|
| [`PROTOCOL.md`](./PROTOCOL.md) | Full GUI protocol spec (HTTP + WebSocket) |
| [`ARCHITECTURE.md`](./ARCHITECTURE.md) | GUI architecture and multi-user model |
| `frontier-cli/protocol_handler.h` | C header (supported operations, API) |
| `frontier-cli/protocol_handler.c` | C implementation |
| `tests/integration/runner.py` | Python test runner (NDJSON client) |

---

## 2. Transport

### 2.1 Activation

```bash
frontier-cli --system-root <path> --protocol
```

The `--protocol` flag puts the CLI into NDJSON protocol mode instead of interactive REPL mode.

### 2.2 Channel Layout

| Stream | Direction | Purpose |
|--------|-----------|---------|
| **stdin** | Client → CLI | NDJSON request lines |
| **stdout** | CLI → Client | NDJSON response lines |
| **stderr** | CLI → Client | Logging output (not protocol data) |

### 2.3 Stdout Isolation

On startup, the protocol handler:
1. Saves the real stdout file descriptor via `dup(STDOUT_FILENO)`
2. Redirects the C library's `stdout` to stderr via `dup2(STDERR_FILENO, STDOUT_FILENO)`
3. Writes all protocol responses to the saved fd

This prevents verb implementations that call `printf()`, `msg()`, or dialog prompts from injecting garbage into the protocol stream. Stray output goes to stderr where it can be logged alongside other diagnostic output. The saved fd is stored in `g_protocol_out`.

### 2.4 Line Framing

- Each message is exactly one line (terminated by `\n`)
- Maximum line length: 64 KB (`PROTOCOL_LINE_MAX`)
- JSON encoding: standard JSON with `\n`, `\t`, `\\`, `\"`, `\uXXXX` escapes
- Surrogate pairs (`\uD800`–`\uDBFF` + `\uDC00`–`\uDFFF`) are decoded to UTF-8

---

## 3. Message Format

### 3.1 Request (client → CLI)

```json
{"op": "<operation>", "id": <integer>, "params": {<operation-specific>}}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `op` | string | Yes | Operation name (e.g., `"script/eval"`) |
| `id` | integer | Yes | Request ID for correlation |
| `params` | object | Depends | Operation-specific parameters |

### 3.2 Success Response (CLI → client)

```json
{"id": <integer>, "result": {<operation-specific>}, "success": true}
```

### 3.3 Error Response (CLI → client)

```json
{"id": <integer>, "error": {"message": "<text>", "category": "<category>"}, "success": false}
```

### 3.4 Acknowledgement (CLI → client)

For operations with no result payload:

```json
{"id": <integer>, "success": true}
```

---

## 4. Operations

### 4.1 `script/eval` — Evaluate UserTalk Expression

Evaluates a UserTalk expression and returns the result.

**Request:**
```json
{"op": "script/eval", "id": 1, "params": {"expression": "2 + 2"}}
```

**Success response:**
```json
{"id": 1, "result": {"value": "4", "type": "long"}, "success": true}
```

**Error response:**
```json
{"id": 1, "error": {"message": "Can't compile this expression", "category": "script"}, "success": false}
```

**Notes:**
- The `value` field is always a JSON string (the result coerced via `coercetostring`)
- The `type` field indicates the original UserTalk type (e.g., `"long"`, `"string"`, `"boolean"`, `"table"`)
- Tables return `"[table]"` as their value representation
- `novaluetype` results return `null` for value and `"none"` for type
- REPL variables from previous evaluations persist until `clearContext`

### 4.2 `script/clearContext` — Reset Evaluation State

Clears all REPL variables, resets focus to root, and resets error state.

**Request:**
```json
{"op": "script/clearContext", "id": 2}
```

**Response:**
```json
{"id": 2, "success": true}
```

**What gets reset:**
- REPL variables table (emptied)
- Focus/jump path (reset to root)
- `langerrordisable` counter (reset to 0)
- `langerrorlogdisable` counter (reset to 0)
- `fllangerror` flag (reset to false)

This operation should be called between logical test sessions to prevent state leaks.

### 4.3 `shutdown` — Clean Exit

Requests the CLI process to exit cleanly.

**Request:**
```json
{"op": "shutdown", "id": 3}
```

**Response:** The process writes an ack and exits with code 0.

---

## 5. Error State Management

### 5.1 The Problem

Some verb implementations (notably `xml.frontiervaluetotaggedtext()`) use `disablelangerror()`/`enablelangerror()` internally. If an error occurs between the disable/enable calls, the global `langerrordisable` counter can be left > 0, which causes `langerrorenabled()` to return false. This prevents `try/else` blocks from catching errors in subsequent evaluations.

In a per-process model (one process per test), this doesn't matter because the process exits. In a long-lived protocol process, the stale state leaks into the next evaluation.

### 5.2 The Fix

`script/clearContext` resets all error-related globals:

```c
langerrordisable = 0;
langerrorlogdisable = 0;
fllangerror = false;
```

Clients MUST call `script/clearContext` between independent evaluation sessions.

---

## 6. Example Session

```
→ {"op":"script/eval","id":1,"params":{"expression":"2 + 2"}}
← {"id":1,"result":{"value":"4","type":"long"},"success":true}

→ {"op":"script/eval","id":2,"params":{"expression":"x = 42"}}
← {"id":2,"result":{"value":"42","type":"long"},"success":true}

→ {"op":"script/eval","id":3,"params":{"expression":"x * 2"}}
← {"id":3,"result":{"value":"84","type":"long"},"success":true}

→ {"op":"script/clearContext","id":4}
← {"id":4,"success":true}

→ {"op":"script/eval","id":5,"params":{"expression":"x"}}
← {"id":5,"error":{"message":"Can't compile this expression","category":"script"},"success":false}

→ {"op":"shutdown","id":6}
← {"id":6,"success":true}
[process exits]
```

---

## 7. Future: GUI Application over Stdio

When the GUI application connects to `frontier-cli`, it can use this same stdio protocol as the transport layer. The path forward:

1. **Add GUI protocol operations** — Implement `odb/get`, `odb/children`, `subscribe`, etc. from [`PROTOCOL.md`](./PROTOCOL.md) as new `op` handlers in `protocol_handler.c`
2. **Add server-initiated events** — The CLI can write unsolicited event lines (no `id` field) for subscription notifications, using the same format as WebSocket events
3. **Keep stdout isolation** — The `g_protocol_out` pattern ensures clean protocol output regardless of what verb implementations print
4. **Consider structured output for stderr** — Currently stderr carries unstructured log output; for GUI use, consider a structured log format or a separate logging channel

### 7.1 Key Differences from WebSocket

| Aspect | Stdio | WebSocket |
|--------|-------|-----------|
| Framing | Newline-delimited | WebSocket frames |
| Multiplexing | Single client | Multiple clients |
| Connection lifecycle | Process lifetime | Connect/disconnect |
| Authentication | Implicit (parent process) | Token-based |

For single-user desktop use, stdio is simpler and avoids the overhead of HTTP/WebSocket. Multi-user scenarios require the WebSocket transport.

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-02-15 | Initial specification (from working implementation) |
