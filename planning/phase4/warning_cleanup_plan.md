# Compiler Warning Cleanup Execution Plan

**Goal**: Zero compiler warnings on clean build
**Current State**: 154 warnings
**Created**: 2026-01-31

---

## Overview

A clean build of `frontier-cli` produces 154 compiler warnings. This plan organizes them into discrete work packages that can be executed independently, with clear verification steps and model requirements.

### Warning Summary by Category

| Category | Count | Priority |
|----------|-------|----------|
| Unused Parameters | 54 | Medium |
| Type Mismatches | 48 | High |
| Macro Redefinitions | 24 | Easy |
| Unused Functions | 14 | Low |
| Unused Variables | 7 | Low |
| Potential Bugs | 4 | Critical |
| Other | 3 | Trivial |

---

## Work Packages

### WP-1: Potential Bug Fixes (CRITICAL)

**Model**: Opus
**Warnings**: 4
**Estimated Time**: 2 hours
**Risk**: HIGH - may affect runtime behavior

These warnings indicate actual bugs or logic errors that need careful analysis:

| File | Line | Warning | Fix |
|------|------|---------|-----|
| `Common/source/langexternal.c` | 942 | Non-void function missing return | Add appropriate return value; analyze control flow |
| `Common/source/langxml.c` | 915 | Uninitialized variable `hnode` | Initialize `hnode = NULL;` |
| `Common/source/langscan.c` | 106 | Comparison always false (`byte` vs signed `char`) | Change `chtrademark` in `portable/standard_portable.h` from `(char)0xAA` to `(byte)0xAA` |
| `Common/source/langverbs.c` | 3397 | `abs()` on `long` truncates | Change `abs(n)` to `labs(n)` |

**Verification**:
1. `make -C frontier-cli clean && make -C frontier-cli 2>&1 | grep -E "(langexternal|langxml|langscan|langverbs).*warning"`
2. `./tools/run_headless_tests.sh`
3. `cd tests && make test-integration`

---

### WP-2: Macro Redefinitions

**Model**: Haiku
**Warnings**: 24
**Estimated Time**: 30 minutes
**Risk**: LOW - compile-time only

#### 2a: `conditionalshortswap` (20 warnings)

**Files**:
- `Common/headers/byteorder.h:46`
- `portable/standard_portable.h:359`

**Fix**: Add include guard in `standard_portable.h`:
```c
#ifndef conditionalshortswap
#define conditionalshortswap(x) OSSwapInt16(x)
#endif
```

#### 2b: `STR_*` macros (4 warnings)

**Files**:
- `Common/headers/stringdefs.h` (definitions)
- `Common/source/langxml.c:60-64` (duplicates)

**Fix**: Remove duplicate definitions from `langxml.c`, lines 60-64. Use `#include "stringdefs.h"` if not already included.

**Verification**:
1. `make -C frontier-cli clean && make -C frontier-cli 2>&1 | grep -c "macro redefined"` should return 0
2. `./tools/run_headless_tests.sh`

---

### WP-3: Date Verb Type Mismatches

**Model**: Sonnet
**Warnings**: 25
**Estimated Time**: 1 hour
**Risk**: MEDIUM - API change but same semantics

**File**: `tests/headless_date_verbs.c`

**Pattern**: All 25 instances use `getlongvalue()` (expects `long*`) with `int64_t date` variables.

**Fix**: Change `getlongvalue(hparam1, N, &date)` to `getdatevalue(hparam1, N, &date)` at lines:
82, 171, 185, 199, 213, 226, 238, 251, 264, 276, 288, 300, 312, 324, 337, 350, 364, 379, 399, 451, 466, 481, 496, 511, 525

**Verification**:
1. `make -C frontier-cli clean && make -C frontier-cli 2>&1 | grep "headless_date_verbs" | grep -c warning` should return 0
2. Run date verb integration tests

---

### WP-4: opverbs.c Type Mismatches

**Model**: Sonnet
**Warnings**: 12
**Estimated Time**: 1 hour
**Risk**: MEDIUM - pointer aliasing

**File**: `Common/source/opverbs.c`

**Pattern**: Passing `hdloutlinevariable` to functions expecting `hdlexternalvariable`

**Lines**: 1044, 1081, 1156, 1185, 1219, 1240, 1470, 1490, 1687, 2165, 4086, 4426

**Fix Options**:
1. Add explicit casts: `(hdlexternalvariable)hv`
2. Or change `opverbinmemory` signature to accept `hdloutlinevariable`

**Verification**:
1. `make -C frontier-cli clean && make -C frontier-cli 2>&1 | grep "opverbs.c" | grep -c warning` should return 0
2. Run outline verb integration tests

---

### WP-5: Other Type Mismatches

**Model**: Sonnet
**Warnings**: 11
**Estimated Time**: 2 hours
**Risk**: MEDIUM

| File | Lines | Issue |
|------|-------|-------|
| `Common/source/langhtml.c` | 7106, 8676 | `unsigned long*` → `int64_t*` |
| `Common/source/stringverbs.c` | 1885, 1906 | `unsigned long*` → `int64_t*` |
| `Common/source/langsystypes.c` | 730, 739 | `tyfsname*` vs `HFSUniStr255*` |
| `Common/source/langxml.c` | 853, 868 | `Handle` vs `bigstring` |
| `tests/headless_script_verbs.c` | 126, 388 | `hdltreenode` vs `Handle` |
| `portable/fileverbs_portable.c` | 2120 | `hdlfilespec*` vs `ptrfilespec` |
| Various | - | `char*` / `Ptr` / `ptrstring` mismatches |

**Verification**:
1. Rebuild and check each file has zero warnings
2. Run full test suite

---

### WP-6: Unused Parameters in Stub Verbs

**Model**: Haiku
**Warnings**: 54
**Estimated Time**: 1 hour
**Risk**: LOW - cosmetic only

**Pattern**: Stub verb implementations don't use standard parameters.

| Parameter | Count |
|-----------|-------|
| `hparam1` | 22 |
| `vreturned` | 24 |
| `bserror` | 7 |
| Other | 1 |

**Files** (in `tests/`):
- headless_osa_verbs.c
- headless_menu_verbs.c
- headless_pict_verbs.c
- headless_mouse_verbs.c
- headless_speaker_verbs.c
- headless_bit_verbs.c
- headless_dll_verbs.c
- headless_python_verbs.c
- headless_htmlcontrol_verbs.c
- headless_statusbar_verbs.c
- headless_re_verbs.c
- headless_sqlite_verbs.c
- headless_mysql_verbs.c
- headless_window_verbs.c
- headless_rez_verbs.c
- headless_search_verbs.c
- headless_filemenu_verbs.c
- headless_editmenu_verbs.c
- headless_launch_verbs.c
- headless_clipboard_verbs.c
- headless_mrcalendar_verbs.c
- And more...

**Also**:
- `frontier-cli/cli_utils.c:34` - `label` parameter
- `Common/source/langhash.c:1262` - `ht` parameter
- `Common/source/timedate.c:346` - `flabbreviate` parameter

**Fix**: Add `(void)param;` at start of each function:
```c
static boolean xxx_valueproc(short token, hdltreenode hparam1,
                             tyvaluerecord *vreturned,
                             bigstring bserror) {
    (void)hparam1;
    (void)vreturned;
    (void)bserror;

    switch(token) {
        // ...
    }
}
```

**Verification**:
1. `make -C frontier-cli 2>&1 | grep -c "unused parameter"` should return 0

---

### WP-7: Unused Functions

**Model**: Haiku (removal) or Sonnet (analysis)
**Warnings**: 14
**Estimated Time**: 1-2 hours
**Risk**: LOW

| Function | File | Recommendation |
|----------|------|----------------|
| `xmlfunctionvalue` | langxml.c | Keep - future use |
| `xmldecodeentities` | langxml.c | Analyze if needed |
| `htmlfunctionvalue` | langhtml.c | Keep - future use |
| `wp_portable_utf8_to_rtf` | wptext_runtime.c | Used at line 754 - investigate false positive |
| `tcp_timeout_remaining_ms` | tcpverbs.c | Keep - timeout infrastructure |
| `odb_detect_cancoon_record` | odbengine.c | Analyze if dead code |
| `langinstallresources` | langstartup.c | Analyze if dead code |
| `headless_should_log` | langstartup.c | Analyze if dead code |
| `langhash_materialize_trace_enabled` | langhash.c | Keep - debugging |
| `db_context_guard_enter` | db_format.c | Keep - guard pattern |
| `db_context_guard_exit` | db_format.c | Keep - guard pattern |
| `db_format_fixup_external_handles` | db_format.c | Analyze if dead code |
| `headless_menu_dup_block` | headless_menu_stubs.c | Analyze if needed |
| `fp_from` | file_portable.c | Analyze if dead code |
| `fixdate` | timedate.c | Analyze if dead code |
| `adjustforcurrenttimezone` | timedate.c | Analyze if dead code |
| `editvalue` | langverbs.c | Analyze if dead code |

**Fix Options**:
1. Remove if truly dead code
2. Add `__attribute__((unused))` if intentionally kept
3. Wrap with `#if 0` / `#endif` if temporarily disabled

**Verification**:
1. `make -C frontier-cli 2>&1 | grep -c "unused function"` should return 0

---

### WP-8: Unused Variables

**Model**: Haiku
**Warnings**: 7
**Estimated Time**: 30 minutes
**Risk**: LOW

| Variable | File | Line | Type |
|----------|------|------|------|
| `sockfd` | tcpverbs.c | 2582 | set but not used |
| `discarded_bytes` | langhtml.c | 2657 | set but not used |
| `has_default` | headless_dialog_verbs.c | 280 | set but not used |
| `saved_fllocaldotparamsonly` | langvalue.c | 3848 | set but not used |
| `valcopy` | headless_xml_verbs.c | 157 | unused |
| `hn` | headless_xml_verbs.c | 161 | unused |
| `hnode` | headless_webserver_verbs.c | 71 | unused |
| `sysBundle` | sysshellcall.c | 82 | unused |
| `success` | fileverbs_portable.c | 1150 | unused |
| `semaphorewhen` | langverbs.c | 81 | unused |
| `mode` | fileverbs_portable.c | 1059 | unused |
| `flpush` | langpack.c | 520 | unused |
| `bsext` | fileverbs_portable.c | 1660 | unused |

**Fix**: Remove variable or add `(void)var;` if intentionally unused.

**Verification**:
1. `make -C frontier-cli 2>&1 | grep -c "unused variable"` should return 0

---

### WP-9: Other Warnings

**Model**: Haiku
**Warnings**: ~6
**Estimated Time**: 15-30 minutes
**Risk**: LOW

| Warning | File | Line | Fix |
|---------|------|------|-----|
| Non-portable include path case | sysshellcall.c | 44 | Change `"CallMachOFramework.h"` to match actual filename case |
| Block comment contains `/*` | db_format.c | 1868 | Reword comment to avoid nested comment markers |
| C23 label before declaration | langhash.c | 3669 | Move declaration before label or add braces |
| Illegal char encoding | iso8859.c | 390 | Expected - add `#pragma clang diagnostic ignored` |
| Sign comparison | tcpverbs.c | 676 | Cast to matching type |
| Sign comparison | fileverbs_portable.c | 1533, 1576 | Cast to matching type |
| Pointer/integer comparison | headless_op_verbs.c | 149 | Use proper NULL comparison |

**Verification**:
1. Rebuild and verify no warnings remain

---

## Execution Order

**Recommended sequence** (bugs first, then easy wins, then systematic cleanup):

1. **WP-1**: Potential Bugs (4 warnings) - CRITICAL
2. **WP-2**: Macro Redefinitions (24 warnings) - Easy win
3. **WP-3**: Date Verb Types (25 warnings) - Mechanical
4. **WP-6**: Unused Parameters (54 warnings) - Mechanical
5. **WP-4**: opverbs Types (12 warnings) - Needs care
6. **WP-5**: Other Types (11 warnings) - Needs care
7. **WP-7**: Unused Functions (14 warnings) - Analysis needed
8. **WP-8**: Unused Variables (7 warnings) - Trivial
9. **WP-9**: Other (6 warnings) - Trivial

---

## Risk Assessment

| Work Package | Risk Level | Mitigation |
|--------------|------------|------------|
| WP-1 (Bugs) | **HIGH** | Careful code review; full test suite; may need Opus review |
| WP-2 (Macros) | LOW | Compile-time only; test both builds |
| WP-3 (Date) | MEDIUM | Verify `getdatevalue` semantics match; run date tests |
| WP-4 (opverbs) | MEDIUM | Review pointer aliasing; run outline tests |
| WP-5 (Types) | MEDIUM | Case-by-case analysis; run affected tests |
| WP-6-9 | LOW | Cosmetic changes; automated testing sufficient |

---

## Success Criteria

1. **Zero warnings on clean build**:
   ```bash
   make -C frontier-cli clean && make -C frontier-cli 2>&1 | grep -c "warning:"
   # Must return 0
   ```

2. **All unit tests pass**:
   ```bash
   ./tools/run_headless_tests.sh
   # Must exit 0
   ```

3. **All integration tests pass**:
   ```bash
   cd tests && make test-integration
   # Must exit 0
   ```

4. **No functional regressions** in:
   - Date handling
   - Verb execution
   - Outline operations
   - File operations

---

## Progress Tracking

| WP | Status | Warnings Before | Warnings After | Date |
|----|--------|-----------------|----------------|------|
| WP-1 | Pending | 4 | - | - |
| WP-2 | Pending | 24 | - | - |
| WP-3 | Pending | 25 | - | - |
| WP-4 | Pending | 12 | - | - |
| WP-5 | Pending | 11 | - | - |
| WP-6 | Pending | 54 | - | - |
| WP-7 | Pending | 14 | - | - |
| WP-8 | Pending | 7 | - | - |
| WP-9 | Pending | 3+ | - | - |
| **Total** | | **154** | - | - |

---

**Last Updated**: 2026-01-31
**Author**: System Architect Agent
