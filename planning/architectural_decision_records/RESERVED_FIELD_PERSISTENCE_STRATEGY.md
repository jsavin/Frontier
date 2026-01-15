# Architectural Decision Record: Reserved Field Persistence Strategy

**Status:** Approved (Phase 3)
**Date:** 2025-12-25
**Deciders:** Core Team
**Related Issues:** #135 Phase 2, PR #164

## Problem Statement

Phase 2 introduced reserved space in two key structures:

1. **`op_context_t.reserved[8]`** (64 bytes) - Operation metadata (Lamport timestamps, change logs, conflict context, etc.)
2. **`tyheadrecord.reserved_identity[16]`** (16 bytes) - Node identity (planned UUIDs for v7.5)

The critical question: **How should these reserved fields be persisted to disk?**

### Current State

- **`op_context_t`**: Operation-scoped, in-memory only. Never persisted (contexts are created/destroyed per operation).
- **`tyheadrecord.reserved_identity[16]`**: Currently zero-filled, in-memory, NOT persisted to v7 database format.

### Future Risk

If we don't establish a clear persistence strategy now:

1. **Phase 6+ CRDT integration will be blocked** - UUIDs in reserved_identity can't be persisted without format changes
2. **Data loss on reload** - Node identity assignments vanish when database closes/reopens
3. **Merge conflicts inevitable** - Two users editing independently will assign conflicting UUIDs to the same nodes
4. **Format migration mess** - Adding persistence later requires complex v7→v7.5 migration

## Decision

**Establish clear persistence rules now, implement in phases:**

### Phase 3 (Current) - Foundation Only
- ✅ `tyheadrecord.reserved_identity[16]` remains zero-filled, not persisted
- ✅ Code explicitly validates that reserved fields are NULL (catch premature use)
- ✅ Database schema unchanged, no format modifications needed

### Phase 4-5 - Extended ODB Coverage
- Extend the same pattern to other ODB types (scripts, WPText, tables, menus, pictures)
- Document reserved space in each type's disk format
- Still do NOT populate reserved fields (keep zeroes)

### Phase 6 - CRDT Foundation (v7.5 Format)
- **Reserve space in database format** for `tyheadrecord.reserved_identity[16]`
- This is purely reserving space - NOT assigning UUIDs yet
- Database format changes from v7 to v7.5 with migration tool
- v7→v7.5 migration: Keep reserved_identity as zeros (UUIDs assigned dynamically)

### Phase 6+ - UUID Assignment (v7.6+)
- Actually assign UUIDs to nodes when first modified
- Persist UUIDs in `reserved_identity[16]`
- Use UUIDs for CRDT conflict resolution

## Implementation Strategy

### Phase 3: What We Do NOW

**In `oppack_v7.c` (pack/unpack operations)**:
Currently, `reserved_identity[16]` is ignored during serialization.

**Recommended**: Add explicit documentation and NO-OP handling:

```c
/* Phase 3: Reserved identity space (not yet persisted)
 * Future use: 16-byte UUID for node identity (Phase 6+)
 * Currently: Always zero, not written to disk, not read from disk
 * See: RESERVED_FIELD_PERSISTENCE_STRATEGY.md
 */

// During pack: skip reserved_identity (write zeros implicitly)
// During unpack: skip reserved_identity (zeros already)

/* Phase 6+: Will persist UUID here for collaborative editing */
```

### Phase 6: Format Migration to v7.5

**Database format changes**:

```c
typedef struct v7_headrecord {
    // ... existing fields ...
    Handle headstring;
    uint8_t reserved_identity[16];  // NEW in v7.5: space for UUID
} v7_headrecord;
```

**Migration tool** (new utility):
```bash
./frontier-migrate --format v7 --output v7.5 databases/Frontier.root7
# Output: databases/Frontier-v7.5.root
# This reserves the 16-byte space, fills with zeros
```

**Backward compatibility**:
- v7.5 reader can open v7 files (treats missing reserved_identity as zeros)
- v7 reader CANNOT open v7.5 files (unknown field)
- No data loss - just space reservation

### Phase 6+: UUID Assignment

When user modifies a node for the first time after v7.5 migration:

```c
if (is_zero_uuid((**hnode).reserved_identity)) {
    /* First modification: assign new UUID */
    generate_uuid((**hnode).reserved_identity);
    mark_node_dirty();
}
op_context_version_bump(ctx);
```

## Why This Phased Approach?

### Avoids Format Lock-In

**Bad**: Assigning UUIDs immediately
- Commits us to UUID format forever
- Forces v7 → v7.5 migration before CRDT is ready
- Adds persistence overhead to single-user mode

**Good**: Reserve space, assign on-demand
- Format is extensible (space reserved but unused)
- Can migrate when collaborative features are ready
- Single-user performance unchanged until CRDT activated

### Enables Graceful Upgrade Path

1. User runs Frontier v7.0 (current) - no UUIDs
2. User upgrades to Frontier v7.5 - space reserved, still no UUIDs
3. User enables collaborative mode - UUIDs assigned on first edit
4. User shares document - other clients request via API with UUID sync

### Parallel ODB Type Coverage

Before persisting reserved_identity in tyheadrecord:
1. Extend pattern to scripts (reserve space)
2. Extend pattern to WPText (reserve space)
3. Extend pattern to tables (reserve space)
4. Extend pattern to menus (reserve space)
5. Extend pattern to pictures (reserve space)

Then in Phase 6, add persistence uniformly across all types.

## Validation and Testing

### Phase 3 Tests (DONE)

✅ `op_context_tests.c` validates:
- Reserved fields initialized to NULL
- Reserved fields remain NULL during operations
- Context validation enforces NULL invariant

### Phase 6 Tests (Future)

Tests will validate:
- Reserved_identity space is persisted correctly
- v7→v7.5 migration preserves reserved_identity as zeros
- UUID assignment happens on first modification
- UUID persists across close/reopen

## Risk Mitigation

### Risk: Reserved Space Misuse Before Phase 6

**Mitigation**:
- Assertion in `op_context_validate()` - catches non-NULL reserved[*]
- Assertion in `oppack_v7.c` - catches premature UUID writing
- Code review checklist: "Check reserved fields are unused"

### Risk: Format Incompatibility Between v7 and v7.5

**Mitigation**:
- Migration tool makes conversion explicit and auditable
- v7.5 loader handles missing reserved_identity gracefully
- Database version number incremented (prevents silent misread)

### Risk: Performance Regression in Phase 6

**Mitigation**:
- UUID generation deferred until first edit (lazy allocation)
- UUID comparison is fast (raw memcmp on 16 bytes)
- No overhead for unchanged nodes

## Related Decisions

- **OUTLINE_OPERATION_CONTEXT.md** - Phase 3 foundation that enables this strategy
- **CRDT_IMPLEMENTATION_ROADMAP.md** - Phase 6+ features that depend on UUIDs
- **NODE_IDENTITY_ARCHITECTURE_ASSESSMENT.md** - Analysis of UUID vs alternatives

## Migration Examples

### Scenario: User with v7 Database Upgrades to v7.5

```bash
# User starts Frontier v7.0
$ frontier --database databases/Frontier.root7
# ... works normally ...

# User upgrades to Frontier v7.5
$ frontier-update v7.5

$ frontier --database databases/Frontier.root7
# v7.5 automatically reads v7 files:
# - Loads all nodes
# - reserved_identity treated as zeros
# - No changes on disk until write

# If user edits an outline:
$ ./frontier --database databases/Frontier.root7
# User edits "My Outline" node
# - First modification triggers UUID assignment
# - reserved_identity populated with UUID
# - Requires v7.5 or later to read
# - v7 client can't open database anymore

# To stay compatible with v7 clients:
# $ frontier-migrate --keep-v7-compatible databases/Frontier.root7
# (doesn't assign UUIDs, only reserves space)
```

### Scenario: Multi-Client Sync (Phase 6+)

```
Client A (v7.5 + CRDT)        Client B (v7.5 + CRDT)
   |                              |
   v                              v
database/Frontier-v7.5.root  database/Frontier-v7.5.root
   |                              |
   +----------[Sync API]----------+

   A creates node: {
       text: "Budget Planning",
       uuid: a1b2c3d4-e5f6-4a5b-9c8d-7e6f5a4b3c2d,
       version: 1
   }

   B concurrently creates node: {
       text: "Schedule Review",
       uuid: f1e2d3c4-b5a6-4d7e-8f9a-1b2c3d4e5f6a,
       version: 1
   }

   [Sync merges both - no conflict, different UUIDs]
```

## Validation Checklist for Phase 6

- [ ] Database format v7.5 specification document created
- [ ] Migration tool `frontier-migrate` implemented
- [ ] v7→v7.5 migration tested with real databases
- [ ] UUID assignment on first edit works correctly
- [ ] Persisted UUIDs survive close/reopen
- [ ] v7.5 reader handles v7 files gracefully
- [ ] v7 reader rejects v7.5 files with clear error
- [ ] CRDT tests use persisted UUIDs for conflict detection
- [ ] Performance impact measured (should be <1%)

## Open Questions

1. **When should v7→v7.5 migration happen automatically?**
   - At first modify? (Current plan)
   - At database upgrade? (Safer)
   - Manual with tool? (Most explicit)

2. **Should we reserve space in op_context_t NOW?**
   - Phase 3: No (op_context_t is operation-scoped, in-memory)
   - Phase 6: Yes (for vector clocks, change logs)

3. **Should UUIDs be v4 (random) or v1 (timestamp-based)?**
   - v4: Simple, no dependencies on clock accuracy
   - v1: Enables causality analysis (future feature)
   - Decision deferred to Phase 6 planning

## Conclusion

**Reserved space is reserved but not persisted until needed.** This allows us to:
- Build collaborative features without format lock-in
- Test CRDT logic in-memory before persistence
- Extend pattern to all 6 ODB types uniformly
- Upgrade gracefully without breaking existing databases

The Phase 3 assertion checks ensure we catch any premature use of reserved fields, protecting the strategy.
