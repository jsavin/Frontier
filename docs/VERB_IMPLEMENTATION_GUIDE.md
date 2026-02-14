# UserTalk Verb Implementation Guide

Complete guide for implementing kernel verbs in C for the Frontier headless runtime.

---

## Table of Contents

1. [Basic Verb Implementation Pattern](#basic-verb-implementation-pattern)
2. [Understanding typeof() in Verbs](#understanding-typeof-in-verbs---critical-)
3. [Handling Infinity in Numeric Parameters](#handling-infinity-in-numeric-parameters)
4. [Setting UserTalk Variables](#setting-usertalk-variables-from-kernel-verbs)
5. [Value Record Creation](#value-record-creation-functions)
6. [ODB Address Parameters](#extracting-odb-address-parameters)
7. [Examples to Study](#examples-to-study)
8. [Testing](#testing)

---

## Basic Verb Implementation Pattern

When implementing new kernel verbs in C:

1. **Add case statement** in appropriate verb function (e.g., `sysverbfunc` in `shellsysverbs.c`)
2. **Extract parameters**: Use `getstringvalue(hparam1, N, varname)` to get parameter values
3. **String conversions**:
   - Pascal → C: `nullterminate(varname)`
   - C → Pascal: `copyctopstring(cstr, result)`
4. **Return values**: Use `setstringvalue(result, v)` or `setlongvalue()` to return values
5. **Mark last parameter**: Set `flnextparamislast = true` before the last parameter
6. **Test**: Run `./tools/run_headless_tests.sh` to verify no regressions

---

## Understanding typeof() in Verbs - CRITICAL ⚠️⚠️⚠️

**ABSOLUTE RULE**: `typeof()` returns OSType codes (4-byte constants like `'TEXT'`, `'tabl'`, `'long'`), NEVER string names.

When implementing verbs that need to check value types or return type information:

### CORRECT - Use OSType Codes

```c
// ✅ Correct: Compare against OSType code
if (v.valuetype == stringvaluetype) {
    // v is a string
}

// ✅ Correct: typeof() returns OSType codes to UserTalk
// When UserTalk code does:  if typeof(x) == stringType then
// It's comparing the OSType code 'TEXT' against system.compiler.language.constants.stringType
```

### WRONG - Don't Try to Return String Names

```c
// ❌ WRONG: Never change typeof() to return string names
setconstvalue("string", vreturned);      // NEVER do this
setstringvalue("filespec", vreturned);   // NEVER do this

// ❌ This would break all UserTalk code:
// if typeof(x) == "string" then...      // Would always be false!
```

### Why This Matters

- All existing UserTalk code uses OSType code comparisons
- Changing `typeof()` behavior would break production code
- Type constants in `system.compiler.language.constants` are OSType codes
- The entire UserTalk type system depends on this contract

**See `docs/USERTALK_SYNTAX_REFERENCE.md` for complete typeof() documentation.**

---

## Handling Infinity in Numeric Parameters

UserTalk has a special `infinity` constant that represents unlimited/maximum values. When implementing verbs that accept numeric parameters where infinity has special meaning (e.g., level counts, iteration limits), you must handle the conversion between UserTalk's 64-bit infinity and C's type-specific limits.

### The Pattern

UserTalk's `infinity` constant is defined as `LONG_MAX` (typically `0x7FFFFFFFFFFFFFFF` on 64-bit systems). However, many C functions expect smaller integer types (e.g., `short` for 16-bit values). Here's the correct pattern:

```c
// ✅ Correct: Accept long, clamp to short range
long levellong;
short level;

flnextparamislast = true;
if (!getlongvalue(hparam1, 1, &levellong))
    return false;

// Clamp to short range (opcountsubheads expects short)
if (levellong > 32767)
    level = 32767;  // Max short value - effectively infinity for C
else if (levellong < -32768)
    level = -32768;
else
    level = (short)levellong;

// Now use 'level' with C function expecting short
long ct = opcountsubheads(hbarcursor, level);
```

### Why This Matters

- **UserTalk infinity** = `LONG_MAX` (64-bit: `9223372036854775807`)
- **C short max** = `32767` (16-bit maximum)
- **Pattern**: When UserTalk passes `infinity`, clamp to the C type's maximum value

### Real-World Example: op.countSubs

```c
// From tests/headless_op_verbs.c - op.countSubs implementation
case opv_countsubs: {
    long levellong;
    short level;

    flnextparamislast = true;
    if (!getlongvalue(hparam1, 1, &levellong))
        return false;

    // Clamp UserTalk infinity to C short infinity
    if (levellong > 32767)
        level = 32767;  // Effectively infinity for 16-bit operations
    else if (levellong < -32768)
        level = -32768;
    else
        level = (short)levellong;

    // ... use level with opcountsubheads() which expects short
}
```

### Common Mistake

```c
// ❌ WRONG: Using getintvalue() directly truncates infinity
short level;
if (!getintvalue(hparam1, 1, &level))  // Only reads 16 bits!
    return false;

// Result: UserTalk's infinity (0x7FFFFFFFFFFFFFFF)
//         truncates to -1 or garbage, not 32767
```

### When to Use This Pattern

Use this clamping pattern when:
1. UserTalk code can pass `infinity` as a parameter
2. Your C function expects a smaller integer type (`short`, `int`)
3. The semantic meaning is "as many as possible" or "unlimited"

**Reference**: See PR #269 for the op.countSubs fix that implements this pattern.

---

## Setting UserTalk Variables from Kernel Verbs ⚠️

**CRITICAL**: When a kernel verb needs to set a UserTalk variable (like `sys.unixshellcommand(cmd, @stdout)` where `@stdout` is an ODB address parameter), you MUST use the complete value record pattern.

### The Correct Pattern

```c
// Example: Setting a string variable in the ODB
boolean set_string_variable(hdlhashtable htable, bigstring varname, Handle hstring) {
    tyvaluerecord val;

    // Step 1: Create a complete value record from the handle
    if (!setheapvalue(hstring, stringvaluetype, &val))
        return (false);

    // Step 2: Assign it to the ODB location
    if (!hashtableassign(htable, varname, val))
        return (false);

    return (true);
}
```

### Common Mistakes ❌

**DON'T pass handles directly:**
```c
// ❌ WRONG - langsetvalue doesn't exist
langsetvalue(htable, varname, hstdout, stringvaluetype);

// ❌ WRONG - missing value record wrapper
hashtableassign(htable, varname, hstring);  // hstring is Handle, not tyvaluerecord
```

**DO create value records first:**
```c
// ✅ CORRECT
tyvaluerecord val;
setheapvalue(hstring, stringvaluetype, &val);
hashtableassign(htable, varname, val);
```

### Key Insight: Complete Value Records

The core `hashassign()` function (in `Common/source/langhash.c:2070`) takes a **complete `tyvaluerecord`**, not just a handle or raw value.

For a string value, the value record contains:
- `val.valuetype = stringvaluetype`
- `val.data.stringvalue = handle` (the actual string data)
- `val.fltmpdata` - flag indicating data ownership
- `val.fltmpstack` - flag for temp stack management

The `setXXXvalue()` functions handle all this correctly - always use them.

---

## Value Record Creation Functions

| Type | Creation Function | Data Field |
|------|------------------|------------|
| String | `setheapvalue(handle, stringvaluetype, &val)` | `val.data.stringvalue` |
| Long | `setlongvalue(long, &val)` | `val.data.longvalue` |
| Boolean | `setbooleanvalue(bool, &val)` | `val.data.flvalue` |
| Double | `setdoublevalue(double, &val)` | `val.data.doublevalue` |
| Binary | `setbinaryvalue(handle, type, &val)` | `val.data.binaryvalue` |
| Address | `setaddressvalue(htable, name, &val)` | `val.data.addressvalue` |

---

## Extracting ODB Address Parameters

When a verb receives an ODB address parameter (e.g., `@stdout`), you need to:

1. Extract the hash table reference (`hdlhashtable`)
2. Extract the variable name (`bigstring`)
3. Use `hashtableassign(htable, varname, val)` to set the value

**Example:**
```c
hdlhashtable htable;
bigstring varname;
tyvaluerecord val;

// Extract address parameter
if (!getvarvalue(hparam1, 1, &htable, varname, &val, nil))
    return (false);

// Create value record
Handle hresult;
if (!newtexthandle("output data", &hresult))
    return (false);

if (!setheapvalue(hresult, stringvaluetype, &val))
    return (false);

// Assign to ODB location
if (!hashtableassign(htable, varname, val))
    return (false);
```

See `docs/usertalk_variable_assignment.md` for the complete assignment chain and detailed examples.

---

## Examples to Study

Look at these working verb implementations that use ODB address parameters:

- **`Common/source/rgbverbs.c`** - `rgb.get` verb (returns RGB components via address parameters)
- **`Common/source/dateverbs.c`** - `date.get` verb (returns date components via address parameters)
- **`Common/source/langregexp.c`** - `re.getPatternInfo` verb (returns pattern info via address parameter)

---

## Adding a New Verb Processor

When adding a completely new verb processor (e.g., `wp`, `menu`, `outline`):

1. **Create the headless verb file**: `tests/headless_{processor}_verbs.c`
   - Follow the dispatcher pattern in `docs/DISPATCHER_PATTERN.md`
   - Implement `{processor}initverbs()` function
   - Register verb keywords via `langaddkeyword()`

2. **Add to the build**: Add the file to `tests/headless_verbs.mk`

That's it. The build system auto-derives the processor name from the filename and generates `generated/kernel_verbs_init.c` for both test and production builds. If you forget step 2, you'll get a build error (not a silent runtime failure).

See ADR-015 for the architectural rationale behind this two-step workflow.

---

## Testing

### Run Headless Test Suite

```bash
./tools/run_headless_tests.sh
```

This rebuilds the CLI, migrates the test database, and runs the complete test suite.

### Test Individual Verbs

```bash
# Test a verb with no database
FRONTIER_HEADLESS_RUN_STARTUP=1 ./frontier-cli/frontier-cli -e "yourverb(args)"

# Test a verb with system root loaded
./frontier-cli/frontier-cli --system-root databases/Frontier.root7 -e "yourverb(args)"
```

### Common Test Patterns

```bash
# Test parameter extraction
./frontier-cli/frontier-cli -e "yourverb(\"test\", 42, true)"

# Test ODB address parameter
./frontier-cli/frontier-cli -e "lang.new(tableType, @t); yourverb(@t); return defined(t)"

# Test multi-line logic
./frontier-cli/frontier-cli -e $'local(x = 5);\nyourverb(x);\nreturn x'
```

---

## Related Documentation

- `docs/usertalk_variable_assignment.md` - Complete variable assignment patterns
- `docs/TESTING_GUIDE.md` - CLI usage and testing strategies
- `CLAUDE.md` - Quick reference and gotchas
