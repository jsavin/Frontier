# ADR-003: Address Value Resolution Strategy

**Status**: Implemented
**Date**: 2025-12-27
**Author**: Codex
**Related Issues**: #166 (bare verb resolution), v6→v7 migration

## Context

Address values (`addressvaluetype`) in Frontier's UserTalk runtime store references to hashtable paths (e.g., `system.compiler.lang`, `system.verbs.globals`). These addresses are used throughout the system, including in `system.paths` which brings various tables into global scope for verb resolution.

During v6→v7 database migration, address values face a critical challenge:
- **Database addresses are format-dependent** (v6 uses 32-bit, v7 uses 64-bit)
- **In-memory pointers cannot be persisted** (pointers are only valid during runtime)
- **Tables may not exist yet** during database loading (EFP tables are linked after database load)

The migration process revealed two problems:
1. How to migrate address values from v6 to v7 without corrupting pointer references
2. How to ensure `system.paths` entries point to correct in-memory tables after loading

## Decision

We implement a **two-phase resolution strategy**:

### Phase 1: Lazy Resolution During Database Unpacking

**When**: During `langpack.c:langunpackvalue()` when unpacking address values from disk

**What**: Create "unresolved" address values with a special marker:
```c
// In langpack.c addressvaluetype unpacking:
fl = setexemptaddressvalue((hdlhashtable)-1, bs_path, &v);
```

**Why**:
- Tables referenced by addresses may not exist yet (EFP tables linked later)
- Avoids circular dependency (stringtoaddress needs tables, tables need addresses)
- Works for ALL address values, not just system.paths

**Mechanism**:
- Address value stores: `[string path][htable pointer]`
- During unpack, set htable = `-1` (0xFFFFFFFFFFFFFFFF) as "unresolved" marker
- Set `flunresolvedaddress = true` flag on the hash node
- First access via `getaddressvalue()` resolves automatically

### Phase 2: Eager Resolution for system.paths After Table Linking

**When**: In `main.c:hydrate_system_root_database()` immediately after `linksystemtablestructure()`

**What**: Walk `system.paths` table and resolve all unresolved addresses:
```c
// After linksystemtablestructure() links EFP tables:
resolve_system_paths(hroot);
```

**Why**:
- `system.paths` is used during verb lookup - must be resolved before script execution
- EFP tables (efp_lang, efp_file, etc.) are now linked and available
- Ensures bare verbs like `new()` can be found via path lookup

**Implementation** (`tablestructure.c:resolve_system_paths()`):
1. Find `system.paths` table
2. Iterate all entries marked `flunresolvedaddress = true`
3. Extract path string from address value
4. Call `langexpandtodotparams()` to resolve path → hashtable pointer
5. Update htable pointer directly in address handle memory
6. Clear `flunresolvedaddress` flag

## Architecture

### Address Value Storage Format

```
Address Handle Memory Layout:
+---------------------------+
| Pascal String (path)      | <- "system.compiler.lang"
+---------------------------+
| hashtable pointer (8 bytes)| <- Points to actual table
+---------------------------+
```

**Key Invariants**:
- **Never persist pointers** - only strings are written to disk
- **Always resolve at runtime** - pointers regenerated on each load
- **Path is source of truth** - pointer is derived, not stored

### Packing (Saving to Disk)

```c
// langpack.c:langpackvalue() - addressvaluetype case
fl = getaddresspath(val, bs);  // Extracts ONLY the path string
fl = langpackdata((long) stringlength(bs), bs + 1, hpackedvalue);
// Pointer is discarded - never written to disk
```

### Unpacking (Loading from Disk)

```c
// langpack.c:langunpackvalue() - addressvaluetype case
texthandletostring(hstring_temp, bs_path);  // Extract path
fl = setexemptaddressvalue((hdlhashtable)-1, bs_path, &v);  // htable=-1 marker
// Creates unresolved address for lazy resolution
```

### Resolution on First Access

```c
// langvalue.c:getaddressvalue()
if (*htable == (hdlhashtable) -1) {  // Unresolved address marker
    pushhashtable(roottable);
    fl = langexpandtodotparams(bs, htable, bs);  // Resolve path
    pophashtable();
}
// Automatic resolution when address is accessed
```

### Eager Resolution for system.paths

```c
// tablestructure.c:resolve_system_paths()
// Called after linksystemtablestructure() links EFP tables
for (each entry in system.paths) {
    if (entry is addressvaluetype && flunresolvedaddress) {
        Extract path from address
        Call langexpandtodotparams() to resolve
        Update htable pointer in handle memory
        Clear flunresolvedaddress flag
    }
}
```

## Timing Critical Points

1. **Database Load** → Tables unpacked with unresolved addresses (htable=-1)
2. **linksystemtablestructure()** → Links EFP tables (efp_lang, etc.) into system.compiler
3. **resolve_system_paths()** → Resolves system.paths entries NOW that EFP tables exist
4. **Script Execution** → Bare verbs like `new()` can be found via system.paths

**CRITICAL**: `resolve_system_paths()` MUST be called AFTER `linksystemtablestructure()` or resolution will fail (tables don't exist yet).

## Benefits

1. **Correct Migration**: v6 addresses migrate cleanly without pointer corruption
2. **Format Independent**: Same resolution works for v6, v7, and future formats
3. **No Circular Dependencies**: Tables can reference each other without load-order issues
4. **Maintainable**: Clear separation between disk format (strings) and runtime format (pointers)
5. **Extensible**: Works for any address value, not just system.paths

## Trade-offs

**Advantages**:
- ✅ Clean separation of concerns (disk vs memory)
- ✅ No pointer corruption during migration
- ✅ Works regardless of table load order
- ✅ Future-proof for new database formats

**Disadvantages**:
- ⚠️ Requires careful timing (must call resolve_system_paths at right point)
- ⚠️ Two-phase resolution adds complexity
- ⚠️ Must document when to use eager vs lazy resolution

## Implementation Files

| File | Purpose |
|------|---------|
| `Common/source/langpack.c` | Lazy resolution during unpack (create -1 markers) |
| `Common/source/langvalue.c` | Automatic resolution on first access (getaddressvalue) |
| `Common/source/tablestructure.c` | Eager resolution for system.paths (resolve_system_paths) |
| `frontier-cli/main.c` | Calls resolve_system_paths() after linksystemtablestructure() |

## Testing

Validated via:
- `tests/save_migration_tests.c` - Verifies v6→v7 migration preserves address values
- `./tools/run_headless_tests.sh` - Full integration test with database loading
- Manual testing: `lang.new(tableType, @t)` works (fully-qualified verb)
- Manual testing: `new(tableType, @t)` requires further investigation (verb lookup issue separate from address resolution)

## Future Considerations

1. **Lazy Resolution Performance**: If lazy resolution shows up in profiles, consider caching
2. **Concurrent Access**: Thread-safety if multiple threads resolve addresses simultaneously
3. **Path Invalidation**: What happens if a path becomes invalid after resolution?
4. **Documentation**: Teach odb-database-expert agent about this pattern

## Related ADRs

- ADR-002: Context-Based Format Versioning (database format strategy)
- mode_management_single_decision_point.md (avoid mode stack issues)
- external-object-loading-architecture.md (external table variable management)

## References

- Issue #166: UserTalk integration tests revealed bare `new()` resolution issues
- Issue #123: External table variable migration (related address format problem)
- v6→v7 Migration: `planning/phase3/MIGRATION_VALIDATION_REPORT.md`
