# Node Identity Architecture Assessment - Outline Node Persistent Identity for CRDT & Collaboration

## Status
- **State**: 📋 ARCHITECTURAL DECISION PENDING
- **Created**: 2025-12-25
- **Phase**: Pre-Phase 6 (CRDT Foundation Planning)
- **Priority**: STRATEGIC (not blocking Phase 3, but affects future-proofing decisions in #135)

## Executive Summary

You've identified a foundational architectural fork: **Does Frontier need stable, persistent node identity for outline nodes?**

**Current Model (Legacy Frontier):**
- Nodes have **no persistent identity** beyond their position in the tree
- Node references are **positional** ("parent index, child index")
- Op cursor stored as line number (invalidated when structure changes)
- OPML export has `permalink` attributes (RSS-style), but they're **not** connected to database structure
- No way to unambiguously refer to a node across modifications

**Proposed Model (for CRDT + Collaborative ODB):**
- Nodes should have **stable, persistent IDs** (for CRDT merge semantics)
- IDs should be **meaningful and shareable** (like OPML permalinks)
- Cursor/expansion state should **reference IDs, not positions**
- OPML permalinks should be **integrated all the way down to database structure**
- Developers should be able to **unambiguously refer to any node** (debugging, automation, etc.)

**This document analyzes:**
1. Whether we need node identity at all
2. What kind of identity model best fits Frontier's architecture
3. How to make Issue #135 forward-compatible with node identity without blocking it
4. Migration strategy and timeline

---

## Problem Statement

### The CRDT Collaboration Challenge

**Why CRDT needs stable node identity:**

CRDTs (Conflict-free Replicated Data Types) enable collaborative editing by tracking **causal relationships** between operations. For outlines, this means:

1. **Operation targeting**: "User A expanded node X" requires stable ID for node X
2. **Merge semantics**: "User B deleted node Y while User A was moving it" requires stable IDs to resolve conflicts
3. **Undo/redo across replicas**: "Undo expansion of node Z on replica 2" requires node Z's identity to persist across network sync
4. **Shared references**: "User A references node W in a script" - that reference must survive User B's reordering

**Current Frontier approach breaks CRDT:**

```c
// Current approach: store cursor as LINE NUMBER
tyoutlinerecord {
    long lnumbarcursor;  // Line number of bar cursor
    hdlheadrecord hline1;  // Pointer to first visible line
}

// Problem: If User A inserts 3 lines above cursor, User B's cursor is now wrong
// CRDT can't merge because "line 42" is not a stable identity
```

**What CRDTs need:**

```javascript
// Example: Yjs CRDT for collaborative editing
{
  type: 'expand',
  nodeId: 'abc123-def456',  // Stable ID survives all edits
  timestamp: 1234567890,
  clientId: 'user-a'
}
```

---

### The OPML Permalink Disconnect

**Current state**: Frontier **already exports** OPML with permalinks, but they're **ephemeral**:

```xml
<!-- OPML export from Frontier -->
<outline text="Server Config" permalink="http://example.com/stories/123">
  <outline text="Database Setup" permalink="http://example.com/stories/124"/>
</outline>
```

**Problem**: These permalinks are **generated at export time** and have **no connection to the database**:
- You can't round-trip OPML back into Frontier and preserve permalinks
- Sharing an OPML outline between Frontier instances loses identity
- No way to reference "the node at permalink X" from UserTalk code

**Opportunity**: If we integrate permalinks into the database structure, OPML becomes a **first-class interchange format** for collaborative outline work.

---

### The Issue #135 Connection

Issue #135 is about refactoring **op cursor management** from global push/pop stack to deterministic context:

```c
// Current: Global mutable state
hdloutlinerecord outlinedata;  // Global "current outline"
oppushoutline(houtline);       // Change global state
oppopoutline();                // Restore previous state

// Proposed (#135): Explicit context
typedef struct op_context {
    hdloutlinerecord outline;
    // ... cursor state ...
} op_context;
```

**The fork**: What goes into `op_context` for cursor state?

**Option A (no node ID support):**
```c
typedef struct op_context {
    hdloutlinerecord outline;
    hdlheadrecord hbarcursor;      // Memory pointer (breaks on reload)
    long lnumbarcursor;            // Line number (breaks on edits)
} op_context;
```

**Option B (reserve space for future node IDs):**
```c
typedef struct op_context {
    hdloutlinerecord outline;
    hdlheadrecord hbarcursor;      // Memory pointer (for runtime)
    long lnumbarcursor;            // Line number (backward compat)
    union {
        byte reserved[16];         // Reserve space for future UUID/ID
        struct {
            byte node_id[16];      // Will use in Phase 6+
        } future;
    } identity;
} op_context;
```

**Option C (design to be identity-agnostic):**
```c
typedef struct op_context {
    hdloutlinerecord outline;
    void *cursor_ref;              // Opaque reference (could be pointer, ID, etc.)
    enum { REF_POINTER, REF_LINE_NUM, REF_NODE_ID } cursor_ref_type;
} op_context;
```

---

## Architectural Analysis

### Question 1: What kind of node identity model?

I analyzed 4 options:

#### Option A: UUID per node (128-bit)

```c
typedef struct tyheadrecord {
    struct tyheadrecord **headlinkdown, **headlinkup, **headlinkleft, **headlinkright;
    short headlevel;
    // ... flags ...
    byte node_uuid[16];  // NEW: 128-bit UUID
    Handle hrefcon;
    Handle headstring;
} tyheadrecord;
```

**Pros:**
- ✅ Globally unique (can merge outlines from different Frontier instances)
- ✅ No collisions (cryptographically strong randomness)
- ✅ Well-understood (UUID v4 standard)
- ✅ Language-agnostic (every platform has UUID support)

**Cons:**
- ❌ Unfriendly to humans (`550e8400-e29b-41d4-a716-446655440000`)
- ❌ Storage cost (16 bytes per node)
- ❌ Not meaningful (can't infer node purpose from UUID)
- ❌ Requires lookup table for "find node by UUID"

**CRDT fit**: ✅ Excellent (Yjs, Automerge use UUIDs or similar)

---

#### Option B: Stable hash of path (e.g., "server-config/database-setup/connection-pool")

```c
typedef struct tyheadrecord {
    // ... existing fields ...
    uint64_t path_hash;  // NEW: 64-bit hash of full path
} tyheadrecord;
```

**Computed on-demand:**
```c
uint64_t compute_path_hash(hdlheadrecord hnode) {
    char path[4096];
    build_path_to_node(hnode, path, sizeof(path));  // "user.prefs.editor.font"
    return fnv1a_hash64(path);  // Fast non-crypto hash
}
```

**Pros:**
- ✅ Deterministic (same path = same hash across instances)
- ✅ Compact (8 bytes vs 16 for UUID)
- ✅ Debuggable (hash can be reverse-mapped to path for logging)
- ✅ No storage needed if computed on-demand

**Cons:**
- ❌ Collision risk (birthday paradox: ~10^9 nodes for 50% collision)
- ❌ Renaming parent changes all child hashes (breaks references)
- ❌ Ambiguous for duplicate node names (user.prefs, system.prefs)
- ❌ Expensive to compute (must walk tree to root)

**CRDT fit**: ❌ Poor (hash changes break operational semantics)

---

#### Option C: User-defined names/permalinks (like OPML)

```c
typedef struct tyheadrecord {
    // ... existing fields ...
    Handle hpermalink;  // NEW: User-defined permalink (optional)
} tyheadrecord;
```

**UserTalk API:**
```javascript
// User assigns meaningful permalink
op.setPermalink("server-config")
op.getNodeByPermalink("server-config")  // Returns node reference
```

**Pros:**
- ✅ Human-friendly ("server-config" vs "550e8400-e29b...")
- ✅ Meaningful (developer intent encoded in name)
- ✅ OPML-native (maps directly to `<outline permalink="...">`)
- ✅ Optional (only nodes that need identity get one)

**Cons:**
- ❌ Collision risk (two nodes named "config")
- ❌ User discipline required (must assign permalinks manually)
- ❌ Not globally unique (can't merge outlines from different instances)
- ❌ Namespace management (who owns "user" vs "system"?)

**CRDT fit**: ⚠️ Mixed (works if namespace is scoped, breaks on collision)

---

#### Option D: Hybrid (stable hash + optional user-defined permalink)

```c
typedef struct tyheadrecord {
    // ... existing fields ...
    byte node_uuid[16];      // NEW: Auto-generated UUID (always present)
    Handle hpermalink;       // NEW: User-defined permalink (optional)
} tyheadrecord;
```

**Semantics:**
- Every node gets a **UUID automatically** (for CRDT operations)
- Users can **optionally assign a permalink** (for scripts, OPML export)
- Permalink is **advisory** (doesn't change UUID if renamed)
- Both stored in v7 database format

**Pros:**
- ✅ Best of both worlds (UUIDs for CRDT, permalinks for humans)
- ✅ Backward compatible (UUID is invisible to existing code)
- ✅ OPML round-trip preserves both UUID and permalink
- ✅ Scriptable (`op.findByPermalink("config")` or `op.findByUUID(...)`)

**Cons:**
- ❌ Highest storage cost (16 bytes UUID + optional permalink string)
- ❌ Complexity (two identity systems to manage)
- ❌ Potential confusion (which ID to use when?)

**CRDT fit**: ✅ Excellent (UUIDs are canonical, permalinks are metadata)

---

### Architect's Recommendation: Option D (Hybrid)

**Rationale:**

1. **CRDT requirements dictate UUIDs** - Path hashes break on rename, user-defined names have collisions. Only globally unique IDs work for CRDTs.

2. **Human usability requires permalinks** - Developers need `op.findByPermalink("server-config")`, not `op.findByUUID("550e8400-...")`.

3. **OPML already has permalinks** - Integrating them into the database is the natural evolution.

4. **Storage cost is acceptable** - 16 bytes per node is negligible (10,000 nodes = 160KB overhead).

5. **Separates concerns** - UUID for system identity, permalink for user identity.

**Implementation strategy:**

```c
// v7 head record (new format)
#pragma pack(2)
typedef struct tyheadrecord_v7 {
    // Existing fields (unchanged)
    struct tyheadrecord_v7 **headlinkdown, **headlinkup, **headlinkleft, **headlinkright;
    short headlevel;
    // ... bit flags ...
    short hpixels, vpixels;

    // NEW: Node identity (v7 only)
    byte node_uuid[16];      // 128-bit UUID v4 (always present)
    Handle hpermalink;       // Optional user-defined permalink

    // Existing fields (unchanged)
    Handle hrefcon;
    Handle headstring;
} tyheadrecord_v7;
#pragma pack(pop)
```

**Migration strategy (v6 → v7):**
- Generate UUIDs on first migration (deterministic seed from node path to avoid changing UUIDs on re-migration)
- Leave `hpermalink = nil` initially
- User can assign permalinks via UserTalk API post-migration

---

### Question 2: How does this affect Issue #135?

**Minimum viable approach for #135 (doesn't block node identity):**

1. **Design `op_context_t` to be identity-agnostic**:
   ```c
   typedef struct op_context {
       hdloutlinerecord outline;
       hdlheadrecord hbarcursor;    // Runtime pointer (for speed)
       long lnumbarcursor;          // Line number (backward compat)

       // FUTURE: Will add node_uuid[16] in Phase 6
       // For now, use line number + pointer for cursor state
   } op_context;
   ```

2. **Reserve field in `tyheadrecord` for future UUID**:
   ```c
   typedef struct tyheadrecord {
       // ... existing fields ...

       // RESERVED: Will become node_uuid[16] in v7.5 format
       // For now: zeroed, not used
       byte reserved_identity[16];

       Handle hrefcon;
       Handle headstring;
   } tyheadrecord;
   ```

3. **Document in #135 ADR**:
   > "The `op_context_t` design does not assume how nodes are identified. Future
   > work (Phase 6+) will add stable node identity (UUIDs) to support CRDT-based
   > collaborative editing. The context structure reserves space for node IDs but
   > does not require them for Phase 3-5 work."

**This approach:**
- ✅ Does NOT block #135 implementation
- ✅ Does NOT require v7 format change now
- ✅ Signals intent to future developers
- ✅ Avoids premature optimization

---

### Question 3: Database format changes

**Option 1: Add UUID to `tyheadrecord` (v7.5 format bump)**

```c
// v7.5 head record (with node identity)
#pragma pack(2)
typedef struct tyheadrecord {
    // ... existing fields ...
    byte node_uuid[16];      // NEW: 128-bit UUID
    Handle hpermalink;       // NEW: Optional permalink
    Handle hrefcon;
    Handle headstring;
} tyheadrecord;
#pragma pack(pop)
```

**Disk format change:**
- Bump `typortablediskheader.versionnumber` from 4 to 5
- Add UUID to serialized head record (16 bytes per node)
- Add optional permalink to refcon section (if present)

**Migration:**
- v6 → v7 migration generates UUIDs on first pass
- v7 → v7.5 migration adds UUIDs to existing v7 databases

**Pros:**
- ✅ Clean separation (identity is part of node struct)
- ✅ Fast access (no lookup table needed)
- ✅ Obvious to developers (UUID is visible in struct)

**Cons:**
- ❌ Increases head record size (breaks ABI for v7)
- ❌ Requires v7.5 format bump (more migration complexity)
- ❌ Storage cost for ALL nodes (even those that don't need IDs)

---

**Option 2: Store UUIDs in separate lookup table**

```c
// Head record (unchanged)
typedef struct tyheadrecord {
    // ... existing fields (no UUID) ...
    Handle hrefcon;
    Handle headstring;
} tyheadrecord;

// Separate UUID registry (stored in system.internal.nodeIdentity)
typedef struct {
    hashtable uuid_to_node;   // Map: UUID → hdlheadrecord
    hashtable node_to_uuid;   // Map: hdlheadrecord → UUID
} node_identity_registry;
```

**Disk format:**
- No change to `tyheadrecord` serialization
- Add new external table `system.internal.nodeIdentity`
- Store UUID mappings as binary table values

**Pros:**
- ✅ No ABI break (head record unchanged)
- ✅ No v7 format bump needed
- ✅ Storage only for nodes that need IDs
- ✅ Can be added incrementally (Phase 6+)

**Cons:**
- ❌ Indirection cost (lookup table access)
- ❌ Complexity (must maintain two hashtables)
- ❌ Pointer invalidation risk (hdlheadrecord changes on move)
- ❌ Not CRDT-friendly (lookups break operational semantics)

---

**Architect's Recommendation: Option 1 (v7.5 format bump)**

**Rationale:**

1. **CRDT semantics require fast UUID access** - Lookup tables add unacceptable latency for collaborative operations

2. **Storage cost is negligible** - 16 bytes per node is trivial compared to refcon data

3. **Clean abstraction** - UUID is intrinsic to node identity, not external metadata

4. **Forward compatibility** - v7.5 format bump is cleaner than bolting on lookup tables later

5. **Migration timing** - v7.5 bump happens AFTER Phase 3-5 parity work is complete

---

### Question 4: UserTalk API

**Current API (position-based):**
```javascript
op.go(down, 5)              // Move cursor down 5 lines
op.getCursor()              // Returns line number (42)
local(x = op.getLine(42))   // Get text of line 42
```

**Problem**: Line numbers are **not stable references**. Insert 3 lines above and all references break.

**Future API (identity-based):**
```javascript
// UUID-based (system use)
local(id = op.getNodeID(op.getCursor()))  // Get UUID of cursor node
op.gotoNode(id)                            // Jump to node by UUID

// Permalink-based (user use)
op.setPermalink("server-config")           // Assign human-readable permalink
local(node = op.findByPermalink("server-config"))
op.gotoPermalink("server-config")          // Jump to node by permalink

// Mixed (backward compat)
op.getCursor()  // Still returns line number (for existing scripts)
op.getCursorNode()  // NEW: Returns UUID (for new scripts)
```

**Migration path:**
1. **Phase 3-5**: No UserTalk API changes (use existing line numbers)
2. **Phase 6**: Add `op.getNodeID()`, `op.gotoNode()`, `op.setPermalink()`
3. **Phase 7**: Deprecate line number APIs (with warnings)
4. **Phase 8**: Remove line number APIs (breaking change, v8.0)

---

### Question 5: OPML Integration

**Current OPML export (ephemeral permalinks):**
```xml
<opml version="2.0">
  <body>
    <outline text="Server Config" permalink="http://example.com/stories/123"/>
  </body>
</opml>
```

**Problem**: Permalinks are **generated at export time** and not connected to database.

**Future OPML export (persistent permalinks):**
```xml
<opml version="2.0">
  <body>
    <outline
      text="Server Config"
      permalink="server-config"
      _uuid="550e8400-e29b-41d4-a716-446655440000"/>
  </body>
</opml>
```

**Semantics:**
- `permalink`: User-defined human-readable name (optional)
- `_uuid`: System-generated UUID (always present in v7.5+)
- OPML import preserves both (if present)
- OPML export includes both (if present)

**OPML round-trip behavior:**
```
1. Export OPML from Frontier A → includes UUIDs
2. Import OPML to Frontier B → preserves UUIDs
3. Edit outline in Frontier B → UUIDs unchanged
4. Export OPML from Frontier B → same UUIDs
5. Import OPML to Frontier A → merge by UUID (CRDT conflict resolution)
```

**This enables:**
- ✅ Sharing outlines between Frontier instances with identity preservation
- ✅ CRDT-based collaborative editing via OPML interchange
- ✅ Human-readable permalinks for scripting and automation

---

### Question 6: Scalability

**Storage cost analysis:**

```
Assumptions:
- 10,000 nodes per outline (large Frontier database)
- 16 bytes UUID per node
- 20 bytes average permalink (optional, 10% of nodes)

Storage overhead:
- UUIDs: 10,000 nodes × 16 bytes = 160 KB
- Permalinks: 1,000 nodes × 20 bytes = 20 KB
- Total: 180 KB (0.18 MB)

For comparison:
- Average refcon size: ~200 bytes per node = 2 MB
- Average text size: ~100 bytes per node = 1 MB
- Total outline size: ~3-5 MB

Identity overhead: 180 KB / 3 MB = 6% of total outline size
```

**Performance cost analysis:**

```
UUID generation (v6→v7 migration):
- 10,000 nodes × 1 µs per UUID = 10 ms (negligible)

UUID comparison (CRDT merge):
- memcmp(uuid1, uuid2, 16) = ~5 CPU cycles = ~2 ns
- 10,000 comparisons = 20 µs (negligible)

Permalink lookup:
- Hashtable lookup = O(1) = ~100 ns
- 1,000 lookups = 100 µs (negligible)
```

**Conclusion**: Storage and performance costs are **negligible** compared to outline data size and existing operations.

---

## Recommendations

### Immediate (Issue #135 - Phase 3)

1. **Design `op_context_t` to be identity-agnostic**:
   - Use `hdlheadrecord` pointers for runtime cursor state
   - Use line numbers for serialized cursor state (backward compat)
   - Do NOT add UUID fields yet (premature)

2. **Reserve space in `tyheadrecord` for future UUID**:
   - Add `byte reserved_identity[16]` to struct
   - Zero-initialize, do not use in Phase 3-5
   - Document as "reserved for Phase 6+ node identity"

3. **Document in #135 ADR**:
   - Note that cursor management is **not** tied to node identity model
   - Signal that Phase 6+ will add UUIDs for CRDT support
   - Reserve design space for future node identity integration

**This approach does NOT block #135 and requires minimal code changes now.**

---

### Near-term (Phase 4-5)

1. **Prototype UUID generation** (exploratory, not committed):
   - Create `tools/uuid_generator.c` with UUID v4 implementation
   - Add migration test that generates UUIDs for v6 outline nodes
   - Validate storage overhead and performance cost
   - Do NOT integrate into production migration yet

2. **Research CRDT libraries**:
   - Evaluate Yjs, Automerge, Diamond Types for outline CRDT semantics
   - Determine if they require UUIDs or support other identity models
   - Document findings in `planning/phase6/CRDT_EVALUATION.md`

3. **OPML permalink audit**:
   - Survey existing Frontier OPML exports to understand permalink usage
   - Determine if users rely on current permalink behavior
   - Assess impact of changing permalink semantics (ephemeral → persistent)

---

### Long-term (Phase 6+ - CRDT Foundation)

1. **Implement hybrid UUID + permalink model**:
   - Bump v7 format to v7.5 (add UUID to `tyheadrecord`)
   - Generate UUIDs on v7 → v7.5 migration
   - Add UserTalk API for permalink management
   - Integrate permalinks into OPML export/import

2. **Refactor cursor management to use UUIDs**:
   - Update `op_context_t` to include `node_uuid[16]`
   - Change serialized cursor state to use UUID instead of line number
   - Maintain line number API for backward compatibility (deprecated)

3. **Implement CRDT-based collaborative editing**:
   - Choose CRDT library (Yjs, Automerge, or custom)
   - Map Frontier outline operations to CRDT operations
   - Implement network sync protocol (WebSocket, PartyKit, etc.)
   - Add conflict resolution UI

4. **OPML as collaboration interchange**:
   - Export OPML with UUIDs and permalinks
   - Import OPML preserving UUIDs
   - Implement OPML-based merge (CRDT conflict resolution)

---

## Decision Matrix

| Decision | Phase 3 (#135) | Phase 4-5 | Phase 6+ (CRDT) |
|----------|----------------|-----------|-----------------|
| **Add UUID to tyheadrecord?** | ❌ No (reserve space only) | ⚠️ Prototype only | ✅ Yes (v7.5 format) |
| **Generate UUIDs on migration?** | ❌ No | ⚠️ Test harness only | ✅ Yes (production) |
| **Expose UserTalk API for UUIDs?** | ❌ No | ❌ No | ✅ Yes |
| **Integrate permalinks into DB?** | ❌ No | ⚠️ Prototype only | ✅ Yes |
| **Change OPML export format?** | ❌ No | ⚠️ Experimental only | ✅ Yes |
| **Refactor op_context for UUIDs?** | ❌ No (design to not block) | ❌ No | ✅ Yes |

---

## Risk Assessment

### Risk: Premature optimization in #135

**Likelihood**: MEDIUM
**Impact**: LOW

**Mitigation**:
- Do NOT add UUID fields to `op_context_t` in Phase 3
- Reserve space in `tyheadrecord` but do not use it
- Document design intent without committing to implementation

---

### Risk: v7.5 format bump breaks compatibility

**Likelihood**: HIGH (if we do v7.5)
**Impact**: MEDIUM

**Mitigation**:
- Implement v7.5 AFTER Phase 3-5 parity work is complete
- Provide v7 → v7.5 migration tool (separate from v6 → v7)
- Support reading BOTH v7 and v7.5 formats in reader code
- Bump format version incrementally (not a breaking change)

---

### Risk: CRDT libraries don't support Frontier's outline model

**Likelihood**: MEDIUM
**Impact**: HIGH

**Mitigation**:
- Research CRDT libraries in Phase 4-5 (before committing to v7.5)
- Prototype CRDT integration with small test outlines
- Consider custom CRDT implementation if libraries don't fit
- Document CRDT requirements in `planning/phase6/CRDT_REQUIREMENTS.md`

---

### Risk: Permalink collisions in user-defined names

**Likelihood**: HIGH (if we use permalinks alone)
**Impact**: MEDIUM

**Mitigation**:
- Use UUIDs as **canonical** identity (no collisions possible)
- Treat permalinks as **advisory** (user convenience, not system identity)
- Validate permalink uniqueness at assignment time (UserTalk API check)
- Provide "find duplicate permalinks" diagnostic tool

---

## Open Questions

1. **Should UUIDs be deterministic or random?**
   - Random (UUID v4): Globally unique, merge-friendly, but re-migration changes UUIDs
   - Deterministic (UUID v5 from path): Reproducible, but breaks on rename

2. **Should permalinks be scoped (namespaced)?**
   - Global scope: Simple, but collision risk
   - Per-outline scope: Safer, but less shareable across outlines

3. **Should OPML export include UUIDs by default or opt-in?**
   - Default: Maximizes interoperability, but bloats OPML size
   - Opt-in: Cleaner OPML, but users must know to enable

4. **Should v7.5 format be mandatory or optional upgrade?**
   - Mandatory: All v7 databases auto-upgrade to v7.5 on first open
   - Optional: Users opt-in to v7.5 (preserves v7 compatibility)

5. **Should node identity be exposed in UserTalk as first-class type?**
   - Yes: `typeof(x) == nodeIDType`, dedicated operators
   - No: Treat as opaque string/binary (simpler API, less type system changes)

---

## Recommendations for Issue #135

**Summary**: Issue #135 should proceed **without** node identity support, but should be designed to **not block** future node identity integration.

**Specific guidance for #135 implementation:**

1. **Do NOT add UUID fields to `op_context_t`** - Use existing line number + pointer approach

2. **Design `op_context_t` to be identity-agnostic** - Don't assume cursor is always a pointer or line number

3. **Reserve space in `tyheadrecord.reserved_identity[16]`** - Signal future UUID integration

4. **Document in #135 ADR**:
   ```markdown
   ## Forward Compatibility: Node Identity

   This design does not assume how outline nodes are identified. Future work
   (Phase 6+) may add stable node identity (UUIDs) to support CRDT-based
   collaborative editing. The `op_context_t` structure is designed to be
   identity-agnostic and does not preclude adding UUID-based cursor references.

   Space is reserved in `tyheadrecord.reserved_identity[16]` for future UUID
   storage, but is not used in Phase 3-5 implementations.
   ```

5. **Add test for "cursor survives reload"** - Validates that cursor state can be serialized/deserialized (prepares for UUID-based cursor in Phase 6)

**This approach:**
- ✅ Does NOT delay #135 implementation
- ✅ Does NOT require v7 format changes now
- ✅ Signals architectural intent for Phase 6+
- ✅ Avoids premature optimization
- ✅ Keeps options open for CRDT integration

---

## Conclusion

**The fork is real, but we can defer the decision.**

You're absolutely right that node identity is **foundational** for collaborative outline editing. BUT:

1. **Phase 3-5 does NOT need node identity** - We can achieve v6 parity without UUIDs
2. **Issue #135 can be designed to not block node identity** - Reserve space, document intent, move on
3. **Phase 6+ is the right time for v7.5 format bump** - After parity is proven, before CRDT integration
4. **Hybrid UUID + permalink model is the right architecture** - UUIDs for CRDT, permalinks for humans

**Immediate action for #135:**
- Proceed with context refactor as planned
- Reserve `tyheadrecord.reserved_identity[16]` field (zeroed, unused)
- Document forward compatibility in ADR
- Do NOT add UUID logic yet

**Next steps for node identity:**
- Phase 4-5: Research CRDT libraries, prototype UUID generation
- Phase 6: Implement v7.5 format with UUIDs, UserTalk API
- Phase 7: Integrate OPML permalinks, implement collaborative editing

This gives us maximum flexibility without blocking current work.

---

## References

- Issue #135: Refactor outline (op) management from push/pop to deterministic context model
- OPML 2.0 Spec: http://opml.org/spec2.opml
- Yjs CRDT: https://github.com/yjs/yjs
- Automerge CRDT: https://github.com/automerge/automerge
- UUID v4 Spec: RFC 4122
- `planning/phase3/EXTERNAL_OBJECT_TEST_COVERAGE_PLAN.md` - Refcon architecture
- `Common/headers/op.h` - Current outline data structures
- `Common/source/oppack_v7.c` - v7 outline serialization format
