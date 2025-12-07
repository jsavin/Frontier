# Processor Audit: `date`

**Status:** ✅ Ready for Implementation
**Audit Date:** 2025-12-05
**Auditor:** Claude (Sonnet 4.5)

---

## Basic Information

| Property | Value |
|----------|-------|
| **Processor Name** | `date` |
| **EFP ID** | 1012 (lang block) |
| **Verb Count** | 30 |
| **Window Required** | NO |
| **Documentation** | [date/](../../../docs/usertalk/docserver.userland.com/date/index.html) |
| **Stub Implementation** | [headless_date_verbs.c](../../../tests/headless_date_verbs.c) |

---

## Category Assessment

**Category:** ✅ **Core Functionality**

**Rationale:**
Date and time manipulation operations are essential system utilities with no GUI dependencies. Provides comprehensive date/time parsing, formatting, extraction, and arithmetic. All operations use standard C time APIs and work with Frontier's 64-bit 1904 epoch timestamps.

**Headless Compatibility:** ✅ **Full**

**Blocking Verbs:** None

---

## Verb Inventory

### Date Extraction (6 verbs)
| # | Verb Name | Signature | Description |
|---|-----------|-----------|-------------|
| 1 | `day` | `date.day(d) -> long` | Get day of month (1-31) |
| 2 | `month` | `date.month(d) -> long` | Get month number (1-12) |
| 3 | `year` | `date.year(d) -> long` | Get year (e.g., 2025) |
| 4 | `hour` | `date.hour(d) -> long` | Get hour (0-23) |
| 5 | `minute` | `date.minute(d) -> long` | Get minute (0-59) |
| 6 | `seconds` | `date.seconds(d) -> long` | Get seconds (0-59) |

### Date Formatting (8 verbs)
| # | Verb Name | Signature | Description |
|---|-----------|-----------|-------------|
| 7 | `shortString` | `date.shortString(d) -> string` | Format as "6/24/99" |
| 8 | `longString` | `date.longString(d) -> string` | Format as "Thursday, June 24, 1999" |
| 9 | `abbrevString` | `date.abbrevString(d) -> string` | Format as "Thu, Jun 24, 1999" |
| 10 | `dayString` | `date.dayString(d) -> string` | Get day name (e.g., "Thursday") |
| 11 | `timeString` | `date.timeString(d) -> string` | Format time as "4:52:54 PM" |
| 12 | `hourToString` | `date.hourToString(d) -> string` | Format hour as "3AM" or "11PM" |
| 13 | `netStandardString` | `date.netStandardString(d) -> string` | Format as GMT/RFC 822 string |
| 14 | `dateToIso8601String` | `date.dateToIso8601String(d) -> string` | Format as ISO 8601 |

### Date Navigation (10 verbs)
| # | Verb Name | Signature | Description |
|---|-----------|-----------|-------------|
| 15 | `tomorrow` | `date.tomorrow(d) -> date` | Get date for tomorrow |
| 16 | `yesterday` | `date.yesterday(d) -> date` | Get date for yesterday |
| 17 | `nextWeek` | `date.nextWeek(d) -> date` | Get date one week ahead |
| 18 | `prevWeek` | `date.prevWeek(d) -> date` | Get date one week back |
| 19 | `nextMonth` | `date.nextMonth(d) -> date` | Get same day next month |
| 20 | `prevMonth` | `date.prevMonth(d) -> date` | Get same day prev month |
| 21 | `nextYear` | `date.nextYear(d) -> date` | Get same date next year |
| 22 | `prevYear` | `date.prevYear(d) -> date` | Get same date prev year |
| 23 | `firstOfMonth` | `date.firstOfMonth(d) -> date` | Get first day of month |
| 24 | `lastOfMonth` | `date.lastOfMonth(d) -> date` | Get last day of month |

### Date Construction & Conversion (4 verbs)
| # | Verb Name | Signature | Description |
|---|-----------|-----------|-------------|
| 25 | `get` | `date.get(d, @year, @month, @day, @hour, @minute, @second)` | Extract components to address params |
| 26 | `set` | `date.set(year, month, day, hour, minute, second) -> date` | Construct date from components |
| 27 | `iso8601StringToDate` | `date.iso8601StringToDate(s) -> date` | Parse ISO 8601 string |
| 28 | `sameDay` | `date.sameDay(d1, d2) -> boolean` | Check if two dates are same day |

### Helper Functions (2 verbs)
| # | Verb Name | Signature | Description |
|---|-----------|-----------|-------------|
| 29 | `dayOfWeek` | `date.dayOfWeek(d) -> long` | Get day number (1=Sunday, 7=Saturday) |
| 30 | `dayOfWeekToString` | `date.dayOfWeekToString(n) -> string` | Convert day number to name |
| 31 | `monthToString` | `date.monthToString(n) -> string` | Convert month number to name |
| 32 | `dayOfYear` | `date.dayOfYear(d) -> long` | Get day of year (1-366) |
| 33 | `daysInMonth` | `date.daysInMonth(d) -> long` | Get number of days in month |
| 34 | `weekInYear` | `date.weekInYear(d) -> long` | Get week number in year |
| 35 | `weeksInMonth` | `date.weeksInMonth(d) -> long` | Get number of weeks in month |
| 36 | `getCurrentTimeZone` | `date.getCurrentTimeZone() -> long` | Get local timezone offset from GMT (seconds) |
| 37 | `timeZoneToString` | `date.timeZoneToString(offset) -> string` | Convert timezone offset to string |
| 38 | `versionLessThan` | `date.versionLessThan(v1, v2) -> boolean` | Compare version strings |

**Note:** Total is actually **35 verbs** (not 30) when including all documented verbs. Stub file has 30 - may need reconciliation.

---

## Implementation Analysis

### Complexity: **MEDIUM**

### Dependencies

- **Other Processors:**
  - `clock` - conceptually related (clock.now() provides base timestamps)
- **External Services:** None
- **OS-Specific Functionality:** YES (C standard library time.h: struct tm, mktime(), gmtime(), localtime(), strftime())
- **GUI/Window Context:** NO

### Key Implementation Notes

**Frontier Timestamp Format:**
- **Epoch:** January 1, 1904 00:00:00 UTC (Mac Classic standard)
- **Type:** `int64_t` (64-bit signed integer)
- **Units:** Seconds since epoch
- **Range:** Effectively unlimited (64-bit prevents 2040 overflow)
- **Unix Epoch Conversion:** Frontier = Unix + 2,082,844,800 seconds
- See `date_time_format_standard.md` for complete specification

**C Standard Library Integration:**
```c
// Convert Frontier timestamp to struct tm
int64_t frontier_time = /* from UserTalk */;
time_t unix_time = (time_t)(frontier_time - 2082844800LL);
struct tm *components = localtime(&unix_time);

// Extract components
int year = components->tm_year + 1900;
int month = components->tm_mon + 1;  // tm_mon is 0-11
int day = components->tm_mday;
int hour = components->tm_hour;
int minute = components->tm_min;
int seconds = components->tm_sec;

// Build Frontier timestamp from components
struct tm build = {0};
build.tm_year = year - 1900;
build.tm_mon = month - 1;
build.tm_mday = day;
build.tm_hour = hour;
build.tm_min = minute;
build.tm_sec = seconds;
time_t unix_result = mktime(&build);
int64_t frontier_result = (int64_t)unix_result + 2082844800LL;
```

**Formatting Strategies:**
- Use `strftime()` for standard formats
- Custom formatters for Frontier-specific formats
- Respect locale for month/day names (or use English defaults for portability)

**Date Arithmetic:**
- Tomorrow/Yesterday: Add/subtract 86400 seconds (24 hours)
- NextWeek/PrevWeek: Add/subtract 604800 seconds (7 days)
- NextMonth/PrevMonth: Convert to struct tm, adjust tm_mon, reconvert
- FirstOfMonth/LastOfMonth: Manipulate tm_mday
- Handle leap years automatically via mktime()

**ISO 8601 Format:**
- Standard: `YYYY-MM-DDTHH:MM:SSZ` or `YYYY-MM-DDTHH:MM:SS±HH:MM`
- Use `strptime()` for parsing (POSIX), manual parsing for Windows
- Use `strftime()` with "%Y-%m-%dT%H:%M:%SZ" for formatting

**RFC 822 (Net Standard) Format:**
- Format: `Day, DD Mon YYYY HH:MM:SS GMT`
- Example: `Thu, 24 Jun 1999 16:52:54 GMT`
- Use GMT for network protocols

**Timezone Handling:**
- `getCurrentTimeZone()`: Returns offset in seconds from GMT
- Use `timezone` global variable (POSIX) or Windows equivalent
- Handle daylight saving time with `tm_isdst`

**Version Comparison:**
- Parse version strings (e.g., "1.2.3" vs "1.2.10")
- Compare numerically, not lexicographically
- Handle different lengths (1.2 vs 1.2.3.4)

**Edge Cases:**
- Leap years (divisible by 4, except centuries unless divisible by 400)
- Month overflow (e.g., nextMonth on Jan 31 → Feb 28/29)
- Timezone changes (DST transitions)
- Invalid date components (e.g., month 13, day 32)
- Dates before 1904 epoch (negative timestamps - should error)
- Dates after 2038 (32-bit time_t overflow - use int64_t)

**Type Coercion:**
- Frontier dates are int64_t (long long)
- Month/day/year components are integers
- Return values for extraction are long
- Strings use bigstring type

---

## UserTalk Documentation Notes

From docserver.userland.com/date/:

**Core Verbs:**
- `date.set()` - Build date from components (year, month, day, hour, minute, second)
- `date.get()` - Extract components to address parameters
- Component extractors (day, month, year, hour, minute, seconds) - Simple read-only access

**Formatting Verbs:**
- `shortString()` - "6/24/99" (locale-dependent)
- `longString()` - "Thursday, June 24, 1999"
- `abbrevString()` - "Thu, Jun 24, 1999"
- `timeString()` - "4:52:54 PM"
- `netStandardString()` - RFC 822 GMT format for HTTP headers
- `dateToIso8601String()` - ISO 8601 for APIs

**Navigation Verbs:**
- tomorrow/yesterday - Simple day arithmetic
- nextWeek/prevWeek - 7-day arithmetic
- nextMonth/prevMonth - Calendar-aware (handles varying month lengths)
- nextYear/prevYear - Year arithmetic
- firstOfMonth/lastOfMonth - Set to start/end of current month

**Helper Verbs:**
- `dayOfWeek()` - Returns 1-7 (Sunday=1, Saturday=7)
- `dayOfYear()` - Returns 1-366
- `daysInMonth()` - Returns 28-31 depending on month/year
- `weekInYear()` - ISO 8601 week number
- `getCurrentTimeZone()` - Local timezone offset in seconds

---

## Testing Requirements

**Minimum Test Cases Per Verb:**
- Valid dates (various years, months, days)
- Boundary conditions (leap years, month ends, year boundaries)
- Invalid inputs (error handling)
- Timezone variations
- Format consistency

**Test Scenarios:**
```usertalk
// date.set() and date.get()
local (d = date.set(2025, 12, 5, 14, 30, 0))  → Frontier timestamp
date.year(d)                                   → 2025
date.month(d)                                  → 12
date.day(d)                                    → 5

// date.tomorrow() / date.yesterday()
local (today = date.set(2025, 12, 31, 12, 0, 0))
date.tomorrow(today)                           → 2026-01-01 12:00:00
date.yesterday(today)                          → 2025-12-30 12:00:00

// date.nextMonth() leap year handling
local (d = date.set(2024, 1, 31, 0, 0, 0))    → Jan 31, 2024
date.nextMonth(d)                              → Feb 29, 2024 (leap year)
local (d2 = date.set(2025, 1, 31, 0, 0, 0))   → Jan 31, 2025
date.nextMonth(d2)                             → Feb 28, 2025 (not leap)

// date.longString()
local (d = date.set(1999, 6, 24, 16, 52, 54))
date.longString(d)                             → "Thursday, June 24, 1999"

// date.netStandardString()
date.netStandardString(d)                      → "Thu, 24 Jun 1999 16:52:54 GMT"

// date.iso8601StringToDate() round-trip
local (iso = "2025-12-05T14:30:00Z")
local (d = date.iso8601StringToDate(iso))
date.dateToIso8601String(d)                    → "2025-12-05T14:30:00Z"

// date.versionLessThan()
date.versionLessThan("1.2.3", "1.2.10")        → true
date.versionLessThan("2.0", "1.9.9")           → false
```

**Edge Case Tests:**
- Leap year detection (2000, 2024, 2100)
- Month overflow (Jan 31 + 1 month → Feb 28/29)
- Timezone DST transitions
- Dates near epoch (1904-01-01)
- Dates far future (year 3000+)
- Invalid components (month 0, day 32)
- String parsing errors

**Platform Differences:**
- Locale-specific month/day names
- Date format conventions (US vs European)
- Timezone database differences
- strptime() availability (POSIX only)

---

## Implementation Effort

**Estimated Time:** 12-16 hours

**Breakdown:**
- Implementation: 6-8 hours (30+ verbs, complex date arithmetic)
- Testing: 4-5 hours (30 verbs × 4-5 test cases each)
- Platform-specific testing: 2-3 hours (timezone, locale, formatting)
- Documentation: 1 hour

**Confidence:** MEDIUM-HIGH - Well-defined C APIs, but date arithmetic edge cases require careful testing

**Platform-Specific Work:**
- Abstract timezone handling (POSIX vs Windows)
- Locale handling for month/day names
- strptime() alternative for Windows
- Testing DST transitions on each platform

---

## Priority & Sequencing

**Priority:** 🎯 **HIGH** (Tier 1)

**Recommended Implementation Order:** 13 (after clock, bit)

**Blockers/Prerequisites:**
- `clock` processor (conceptually related, provides clock.now())
- 64-bit timestamp support (already implemented)
- date_time_format_standard.md (already exists)

**Implementation Sequence:**
1. Implement date.set() and component extractors (day, month, year, hour, minute, seconds)
2. Implement date.get() for component extraction to addresses
3. Implement simple navigation (tomorrow, yesterday)
4. Implement month/year navigation (nextMonth, prevMonth, nextYear, prevYear)
5. Implement month boundaries (firstOfMonth, lastOfMonth)
6. Implement basic formatting (shortString, longString, timeString)
7. Implement helper functions (dayOfWeek, daysInMonth, etc.)
8. Implement ISO 8601 support (parse and format)
9. Implement RFC 822 support (netStandardString)
10. Implement timezone functions
11. Implement version comparison
12. Write comprehensive tests
13. Test formatting on each platform

---

## Quick Win Justification

**Why This Is a Quick Win:**
1. **Essential Utility:** Critical for any time-based operations
2. **Well-Defined APIs:** Standard C time functions (time.h, struct tm)
3. **No Dependencies:** Standalone functionality (conceptually uses clock)
4. **Portable:** Works on all platforms with standard C library
5. **High Value:** Enables date manipulation in UserTalk scripts
6. **Foundation:** Required for logging, scheduling, file operations

**Value Proposition:**
- Required for timestamp formatting in logs
- Essential for scheduled tasks and cron-like operations
- Enables date arithmetic in scripts
- Supports HTTP headers (RFC 822 dates)
- Provides ISO 8601 for modern APIs
- Foundation for file.created/modified dates

---

## Related Processors

- **clock** - Time primitives (clock.now() provides base timestamps)
- **file** - File operations use dates (created, modified)
- **sys** - System operations may use dates
- **string** - String formatting related to dates

---

## Special Considerations

**Frontier 1904 Epoch:**
- **CRITICAL:** All dates use Mac epoch (1904-01-01 00:00:00 UTC)
- **NOT** Unix epoch (1970-01-01)
- Conversion constant: +2,082,844,800 seconds
- Must convert at boundaries when calling C time functions
- Database timestamps (timecreated, timemodified) use same epoch
- See `date_time_format_standard.md` for helpers

**Leap Year Rules:**
- Year divisible by 4 → leap year
- EXCEPT if divisible by 100 → not leap year
- EXCEPT if divisible by 400 → leap year
- Examples: 2000 (leap), 1900 (not leap), 2024 (leap)
- mktime() handles this automatically

**Month Overflow Handling:**
- nextMonth(Jan 31) → should return Feb 28/29 (last day of month)
- NOT Mar 3 (31 days forward would overflow)
- Requires smart date arithmetic, not just adding seconds
- Use struct tm manipulation with mktime() normalization

**Timezone Considerations:**
- Local time vs GMT/UTC distinction
- DST (Daylight Saving Time) transitions
- Timezone database variations across platforms
- netStandardString() always uses GMT
- Other formats use local time by default

**String Formatting Portability:**
- Month/day names: Use English defaults or respect locale?
- Date separators: "/" vs "-" vs "."
- 12-hour vs 24-hour time display
- AM/PM placement and case
- Consider providing both localized and portable versions

**ISO 8601 Support:**
- Standard format: `YYYY-MM-DDTHH:MM:SSZ`
- Also support: `YYYY-MM-DDTHH:MM:SS±HH:MM` (timezone offset)
- Parsing flexibility (allow 'T' or space separator)
- Microseconds/milliseconds? (not in current spec)

**Version Comparison Algorithm:**
```c
// Correctly handle: "1.2.3" < "1.2.10" (NOT lexicographic)
// Split by '.', compare each segment as integer
// Handle different lengths: "1.2" vs "1.2.3.4"
```

**Thread Safety:**
- localtime() and gmtime() use static buffers
- Use localtime_r() / gmtime_r() (POSIX) for thread safety
- Windows has localtime_s() / gmtime_s()
- Consider thread-local storage for date buffers

---

## References

**Documentation:**
- DocServer: `docs/usertalk/docserver.userland.com/date/`
- Individual verb pages: `docs/usertalk/docserver.userland.com/date/{verb}.tmp`

**Implementation:**
- Stub: `tests/headless_date_verbs.c` (30 verbs defined)
- Legacy source: `/Users/jake/dev/tedchoward/Frontier/Common/source/langverbs.c`

**Standards:**
- POSIX time APIs: time(), mktime(), localtime(), gmtime(), strftime(), strptime()
- Windows time APIs: localtime_s(), gmtime_s()
- **Frontier epoch:** seconds since 1904-01-01 00:00:00 UTC
- **Unix epoch offset:** +2,082,844,800 seconds
- ISO 8601: [ISO 8601:2004](https://www.iso.org/iso-8601-date-and-time-format.html)
- RFC 822 (HTTP dates): [RFC 822 Section 5](https://www.ietf.org/rfc/rfc822.txt)
- See `planning/phase3/date_time_format_standard.md` for complete specification

---

## Next Steps

1. ✅ Audit complete - ready for implementation
2. ⏳ Implement core verbs (set, get, extractors) in `tests/headless_date_verbs.c`
3. ⏳ Implement navigation verbs (tomorrow, yesterday, nextMonth, etc.)
4. ⏳ Implement formatting verbs (shortString, longString, ISO 8601, RFC 822)
5. ⏳ Implement helper functions (dayOfWeek, daysInMonth, timezone)
6. ⏳ Create comprehensive unit tests
7. ⏳ Test leap year handling, month overflow, timezone edge cases
8. ⏳ Test formatting consistency across platforms
9. ⏳ Document platform-specific behaviors
10. ⏳ Update implementation status

---

**Audit Status:** ✅ Complete and Approved for Implementation
