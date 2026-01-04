# UserTalk Verb Implementation Guide

Complete guide for implementing kernel verbs in C for the Frontier headless runtime.

---

## Table of Contents

1. [Basic Verb Implementation Pattern](#basic-verb-implementation-pattern)
2. [Setting UserTalk Variables](#setting-usertalk-variables-from-kernel-verbs)
3. [Value Record Creation](#value-record-creation-functions)
4. [ODB Address Parameters](#extracting-odb-address-parameters)
5. [Examples to Study](#examples-to-study)
6. [Testing](#testing)

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
./frontier-cli/frontier-cli --system-root databases/Frontier-v7.root -e "yourverb(args)"
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
