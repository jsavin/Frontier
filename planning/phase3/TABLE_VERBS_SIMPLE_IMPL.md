# Table Verbs: Simple Implementation Guide

**Status:** Phase 3 Implementation Ready
**Date:** 2025-12-28
**Category:** Simple Table Navigation & Selection Verbs
**Verbs Covered:** sortBy, getCursor, gotoName, getDisplaySettings, setDisplaySettings, select, clearSelection, expand, collapse

---

## Overview

This document covers implementation of simpler table verbs that don't require complex expansion state traversal. These verbs are either metadata operations, direct lookups, or simple state management.

---

## 1. table.sortBy()

### Verb Signature

```usertalk
table.sortBy(column1, column2, ...) → boolean
```

**Parameters:**
- Variable number of column names (bigstrings)

**Returns:** true if sort order set

**Description:**
Sets sort order metadata for table. In headless mode, this is metadata-only (no visual re-sorting). Actual sorting happens when table is displayed in window.

### Legacy Windowed Behavior

In classic Frontier:
- Sets table's sort order (stored in `(**htable).sortorder`)
- Re-sorts table window display
- Multiple columns supported (sort by column1, then column2, etc.)

### Headless Behavior

In headless mode:
- Store sort order in table structure (same as windowed)
- No visual re-sorting (no window)
- Return true (metadata updated)

### Implementation

```c
/**
 * table_sortby_headless - Set sort order metadata
 *
 * @param htable - Table to set sort order on
 * @param columns - List of column names
 * @return true (always succeeds in headless)
 */
boolean table_sortby_headless(hdlhashtable htable, hdllistrecord columns) {
    // In headless mode, we just store the sort order metadata
    // Actual visual sorting not needed (no window)

    // Note: Legacy code stores sort order as short value
    // For multi-column sorting, would need to extend structure
    // For Phase 3, just return true (no-op or store first column)

    // Option 1: No-op (return true)
    return true;

    // Option 2: Store first column (if we extend table structure)
    // bigstring first_column;
    // if (listgetnthitem(columns, 1, &first_column)) {
    //     // Store in table metadata (requires structure extension)
    //     (**htable).sortorder = hash_column_name(first_column);
    // }
    // return true;
}
```

**Verb Binding:**
```c
case sortbyfunc:
    if (flheadless) {
        // In headless mode, just return true (metadata-only)
        (*v).data.flvalue = true;
    } else {
        (*v).data.flvalue = tablesortbyverb(hparam1, v);
    }
    return (true);
```

### Test Case

```usertalk
// Setup
local (t)
lang.new(tableType, @t)
t.z = 1; t.a = 2; t.m = 3

// Test
local (result = table.sortBy(@t, "name"))

// Verify
assert(result == true, "Should succeed")
// Note: Visual order not observable in headless mode
```

---

## 2. table.getCursor()

### Verb Signature

```usertalk
table.getCursor() → address
```

**Parameters:** None

**Returns:** Address of current cursor position, or nil if no cursor

**Description:**
Returns address of entry at current cursor position.

### Implementation

```c
/**
 * table_getcursor_headless - Get current cursor position
 *
 * @param v - Output: address value of cursor position
 * @return true if cursor set, false if no cursor
 */
boolean table_getcursor_headless(tyvaluerecord *v) {
    table_selection_context_t *ctx = table_selection_acquire();
    boolean fl = false;

    if (ctx == NULL) {
        return langerror(BIGSTRING("\x20" "Can't get selection context"));
    }

    // Check if cursor is set
    if (ctx->cursor_key[0] > 0 && ctx->current_table != NULL) {
        // Return cursor as address
        fl = setaddressvalue(ctx->current_table, ctx->cursor_key, v);
    } else {
        // No cursor set, return nil
        fl = setnilvalue(v);
    }

    table_selection_release(ctx);
    return fl;
}
```

**Verb Binding:**
```c
case getcursorfunc:
    if (flheadless) {
        (*v).data.flvalue = table_getcursor_headless(v);
    } else {
        (*v).data.flvalue = tablegetcursorverb(v);
    }
    return (true);
```

### Test Cases

```usertalk
// Test 1: Cursor set
local (t)
lang.new(tableType, @t)
t.a = 1; t.b = 2

table.goto(@t, 2)  // Set cursor to b
local (cursor = table.getCursor())
assert(cursor == @t.b, "Cursor should be at b")

// Test 2: No cursor
local (t2)
lang.new(tableType, @t2)
t2.x = 1
local (cursor2 = table.getCursor())
assert(cursor2 == nil, "No cursor set yet")
```

---

## 3. table.gotoName()

### Verb Signature

```usertalk
table.gotoName(name) → address
```

**Parameters:**
- `name` (string): Entry name to navigate to

**Returns:** Address of entry with that name

**Description:**
Moves cursor to entry with specified name. Simpler than table.goto() because no row counting needed.

### Implementation

```c
/**
 * table_gotoname_headless - Navigate to entry by name
 *
 * @param htable - Table to navigate within
 * @param name - Entry name (bigstring)
 * @param v - Output: address value of entry
 * @return true if found, false if not found
 */
boolean table_gotoname_headless(hdlhashtable htable, bigstring name,
                                tyvaluerecord *v) {
    table_selection_context_t *ctx = NULL;
    hdlhashnode hnode = NULL;

    if (htable == NULL) {
        return langerror(BIGSTRING("\x10" "No table given"));
    }

    // Look up entry by name
    if (!hashlookup(name, &hnode, htable)) {
        return langerror(BIGSTRING("\x0E" "Key not found"));
    }

    // Acquire selection context
    ctx = table_selection_acquire();
    if (ctx == NULL) {
        return langerror(BIGSTRING("\x20" "Can't get selection context"));
    }

    // Update cursor
    copystring(name, ctx->cursor_key);
    ctx->cursor_node = hnode;
    ctx->current_table = htable;

    // Calculate flat index (optional, for later goto operations)
    ctx->cursor_flat_index = table_selection_get_row_for_key(ctx, htable, name);

    // Return address
    boolean fl = setaddressvalue(htable, name, v);

    table_selection_release(ctx);
    return fl;
}
```

**Verb Binding:**
```c
case gotonamefunc:
    {
        bigstring name;
        hdlhashtable htable;

        flnextparamislast = true;

        // Get name parameter
        if (!getstringvalue(hparam1, 1, name)) {
            break;
        }

        // Get target table
        if (!langfindtargethashtable(&htable)) {
            langerror(BIGSTRING("\x18" "No table is current"));
            break;
        }

        if (flheadless) {
            (*v).data.flvalue = table_gotoname_headless(htable, name, v);
        } else {
            (*v).data.flvalue = tablegotonameverb(name, v);
        }

        return (true);
    }
```

### Test Cases

```usertalk
// Test 1: Navigate by name
local (t)
lang.new(tableType, @t)
t.foo = 1; t.bar = 2; t.baz = 3

local (addr = table.gotoName(@t, "bar"))
assert(addr == @t.bar, "Found bar")
assert(table.getCursor() == @t.bar, "Cursor at bar")

// Test 2: Key not found
try
    local (addr2 = table.gotoName(@t, "missing"))
    assert(false, "Should have thrown error")
bundle
    assert(true, "Correctly threw key not found error")
```

---

## 4. table.getDisplaySettings() / table.setDisplaySettings()

### Verb Signatures

```usertalk
table.getDisplaySettings() → record
table.setDisplaySettings(record) → boolean
```

**Description:**
Get/set table window display settings (fonts, column widths, etc.). In headless mode, these are no-ops (no window to configure).

### Implementation

```c
/**
 * table_getdisplaysettings_headless - Get display settings (no-op)
 *
 * @param v - Output: empty record
 * @return true
 */
boolean table_getdisplaysettings_headless(tyvaluerecord *v) {
    // Headless mode: no display settings
    // Return empty record
    hdllistrecord hrec;
    if (!newrecord(&hrec)) {
        return false;
    }
    return setheapvalue((Handle)hrec, recordvaluetype, v);
}

/**
 * table_setdisplaysettings_headless - Set display settings (no-op)
 *
 * @param rec - Display settings record (ignored)
 * @return true
 */
boolean table_setdisplaysettings_headless(tyvaluerecord *rec) {
    // Headless mode: no display settings to set
    // Just return true (no-op)
    return true;
}
```

**Verb Binding:**
```c
case getdisplaysettingsfunc:
    if (flheadless) {
        (*v).data.flvalue = table_getdisplaysettings_headless(v);
    } else {
        (*v).data.flvalue = tablegetdisplaysettingsverb(v);
    }
    return (true);

case setdisplaysettingsfunc:
    if (flheadless) {
        (*v).data.flvalue = table_setdisplaysettings_headless(hparam1);
    } else {
        (*v).data.flvalue = tablesetdisplaysettingsverb(hparam1);
    }
    return (true);
```

### Test Cases

```usertalk
// Test get (returns empty record)
local (settings = table.getDisplaySettings())
assert(typeof(settings) == recordType, "Returns record")
assert(sizeof(settings) == 0, "Empty in headless mode")

// Test set (no-op, returns true)
local (result = table.setDisplaySettings({font: "Monaco"}))
assert(result == true, "Succeeds as no-op")
```

---

## 5. table.select() - NEW VERB

### Verb Signature

```usertalk
table.select(address) → boolean
```

**Parameters:**
- `address` (tyaddressvalue): Address of entry to add to selection

**Returns:** true if added to selection, false if already selected

**Description:**
Adds entry to multi-selection list. This is the headless equivalent of shift-clicking in table window.

### Implementation

```c
/**
 * table_select_headless - Add entry to selection
 *
 * @param htable - Table containing entry
 * @param key - Entry key to select
 * @return true if added, false if already selected
 */
boolean table_select_headless(hdlhashtable htable, bigstring key) {
    table_selection_context_t *ctx = NULL;
    boolean fl = false;

    if (htable == NULL) {
        return langerror(BIGSTRING("\x10" "No table given"));
    }

    // Verify key exists
    hdlhashnode hnode;
    if (!hashlookup(key, &hnode, htable)) {
        return langerror(BIGSTRING("\x0E" "Key not found"));
    }

    // Acquire selection context
    ctx = table_selection_acquire();
    if (ctx == NULL) {
        return langerror(BIGSTRING("\x20" "Can't get selection context"));
    }

    ctx->current_table = htable;

    // Add to selection
    fl = table_selection_add(ctx, key);

    table_selection_release(ctx);
    return fl;
}
```

**Verb Binding (New Entry in tableverbs.c):**
```c
case selectfunc:  // NEW VERB
    {
        bigstring key;
        hdlhashtable htable;

        flnextparamislast = true;

        // Get address parameter and extract key
        tyvaluerecord addrval;
        if (!getparamvalue(hparam1, 1, &addrval)) {
            break;
        }

        if (addrval.valuetype != addressvaluetype) {
            langerror(BIGSTRING("\x1C" "Parameter must be address"));
            break;
        }

        // Extract table and key from address
        if (!getaddressvalue(&addrval, &htable, key)) {
            langerror(BIGSTRING("\x14" "Invalid address"));
            break;
        }

        if (flheadless) {
            (*v).data.flvalue = table_select_headless(htable, key);
        } else {
            // Windowed mode: mark entry (similar to shift-click)
            (*v).data.flvalue = tableselectverb(htable, key);
        }

        return (true);
    }
```

**kernelverbs.rc Entry (Add to table processor):**
```
select      // NEW VERB - Add entry to selection
```

### Test Cases

```usertalk
// Test 1: Select single entry
local (t)
lang.new(tableType, @t)
t.a = 1; t.b = 2; t.c = 3

local (result = table.select(@t.b))
assert(result == true, "Added to selection")

local (sel = table.getSelection())
assert(sizeof(sel) == 1, "One entry selected")
assert(sel[1] == @t.b, "Selected entry is b")

// Test 2: Select multiple entries
table.select(@t.a)
table.select(@t.c)

local (sel2 = table.getSelection())
assert(sizeof(sel2) == 3, "Three entries selected")

// Test 3: Select already-selected entry
local (result2 = table.select(@t.a))
assert(result2 == false, "Already selected (idempotent)")
```

---

## 6. table.clearSelection() - NEW VERB

### Verb Signature

```usertalk
table.clearSelection() → boolean
```

**Parameters:** None

**Returns:** true

**Description:**
Clears all multi-selection entries. Cursor position preserved.

### Implementation

```c
/**
 * table_clearselection_headless - Clear all selections
 *
 * @return true
 */
boolean table_clearselection_headless(void) {
    table_selection_context_t *ctx = table_selection_acquire();

    if (ctx == NULL) {
        return langerror(BIGSTRING("\x20" "Can't get selection context"));
    }

    // Clear selection list
    table_selection_clear(ctx);

    table_selection_release(ctx);
    return true;
}
```

**Verb Binding:**
```c
case clearselectionfunc:  // NEW VERB
    if (!langcheckparamcount(hparam1, 0)) {
        break;
    }

    if (flheadless) {
        (*v).data.flvalue = table_clearselection_headless();
    } else {
        (*v).data.flvalue = tableclearselectionverb();
    }

    return (true);
```

**kernelverbs.rc Entry:**
```
clearSelection  // NEW VERB - Clear multi-selection
```

### Test Cases

```usertalk
// Setup
local (t)
lang.new(tableType, @t)
t.a = 1; t.b = 2; t.c = 3

table.select(@t.a)
table.select(@t.b)

local (sel1 = table.getSelection())
assert(sizeof(sel1) == 2, "Two entries selected")

// Test
table.clearSelection()

local (sel2 = table.getSelection())
assert(sizeof(sel2) == 0, "Selection cleared")
```

---

## 7. table.expand() - NEW VERB

### Verb Signature

```usertalk
table.expand(address) → boolean
```

**Parameters:**
- `address` (tyaddressvalue): Address of nested table to expand

**Returns:** true if expanded, false if already expanded

**Description:**
Marks nested table as expanded (makes children visible in row counting). Required for nested table navigation.

### Implementation

```c
/**
 * table_expand_headless - Mark nested table as expanded
 *
 * @param htable - Nested table to expand
 * @return true if newly expanded, false if already expanded
 */
boolean table_expand_headless(hdlhashtable htable) {
    table_selection_context_t *ctx = NULL;
    boolean fl = false;

    if (htable == NULL) {
        return langerror(BIGSTRING("\x10" "No table given"));
    }

    ctx = table_selection_acquire();
    if (ctx == NULL) {
        return langerror(BIGSTRING("\x20" "Can't get selection context"));
    }

    // Add to expansion list
    fl = table_selection_expand(ctx, htable);

    table_selection_release(ctx);
    return fl;
}
```

**Verb Binding:**
```c
case expandfunc:  // NEW VERB
    {
        hdlhashtable htable;
        bigstring key;

        flnextparamislast = true;

        // Get address parameter
        tyvaluerecord addrval;
        if (!getparamvalue(hparam1, 1, &addrval)) {
            break;
        }

        // Extract table from address
        if (!getaddressvalue(&addrval, &htable, key)) {
            break;
        }

        // Look up the nested table
        hdlhashnode hnode;
        if (!hashlookup(key, &hnode, htable)) {
            langerror(BIGSTRING("\x0E" "Key not found"));
            break;
        }

        // Verify it's a table
        if ((**hnode).val.valuetype != externalvaluetype) {
            langerror(BIGSTRING("\x18" "Not a nested table"));
            break;
        }

        hdlexternalvariable hv = (hdlexternalvariable)((**hnode).val.data.externalvalue);
        if ((**hv).id != idtableprocessor) {
            langerror(BIGSTRING("\x18" "Not a nested table"));
            break;
        }

        hdlhashtable nested = (hdlhashtable)((**hv).variabledata);

        if (flheadless) {
            (*v).data.flvalue = table_expand_headless(nested);
        } else {
            // Windowed mode: expand in UI
            (*v).data.flvalue = tableexpandverb(nested);
        }

        return (true);
    }
```

**kernelverbs.rc Entry:**
```
expand      // NEW VERB - Expand nested table
```

### Test Cases

```usertalk
// Test
local (t)
lang.new(tableType, @t)
lang.new(tableType, @t.nested)
t.nested.x = 1

// Initially collapsed (2 visible rows)
assert(table.countVisibleRows(@t) == 2, "2 rows when collapsed")

// Expand
local (result = table.expand(@t.nested))
assert(result == true, "Newly expanded")

assert(table.countVisibleRows(@t) == 3, "3 rows when expanded")

// Expand again (idempotent)
local (result2 = table.expand(@t.nested))
assert(result2 == false, "Already expanded")
```

---

## 8. table.collapse() - NEW VERB

### Verb Signature

```usertalk
table.collapse(address) → boolean
```

**Parameters:**
- `address` (tyaddressvalue): Address of nested table to collapse

**Returns:** true if collapsed, false if already collapsed

**Description:**
Marks nested table as collapsed (hides children from row counting).

### Implementation

```c
/**
 * table_collapse_headless - Mark nested table as collapsed
 *
 * @param htable - Nested table to collapse
 * @return true if collapsed, false if already collapsed
 */
boolean table_collapse_headless(hdlhashtable htable) {
    table_selection_context_t *ctx = NULL;
    boolean fl = false;

    if (htable == NULL) {
        return langerror(BIGSTRING("\x10" "No table given"));
    }

    ctx = table_selection_acquire();
    if (ctx == NULL) {
        return langerror(BIGSTRING("\x20" "Can't get selection context"));
    }

    // Remove from expansion list
    fl = table_selection_collapse(ctx, htable);

    table_selection_release(ctx);
    return fl;
}
```

**Verb Binding:** (Similar to expand, omitted for brevity)

**kernelverbs.rc Entry:**
```
collapse    // NEW VERB - Collapse nested table
```

### Test Cases

```usertalk
// Test
local (t)
lang.new(tableType, @t)
lang.new(tableType, @t.nested)
t.nested.x = 1

table.expand(@t.nested)
assert(table.countVisibleRows(@t) == 3, "3 rows when expanded")

// Collapse
local (result = table.collapse(@t.nested))
assert(result == true, "Newly collapsed")

assert(table.countVisibleRows(@t) == 2, "2 rows when collapsed")
```

---

## 9. table.countVisibleRows() - NEW HELPER VERB

### Verb Signature

```usertalk
table.countVisibleRows() → long
```

**Parameters:** None

**Returns:** Total number of visible rows (accounting for expansion)

**Description:**
Helper verb for testing and scripting. Counts all visible rows in table.

### Implementation

```c
boolean table_countvisiblerows_headless(hdlhashtable htable, tyvaluerecord *v) {
    table_selection_context_t *ctx = table_selection_acquire();
    long count = 0;

    if (ctx == NULL) {
        return langerror(BIGSTRING("\x20" "Can't get selection context"));
    }

    count = table_selection_count_visible_rows(ctx, htable);

    table_selection_release(ctx);
    return setlongvalue(count, v);
}
```

---

## Summary of New Verbs

| Verb | Purpose | Phase |
|------|---------|-------|
| `table.select(addr)` | Add to multi-selection | Phase 3 |
| `table.clearSelection()` | Clear multi-selection | Phase 3 |
| `table.expand(addr)` | Expand nested table | Phase 3 |
| `table.collapse(addr)` | Collapse nested table | Phase 3 |
| `table.countVisibleRows()` | Count visible rows | Phase 3 (testing helper) |

---

## Integration Checklist

- [ ] Implement all functions in `Common/source/headless_selection.c`
- [ ] Add prototypes to `Common/headers/headless_selection.h`
- [ ] Update `tableverbs.c` with all verb bindings
- [ ] Add new verbs to `kernelverbs.rc` (select, clearSelection, expand, collapse)
- [ ] Add unit tests for each verb
- [ ] Add integration tests (UserTalk)
- [ ] Verify via `./tools/run_headless_tests.sh`

---

## Error Messages

```c
BIGSTRING("\x10" "No table given")
BIGSTRING("\x20" "Can't get selection context")
BIGSTRING("\x0E" "Key not found")
BIGSTRING("\x18" "No table is current")
BIGSTRING("\x1C" "Parameter must be address")
BIGSTRING("\x14" "Invalid address")
BIGSTRING("\x18" "Not a nested table")
```
