# Table Verb: table.getSelection() Implementation

**Status:** Phase 3 Implementation Ready
**Date:** 2025-12-28
**Verb:** `table.getSelection()`
**Category:** Navigation/Selection

---

## Verb Signature

```usertalk
table.getSelection() → list
```

**Parameters:** None

**Returns:** List of addresses pointing to selected table entries

**Description:**
Returns a list of addresses for all entries currently marked as selected in the table. If no entries are explicitly selected, returns a single-item list containing the current cursor position. If no cursor is set, returns first entry in sorted order.

---

## Legacy Windowed Behavior

In classic Frontier with table windows:
1. User shift-clicks entries to mark them (sets selection)
2. `table.getSelection()` returns list of addresses for marked entries
3. If no entries marked, returns cursor position
4. Used for batch operations (delete selected, copy selected, etc.)

**Example:**
```usertalk
// User shift-clicked rows 3, 5, 7 in table window
local (sel = table.getSelection())
// sel = [@workspace.item3, @workspace.item5, @workspace.item7]

// Iterate and process selected
local (i)
for i = 1 to sizeof(sel)
    delete (sel[i])
```

---

## Headless Behavior

In headless mode (no table window):
1. Selection tracked in thread-local `table_selection_context_t`
2. Users call `table.select(key)` to mark entries
3. `table.getSelection()` returns list of marked entries
4. If no marks, returns cursor position
5. If no cursor, returns first entry in sorted order

**Implementation depends on:**
- Thread-local selection context (see TABLE_VERBS_HEADLESS_SELECTION_MODEL.md)
- Current table tracking
- Multi-selection list management

---

## Implementation

### Function Signature (C)

```c
/**
 * table_getselection_headless - Get list of selected table entries
 *
 * @param v - Output: value record to store result list
 * @return true if successful, false on error
 *
 * Returns list of addresses (tyaddressvalue) for selected entries.
 * If no selection, returns cursor position.
 * If no cursor, returns first entry in sorted order.
 */
boolean table_getselection_headless(tyvaluerecord *v);
```

### Complete Implementation

```c
boolean table_getselection_headless(tyvaluerecord *v) {
    table_selection_context_t *ctx = NULL;
    hdllistrecord hlist = NULL;
    hdlhashtable htable = NULL;
    boolean fl = false;

    // Acquire selection context
    ctx = table_selection_acquire();
    if (ctx == NULL) {
        return langerror(BIGSTRING("\x20" "Can't get selection context"));
    }

    // Get current table from context
    htable = ctx->current_table;
    if (htable == NULL) {
        // No table context, try to get from target
        if (!langfindtargethashtable(&htable)) {
            table_selection_release(ctx);
            return langerror(BIGSTRING("\x18" "No table is current"));
        }
        // Update context with current table
        ctx->current_table = htable;
    }

    // Create result list
    if (!newlist(&hlist, false)) {
        table_selection_release(ctx);
        return false;
    }

    //
    // CASE 1: Multi-selection exists (entries explicitly marked)
    //
    if (ctx->ct_selected > 0) {
        // Return all selected entries
        for (long i = 0; i < ctx->ct_selected; i++) {
            bigstring key;
            tyvaluerecord keyval;

            // Get key from selection list
            if (!listgetnthitem(ctx->selected_keys, i, &keyval)) {
                opdisposelist(hlist);
                table_selection_release(ctx);
                return false;
            }

            // Extract key string
            if (!getstringvalue(&keyval, 1, key)) {
                opdisposelist(hlist);
                table_selection_release(ctx);
                return false;
            }

            // Add address to result list
            tyvaluerecord addrval;
            if (!setaddressvalue(htable, key, &addrval)) {
                opdisposelist(hlist);
                table_selection_release(ctx);
                return false;
            }

            if (!langpushlistval(hlist, nil, &addrval)) {
                opdisposelist(hlist);
                table_selection_release(ctx);
                return false;
            }
        }

        fl = true;
        goto done;
    }

    //
    // CASE 2: Cursor is set (single position)
    //
    if (ctx->cursor_key[0] > 0) {
        // Return cursor position
        tyvaluerecord addrval;

        if (!setaddressvalue(htable, ctx->cursor_key, &addrval)) {
            opdisposelist(hlist);
            table_selection_release(ctx);
            return false;
        }

        if (!langpushlistval(hlist, nil, &addrval)) {
            opdisposelist(hlist);
            table_selection_release(ctx);
            return false;
        }

        fl = true;
        goto done;
    }

    //
    // CASE 3: No selection, no cursor → return first entry
    //
    hdlhashnode hfirst = (**htable).hfirstsort;
    if (hfirst != nil) {
        bigstring key;
        tyvaluerecord addrval;

        // Get first entry's key
        gethashkey(hfirst, key);

        // Create address value
        if (!setaddressvalue(htable, key, &addrval)) {
            opdisposelist(hlist);
            table_selection_release(ctx);
            return false;
        }

        // Add to list
        if (!langpushlistval(hlist, nil, &addrval)) {
            opdisposelist(hlist);
            table_selection_release(ctx);
            return false;
        }

        fl = true;
    } else {
        // Empty table, return empty list
        fl = true;
    }

done:
    table_selection_release(ctx);

    if (fl) {
        // Return list as value
        return setheapvalue((Handle)hlist, listvaluetype, v);
    } else {
        opdisposelist(hlist);
        return false;
    }
}
```

---

## Verb Binding (tableverbs.c)

### Integration Point

In `Common/source/tableverbs.c`, the `tableverbfunc` switch statement needs:

```c
case getselectionfunc:  // Line ~841
    // Check for headless mode
    if (flheadless) {
        (*v).data.flvalue = table_getselection_headless(v);
    } else {
        // Existing windowed implementation
        (*v).data.flvalue = tablegetselectionverb(v);
    }
    return (true);
```

### Headless Mode Detection

```c
// In shellheaders.h or similar
extern boolean flheadless;  // Set at startup based on environment

// Or check for window context
boolean is_headless_mode(void) {
    return (outlinewindow == NULL && !langfindtargetwindow(NULL));
}
```

---

## Edge Cases and Error Handling

### Edge Case 1: Empty Table

**Scenario:** Table has no entries.

**Behavior:** Return empty list.

**Implementation:**
```c
if ((**htable).hfirstsort == nil) {
    // Empty table, return empty list
    return setheapvalue((Handle)hlist, listvaluetype, v);
}
```

### Edge Case 2: Selection Contains Deleted Keys

**Scenario:** User selected keys, then deleted some entries. Selection list contains stale keys.

**Behavior:** Skip deleted keys, only return valid entries.

**Implementation:**
```c
// When iterating selection list
hdlhashnode hnode;
if (hashlookup(key, &hnode, htable)) {
    // Key still exists, add to result
    langpushlistval(hlist, nil, &addrval);
} else {
    // Key deleted, skip it
    // Optional: Remove from selection context
    table_selection_remove(ctx, key);
}
```

### Edge Case 3: Cursor Points to Deleted Entry

**Scenario:** Cursor was set to key "foo", then "foo" was deleted.

**Behavior:** Move cursor to first entry, return that.

**Implementation:**
```c
// Validate cursor before returning
hdlhashnode hnode;
if (!hashlookup(ctx->cursor_key, &hnode, htable)) {
    // Cursor invalid, reset to first entry
    hdlhashnode hfirst = (**htable).hfirstsort;
    if (hfirst != nil) {
        gethashkey(hfirst, ctx->cursor_key);
        ctx->cursor_node = hfirst;
    } else {
        // Table now empty
        ctx->cursor_key[0] = 0;
        ctx->cursor_node = nil;
    }
}
```

### Edge Case 4: Nested Table Expanded/Collapsed

**Scenario:** Selection contains entries from nested table. User collapses nested table.

**Behavior:** Return all selected entries, even if parent collapsed (selection is independent of visibility).

**Rationale:** Selection represents logical set, not visual display.

---

## Test Cases

### Test 1: Empty Selection, No Cursor

```usertalk
// Setup
local (t)
lang.new(tableType, @t)
t.a = 1; t.b = 2; t.c = 3

// Test
local (sel = table.getSelection(@t))

// Verify
assert(sizeof(sel) == 1, "Should return one entry")
assert(sel[1] == @t.a, "Should return first entry (sorted)")
```

### Test 2: Cursor Set, No Selection

```usertalk
// Setup
local (t)
lang.new(tableType, @t)
t.a = 1; t.b = 2; t.c = 3
table.goto(@t, 2)  // Set cursor to 'b'

// Test
local (sel = table.getSelection(@t))

// Verify
assert(sizeof(sel) == 1, "Should return one entry")
assert(sel[1] == @t.b, "Should return cursor position")
```

### Test 3: Multi-Selection

```usertalk
// Setup
local (t)
lang.new(tableType, @t)
t.a = 1; t.b = 2; t.c = 3; t.d = 4
table.select(@t.a)
table.select(@t.c)
table.select(@t.d)

// Test
local (sel = table.getSelection(@t))

// Verify
assert(sizeof(sel) == 3, "Should return three entries")
assert(sel[1] == @t.a, "First selection")
assert(sel[2] == @t.c, "Second selection")
assert(sel[3] == @t.d, "Third selection")
```

### Test 4: Empty Table

```usertalk
// Setup
local (t)
lang.new(tableType, @t)

// Test
local (sel = table.getSelection(@t))

// Verify
assert(sizeof(sel) == 0, "Should return empty list")
```

### Test 5: Selection After Deletion

```usertalk
// Setup
local (t)
lang.new(tableType, @t)
t.a = 1; t.b = 2; t.c = 3
table.select(@t.a)
table.select(@t.b)
table.select(@t.c)

// Delete middle entry
delete(@t.b)

// Test
local (sel = table.getSelection(@t))

// Verify
assert(sizeof(sel) == 2, "Should return two entries (b deleted)")
assert(sel[1] == @t.a, "First selection")
assert(sel[2] == @t.c, "Second selection (b skipped)")
```

### Test 6: Nested Table Selection

```usertalk
// Setup
local (t)
lang.new(tableType, @t)
t.a = 1
lang.new(tableType, @t.nested)
t.nested.x = 10; t.nested.y = 20

// Expand nested table
table.expand(@t.nested)

// Select from both levels
table.select(@t.a)
table.select(@t.nested.x)

// Test
local (sel = table.getSelection(@t))

// Verify
assert(sizeof(sel) == 2, "Should return two entries")
assert(sel[1] == @t.a, "Top-level selection")
assert(sel[2] == @t.nested.x, "Nested selection")
```

---

## Performance Considerations

### Optimization 1: Direct List Construction

Instead of individual langpushlistval calls, construct list in one pass:

```c
// Pre-allocate list size if known
if (ctx->ct_selected > 0) {
    // Reserve space for N items
    // (Frontier lists don't have reserve, so skip)
}
```

### Optimization 2: Cached Selection Validation

Keep selection list validated (remove deleted keys on delete operations):

```c
// In table.delete() or table.assign()
table_selection_validate(ctx, htable);  // Remove stale keys
```

### Optimization 3: Lazy List Creation

Only create list handle when needed:

```c
// Already implemented above (create hlist early, populate, then return)
```

---

## Thread Safety Notes

### Phase 3 (Current)
- Thread-local context → no locking needed
- Single-threaded script execution
- No concurrent access to selection context

### Phase 6+ (Multi-User)
- Each user thread has isolated selection context
- User A's selection invisible to User B
- Object-level mutations locked via `table_context_t`
- Selection reads/writes remain lock-free (thread-local)

---

## Error Messages

```c
// No table context
BIGSTRING("\x18" "No table is current")

// Selection context allocation failed
BIGSTRING("\x20" "Can't get selection context")

// Memory allocation failed
BIGSTRING("\x1C" "Can't create result list")
```

---

## Integration Checklist

- [ ] Implement `table_getselection_headless()` in `Common/source/headless_selection.c`
- [ ] Add prototype to `Common/headers/headless_selection.h`
- [ ] Update `tableverbs.c` with headless dispatch
- [ ] Add unit tests to `tests/headless_selection_tests.c`
- [ ] Add UserTalk integration tests
- [ ] Verify via `./tools/run_headless_tests.sh`
- [ ] Document in `docs/table_verbs_headless.md`

---

## Summary

`table.getSelection()` in headless mode:
- Returns list of selected entry addresses
- Falls back to cursor position if no selection
- Falls back to first entry if no cursor
- Handles deleted entries gracefully
- Thread-safe via thread-local context
- Backward compatible with windowed behavior (same return type/semantics)
