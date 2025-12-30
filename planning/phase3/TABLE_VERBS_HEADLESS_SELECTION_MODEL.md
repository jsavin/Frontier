# Table Verbs: Headless Selection Model Architecture

**Status:** Phase 3 Implementation Ready
**Date:** 2025-12-28
**Priority:** P1 - Critical for table navigation verbs
**Estimated Effort:** 8-10 hours

---

## Executive Summary

This document defines the architecture for headless table selection and navigation operations. Unlike the original design assumption, **nested tables must support expansion/collapse** just like outlines in legacy Frontier. This requires:

- Thread-local selection context with expansion state tracking
- Full multi-selection support via `table.select()` verb
- 1-based row indexing that respects expansion state
- Navigation verbs (goto, go) that understand nested table hierarchy

---

## Requirements (User-Confirmed)

1. **Index Base:** 1-based (row 1 = first item) - matches windowed mode
2. **Multi-Selection:** Full multi-selection with `table.select()` verb - implement upfront
3. **Nested Tables:** Support expansion/collapse (like outlines/scripts in legacy UI)
4. **Scope:** Tables only (extend to outlines later in separate phase)

---

## Architecture Overview

### Thread-Local Selection Context

Selection/cursor state stored in thread-local storage, isolated per-thread for future multi-user support.

**Why Thread-Local:**
- Clean separation from persistent object data (selection is ephemeral)
- Thread-safe for Phase 6+ collaborative editing
- No database pollution (not saved to disk)
- Easy to reset between script executions

**Storage Mechanism:**
```c
// Use platform-specific TLS
#ifdef __APPLE__
    #include <pthread.h>
    static pthread_key_t table_selection_key;
#else
    static __thread table_selection_context_t *table_selection_ctx = NULL;
#endif
```

---

## Data Structures

### Main Selection Context

```c
/**
 * table_selection_context_t - Thread-local selection and navigation state
 *
 * Design:
 * - One instance per thread
 * - Created on first access, persists until thread exit or explicit reset
 * - Stores ephemeral UI state (selection, cursor, expansion)
 * - NOT saved to database
 *
 * Thread Safety:
 * - Thread-local → no locking needed in Phase 3
 * - Phase 6+: Each user thread has isolated context
 */
typedef struct table_selection_context {
    //
    // OBJECT IDENTITY
    //
    hdlhashtable current_table;     // Which table context applies to
    boolean is_valid;                // Is context initialized?

    //
    // SELECTION STATE (Multi-Select Support)
    //
    // Frontier allows shift-click to mark multiple table entries.
    // In headless mode, table.select(key) marks entries.
    //
    hdllistrecord selected_keys;     // List of bigstrings (selected keys)
    long ct_selected;                // Count of selections

    //
    // CURSOR STATE (Single Position)
    //
    bigstring cursor_key;            // Current cursor key (empty = no cursor)
    hdlhashnode cursor_node;         // Direct pointer to current node
    long cursor_flat_index;          // 1-based flat index (accounting for expansion)

    //
    // EXPANSION STATE (Nested Table Support)
    //
    // Track which nested tables are expanded (similar to outline flexpanded).
    // Key insight: Row numbers depend on expansion state.
    //
    // Example:
    //   Row 1: workspace
    //   Row 2:   workspace.prefs (expanded, has children)
    //   Row 3:     workspace.prefs.user
    //   Row 4:     workspace.prefs.system
    //   Row 5:   workspace.data
    //
    // If workspace.prefs collapsed:
    //   Row 1: workspace
    //   Row 2:   workspace.prefs (collapsed, children hidden)
    //   Row 3:   workspace.data
    //
    hdlhashtable *expanded_tables;   // Array of expanded table handles
    long ct_expanded;                // Count of expanded tables
    long max_expanded;               // Array capacity

    //
    // ITERATION STATE
    //
    long iteration_depth;            // Current recursion depth
    boolean iteration_in_progress;   // Guard against nested iteration

    //
    // REFERENCE COUNTING
    //
    _Atomic uint32_t refcount;       // For nested operation safety

    //
    // RESERVED (Phase 6+)
    //
    void *reserved[4];               // Future: CRDT metadata, sync state

} table_selection_context_t;
```

### Helper: Expansion Map Entry

```c
/**
 * table_expansion_entry_t - Track expansion state for one table
 *
 * Used to quickly check if a nested table is expanded.
 */
typedef struct table_expansion_entry {
    hdlhashtable htable;             // Which table
    boolean flexpanded;              // Is it expanded?
} table_expansion_entry_t;
```

---

## Lifecycle API

### Context Acquisition

```c
/**
 * table_selection_acquire - Get or create thread-local selection context
 *
 * @return Context for current thread (never NULL)
 *
 * Implementation:
 * - Check thread-local storage for existing context
 * - If not found, allocate new context with refcount=1
 * - Initialize all fields to defaults
 * - Return context pointer
 *
 * Thread Safety: Thread-local, no locking needed
 */
table_selection_context_t* table_selection_acquire(void);
```

**Pseudocode:**
```c
table_selection_context_t* table_selection_acquire(void) {
    // Get from thread-local storage
    table_selection_context_t *ctx = pthread_getspecific(table_selection_key);

    if (ctx == NULL) {
        // Allocate new context
        ctx = (table_selection_context_t*)malloc(sizeof(table_selection_context_t));
        memset(ctx, 0, sizeof(table_selection_context_t));

        // Initialize
        ctx->refcount = 1;
        ctx->is_valid = true;
        ctx->max_expanded = 16;  // Initial capacity
        ctx->expanded_tables = malloc(sizeof(hdlhashtable) * ctx->max_expanded);

        // Store in TLS
        pthread_setspecific(table_selection_key, ctx);
    } else {
        // Increment refcount
        atomic_fetch_add(&ctx->refcount, 1);
    }

    return ctx;
}
```

### Context Release

```c
/**
 * table_selection_release - Release reference to context
 *
 * @param ctx - Context to release (NULL-safe)
 *
 * Decrements refcount. If refcount reaches 0, frees all resources.
 */
void table_selection_release(table_selection_context_t *ctx);
```

**Pseudocode:**
```c
void table_selection_release(table_selection_context_t *ctx) {
    if (ctx == NULL) return;

    uint32_t old_count = atomic_fetch_sub(&ctx->refcount, 1);

    if (old_count == 1) {
        // Last reference, free resources
        if (ctx->selected_keys != NULL) {
            disposelist(ctx->selected_keys);
        }
        if (ctx->expanded_tables != NULL) {
            free(ctx->expanded_tables);
        }

        free(ctx);
        pthread_setspecific(table_selection_key, NULL);
    }
}
```

### Context Reset

```c
/**
 * table_selection_reset - Clear selection and cursor, keep expansion state
 *
 * @param ctx - Context to reset
 *
 * Use case: User calls table.clearSelection()
 */
void table_selection_reset(table_selection_context_t *ctx);

/**
 * table_selection_reset_full - Clear everything including expansion state
 *
 * @param ctx - Context to reset
 *
 * Use case: Switching to different table
 */
void table_selection_reset_full(table_selection_context_t *ctx);
```

---

## Expansion State Management

### Why Expansion State Matters

**Legacy Frontier Behavior:**
- Tables can contain nested tables (external values of type tableType)
- In table window, user can expand/collapse nested tables (like outline nodes)
- Row numbers depend on expansion state

**Example:**
```usertalk
workspace
  prefs (collapsed)
  data
    config (expanded)
      user
      system
  temp
```

**Row numbering when prefs collapsed, data.config expanded:**
1. workspace
2. workspace.prefs (collapsed, children hidden)
3. workspace.data
4. workspace.data.config (expanded)
5. workspace.data.config.user
6. workspace.data.config.system
7. workspace.temp

**If we expand prefs (has 2 children):**
1. workspace
2. workspace.prefs (expanded)
3. workspace.prefs.fontsize
4. workspace.prefs.theme
5. workspace.data
6. workspace.data.config (expanded)
7. workspace.data.config.user
8. workspace.data.config.system
9. workspace.temp

**Row numbers shifted by expansion!**

### Checking Expansion State

```c
/**
 * table_selection_is_expanded - Check if nested table is expanded
 *
 * @param ctx - Selection context
 * @param htable - Table to check
 * @return true if table is in expanded list
 */
boolean table_selection_is_expanded(table_selection_context_t *ctx, hdlhashtable htable);
```

**Pseudocode:**
```c
boolean table_selection_is_expanded(table_selection_context_t *ctx, hdlhashtable htable) {
    for (long i = 0; i < ctx->ct_expanded; i++) {
        if (ctx->expanded_tables[i] == htable) {
            return true;
        }
    }
    return false;
}
```

### Expanding/Collapsing Tables

```c
/**
 * table_selection_expand - Mark table as expanded
 *
 * @param ctx - Selection context
 * @param htable - Table to expand
 * @return true if newly expanded, false if already expanded
 */
boolean table_selection_expand(table_selection_context_t *ctx, hdlhashtable htable);

/**
 * table_selection_collapse - Mark table as collapsed
 *
 * @param ctx - Selection context
 * @param htable - Table to collapse
 * @return true if collapsed, false if wasn't expanded
 */
boolean table_selection_collapse(table_selection_context_t *ctx, hdlhashtable htable);
```

**Pseudocode for expand:**
```c
boolean table_selection_expand(table_selection_context_t *ctx, hdlhashtable htable) {
    // Already expanded?
    if (table_selection_is_expanded(ctx, htable)) {
        return false;
    }

    // Grow array if needed
    if (ctx->ct_expanded >= ctx->max_expanded) {
        ctx->max_expanded *= 2;
        ctx->expanded_tables = realloc(ctx->expanded_tables,
                                       sizeof(hdlhashtable) * ctx->max_expanded);
    }

    // Add to expanded list
    ctx->expanded_tables[ctx->ct_expanded++] = htable;
    return true;
}
```

---

## Row Number Calculation

### The Core Problem

**Row number calculation must account for:**
1. Base table entries (always visible)
2. Nested tables (visible only if parent expanded)
3. Recursion depth (nested within nested)

**Algorithm: Flat Iteration with Expansion Check**

```c
/**
 * table_selection_count_visible_rows - Count total visible rows
 *
 * @param ctx - Selection context
 * @param htable - Table to count
 * @return Total number of visible rows (1-based indexing)
 *
 * Algorithm:
 * 1. Start with immediate children count
 * 2. For each child that is an expanded table, recurse
 * 3. Sum up visible descendants
 */
long table_selection_count_visible_rows(table_selection_context_t *ctx,
                                        hdlhashtable htable);
```

**Pseudocode:**
```c
long table_selection_count_visible_rows(table_selection_context_t *ctx,
                                        hdlhashtable htable) {
    long count = 0;
    hdlhashnode nomad = (**htable).hfirstsort;

    while (nomad != nil) {
        count++;  // This entry is visible

        // Is this entry a nested table?
        if ((**nomad).val.valuetype == externalvaluetype) {
            hdlexternalvariable hv = (hdlexternalvariable)(**nomad).val.data.externalvalue;

            if ((**hv).id == idtableprocessor) {
                // This is a nested table
                hdlhashtable nested = (hdlhashtable)(**hv).variabledata;

                // Is it expanded?
                if (table_selection_is_expanded(ctx, nested)) {
                    // Add nested table's visible rows
                    count += table_selection_count_visible_rows(ctx, nested);
                }
            }
        }

        nomad = (**nomad).sortedlink;
    }

    return count;
}
```

### Finding Node at Row N

```c
/**
 * table_selection_get_node_at_row - Find hash node at 1-based row index
 *
 * @param ctx - Selection context
 * @param htable - Root table
 * @param row - 1-based row number
 * @param hnode_out - Output: hash node at that row
 * @param htable_out - Output: table containing that node
 * @return true if found, false if row out of bounds
 */
boolean table_selection_get_node_at_row(table_selection_context_t *ctx,
                                        hdlhashtable htable,
                                        long row,
                                        hdlhashnode *hnode_out,
                                        hdlhashtable *htable_out);
```

**Pseudocode:**
```c
boolean table_selection_get_node_at_row(table_selection_context_t *ctx,
                                        hdlhashtable htable,
                                        long row,
                                        hdlhashnode *hnode_out,
                                        hdlhashtable *htable_out) {
    long current_row = 0;
    hdlhashnode nomad = (**htable).hfirstsort;

    while (nomad != nil) {
        current_row++;  // Increment for this entry

        if (current_row == row) {
            // Found it!
            *hnode_out = nomad;
            *htable_out = htable;
            return true;
        }

        // Check if this is an expanded nested table
        if ((**nomad).val.valuetype == externalvaluetype) {
            hdlexternalvariable hv = (hdlexternalvariable)(**nomad).val.data.externalvalue;

            if ((**hv).id == idtableprocessor) {
                hdlhashtable nested = (hdlhashtable)(**hv).variabledata;

                if (table_selection_is_expanded(ctx, nested)) {
                    // Count nested rows
                    long nested_count = table_selection_count_visible_rows(ctx, nested);

                    if (current_row + nested_count >= row) {
                        // Row is inside nested table, recurse
                        long nested_row = row - current_row;
                        return table_selection_get_node_at_row(ctx, nested,
                                                               nested_row,
                                                               hnode_out,
                                                               htable_out);
                    }

                    current_row += nested_count;
                }
            }
        }

        nomad = (**nomad).sortedlink;
    }

    return false;  // Row out of bounds
}
```

---

## Multi-Selection Support

### Selection List Management

```c
/**
 * table_selection_add - Add key to selection
 *
 * @param ctx - Selection context
 * @param key - Key to add (bigstring)
 * @return true if added, false if already selected
 */
boolean table_selection_add(table_selection_context_t *ctx, bigstring key);

/**
 * table_selection_remove - Remove key from selection
 *
 * @param ctx - Selection context
 * @param key - Key to remove
 * @return true if removed, false if wasn't selected
 */
boolean table_selection_remove(table_selection_context_t *ctx, bigstring key);

/**
 * table_selection_is_selected - Check if key is selected
 *
 * @param ctx - Selection context
 * @param key - Key to check
 * @return true if in selection list
 */
boolean table_selection_is_selected(table_selection_context_t *ctx, bigstring key);

/**
 * table_selection_clear - Clear all selections
 *
 * @param ctx - Selection context
 */
void table_selection_clear(table_selection_context_t *ctx);
```

**Pseudocode for add:**
```c
boolean table_selection_add(table_selection_context_t *ctx, bigstring key) {
    // Already selected?
    if (table_selection_is_selected(ctx, key)) {
        return false;
    }

    // Create list if needed
    if (ctx->selected_keys == NULL) {
        if (!newlist(&ctx->selected_keys, false)) {
            return false;
        }
    }

    // Add to list
    if (!langpushliststring(ctx->selected_keys, key)) {
        return false;
    }

    ctx->ct_selected++;
    return true;
}
```

---

## Cursor Management

### Setting Cursor

```c
/**
 * table_selection_set_cursor - Set cursor to specific key
 *
 * @param ctx - Selection context
 * @param htable - Table containing key
 * @param key - Key to set cursor to
 * @return true if cursor set, false if key not found
 */
boolean table_selection_set_cursor(table_selection_context_t *ctx,
                                   hdlhashtable htable,
                                   bigstring key);
```

**Pseudocode:**
```c
boolean table_selection_set_cursor(table_selection_context_t *ctx,
                                   hdlhashtable htable,
                                   bigstring key) {
    // Find the hash node
    hdlhashnode hnode;
    if (!hashlookup(key, &hnode, htable)) {
        return false;  // Key not found
    }

    // Update cursor
    copystring(key, ctx->cursor_key);
    ctx->cursor_node = hnode;
    ctx->current_table = htable;

    // Calculate flat index (expensive, but needed for goto)
    ctx->cursor_flat_index = table_selection_get_row_for_key(ctx, htable, key);

    return true;
}
```

### Getting Cursor

```c
/**
 * table_selection_get_cursor - Get current cursor position
 *
 * @param ctx - Selection context
 * @param key_out - Output: cursor key (empty if no cursor)
 * @return true if cursor set, false if no cursor
 */
boolean table_selection_get_cursor(table_selection_context_t *ctx,
                                   bigstring key_out);
```

---

## Integration with Table Context

### Relationship to table_context_t

**table_context_t (in table structure):**
- Version tracking (increments on mutations)
- Mutation metadata (last change time/type)
- Dirty flag (unsaved changes)
- Stored IN table structure, persisted to database

**table_selection_context_t (thread-local):**
- Selection state (which entries marked)
- Cursor position (current entry)
- Expansion state (which nested tables expanded)
- NOT stored in table, NOT persisted

**Key Difference:**
- `table_context_t` = persistent object metadata
- `table_selection_context_t` = ephemeral navigation state

### No Direct Linkage

Selection context does NOT reference table context directly. They are orthogonal:
- Table mutations (via table.assign, etc.) bump `table_context_t.version`
- Navigation (via table.goto, etc.) updates `table_selection_context_t` state
- No cross-coupling

---

## Thread Safety (Phase 3 vs Phase 6+)

### Phase 3 (Current)
- Single-threaded execution assumed
- Thread-local storage prevents accidental sharing
- No locking needed
- Each script execution has isolated context

### Phase 6+ (Multi-User Collaborative)
- Multiple user threads with own selection contexts
- Object-level locking handled by `table_context_t`
- Selection context remains thread-local (no sharing)
- User A's selection invisible to User B

**Example Multi-User Scenario:**
```
User A thread:
  - Acquires table_selection_context_t (thread-local)
  - Navigates to row 5
  - Selects rows 5, 6, 7
  - Calls table.getSelection() → returns [row5, row6, row7]

User B thread:
  - Acquires DIFFERENT table_selection_context_t (own thread)
  - Navigates to row 10
  - Selects row 10
  - Calls table.getSelection() → returns [row10]

No interference!
```

---

## Initialization and Cleanup

### Module Initialization

```c
/**
 * table_selection_init - Initialize table selection subsystem
 *
 * Called once at process startup.
 * Sets up thread-local storage key.
 */
void table_selection_init(void);
```

**Implementation:**
```c
void table_selection_init(void) {
    // Create TLS key
    pthread_key_create(&table_selection_key, table_selection_thread_cleanup);
}

// Cleanup callback when thread exits
static void table_selection_thread_cleanup(void *ctx) {
    table_selection_context_t *context = (table_selection_context_t*)ctx;
    if (context != NULL) {
        table_selection_release(context);
    }
}
```

### Script Completion Cleanup

**Question:** When should selection context be reset?

**Options:**
1. **Never** (persist across script executions) - matches windowed mode
2. **After each script** (clean slate for each execution)
3. **On table switch** (reset when changing tables)

**Recommendation:** Option 3 (reset on table switch)
- Matches user expectation (selection tied to table)
- Prevents stale selection from old table
- Allows multi-step scripts to preserve selection

---

## Error Handling

### Common Error Conditions

```c
// Row number out of bounds
if (row < 1 || row > total_rows) {
    return langerror(BIGSTRING("\x18" "Row number out of range"));
}

// No cursor set
if (ctx->cursor_key[0] == 0) {
    return langerror(BIGSTRING("\x10" "No cursor set"));
}

// Table not found
if (!hashlookup(key, &hnode, htable)) {
    return langerror(BIGSTRING("\x0E" "Key not found"));
}

// Invalid direction
if (dir != up && dir != down) {
    return langerror(BIGSTRING("\x2A" "Invalid direction (use up or down)"));
}
```

### Defensive Checks

```c
// Always validate context
assert(ctx != NULL);
assert(ctx->is_valid);

// Always validate table handle
assert(htable != NULL);
assert(**htable != NULL);

// Check for recursion overflow
if (ctx->iteration_depth > 100) {
    return langerror(BIGSTRING("\x1A" "Table nesting too deep"));
}
```

---

## Testing Strategy

### Unit Tests (C-level)

```c
// Test context lifecycle
test_selection_context_acquire_release()
test_selection_context_thread_isolation()

// Test expansion state
test_expansion_add_remove()
test_expansion_is_expanded()
test_expansion_array_growth()

// Test row counting
test_count_visible_rows_flat()
test_count_visible_rows_nested()
test_count_visible_rows_deep_nesting()

// Test node lookup
test_get_node_at_row_first()
test_get_node_at_row_middle()
test_get_node_at_row_last()
test_get_node_at_row_nested()
test_get_node_at_row_out_of_bounds()

// Test multi-selection
test_selection_add_remove()
test_selection_is_selected()
test_selection_clear()
```

### Integration Tests (UserTalk)

```usertalk
// Test basic navigation
local (t)
lang.new(tableType, @t)
t.a = 1; t.b = 2; t.c = 3

table.goto(@t, 1)  // Move to first
assert(table.getCursor() == @t.a)

table.goto(@t, 2)  // Move to second
assert(table.getCursor() == @t.b)

// Test nested table expansion
lang.new(tableType, @t.nested)
t.nested.x = 10; t.nested.y = 20

// Initially collapsed (3 visible rows)
assert(table.countVisibleRows(@t) == 4)  // a, b, c, nested

// Expand nested table (5 visible rows)
table.expand(@t.nested)
assert(table.countVisibleRows(@t) == 6)  // a, b, c, nested, nested.x, nested.y

// Row numbers shift when expanded
table.goto(@t, 5)  // Should be nested.x
assert(table.getCursor() == @t.nested.x)
```

---

## Performance Considerations

### Row Calculation Cost

**Problem:** Calculating row numbers requires tree traversal.

**Mitigation:**
- Cache cursor flat index when cursor changes
- Invalidate cache on expansion/collapse
- Use direct node pointers when possible

### Expansion State Lookup

**Problem:** Linear search through expanded tables array.

**Mitigation:**
- Most tables have few expanded nested tables (<10)
- Linear search acceptable for small arrays
- Phase 6+: Consider hash table for large expansion sets

---

## Files to Create/Modify

### New Files
1. `Common/headers/headless_selection.h` - Public API
2. `Common/source/headless_selection.c` - Implementation
3. `tests/headless_selection_tests.c` - Unit tests

### Modified Files
1. `Common/source/tableverbs.c` - Add headless verb implementations
2. `tests/headless_table_verbs.c` - Remove stubs (use production code)

---

## Summary

This architecture provides:
- ✅ Thread-local selection context (multi-user ready)
- ✅ Expansion state tracking (nested table support)
- ✅ Multi-selection support (via table.select)
- ✅ 1-based row indexing (backward compatible)
- ✅ Clean separation of persistent vs ephemeral state
- ✅ Foundation for Phase 6+ collaborative editing

**Next Steps:**
1. Implement detailed verb specifications (5 additional markdown files)
2. Code implementation (headless_selection.c)
3. Verb binding (tableverbs.c updates)
4. Testing (unit + integration)

---

## Deferred Testing & Future Work

The following testing and optimization work has been deferred to later phases:

### Phase 2: Hash Table Integration Tests
- **Status:** Deferred from Phase 1
- **Rationale:** Phase 1 tests use dummy pointers for infrastructure validation only
- **Required Work:**
  - Setting cursor on actual hash nodes
  - Row counting with real nested tables
  - Multi-selection with real table entries
  - Expansion state affecting row indexing
  - Verify iteration_depth handling with deeply nested tables
- **Phase:** Phase 2 (table verb implementation)

### Phase 5: Performance Testing
- **Status:** Deferred from Phase 1
- **Rationale:** Current algorithms are O(n) with recursion - acceptable for typical use cases
- **Required Work:**
  - Benchmark `table_selection_count_visible_rows()` with large tables (>1000 entries)
  - Benchmark `table_selection_get_node_at_row()` for repeated lookups (rendering scenarios)
  - Benchmark `table_selection_find_in_list()` with large selection sets (>100 items)
  - Profile deep nesting scenarios (10+ levels)
- **Performance Assumptions:**
  - Typical table sizes: <1000 entries
  - Typical selection sizes: <100 items
  - Typical nesting depth: <10 levels
- **Potential Optimizations (if profiling shows issues):**
  - Cache visible row counts for expanded tables
  - Use hash table for selection list (instead of linear search)
  - Index caching for repeated row lookups
- **Phase:** Phase 5 (integration testing)

### Phase 6: Multi-Threaded Stress Testing
- **Status:** Deferred from Phase 1
- **Rationale:** Thread-local storage ensures isolation, but concurrent editing needs validation
- **Required Work:**
  - Concurrent operations on different tables
  - Concurrent operations on same table from different threads
  - Reference counting correctness under load
  - Memory leak detection with thread churn
- **Phase:** Phase 6 (collaborative ODB support)

---

**Document Updated:** 2025-12-30 (added deferred testing section)
