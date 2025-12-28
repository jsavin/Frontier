# Table Verb: table.goto() Implementation

**Status:** Phase 3 Implementation Ready
**Date:** 2025-12-28
**Verb:** `table.goto(row)`
**Category:** Navigation

---

## Verb Signature

```usertalk
table.goto(row) → address
```

**Parameters:**
- `row` (long): 1-based row number to navigate to

**Returns:** Address of entry at row N

**Description:**
Moves cursor to the Nth row in the table (1-based indexing). Row numbers account for expansion state of nested tables. Returns address of entry at that position.

---

## Legacy Windowed Behavior

In classic Frontier with table windows:
1. Table displayed in window with visible row numbers
2. User can expand/collapse nested tables (changes row numbering)
3. `table.goto(N)` moves cursor to row N in visual display
4. Returns address of entry at that row

**Example:**
```usertalk
// Table window shows:
//   1: workspace
//   2:   workspace.prefs (collapsed)
//   3:   workspace.data
//   4: workspace.temp

table.goto(2)
// Returns: @workspace.prefs
// Cursor now at row 2

// User expands workspace.prefs (has 2 children)
//   1: workspace
//   2:   workspace.prefs (expanded)
//   3:     workspace.prefs.fontsize
//   4:     workspace.prefs.theme
//   5:   workspace.data
//   6: workspace.temp

table.goto(4)
// Returns: @workspace.prefs.theme (row 4 now different!)
```

---

## Headless Behavior

In headless mode (no table window):
1. Expansion state tracked in thread-local context
2. Row numbers calculated dynamically (accounting for expansion)
3. `table.goto(N)` sets cursor to Nth visible row
4. Returns address of entry at that position

**Key Challenge:** Row numbers depend on expansion state, which is dynamic.

---

## Implementation

### Function Signature (C)

```c
/**
 * table_goto_headless - Navigate to row N in table
 *
 * @param htable - Table to navigate within
 * @param row - 1-based row number
 * @param v - Output: address value of entry at row N
 * @return true if successful, false if row out of bounds
 *
 * Algorithm:
 * 1. Validate row number (must be >= 1)
 * 2. Walk sorted list, accounting for expanded nested tables
 * 3. Find hash node at position N
 * 4. Update cursor in selection context
 * 5. Return address of node
 */
boolean table_goto_headless(hdlhashtable htable, long row, tyvaluerecord *v);
```

### Complete Implementation

```c
boolean table_goto_headless(hdlhashtable htable, long row, tyvaluerecord *v) {
    table_selection_context_t *ctx = NULL;
    hdlhashnode target_node = NULL;
    hdlhashtable target_table = NULL;
    bigstring key;

    // Validate parameters
    if (htable == NULL) {
        return langerror(BIGSTRING("\x10" "No table given"));
    }

    if (row < 1) {
        return langerror(BIGSTRING("\x1E" "Row number must be 1 or greater"));
    }

    // Acquire selection context
    ctx = table_selection_acquire();
    if (ctx == NULL) {
        return langerror(BIGSTRING("\x20" "Can't get selection context"));
    }

    // Update context table reference
    ctx->current_table = htable;

    // Find node at row N (accounting for expansion state)
    if (!table_selection_get_node_at_row(ctx, htable, row,
                                         &target_node, &target_table)) {
        table_selection_release(ctx);
        return langerror(BIGSTRING("\x18" "Row number out of range"));
    }

    // Get key from node
    gethashkey(target_node, key);

    // Update cursor in context
    copystring(key, ctx->cursor_key);
    ctx->cursor_node = target_node;
    ctx->cursor_flat_index = row;

    // Create address value for return
    boolean fl = setaddressvalue(target_table, key, v);

    table_selection_release(ctx);
    return fl;
}
```

### Helper: Get Node at Row N

```c
/**
 * table_selection_get_node_at_row - Find hash node at 1-based row index
 *
 * @param ctx - Selection context (for expansion state)
 * @param htable - Root table to search
 * @param row - 1-based row number
 * @param hnode_out - Output: hash node at that row
 * @param htable_out - Output: table containing that node (may be nested)
 * @return true if found, false if row out of bounds
 *
 * Algorithm:
 * 1. Walk sorted list of immediate children
 * 2. For each child:
 *    a. Increment current row counter
 *    b. If current row == target row, return this node
 *    c. If this child is an expanded nested table:
 *       - Count visible rows in nested table
 *       - If target row is within nested range, recurse
 *       - Otherwise, skip nested rows and continue
 * 3. If we exhaust list without finding row, return false
 */
boolean table_selection_get_node_at_row(table_selection_context_t *ctx,
                                        hdlhashtable htable,
                                        long row,
                                        hdlhashnode *hnode_out,
                                        hdlhashtable *htable_out) {
    long current_row = 0;
    hdlhashnode nomad = (**htable).hfirstsort;

    while (nomad != nil) {
        current_row++;  // This entry counts as a row

        // Is this our target row?
        if (current_row == row) {
            *hnode_out = nomad;
            *htable_out = htable;
            return true;
        }

        // Check if this entry is an expanded nested table
        if ((**nomad).val.valuetype == externalvaluetype) {
            hdlexternalvariable hv;
            hv = (hdlexternalvariable)((**nomad).val.data.externalvalue);

            // Is it a table?
            if ((**hv).id == idtableprocessor) {
                hdlhashtable nested_table;
                nested_table = (hdlhashtable)((**hv).variabledata);

                // Is it expanded?
                if (table_selection_is_expanded(ctx, nested_table)) {
                    // Count visible rows in nested table
                    long nested_count;
                    nested_count = table_selection_count_visible_rows(ctx, nested_table);

                    // Is target row within nested range?
                    if (current_row + nested_count >= row) {
                        // Target is inside nested table, recurse
                        long nested_row = row - current_row;
                        return table_selection_get_node_at_row(ctx,
                                                               nested_table,
                                                               nested_row,
                                                               hnode_out,
                                                               htable_out);
                    }

                    // Target row is beyond this nested table, skip its rows
                    current_row += nested_count;
                }
            }
        }

        // Move to next sibling
        nomad = (**nomad).sortedlink;
    }

    // Row number out of bounds
    return false;
}
```

### Helper: Count Visible Rows

```c
/**
 * table_selection_count_visible_rows - Count total visible rows
 *
 * @param ctx - Selection context (for expansion state)
 * @param htable - Table to count
 * @return Total number of visible rows
 *
 * Algorithm:
 * 1. Start count at 0
 * 2. For each immediate child:
 *    a. Increment count (this entry is visible)
 *    b. If this entry is an expanded nested table:
 *       - Recursively count visible rows in nested table
 *       - Add to total count
 * 3. Return total count
 */
long table_selection_count_visible_rows(table_selection_context_t *ctx,
                                        hdlhashtable htable) {
    long count = 0;
    hdlhashnode nomad = (**htable).hfirstsort;

    while (nomad != nil) {
        count++;  // This entry is visible

        // Check if this is an expanded nested table
        if ((**nomad).val.valuetype == externalvaluetype) {
            hdlexternalvariable hv;
            hv = (hdlexternalvariable)((**nomad).val.data.externalvalue);

            if ((**hv).id == idtableprocessor) {
                hdlhashtable nested_table;
                nested_table = (hdlhashtable)((**hv).variabledata);

                if (table_selection_is_expanded(ctx, nested_table)) {
                    // Add nested table's visible rows
                    count += table_selection_count_visible_rows(ctx, nested_table);
                }
            }
        }

        nomad = (**nomad).sortedlink;
    }

    return count;
}
```

---

## Verb Binding (tableverbs.c)

### Integration Point

In `Common/source/tableverbs.c`, the `tableverbfunc` switch statement needs:

```c
case gotofunc:  // Line ~846
    {
        long row;
        hdlhashtable htable;

        flnextparamislast = true;

        // Get row parameter
        if (!getlongvalue(hparam1, 1, &row)) {
            break;
        }

        // Get target table
        if (!langfindtargethashtable(&htable)) {
            langerror(BIGSTRING("\x18" "No table is current"));
            break;
        }

        // Check for headless mode
        if (flheadless) {
            (*v).data.flvalue = table_goto_headless(htable, row, v);
        } else {
            // Existing windowed implementation
            (*v).data.flvalue = tablegotoverb(row, v);
        }

        return (true);
    }
```

---

## Edge Cases and Error Handling

### Edge Case 1: Row 0 or Negative

**Scenario:** User calls `table.goto(0)` or `table.goto(-5)`.

**Behavior:** Error (row numbers start at 1).

**Implementation:**
```c
if (row < 1) {
    return langerror(BIGSTRING("\x1E" "Row number must be 1 or greater"));
}
```

### Edge Case 2: Row Beyond End

**Scenario:** Table has 10 visible rows, user calls `table.goto(15)`.

**Behavior:** Error (row out of bounds).

**Implementation:**
```c
// In table_selection_get_node_at_row, if we exhaust list:
return false;  // Triggers "Row number out of range" error
```

### Edge Case 3: Empty Table

**Scenario:** Table has no entries, user calls `table.goto(1)`.

**Behavior:** Error (no rows to navigate to).

**Implementation:**
```c
if ((**htable).hfirstsort == nil) {
    return langerror(BIGSTRING("\x10" "Table is empty"));
}
```

### Edge Case 4: Expansion State Changes

**Scenario:** User navigates to row 5, then expands a nested table. Row 5 now points to different entry.

**Behavior:** Row numbers are always recalculated from current expansion state.

**Rationale:** Matches windowed behavior (row numbers shift on expand/collapse).

### Edge Case 5: Deep Nesting

**Scenario:** Table with deeply nested structure (10+ levels).

**Behavior:** Recursion handles arbitrary depth, with stack overflow protection.

**Implementation:**
```c
// In table_selection_get_node_at_row
if (recursion_depth > 100) {
    return langerror(BIGSTRING("\x1A" "Table nesting too deep"));
}
```

---

## Test Cases

### Test 1: Navigate to First Row

```usertalk
// Setup
local (t)
lang.new(tableType, @t)
t.a = 1; t.b = 2; t.c = 3

// Test
local (addr = table.goto(@t, 1))

// Verify
assert(addr == @t.a, "Row 1 should be first entry")
assert(table.getCursor() == @t.a, "Cursor should be at row 1")
```

### Test 2: Navigate to Middle Row

```usertalk
// Setup
local (t)
lang.new(tableType, @t)
t.a = 1; t.b = 2; t.c = 3; t.d = 4

// Test
local (addr = table.goto(@t, 3))

// Verify
assert(addr == @t.c, "Row 3 should be third entry")
assert(table.getCursor() == @t.c, "Cursor should be at row 3")
```

### Test 3: Navigate to Last Row

```usertalk
// Setup
local (t)
lang.new(tableType, @t)
t.a = 1; t.b = 2; t.c = 3

// Test
local (addr = table.goto(@t, 3))

// Verify
assert(addr == @t.c, "Row 3 should be last entry")
```

### Test 4: Row Out of Bounds

```usertalk
// Setup
local (t)
lang.new(tableType, @t)
t.a = 1; t.b = 2; t.c = 3

// Test
try
    local (addr = table.goto(@t, 10))
    assert(false, "Should have thrown error")
bundle
    // Expected error
    assert(true, "Correctly threw row out of range error")
```

### Test 5: Nested Table Collapsed

```usertalk
// Setup
local (t)
lang.new(tableType, @t)
t.a = 1
lang.new(tableType, @t.nested)
t.nested.x = 10; t.nested.y = 20
t.b = 2

// Initially collapsed (3 visible rows: a, nested, b)
// Row 1: a
// Row 2: nested (collapsed)
// Row 3: b

// Test
local (addr = table.goto(@t, 2))

// Verify
assert(addr == @t.nested, "Row 2 should be nested table")
assert(nameof(addr) == "nested", "Cursor at nested")
```

### Test 6: Nested Table Expanded

```usertalk
// Setup
local (t)
lang.new(tableType, @t)
t.a = 1
lang.new(tableType, @t.nested)
t.nested.x = 10; t.nested.y = 20
t.b = 2

// Expand nested table
table.expand(@t.nested)

// Now 5 visible rows:
// Row 1: a
// Row 2: nested (expanded)
// Row 3:   nested.x
// Row 4:   nested.y
// Row 5: b

// Test
local (addr = table.goto(@t, 4))

// Verify
assert(addr == @t.nested.y, "Row 4 should be nested.y")
assert(nameof(addr) == "y", "Cursor at nested.y")
```

### Test 7: Expansion State Changes Row Numbers

```usertalk
// Setup
local (t)
lang.new(tableType, @t)
t.a = 1
lang.new(tableType, @t.nested)
t.nested.x = 10; t.nested.y = 20
t.b = 2

// Initially collapsed: row 3 is 'b'
local (addr1 = table.goto(@t, 3))
assert(addr1 == @t.b, "Row 3 is 'b' when collapsed")

// Expand nested table
table.expand(@t.nested)

// Now row 3 is 'nested.x', row 5 is 'b'
local (addr2 = table.goto(@t, 3))
assert(addr2 == @t.nested.x, "Row 3 is 'nested.x' when expanded")

local (addr3 = table.goto(@t, 5))
assert(addr3 == @t.b, "Row 5 is 'b' when expanded")
```

### Test 8: Deep Nesting

```usertalk
// Setup
local (t)
lang.new(tableType, @t)
lang.new(tableType, @t.level1)
lang.new(tableType, @t.level1.level2)
lang.new(tableType, @t.level1.level2.level3)
t.level1.level2.level3.data = "deep"

// Expand all levels
table.expand(@t.level1)
table.expand(@t.level1.level2)
table.expand(@t.level1.level2.level3)

// Navigate to deepest entry
// Row 1: level1
// Row 2:   level1.level2
// Row 3:     level1.level2.level3
// Row 4:       level1.level2.level3.data
local (addr = table.goto(@t, 4))

// Verify
assert(addr == @t.level1.level2.level3.data, "Found deep entry")
```

### Test 9: Empty Table

```usertalk
// Setup
local (t)
lang.new(tableType, @t)

// Test
try
    local (addr = table.goto(@t, 1))
    assert(false, "Should have thrown error")
bundle
    // Expected error
    assert(true, "Correctly threw empty table error")
```

---

## Performance Considerations

### Optimization 1: Cache Row Count

**Problem:** Counting visible rows requires full tree traversal (expensive).

**Solution:** Cache total visible row count, invalidate on expansion changes.

```c
// In table_selection_context_t
long cached_total_rows;
boolean row_cache_valid;

// Invalidate on expansion/collapse
void table_selection_expand(ctx, htable) {
    // ... expand logic ...
    ctx->row_cache_valid = false;  // Invalidate cache
}
```

### Optimization 2: Binary Search (Future)

For very large tables (1000+ entries), linear search is slow. Could implement binary search if we maintain indexed expansion map.

**Not implemented in Phase 3** (premature optimization).

### Optimization 3: Direct Node Access

If cursor already at row N, and user calls `table.goto(N)`, skip traversal:

```c
if (ctx->cursor_flat_index == row && ctx->cursor_node != nil) {
    // Already at target row
    return setaddressvalue(ctx->current_table, ctx->cursor_key, v);
}
```

---

## Thread Safety Notes

### Phase 3 (Current)
- Thread-local context → no locking needed
- Expansion state isolated per thread
- No shared state

### Phase 6+ (Multi-User)
- Each user has own expansion state
- User A expanding table doesn't affect User B
- Object mutations (adding/deleting entries) locked separately

---

## Error Messages

```c
// No table given
BIGSTRING("\x10" "No table given")

// Row number < 1
BIGSTRING("\x1E" "Row number must be 1 or greater")

// Row number out of bounds
BIGSTRING("\x18" "Row number out of range")

// Empty table
BIGSTRING("\x10" "Table is empty")

// Nesting too deep
BIGSTRING("\x1A" "Table nesting too deep")
```

---

## Integration Checklist

- [ ] Implement `table_goto_headless()` in `Common/source/headless_selection.c`
- [ ] Implement `table_selection_get_node_at_row()` helper
- [ ] Implement `table_selection_count_visible_rows()` helper
- [ ] Add prototypes to `Common/headers/headless_selection.h`
- [ ] Update `tableverbs.c` with headless dispatch
- [ ] Add unit tests to `tests/headless_selection_tests.c`
- [ ] Add UserTalk integration tests
- [ ] Verify via `./tools/run_headless_tests.sh`

---

## Summary

`table.goto(row)` in headless mode:
- Navigates to Nth visible row (1-based)
- Accounts for expansion state of nested tables
- Updates cursor in thread-local context
- Returns address of entry at that row
- Handles edge cases (empty table, out of bounds, deep nesting)
- Thread-safe via thread-local storage
- Backward compatible with windowed behavior
