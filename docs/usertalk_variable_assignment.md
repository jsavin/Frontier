# UserTalk Variable Assignment - Implementation Guide

**For implementing kernel verbs that need to set UserTalk variables**

## Overview

When you implement a kernel verb in C that needs to set a UserTalk variable (like `sys.unixshellcommand(cmd, @stdout)` where `@stdout` is an ODB address parameter), you need to understand how UserTalk's assignment operator works internally.

## The Assignment Chain

When UserTalk executes `scratchpad.foo = "some text"`, here's what happens:

```
assignvalue(hlhs, vrhs)                      [langvalue.c:5971]
  ↓
assignordeletevalue(hlhs, &vrhs, assignop)   [langvalue.c:5841]
  ↓
langsetsymbolval(bsname, *vassign)           [langops.c:528]
  ↓
hashtableassign(htable, bs, val)             [langhash.c:2123]
  ↓
hashassign(bs, val)                          [langhash.c:2070]
```

## Core Implementation: hashassign()

**Location:** `Common/source/langhash.c:2070-2119`

```c
boolean hashassign (const bigstring bs, tyvaluerecord val) {
    boolean fllocal = (**currenthashtable).fllocaltable;

    // Handle temporary data ownership
    if (val.fltmpdata) {  // val doesn't own its data
        if (val.fltmpstack)
            val.fltmpdata = false;
        else
            if (!copyvaluedata(&val))
                return (false);
    }

    val.fltmpstack = false;

    // Set locality to match parent table
    hashsetlocality(&val, fllocal);

    // Find or create the hash node
    if (!hashlocate(bs, &hnode, &hprev)) {
        return (hashinsert(bs, val));  // Variable doesn't exist, create it
    }

    // Variable exists, dispose old value and assign new
    existingval = (**hnode).val;

    // Protect externals from being overwritten
    if (fllanghashassignprotect) {
        if ((existingval.valuetype == externalvaluetype) &&
            (val.valuetype != externalvaluetype)) {
            // Error: can't assign non-external to external
            return (false);
        }
    }

    disposevaluerecord(existingval, !fllocal);
    (**hnode).val = val;  // Store complete value record

    langsymbolchanged(currenthashtable, bs, hnode, true);

    return (true);
}
```

## Key Insight: Complete Value Records

**Critical:** `hashassign()` takes a **complete `tyvaluerecord`**, not just a handle.

For a string value, the value record contains:
- `val.valuetype = stringvaluetype`
- `val.data.stringvalue = handle` (the actual string data)
- `val.fltmpdata` - flag indicating data ownership
- `val.fltmpstack` - flag for temp stack management

## How Kernel Verbs Should Set Variables

### Pattern for String Values

```c
// Example: Setting a string variable in the ODB
boolean set_string_variable(hdlhashtable htable, bigstring varname, Handle hstring) {
    tyvaluerecord val;

    // Create a value record from the handle
    if (!setheapvalue(hstring, stringvaluetype, &val))
        return (false);

    // Assign it to the ODB location
    if (!hashtableassign(htable, varname, val))
        return (false);

    return (true);
}
```

### Pattern for Integer Values

```c
// Example: Setting an integer variable in the ODB
boolean set_long_variable(hdlhashtable htable, bigstring varname, long value) {
    tyvaluerecord val;

    // Create a value record from the long
    if (!setlongvalue(value, &val))
        return (false);

    // Assign it to the ODB location
    if (!hashtableassign(htable, varname, val))
        return (false);

    return (true);
}
```

### Pattern Used by Working Verbs

Look at `rgb.get`, `date.get`, or other verbs that take ODB address parameters:

1. **Extract the ODB address parameter** - Get the hash table and variable name from the parameter
2. **Create the value record** - Use `setlongvalue()`, `setheapvalue()`, etc.
3. **Assign to the ODB** - Use `hashtableassign(htable, varname, val)`

## Common Mistakes

### ❌ Wrong: Passing handles directly
```c
// DON'T DO THIS - langsetvalue doesn't exist or expects different params
langsetvalue(htable, varname, hstdout, stringvaluetype);
```

### ❌ Wrong: Not creating a value record
```c
// DON'T DO THIS - missing the value record wrapper
hashtableassign(htable, varname, hstring);  // hstring is a Handle, not tyvaluerecord
```

### ✅ Correct: Create value record, then assign
```c
tyvaluerecord val;
setheapvalue(hstring, stringvaluetype, &val);
hashtableassign(htable, varname, val);
```

## Parameter Extraction

To get the ODB address from a verb parameter (e.g., `@stdout` in UserTalk):

```c
// Look for how other verbs extract ODB address parameters
// Typically involves:
// 1. Getting the parameter tree node
// 2. Extracting htable and varname from the parameter
// 3. Using hashtableassign to set the value at that location
```

**See also:**
- `Common/source/rgbverbs.c` - Examples of verbs using ODB address parameters
- `Common/source/dateverbs.c` - More examples of multi-parameter ODB assignments
- `Common/source/langregexp.c` - `re.getPatternInfo` uses ODB address parameter

## Value Record Types

For reference, here are the common value types and their creation functions:

| Type | Creation Function | Data Field |
|------|------------------|------------|
| String | `setheapvalue(handle, stringvaluetype, &val)` | `val.data.stringvalue` |
| Long | `setlongvalue(long, &val)` | `val.data.longvalue` |
| Boolean | `setbooleanvalue(bool, &val)` | `val.data.flvalue` |
| Double | `setdoublevalue(double, &val)` | `val.data.doublevalue` |
| Binary | `setbinaryvalue(handle, type, &val)` | `val.data.binaryvalue` |
| Address | `setaddressvalue(htable, name, &val)` | `val.data.addressvalue` |

## Temp Data Management

The `fltmpdata` flag is critical for memory management:

- **`fltmpdata = false`**: Value owns its data (handle, allocated memory, etc.)
- **`fltmpdata = true`**: Value is temporary, data will be copied during assignment

When creating a value record for assignment:
- Use the `setXXXvalue()` functions - they handle `fltmpdata` correctly
- `hashassign()` will copy data if needed
- Don't manually manage `fltmpdata` unless you know what you're doing

## Related Documentation

- `docs/external_table_variable_management.md` - External table variables
- `CLAUDE.md` - Project development guidelines (see "Implementing Kernel Verbs" section)
- Source files:
  - `Common/source/langhash.c` - Hash table operations
  - `Common/source/langvalue.c` - Value record operations
  - `Common/source/langops.c` - Symbol lookup and assignment
