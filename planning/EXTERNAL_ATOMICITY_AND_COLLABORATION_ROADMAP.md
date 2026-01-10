# External Atomicity and Collaboration Roadmap

**Status**: Planning / Discussion
**Date**: 2026-01-08
**Context**: Phase 3 op verb implementation raised questions about thread safety and future collaboration

---

## Executive Summary

This document outlines the evolution from single-threaded legacy Frontier behavior to multi-user collaborative editing. The key insight: **whatever we implement for thread safety must be a stepping stone to collaboration, not something we'll replace later.**

**Recommendation**: Start with legacy Frontier behavior (Step 0), document the roadmap, implement collaboration features incrementally.

---

## Historical Context: Legacy Frontier (Pre-2004)

### What Legacy Frontier Did

**Single-Threaded Model**:
- One script execution at a time (no concurrent UserTalk)
- Global `outlinedata` variable (current outline)
- Push/pop stack for temporarily switching contexts
- Database-level atomicity (all-or-nothing save)

**Collaboration Model (5 developers)**:
- Last-write-wins at database file level
- Manual coordination ("I'm editing system.startup")
- Working in non-overlapping areas
- Explicit communication for core changes
- No source control system!

**What Made It Work**:
- Small team with deep system knowledge
- Thoughtful delegation of areas
- Understanding of whole system
- Trust and communication

**Known Limitations**:
- Doesn't scale beyond ~5 developers
- No conflict detection
- No version tracking
- Database corruption if crashes during save
- No concurrent script execution

### What We Should Match Now (Step 0)

For Phase 3 op verbs, replicate legacy behavior:
- ✅ Single-threaded script execution (existing)
- ✅ Global outline context (existing - `outlinedata` in thread-local storage)
- ✅ Database-level atomicity (existing - `file.save()`)
- ✅ No concurrent access to same external (existing assumption)
- ✅ No version tracking (existing)
- ✅ No conflict detection (existing)

**Critical**: Legacy Frontier was NOT thread-safe for concurrent access to externals. We shouldn't add thread safety YET - just match legacy behavior and document limitations.

---

## Stepwise Migration Path

### Step 0: Legacy Parity (Phase 3 - NOW)

**Goal**: Match legacy Frontier behavior exactly

**Implementation**:
- Single-threaded script execution (already true)
- No locks on externals (match legacy)
- Database-level save atomicity (already implemented)
- Global outline context via thread-local storage (already done)

**Limitations**:
- ❌ No concurrent script execution
- ❌ No multi-user collaboration
- ❌ No conflict detection
- ❌ No version tracking on externals

**Documentation Required**:
- Update CLAUDE.md to note single-threaded execution assumption
- Document in Phase 3 op verb implementation plan
- File issues for Step 1+ work

**Timeline**: Phase 3 (immediate - required for db verbs)

---

### Step 1: Last-Write-Wins with Conflict Detection (Future)

**Goal**: Detect when externals were modified by another user/session, reject conflicting saves

**Architecture**: External version headers

```c
typedef struct v7_external_header {
    uint16_t headerversion;         // 7
    uint32_t headersize;
    // ... existing fields ...

    // NEW: Last-write-wins metadata
    int64_t last_modified_time;     // Unix timestamp (microseconds)
    uint64_t modification_version;  // Monotonic counter
    char last_modifier_id[32];      // Client/user identifier

    // Reserved for Steps 2-3
    uint64_t reserved_crdt[6];      // 48 bytes for future CRDT
} v7_external_header;
```

**Conflict Detection**:
```c
// Load external
external_context_t *ext = external_load(@workspace.outline);
// ext->base_version = 42

// User makes changes...
external_mark_modified(ext);

// Another user saved version 43 in the meantime

// Save attempt
if (external_save(ext) == CONFLICT_DETECTED) {
    langerrormessage("Cannot save: modified by another user. Reload and try again.");
    return false;
}
```

**Atomic Unit**: Top-level external value (e.g., `@workspace.myTable`, `@workspace.outline1`)
- Matches Frontier's mental model (guest database entries)
- Nested externals are independent atomic units
- "I'm editing @workspace.myTable" is a natural coordination claim

**Use Cases**:
- Multiple developers editing same database file (Dropbox/Git sync)
- Single user, multiple devices (save conflict detection)
- Manual merge required (no automatic resolution)

**Pros**:
- ✅ Simple to implement
- ✅ Prevents data loss from concurrent edits
- ✅ Explicit coordination (like legacy, but automated)
- ✅ Foundation for Step 2

**Cons**:
- ❌ Still requires manual coordination
- ❌ No automatic merge
- ❌ Doesn't scale to real-time collaboration

**Timeline**: Post-Phase 3 (when multi-device/multi-user becomes a requirement)

---

### Step 2: Diff/Compare with Manual Merge (Future)

**Goal**: When conflicts detected, show 3-way diff UI and let user merge

**Architecture**: Change log storage

```c
typedef struct v7_external_header_v2 {
    // Step 1 fields (version, timestamp, modifier)

    // Step 2: Change tracking
    uint64_t change_log_address;    // Pointer to serialized diff log
    uint32_t change_log_size;
} v7_external_header_v2;
```

**Conflict Resolution**:
```c
if (external_save(ext) == CONFLICT_DETECTED) {
    // Load all three versions
    external_t *base = external_load_version(@workspace.outline, 42);
    external_t *theirs = external_load_version(@workspace.outline, 43);
    external_t *ours = ext;

    // Show 3-way diff UI (like Git merge tools)
    conflict_resolution_ui(base, theirs, ours);

    // User manually picks changes or merges
    // Save merged result as version 44
}
```

**Use Cases**:
- Distributed team with occasional conflicts
- User wants to review changes before accepting
- Complex merges requiring human judgment

**Pros**:
- ✅ User maintains control over merges
- ✅ Can see exactly what changed
- ✅ Like Git - familiar model

**Cons**:
- ❌ Interrupts workflow
- ❌ Doesn't work for real-time collaboration
- ❌ Requires diff/merge UI implementation

**Timeline**: Phase 5-6 (when distributed teams become common)

---

### Step 3: Automatic Conflict Resolution (CRDT) (Future)

**Goal**: Google Docs-style collaborative editing - multiple users edit simultaneously, conflicts merge automatically

**Architecture**: Operation-based CRDT

```c
typedef struct op_command {
    uuid_t operation_id;             // Unique operation ID
    uuid_t target_node_id;           // Stable node identity
    op_type type;                    // INSERT, DELETE, MODIFY, MOVE
    tyvaluerecord value;

    // CRDT metadata
    lamport_timestamp_t timestamp;   // Causality tracking
    vector_clock_t vector_clock;     // Operation ordering
    client_id_t client_id;           // Multi-user session
    uuid_t parent_op_id;             // Operation dependency
} op_command_t;

typedef struct v7_external_header_v3 {
    // Step 1 & 2 fields

    // Step 3: CRDT metadata
    uint64_t operation_log_address;  // CRDT operation log
    vector_clock_t vector_clock;     // Current version vector
    client_id_t client_id;           // This client's ID
} v7_external_header_v3;
```

**How It Works**:
```c
// User A: Insert "Node A"
op_a = op_create_insert(node_id_1, "Node A", down);
op_log_append(external, op_a);
op_sync_send_to_server(op_a);

// User B (concurrent): Insert "Node B" at same position
op_b = op_create_insert(node_id_2, "Node B", down);
op_log_append(external, op_b);
op_sync_send_to_server(op_b);

// Server receives both operations
// CRDT merge rules apply:
// - Both inserts succeed
// - Deterministic ordering (e.g., by timestamp + client_id)
// - Result: "Node A" then "Node B" (or vice versa, but consistent)

// Both clients receive merged state automatically
```

**Key Requirements**:

1. **Stable Node Identity**:
```c
typedef struct headrecord {
    // Existing positional links
    hdlheadrecord headlink;
    hdlheadrecord headlinkleft;

    // NEW: Stable identity
    uuid_t node_id;              // Never changes, globally unique
    uint64_t node_version;       // Node's local version
} headrecord;
```

2. **Operation Primitives**:
- INSERT(node_id, parent_id, direction, text)
- DELETE(node_id)
- MODIFY(node_id, attribute, value)
- MOVE(node_id, new_parent_id, direction)

3. **CRDT Merge Rules**:
- Commutative: ops can apply in any order
- Idempotent: same op applied twice = apply once
- Conflict-free: deterministic merge (no user intervention)

**Use Cases**:
- Real-time collaborative editing (Google Docs model)
- Automattic partnership (WordPress admins editing server config)
- Dave Winer's multi-user outline editing
- Distributed teams working on same outlines simultaneously

**Pros**:
- ✅ No blocking on network latency
- ✅ Automatic conflict resolution
- ✅ Scales to many concurrent users
- ✅ Works offline (sync when reconnected)

**Cons**:
- ❌ Complex implementation (CRDT algorithms)
- ❌ Requires stable node IDs (migration needed)
- ❌ Network infrastructure required
- ❌ Conflict resolution may surprise users (unexpected merges)

**Timeline**: Phase 6+ (long-term, requires Step 1-2 foundation)

---

## Architectural Options Analysis

### Option A: Locks (Traditional Concurrency)

**How It Works**:
```c
pthread_rwlock_t external_lock;

// Thread A
external_write_lock(@workspace.outline);
op.insert("Node A", down);
external_unlock(@workspace.outline);

// Thread B (blocked until Thread A unlocks)
external_write_lock(@workspace.outline);
op.insert("Node B", down);
external_unlock(@workspace.outline);
```

**Pros**:
- ✅ Simple to implement
- ✅ Prevents data corruption
- ✅ Well-understood pattern

**Cons**:
- ❌ Blocks concurrent access (kills collaboration)
- ❌ Deadlocks with nested externals
- ❌ Network latency = unacceptable blocking
- ❌ Does NOT scale to Google Docs-style collaboration
- ❌ **Would need to be replaced for Step 3**

**Verdict**: Local maximum. Good for single-process thread safety, but fundamentally incompatible with collaborative editing. **NOT RECOMMENDED** for our roadmap.

---

### Option B: Copy-on-Write + Operation Log (CRDT Foundation)

**How It Works**:
```c
// Thread A gets its own context
external_context_t *ctx_a = external_acquire(@workspace.outline);
// ctx_a->local_view is a copy

// Thread A creates operation
op_command_t *op = op_create_insert("Node A", down);
op_log_append(ctx_a, op);  // Locks only the log append (microseconds)
outline_apply_operation(ctx_a->local_view, op);  // No lock needed

// Thread B gets its own context (different copy)
external_context_t *ctx_b = external_acquire(@workspace.outline);
op_command_t *op2 = op_create_insert("Node B", down);
op_log_append(ctx_b, op2);
outline_apply_operation(ctx_b->local_view, op2);

// Merge happens explicitly (at save or on request)
external_merge(ctx_a, ctx_b);  // CRDT rules apply
```

**Locking Strategy**:
- Operation log append: Brief mutex (microseconds)
- Ref count updates: Atomic operations (no lock)
- Content access: NO LOCKS (each thread has own copy)
- Database save: Exclusive lock (but async)

**Pros**:
- ✅ Non-blocking concurrent edits
- ✅ Foundation for CRDT (Step 3)
- ✅ Works locally (Step 0-1) and over network (Step 3)
- ✅ No architectural replacement needed later
- ✅ Dave Winer-approved (lock-free!)

**Cons**:
- ❌ More complex than locks
- ❌ Requires stable node IDs (UUIDs)
- ❌ Memory overhead (multiple copies)
- ❌ Merge complexity (though deferred to Step 3)

**Verdict**: **RECOMMENDED** for long-term roadmap. Can implement incrementally:
- Step 0: Single-threaded (no copies needed yet)
- Step 1: Add version tracking
- Step 2: Add operation logging
- Step 3: Add CRDT merge

---

### Option C: Hybrid (Simple Now, Upgrade Later)

**Step 0-1**: Single-threaded (match legacy)
- No locks, no copies
- Simple and fast
- Document limitations

**Step 2**: Add operation logging (no CRDT yet)
- Operations logged for debugging/undo
- Still single-threaded application
- Foundation for Step 3

**Step 3**: Upgrade to CRDT
- Add stable node IDs
- Implement CRDT merge
- Enable collaboration

**Pros**:
- ✅ Simplest for Phase 3
- ✅ Incremental complexity
- ✅ Each step adds value

**Cons**:
- ❌ Requires migration at each step
- ❌ May discover architectural issues late

**Verdict**: **RECOMMENDED for pragmatic implementation**. This is the path we should take.

---

## Critical Architectural Decisions

### Decision 1: Node Identity (UUIDs)

**Question**: Should outline nodes have stable, unique identifiers?

**Current State**: Nodes identified by position in tree (headlink, headlinkleft pointers)

**Proposal**: Add UUIDs
```c
typedef struct headrecord {
    // Existing positional links (fast local navigation)
    hdlheadrecord headlink;
    hdlheadrecord headlinkleft;

    // NEW: Stable identity (for operations and collaboration)
    uuid_t node_id;              // 16 bytes, globally unique, never changes
    uint64_t node_version;       // 8 bytes, local version counter
} headrecord;
```

**When to Add**:
- **Option A**: Now (Phase 3) - reserves space in v7 format
- **Option B**: Step 2 (when operation logging needed)
- **Option C**: Step 3 (when CRDT needed)

**Trade-offs**:
- **Add now**: No migration later, but 24 bytes per node overhead
- **Add later**: Smaller now, but requires v7→v8 migration

**Recommendation**: TBD - depends on timeline pressure for collaboration

---

### Decision 2: External Version Storage

**Question**: Where to store external version numbers?

**Option A**: In external header (on disk)
```c
typedef struct v7_external_header {
    uint64_t modification_version;  // Persisted to disk
} v7_external_header;
```
- Pro: Survives across sessions
- Con: Requires v7 format change (or use reserved bytes)

**Option B**: In-memory only (runtime tracking)
```c
external_context_t {
    uint64_t runtime_version;  // Lost when process exits
}
```
- Pro: No format change needed
- Con: Can't detect conflicts across sessions

**Recommendation**: Option A (disk storage) for Step 1+, but reserve space in v7 headers now

---

### Decision 3: Atomic Unit for Collaboration

**Question**: What's the unit of "lock" or "claim" for collaboration?

**Option A**: Top-level external value (e.g., `@workspace.myTable`)
- Matches Frontier mental model
- Natural coordination unit
- Nested externals are independent

**Option B**: Database file (e.g., `MyGuest.root`)
- Coarser granularity
- Simpler locking
- Matches how databases are loaded

**Option C**: Individual outline node
- Finest granularity
- Maximum concurrency
- Too complex for Step 1

**Recommendation**: Option A (external value) - aligns with user expectations

---

## Implementation Roadmap

### Phase 3 (Immediate): Legacy Parity

**Scope**: Match legacy Frontier behavior exactly

**Tasks**:
1. ✅ Implement op verbs (getCursor, setCursor, etc.)
2. ✅ Use existing global outline context (thread-local `outlinedata`)
3. ✅ Single-threaded script execution (already true)
4. ✅ No locks, no version tracking (match legacy)
5. ✅ Document limitations in CLAUDE.md and verb implementation plan

**Success Criteria**:
- Op verbs work correctly for single-user, single-session use
- Test suite passes
- No new thread-safety assumptions introduced

**Timeline**: Current phase (prerequisite for db verbs)

---

### Phase 4-5: Version Tracking Foundation (Future)

**Scope**: Add version tracking without CRDT

**Tasks**:
1. Reserve space in v7 external headers for version metadata
2. Add `external_context_t` structure (version tracking)
3. Implement Step 1 (conflict detection, last-write-wins)
4. Add operation logging infrastructure (no CRDT merge yet)
5. Optional: Add stable node IDs if timeline permits

**Success Criteria**:
- Can detect when external was modified by another session
- Last-write-wins enforced (conflicting saves rejected)
- Operation log can be replayed for debugging

**Timeline**: When multi-device or multi-user becomes requirement

---

### Phase 6+: Collaborative Editing (Long-term)

**Scope**: Full CRDT-based collaboration

**Tasks**:
1. Implement stable node IDs (UUIDs) if not already done
2. Implement CRDT merge algorithms for each operation type
3. Add network sync protocol
4. Build conflict resolution UI (for edge cases)
5. Test with concurrent users

**Success Criteria**:
- Multiple users can edit same external simultaneously
- Conflicts merge automatically (no user intervention in common cases)
- Works over network with latency
- Scales to 10+ concurrent users

**Timeline**: Phase 6+ (long-term, after Automattic partnership requirements clear)

---

## Format Compatibility Strategy

### v7 Format Extensions

**Current v7 Header** (implemented):
```c
typedef struct v7_external_header {
    uint16_t headerversion;    // 7
    uint32_t headersize;
    // ... fields ...
} v7_external_header;
```

**Proposed: Reserve Collaboration Space** (for Step 1-3):
```c
typedef struct v7_external_header {
    uint16_t headerversion;    // 7
    uint32_t headersize;
    // ... existing fields ...

    // NEW: Reserved for collaboration features (64 bytes)
    uint64_t reserved_collaboration[8];

    // When Step 1 implemented, use reserved space:
    // reserved_collaboration[0] = modification_version
    // reserved_collaboration[1] = last_modified_time
    // reserved_collaboration[2-3] = last_modifier_id (16 bytes)
    // reserved_collaboration[4-7] = CRDT metadata (32 bytes)
} v7_external_header;
```

**Benefits**:
- No migration needed when implementing Step 1
- v7 format "collaboration-ready" from start
- Can enable features with config flag (no format bump)

**Cost**:
- 64 bytes per external (minimal)

**Decision**: TBD - depends on whether we want to reserve now or migrate later

---

## Open Questions

### Q1: Timeline and Priority

What's the urgency for each step?
- Step 0 (legacy parity): **IMMEDIATE** (Phase 3 requirement)
- Step 1 (conflict detection): When?
- Step 2 (diff/merge): When?
- Step 3 (CRDT collaboration): When?

Depends on:
- Automattic partnership requirements
- Dave Winer's multi-user timeline
- Single-user vs multi-user priorities

### Q2: Use Case Clarity

What's the primary collaboration use case?
- **Scenario A**: Distributed team, occasional conflicts (Step 1-2 sufficient)
- **Scenario B**: Real-time collaborative editing (Step 3 required)
- **Scenario C**: Single user, multiple devices (Step 1 sufficient)

Answer determines which features to prioritize.

### Q3: Node Identity Timing

When to add UUIDs to outline nodes?
- **Now**: No migration later, but overhead now
- **Step 2**: When operation logging needed
- **Step 3**: When CRDT needed

Depends on confidence in Step 3 timeline.

### Q4: Format Reservation

Should we reserve space in v7 external headers now?
- **Yes**: Future-proof, no migration needed
- **No**: Smaller now, add when needed (v7→v8 migration)

Depends on format stability preferences.

---

## Recommendations

### For Phase 3 (Immediate)

**DO**:
- ✅ Implement op verbs matching legacy Frontier behavior
- ✅ Use existing thread-local outline context (`outlinedata`)
- ✅ Single-threaded execution (no concurrent script support)
- ✅ Document limitations clearly
- ✅ File issues for Step 1+ work

**DON'T**:
- ❌ Add locks (would need to remove for Step 3)
- ❌ Add version tracking yet (wait for Step 1 design)
- ❌ Try to solve thread safety now (premature)

### For Format Design (Near-term Decision)

**Recommended**: Reserve 64 bytes in v7 external headers for collaboration metadata
- Small cost now
- Enables Step 1-3 without migration
- Can populate incrementally

### For Long-term Architecture

**Recommended**: Operation-based concurrency (Option B)
- Start simple (Step 0: single-threaded)
- Add operations incrementally (Step 1-2)
- Enable CRDT when needed (Step 3)
- No architectural replacement needed

---

## Success Criteria

### Phase 3 Success
- [ ] Op verbs implemented and tested
- [ ] Matches legacy Frontier behavior
- [ ] Single-user, single-session works correctly
- [ ] Limitations documented
- [ ] No regression in existing functionality

### Step 1 Success (Future)
- [ ] Can detect external modification conflicts
- [ ] Last-write-wins enforced
- [ ] Conflicting saves rejected with clear error
- [ ] Works across sessions/devices

### Step 2 Success (Future)
- [ ] 3-way diff/merge UI implemented
- [ ] User can manually resolve conflicts
- [ ] Operation log can be inspected/replayed

### Step 3 Success (Future)
- [ ] Automatic conflict resolution (CRDT)
- [ ] Multiple users edit simultaneously
- [ ] Works over network with latency
- [ ] Scales to 10+ concurrent users

---

## References

- Issue #135: Outline context refactoring (op_context_t infrastructure)
- ADR-005: Parameter state thread-safety (pattern for thread-local migration)
- `planning/phase3/GLOBAL_STATE_AUDIT.md`: Thread-safety audit findings
- `planning/CRDT_FOUNDATION_ROADMAP.md`: Collaborative ODB vision
- Dave Winer partnership discussions (2026-01-08)
- Automattic partnership requirements (TBD)

---

## Document Status

**Next Steps**:
1. Review with user - confirm Step 0 approach for Phase 3
2. Decide on v7 format reservation question
3. File issues for Step 1+ work
4. Update Phase 3 op verb implementation plan with limitations
5. Proceed with pragmatic implementation (match legacy behavior)
