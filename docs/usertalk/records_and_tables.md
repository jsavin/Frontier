# Records, Tables, Lists, and Addresses

Pulled in when: you hit `Can't find a sub-table named X`, you need to walk an unknown structure, or you're not sure what a verb returns.

This is the doc that disambiguates the four container/reference types and the access syntax for each. The primer's "five idioms" section is the fast-path; this is the full picture.

---

## Type taxonomy

All UserTalk types are defined in `system.compiler.language.constants` as `string4` (4-byte OSType) scalars whose names end in `Type`. The four shapes you'll touch most:

| Constant | OSType | Shape | When you'll see it |
|---|---|---|---|
| `tableType` | `'tabl'` | Named-member container, dotted-name access, may be persisted in ODB | The workhorse. `system.*`, `workspace.*`, anything in `.root` files |
| `recordType` | `'reco'` | Insertion-ordered, ordinal-access, no dotted-name read | Rare. Returned by some verbs that emit ordered key/value sequences |
| `listType` | `'list'` | Positional array, value-iteration | Function args (sometimes), parsed sequences |
| `addressType` | `'addr'` | Reference to a location in the ODB | Output parameters, "address of" via `@`, dereference with `^` |

You can always check at runtime with `typeof (value)` — but be aware that **the protocol JSON serialization** maps OSType codes to strings, so `result_type` in protocol replies shows up as `"unknown"` for `'reco'` and `'tabl'`, with the OSType in `result.value`.

---

## Tables (`tableType`)

The default container in UserTalk. Most of `system.*` and all of `workspace.*` are tables.

### Literal & creation

Tables don't have a literal syntax — you create one with `new ()`:

```
new (tableType, @examples.myTable);
examples.myTable.alpha = 1;
examples.myTable.beta = "two"}
```

Members are added by simple assignment to a previously nonexistent path.

### Member access

Dotted-name addressing is the normal path:

```
examples.myTable.alpha           // ✓ read by name
examples.myTable.alpha = 99      // ✓ write by name
```

When the name is dynamic, comes from a variable, or shadows a global/keyword, use bracket form:

```
local (n = "alpha");
local (val = examples.myTable.[n])         // ✓ dynamic name from variable
local (typVal = constants.["stringType"])     // ✓ bracket form to avoid global collision
```

### Uniqueness

Member names **must be unique** within a table. (Exception: tables holding XML representations have special name-collision handling — see `usertalk-engineer.md` if you're touching XML.)

### Iteration

Two canonical patterns, situationally chosen:

```
// Pattern 1: single-layer walk. Visitor is the ADDRESS of each member.
for adrMember in @examples.myTable {
    local (val = adrMember^);                  // dereference for value
    local (name = nameOf (adrMember^));        // member name (NOT nameOf(adrMember))
    log (name + " = " + val)}

// Pattern 2: index walk. Useful for tree-walking or when you need the position.
for i = 1 to sizeOf (examples.myTable) {
    local (adrSubitem = @examples.myTable[i]);
    ...}
```

**Critical**: in pattern 1, the iteration target is the **address** of the table (`@examples.myTable`), not the table value (`examples.myTable`). Passing the value gives you `This operation is not supported for table values.`

**Why `nameOf (adrMember^)` and not `nameOf (adrMember)`**: inside the loop, `adrMember` is the local variable holding an address. `nameOf (adrMember)` returns the local variable's name (the string `"adrMember"`). `nameOf (adrMember^)` dereferences first and returns the name of the **member** in the parent table.

### Sort order — headless ≠ legacy Mac

Legacy Mac Frontier: tables auto-sort alphabetically by member name unless a script explicitly sets a non-alpha sort order. Sort order is per-table, per-thread.

Headless build: tables iterate in **insertion order**. No alpha-sort applied.

**Implication**: integration tests that depend on iteration order may pass in headless and fail in production Mac (or vice versa). When iteration order matters for a test, either sort explicitly or assert on a sorted form.

### Polymorphic members

Table members can be any UserTalk type — strings, numbers, scripts, outlines, nested tables, records, addresses, anything. Don't assume member values are scalars when walking unknown structure; `typeof (adrMember^)` to be sure.

---

## Records (`recordType`)

The "I want preserved insertion order" container. Used infrequently — tables are the default.

### Literal & creation

```
local (r = {"key1": "value1", "key2": "value2"})       // ✓ curly braces, colon-separated
new (recordType, @examples.myRecord)                 // ✓ programmatic creation
```

**Curly braces**, not square brackets. Square brackets aren't a UserTalk syntax at all. (The agent definition's earlier `["k": "v"]` notation is incorrect.)

### Member access — the gotcha

**Records have no dotted-name access.** `r.key1` does NOT read the value named `"key1"`. There is no syntax for "get record member by name." Access is ordinal-only, via iteration:

```
for x in r {
    log (x)}                                  // x is set to each VALUE in insertion order
```

The visitor is the **value** (not an address). Order is **preserved** from how the record was constructed.

If you need name-keyed access, **don't use records — use tables**.

### Insertion order

Records preserve the order in which members were added. Iteration follows that order:

```
local (r = {"zebra": 1, "apple": 2, "middle": 3});
for x in r {log (x)}                          // logs: 1, 2, 3 (insertion order)
```

### Mutation — the second gotcha

Records are mutable (in the UserTalk sense — you can change them), but **you cannot assign a record member by name**. There's no `r.key = value` syntax that works on records.

To add a member, use the `+` operator:

```
r = r + {"newKey": "newValue"}                // ✓ adds member
```

`+` also merges two records.

**Name-collision behavior of `+` (quirky)**:

```
local (r = {"a": 1});
r = r + {"a": 99};                             // SILENT NO-OP — r still has "a" = 1
```

Adding a member whose name already exists is a **silent no-op**: size doesn't change, value doesn't update. This appears to be both legacy Mac and headless behavior. Likely a historical bug that was never caught because records were rarely used in production at UserLand.

**If you need to update an existing key**, you cannot do it with records. Convert to a table or recreate the record from scratch.

### Duplicate-name literals — unspecified

In headless, `{"a": 1, "a": 2}` creates a record with **two members both named `"a"`** (size 2, iterates both values). On legacy Mac, names "must be" unique. This is divergent and unspecified; don't rely on it.

### Polymorphic members

Like tables and lists: any UserTalk type in any slot.

### When to use a record vs a table

Pick record when:
- You need members in **insertion order** (no alpha-sort, no thread-local sort state)
- You're mirroring an external ordered key/value structure (JSON-ish input, ordered headers)

Pick table when:
- You need to read/update by name (the common case)
- You're persisting to the ODB
- You need name-uniqueness enforcement (somewhat)

When in doubt, use a table.

---

## Lists / arrays (`listType`)

Positional array. The third container type.

### Literal & creation

```
local (xs = {1, 2, 3});                       // ✓ curly braces, comma-separated
local (mixed = {"a", 42, true, @workspace});  // ✓ mixed types fine
```

Same curly-brace syntax as records, but no `key:` prefix — bare values separated by commas.

The parser distinguishes record from list by whether literals have `"key":` prefix on each element.

### Iteration

```
for x in xs {
    log (x)}                                   // x is the VALUE in order
```

The visitor is the **value** (not an address, unlike table iteration). Order is positional.

### Index access

Lists are 1-indexed for `sizeOf` and the index form, similar to tables. (The agent definition's claim of 0-indexed lists may not match current headless behavior — verify before relying on indexing.)

### Polymorphic members

Same as records and tables: any type, any slot.

---

## Addresses (`addressType`)

A reference to a location in the ODB, not the value at that location.

### Creating an address

```
local (adr = @examples.foo.bar);                // address-of operator
local (adr2 = @examples.myTable.alpha);
```

`@x.y.z` builds an address value naming `x.y.z`. **It does not resolve the path.** The address is well-formed even if `x.y.z` doesn't exist.

### Dereferencing

```
local (val = adr^);                            // get the value at this address
```

If the address doesn't point to anything, `adr^` errors at runtime — `Can't evaluate the expression because the name X hasn't been defined.`

### `defined` on addresses — what it really means

`defined ()` checks **address validity**, not target presence. An address is valid if either (1) it points to a real object, OR (2) the parent path exists, making the address a creatable location.

```
defined (@examples.foo)              // true if examples.foo points to a real object
defined (@examples.notYetCreated)    // true if examples exists — even if .notYetCreated doesn't
defined (@bogus.parent.leaf)         // false — parent path doesn't resolve
defined (examples.foo.bar)           // ✓ resolves path; runtime error if missing — wrap in try{...}
defined (adr^)                       // ✓ dereferences; true iff a real object lives there now
```

When you want "is there actually something here right now?" use `defined (adr^)`. When you want "is it OK to write to this address?" use `defined (@adr)`. See issue #625 for the docs-clarification follow-up.

### What's at the end of an address?

When `adr = @examples.foo.bar`:
- `system` and `foo` are **always tables** (they have sub-paths through them)
- `bar` can be **any UserTalk type** — table, record, list, string, number, script, outline, etc.

Always `typeof (adr^)` before assuming what kind of value you're holding.

### Why verbs take addresses

Output parameters: a verb that writes back to its caller's variable takes the variable's address:

```
local (result);
dialog.ask ("What's your name?", @result);   // verb writes "Jake" into result
log (result)
```

This is also how some verbs avoid copying large values — they take the address of the source and operate in place.

---

## Quick reference: which form for which type?

| You have… | Type | Read named member | Iterate |
|---|---|---|---|
| A table at `@t` | `tableType` | `t.name` or `t.[var]` | `for adrM in @t { adrM^ }` |
| A record value `r` | `recordType` | **not possible by name** | `for x in r { ... }` (x is value) |
| A list value `xs` | `listType` | `xs[i]` | `for x in xs { ... }` (x is value) |
| An address `adr` | `addressType` | `adr^` then continue | `adr^` to get the value, then iterate per its type |

---

## See also

- `CLAUDE_PRIMER.md` — the entry-point primer; Section 2 covers the table/list iteration idioms at a glance
- `values_and_types.md` — full type taxonomy reference (stub; expand as topics come up)
- `usertalk-engineer.md` (agent def) — comprehensive language reference loaded by the sub-agent; **note**: the records section there has errors (square-bracket literal syntax) being corrected
- `feedback_usertalk_comments.md` (in memory) — `«…»` parse errors in yaml tests
