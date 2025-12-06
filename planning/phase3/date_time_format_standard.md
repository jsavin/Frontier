# Frontier Date/Time Format Standard

**Status:** Design Decision Required
**Date:** 2025-12-05
**Issue:** Legacy Frontier uses Mac epoch (1904) with 32-bit unsigned, which overflows in year 2040

---

## Background

### Legacy Frontier Time Format

**Epoch:** January 1, 1904 00:00:00 (Classic Mac OS standard)
**Type:** `unsigned long` (32-bit unsigned integer)
**Range:** 1904-01-01 to 2040-02-06 06:28:15
**Overflow:** Year 2050 causes overflow (wraps to 1904)

**Evidence from legacy source:**
```c
// From Common/source/timedate.c:
// "time in seconds since 12:00 AM 1904"

// From Common/headers/lang.h, op.h:
unsigned long timecreated, timelastsave; /*number of seconds since 1/1/04*/
```

**Math:**
- Unix epoch (1970) - Mac epoch (1904) = 2,082,844,800 seconds (0x7C25B080)
- 32-bit unsigned max: 4,294,967,295
- Mac epoch + max = February 6, 2040 06:28:15

---

## Problem Statement

### Objects with Time Metadata

All non-scalar UserTalk objects store creation and modification times:

1. **WPText** (word processor documents)
   - `timecreated`, `timelastsave` in `wpengine.h`

2. **Outlines**
   - `timecreated`, `timelastsave` in `op.h`

3. **Scripts**
   - `timecreated`, `timelastsave` in `lang.h`

4. **Pictures**
   - `timecreated`, `timelastsave` in `pict.h`

5. **Tables**
   - `timecreated`, `timelastsave` in `tableformats.h`

6. **Files** (file metadata)
   - `timecreated`, `timemodified`, `timeaccessed` in `file.h`

### UserTalk Verbs Affected

- `timecreated(adrObject)` - Get creation time
- `timemodified(adrObject)` - Get modification time
- `clock.now()` - Current time
- `clock.set(time)` - Set system time
- All `date.*` verbs operate on time values

### User Impact

Applications rely on these timestamps:
- Document management systems
- Content publishing workflows
- Backup/sync operations
- Audit trails
- User mentions "at least one application I created that relies on these properties"

---

## Design Options

### Option 1: Break Compatibility - Unix Epoch ❌

**Change:** Use Unix epoch (1970) with 64-bit signed integers

**Pros:**
- Standard modern format
- Huge range (year ~292 billion)
- Matches external APIs

**Cons:**
- **BREAKING:** Existing .root databases become incompatible
- All stored timestamps off by 66 years (2,082,844,800 seconds)
- Migration tool required for every existing database
- User applications break without code changes
- Loss of historical data fidelity

**Verdict:** **Rejected** - Too destructive for compatibility

---

### Option 2: Maintain Compatibility - 64-bit Mac Epoch ✅

**Change:** Keep Mac epoch (1904), upgrade to 64-bit signed integers

**Pros:**
- **100% compatible** with legacy databases
- Existing .root files work without conversion
- User scripts continue to work unchanged
- Application code unaffected
- Extends range to year ~584 billion (effectively infinite)
- Solves 2040 overflow problem

**Cons:**
- Internal format differs from Unix standard
- Need conversion when interfacing with external systems
- Slightly larger storage (8 bytes vs 4 bytes per timestamp)

**Implementation:**
```c
typedef int64_t frontier_time_t;  // Seconds since 1904-01-01 00:00:00

// Conversion helpers
int64_t unix_to_frontier_time(time_t unix_time) {
    return unix_time + 2082844800LL;  // 0x7C25B080
}

time_t frontier_to_unix_time(int64_t frontier_time) {
    return (time_t)(frontier_time - 2082844800LL);
}
```

**Verdict:** **RECOMMENDED**

---

### Option 3: Hybrid Approach ⚠️

**Change:** Internal 64-bit Mac epoch, external APIs return Unix epoch

**Pros:**
- Database compatibility maintained
- External APIs follow modern conventions
- Best of both worlds?

**Cons:**
- **CONFUSING:** Same verb returns different values depending on context
- Scripts that store `clock.now()` results would be incompatible with database timestamps
- Breaks user mental model
- Complex implementation with many edge cases

**Verdict:** **Rejected** - Too confusing, violates principle of least surprise

---

## Recommended Implementation

### Standard: 64-bit Mac Epoch

**Type Definition:**
```c
// In Common/headers/frontier.h or standard.h
typedef int64_t frontier_time_t;  // Seconds since 1904-01-01 00:00:00 UTC

// For database v7 format
#define FRONTIER_EPOCH_YEAR 1904
#define FRONTIER_EPOCH_TO_UNIX_OFFSET 2082844800LL
```

### Database Storage

**v7 Format:**
- All time fields: 64-bit signed integer (big-endian)
- Epoch: January 1, 1904 00:00:00 UTC
- Range: Effectively unlimited (±292 billion years from 1904)

**Affected Fields:**
- Table header: database creation/modification times (if any)
- Value records: `timecreated`, `timelastsave` for all non-scalars
- WPText, Outline, Script, Pict, Table internal structures

### Migration from v6

**v6 → v7 Conversion:**
- Read 32-bit unsigned `timecreated`, `timelastsave` from v6
- Sign-extend to 64-bit (zero-extend since unsigned)
- Write as 64-bit signed in v7
- **No epoch change needed** - both use 1904

### UserTalk Verb Behavior

**clock.now():**
- Returns `frontier_time_t` (seconds since 1904)
- Internally converts from `time(NULL)` or similar
- Consistent with database timestamps

**date.* verbs:**
- All operate on `frontier_time_t`
- No changes to script behavior
- Extended range prevents overflow

**timecreated() / timemodified():**
- Return `frontier_time_t` from object metadata
- Directly compatible with `clock.now()` results
- Comparisons work as expected

### External API Conversions

When interfacing with external systems (HTTP headers, file metadata, etc.):

```c
// Reading external time (e.g., HTTP Date header)
time_t external_time = parse_http_date(header);
frontier_time_t frontier_time = unix_to_frontier_time(external_time);

// Writing external time (e.g., Last-Modified header)
frontier_time_t frontier_time = get_object_timemodified();
time_t unix_time = frontier_to_unix_time(frontier_time);
format_http_date(unix_time, header);
```

---

## Testing Requirements

### Compatibility Tests

1. **v6 Migration:**
   - Migrate v6 database with objects created in various years (1990, 2000, 2020)
   - Verify `timecreated()` returns correct values
   - Verify dates don't shift

2. **Range Tests:**
   - Create objects with dates in 2040, 2050, 2100
   - Verify no overflow
   - Verify correct timestamp storage/retrieval

3. **Comparison Tests:**
   - Compare timestamps using `>`, `<`, `==`
   - Verify sorting by timestamp works
   - Test `clock.now()` comparisons with stored times

4. **Cross-Platform:**
   - Verify same .root file produces same timestamp values on macOS, Linux, Windows
   - Check big-endian storage in database

### Edge Cases

- Year 1904 (epoch)
- Year 2040 (old overflow point)
- Year 2050 (verified working)
- Year 9999 (far future)
- Negative values (before 1904 - should error or wrap?)

---

## Documentation Updates Needed

1. **clock.md audit** - Update with Mac epoch standard
2. **date.md audit** - Document epoch and range
3. **Database format docs** - Specify 64-bit time fields
4. **Migration guide** - Explain v6→v7 time handling
5. **API documentation** - Note conversion for external systems

---

## Action Items

- [ ] Update clock.md audit with Mac epoch decision
- [ ] Define `frontier_time_t` typedef in headers
- [ ] Implement conversion helper functions
- [ ] Update database format specification (if exists)
- [ ] Add time format tests
- [ ] Document in user-facing documentation

---

## Decision

**APPROVED:** Use 64-bit Mac epoch (1904) for all Frontier time values

**Rationale:**
- Maintains 100% compatibility with legacy databases
- Solves 2040 overflow problem
- Minimal code changes
- No user script modifications needed
- Applications continue to work
- Clean, understandable model

**Owner:** Implementation team
**Target:** Include in Phase 3 clock/date processor implementation
