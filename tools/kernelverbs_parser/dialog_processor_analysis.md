# Pattern D Analysis: dialog Processor - Multi-Processor Consolidation

## Overview

The `dialog` processor demonstrates a **different architectural pattern** where:
1. **RC file lists it as a separate processor** with 19 verbs
2. **No standalone C implementation file** (`langdialog.c` contains helper functions only)
3. **Verbs are consolidated into `langverbs.c`** along with other small processors

This is Pattern D: **Multi-Processor Consolidation Pattern**

## Architecture

### RC File Declaration
```
"dialog\0",
    true,           // window required
    19,             // verb count
    "alert\0",
    "run\0",
    "runmodeless\0",
    ...
```

### C Implementation Structure

#### Files Involved:

1. **`Common/source/langdialog.c`** - Helper functions (NOT verb dispatch):
   - `langgetdialogvalue()` - Implementation helper
   - `langsetdialogvalue()` - Implementation helper
   - `langsetdialogitemenable()` - Implementation helper
   - `langrundialog()` - Implementation helper
   - No enum, no verb dispatch function

2. **`Common/source/langverbs.c`** - Actual verb dispatch:
   - Contains `tylangtoken` enum with dialog verbs
   - Contains dispatch switch statement calling helpers from `langdialog.c`

### Complete Verb Mapping

| Index | RC Verb Name      | langverbs.c Enum Token    | Helper Function          |
|-------|-------------------|---------------------------|--------------------------|
| 0     | alert             | alertdialogfunc           | alertdialog()            |
| 1     | run               | rundialogfunc             | langrundialog()          |
| 2     | runmodeless       | runmodelessfunc           | langrunmodeless()        |
| 3     | runcard           | runcardfunc               | (inline)                 |
| 4     | runmodalcard      | runmodalcardfunc          | (inline)                 |
| 5     | ismodalcard       | ismodalcardfunc           | (inline)                 |
| 6     | setmodalcardtimeout| setmodalcardtimeoutfunc  | (inline)                 |
| 7     | getvalue          | getdialogvaluefunc        | langgetdialogvalue()     |
| 8     | setvalue          | setdialogvaluefunc        | langsetdialogvalue()     |
| 9     | setitemenable     | setdialogitemenablefunc   | langsetdialogitemenable()|
| 10    | showitem          | showdialogitemfunc        | (inline)                 |
| 11    | hideitem          | hidedialogitemfunc        | (inline)                 |
| 12    | twoway            | twowaydialogfunc          | (inline)                 |
| 13    | threeway          | threewaydialogfunc        | (inline)                 |
| 14    | ask               | askdialogfunc             | (inline)                 |
| 15    | getint            | getintdialogfunc          | (inline)                 |
| 16    | notify            | notifytdialogfunc         | (inline)                 |
| 17    | getuserinfo       | getuserinfodialogfunc     | (inline)                 |
| 18    | getpassword       | askpassworddialogfunc     | (inline)                 |

## Transformation Patterns

### Pattern 1: Suffix + "dialog" infix (most common)
```
RC: "run"      → Enum: rundialogfunc
RC: "getvalue" → Enum: getdialogvaluefunc
RC: "showitem" → Enum: showdialogitemfunc
```

### Pattern 2: Prefix "dialog" (rare)
```
RC: "alert" → Enum: alertdialogfunc
RC: "twoway" → Enum: twowaydialogfunc
```

### Pattern 3: Complete mismatch (1 case)
```
RC: "notify" → Enum: notifytdialogfunc  (should be notifydialogfunc)
RC: "getpassword" → Enum: askpassworddialogfunc  (completely different)
```

## Other Processors Using This Pattern

### From `langverbs.c` enum analysis:

The following processors are also consolidated into `langverbs.c`:

1. **clock processor** (7 verbs):
   - timefunc, settimefunc, sleepfunc, tickcountfunc, millisecondcountfunc, delayfunc, delaysixtiethsfunc

2. **date processor** (20 verbs):
   - getdatefunc, setdatefunc, abbrevstringfunc, dayofweekfunc, daysinmonthfunc, etc.

3. **kb processor** (4 verbs):
   - optionkeyfunc, cmdkeyfunc, shiftkeyfunc, controlkeyfunc

4. **mouse processor** (2 verbs):
   - mousebuttonfunc, mouselocationfunc

5. **point processor** (2 verbs):
   - getpointfunc, setpointfunc

6. **rectangle processor** (2 verbs):
   - getrectfunc, setrectfunc

7. **rgb processor** (2 verbs):
   - getrgbfunc, setrgbfunc

8. **speaker processor** (3 verbs):
   - sysbeepfunc, soundfunc, playsoundfunc

9. **target processor** (3 verbs):
   - gettargetfunc, settargetfunc, cleartargetfunc

## Detection Heuristic

To detect Pattern D processors:

```python
def is_pattern_d_processor(processor_name):
    """
    Check if processor uses multi-processor consolidation pattern.

    Pattern D processors:
    - Listed in kernelverbs.rc as separate processors
    - No standalone {processor}verbs.c file
    - Implemented in langverbs.c
    """
    # Check if processor is small (< 20 verbs is a hint)
    # Check if no {processor}verbs.c exists
    # Check if verbs appear in langverbs.c enum

    PATTERN_D_PROCESSORS = {
        'dialog', 'clock', 'date', 'kb', 'mouse',
        'point', 'rectangle', 'rgb', 'speaker', 'target'
    }

    return processor_name in PATTERN_D_PROCESSORS
```

## Headless Implementation Strategy

For headless stubs, Pattern D processors should:

1. **Create standalone stub file**: `tests/headless_{processor}_verbs.c`
2. **Use standard enum pattern**: Create local enum with `{proc}v_{verb}` naming
3. **Follow stub template**: Match the structure of other headless stubs

Example from `tests/headless_dialog_verbs.c`:
```c
enum {
    diav_alert = 0,
    diav_run = 1,
    diav_runmodeless = 2,
    ...
};
```

This is **cleaner than the langverbs.c approach** because it:
- Keeps each processor isolated
- Makes verb indices explicit
- Avoids naming collisions
- Simplifies testing

## Recommendation

**Pattern D is not a bug - it's intentional architectural consolidation** for small processors.

For the automatic binding analyzer:

1. **Detect Pattern D**: Check if processor appears in RC but no `{name}verbs.c` exists
2. **Search langverbs.c**: Look for enum tokens in `tylangtoken` enum
3. **Use manual mapping table**: Some transformations are unpredictable (e.g., "getpassword" → "askpassworddialogfunc")

## Files Involved

- RC file: `/Users/jake/dev/jsavin/Frontier/Common/resources/Win32/kernelverbs.rc` (lines 348-369)
- Enum definition: `/Users/jake/dev/jsavin/Frontier/Common/source/langverbs.c` (lines 283-323, `tylangtoken`)
- Dispatch: `/Users/jake/dev/jsavin/Frontier/Common/source/langverbs.c` (switch statement)
- Helper functions: `/Users/jake/dev/jsavin/Frontier/Common/source/langdialog.c`
- Headless stub: `/Users/jake/dev/jsavin/Frontier/tests/headless_dialog_verbs.c`
