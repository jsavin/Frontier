# Frontier Outline Structure

## Critical Architectural Fact

**All new outlines start with a single empty summit headline.**

This is a fundamental design decision in Frontier that affects how outline operations work.

## Structure Details

When you create a new outline:
```usertalk
lang.new(outlineType, @outline);
```

The outline is initialized with:
- **1 empty headline** at the summit level
- Text: "" (empty string)
- `flexpanded`: true
- This headline serves as the root/anchor for the outline structure

## First Insert Behavior

When you first insert into a new outline, the behavior depends on the operation:

### Using `op.insert(text, direction)`

`op.insert()` **does NOT replace** the empty summit - it **adds** a new headline:

```usertalk
lang.new(outlineType, @outline);
target.set(@outline);
op.insert("First Item", down);   // Adds BELOW the empty summit
```

**Result:**
- Line 1: Empty headline (original summit)
- Line 2: "First Item"

### Using `op.setLineText(text)`

To replace the empty summit instead of adding below it:

```usertalk
lang.new(outlineType, @outline);
target.set(@outline);
op.setLineText("First Item");     // Replaces the empty summit's text
```

**Result:**
- Line 1: "First Item" (replaced empty summit)

### Deleting the Empty Summit

You can explicitly delete the empty summit:

```usertalk
lang.new(outlineType, @outline);
target.set(@outline);
op.deleteLine();                  // Deletes empty summit
op.insert("First Item", down);    // Now creates new summit
```

**Result:**
- Line 1: "First Item" (new summit, no empty headline)

## Example: Three-Node Outline

```usertalk
lang.new(outlineType, @outline);
target.set(@outline);
op.insert("Parent", down);         // Line 2 (empty summit still at line 1)
op.insert("Child", right);         // Line 3 (child of line 2)
op.go(left, 1);                    // Back to Parent
op.expand(1);                      // Expand to show Child
```

**Structure:**
- Line 1: Empty headline (summit, always expanded)
- Line 2: "Parent" (summit, expanded to show child)
- Line 3: "Child" (child of line 2)

**Total lines:** 3 (not 2)

## Impact on Verb Implementations

### Line Counting
When counting lines or iterating, remember the empty summit counts as line 1.

### Expansion State
`op.getExpansionState()` returns line numbers of expanded headlines.

For the example above:
```usertalk
op.getExpansionState()  // Returns: {2}
```

This means "line 2 is expanded" (showing its children).

### Cursor and Collapsed Nodes - Critical Invariant ⚠️

**The cursor can NEVER be on a child of a collapsed node.**

This is a fundamental constraint in Frontier's outline model:
- Collapsed nodes hide their children from the navigable structure
- The cursor must always be on a **visible (accessible)** node
- When a parent is collapsed while the cursor is on a child, **the cursor automatically moves to the parent**

**Implications for State Restoration:**

When `op.setExpansionState()` restores expansion state:
1. The expansion state is restored (nodes expand/collapse as saved)
2. The cursor position is NOT changed by the operation itself
3. **BUT** if the cursor ends up on a child of a now-collapsed node, it auto-moves to the first visible ancestor

Example:
```usertalk
op.insert("Parent", down);
op.insert("Child", right);
op.expand(1);           // Parent expanded, Child visible
local(cursor = op.getCursor());  // Cursor on Child
local(state = op.getExpansionState());  // Save state with Parent expanded

op.collapse();          // Parent collapsed, cursor auto-moves to Parent (Child no longer accessible)
op.setExpansionState(state);  // Restore: Parent expands again
op.setCursor(cursor);   // Cursor moves back to Child (now accessible)
```

**Key insight:** Expansion state is independent of cursor position, but cursor position must always satisfy the visibility constraint.

### Navigation
`op.firstSummit()` moves to the first summit headline (the empty one if it exists).

To get to the first "real" content:
```usertalk
op.firstSummit();     // Now at empty summit (line 1)
op.go(down, 1);       // Move to first real content (line 2)
```

## Historical Context

This design dates to the original Frontier implementation and is preserved for compatibility. The empty summit:
- Provides a consistent root node for the outline tree structure
- Ensures there's always a valid cursor position
- Simplifies internal traversal algorithms (always has a parent)

## Testing Implications

When writing tests, account for the empty summit:

```yaml
# ❌ WRONG - Expects 2 lines
- name: "Count nodes in simple outline"
  script: |
    lang.new(outlineType, @o);
    target.set(@o);
    op.insert("Item 1", down);
    return op.countLines()
  expected_result: "2"  # Wrong! Should be 2 (empty summit + item)

# ✅ CORRECT
  expected_result: "2"  # Correct (empty summit + item)
```

## Related Documentation

- `Common/source/opinit.c` - Outline initialization (`opnewsummit()`)
- `Common/source/opstructure.c` - Insert/delete operations
- `planning/phase3/OUTLINE_EMPTY_SUMMIT_FIX.md` - Proposed changes to this behavior
