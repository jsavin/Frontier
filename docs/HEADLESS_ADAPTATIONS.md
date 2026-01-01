# Headless Mode Adaptations

**Status**: Living document
**Created**: 2026-01-01
**Maintainer**: Verb binding workstream

## Overview

Frontier's headless mode requires adaptations for verbs and operations that depend on UI, resource files, or other windowed-mode infrastructure. This document captures patterns for making Frontier functionality work in headless environments.

---

## Table of Contents

1. [Serialization: pack() vs Processor-Specific Pack Verbs](#serialization-pack-vs-processor-specific-pack-verbs)
2. [Resource File Dependencies](#resource-file-dependencies)
3. [UI-Dependent Operations](#ui-dependent-operations)
4. [Quick Reference](#quick-reference)

---

## Serialization: pack() vs Processor-Specific Pack Verbs

### The Problem

Many processors have specific pack verbs that write to application resource files:
- `table.packtable(table, resourceName)` - writes HASH resource to file
- `op.packtable(outline, resourceName)` - writes LAYO resource to file
- Similar patterns in other processors

These verbs **fail in headless mode** because:
1. No application resource file exists
2. `filewriteresource()` calls segfault or error
3. Resource manager APIs unavailable

### The Solution: Use pack() Builtin

The `pack()` builtin (`system.compiler.language.builtins.pack`) is a **generic abstraction** that:
- Works with **any** external type (tables, outlines, scripts, WPText, etc.)
- Returns binary data **in-memory** instead of writing to files
- Is fully **headless-compatible** ✓

### Syntax

```usertalk
// ❌ WRONG - Headless segfault
table.packtable(@myTable, "resourceName")

// ✅ CORRECT - Works in headless
pack(myTable, @binaryData)
// binaryData now contains packed table as binary type
```

### How It Works

**UserTalk Domain Architecture**:
```
pack() builtin (token=6)
    ↓
langpackverb() in langpack.c
    ↓
langpackvalue() - type-agnostic packing
    ↓
Type-specific packers:
  - hashpacktable() for tables
  - oppackoutline() for outlines
  - etc.
```

**Key Insight**: Processor-specific pack verbs (like `table.packtable`) are **low-level operations** that delegate from the `pack()` abstraction. In headless mode, use the abstraction directly.

### Examples

#### Packing a Table

```usertalk
// Create and populate table
lang.new(tableType, @myTable);
myTable.name = "Frontier";
myTable.version = 7;

// Pack to binary
pack(myTable, @packedData);

// Verify type
return typeof(packedData);  // Returns "data"
```

#### Packing an Outline

```usertalk
// Create outline
lang.new(outlineType, @myOutline);
// ... populate outline ...

// Pack to binary
pack(myOutline, @packedOutline);
```

#### Round-Trip Pack/Unpack

```usertalk
// Original table
lang.new(tableType, @original);
original.key = "value";

// Pack it
pack(original, @packed);

// Unpack to new table
unpack(packed, @restored);

return restored.key;  // Returns "value"
```

### Integration Testing Pattern

When testing table operations, use `pack()` for serialization tests:

```yaml
# tests/integration/test_cases/table_verbs.yaml

- name: "pack table - simple table"
  description: "Pack table to binary using pack() builtin"
  script: |
    lang.new(tableType, @t);
    t.a = 1;
    t.b = "test";
    pack(t, @packedData);
    return typeof(packedData)
  expected_success: true
  expected_result: "data"
```

**Don't** write tests for `table.packtable()` in headless mode - they will segfault.

### Implementation Notes

**From langpack.c:340-366**:
```c
boolean langpackverb (hdltreenode hparam1, tyvaluerecord *vreturned) {
    tyvaluerecord val;
    hdlhashtable htable;
    bigstring bsvarname;
    Handle hpacked;

    // Get value to pack (param 1)
    if (!getparamvalue (hparam1, 1, &val))
        return (false);

    // Get destination var (param 2)
    flnextparamislast = true;
    if (!getvarparam (hparam1, 2, &htable, bsvarname))
        return (false);

    // Pack value (type-agnostic)
    if (!langpackvalue (val, &hpacked, HNoNode))
        return (false);

    // Store as binary
    return langsetbinaryval (htable, bsvarname, hpacked);
}
```

**Key**: `langpackvalue()` dispatches to type-specific packers without file I/O.

---

## Resource File Dependencies

### Affected Verbs

Verbs that read/write application resource files will fail in headless mode:

**Write Operations** (typically segfault):
- `table.packtable(table, resName)` - writes HASH resource
- `op.packtable(outline, resName)` - writes LAYO resource
- Any verb using `filewriteresource()`

**Read Operations** (typically return errors):
- Verbs loading from resources
- Menu loading (resource-based menus)
- String table lookups from resources (if not migrated to YAML)

### Workarounds

1. **Serialization**: Use `pack()` builtin (see above)
2. **String Tables**: Migrate to YAML-based strings (see `tools/strings_compiler/`)
3. **Menu Loading**: Use scripted menu definitions in ODB
4. **Custom Resources**: Store in ODB tables instead of resource forks

---

## UI-Dependent Operations

### Categories of UI Dependencies

1. **Window Management**
   - `window.*` verbs - unavailable in headless
   - Current window tracking - no concept in headless
   - Window-specific operations (zoom, minimize, etc.)

2. **User Interaction**
   - `dialog.*` verbs - no UI for dialogs
   - `mouse.*` verbs - no mouse in headless
   - `kb.*` verbs (some) - keyboard simulation unavailable

3. **Graphics/Display**
   - `quickdraw.*` verbs - rendering unavailable
   - `pict.*` verbs - picture display unavailable
   - Color/font selection dialogs

### Testing Strategy

For UI-dependent verbs:
- **Don't** write integration tests for headless mode
- **Do** document as unavailable in headless
- **Do** provide error messages if called (don't segfault)

**Example stub pattern**:
```c
// tests/headless_window_verbs.c
static boolean window_valueproc(short token, hdltreenode hparam1,
                                tyvaluerecord *vreturned, bigstring bserror) {
    setstringerror("\pWindow verbs not available in headless mode", bserror);
    return false;
}
```

### Headless-Safe Alternatives

Some UI verbs have headless-compatible alternatives:

| UI Verb | Headless Alternative | Notes |
|---------|---------------------|-------|
| `window.gettarget()` | Use explicit table refs | No current window concept |
| `dialog.alert(msg)` | `log_info()` or `stderr()` | Non-interactive logging |
| `mouse.click()` | Script direct actions | No mouse simulation |
| `quickdraw.line()` | Store geometry in tables | No rendering, but can store |

---

## Quick Reference

### Common Headless Adaptations

```usertalk
// ❌ Windowed Pattern → ✅ Headless Pattern

// Serialization
table.packtable(@t, "res") → pack(t, @data)

// Window targeting
window.gettarget()         → Use explicit @table references

// User alerts
dialog.alert("msg")        → Not available (use logging)

// Resource strings
getresourcestring(id)      → Use YAML string tables

// File operations
Use file.* verbs           → ✓ Work in headless (file_portable.c)
```

### Headless-Compatible Operations

These **do work** in headless mode:
- ✅ All `file.*` verbs (via `file_portable.c`)
- ✅ `lang.*` verbs (language runtime)
- ✅ `string.*` verbs (string manipulation)
- ✅ `table.assign/goto/getcursor/emptytable` (table navigation)
- ✅ `pack()` and `unpack()` builtins
- ✅ `op.*` outline verbs (most - outline data structures)
- ✅ `db.*` database verbs (ODB operations)
- ✅ `sys.*` system verbs (OS interaction)

### Verb Implementation Checklist

When implementing verb bindings for headless mode:

- [ ] Check for resource file dependencies (`filewriteresource`, `filereadresource`)
- [ ] Check for UI dependencies (windows, dialogs, mouse, graphics)
- [ ] Use `pack()` instead of processor-specific pack verbs
- [ ] Provide clear error messages for unavailable operations
- [ ] Write integration tests only for headless-compatible verbs
- [ ] Document any headless-specific limitations

---

## References

- **pack() Implementation**: `Common/source/langpack.c:340-366`
- **Type-Agnostic Packing**: `Common/source/langpack.c` (langpackvalue)
- **Table Packing**: `Common/source/tablepack.c` (hashpacktable)
- **Integration Tests**: `tests/integration/test_cases/table_verbs.yaml`
- **Dispatcher Pattern**: `docs/DISPATCHER_PATTERN.md`
- **File Portability**: `portable/file_portable.c`

---

## History

- **2026-01-01**: Initial documentation (table.packtable → pack() pattern)

---

**Last Updated**: 2026-01-01
**Maintainer**: Verb binding workstream
