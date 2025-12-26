# ADR: Operation Context Pattern for ODB Collaboration

**Status**: Accepted (Phase 1 - Outline foundation, Phase 6+ - Full ODB)
**Date**: 2025-12-25
**Author**: System Architect
**Relates to**: Issue #135, North Star Vision: Collaborative ODB

## Context

Frontier's Object Database (ODB) contains six external object types:
1. **Outline (op)** - Hierarchical text with attributes
2. **Script** - UserTalk source code
3. **WPText (wp)** - Rich text (RTF-based)
4. **Table** - Hash table key-value pairs
5. **Menu** - Menu structure
6. **Picture (pict)** - Bitmap images

Current State (Phase 3):
- All operations assume single-threaded, single-user access
- Global variables track "current" outline, script, etc.
- No mechanism for concurrent access or versioning
- No way to track who changed what, when, or why

Vision (Phase 6+):
- **Google Docs-style collaborative editing** of any ODB object
- Multiple users editing different (or same) objects simultaneously
- Automatic conflict resolution using CRDT principles
- Version history and audit trails
- Stable node identity (UUIDs) for cross-version references

## Decision

Implement **operation context pattern** across all six ODB types, establishing foundation for CRDT-based collaboration.

### Phase 3: Outline Context Only

Begin with outline objects (Issue #135):
- `op_context_t` structure with atomic refcounting and fine-grained versioning
- Lifecycle: `op_context_acquire()`, `op_context_retain()`, `op_context_release()`
- Version bumps on every mutation (insert, delete, move, edit)
- 64-byte reserved space for Phase 6+ sync/conflict metadata
- 16-byte reserved space in outline nodes for UUIDs

Why start with outlines:
- Most complex external type (hierarchical structure)
- Most likely use case for collaboration (Dave Winer's system)
- Pattern proven effective in database context (db_context)
- Acts as reference pattern for other types

### Phase 6+: Extend to All ODB Types

Apply same context pattern to scripts, WPText, tables, menus, pictures:

```c
// Template pattern (apply to each type)
typedef struct <type>_context_t {
    // Core fields (required for all types)
    <type>handle handle;                    // Reference to object
    _Atomic uint32_t refcount;              // Reference counting
    _Atomic uint64_t version;               // Fine-grained versioning
    uint32_t flags;                         // Operation flags

    // Type-specific fields (optimize per object)
    // Scripts: cursor_position, selection_start, selection_end
    // WPText: paragraph_index, run_offset, format_cache
    // Tables: current_key, iteration_state
    // Menus: selected_item, hover_state
    // Pictures: zoom_level, pan_x, pan_y

    // Reserved for Phase 6+
    void *reserved[8];
} <type>_context_t;

// Required lifecycle functions (per type)
<type>_context_t* <type>_context_acquire(uint32_t flags);
<type>_context_t* <type>_context_retain(<type>_context_t *ctx);
void <type>_context_release(<type>_context_t *ctx);
uint64_t <type>_context_version_bump(<type>_context_t *ctx);
```

### Why This Pattern Enables CRDT

**Problem**: Traditional databases don't support collaborative editing because:
- No version history (can't detect what changed)
- No node identity (can't merge concurrent edits)
- No operation log (can't replay changes)

**Solution**: Context pattern provides foundation:
1. **Fine-grained versioning** (every operation increments version)
   - Server can ask: "What changed between v42 and v51?"
   - Client can store: "My last seen version was v42"
   - Conflict detector: "Other user modified v43-v50, I'm at v42"

2. **Stable node identity** (16-byte UUID reserved)
   - User A edits node with ID `abc123def`
   - User B moves that node in tree
   - `abc123def` still points to correct node
   - CRDT can merge edits without position ambiguity

3. **Operation ownership** (context is operation-scoped)
   - No shared mutable state
   - Each operation has its own context snapshot
   - Multi-user operations don't interfere
   - Natural foundation for optimistic locking

## Timeline and Phases

### Phase 3 (Current): Outline Foundation
- Implement `op_context_t` (DONE in Issue #135)
- Integrate into all outline operations
- Establish as reference pattern
- **Outcome**: Outline context enables Phase 6+ work

### Phase 4-5: Complete v7 Parity
- Outline context fully integrated
- No CRDT yet, but foundation ready
- Other types still use global state (acceptable for Phase 3)

### Phase 6: CRDT Foundation & Script Context
- Implement `script_context_t` following outline pattern
- Add UUID infrastructure (v7.5 format)
- Implement Lamport timestamps for causality
- Research CRDT approach (OT vs pure CRDT vs hybrid)
- **First CRDT prototype**: Scripts (simplest text-based type)

### Phase 7: Multi-Type Collaboration
- Extend pattern to WPText, tables, menus, pictures
- Unified CRDT approach across all types
- Multi-user synchronization infrastructure
- Conflict resolution with user feedback

### Phase 8: Deployment Ready
- Full Google Docs-style collaboration
- Tested with Dave Winer and Automattic partners
- Production-grade conflict resolution
- Audit trails and version history

## Consequences

### Positive
- **Foundation for collaboration**: Pattern proven scalable to all ODB types
- **User isolation**: Developers write single-threaded code, runtime handles concurrency
- **Version transparency**: Every operation tracked, enabling merge/rebase
- **Node stability**: UUIDs survive renames, moves, structural changes
- **Audit trail**: Who changed what, when, for compliance/debugging

### Negative
- **API expansion**: Every object type gets `_context` function variants
- **Cognitive load**: Developers must understand context lifecycle
- **Performance**: Atomic operations on every mutation (measurable but acceptable)
- **Long-term commitment**: Requires 12-18 months to fully implement

### Mitigation
- Backward-compat wrappers maintain old API (phased migration)
- Comprehensive logging for debugging
- Pattern clearly documented in ADRs
- Single reference implementation (outline) guides others

## Architectural Constraints

### Must Preserve
1. **Back-compat**: Existing UserTalk code continues working
2. **Transparency**: Developers shouldn't see concurrency complexity
3. **Flexibility**: Frontier's unique features remain accessible
4. **Performance**: Small overhead acceptable, not prohibitive

### Must Avoid
1. ~~Eventual consistency~~ - Last-write-wins with explicit merging
2. ~~Distributed locks~~ - Per-object locking at runtime layer
3. ~~Vector clocks in Phase 3~~ - Lamport timestamp in Phase 6+
4. ~~Full CRDT in Phase 3~~ - Foundation only, implementation in Phase 6+

## Phase 3 Success Criteria

For Issue #135 (Outline context):
- [ ] `op_context_t` fully implemented and tested
- [ ] All outline mutations integrated (opinsert, opdelete, etc.)
- [ ] Backward-compat wrappers maintain old API
- [ ] Headless tests pass 100%
- [ ] Zero regressions in v7 functionality
- [ ] ADRs document pattern and future intent clearly

For Phase 6+ enablement:
- [ ] Reserved space not touched in Phase 3 (stays NULL)
- [ ] Version tracking validated as monotonic
- [ ] Refcount semantics proven correct
- [ ] ADRs explain how Phase 6 will extend pattern

## References

- Issue #135: https://github.com/jsavin/Frontier/issues/135
- CLAUDE.md: "Collaborative ODB Editing - North Star Vision"
- Related ADRs:
  - OUTLINE_OPERATION_CONTEXT.md
  - NODE_IDENTITY_RESERVATION.md
- CRDT References:
  - Operational Transformation: https://en.wikipedia.org/wiki/Operational_transformation
  - CRDT (Conflict-Free Replicated Data Type): https://crdt.tech/
  - Automerge (CRDT library): https://automerge.org/
  - Yjs (CRDT library): https://docs.yjs.dev/

## Questions for Phase 6+ Planning

1. **CRDT Approach**: Pure CRDT (Yjs/Automerge) vs Operational Transformation vs hybrid?
2. **Conflict Resolution**: Last-write-wins vs merge UI vs automatic smart merging?
3. **Network**: Local-only vs cloud sync vs peer-to-peer?
4. **Performance**: How many concurrent users? How large outlines?
5. **History**: Keep full version history or prune after N versions?
6. **Permissions**: Per-object locks? Per-user roles? Per-branch access control?
