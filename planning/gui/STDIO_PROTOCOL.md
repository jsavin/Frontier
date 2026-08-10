# Frontier CLI — NDJSON Stdio Protocol

| | |
|---|---|
| **Version** | 1.2.0 |
| **Status** | Implemented |
| **Last Updated** | 2026-08-09 |
| **Implementation** | `frontier-cli/protocol_handler.c` (framing), `frontier-cli/op_handler.c` (dispatch + script/odb ops), `frontier-cli/debug_handler.c` (debug ops + notifications), `frontier-cli/odb_ops.c` (per-item odb results) |
| **Contract tests** | `tests/integration/test_cases/protocol_contract_tests.yaml`, `tests/integration/test_cases/protocol_odb_save.yaml` (persistence) |

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
- A request line exceeding the maximum is discarded through its terminating newline and answered with a single `{"id":null, "error":{"code":"line_too_long", ...}, "success":false}` response; the session remains usable
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

Envelope validation order: `id` is checked first, then `op`. A request
missing (or mistyping) both gets the `missing_id` error.

### 3.2 Success Response (CLI → client)

```json
{"id": <integer>, "result": {<operation-specific>}, "success": true}
```

### 3.3 Error Response (CLI → client)

```json
{"id": <integer|null>, "error": {"code": "<code>", "message": "<text>"}, "success": false}
```

`error.code` is a stable machine-readable code from the set in §5. `id` is
`null` when the request's id could not be recovered (unparseable JSON,
missing/mistyped id, oversized line). `script/eval` errors may additionally
carry `error.location`, `error.stack`, and `error.causedBy` (see §4.1).

### 3.4 Acknowledgement (CLI → client)

For operations with no result payload:

```json
{"id": <integer>, "success": true}
```

### 3.5 Notification (CLI → client, server-initiated)

Debug threads emit unsolicited notification lines. They are distinguishable
from responses by `"id": null` **plus** the presence of an `op` field:

```json
{"id": null, "op": "debug/suspended", "params": {"threadId": 3, "line": 0, "reason": "entry"}}
{"id": null, "op": "debug/completed", "params": {"threadId": 3, "success": true}}
```

Clients must tolerate notification lines interleaved between a request and
its response. `debug/suspended` reasons: `entry`, `interrupted`,
`breakpoint`, `step`, `watchpoint`, `error`. `debug/suspended` includes a
`script` field when the suspended statement is inside a named script.
`debug/completed` fires on normal completion (`success: true`) and on kill
or error (`success: false`).

---

## 4. Operations

Dispatch table (`op_handler.c::op_dispatch`), 23 operations:

| Group | Operations |
|-------|------------|
| Script | `script/eval`, `script/clearContext` |
| ODB | `odb/get`, `odb/set`, `odb/list`, `odb/delete`, `odb/save` |
| Debug execution | `debug/run`, `debug/step`, `debug/continue`, `debug/kill`, `debug/pause` |
| Breakpoints | `debug/setBreakpoint`, `debug/listBreakpoints`, `debug/clearBreakpoints` |
| Inspection | `debug/getLocals`, `debug/getSource`, `debug/getStack`, `debug/listThreads` |
| Watchpoints | `debug/setWatchpoint`, `debug/listWatchpoints`, `debug/clearWatchpoints` |
| Lifecycle | `shutdown` |

There are no dedicated sync ops: `.ut` sync is driven through UserTalk verbs
(`repl.syncScan()` etc.) via `script/eval`.

### 4.1 `script/eval` — Evaluate UserTalk Expression

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
{"id": 1, "error": {"code": "script_error", "message": "...", "location": {"script": "<eval>", "line": 1, "column": 3, "tokenStart": 2, "tokenEnd": 3}, "stack": [{"script": "<eval>", "line": 1, "column": 3}]}, "success": false}
```

**Notes:**
- The `value` field is always a JSON string (the result coerced via `coercetostring`)
- The `type` field indicates the original UserTalk type (e.g., `"long"`, `"string"`, `"boolean"`, `"table"`)
- Tables return `"[table]"` as their value representation
- `novaluetype` results return `null` for value and `"none"` for type
- REPL variables from previous evaluations persist until `clearContext`
- Missing/non-string `expression` → `bad_params`; compile and runtime failures → `script_error`
- `error.location` / `error.stack` / `error.causedBy` are attached when the runtime captured them
- KNOWN GAP: bare runtime errors (e.g. `scriptError("x")` outside try/else) currently report the generic message `"Script evaluation failed"` — the real message is lost on the protocol eval path (#618/#620 context); location/stack are still populated

### 4.2 `script/clearContext` — Reset Evaluation State

**Request:** `{"op": "script/clearContext", "id": 2}` — extra params are ignored.

**Response:** bare ack `{"id": 2, "success": true}`.

**What gets reset:** REPL variables table (emptied), focus/jump path (reset
to root), `langerrordisable` / `langerrorlogdisable` counters,
`fllangerror` flag, TCP rate-limit window. Breakpoints and watchpoints are
NOT cleared (use `debug/clearBreakpoints` / `debug/clearWatchpoints`).

### 4.3 ODB Operations — `odb/get`, `odb/set`, `odb/list`, `odb/delete`

All four share batch semantics: `params.items` is always an array (max
1000 items, else `batch_too_large`), and the response envelope is always
`{"id": N, "success": true, "results": [...]}` with one result object per
item, in order. Per-item failures do NOT fail the envelope.

`odb/set` and `odb/delete` response envelopes additionally carry a
top-level `"dirty"` boolean: the unsaved-changes state of the system root
after the batch (`true` = a save is pending; see §4.4 for the persistence
contract). Read-only ops (`odb/get`, `odb/list`) do not carry it.

**Envelope errors:** missing/non-array `items` → `bad_params`.

**Per-item error shape** (note: per-item errors carry `message` only — the
stable codes of §5 apply to top-level `error` objects):

```json
{"path": "<echoed path>", "error": {"message": "Path not found"}, "success": false}
```

**Per-item success shapes:**

| Op | Item request fields | Item success result |
|----|---------------------|---------------------|
| `odb/get` | `path` (required) | `{"path", "name", "type", "value", "success": true}` |
| `odb/set` | `path` (required), `type` (optional, inferred from JSON value when omitted), `value` | `{"path", "success": true}` |
| `odb/list` | `path` (required), `depth` (default 1, `-1` recursive), `maxResults` (default 10000) | `{"path", "entries": [{"name", "path", "type"}...], "truncated"?: true, "success": true}` |
| `odb/delete` | `path` (required) | `{"path", "success": true}` |

Item-level validation: missing/non-string `path` → per-item
`{"error": {"message": "Missing 'path'"}, "success": false}`. Paths are
restricted to a letters/digits/dots allowlist (spaces, hyphens, semicolons
rejected per item). `odb/set` cannot create tables (use `script/eval` with
`new(tableType, @path)`). Mutations are **in-memory until an explicit
`odb/save`** — see §4.4.

### 4.4 `odb/save` — Persist the System Root

**Request:** `{"op": "odb/save", "id": 7}` — no parameters (all odb paths
resolve within the loaded system root, so it is the only addressable save
target; guest databases save via `script/eval` `db.save()`). Extra params
are ignored, like `script/clearContext`.

**Success response:**
```json
{"id": 7, "result": {"root": "Frontier.root", "saved": true, "dirty": false}, "success": true}
```

`saved` is `false` when the root was already clean — the save is skipped
entirely (no blocks rewritten). `dirty` is the post-save state (normally
`false`; if `true`, something re-dirtied during pack and the client may
save again). Implementation reuses the exact save path behind
`fileMenu.save()` (`filemenu_save_systemroot`), so the protocol op and the
UserTalk verb cannot drift apart. The process remains open and usable
after the save.

**Errors:**

| Condition | Code |
|-----------|------|
| `--lock-opened-roots` / `FRONTIER_LOCK_OPENED_ROOTS=1` active | `locked` |
| No system root loaded | `bad_state` |
| A debug thread was killed this session (pack traversal unsafe — same guard as the exit save) | `bad_state` |
| Save failed (disk/pack error) | `internal_error` |

**Persistence contract** (proven by `protocol_odb_save.yaml`):

`odb/set` / `odb/delete` mutations live in memory only. Loss of unsaved
mutations on process termination is **by design** — `odb/save` is how a
client opts into durability. The full matrix:

| Root mode | `odb/save` | Clean exit (`shutdown` op) | Abnormal termination |
|-----------|-----------|----------------------------|----------------------|
| Read-write (default) | persists immediately | legacy exit save persists unsaved mutations (issue #127) | unsaved mutations LOST |
| Locked (`--lock-opened-roots`) | refused with `locked`; file untouched | nothing saved (flag's contract) | nothing saved |

A GUI/agent client that must not lose data should treat `dirty: true` in
any `odb/set`/`odb/delete` response as "call `odb/save` before exit" and
must not rely on the clean-exit save (a crash forfeits it).

### 4.5 `debug/run` — Spawn a Debuggable Script Thread

**Request:** `{"op": "debug/run", "id": 5, "params": {"expression": "<UserTalk>"}}`

**Success response:** `{"id": 5, "result": {"threadId": 3, "status": "started"}, "success": true}`

The thread starts **suspended before its first statement**; a
`debug/suspended` notification with `reason: "entry"`, `line: 0` follows.
Set breakpoints, then `debug/continue` or `debug/step`.

**Errors:** missing `expression` → `bad_params`; compile failure →
`script_error` (`"Compilation failed: <msg>"`); spawn failure →
`internal_error`.

### 4.6 `debug/continue`, `debug/step`, `debug/pause`, `debug/kill`

All take `params.threadId` (number). Shared errors: missing/non-numeric
`threadId` → `bad_params`; unknown thread → `not_found`
(`"No debug thread with that ID"`).

| Op | Precondition | Success result | State errors (`bad_state`) |
|----|--------------|----------------|----------------------------|
| `debug/continue` | — | `{"threadId", "status": "running"}` | — |
| `debug/step` | thread suspended | `{"threadId", "status": "stepping"}` | `"Thread N is not suspended"` |
| `debug/pause` | thread running | `{"threadId", "status": "interrupting"}` | `"Thread N is already suspended"` |
| `debug/kill` | — | `{"threadId", "status": "killed"}` | — |

`debug/step` takes optional `direction`: `"over"` (default), `"into"`,
`"out"`; anything else → `bad_params`. After `step`, a `debug/suspended`
notification with `reason: "step"` follows; after `pause`, one with
`reason: "interrupted"` (at the next executed statement). A killed thread
emits `debug/completed` with `success: false`.

### 4.7 `debug/setBreakpoint`, `debug/listBreakpoints`, `debug/clearBreakpoints`

**setBreakpoint** — toggle semantics: setting an existing script+line
clears it.

Request params: `script` (dotted path, leading `@` stripped), `line`
(integer 1–1000000), optional `condition` (UserTalk expression).

Success result: `{"action": "set"|"cleared", "script", "line", "condition"?}`.

Errors: missing/non-string `script`, missing/non-numeric `line` →
`bad_params`; non-integer or out-of-range line → `bad_params`
(`"Line must be a positive integer"`); path ≥ 256 bytes → `bad_params`;
all 256 slots full → `limit_exceeded`.

**listBreakpoints** — no params. Result:
`{"breakpoints": [{"script", "line", "type": "session", "condition"?}...]}`.

**clearBreakpoints** — no params. Result: `{"cleared": <count>}`.

### 4.8 `debug/getLocals`, `debug/getStack` — Inspect a Suspended Thread

Both take `params.threadId` and require the thread to be **suspended**
(else `bad_state`). Unknown thread → `not_found`; missing/mistyped
threadId → `bad_params`.

**getLocals** result: `{"locals": [{"name", "value", "type"}...], "script"?, "line"}`
— the innermost local table's entries (values as display strings, truncated
at 255 chars).

**getStack** result: `{"frames": [{"level", "script", "line"?}...]}` —
outermost caller first, current script last.

### 4.9 `debug/getSource` — Fetch Script Source with Breakpoint Overlay

Request params: `script` (fully qualified dotted path; leading `@`
stripped), optional `threadId` (adds `currentLine` when that thread is
suspended in this script).

Success result:
`{"script", "currentLine"?, "lines": [{"num", "text", "breakpoint": bool, "current"?: true}...]}`.

Errors: missing `script` → `bad_params`; unqualified path → `bad_params`
(`"Script path must be fully qualified (e.g. system.temp.myFunc)"`); table
or script not found, or object has no source → `not_found`; database load
failure → `internal_error`.

### 4.10 `debug/listThreads` — List Registered Debug Threads

No params. Result: `{"threads": [{"threadId", "suspended": bool, "script"?, "line"?}...]}`
— `script`/`line` only present for suspended threads.

### 4.11 `debug/setWatchpoint`, `debug/listWatchpoints`, `debug/clearWatchpoints`

**setWatchpoint** — toggle semantics, like setBreakpoint. Request params:
`variable` (name, < 64 bytes). Success result:
`{"action": "set"|"cleared", "variable"}`. Errors: missing/non-string
`variable` → `bad_params`; name too long → `bad_params`; 64 slots full →
`limit_exceeded`.

**listWatchpoints** — no params. Result:
`{"watchpoints": [{"variable", "lastValue"?}...]}`.

**clearWatchpoints** — no params. Result: `{"cleared": <count>}`.

### 4.12 `shutdown` — Clean Exit

**Request:** `{"op": "shutdown", "id": 3}`

**Response:** bare ack, then the process exits with code 0.

---

## 5. Error Semantics

Every top-level error response carries `error.code` from this stable set
(constants `OP_ERRCODE_*` in `frontier-cli/op_handler.h` — keep in sync):

| Code | Meaning | `id` in response |
|------|---------|------------------|
| `parse_error` | Request line is not valid JSON | `null` |
| `missing_id` | No numeric `id` field | `null` |
| `missing_op` | No string `op` field | echoed |
| `unknown_op` | `op` not in the dispatch table | echoed |
| `bad_params` | Missing or wrong-typed parameter (a wrong-typed param is treated as missing) | echoed |
| `batch_too_large` | `params.items` exceeds 1000 items | echoed |
| `limit_exceeded` | Breakpoint (256) or watchpoint (64) slots full | echoed |
| `not_found` | Debug thread / table / script not found | echoed |
| `bad_state` | Op invalid for the target's current state (step on running thread, pause on suspended thread, save after a killed debug thread) | echoed |
| `locked` | `odb/save` refused: the root is save-locked (`--lock-opened-roots` / `FRONTIER_LOCK_OPENED_ROOTS=1`) | echoed |
| `script_error` | UserTalk compile or runtime failure | echoed |
| `internal_error` | Allocation, spawn, or database-load failure | echoed |
| `line_too_long` | Request line exceeds 64 KB | `null` |

Notes:

- Per-item errors inside `results` arrays (odb ops) carry `message` only;
  codes are envelope-level.
- The message text is human-readable and NOT part of the stable contract;
  branch on `code`, not on message content.
- Contract coverage: every code above except `internal_error` and
  `limit_exceeded` (not reachable without fault injection / 256+ set
  calls) is exercised by `protocol_contract_tests.yaml`; `locked` is
  exercised by `protocol_odb_save.yaml`.

---

## 6. Error State Management

### 6.1 The Problem

Some verb implementations (notably `xml.frontiervaluetotaggedtext()`) use `disablelangerror()`/`enablelangerror()` internally. If an error occurs between the disable/enable calls, the global `langerrordisable` counter can be left > 0, which causes `langerrorenabled()` to return false. This prevents `try/else` blocks from catching errors in subsequent evaluations.

In a per-process model (one process per test), this doesn't matter because the process exits. In a long-lived protocol process, the stale state leaks into the next evaluation.

### 6.2 The Fix

`script/clearContext` resets all error-related globals:

```c
langerrordisable = 0;
langerrorlogdisable = 0;
fllangerror = false;
```

Clients MUST call `script/clearContext` between independent evaluation sessions.

---

## 7. Example Session

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
← {"id":5,"error":{"code":"script_error","message":"...","location":{...},"stack":[...]},"success":false}

→ {"op":"shutdown","id":6}
← {"id":6,"success":true}
[process exits]
```

---

## 8. Future: GUI Application over Stdio

When the GUI application connects to `frontier-cli`, it can use this same stdio protocol as the transport layer. The path forward:

1. **Add GUI protocol operations** — Implement `odb/get`, `odb/children`, `subscribe`, etc. from [`PROTOCOL.md`](./PROTOCOL.md) as new `op` handlers in `protocol_handler.c`
2. **Add server-initiated events** — The CLI can write unsolicited event lines (no `id` field) for subscription notifications, using the same format as WebSocket events
3. **Keep stdout isolation** — The `g_protocol_out` pattern ensures clean protocol output regardless of what verb implementations print
4. **Consider structured output for stderr** — Currently stderr carries unstructured log output; for GUI use, consider a structured log format or a separate logging channel

### 8.1 Key Differences from WebSocket

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
| 1.2.0 | 2026-08-09 | Unit 1.2 persistence semantics: new `odb/save` op (§4.4) reusing the `fileMenu.save()` kernel path; top-level `dirty` boolean on `odb/set`/`odb/delete` responses; new stable error code `locked`; explicit persistence contract matrix (in-memory until save; abnormal-termination loss is by design; clean-exit save of a RW root per issue #127) proven by restart-then-verify tests (`protocol_odb_save.yaml`) |
| 1.1.0 | 2026-08-09 | Unit 1.1 protocol contract hardening: documented all 22 dispatch-table ops + debug notifications; stable `error.code` set (§5); unparseable JSON now answered with `parse_error` (was: silent drop); id validated before op; oversized lines answered with a single `line_too_long` error; removed the never-implemented `error.category` field from examples |
| 1.0.0 | 2026-02-15 | Initial specification (from working implementation) |
