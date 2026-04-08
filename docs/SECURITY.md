# Security Overview

Frontier is a single-user desktop application with a CLI-based runtime. Its current deployment model is one user on their own machine, with protocol access restricted to localhost. This document catalogs implemented security protections, defines the current threat model, and outlines deferred hardening for future phases.

Security is a top design priority for Frontier ("security and privacy by design"). Protections are built into the codebase rather than bolted on afterward.

**Audience**: Internal technical (developers working on Frontier).

---

## Current Threat Model (Phase 1)

### In Scope

- **Path traversal**: Malicious or malformed paths escaping intended directories
- **Symlink/link attacks**: Following symlinks to unintended files
- **Buffer overflows**: Fixed-size buffers (bigstrings, file paths) exceeding bounds
- **Database corruption**: Duplicate opens, interrupted migrations, malformed headers
- **Cross-site WebSocket hijacking**: Browser-based attacks against the localhost WS server
- **Denial of service**: Resource exhaustion via oversized frames or excessive connections
- **Deadlocks**: Incorrect lock ordering between GIL and debugger mutex

### Out of Scope (Phase 1)

- Remote attackers (no network-facing services beyond localhost)
- Multi-user isolation or privilege escalation
- Encrypted storage or at-rest database encryption
- Authentication or authorization (single-user model)

---

## Implemented Protections

### File System Security

**Path traversal validation** -- Rejects absolute paths and parent directory traversals (`..`) in log path construction. Prevents writes to arbitrary filesystem locations.

- Rejects paths starting with `/` and paths containing `..`
- File: `Common/source/langhash.c` (`is_safe_log_path()`)

**Fixed-size path buffers** -- All filesystem path buffers use named constants (`DB_PATH_MAX`, `PATH_MAX`, `TMPFILE_PATH_MAX`) rather than hardcoded magic numbers. Prevents buffer overflows in path manipulation.

- `DB_PATH_MAX`: database paths (`Common/source/db_format.c`)
- `PATH_MAX`: POSIX file operations (`portable/file_portable.c`)
- `TMPFILE_PATH_MAX`: temporary files (`Common/source/sysshellcall.c`)

**Bigstring length guards** -- Bigstrings (Pascal-style strings, max 255 bytes) include overflow checks before copy operations. Paths that exceed the limit are rejected with a `langerrormessage` error rather than silently overflowing or truncating.

- File: `Common/source/shellsysverbs.c`, `Common/source/langcallbacks.c`

### Protocol Security

**Localhost-only binding** -- The WebSocket server binds exclusively to `127.0.0.1`. No remote connections are accepted. The NDJSON protocol handler reads from stdin (no network exposure).

- File: `frontier-cli/ws_server.c` (bind call), `frontier-cli/protocol_handler.c` (stdin)

**Origin header validation** -- WebSocket handshakes validate the `Origin` header against a localhost allowlist (`http://localhost`, `https://localhost`, `http://127.0.0.1`, `https://127.0.0.1`, `http://[::1]`, `https://[::1]`). A suffix check prevents prefix-match bypasses (e.g., `http://localhost.attacker.com`).

- Verifies the character after the hostname is `:`, `/`, or `\0`
- Non-browser clients (no Origin header) are permitted -- any process that can connect to the loopback interface already has equivalent local access, so Origin validation adds no security for non-browser clients
- File: `frontier-cli/ws_frame.c` (`ws_handshake()`)

**Connection limits** -- `WS_MAX_CLIENTS` caps concurrent WebSocket connections at 8, preventing resource exhaustion from runaway or malicious clients.

- File: `frontier-cli/ws_server.h`

**Frame size limits** -- `WS_MAX_FRAME_PAYLOAD` (256 KB) caps individual WebSocket frame payloads. Frames exceeding this limit are rejected during decode, preventing memory exhaustion.

- Includes overflow check: `header_len + payload_len < header_len` guards against integer wraparound
- Constant defined in: `frontier-cli/ws_frame.h`
- Enforcement: `frontier-cli/ws_frame.c` (`ws_frame_decode()`)

### Database Security

**Duplicate open prevention** -- The runtime detects when a database file is already open and returns `dbalreadyopenederror` rather than allowing concurrent access that could corrupt data. Related to issue #270.

- File: `Common/source/dbverbs.c`

**Migration locking** -- Database migration (v6 to v7) uses atomic lock file creation (`O_CREAT | O_EXCL`) to prevent concurrent migrations of the same database. Stale locks (from crashed processes) are detected by age and cleaned up. Related to issue #271.

- `migration_lock_acquire()`: creates lock file atomically
- `migration_lock_release()`: removes lock file and closes fd
- `migration_lock_wait()`: polls for another process to finish
- File: `Common/source/db_format.c`

**Header validation on read** -- Database open and migration paths validate file headers with explicit `fread` size checks. Malformed or truncated headers are rejected before any data is processed. Related to issue #264.

- File: `Common/source/db_format.c`

### Debugger Security

**Lock ordering invariant** -- The debugger enforces a strict lock ordering: GIL first, then `g_debug_mutex`. Protocol handlers never hold the GIL when calling debug operations. The debugger callback (which runs with GIL held) acquires `g_debug_mutex` for breakpoint checks. Violating this order would deadlock.

- Documented at top of file with enforcement throughout
- File: `frontier-cli/debug_handler.c`

**Reference-counted debug state** -- Debug state uses atomic reference counting (`refcount`) to prevent use-after-free. Callers acquire a reference via `debug_get_state_for_thread()` (under `g_debug_mutex`) and release via `debug_release_state()`. State is freed only when the last reference is dropped.

- `atomic_fetch_add` / `atomic_fetch_sub` for thread-safe counting
- Assert on refcount underflow
- File: `frontier-cli/debug_handler.c`, `frontier-cli/debug_handler.h`

**OOM fallback handling** -- All `cJSON_PrintUnformatted` calls in the debugger check for NULL returns and log warnings rather than dereferencing null pointers. The debugger degrades gracefully under memory pressure.

- File: `frontier-cli/debug_handler.c` (throughout)

### Threading

**Global Interpreter Lock (GIL)** -- All access to C globals is serialized through a single mutex. Only the GIL holder can read or write shared state. Yield points are explicit (`langbackgroundtask()`, `thread.sleepTicks()`). See ADR-014 for the full threading model.

- File: See `planning/architectural_decision_records/ADR-014-gil-cooperative-threading.md`

---

## Security Boundaries

| Boundary | Protected | Not Protected |
|----------|-----------|---------------|
| File I/O | Path traversal in log paths, buffer overflows in path buffers | Arbitrary filesystem access (single-user model assumes trust) |
| Protocol | Cross-site WS hijacking, JSON frame size, connection count | Authentication, encryption, per-session authorization |
| Database | Corruption from duplicate opens, concurrent migration, malformed headers | Full multi-process concurrent access |
| Memory | Bigstring bounds, path buffer sizes, refcount lifetime | Full ASLR/stack canaries (depends on compiler/OS defaults) |
| Threading | Deadlock prevention via lock ordering, atomic state management | Priority inversion, starvation (cooperative model) |

---

## Deferred Hardening (Phase 2+)

These protections are not needed for the current single-user localhost deployment but will be required as Frontier's deployment model expands.

### Daemon Mode

- **Sandbox/chroot**: Restrict filesystem access to the database directory and temp
- **Privilege separation**: Drop privileges after binding to port
- **Process isolation**: Separate processes for protocol handling and script execution

### Multi-User

- **Per-user access control**: Database-level and table-level permissions
- **Authentication**: Token-based auth for protocol connections
- **Session isolation**: Separate GIL contexts per user session

### Network

- **TLS for WebSocket**: Encrypted transport when accepting non-localhost connections
- **Token-based protocol auth**: Require a shared secret or OAuth token for protocol operations
- **Rate limiting**: Per-client request throttling beyond the current connection cap

### Database

- **Full inter-process locking**: Advisory or mandatory file locks for concurrent multi-process access
- **Encrypted at rest**: Optional database encryption for sensitive deployments
- **Integrity checksums**: Per-page checksums to detect bit-rot or tampering

---

## Developer Guidelines

### Path Handling

- Always use named constants (`DB_PATH_MAX`, `PATH_MAX`) for path buffers
- Validate user-supplied paths before use: reject `..` components and absolute paths when relative paths are expected
- Use `is_safe_log_path()` as a reference pattern for path validation

### Protocol Input

- Validate all JSON fields before use; never trust client-supplied sizes or offsets
- Check `cJSON_PrintUnformatted` return values for NULL (OOM)
- Cap result sets (`max_results` parameter) to prevent unbounded memory allocation

### Lock Ordering

The project-wide lock ordering is:

1. **GIL** (outermost)
2. **g_debug_mutex** (innermost)

Never acquire the GIL while holding `g_debug_mutex`. Protocol handlers that need both must acquire the GIL first, then `g_debug_mutex`. The debugger callback (which runs with GIL already held) may acquire `g_debug_mutex` directly.

### OOM Patterns

- Check all allocation return values (`malloc`, `newhandle`, `cJSON_Create*`)
- Log a warning with `log_warn()` and return gracefully; do not abort
- For the debugger: degrade to "no notification sent" rather than crashing

### Adding New Protocol Operations

When adding a new `op_handler` entry:

1. Validate all input fields from the JSON request
2. Respect `WS_MAX_FRAME_PAYLOAD` for response sizes
3. Use `localhost-only` as the security assumption -- do not add operations that would be unsafe if a malicious local process connects
4. Document any new security-relevant behavior in this file

---

## Related Resources

- [ADR-014: GIL Cooperative Threading](../planning/architectural_decision_records/ADR-014-gil-cooperative-threading.md) -- Threading model and lock semantics
- [Debugging Guide](DEBUGGING_GUIDE.md) -- LLDB patterns, protocol debugger usage
- [Architectural Anti-Patterns](ARCHITECTURAL_ANTIPATTERNS.md) -- Memory safety pitfalls, context guard patterns
- [CLI Usage Guide](CLI_USAGE_GUIDE.md) -- Protocol handler and WebSocket server usage
- [Testing Guide](TESTING_GUIDE.md) -- How to test security-relevant changes
