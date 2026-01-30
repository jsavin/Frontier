# Integration Test Failures Report

**Date:** 2026-01-29
**Branch:** feature/webserver-hello-world
**Total Tests:** 1684
**Passed:** 1300
**Skipped:** 205
**Failed:** 179

## Summary by Category

| Category | Failed | Root Cause |
|----------|--------|------------|
| File Dialog Verbs | 11 | GUI dialogs not supported in headless mode |
| File I/O Verbs | 33 | Sandbox restrictions / path handling |
| TCP Client/Server | 65 | External network connections / test infrastructure |
| Thread Verbs | 10 | Thread evaluation not fully implemented |
| XML Verbs | 32 | xml.compile() not implemented |
| Path Resolution | 8 | System paths / defined() edge cases |
| Op Verbs | 4 | op.outlinetoxml requires outline context |
| Sys Verbs | 6 | Process management not implemented |
| Database Hydration | 3 | Result type mismatch (True vs 'true') |
| Other | 7 | Various edge cases |

---

## Detailed Failures

### File Dialog Verbs (11 failures)

These tests require GUI file/folder picker dialogs which are not available in headless mode.

| Test | Expected | Actual |
|------|----------|--------|
| file.getFileDialog - select file with full path | success=True | success=False |
| file.getFileDialog - returns full absolute path | success=True | success=False |
| file.getFileDialog - shows hidden files | success=True | success=False |
| file.getFileDialog - unicode filename (CJK) | success=True | success=False |
| file.getFileDialog - unicode filename (emoji) | success=True | success=False |
| file.getFileDialog - filename with spaces | success=True | success=False |
| file.getFileDialog - very long path (256 chars) | success=True | success=False |
| file.getFileDialog - starts from output address path | success=True | success=False |
| file.putFileDialog - select existing file to overwrite | success=True | success=False |
| file.putFileDialog - shows all files (not just directories) | success=True | success=False |
| file.getFolderDialog - only shows directories and symlinks | success=True | success=False |

**Action:** Mark as expected failures in headless mode, or skip these tests.

---

### File I/O Verbs (33 failures)

Most failures are due to sandbox restrictions preventing file operations outside allowed paths.

| Test | Expected | Actual |
|------|----------|--------|
| file.writeWholeFile - write string to file | success=True | success=False |
| file.readWholeFile - read back written content | success=True | success=False |
| file.readWholeFile - round trip binary data | success=True | success=False |
| file.delete - nonexistent file error | success=False | success=True |
| file.size - get file size | success=True | success=False |
| file.modified - get file modification date | success=True | success=False |
| file.copy - copy file to new location | success=True | success=False |
| file.copy - verify destination exists with same content | success=True | success=False |
| file.rename - rename file | success=True | success=False |
| file.rename - verify old name no longer exists | result='false' | result='true' |
| file.rename - verify new name exists with same content | success=True | success=False |
| file.move - move file to different location | success=True | success=False |
| file.move - verify source no longer exists | result='false' | result='true' |
| file.move - verify destination exists with same content | success=True | success=False |
| file.open/close - open and close file for reading | success=True | success=False |
| file.read - read bytes from file | success=True | success=False |
| file.readline - read line from file | success=True | success=False |
| file.endoffile - check if at end of file | success=True | success=False |
| file.getposition - get current file position | success=True | success=False |
| file.setposition - seek to file position | success=True | success=False |
| file.getendoffile - get file size via handle | success=True | success=False |
| file.compare - compare identical files | success=True | success=False |
| file.compare - compare different files | success=True | success=False |
| file.setposition - position beyond EOF | success=True | success=False |
| file.readline - Unix line ending (LF) | success=True | success=False |
| file.readline - Windows line ending (CRLF) | success=True | success=False |
| file.readline - Classic Mac line ending (CR) | success=True | success=False |
| file.type - extract extension from .txt file | success=True | success=False |
| file.type - file with no extension | success=True | success=False |
| file.type - file with multiple extensions | success=True | success=False |
| file.creator - returns empty string in headless mode | success=True | success=False |
| file.setmodified - set modification time | success=True | success=False |
| file.setcreated - behavior on different platforms | success=True | success=False |

**Action:** Verify tests use `{FRONTIER_TEST_TMP_DIR}` template. Check sandbox configuration.

---

### TCP Client Verbs (34 failures)

Tests requiring external network connections fail. The tests attempt to connect to external servers which may not be available or may timeout.

| Test | Expected | Actual |
|------|----------|--------|
| tcp.openNameStream - connect to localhost by name | success=True | success=False |
| tcp.openNameStream - returns positive stream ID | success=True | success=False |
| tcp.openNameStream - connection refused error | success=True | success=False |
| tcp.openNameStream - DNS failure for invalid hostname | success=True | success=False |
| tcp.openAddrStream - connect to localhost via IP | success=True | success=False |
| tcp.openAddrStream - localhost connection | success=True | success=False |
| tcp.openAddrStream - connect to example.com via IP | success=True | success=False |
| tcp.closeStream - graceful close after connect | success=True | success=False |
| tcp.abortStream - immediate close after connect | success=True | success=False |
| tcp.countConnections - track stream lifecycle | success=True | success=False |
| tcp.countConnections - multiple concurrent streams | success=True | success=False |
| tcp.readStream - empty read when no data sent | success=True | success=False |
| tcp.readStream - read after HTTP request | success=True | success=False |
| tcp.readStream - read from closed stream | success=True | success=False |
| tcp.readStream - zero byte count | success=True | success=False |
| tcp.writeStream - write HTTP request | success=True | success=False |
| tcp.writeStream - empty data | success=True | success=False |
| tcp.writeStream - write to closed stream | success=True | success=False |
| tcp.writeStream - large data write | success=True | success=False |
| tcp.closeStream - double close (idempotent) | success=True | success=False |
| tcp connection lifecycle - full HTTP exchange | success=True | success=False |
| tcp.openNameStream - invalid port (negative) | success=True | success=False |
| tcp.openNameStream - invalid port (zero) | success=True | success=False |
| tcp.openNameStream - invalid port (too large) | success=True | success=False |
| tcp.openNameStream - empty hostname | success=True | success=False |
| tcp.openAddrStream - invalid port (negative) | success=True | success=False |
| tcp.openAddrStream - invalid port (too large) | success=True | success=False |
| tcp.openAddrStream - invalid address (zero) | success=True | success=False |
| tcp.statusStream - returns OPEN for idle connection | success=True | success=False |
| tcp.statusStream - return type is string | success=True | success=False |
| tcp.statusStream - after stream closed | success=True | success=False |
| tcp.getPeerAddress - returns localhost for local connection | success=True | success=False |
| tcp.getPeerAddress - return type is long | success=True | success=False |
| tcp.getPeerAddress - after stream closed | success=True | success=False |
| tcp.getPeerPort - returns server port for client | success=True | success=False |
| tcp.getPeerPort - return type is long | success=True | success=False |
| tcp.getPeerPort - after stream closed | success=True | success=False |

**Action:** These tests need a local echo server or mock. Consider spawning a test server before running these tests.

---

### TCP Server Verbs (31 failures)

Server-side TCP tests also fail, likely due to port binding or callback infrastructure issues.

| Test | Expected | Actual |
|------|----------|--------|
| tcp.listenStream - basic listener startup | success=True | success=False |
| tcp.listenStream - return type validation | success=True | success=False |
| tcp.listenStream - listener restart on same port | success=True | success=False |
| tcp.listenStream - queue depth limiting | success=True | success=False |
| tcp.listenStream - port already in use | success=True | success=False |
| tcp.listenStream - return type spec | success=True | success=False |
| tcp.listenStream - explicit localhost binding | success=True | success=False |
| tcp.listenStream - maximum queue depth | success=True | success=False |
| tcp.listenStream - new connections rejected after closeListen | success=True | success=False |
| tcp.closeListen - basic listener shutdown | success=True | success=False |
| tcp.closeListen - idempotent behavior | success=True | success=False |
| tcp.closeListen - return type spec | success=True | success=False |

**Note:** The webserver Hello World tests DO pass, indicating the core listener infrastructure works. These standalone TCP tests may have different requirements.

---

### Thread Verbs (10 failures)

Thread evaluation and management verbs are not fully implemented in headless mode.

| Test | Expected | Actual |
|------|----------|--------|
| thread.evaluate - execute simple script in new thread | success=True | success=False |
| thread.evaluate - return value from thread | success=True | success=False |
| thread.evaluate - script error in thread is handled gracefully | success=True | success=False |
| thread.evaluate - multiple threads run independently | success=True | success=False |
| thread.evaluate - safely modify ODB during execution | success=True | success=False |
| thread.sleepFor - thread sleeps and wakes | success=True | success=False |
| thread.wake - wake sleeping thread | success=True | success=False |
| thread.kill - terminate thread | success=True | success=False |
| thread.getCount - report active thread count | success=True | success=False |
| thread.getCurrentID - get current thread ID | result=True | result='true' |

**Action:** Implement thread.evaluate() for headless mode. Note: `thread.getCurrentID` is a result type issue (boolean True vs string 'true').

---

### XML Verbs (32 failures)

xml.compile() and related XML parsing verbs are not implemented.

| Test | Expected | Actual |
|------|----------|--------|
| xml.compile - simple element | success=True | success=False |
| xml.compile - multiple elements | success=True | success=False |
| xml.compile - nested elements | success=True | success=False |
| xml.compile - element with attributes | success=True | success=False |
| xml.compile - XML entities | success=True | success=False |
| xml.compile - empty element | success=True | success=False |
| xml.compile - malformed XML | error contains 'Script execution failed' | 'Invalid JSON output' |
| xml.getAddress - simple lookup | success=True | success=False |
| xml.getAddress - nested element | success=True | success=False |
| xml.getAddress - element not found | success=True | success=False |
| xml.getAddress - multiple elements (returns first) | success=True | success=False |
| xml.getAddressList - multiple elements | success=True | success=False |
| xml.getAddressList - single element | success=True | success=False |
| xml.getAddressList - no matching elements | success=True | success=False |
| xml.getAddressList - verify list contents | success=True | success=False |
| xml.getAttribute - simple attribute | success=True | success=False |
| xml.getAttribute - multiple attributes | success=True | success=False |
| xml.getAttribute - attribute not found | success=True | success=False |
| xml.getAttribute - no attributes | success=True | success=False |
| xml.getAttributeValue - simple attribute | success=True | success=False |
| xml.getAttributeValue - numeric attribute | success=True | success=False |
| xml.getAttributeValue - attribute with entities | success=True | success=False |
| xml.getAttributeValue - attribute not found | success=True | success=False |
| xml.getPathAddress - simple path | success=True | success=False |
| xml.getPathAddress - deep path | success=True | success=False |
| xml.getPathAddress - path not found | success=True | success=False |
| xml.getPathAddress - single element path | success=True | success=False |
| xml.getPathAddress - path with array index | success=True | success=False |
| xml.getPathAddress - array index [1] (first element) | success=True | success=False |
| xml.getPathAddress - array index [0] (invalid, treated as literal) | success=True | success=False |
| xml.getPathAddress - array index out of bounds | success=True | success=False |
| xml.getPathAddress - malformed array index (empty brackets) | success=True | success=False |
| xml.getPathAddress - malformed array index (non-numeric) | success=True | success=False |
| xml - full round trip compile/decompile | success=True | success=False |
| xml - compile then navigate with getPathAddress | success=True | success=False |
| xml - compile with attributes then getAttribute | success=True | success=False |
| xml - getAddressList and iterate | success=True | success=False |
| xml - entity encoding roundtrip | success=True | success=False |

**Action:** Implement xml.compile() kernel verb. Note that xml.decompile() and xml.frontiervaluetotaggedtext() work correctly.

---

### Path Resolution / defined() (8 failures)

Issues with system.paths resolution and defined() for certain table paths.

| Test | Expected | Actual |
|------|----------|--------|
| system.paths entries resolve correctly | result='true' | result='false' |
| defined() - nested lookup 'html.getTitle' | result='true' | result='false' |
| defined() - hardcoded 'builtins' table | result='true' | result='false' |
| defined() - in-memory table child 'builtins.init' | result='true' | result='false' |
| Call verb via path-resolved identifier | result='true' | result='false' |
| defined() - internal table not in paths | result='false' | result='true' |
| defined() - builtins.init EFP table should exist | result='true' | result='false' |
| defined() - system.verbs.builtins.user.radio3.init EFP table should exist | result='true' | result='false' |
| defined() - suites.tcp EFP table should exist | result='true' | result='false' |

**Action:** Investigate path resolution for EFP tables and builtins. May need to ensure system.paths is properly populated during hydration.

---

### Op Verbs (4 failures)

Outline-to-XML conversion requires an active outline context.

| Test | Expected | Actual |
|------|----------|--------|
| op.outlinetoxml - basic conversion | success=True | success=False |
| op.outlinetoxml - contains outline text | success=True | success=False |
| op.outlinetoxml - preserves hierarchy | success=True | success=False |
| op.xmltooutline - round trip | success=True | success=False |

**Action:** These may require outline window context not available in headless mode.

---

### Sys Verbs (6 failures)

Process management and shell command verbs have issues.

| Test | Expected | Actual |
|------|----------|--------|
| sys.winshellcommand - not available on macOS/Linux | success=True | success=False |
| sys.appisrunning - current process returns true | result='true' | result='false' |
| sys.getapppath - current process returns path | result='true' | result='false' |
| sys.getapppath - path contains process name | result='true' | result='false' |
| sys verbs - process management suite | result='true' | result='false' |
| sys.unixshellcommand - 1 param (command only) | result='true' | result='hello\n' |
| sys.unixshellcommand - 1 param (command fails) | result='false' | result='' |

**Action:**
- `sys.appisrunning` and `sys.getapppath` need process introspection implementation
- `sys.unixshellcommand` 1-param form returns output, not boolean - update test expectations
- `sys.winshellcommand` should return appropriate error on non-Windows

---

### Database Hydration (3 failures)

Result type mismatch - tests expect boolean `True` but get string `'true'`.

| Test | Expected | Actual |
|------|----------|--------|
| Database hydration - opens correct file after migration | result=True | result='true' |
| Database - system table accessible after hydration | result=True | result='true' |
| Database - workspace accessible after hydration | result=True | result='true' |

**Action:** Fix test expectations to match string 'true' or fix runner to parse boolean values.

---

### REPL Tests (1 failure)

| Test | Expected | Actual |
|------|----------|--------|
| REPL - workspace doesn't affect system table | success=True | success=False |
| Mixed types - string child if exists | success=True | success=False |

**Action:** Investigate workspace isolation in REPL mode.

---

## Priority Recommendations

### High Priority (blocking core functionality)
1. **xml.compile()** - 32 tests depend on this
2. **Thread evaluation** - 10 tests, needed for async operations
3. **File I/O sandbox** - 33 tests, core functionality

### Medium Priority (improves test coverage)
4. **TCP test infrastructure** - Need local echo server for 65 tests
5. **Path resolution / defined()** - 8 tests, affects verb discovery

### Low Priority (can skip or mark expected)
6. **File dialog verbs** - 11 tests, GUI-only
7. **Sys process verbs** - 6 tests, edge cases
8. **Database hydration** - 3 tests, type coercion issue

---

## Test Infrastructure Improvements Needed

1. **Local echo server for TCP tests** - Spawn a simple TCP server before running tcp_* tests
2. **Sandbox-aware test paths** - Ensure all file tests use `{FRONTIER_TEST_TMP_DIR}`
3. **Result type normalization** - Handle boolean True vs string 'true' in test runner
4. **Skip markers for headless mode** - Add `headless_skip: true` option for GUI-only tests
