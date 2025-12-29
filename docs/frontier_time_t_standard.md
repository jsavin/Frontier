# Frontier Time Type (`frontier_time_t`) Portability Standard

## Overview

`frontier_time_t` is Frontier's portable timestamp type, designed to work correctly across all platforms and remain functional beyond the year 2038. It is defined as a signed 64-bit integer (`int64_t`) and uses a 1904 epoch (different from Unix's 1970 epoch).

## Why frontier_time_t Was Introduced

Prior to this fix, Frontier used the system's native `time_t` type for storing timestamps. This created a critical portability and future-proofing issue:

- **Platform-dependent size**: On 32-bit systems, `time_t` is 32 bits. On 64-bit systems, it may be 32 or 64 bits depending on the platform.
- **Year 2038 problem**: 32-bit `time_t` (signed) overflows on January 19, 2038, causing timestamp arithmetic to fail catastrophically.
- **Database format inconsistency**: Migration from v6 (32-bit) to v7 (64-bit) databases required consistent timestamp representation.

By introducing `frontier_time_t` as a fixed-size 64-bit type, Frontier ensures:

1. **Consistent size across all platforms** (always 8 bytes)
2. **No overflow until ~292 billion years from 1904** (effectively infinite for practical purposes)
3. **Clean migration path** from legacy v6 databases to modern v7 databases

## Key Differences from Unix `time_t`

| Feature | `time_t` (Unix) | `frontier_time_t` (Frontier) |
|---------|-----------------|------------------------------|
| **Epoch** | January 1, 1970 00:00:00 UTC | January 1, 1904 00:00:00 |
| **Size** | Platform-dependent (32 or 64 bits) | Always 64 bits (`int64_t`) |
| **Overflow** | 2038 on 32-bit systems | ~292 billion years from 1904 |
| **Portability** | Varies by platform | Consistent across all platforms |

### Epoch Conversion

To convert between Unix time (1970 epoch) and Frontier time (1904 epoch):

```c
/* Seconds between 1904 and 1970 */
#define SECONDS_1904_TO_1970 2082844800UL

/* Convert Unix time_t to frontier_time_t */
frontier_time_t unix_to_frontier(time_t unix_time) {
    return (frontier_time_t)unix_time + SECONDS_1904_TO_1970;
}

/* Convert frontier_time_t to Unix time_t */
time_t frontier_to_unix(frontier_time_t frontier_time) {
    return (time_t)(frontier_time - SECONDS_1904_TO_1970);
}
```

**Note**: These conversions are implemented in `Common/headers/timedate.h`. Use `timenow64()` for new code (returns `frontier_time_t`). The legacy `timenow()` function returns `unsigned long` (32-bit on most systems) and should not be used for new timestamp storage.

## How Frontier Uses frontier_time_t

### Database Storage

All timestamp fields in the v7 database format use `frontier_time_t`:

- **Table mutation times** (`tytableformats.timelastsave`, `timelastmodify`)
- **File modification times** stored in database objects
- **Any timestamp stored in ODB objects**

This ensures consistent timestamp representation regardless of the platform or system's native `time_t` size.

### Migration from v6 to v7

During database migration:

1. **v6 tables** store timestamps as 32-bit `long` values (legacy `time_t`)
2. **Migration code** reads these as `long`, then converts to `frontier_time_t` for v7 storage
3. **v7 tables** always store timestamps as 64-bit `frontier_time_t`

This migration preserves timestamp values correctly while ensuring future compatibility.

## Developer Guidelines

### When to Use frontier_time_t

Use `frontier_time_t` whenever:

- Storing timestamps in the database (v7 format)
- Performing timestamp arithmetic that will persist beyond the current session
- Comparing timestamps from different sources (database vs. system clock)

### When to Use time_t

Use system `time_t` only for:

- Interfacing with system APIs (e.g., `time()`, `localtime()`)
- Temporary timestamp calculations that don't persist to database

**Always convert `time_t` to `frontier_time_t` before storing in the database.**

### Example: Storing Current Time

```c
#include "timedate.h"

/* Get current time as frontier_time_t (RECOMMENDED) */
frontier_time_t now = timenow64();

/* Store in database table context */
tytableformats *formats = &(**htable).tformats;
formats->timelastmodify = now;
```

**Note**: Use `timenow64()` instead of the legacy `timenow()` function. While `timenow()` returns `unsigned long` (32-bit on most systems), `timenow64()` returns `frontier_time_t` (64-bit) and centralizes epoch conversion in the timedate module, following the architectural principle that boundary conversions should happen in one place.

**Legacy alternative (not recommended for new code)**:
```c
/* Legacy: timenow() returns unsigned long (32-bit) */
unsigned long legacy_now = timenow();
```

### Example: Comparing Timestamps

```c
/* Safe comparison across platforms */
frontier_time_t time1 = (**htable1).tformats.timelastmodify;
frontier_time_t time2 = (**htable2).tformats.timelastmodify;

if (time1 > time2) {
    /* table1 was modified more recently */
}
```

### Example: Retrieving Timestamp for Display

```c
#include "timedate.h"

/* Convert frontier_time_t to system time_t for formatting */
frontier_time_t saved_time = (**htable).tformats.timelastsave;
time_t unix_time = frontier_to_unix(saved_time);

/* Now use standard C library functions */
struct tm *tm_info = localtime(&unix_time);
printf("Last saved: %s", asctime(tm_info));
```

## Testing Considerations

When writing tests that involve timestamps:

1. **Use explicit frontier_time_t values** for predictable test behavior
2. **Test boundary cases** (e.g., timestamps near 2038, very old timestamps)
3. **Verify round-trip conversion**: store → retrieve → verify match
4. **Test migration**: ensure v6 timestamps migrate correctly to v7

Example test pattern:

```c
/* Store known timestamp */
frontier_time_t expected = (frontier_time_t)2147483647 + SECONDS_1904_TO_1970;
(**htable).tformats.timelastmodify = expected;

/* Save and reload table */
hashpacktable(htable, ...);
hashunpacktable(..., &htable);

/* Verify timestamp survived round-trip */
frontier_time_t actual = (**htable).tformats.timelastmodify;
assert(actual == expected);
```

## Related Planning Documents

- **`planning/phase3/MIGRATION_VALIDATION_REPORT.md`** - Database migration validation and time portability testing
- **Issue #159** - Time portability fix (32-bit to 64-bit time_t migration)
- **PR #193** - Implementation of frontier_time_t and related fixes

## Summary

`frontier_time_t` solves critical portability and future-proofing issues by:

- Providing a consistent 64-bit timestamp type across all platforms
- Using a 1904 epoch (compatible with legacy Frontier design)
- Ensuring databases remain valid beyond the 2038 overflow point
- Enabling clean migration from v6 (32-bit) to v7 (64-bit) databases

Developers should use `frontier_time_t` for all persistent timestamp storage and convert to/from system `time_t` only at the boundaries where system APIs are called.

## See Also

- **`Common/headers/timedate.h`** - Type definition and API for time handling
- **`Common/source/table_context.c`** - Implementation of timestamp mutation tracking using frontier_time_t
- **`tests/time_portability_test.c`** - Test suite validating frontier_time_t behavior across platforms
- **`planning/phase3/date_time_format_standard.md`** - Architectural decision for date/time format choices
- **PR #193** - Implementation PR introducing this standard
- **Issue #167** - Original P0 bug report on time_t portability
