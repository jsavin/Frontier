# Date/Time Modernization Plan

**Status**: Draft (pending implementation)  
**Owner**: Codex (GPT-5)  
**Last Updated**: November 9, 2025  
**Related Branches**: `feature/carbon-migration`

## Purpose
Create a portable, high-fidelity time subsystem that:
1. Completely removes the Carbon/Mac Toolbox dependency for date/time verbs.
2. Provides millisecond (or better) precision inside the runtime.
3. Stores timezone metadata alongside timestamps in the v7 database format so migrations preserve user intent and future script APIs can expose timezone information explicitly.

## Legacy Baseline
- Frontier v6 relied on Carbon `LongDateTime`, `LongDateRec`, and `MachineLocation` calls (mostly via `FastTimes.c` and `system.macintosh.clock` glue) to compute “seconds since 1 Jan 1904 GMT” (Mac epoch) values and to format local strings.
- On-disk tables only store the Mac epoch integer, so higher precision and timezone offsets are lost when the kernel restarts.
- Headless builds stubbed all Carbon APIs, so `clock.now()` and related verbs fail once the AppleEvent/macintosh glue is unavailable.

## Target Architecture
| Layer | Responsibility | Notes |
| --- | --- | --- |
| `portable/time_portable.[ch]` | Core OS abstraction returning wall-clock (UTC) and monotonic timestamps at millisecond resolution. | POSIX: `clock_gettime(CLOCK_REALTIME/CLOCK_MONOTONIC)`; Windows: `GetSystemTimePreciseAsFileTime` + `QueryPerformanceCounter`. |
| `Common/source/clockverbs.c` (new) | Implement `clock.*` verbs (now, seconds, sleep, clock string helpers) using the portable layer. | No references to `system.macintosh` table during execution. |
| Database serialization (`db_format.c`, etc.) | Store high-fidelity record: `{ int64 unixMillis; int16 tzMinutes; uint16 flags }`. | Backwards-compatible migration writes tz offset determined during conversion. |
| Migration helpers | Wrap v6 Mac-epoch integers during save-as, capture the local timezone offset at migration time, and convert to Unix epoch milliseconds. | For values lacking timezone info, assume the local offset applied during migration. |
| Script API extensions | Future verbs (Phase 3 follow-up) expose timezone and formatting helpers (`clock.getTimeZone`, `clock.toISO8601`, etc.). | Not required to unblock current milestone, but plan now for compatibility. |

## Proposed Data Format
Store the following structure wherever timestamps currently use raw integers (table metadata, script records, etc.):

```
struct headless_timestamp_v7 {
    int64_t unix_ms;      // Milliseconds since Unix epoch (UTC).
    int16_t tz_minutes;   // Signed offset minutes from UTC (e.g., -480 for PST).
    uint16_t flags;       // Bitfield for DST observation, precision indicator, reserved bits.
};
```

**Rationale**
- `int64_t unix_ms` keeps resolution to ~292 million years and matches modern APIs.
- `tz_minutes` preserves human intent (e.g., user created data while in GMT+2). Scripts can later adjust behavior per user preference.
- `flags` keeps room for DST indicator, “local offset unknown” bit, or future leaps.

## Migration Strategy
1. **Inventory Fields**  
   Enumerate every timestamp stored in the v7 root: table metadata (timeCreated/timeModified), script nodes, logs, pending scheduler entries, etc. Capture the list in `planning/carbon_migration/data/timestamp_fields.md`.

2. **Legacy Extraction**  
   During v6→v7 migration, read existing `longDateTime` values and convert them to Unix epoch milliseconds:  
   `unix_ms = (macEpochSeconds * 1000) + fractionalMillis` (fractional portion assumed zero because v6 stored integers).  
   Record the migration timezone: either (a) local system offset at migration time or (b) per-record offset if available (some tables store DST flags).

3. **Runtime Serialization**  
   Update `tablepack.c` / `db_format.c` so any in-memory timestamp is serialized using `headless_timestamp_v7`. Provide helper functions:
   - `timestamp_from_legacy_mac_epoch(int32 seconds)`  
   - `timestamp_to_legacy_mac_epoch(const headless_timestamp_v7*)` (only for compatibility with old APIs/tests).

4. **Script Verbs**  
   Re-implement `clock.now`, `clock.seconds`, `clock.sleepFor`, etc., in C using `portable/time_portable`. `clock.now()` returns a new `headless_timestamp_v7` and the UserTalk layer exposes the integer or formatted string per legacy expectations until new APIs land.

5. **Timezone Propagation**  
   Provide C helpers for retrieving the system timezone offset and DST status. On Unix, rely on `localtime_r` / `tm_gmtoff`. On Windows, use `GetTimeZoneInformation`. Store results in `tz_minutes`. Future script verbs will allow overrides.

6. **Testing**  
   - Unit tests for `portable/time_portable` backends (POSIX vs Windows mocks).  
   - Migration tests: feed known Mac-epoch values and ensure serialized v7 blobs contain the expected Unix ms + offset.  
   - Integration tests: `tests/cli_system_defined` extended to call `clock.now()`, verify monotonicity, and format conversions.

## Implementation Phases
### Phase A — Portable Runtime Layer
1. Create `portable/time_portable.[ch]` with OS-specific implementations.
2. Remove `FastTimes.c` from headless builds; update makefiles and headers.
3. Write small unit tests to validate cross-platform behavior (mock Windows path where necessary).

### Phase B — Verb Rework
1. Introduce `Common/source/clock_portable.c` (or reuse existing verb file) to call the new layer.
2. Update `system.verbs.clock.*` glue to invoke the C implementation instead of `system.macintosh` tables.
3. Ensure `langrunstring` path no longer needs `system.macintosh` search results; clean up instrumentation.

### Phase C — Serialized Format Update
1. Define `headless_timestamp_v7` in a shared header (`Common/headers/db_timestamp.h`).
2. Patch migrator and runtime pack/unpack code to read/write the struct.
3. Update docs (database format guide) to reflect the new layout.
4. Capture fixtures (legacy + migrated records) for regression testing.

### Phase D — Timezone & Future-proofing
1. Implement timezone detection helpers and integrate them into migration + runtime writes.
2. Add TODO items for script-level timezone verbs and remote/local guest database interactions.
3. Document how headless + future remote runtimes should reconcile timezone overrides.

## Risks & Mitigations
| Risk | Mitigation |
| --- | --- |
| Inconsistent timezone offsets between migration and runtime writes | Record per-node offsets during migration; runtime uses the OS offset at write time; future user overrides stored elsewhere. |
| Windows path lagging behind POSIX implementation | Build a thin compatibility layer using `GetSystemTimePreciseAsFileTime` and regression tests under Cross (or mocked). |
| Legacy scripts expecting pure Mac epoch integers | Provide compatibility verbs (`clock.macSeconds`) or temporary conversion helpers until scripts migrate. |
| Increased DB footprint | Timestamp struct adds 4 bytes vs old 32-bit integer; acceptable trade-off, but compress duplicates if needed later. |

## Follow-Up Work (Backlog)
- P1: Extend script API with timezone introspection/config verbs.
- P1: Add CLI/runtime switches to force a specific timezone during batch operations.
- P2: Provide ISO-8601 formatting helpers and unit tests.
- P2: Design per-user view preferences storage (fonts/window geometry) outside the system root before removing these fields entirely.

---
Document owner: Codex (GPT-5). Update this plan after each milestone (Phase A/B/C/D) is completed.***
