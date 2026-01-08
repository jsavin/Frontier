# Frontier Outline Concepts

## Overview

Outlines are hierarchical data structures in Frontier that organize information in a tree-like format. This document explains key concepts and behaviors that are essential for working with Frontier's outline implementation.

---

## Outline Minimal State

**Core Principle:** Every outline maintains at least one node at all times. This is the "minimal state" - an outline can never be completely empty.

### Implications

1. **Creation**: When you create a new outline with `lang.new(outlineType, @var)`, it starts with one empty node
2. **Deletion**: When you delete the only/last node in an outline, a new blank node is automatically created
3. **Validation**: Code that works with outlines can always assume at least one node exists

### Example Behavior

```usertalk
lang.new(outlineType, @outline)
target.set(@outline)
op.insert("Only node", down)
op.deleteLine()              // Deletes "Only node"
return op.countSummits()      // Returns 1 (new blank node created)
```

---

## Cursor Movement After Deletion

When `op.deleteLine()` deletes a node (and all its children), the cursor must move to a valid location. The movement rules depend on the context:

### Top-Level Nodes

**Case 1: Deleting a node with a node above it**
- Cursor moves to the node above
- This applies even if the node above has children (sub-headings)

**Case 2: Deleting the only top-level node**
- A new blank node is created (minimal state)
- Cursor remains on this new blank node

### Nodes Under Sub-Headings

**Case 1: Deleting last node with a sibling above**
- Cursor moves to the sibling node above

**Case 2: Deleting last node without a sibling**
- Cursor moves to the parent node

### Visual Example

```
Before deletion:              After deleting "Child 2":
  Parent                        Parent
    Child 1                       Child 1
    Child 2  ← cursor               ← cursor moves here
```

```
Before deletion:              After deleting "Only Child":
  Parent                        Parent  ← cursor moves here
    Only Child  ← cursor
```

---

## Node Structure

Each outline node (headline) contains:

- **Text content**: The headline text (can be empty)
- **Children**: Zero or more sub-headings
- **Expansion state**: Whether children are visible (expanded) or hidden (collapsed)
- **Level**: Indentation level (1 = top-level, 2 = first indent, etc.)

### Special Node Types

**Summit Node**: A top-level node (level 1)
- Every outline has at least one summit (minimal state)
- Use `op.countSummits()` to count top-level nodes

**Empty Node**: A node with zero-length text
- Created when deleting the only node
- Part of outline minimal state

---

## Headless Mode Considerations

In headless CLI mode (no GUI), outline operations work differently than in the GUI:

### Target Management

- GUI mode: Current outline is the frontmost window
- Headless mode: Must explicitly set target with `target.set(@outline)`

### Display Refresh

- GUI mode: Display automatically refreshes after operations
- Headless mode: No display, so cursor movement logic must be explicit

### Empty Node Handling

The `op.deleteLine()` implementation includes special logic for headless mode:

```c
/* Post-delete cursor handling for headless mode:
 * opdeleteline() may leave cursor on empty root summit node (minimal state).
 * In GUI mode, display refresh handles this. In headless mode,
 * we must explicitly move to next valid node. */
```

This ensures consistent behavior between GUI and headless modes.

---

## Common Pitfalls

### Assuming Empty Outlines

❌ **Wrong**: Checking if outline is "empty" by comparing `op.countSummits() == 0`
✅ **Right**: An outline always has at least one summit (minimal state)

### Ignoring Cursor Movement

❌ **Wrong**: Assuming cursor stays in place after `op.deleteLine()`
✅ **Right**: Cursor moves according to deletion rules (see above)

### Not Handling Minimal State

❌ **Wrong**: Deleting last node and assuming outline is destroyed
✅ **Right**: Minimal state creates new blank node automatically

---

## Related Documentation

- **[Verb Implementation Guide](VERB_IMPLEMENTATION_GUIDE.md)** - Implementing op verbs
- **[Testing Guide](TESTING_GUIDE.md)** - Testing outline operations
- **Planning**: `planning/phase3/op_verb_implementation_plan.md` - Op verb roadmap

---

## References

**Implementation Files:**
- `tests/headless_op_verbs.c` - Op verb implementations (see op.deleteLine)
- `Common/source/opverbs.c` - Legacy op verb implementations
- `Common/headers/op.h` - Outline data structures

**Test Cases:**
- `tests/integration/test_cases/op_verbs.yaml` - Integration tests demonstrating behaviors

---

**Last Updated**: 2026-01-07
**Applies To**: Frontier v7 (headless CLI implementation)
