# Table Verb: table.go() Implementation

**Status:** Phase 3 Implementation Ready
**Date:** 2025-12-28
**Verb:** `table.go(direction, count)`
**Category:** Navigation

---

## Verb Signature

```usertalk
table.go(direction, count) → address
```

**Parameters:**
- `direction` (tydirection): Movement direction (up, down, left, right, flatup, flatdown)
- `count` (long): Number of rows to move (default 1)

**Returns:** Address of entry at new cursor position

**Description:**
Moves cursor by N rows in specified direction. In headless mode, only up/down are supported (flatup/flatdown treat as up/down, left/right error). Accounts for expansion state of nested tables.

---

## Legacy Windowed Behavior

In classic Frontier with table windows:
- **down**: Move cursor down N visible rows
- **up**: Move cursor up N visible rows
- **right**: Expand current entry (if table), move into first child
- **left**: Collapse current entry (if expanded), or move to parent
- **flatdown**: Move down N rows, skipping nested tables
- **flatup**: Move up N rows, skipping nested tables

**Example:**
```usertalk
// Table window shows:
//   1: workspace
//   2:   workspace.prefs (expanded)
//   3:     workspace.prefs.fontsize  ← cursor here
//   4:     workspace.prefs.theme
//   5:   workspace.data

table.go(down, 1)
// Cursor moves to row 4 (workspace.prefs.theme)
// Returns: @workspace.prefs.theme

table.go(up, 2)
// Cursor moves to row 2 (workspace.prefs)
// Returns: @workspace.prefs

table.go(right, 1)
// Cursor stays at row 2 (already expanded)
// (If collapsed, would expand and move to first child)

table.go(left, 1)
// Cursor collapses workspace.prefs, stays at row 2
// Or if already collapsed, moves to parent (workspace)
```

---

## Headless Behavior

In headless mode (no table window):
- **down**: Move cursor down N visible rows (accounting for expansion)
- **up**: Move cursor up N visible rows
- **flatdown**: Same as down (no outline hierarchy in tables)
- **flatup**: Same as up
- **left/right**: Error (not supported in headless mode)

**Rationale:**
- Left/right navigation makes sense only with visual tree display
- Tables don't have "move into" semantics like outlines
- Expansion controlled explicitly via table.expand/collapse verbs

---

## Implementation

### Function Signature (C)

```c
/**
 * table_go_headless - Move cursor relative to current position
 *
 * @param htable - Table to navigate within
 * @param dir - Direction to move (up or down only)
 * @param ct - Number of rows to move
 * @param v - Output: address value of entry at new position
 * @return true if successful, false on error
 *
 * Algorithm:
 * 1. Validate direction (only up/down supported)
 * 2. Get current cursor position (as row number)
 * 3. Calculate target row: current + ct (down) or current - ct (up)
 * 4. Validate target row is in bounds
 * 5. Move cursor to target row (via table_goto_headless)
 * 6. Return address of new position
 */
boolean table_go_headless(hdlhashtable htable, tydirection dir, long ct,
                          tyvaluerecord *v);
```

### Complete Implementation

```c
boolean table_go_headless(hdlhashtable htable, tydirection dir, long ct,
                          tyvaluerecord *v) {
    table_selection_context_t *ctx = NULL;
    long current_row = 0;
    long target_row = 0;
    long total_rows = 0;

    // Validate parameters
    if (htable == NULL) {
        return langerror(BIGSTRING("\x10" "No table given"));
    }

    // Validate direction
    if (dir == left || dir == right) {
        return langerror(BIGSTRING("\x2C" "Left/right not supported in headless mode"));
    }

    // Normalize flat directions to standard up/down
    if (dir == flatup) {
        dir = up;
    } else if (dir == flatdown) {
        dir = down;
    }

    if (dir != up && dir != down) {
        return langerror(BIGSTRING("\x18" "Invalid direction"));
    }

    if (ct < 0) {
        return langerror(BIGSTRING("\x1C" "Count must be non-negative"));
    }

    if (ct == 0) {
        // No movement, return current cursor
        return table_getselection_headless(v);
    }

    // Acquire selection context
    ctx = table_selection_acquire();
    if (ctx == NULL) {
        return langerror(BIGSTRING("\x20" "Can't get selection context"));
    }

    ctx->current_table = htable;

    // Get current cursor position
    if (ctx->cursor_key[0] == 0) {
        // No cursor set, default to row 1
        current_row = 1;
    } else {
        // Find current row number
        current_row = table_selection_get_row_for_key(ctx, htable, ctx->cursor_key);
        if (current_row == 0) {
            // Cursor invalid (entry deleted), reset to row 1
            current_row = 1;
        }
    }

    // Calculate total visible rows
    total_rows = table_selection_count_visible_rows(ctx, htable);

    if (total_rows == 0) {
        table_selection_release(ctx);
        return langerror(BIGSTRING("\x10" "Table is empty"));
    }

    // Calculate target row
    if (dir == down) {
        target_row = current_row + ct;
    } else {  // up
        target_row = current_row - ct;
    }

    // Validate target row is in bounds
    if (target_row < 1) {
        table_selection_release(ctx);
        return langerror(BIGSTRING("\x1C" "Moved before first row"));
    }

    if (target_row > total_rows) {
        table_selection_release(ctx);
        return langerror(BIGSTRING("\x18" "Moved past last row"));
    }

    // Release context before calling table_goto (which re-acquires)
    table_selection_release(ctx);

    // Move to target row
    return table_goto_headless(htable, target_row, v);
}
```

### Helper: Get Row Number for Key

```c
/**
 * table_selection_get_row_for_key - Find 1-based row number for key
 *
 * @param ctx - Selection context (for expansion state)
 * @param htable - Root table
 * @param key - Key to find
 * @return 1-based row number, or 0 if not found
 *
 * Algorithm:
 * 1. Walk sorted list, counting visible rows
 * 2. When we find matching key, return current row count
 * 3. Account for expanded nested tables (recurse)
 */
long table_selection_get_row_for_key(table_selection_context_t *ctx,
                                     hdlhashtable htable,
                                     bigstring key) {
    long current_row = 0;
    long result = 0;

    if (!table_selection_get_row_for_key_recursive(ctx, htable, key,
                                                   &current_row, &result)) {
        return 0;  // Not found
    }

    return result;
}

/**
 * Helper: Recursive search for key
 *
 * @param ctx - Selection context
 * @param htable - Current table
 * @param key - Key to find
 * @param current_row - Running row counter (updated as we walk)
 * @param result_row - Output: row number when found
 * @return true if found, false if not found
 */
boolean table_selection_get_row_for_key_recursive(
    table_selection_context_t *ctx,
    hdlhashtable htable,
    bigstring key,
    long *current_row,
    long *result_row) {

    hdlhashnode nomad = (**htable).hfirstsort;

    while (nomad != nil) {
        (*current_row)++;  // Increment row counter

        // Get this entry's key
        bigstring node_key;
        gethashkey(nomad, node_key);

        // Is this our target?
        if (equalstrings(node_key, key)) {
            *result_row = *current_row;
            return true;  // Found it!
        }

        // Check if this is an expanded nested table
        if ((**nomad).val.valuetype == externalvaluetype) {
            hdlexternalvariable hv;
            hv = (hdlexternalvariable)((**nomad).val.data.externalvalue);

            if ((**hv).id == idtableprocessor) {
                hdlhashtable nested_table;
                nested_table = (hdlhashtable)((**hv).variabledata);

                if (table_selection_is_expanded(ctx, nested_table)) {
                    // Search in nested table
                    if (table_selection_get_row_for_key_recursive(
                            ctx, nested_table, key, current_row, result_row)) {
                        return true;  // Found in nested table
                    }
                    // Not in this nested table, current_row already updated
                }
            }
        }

        nomad = (**nomad).sortedlink;
    }

    return false;  // Not found in this table
}
```

---

## Verb Binding (tableverbs.c)

### Integration Point

In `Common/source/tableverbs.c`, the `tableverbfunc` switch statement needs:

```c
case gofunc:  // Line ~888
    {
        tydirection dir;
        long ct = 1;  // Default to 1
        hdlhashtable htable;

        // Get direction parameter
        if (!getdirparam(hparam1, 1, &dir)) {
            break;
        }

        // Get count parameter (optional, default 1)
        if (!getlongvalue(hparam1, 2, &ct)) {
            ct = 1;  // Use default
        }

        flnextparamislast = true;

        // Get target table
        if (!langfindtargethashtable(&htable)) {
            langerror(BIGSTRING("\x18" "No table is current"));
            break;
        }

        // Check for headless mode
        if (flheadless) {
            (*v).data.flvalue = table_go_headless(htable, dir, ct, v);
        } else {
            // Existing windowed implementation
            (*v).data.flvalue = tablegoverb(dir, ct, v);
        }

        return (true);
    }
```

---

## Edge Cases and Error Handling

### Edge Case 1: Move Before First Row

**Scenario:** Cursor at row 3, user calls `table.go(up, 5)`.

**Behavior:** Error (can't move to row -2).

**Implementation:**
```c
if (target_row < 1) {
    return langerror(BIGSTRING("\x1C" "Moved before first row"));
}
```

### Edge Case 2: Move Past Last Row

**Scenario:** Table has 10 rows, cursor at row 8, user calls `table.go(down, 5)`.

**Behavior:** Error (can't move to row 13).

**Implementation:**
```c
if (target_row > total_rows) {
    return langerror(BIGSTRING("\x18" "Moved past last row"));
}
```

### Edge Case 3: No Cursor Set

**Scenario:** User calls `table.go(down, 1)` without setting cursor.

**Behavior:** Default cursor to row 1, then move from there.

**Implementation:**
```c
if (ctx->cursor_key[0] == 0) {
    current_row = 1;  // Start from first row
}
```

### Edge Case 4: Left/Right in Headless Mode

**Scenario:** User calls `table.go(left, 1)` in headless mode.

**Behavior:** Error (not supported).

**Implementation:**
```c
if (dir == left || dir == right) {
    return langerror(BIGSTRING("\x2C" "Left/right not supported in headless mode"));
}
```

### Edge Case 5: Count = 0

**Scenario:** User calls `table.go(down, 0)`.

**Behavior:** No movement, return current cursor position.

**Implementation:**
```c
if (ct == 0) {
    return table_getselection_headless(v);
}
```

### Edge Case 6: Expansion Changes During Navigation

**Scenario:** User at row 5, expands nested table (shifts row numbers), then calls `table.go(down, 1)`.

**Behavior:** Row numbers recalculated from current expansion state.

**Rationale:** Movement always relative to current visual state.

---

## Test Cases

### Test 1: Move Down from First Row

```usertalk
// Setup
local (t)
lang.new(tableType, @t)
t.a = 1; t.b = 2; t.c = 3; t.d = 4

table.goto(@t, 1)  // Start at row 1 (a)

// Test
local (addr = table.go(@t, down, 2))

// Verify
assert(addr == @t.c, "Should move to row 3 (c)")
assert(table.getCursor() == @t.c, "Cursor at c")
```

### Test 2: Move Up from Last Row

```usertalk
// Setup
local (t)
lang.new(tableType, @t)
t.a = 1; t.b = 2; t.c = 3; t.d = 4

table.goto(@t, 4)  // Start at row 4 (d)

// Test
local (addr = table.go(@t, up, 2))

// Verify
assert(addr == @t.b, "Should move to row 2 (b)")
assert(table.getCursor() == @t.b, "Cursor at b")
```

### Test 3: Move Down Past End

```usertalk
// Setup
local (t)
lang.new(tableType, @t)
t.a = 1; t.b = 2; t.c = 3

table.goto(@t, 2)  // Start at row 2 (b)

// Test
try
    local (addr = table.go(@t, down, 5))
    assert(false, "Should have thrown error")
bundle
    // Expected error
    assert(true, "Correctly threw past last row error")
```

### Test 4: Move Up Before First

```usertalk
// Setup
local (t)
lang.new(tableType, @t)
t.a = 1; t.b = 2; t.c = 3

table.goto(@t, 2)  // Start at row 2 (b)

// Test
try
    local (addr = table.go(@t, up, 5))
    assert(false, "Should have thrown error")
bundle
    // Expected error
    assert(true, "Correctly threw before first row error")
```

### Test 5: No Cursor, Default to Row 1

```usertalk
// Setup
local (t)
lang.new(tableType, @t)
t.a = 1; t.b = 2; t.c = 3

// Don't set cursor

// Test
local (addr = table.go(@t, down, 1))

// Verify
assert(addr == @t.b, "Should start at row 1, move to row 2 (b)")
```

### Test 6: Navigate with Expanded Nested Table

```usertalk
// Setup
local (t)
lang.new(tableType, @t)
t.a = 1
lang.new(tableType, @t.nested)
t.nested.x = 10; t.nested.y = 20
t.b = 2

table.expand(@t.nested)

// Rows now:
// 1: a
// 2: nested (expanded)
// 3:   nested.x
// 4:   nested.y
// 5: b

table.goto(@t, 2)  // Start at nested

// Test
local (addr = table.go(@t, down, 2))

// Verify
assert(addr == @t.nested.y, "Should be at row 4 (nested.y)")
```

### Test 7: Navigate Across Nested Table Boundary

```usertalk
// Setup
local (t)
lang.new(tableType, @t)
t.a = 1
lang.new(tableType, @t.nested)
t.nested.x = 10
t.b = 2

table.expand(@t.nested)

// Rows:
// 1: a
// 2: nested
// 3:   nested.x
// 4: b

table.goto(@t, 3)  // Start at nested.x

// Test
local (addr = table.go(@t, down, 1))

// Verify
assert(addr == @t.b, "Should move from nested.x to b")
```

### Test 8: Left/Right Not Supported

```usertalk
// Setup
local (t)
lang.new(tableType, @t)
t.a = 1; t.b = 2

table.goto(@t, 1)

// Test left
try
    local (addr = table.go(@t, left, 1))
    assert(false, "Should have thrown error")
bundle
    assert(true, "Correctly rejected left direction")

// Test right
try
    local (addr = table.go(@t, right, 1))
    assert(false, "Should have thrown error")
bundle
    assert(true, "Correctly rejected right direction")
```

### Test 9: Count = 0

```usertalk
// Setup
local (t)
lang.new(tableType, @t)
t.a = 1; t.b = 2; t.c = 3

table.goto(@t, 2)  // Set cursor to b

// Test
local (addr = table.go(@t, down, 0))

// Verify
assert(addr == @t.b, "Should stay at current position (b)")
```

### Test 10: Flatdown/Flatup Treated as Down/Up

```usertalk
// Setup
local (t)
lang.new(tableType, @t)
t.a = 1; t.b = 2; t.c = 3

table.goto(@t, 1)

// Test flatdown
local (addr1 = table.go(@t, flatdown, 1))
assert(addr1 == @t.b, "Flatdown works like down")

// Test flatup
local (addr2 = table.go(@t, flatup, 1))
assert(addr2 == @t.a, "Flatup works like up")
```

---

## Performance Considerations

### Optimization 1: Cache Current Row

If cursor hasn't moved since last operation, reuse cached row number:

```c
if (ctx->cursor_node == last_cursor_node && ctx->row_cache_valid) {
    current_row = ctx->cursor_flat_index;
} else {
    current_row = table_selection_get_row_for_key(ctx, htable, ctx->cursor_key);
}
```

### Optimization 2: Relative Movement

Instead of calculating absolute row numbers, could walk sorted list by N steps:

```c
// Pseudo-code for optimized down movement
hdlhashnode nomad = ctx->cursor_node;
for (long i = 0; i < ct; i++) {
    nomad = get_next_visible_node(nomad);  // Skip collapsed nested tables
}
```

**Not implemented in Phase 3** (premature optimization, adds complexity).

---

## Thread Safety Notes

### Phase 3 (Current)
- Thread-local context → no locking
- Cursor movement isolated per thread

### Phase 6+ (Multi-User)
- Each user's cursor independent
- No shared state to lock

---

## Error Messages

```c
// No table given
BIGSTRING("\x10" "No table given")

// Left/right not supported
BIGSTRING("\x2C" "Left/right not supported in headless mode")

// Invalid direction
BIGSTRING("\x18" "Invalid direction")

// Count negative
BIGSTRING("\x1C" "Count must be non-negative")

// Moved before first row
BIGSTRING("\x1C" "Moved before first row")

// Moved past last row
BIGSTRING("\x18" "Moved past last row")

// Empty table
BIGSTRING("\x10" "Table is empty")
```

---

## Integration Checklist

- [ ] Implement `table_go_headless()` in `Common/source/headless_selection.c`
- [ ] Implement `table_selection_get_row_for_key()` helper
- [ ] Implement `table_selection_get_row_for_key_recursive()` helper
- [ ] Add prototypes to `Common/headers/headless_selection.h`
- [ ] Update `tableverbs.c` with headless dispatch
- [ ] Add unit tests to `tests/headless_selection_tests.c`
- [ ] Add UserTalk integration tests
- [ ] Verify via `./tools/run_headless_tests.sh`

---

## Summary

`table.go(direction, count)` in headless mode:
- Moves cursor by N rows relative to current position
- Supports up/down directions (flatup/flatdown treated as up/down)
- Rejects left/right (not meaningful without visual tree)
- Accounts for expansion state of nested tables
- Validates bounds (can't move before first or past last)
- Returns address of new cursor position
- Thread-safe via thread-local storage
- Backward compatible semantics (where applicable)
