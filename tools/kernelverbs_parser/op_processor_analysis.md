# Pattern C Analysis: op Processor Verb Mapping

## Overview

The `op` processor demonstrates an **inconsistent prefix-stripping pattern** that requires a manual mapping table.

## RC File Verbs vs C Enum Tokens

### Complete Mapping Table

| Index | RC Verb Name          | C Enum Token            | Transformation Rule                |
|-------|-----------------------|-------------------------|------------------------------------|
| 0     | getlinetext           | linetextfunc            | Strip "get" prefix                 |
| 1     | level                 | levelfunc               | Direct append "func"               |
| 2     | countsubs             | countsubsfunc           | Direct append "func"               |
| 3     | countsummits          | countsummitsfunc        | Direct append "func"               |
| 4     | go                    | gofunc                  | Direct append "func"               |
| 5     | firstsummit           | firstsummitfunc         | Direct append "func"               |
| 6     | expand                | expandfunc              | Direct append "func"               |
| 7     | collapse              | collapsefunc            | Direct append "func"               |
| 8     | subsexpanded          | getexpandedfunc         | **NO PATTERN** - manual mapping    |
| 9     | insert                | insertfunc              | Direct append "func"               |
| 10    | find                  | findfunc                | Direct append "func"               |
| 11    | sort                  | sortfunc                | Direct append "func"               |
| 12    | setlinetext           | setlinetextfunc         | Direct append "func"               |
| 13    | reorg                 | reorgfunc               | Direct append "func"               |
| 14    | promote               | promotefunc             | Direct append "func"               |
| 15    | demote                | demotefunc              | Direct append "func"               |
| 16    | hoist                 | hoistfunc               | Direct append "func"               |
| 17    | dehoist               | dehoistfunc             | Direct append "func"               |
| 18    | deletesubs            | deletesubsfunc          | Direct append "func"               |
| 19    | deleteline            | deletelinefunc          | Direct append "func"               |
| 20    | tabkeyreorg           | tabkeyreorgfunc         | Direct append "func"               |
| 21    | flatcursorkeys        | flatcursorkeysfunc      | Direct append "func"               |
| 22    | getdisplay            | getdisplayfunc          | Direct append "func"               |
| 23    | setdisplay            | setdisplayfunc          | Direct append "func"               |
| 24    | getcursor             | getcursorfunc           | Direct append "func"               |
| 25    | setcursor             | setcursorfunc           | Direct append "func"               |
| 26    | getrefcon             | getrefconfunc           | Direct append "func"               |
| 27    | setrefcon             | setrefconfunc           | Direct append "func"               |
| 28    | getexpansionstate     | getexpansionstatefunc   | Direct append "func"               |
| 29    | setexpansionstate     | setexpansionstatefunc   | Direct append "func"               |
| 30    | getscrollstate        | getscrollstatefunc      | Direct append "func"               |
| 31    | setscrollstate        | setscrollstatefunc      | Direct append "func"               |
| 32    | getsuboutline         | getsuboutlinefunc       | Direct append "func"               |
| 33    | insertoutline         | insertoutlinefunc       | Direct append "func"               |
| 34    | setmodified           | setmodifiedfunc         | Direct append "func"               |
| 35    | getselection          | getselectfunc           | **NO PATTERN** - manual mapping    |
| 36    | getheadnumber         | getheadnumberfunc       | Direct append "func"               |
| 37    | visitall              | visitallfunc            | Direct append "func"               |
| 38    | getselectedsuboutlines| getselectedsuboutlinesfunc | Direct append "func"             |
| 39    | xmltooutline          | xmltooutlinefunc        | Direct append "func"               |
| 40    | outlinetoxml          | outlinetoxmlfunc        | Direct append "func"               |
| 41    | sethtmlformatting     | sethtmlformattingfunc   | Direct append "func"               |
| 42    | gethtmlformatting     | gethtmlformattingfunc   | Direct append "func"               |
| 43    | setdynamic            | setdynamicfunc          | Direct append "func"               |
| 44    | getdynamic            | getdynamicfunc          | Direct append "func"               |

## Problem Cases

### Case 1: `subsexpanded` → `getexpandedfunc`
- RC verb: `subsexpanded`
- Expected enum: `subsexpandedfunc` (if following pattern)
- Actual enum: `getexpandedfunc`
- **Issue**: No systematic transformation possible

### Case 2: `getselection` → `getselectfunc`
- RC verb: `getselection`
- Expected enum: `getselectionfunc` (if following pattern)
- Actual enum: `getselectfunc`
- **Issue**: Truncated "selection" → "select"

### Case 3: `getlinetext` → `linetextfunc`
- RC verb: `getlinetext`
- Expected enum: `getlinetextfunc` (if following pattern)
- Actual enum: `linetextfunc`
- **Issue**: "get" prefix stripped

## Pattern Summary

1. **Dominant pattern (42/45 verbs)**: Direct append "func" suffix
2. **Inconsistent patterns (3/45 verbs)**:
   - Strip "get" prefix: `getlinetext` → `linetextfunc`
   - Truncate word: `getselection` → `getselectfunc`
   - Completely different: `subsexpanded` → `getexpandedfunc`

## Recommendation

**Pattern C requires a manual mapping table** for edge cases. Suggested approach:

1. **Primary heuristic**: Append "func" to RC verb name
2. **Fallback table**: Manual overrides for exceptions:
   ```python
   OP_VERB_EXCEPTIONS = {
       'getlinetext': 'linetextfunc',
       'subsexpanded': 'getexpandedfunc',
       'getselection': 'getselectfunc',
   }
   ```

## Files Involved

- RC file: `/Users/jake/dev/jsavin/Frontier/Common/resources/Win32/kernelverbs.rc` (lines 38-85)
- C enum: `/Users/jake/dev/jsavin/Frontier/Common/source/opverbs.c` (lines 89-235, `tyoptoken`)
- Dispatch: `/Users/jake/dev/jsavin/Frontier/Common/source/opverbs.c` (`opfunctionvalue` function)
