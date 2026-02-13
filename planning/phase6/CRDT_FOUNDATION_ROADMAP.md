# CRDT Foundation Roadmap: Phase 6+

**Status**: Planning (Phase 6+, post-v7 parity)
**Timeline**: Months 6-18 of Frontier modernization
**Priority**: P0 Strategic Initiative (enables Automattic partnership, Dave Winer 2.0)
**Owner**: Technical Product Manager + System Architect

## Overview

This document outlines the path to Google Docs-style collaborative editing across Frontier's Object Database (ODB), building on the operation context pattern established in Issue #135 (Phase 3).

**Strategic Importance**:
- Dave Winer's system is currently non-collaborative (single user only)
- Automattic partnership requires multi-user outline/config editing
- This is a 12-18 month initiative with massive impact on product positioning
- Foundation must be laid in Phase 3 (Issue #135), implementation in Phase 6+

## Long-Term Vision

### Target State: Collaborative ODB (Months 18+)

Users can simultaneously edit:
- **Outlines** - Multiple users editing different (or same) branches
- **Scripts** - Team development with version control
- **WPText** - Collaborative document editing
- **Tables** - Shared configuration management
- **Menus** - Collaborative UI definition
- **Pictures** - Shared image resources with versioning

All with **zero concurrency code** in UserTalk:

```javascript
// Simple UserTalk script - runtime handles all concurrency
op.setHeadlineText("Updated headline");  // Safe even if 10 users editing simultaneously
wp.insertText("New paragraph");           // Automatic merge if User B edited nearby
table.set("server.address", "10.0.0.1");  // Lock held transparently
```

### Developer Experience Requirement

**Principle: Single-threaded mindset, multi-user reality**

- Developers write as if they're the only user
- Runtime guarantees consistency under concurrent access
- Conflicts automatically resolved (or escalated with user feedback)
- No visible locks, no race conditions, no eventual consistency

## Phase-by-Phase Implementation

### Phase 3 (Current): Foundation Layer
**Goal**: Establish operation context pattern for outlines

**Deliverables**:
- ✅ `op_context_t` structure (Issue #135)
- ✅ Atomic refcounting and fine-grained versioning
- ✅ 64-byte reserved space for Phase 6+ metadata
- ✅ 16-byte UUID reservation in outline nodes
- ✅ ADRs documenting pattern and intent

**Success Criteria**:
- Outline operations use context pattern
- Backward compatibility maintained (old API works)
- Zero regressions in v7 functionality
- Pattern documented clearly for future extension

**NOT in Phase 3**:
- UUID implementation (space reserved only)
- CRDT operations (versioning enables, not implements)
- Multi-user testing (single-threaded only)
- Synchronization infrastructure

### Phase 6: CRDT Research & First Implementation
**Goal**: Prototype CRDT system with scripts, prove concept works

**Timeline**: 6-8 weeks

**Deliverables**:

1. **Script Context** (1 week)
   - `script_context_t` following outline pattern
   - Integrate into `scriptins`, `scriptdelete`, script edit operations
   - Backward-compat wrappers
   - Version tracking on every text operation

2. **CRDT Research & Selection** (2 weeks)
   - Evaluate: Yjs, Automerge, custom OT implementation
   - Prototype with scripts (simplest type)
   - Decision: Pure CRDT vs OT vs hybrid
   - Document choice rationale

3. **Version Infrastructure** (2 weeks)
   - Lamport timestamps (causality tracking)
   - Client ID / session tracking
   - Change log storage and retrieval
   - Populate reserved fields in `op_context_t`

4. **Script Sync Prototype** (2-3 weeks)
   - Two clients editing same script
   - Concurrent changes automatically merged
   - Version history queryable
   - Conflict detection working
   - Test with 2-3 concurrent users

**Success Criteria**:
- Scripts can be edited by 2+ users concurrently
- Edits merged correctly (no data loss)
- Version history queryable
- Conflicts detected and handled
- Performance acceptable (< 100ms latency)

**Decisions to Make**:
1. CRDT Library: Use Yjs/Automerge or implement custom?
2. Storage: In-memory during operation or persist to disk?
3. History: Full history for all scripts or prune old versions?
4. Conflicts: Auto-merge or conflict resolution UI?

### Phase 6 (Continued): Script CRDT to Production
**Goal**: Ship collaborative script editing (first user-facing collaboration)

**Timeline**: 4-6 weeks (after Phase 6 research prototype)

**Deliverables**:
- Script collaboration fully tested
- UI shows other users' cursors/selections
- Conflict resolution UI (if needed)
- Audit trail queryable
- Production-grade performance (10+ concurrent users)

### Phase 7: Multi-Type Collaboration Foundation
**Goal**: Extend pattern to all ODB types, prepare for unified CRDT

**Timeline**: 8-12 weeks

**Deliverables**:

1. **WPText Context** (1.5 weeks)
   - `wp_context_t` structure
   - Rich text-specific handling (format preservation)
   - Paragraph-level granularity
   - Integration with RTF packing/unpacking

2. **Table Context** (1.5 weeks)
   - `table_context_t` structure
   - Key-value specific handling
   - Row-level versioning
   - Merging strategy for hash tables

3. **Menu Context** (1 week)
   - `menu_context_t` structure
   - Minimal versioning (mainly identity)
   - Reserve space for future expansion

4. **Picture Context** (1 week)
   - `pict_context_t` structure
   - Bitmap-specific handling
   - Reference-based (not content versioning)

5. **Unified CRDT Layer** (3-4 weeks)
   - Abstract CRDT interface
   - Type-specific conflict resolution
   - Cross-type operation support
   - Performance optimization

**Success Criteria**:
- All six ODB types have context pattern
- Each type tested with 2+ concurrent users
- Unified CRDT layer handles all types
- Performance remains acceptable

### Phase 8: Production Collaboration
**Goal**: Ship collaborative ODB editing (full Google Docs parity)

**Timeline**: 4-6 weeks

**Deliverables**:
- All ODB types support concurrent editing
- Multi-user UI enhancements (cursors, presence, etc.)
- Conflict resolution UI refined
- Audit trails for all operations
- API documentation and best practices

## Technical Decisions (Phase 6+)

### CRDT Approach

**Option A: Pure CRDT (Yjs or Automerge)**
- Pro: Proven, well-tested libraries
- Pro: Automatic conflict resolution
- Pro: Offline-first architecture
- Con: Larger memory footprint
- Con: Might need to fork/customize for Frontier's needs
- Con: Learning curve for team

**Option B: Operational Transformation (custom)**
- Pro: Lighter weight than CRDT
- Pro: Full control over implementation
- Con: More complex to implement correctly
- Con: Harder to test concurrent scenarios
- Con: More failure modes in production

**Option C: Hybrid (CRDT for content + OT for structure)**
- Pro: Best of both worlds potentially
- Pro: CRDT handles text, OT handles moves/deletes
- Con: More complex logic
- Con: Hybrid systems can have subtle bugs

**Recommendation**: Start with pure CRDT (Yjs), evaluate for fit. If too heavy, evaluate custom OT.

### Conflict Resolution Strategy

**Option A: Last-Write-Wins (simple)**
- User A edits line 10
- User B edits line 10 simultaneously
- Whoever saves last wins
- Other user's changes lost
- **Problem**: Data loss, unacceptable

**Option B: Automatic Merge (smart)**
- User A edits headline text
- User B edits same headline's refcon
- Changes applied to different fields, auto-merge succeeds
- **Use case**: Most common, works great
- **Fallback**: If merge fails, escalate to Option C

**Option C: Conflict Resolution UI**
- User A edits line 5
- User B edits line 5 simultaneously
- Show both versions, let user choose
- **Use case**: Line-level conflicts
- **UX**: Clean UI shows diff, user picks version

**Recommendation**: Auto-merge first, UI escalation for true conflicts. Never silent data loss.

### Version History Management

**Question**: How much history to keep?

**Option A: Keep all versions (unlimited)**
- Pro: Complete audit trail
- Pro: Users can revert to any previous state
- Con: Storage grows unbounded
- Con: Slow to query large histories

**Option B: Prune old versions (e.g., keep 90 days)**
- Pro: Bounded storage
- Pro: Fast queries
- Con: Ancient history lost
- Con: Compliance/audit concerns

**Option C: Tiered storage (hot/cold)**
- Pro: Recent history in fast store, old in slow storage
- Pro: Reasonable storage and performance
- Con: More complex implementation
- Con: Query complexity (which store to search?)

**Recommendation**: Start with unlimited (Phase 6), evaluate pruning strategy based on usage patterns.

## Infrastructure Requirements

### Database Format Changes

**v7.5 Format** (Phase 6+):
- Persist UUIDs in outline nodes (reserved_identity field)
- Persist version vectors (Lamport timestamp or vector clock)
- Persist client IDs (who made this change)
- Change log storage (linked list or separate table)

**v7 Compatibility**: Must handle both v7 (no versioning) and v7.5 (with versioning).

### New Data Structures

**Change Log Entry**:
```c
typedef struct {
    uint64_t lamport_timestamp;  // Causality ordering
    uint32_t client_id;          // Who made this change
    uint64_t target_uuid;        // Which node was affected
    op_type_t operation;         // INSERT, DELETE, MODIFY, MOVE
    void *operation_data;        // Type-specific operation payload
    timestamp_t created_at;      // Wall-clock time
} change_log_entry_t;
```

**Version Vector** (if using vector clocks):
```c
typedef struct {
    uint32_t client_id;
    uint64_t version_number;
} version_vector_entry_t;  // Array of these
```

### Synchronization Infrastructure

**Server-side** (Phase 7+):
- Change log streaming (send deltas to clients)
- Conflict detection (identify concurrent writes)
- Merge orchestration (apply CRDT algorithm)
- History querying

**Client-side** (Phase 7+):
- Local buffering (queue operations while offline)
- Merge on reconnect (apply changes from server)
- UI updates (show merge progress)

## Testing Strategy

### Phase 6: Script CRDT Tests

**Unit tests**:
- Version bumping monotonic
- Refcount correctness
- Context lifecycle under stress

**Integration tests**:
- Two concurrent script edits, different positions → auto-merge
- Two concurrent script edits, same line → conflict UI
- One user offline, other edits, first user reconnects → auto-merge
- Three concurrent users editing → conflict resolution
- Rapid fire edits (100 ops/sec) → no data loss

**Performance tests**:
- Merge 10,000 operations → < 1 second
- Store/retrieve change log → < 100ms
- Query version history → < 500ms

**Stress tests**:
- 10 concurrent users editing same script
- 100 concurrent users editing different scripts
- Hour-long session with continuous edits
- Network failures and reconnects

### Phase 7: Multi-Type Tests

Apply same test pattern to each ODB type:
- Concurrent edits auto-merge
- Conflict detection works
- Version history correct
- Performance acceptable

### Phase 8: End-to-End Tests

- Dave Winer's workflow (outline + scripts + WPText)
- Automattic use case (config management + audit trail)
- Real-world concurrency patterns
- User acceptance testing with partners

## Success Metrics

### Phase 6
- [ ] Scripts editable concurrently with 99%+ success rate
- [ ] Merge time < 100ms for 100 concurrent operations
- [ ] Zero silent data loss (100% of changes preserved)
- [ ] Version history queryable in < 500ms

### Phase 7
- [ ] All ODB types support concurrent editing
- [ ] Unified CRDT layer handles all types consistently
- [ ] Performance acceptable across all types
- [ ] Test coverage > 90% for new code

### Phase 8
- [ ] Ship to Dave Winer (dogfood in real system)
- [ ] Production deployment with Automattic
- [ ] 10+ concurrent users stable
- [ ] User satisfaction > 90%

## Risk Assessment

### High Risk
1. **Complexity**: CRDT/OT can introduce subtle bugs
   - Mitigation: Start with pure CRDT library, custom implementation later
2. **Performance**: Versioning overhead might be significant
   - Mitigation: Profile early, optimize if needed
3. **User acceptance**: Developers might resist new API
   - Mitigation: Clear documentation, gradual rollout, training

### Medium Risk
1. **Integration**: Difficult to retrofit CRDT into existing code
   - Mitigation: Pattern established in Phase 3, less risky
2. **Testing**: Concurrent scenarios hard to test reliably
   - Mitigation: Use property-based testing, stress test
3. **Storage**: Version history might grow unexpectedly
   - Mitigation: Monitor growth, implement pruning if needed

### Low Risk
1. **Backward compatibility**: Old code must continue working
   - Mitigation: Wrappers maintain old API throughout
2. **Performance regression**: New code might slow down v7 parity
   - Mitigation: Context is lightweight, impact minimal

## Dependencies and Assumptions

**Dependencies**:
- Issue #135 completed (outline context foundation)
- v7 format stable and proven (Phases 3-5)
- CRDT library selection finalized (Phase 6 research)

**Assumptions**:
- Dave Winer partnership continues through Phase 6+
- Automattic interested in collaborative features
- User/UX team available for conflict resolution UI
- Performance requirements achievable (< 100ms merge)

## Related Issues and PRs

- Issue #135: Outline context refactoring (Phase 3 foundation)
- PR #163: menuverbpack refactoring (context pattern proof)
- CRDT Libraries: https://crdt.tech/ (research reference)

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2025-12-25 | 1.0 | Initial roadmap, Phase 6+ planning |

## Questions for Future Planning

1. **Timeline**: Can Phase 6 CRDT prototype be ready by Month 8?
2. **Resources**: How many engineers allocated to CRDT work?
3. **Partners**: Will Dave Winer and Automattic participate in testing?
4. **Success Criteria**: What metrics define "successful" collaboration?
5. **Deployment**: Cloud-based sync vs local-only first?
