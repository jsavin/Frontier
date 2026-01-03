# Comprehensive DateTime Handling Audit

## Executive Summary
Systematic audit of all datetime handling in Frontier codebase to identify non-compliant usage patterns that could cause truncation or portability issues.

**Audit Date**: 2026-01-02  
**Scope**: Common/source, Common/headers, portable directories  
**Total Files Audited**: 462

---

## Critical Issues Found

### CRITICAL #1: datenetstandardstring() Function Signature

**Location**: `Common/source/langdate.c:75`  
**Function**: `boolean datenetstandardstring (long localdate, tyvaluerecord *vreturn)`

**Problem**: 
- Function accepts `long` parameter (32-bit on Windows, 32-bit systems)
- Callers pass 64-bit timestamps, causing silent truncation
- Function internally converts to `int64_t` but damage is already done

**Impact**:
- RFC 822 date strings generated for post-2038 timestamps will be wrong
- XML export timestamps truncated
- RSS/OPML generation affected

**Callers Affected**:
1. `opxml.c:1124, 1127` - Outline XML export (dateCreated, dateModified tags)
2. `langverbs.c:2334` - date.netstandardstring verb
3. `langhtml.c:5183` - HTML generation timestamps

**Recommended Fix**:
```c
// Change signature from:
boolean datenetstandardstring (long localdate, tyvaluerecord *vreturn)

// To:
boolean datenetstandardstring (int64_t localdate, tyvaluerecord *vreturn)
```

---

### CRITICAL #2: opxml.c Local Variable Truncation

**Location**: `Common/source/opxml.c:1102`  
**Function**: `opxmlbuildhead()`

**Problem**:
```c
unsigned long timecreated, timemodified;  // Line 1102 - WRONG!
timecreated = (**ho).timecreated;        // Line 1120 - truncates int64_t to unsigned long
timemodified = (**ho).timelastsave;      // Line 1122 - truncates int64_t to unsigned long
```

**Impact**:
- OPML export loses timestamp precision
- Outline dateCreated/dateModified XML tags truncated to 32-bit

**Recommended Fix**:
```c
int64_t timecreated, timemodified;  // Use full 64-bit
```

---

### MEDIUM: Legacy Code Still Using 32-bit Timestamps

**Location**: `Common/source/legacy/tablepack_legacy.c`

**Functions**:
- `tableverbgettimes_legacy (hdlexternalvariable h, long *timecreated, long *timemodified, hdlhashnode hnode)`
- `tableverbsettimes_legacy (hdlexternalvariable h, long timecreated, long timemodified, hdlhashnode hnode)`

**Status**: ✅ **OK - Intentional for legacy v6 format compatibility**

These functions are specifically for reading/writing v6 database format which uses 32-bit timestamps on disk. This is acceptable as long as:
1. Modern code uses 64-bit APIs
2. Conversion happens at disk I/O boundary
3. In-memory representation is 64-bit

---

## Low Priority Issues

### claybrowser.h - Mac-Only GUI Code

**Location**: `Common/headers/claybrowser.h`

**Problem**:
```c
typedef struct tybrowserinfo {
    long timecreated, timemodified;  // Should be int64_t
    // ...
}
```

**Status**: ⚠️ **Low Priority - Not used in headless builds**

**Recommendation**: Fix during Mac GUI modernization work, not blocking for headless.

---

## Verified Correct Usage

### ✅ Core Data Structures - All Using int64_t

1. **op.h** - `tyoutlinerecord`:
   ```c
   int64_t timecreated, timelastsave;  ✅
   ```

2. **lang.h** - `tyhashtable`:
   ```c
   int64_t timecreated, timelastsave;  ✅
   ```

3. **tableformats.h** - `tyversion1tablediskrecord`:
   ```c
   int64_t timecreated, timelastsave;  ✅
   ```

4. **pict.h** - `typictrecord`:
   ```c
   int64_t timecreated, timelastsave;  ✅
   ```

5. **wpengine.h** - `tywprecord`:
   ```c
   int64_t timecreated, timelastsave;  ✅
   ```

6. **file.h** - File info:
   ```c
   int64_t timecreated, timemodified, timeaccessed;  ✅
   ```

### ✅ API Functions - Correct Signatures

1. **langexternal.c**:
   ```c
   boolean langexternalgettimes (hdlexternalhandle h, int64_t *timecreated, int64_t *timemodified, hdlhashnode hnode)  ✅
   boolean langexternalsettimes (hdlexternalhandle h, int64_t timecreated, int64_t timemodified, hdlhashnode hnode)  ✅
   ```

2. **wpverbs.c** (fixed in PR #232):
   ```c
   boolean wpverbgettimes (hdlexternalvariable h, int64_t *timecreated, int64_t *timemodified)  ✅
   boolean wpverbsettimes (hdlexternalvariable h, int64_t timecreated, int64_t timemodified)  ✅
   ```

3. **pictverbs.c**:
   ```c
   boolean pictverbgettimes (hdlexternalvariable h, int64_t *timecreated, int64_t *timemodified)  ✅
   boolean pictverbsettimes (hdlexternalvariable h, int64_t timecreated, int64_t timemodified)  ✅
   ```

---

## Out of Scope: Tick-Based Timing

The following use `long` or `unsigned long` for tick counts (not timestamps):

- `process.c` - Process timing uses ticks (60 or 1000 per second)
- `mouse.c` - Mouse double-click timing
- `op.c` - Keyboard timing

**Status**: ✅ **OK - Not calendar timestamps**

These are short-duration intervals measured in system ticks, not calendar dates. 32-bit is sufficient for timing intervals up to ~828 days at 60Hz or ~49 days at 1000Hz.

---

## Recommended Action Plan

### Phase 1: Critical Fixes (Block PR Merges)

1. **Fix datenetstandardstring() signature**:
   - Change parameter from `long` to `int64_t`
   - Update all callers
   - Verify XML/OPML export correctness

2. **Fix opxml.c local variables**:
   - Change `unsigned long timecreated, timemodified` to `int64_t`
   - Test OPML export with post-2038 dates

### Phase 2: Code Quality (Next Sprint)

3. **Update claybrowser.h** (Mac GUI):
   - Change `long timecreated, timemodified` to `int64_t`
   - Only when Mac GUI work resumes

### Phase 3: Future Proofing (Ongoing)

4. **Add pre-commit check**:
   - Grep for new `long.*time` patterns in PRs
   - Flag for review before merge

5. **Update CLAUDE.md**:
   - Add datetime handling to pre-merge checklist
   - Reference this audit document

---

## Testing Requirements

After fixes applied:

1. **Unit Tests**:
   - Test datenetstandardstring() with dates >2038
   - Verify no truncation in string output

2. **Integration Tests**:
   - Export OPML with post-2038 timestamps
   - Verify dateCreated/dateModified accuracy
   - Round-trip test: export → re-import → verify

3. **Regression Tests**:
   - Ensure pre-2000 dates still work
   - Verify timezone handling unchanged

---

## Files Requiring Changes

1. **Common/source/langdate.c** - datenetstandardstring() signature
2. **Common/headers/lang.h** - datenetstandardstring() declaration
3. **Common/source/opxml.c** - Local variable types in opxmlbuildhead()
4. **(Optional) Common/headers/claybrowser.h** - GUI structure (low priority)

---

## References

- **Original Audit**: `planning/phase3/uint32_timestamp_audit.md`
- **Standard**: `docs/frontier_time_t_standard.md`
- **PR #231**: File verb bindings (discovered wptext issue)
- **PR #232**: WPText timestamp migration
