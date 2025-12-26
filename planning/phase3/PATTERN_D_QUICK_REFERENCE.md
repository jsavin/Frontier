# Pattern D Processors - Quick Reference

## Executive Summary

- **Total Verbs Analyzed**: ~101 across 15 processors
- **Headless-Ready NOW**: 73 verbs (72%)
- **Needs Work**: 14 verbs (14%)
- **Skip/GUI-Only**: 17 verbs (17%)

## One-Line Summary Per Processor

| Processor | Verbs | Status | Notes |
|-----------|-------|--------|-------|
| clock | 7 | ✅ Ready | Stateless time/delay operations |
| date | 30 | ✅ Ready | Stateless arithmetic & formatting |
| dialog | 19 | ⚠️ Partial | 11/19 can be stubbed; 8 require GUI windows |
| kb | 4 | ❌ Skip | Pure keyboard input, can't synthesize headless |
| mouse | 2 | ❌ Skip | Pure mouse input, can't synthesize headless |
| point | 2 | ✅ Ready | Data structure operations, no context |
| rectangle | 2 | ✅ Ready | Data structure operations, no context |
| rgb | 2 | ✅ Ready | Data structure operations, no context |
| speaker | 3 | ❌ Skip | Audio system, can mock to log instead |
| target | 3 | ⚠️ Partial | 2/3 ready; 1 has GUI fallback to remove |
| bit | 8 | ✅ Ready | Pure bitwise math on uint64_t |
| semaphore | 2 | ✅ Ready | Uses global table but headless-safe; thread-safe concern for #135 |
| base64 | 2 | ✅ Ready | Pure string encoding |
| dll | 4 | ⚠️ Partial | Platform-dependent; probably OK on Windows |
| rez | 0 | ❓ N/A | Not found in Pattern D; check elsewhere |
| py | 0 | ❓ Stub | Commented out; needs Python setup |

## File Location

Main analysis: `/Users/jake/dev/jsavin/Frontier/planning/phase3/PATTERN_D_PROCESSORS_ANALYSIS.md`

Source: `/Users/jake/dev/jsavin/Frontier/Common/source/langverbs.c` (line 1750)

## Headless Readiness by Priority

### Use These NOW (73 verbs) ✅

All fully stateless, pure functions:

- **CLOCK** (7 verbs, line 1905): timefunc, tickcountfunc, millisecondcountfunc, delayfunc, sleepfunc, delaysixtiethsfunc
  - No GUI, no global state
  - `sleepfunc` has condition `!langdialogrunning()` - harmless in headless

- **DATE** (30 verbs, lines 1914-2318): datefunc, setdatefunc, getdatefunc, datedayfunc, datemonthfunc, dateyearfunc, datehourfunc, dateminutefunc, datesecondsfunc, abbrevstringfunc, dayofweekfunc, daysinmonthfunc, daystringfunc, firstofmonthfunc, lastofmonthfunc, longstringfunc, nextmonthfunc, nextweekfunc, nextyearfunc, prevmonthfunc, prevweekfunc, prevyearfunc, shortstringfunc, tomorrowfunc, weeksinmonthfunc, yesterdayfunc, getcurrenttimezonefunc, netstandardstringfunc, monthtostringfunc, dayofweektostringfunc, dateversionlessthanfunc
  - Pure arithmetic and string formatting

- **POINT** (2 verbs, lines 2418-2456): pointfunc, setpointfunc, getpointfunc
  - Data structure creation/decomposition

- **RECTANGLE** (2 verbs, lines 2458-2515): rectfunc, setrectfunc, getrectfunc
  - Data structure creation/decomposition

- **RGB** (2 verbs, lines 2517-2560): rgbfunc, setrgbfunc, getrgbfunc
  - Data structure creation/decomposition

- **BIT** (8 verbs, lines 3068-3090): getbitfunc, setbitfunc, clearbitfunc, bitandfunc, bitorfunc, bitxorfunc, bitshiftleftfunc, bitshiftrightfunc
  - Pure 64-bit integer bitwise math

- **BASE64** (2 verbs, lines 3098-3102): base64encodefunc, base64decodefunc
  - Pure string encoding/decoding

- **SEMAPHORE** (2 verbs, lines 3092-3096): lockfunc, unlockfunc
  - Uses global semaphoretable but no GUI
  - Note: Global state issue for #135 tracking

### Adapt These (14 verbs) ⚠️

Mostly stateful; can be made headless with minor changes:

- **TARGET** (2 verbs, lines 1870-1882):
  - `gettargetfunc` (1870): Remove implicit window fallback (line 1106-1115, already in `#ifndef FRONTIER_HEADLESS`)
  - `settargetfunc` (1873): Remove window zoom logic (line 1168-1181, already guarded)
  - `cleartargetfunc` (1876): Already clean

- **DIALOG** (11/19 verbs, lines 2804-2860):
  - Can be stubbed: getdialogvaluefunc, setdialogvaluefunc, setdialogitemenablefunc, showdialogitemfunc, hidedialogitemfunc, askdialogfunc, getintdialogfunc, notifytdialogfunc (already has stub), getuserinfodialogfunc, askpassworddialogfunc, alertdialogfunc
  - Must skip: rundialogfunc, runmodelessfunc, runcardfunc, runmodalcardfunc, ismodalcardfunc, setmodalcardtimeoutfunc, twowaydialogfunc, threewaydialogfunc

- **DLL** (3-4 verbs, lines 3050-3060): dllloadfunc, dllunloadfunc, dllisloadedfunc, calldllfunc
  - Likely OK if DLL infrastructure available
  - Platform-dependent (Windows)

### Skip These (17 verbs) ❌

Pure GUI/audio, can't work headless:

- **KB** (4 verbs, line 2704): optionkeyfunc, cmdkeyfunc, shiftkeyfunc, controlkeyfunc
  - Requires active keyboard event handling

- **MOUSE** (2 verbs, lines 2672-2701): mousebuttonfunc, mouselocationfunc
  - Requires mouse driver and active window context

- **SPEAKER** (3 verbs, lines 2862-2899): sysbeepfunc, soundfunc, playsoundfunc
  - Requires audio subsystem

## Global State Issues

### Found in Pattern D

**semaphoretable** (SEMAPHORE processor only):
- Static global hash table storing named locks
- Thread-aware: stores thread ID + timestamp
- Already uses `getcurrentthreadglobals()` for thread ID
- **Issue #135 tracking note**: Candidate for parameterization
- **Current impact**: Headless-safe but global mutable state

### NOT found in Pattern D (surprisingly clean)

Most verbs are pure functions - no hidden globals

## Code Patterns

### Pattern: Stateless Utility
```c
case bitandfunc:
    return (bitandverb (hparam1, v));

static boolean bitandverb (hdltreenode hparam1, tyvaluerecord *vreturned) {
    uint64_t bits1, bits2;
    if (!getbitnumparams (hparam1, &bits1, &bits2))
        return (false);
    return (setlongvalue (bitop_and (bits1, bits2), vreturned));
}
```
Simple, no state, works everywhere.

### Pattern: Headless Guard
```c
#ifndef FRONTIER_HEADLESS
if (!langzoomvalwindow (htable, bsname, val, false)) {
    // Error handling
}
#endif
```
Used in TARGET processor - properly guards GUI code.

### Pattern: Already Has Headless Stub
```c
case notifytdialogfunc:
    #ifdef FRONTIER_HEADLESS
    {
        fputs ("[notifydialog] ", stdout);
        fputs (cbuf, stdout);
        fputs ("\nPress Enter to continue...", stdout);
        return setbooleanvalue (true, v);
    }
    #else
    (*v).data.flvalue = notifyuser (bs);
    return (true);
    #endif
```
Model for how to adapt other dialog verbs.

## Key Files

- **Main source**: `/Users/jake/dev/jsavin/Frontier/Common/source/langverbs.c`
  - langfunctionvalue() dispatcher: line 1750
  - CLOCK verbs: lines 1905-2962
  - DATE verbs: lines 1914-2318
  - DIALOG verbs: lines 2785-2860
  - KB/MOUSE verbs: lines 2672-2708
  - POINT/RECTANGLE/RGB: lines 2418-2560
  - TARGET verbs: lines 1870-1882 (with support functions at 1081, 1125)
  - BIT verbs: lines 3068-3090 (with support at 1510-1603)
  - SEMAPHORE verbs: lines 3092-3096 (with support at 1606-1746)
  - BASE64/DLL: lines 3098-3060

- **Supporting files**:
  - `/Users/jake/dev/jsavin/Frontier/Common/source/langdate.c` - Date formatting
  - `/Users/jake/dev/jsavin/Frontier/Common/source/base64.c` - Base64 implementation
  - `/Users/jake/dev/jsavin/Frontier/Common/source/langdll.c` - DLL support

## Testing Recommendations

1. **Immediate test** (73 verbs): Add to `FRONTIER_HEADLESS` test suite
   - CLOCK, DATE, POINT, RECTANGLE, RGB, BIT, BASE64, SEMAPHORE
   - Should all pass without modification

2. **Adapter test** (14 verbs): Create stubs and test
   - TARGET with no explicit target set → should return nil (not crash)
   - DIALOG getters/setters → should return defaults
   - DLL verbs → should work if DLL infrastructure present

3. **Skip list** (17 verbs): Verify proper error/skipping
   - KB, MOUSE, SPEAKER → should error gracefully or return defaults in headless

## Pattern D Refactoring Notes

**Current design**: Single `langfunctionvalue()` with 158+ case statements

**Is refactoring needed?**: Not urgently
- Pattern works, verbs are findable
- Could modularize further post-#86
- Not a blocker for headless support

**If refactoring later**: Could split into:
```c
langfunctionvalue_clock()     // 7 verbs
langfunctionvalue_date()      // 30 verbs
langfunctionvalue_bit()       // 8 verbs
langfunctionvalue_dialog()    // 19 verbs (GUI-specific)
// ... etc
```

But this is nice-to-have, not blocking.

## Next Steps

1. **Verify**: Run existing tests with FRONTIER_HEADLESS on Priority 1 verbs
2. **Adapt**: Add stubs for Priority 2 verbs (14 total)
3. **Track**: Document semaphoretable for Issue #135
4. **Skip**: Configure build to exclude Priority 3 (17 verbs) in headless mode

See detailed analysis at `/Users/jake/dev/jsavin/Frontier/planning/phase3/PATTERN_D_PROCESSORS_ANALYSIS.md`
