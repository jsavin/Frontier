# Integration Test Gap Analysis & Plan

Status: In Progress
Created: 2026-03-20
Last Updated: 2026-03-20

## Context

As of March 2026, Frontier has 2,012 integration tests across 56 YAML files with 0 failures. However, several critical runtime paths lack coverage — particularly around persistence, data integrity, and error recovery. This document tracks the gaps and their resolution.

## Current Baseline

- **Test files**: 56 YAML files in `tests/integration/test_cases/`
- **Total tests**: 2,012 (1,920 active, ~90 skipped)
- **Protocol operations**: 7/7 covered (script/eval, script/clearContext, odb/get, odb/set, odb/list, odb/delete, shutdown)
- **Verb coverage**: 68% (482/710 verbs)

---

## P0: Persistence & Data Integrity

These gaps risk silent data loss or corruption.

| # | Gap | Description | Status |
|---|-----|-------------|--------|
| 1 | **`filemenu.save()` via protocol** | No test verifies `script/eval "filemenu.save()"` through the protocol layer. This is the path a GUI client will use to trigger File > Save. Must verify the database file is actually updated on disk. | Done — `persistence_save_tests.yaml` |
| 2 | **Guest DB full lifecycle round-trip** | Individual pieces are tested, but no single test covers: open guest DB → modify values → `filemenu.save()` → close → reopen → verify values survived. | Done — `persistence_save_tests.yaml` |
| 3 | **System root + guest DB both modified and saved** | No test modifies BOTH databases, saves both, closes, reopens, and verifies both. Pack/unpack context switching between databases is a corruption risk area (cf. databasedata elimination work). | Done — `persistence_save_tests.yaml` |
| 4 | **Protocol mutations are in-memory only** | No test verifies that `odb/set` changes are NOT persisted if the process exits without an explicit save. Users and client apps need to understand this contract. | Partial — in-memory contract verified; restart non-persistence cannot be tested with current framework |
| 5 | **Guest DBs flushed on shutdown** | No test verifies that guest databases are properly saved/closed when the process exits (via `shutdown` op or normal exit). Risk: data loss if guest DB handles not flushed. | Done — `persistence_save_tests.yaml` |

## P1: Error Recovery & Concurrency

These gaps risk inconsistent state or resource leaks.

| # | Gap | Description | Status |
|---|-----|-------------|--------|
| 6 | **Error mid-transaction** | No test for: `try { db.setvalue(x); error(); db.setvalue(y) }` — verify first setvalue committed and DB not left in inconsistent state. | Done — `error_recovery_concurrency_tests.yaml` |
| 7 | **Thread modifying DB while main thread saves** | GIL serializes access, but no test proves databasedata global stays consistent when a background thread is actively modifying tables while `filemenu.save()` runs on the main thread. | Done — `error_recovery_concurrency_tests.yaml` |
| 8 | **`fileMenu.closeall()` actually closes everything** | Tests check return value is true but don't verify all guest DB handles are actually released and the `hodblist` linked list is properly cleared. | Done — `error_recovery_concurrency_tests.yaml` |
| 9 | **Webserver HTTP round-trip** | HTTP request → webserver dispatch → responder script → response. Currently skipped due to port conflicts. This is the primary external-facing interface. | **Done** — `webserver_http_roundtrip_tests.yaml` (7 active, 2 skipped for fwsNetEvent* migration) |
| 10 | **bigstring boundary values** | No tests for strings at exactly 254, 255, and 256 bytes. The 255-byte bigstring limit (issue #475, #423) truncates silently — tests should document and verify this boundary behavior. | **Done** — `bigstring_boundary_tests.yaml` (27 tests covering variables, ODB, concatenation, substring, comparison, file paths, env vars) |

## P2: Verb Coverage Gaps

Important verbs with zero or minimal test coverage.

| # | Gap | Current Tests | Description | Status |
|---|-----|---------------|-------------|--------|
| 11 | **clock/date verbs** | 7 | Only `timeModified`/`timeCreated` on binary types. Missing: `clock.now()`, `clock.ticks()`, `date.get()`, `date.set()`, date arithmetic, format/parse. | Not started |
| 12 | **math verbs** | 0 | No tests for `math.sin`, `math.cos`, `math.sqrt`, `math.abs`, `math.min`, `math.max`, `math.random`, etc. | Not started |
| 13 | **base64 encode/decode** | 0 | No tests for `base64.encode()`, `base64.decode()`. Used by the protocol layer for binary values. | Not started |

## P3: Edge Cases & Stress Tests

Lower probability but high impact if triggered.

| # | Gap | Description | Status |
|---|-----|-------------|--------|
| 14 | **Deeply nested auto-vivification** | `odb/set` with path `a.b.c.d.e.f.g` where intermediate tables don't exist. Verify all intermediates created, no address corruption. | Not started |
| 15 | **Overwrite table with scalar** | `odb/set` or `db.setvalue` where target path is an existing table. Verify table properly freed, no orphaned blocks. | Not started |
| 16 | **Large binary objects in guest DB** | Store MB+ binary data in a guest database, save, close, reopen, verify integrity. Tests block allocation and length field handling. | Not started |
| 17 | **Concurrent guest DB access from threads** | Multiple threads reading/writing the same guest database. GIL should serialize, but verify no databasedata context confusion. | Not started |
| 18 | **Error in HTTP responder script** | Responder script errors mid-execution. Verify stream properly closed, no half-written response. | Not started |
| 19 | **Batch `odb/set` with partial failure** | Batch of 10 items where 5th item is invalid. Verify items 1-4 committed, items 6-10 also attempted (per-item error isolation). | Not started |
| 20 | **`odb/delete` + save + reopen** | Delete values via protocol, save, reopen, verify deleted values don't reappear (no ghost blocks). | Not started |

---

## Implementation Notes

### Test file organization

New tests should go in existing files where they fit logically, or in new files when the scope warrants it:

- **#1, #4**: `protocol_odb_ops.yaml` or new `protocol_persistence.yaml`
- **#2, #3, #5**: `filemenu_verbs.yaml` or `db_verbs.yaml`
- **#6**: `try_error.yaml`
- **#7, #17**: `thread_verbs_foundation.yaml`
- **#8**: `filemenu_verbs.yaml`
- **#9, #18**: `webserver_hello_world.yaml`
- **#10**: `string_verbs.yaml` or new `bigstring_boundary.yaml`
- **#11**: New `clock_date_verbs.yaml`
- **#12**: New `math_verbs.yaml`
- **#13**: New `base64_verbs.yaml`
- **#14, #15, #19, #20**: `protocol_odb_ops.yaml`
- **#16**: `guest_db_externals.yaml` or `db_verbs.yaml`

### Verification patterns

For persistence tests, the key pattern is:
1. Mutate state (via protocol or script/eval)
2. Save explicitly (`filemenu.save()`)
3. Shut down or restart the process
4. Reopen and verify state survived

The integration test framework supports multi-step sequences within a single test case — use `steps` with sequential script/eval calls. For restart verification, a separate test case can read values written by a prior test (since all tests run against the same .root7 copy per worker).

### Known blockers

- **Webserver tests (#9, #18)**: Previously skipped due to port conflicts in parallel execution. May need single-worker mode or dynamic port allocation.
- **bigstring (#10)**: The 255-byte limit is a known issue (#475, #423). Tests should document current behavior as a baseline, then update when the limit is lifted.
- **Uncatchable verb errors**: Some fileMenu error paths bypass try/else (documented in `planning/FIX_TESTS_2026_02_16.md` B2). This affects #6 and #8.

---

## References

- `planning/_CURRENT_STATUS.md` — Overall project status
- `planning/_CURRENT_TODO_LIST.md` — Active TODO items
- `planning/FIX_TESTS_2026_02_16.md` — Known test gaps and uncatchable error bug
- `docs/TESTING_GUIDE.md` — Test framework documentation
- Issue #475 — bigstring 255-byte limit truncates long paths
- Issue #423 / #380 — Refactor bigstring to remove 255-char limit
- PR #481 — Protocol ODB and script operations (introduced op_handler.c)
- PR #391, #394 — fileMenu verb implementations
