# Outline Empty Summit Fix

## Problem

When creating outlines via `lang.new(outlineType, @outline)` followed by `op.insert()`, the initial empty summit node is not removed, causing:

1. **Extra phantom node** in traversal (3 nodes instead of 2)
2. **Incorrect `opsubheadsexpanded()` values** due to `flexpanded` flag inheritance

## Root Cause

When `lang.new(outlineType, @outline)` creates a new outline:
1. `newoutlinerecord()` calls `opnewsummit()` which creates an empty summit node
2. The empty summit has empty headstring `""`
3. When `op.insert("Parent 1", down)` is called, it inserts BELOW the empty summit
4. The empty summit remains in the structure

**Actual outline structure**:
```
1. Empty summit (headstring="", flexpanded=true)
2. Parent 1 (headstring="Parent 1", flexpanded=true, inherited from empty summit)
3. Child 1 (headstring="Child 1", flexpanded=true, inherited from Parent 1)
```

**Expected outline structure**:
```
1. Parent 1 (headstring="Parent 1")
2. Child 1 (headstring="Child 1", child of Parent 1)
```

## Why opsubheadsexpanded() Returns Wrong Values

```c
boolean opsubheadsexpanded (hdlheadrecord hnode) {
    register hdlheadrecord hkid = (**hnode).headlinkright;

    if (hkid == hnode) /*no subheads*/
        return (false);

    return ((**hkid).flexpanded);  /* Returns CHILD's flexpanded flag */
}
```

This function returns the `flexpanded` flag of the **first child**, not whether this node's children are visible.

When nodes are inserted:
- `opdepositdown()` (line 418): child inherits `flexpanded` from parent
- `opdepositright()` (line 484/491): child inherits `flexpanded` from parent or sibling

So:
- Empty summit → `flexpanded=true` (from `newoutlinerecord` line 1537)
- Parent 1 → `flexpanded=true` (inherited from empty summit)
- Child 1 → `flexpanded=true` (inherited from Parent 1)

Thus:
- `opsubheadsexpanded(Empty summit)` → checks Parent 1's `flexpanded` → returns `true`
- `opsubheadsexpanded(Parent 1)` → checks Child 1's `flexpanded` → returns `true` ❌ WRONG (Child has no children)

## Existing Pattern in GUI Frontier

From `opverbs.c` line 3113:
```c
getheadstring ((**ho).hsummit, bsheadstring);

if (equalstrings (bsheadstring, emptystring)) /*if it's "", delete it*/
    opdelete ();
```

GUI Frontier manually deletes empty summit nodes after certain operations.

## Proposed Fix Options

### Option 1: Delete empty summit on first insert (Preferred)

Modify `opinsertheadline_ctx()` or `opinserthandle_ctx()` to check if inserting into an outline with only an empty summit, and replace it instead of inserting below it.

**Implementation**:
```c
/* In opinsertheadline_ctx or opinserthandle_ctx */
hdloutlinerecord ho = op_get_outlinedata();
hdlheadrecord hsummit = (**ho).hsummit;
bigstring bshead;

/* Check if outline has only empty summit */
if ((**hsummit).headlinkdown == hsummit &&  /* No siblings */
    (**hsummit).headlinkright == hsummit) {  /* No children */

    getheadstring(hsummit, bshead);

    if (stringlength(bshead) == 0) {  /* Empty string */
        /* Replace empty summit instead of inserting */
        if (!opsetheadtext(hsummit, hstring)) {
            return false;
        }
        opmoveto(hsummit);
        return true;
    }
}

/* Normal insertion logic... */
```

**Pros**:
- Fixes root cause immediately
- No phantom nodes in traversal
- Matches user expectations

**Cons**:
- Changes insertion semantics slightly

### Option 2: Skip empty summit in traversal

Modify `opbumpflatdown()` to skip nodes with empty headstring.

**Pros**:
- Doesn't change insertion logic
- Transparent to user

**Cons**:
- Empty summit still exists in structure
- `opsubheadsexpanded()` still returns wrong values

### Option 3: Don't create empty summit for new outlines

Modify `newoutlinerecord()` to not call `opnewsummit()`, and handle NULL summit case.

**Pros**:
- Most efficient (no extra node)

**Cons**:
- Major architectural change
- Many functions assume summit always exists
- High risk of breaking existing code

## Recommendation

**Use Option 1**: Delete empty summit on first insert.

This is the least invasive fix that solves both issues (phantom node and incorrect `opsubheadsexpanded()` values) while matching user expectations.

## Implementation Plan

1. Add helper function `op_is_empty_outline()` to check if outline has only empty summit
2. Modify `opinsertheadline_ctx()` to call helper and replace empty summit on first insert
3. Add test to verify fix
4. Document behavior in op verb documentation

## Test Case

```usertalk
lang.new(outlineType, @outline);
target.set(@outline);
op.insert("Parent 1", down);
op.insert("Child 1", right);
op.go(left, 1);
op.expand(1);
op.firstSummit();

local (ct = 1);
loop {
  if not op.go(flatdown, 1) { break };
  ct = ct + 1;
  if ct > 10 { break }
};

/* Should be 2 (Parent 1 + Child 1), not 3 */
return ct == 2
```

## References

- `newoutlinerecord()` - Common/source/opops.c:1496
- `opnewsummit()` - Common/source/opops.c:1468
- `opdepositdown()` - Common/source/opstructure.c:388
- `opdepositright()` - Common/source/opstructure.c:453
- `opsubheadsexpanded()` - Common/source/opops.c:243
- GUI empty summit deletion - Common/source/opverbs.c:3113
