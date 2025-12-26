# CRDT Implementation Roadmap for Frontier's Collaborative ODB

**Status**: Planning (Phase 6+ Implementation)
**Date**: 2025-12-25
**Author**: System Architect
**Related Documents**:
- `CONTEXT_PATTERN_FOR_ODB_COLLABORATION.md` - Operation context pattern
- `OUTLINE_OPERATION_CONTEXT.md` - Phase 3 implementation
- `NODE_IDENTITY_ARCHITECTURE_ASSESSMENT.md` - UUID infrastructure
- `../CRDT_FOUNDATION_ROADMAP.md` - Strategic vision

## Executive Summary

This document describes how Frontier's operation context foundation (Phases 1-3) will be extended to support Conflict-Free Replicated Data Type (CRDT) based collaborative editing across the entire Object Database (ODB). The approach builds incrementally on proven infrastructure, starting with scripts in Phase 6 and expanding to all six ODB types by Phase 8.

**Key Insight**: The `op_context_t` pattern implemented in Phase 3 is not just refactoring—it's the foundation that makes Google Docs-style collaboration possible without rewriting the entire system.

## Table of Contents

1. [How Operation Context Enables CRDT](#how-operation-context-enables-crdt)
2. [Additional Infrastructure Needed](#additional-infrastructure-needed)
3. [Reserved Space Utilization](#reserved-space-utilization)
4. [Phase-by-Phase Roadmap](#phase-by-phase-roadmap)
5. [Real-World Scenarios](#real-world-scenarios)
6. [Implementation Examples](#implementation-examples)
7. [Technical Deep Dive](#technical-deep-dive)
8. [Risk Assessment and Mitigations](#risk-assessment-and-mitigations)

---

## How Operation Context Enables CRDT

### The Foundation: Three Pillars

The `op_context_t` structure (implemented in Phase 3) provides three critical capabilities for CRDT-based collaboration:

#### 1. Fine-Grained Versioning

```c
typedef struct op_context_t {
    _Atomic uint64_t version;  // Increments on EVERY mutation
    // ...
} op_context_t;
```

**What this enables:**
- **Operation ordering**: Server knows "edit A happened before edit B"
- **Conflict detection**: "User A's edit conflicts with User B's edit at version 42"
- **Optimistic locking**: "I'm editing version 42, has anyone else modified it?"
- **Efficient sync**: "Send me all operations since version 100"

**Example scenario:**
```
User A starts editing at version 42
User A makes 5 local changes → local version 47
Meanwhile, User B makes 3 server changes → server version 45
User A syncs: "I have 42-47, server has 42-45, conflict detected at 43+"
```

#### 2. Operation-Scoped Metadata

```c
typedef struct op_context_t {
    void *reserved[8];  // 64 bytes for Phase 6+ features
    // ...
} op_context_t;
```

**What this enables:**
- **Client identification**: Which user made this change?
- **Causality tracking**: Which operations must come before others?
- **Transaction grouping**: These 5 operations are atomic
- **Conflict resolution hints**: Prefer local/remote/merge strategy

**Example scenario:**
```
Operation 1: User A inserts "Hello" at position 0
  → metadata: {client_id: A, timestamp: 100, parent_version: 42}
Operation 2: User B inserts "World" at position 0
  → metadata: {client_id: B, timestamp: 101, parent_version: 42}
CRDT algorithm uses metadata to determine: "A happened first, then B"
Final state: "HelloWorld" (not "WorldHello")
```

#### 3. Stable Node Identity (Reserved)

```c
// In tyheadrecord (outline nodes)
typedef struct tyheadrecord {
    byte reserved_identity[16];  // Will become node_uuid in v7.5
    // ...
} tyheadrecord;
```

**What this enables:**
- **Cross-version references**: Node ID survives renames, moves, structural changes
- **Merge without ambiguity**: "Edit the node with UUID abc123, regardless of position"
- **Distributed editing**: Different instances can reference same logical node

**Example scenario:**
```
Initial state: Node "Config" has UUID abc123def456
User A: Renames "Config" → "Settings" (UUID unchanged)
User B: Edits text in "Config" node (references UUID abc123def456)
Merge: User B's edit applies to "Settings" because UUID matches
```

### Why Traditional Approaches Fail

**Without operation context**, collaboration requires:
- **Pessimistic locking**: User A locks entire document, User B waits
- **Last-write-wins**: User B overwrites User A's changes (data loss)
- **Manual merging**: Users resolve conflicts manually (like Git)

**With operation context**, we get:
- **Optimistic merging**: Both users edit freely, conflicts auto-resolved
- **No data loss**: All edits preserved, merged intelligently
- **Transparent to users**: Feels like single-user editing

---

## Additional Infrastructure Needed

### 1. Vector Clocks for Causality Tracking

**Purpose**: Determine which operations happened before others (partial order)

**Data Structure**:
```c
typedef struct {
    uint32_t client_id;          // Which client
    uint64_t version_number;     // Latest version seen from that client
} version_vector_entry_t;

typedef struct {
    uint32_t num_clients;
    version_vector_entry_t entries[MAX_CLIENTS];
} version_vector_t;
```

**How it works**:
```
Client A: {A:5, B:0, C:0}  // "I've seen A's version 5, nothing from B/C"
Client B: {A:3, B:2, C:0}  // "I've seen A's version 3, B's version 2"
Client C: {A:5, B:2, C:1}  // "I've seen A's 5, B's 2, my own version 1"

Operation from A with vector {A:5, B:0, C:0}
Operation from B with vector {A:3, B:2, C:0}

Server determines:
- A's operation happened after seeing A:5 (but before B's B:2)
- B's operation happened after seeing A:3, B:2
- A's operation is concurrent with B's (neither causally depends on the other)
```

**Where it lives**: `op_context_t.reserved[0]` → pointer to `version_vector_t`

**Phase 6 implementation**: Start with simple Lamport timestamps (single counter), upgrade to vector clocks in Phase 7.

### 2. Change Logs for Operation Replay

**Purpose**: Record every operation for conflict resolution and history queries

**Data Structure**:
```c
typedef struct {
    uint64_t lamport_timestamp;  // Total order timestamp
    uint32_t client_id;          // Who made this change
    byte target_uuid[16];        // Which node was affected
    op_type_t operation;         // INSERT, DELETE, MODIFY, MOVE
    version_vector_t version;    // Vector clock at operation time
    void *operation_data;        // Type-specific payload
    timestamp_t created_at;      // Wall-clock time (debugging)
} change_log_entry_t;

typedef struct {
    uint32_t num_entries;
    uint32_t capacity;
    change_log_entry_t *entries;  // Dynamic array
} change_log_t;
```

**Storage strategy**:
- **In-memory during editing**: Changes append to `change_log_t` in `op_context_t.reserved[1]`
- **Persist to database**: On save, serialize change log to `system.internal.changeLogs.<object_id>`
- **Pruning**: Keep last N days of changes, archive older entries

**Example log**:
```
Entry 0: {ts:100, client:A, uuid:abc123, op:INSERT, data:"Hello"}
Entry 1: {ts:101, client:B, uuid:abc123, op:MODIFY, data:"World"}
Entry 2: {ts:102, client:A, uuid:def456, op:DELETE}
```

**Query scenarios**:
- "What changed between version 100 and 150?" → Filter entries with ts ∈ [100, 150]
- "Who edited node abc123?" → Filter entries with uuid=abc123
- "Show me all User A's changes" → Filter entries with client_id=A

### 3. Conflict Resolution Strategies

**Purpose**: Decide what to do when concurrent edits collide

**Types of conflicts**:

#### Type A: Non-Conflicting (Auto-Merge)
```
User A: Set node text to "Hello"
User B: Set node refcon to 0x1234
→ Merge: Both changes apply (different fields)
```

#### Type B: Field-Level Conflict (Deterministic Merge)
```
User A: Set node text to "Hello"
User B: Set node text to "World"
→ Strategy: Last-write-wins (based on Lamport timestamp)
→ Result: "World" (if B's timestamp > A's timestamp)
```

#### Type C: Structural Conflict (CRDT Algorithm)
```
User A: Insert "X" at position 5
User B: Insert "Y" at position 5
→ CRDT strategy: Ordered by client_id (A < B) or timestamp
→ Result: "XY" at position 5 (consistent across all clients)
```

#### Type D: Semantic Conflict (Escalate to User)
```
User A: Delete node "Config"
User B: Edit text in node "Config"
→ Cannot auto-resolve (delete vs modify)
→ UI: "User B edited node that User A deleted. Keep edit?"
```

**Conflict resolution matrix**:
```c
typedef enum {
    CONFLICT_AUTO_MERGE,      // Different fields, no conflict
    CONFLICT_LWW,             // Last-write-wins
    CONFLICT_CRDT,            // Use CRDT merge algorithm
    CONFLICT_USER_RESOLVE,    // Show conflict resolution UI
} conflict_resolution_strategy_t;

typedef struct {
    op_type_t op_a;
    op_type_t op_b;
    conflict_resolution_strategy_t strategy;
} conflict_rule_t;

static conflict_rule_t conflict_rules[] = {
    {OP_MODIFY_TEXT, OP_MODIFY_REFCON, CONFLICT_AUTO_MERGE},
    {OP_MODIFY_TEXT, OP_MODIFY_TEXT,   CONFLICT_CRDT},
    {OP_DELETE,      OP_MODIFY_TEXT,   CONFLICT_USER_RESOLVE},
    // ...
};
```

**Where it lives**: `op_context_t.reserved[2]` → pointer to conflict resolution context

### 4. Client Session Management

**Purpose**: Track which clients are active, assign unique IDs

**Data Structure**:
```c
typedef struct {
    uint32_t client_id;          // Unique per-session ID
    byte session_uuid[16];       // UUID for session
    char username[64];           // Human-readable user
    timestamp_t session_start;   // When session started
    timestamp_t last_heartbeat;  // Last activity
    version_vector_t last_seen;  // What versions this client has seen
} client_session_t;

typedef struct {
    uint32_t num_clients;
    client_session_t clients[MAX_CLIENTS];
} session_registry_t;
```

**Session lifecycle**:
1. **Client connects**: Server assigns `client_id`, creates session
2. **Client edits**: All operations tagged with `client_id`
3. **Client syncs**: Server sends operations from other clients
4. **Client disconnects**: Session marked inactive, pruned after timeout

**Where it lives**: Server-side registry, not in `op_context_t` (server manages sessions)

**Client-side**: `op_context_t.reserved[3]` → pointer to `my_session_info_t`

---

## Reserved Space Utilization

### `op_context_t.reserved[8]` (64 bytes)

Current state (Phase 3): All reserved fields are `NULL`

Planned use (Phase 6+):

```c
typedef struct op_context_t {
    _Atomic uint32_t refcount;
    _Atomic uint64_t version;
    uint32_t flags;
    const char *source_file;
    uint32_t source_line;

    // Reserved space allocation:
    void *reserved[8];
    // ↓ Phase 6+ usage ↓
    // reserved[0] → version_vector_t *vector_clock
    // reserved[1] → change_log_t *change_log
    // reserved[2] → conflict_context_t *conflict_ctx
    // reserved[3] → session_info_t *my_session
    // reserved[4] → transaction_id_t *txn_id
    // reserved[5] → lock_owner_t *lock_owner
    // reserved[6] → crdt_metadata_t *crdt_meta
    // reserved[7] → (unused, future expansion)
} op_context_t;
```

**Detailed field assignments**:

#### `reserved[0]`: Vector Clock
```c
typedef struct version_vector_t {
    uint32_t num_clients;
    version_vector_entry_t *entries;  // Dynamic array
} version_vector_t;
```
**Size**: 8 bytes (pointer)
**Lifecycle**: Allocated in `op_context_acquire()` if CRDT enabled

#### `reserved[1]`: Change Log Pointer
```c
typedef struct change_log_t {
    uint32_t num_entries;
    uint32_t capacity;
    change_log_entry_t *entries;
} change_log_t;
```
**Size**: 8 bytes (pointer)
**Lifecycle**: Allocated on first mutation, persisted on save

#### `reserved[2]`: Conflict Resolution Context
```c
typedef struct conflict_context_t {
    conflict_resolution_strategy_t strategy;
    void *local_state;   // Local version of conflicting data
    void *remote_state;  // Remote version of conflicting data
    void *merged_state;  // Merged result (if auto-resolved)
} conflict_context_t;
```
**Size**: 8 bytes (pointer)
**Lifecycle**: Allocated only when conflict detected

#### `reserved[3]`: Session Info
```c
typedef struct session_info_t {
    uint32_t my_client_id;
    byte my_session_uuid[16];
    timestamp_t session_start;
} session_info_t;
```
**Size**: 8 bytes (pointer)
**Lifecycle**: Allocated on session start, copied to each context

#### `reserved[4]`: Transaction ID
```c
typedef struct transaction_id_t {
    byte txn_uuid[16];
    uint32_t num_operations;
} transaction_id_t;
```
**Size**: 8 bytes (pointer)
**Lifecycle**: Allocated when multi-operation transaction starts

#### `reserved[5]`: Lock Owner
```c
typedef struct lock_owner_t {
    uint32_t owner_client_id;
    lock_type_t lock_type;  // READ, WRITE, EXCLUSIVE
    timestamp_t acquired_at;
} lock_owner_t;
```
**Size**: 8 bytes (pointer)
**Lifecycle**: Allocated when lock acquired, freed on release

#### `reserved[6]`: CRDT Metadata
```c
typedef struct crdt_metadata_t {
    crdt_type_t type;  // YJS, AUTOMERGE, CUSTOM_OT
    void *crdt_state;  // Type-specific CRDT state
} crdt_metadata_t;
```
**Size**: 8 bytes (pointer)
**Lifecycle**: Allocated if using external CRDT library

#### `reserved[7]`: Future Expansion
**Size**: 8 bytes (pointer)
**Lifecycle**: Reserved for Phase 8+ features

### `tyheadrecord.reserved_identity[16]` (16 bytes)

Current state (Phase 3): Zeroed, unused

Planned use (Phase 6+):

```c
typedef struct tyheadrecord {
    // ... existing fields ...

    // Phase 6+: Will become node UUID
    byte reserved_identity[16];  // Currently zero-filled
    // ↓ Will be renamed to: ↓
    // byte node_uuid[16];  // UUID v4 (128-bit)

    Handle hrefcon;
    Handle headstring;
} tyheadrecord;
```

**UUID generation strategy**:
- **Format**: UUID v4 (random 128-bit)
- **When generated**: On node creation (v7.5+ format)
- **Migration**: v6→v7.5 generates deterministic UUIDs based on node path
- **Persistence**: Stored in v7.5 database format

**Why 16 bytes**:
- UUID v4 standard size
- Globally unique across all Frontier instances
- Collision probability: ~0% for practical node counts
- Matches CRDT library expectations (Yjs, Automerge)

**Example UUID**:
```
Node "System Preferences" → UUID: 550e8400-e29b-41d4-a716-446655440000
- Even if renamed to "Settings", UUID stays 550e8400...
- Even if moved to different parent, UUID stays 550e8400...
- User A's edit references UUID 550e8400... → guaranteed to find node
```

---

## Phase-by-Phase Roadmap

### Phase 3 (COMPLETED): Foundation Layer

**Goal**: Establish operation context pattern for outlines

**Deliverables**:
- ✅ `op_context_t` structure (Issue #135)
- ✅ Atomic refcounting and fine-grained versioning
- ✅ 64-byte reserved space for Phase 6+ metadata
- ✅ 16-byte UUID reservation in outline nodes
- ✅ ADRs documenting pattern and intent

**Success Criteria**:
- ✅ Outline operations use context pattern
- ✅ Backward compatibility maintained (old API works)
- ✅ Zero regressions in v7 functionality
- ✅ Pattern documented clearly for future extension

**NOT in Phase 3**:
- ❌ UUID implementation (space reserved only)
- ❌ CRDT operations (versioning enables, not implements)
- ❌ Multi-user testing (single-threaded only)
- ❌ Synchronization infrastructure

### Phase 4-5: Complete v7 Parity

**Goal**: Ship functional v7 runtime without collaboration

**Deliverables**:
- Complete kernel verb implementation
- String/text modernization (UTF-8 support)
- Database validation and migration tools

**Collaboration work**: NONE (defer to Phase 6+)

**Status**: In Progress (Q1 2026 estimated)

### Phase 6: CRDT Research & Script Prototype (6-8 weeks)

**Goal**: Prove CRDT concept works with simplest ODB type (scripts)

#### Week 1: Script Context Foundation

**Tasks**:
1. Create `script_context_t` following `op_context_t` pattern
   ```c
   typedef struct script_context_t {
       _Atomic uint32_t refcount;
       _Atomic uint64_t version;
       uint32_t flags;

       // Script-specific fields
       hdlscriptrecord script;
       uint32_t cursor_position;
       uint32_t selection_start;
       uint32_t selection_end;

       // Reserved for Phase 6+
       void *reserved[8];
   } script_context_t;
   ```

2. Integrate into `scriptins`, `scriptdelete`, `scriptreplace` operations
   ```c
   boolean scriptins_ctx(script_context_t *ctx, long pos, char *text, long len);
   boolean scriptdelete_ctx(script_context_t *ctx, long start, long end);
   boolean scriptreplace_ctx(script_context_t *ctx, long start, long end, char *text, long len);
   ```

3. Add backward-compat wrappers
   ```c
   boolean scriptins(long pos, char *text, long len) {
       script_context_t *ctx = script_context_acquire(SCRIPT_CONTEXT_NORMAL);
       boolean result = scriptins_ctx(ctx, pos, text, len);
       script_context_release(ctx);
       return result;
   }
   ```

4. Version tracking on every text operation
   ```c
   boolean scriptins_ctx(script_context_t *ctx, long pos, char *text, long len) {
       // Insert text...
       script_context_version_bump(ctx);  // Increment version
       return true;
   }
   ```

**Success Criteria**:
- ✅ Scripts use context pattern
- ✅ Version increments on every edit
- ✅ Zero regressions in script tests

#### Weeks 2-3: CRDT Research & Library Selection

**Evaluation criteria**:

| Library | Pros | Cons | Verdict |
|---------|------|------|---------|
| **Yjs** | - Proven (Google Docs uses similar)<br>- Rich text support<br>- Active community | - Large memory footprint<br>- Requires WebSocket server<br>- JavaScript-first (C binding needed) | **Top choice** |
| **Automerge** | - Pure CRDT (no central server)<br>- Offline-first<br>- C library available | - Heavier than Yjs<br>- Complex merge algorithm<br>- Learning curve | **Backup choice** |
| **Custom OT** | - Lighter weight<br>- Full control | - Hard to implement correctly<br>- More testing needed<br>- Reinventing wheel | **Last resort** |

**Decision process**:
1. Prototype with Yjs (Week 2)
2. Test with 2 concurrent script editors
3. Measure memory overhead and latency
4. If Yjs acceptable → commit to Yjs
5. If Yjs too heavy → try Automerge (Week 3)
6. If both fail → custom OT (Phase 7 work)

**Deliverable**: ADR documenting CRDT library choice with rationale

#### Weeks 4-5: Version Infrastructure

**Tasks**:

1. **Lamport Timestamps** (simple causality)
   ```c
   typedef struct {
       uint64_t timestamp;  // Monotonic counter
       uint32_t client_id;  // Break ties
   } lamport_clock_t;

   void lamport_tick(lamport_clock_t *clock) {
       clock->timestamp++;
   }

   void lamport_update(lamport_clock_t *clock, uint64_t remote_ts) {
       clock->timestamp = max(clock->timestamp, remote_ts) + 1;
   }
   ```

2. **Client ID / Session Tracking**
   ```c
   typedef struct {
       uint32_t client_id;
       byte session_uuid[16];
       char username[64];
   } client_session_t;

   client_session_t* session_start(const char *username) {
       client_session_t *session = malloc(sizeof(client_session_t));
       session->client_id = next_client_id++;
       uuid_generate(session->session_uuid);
       strncpy(session->username, username, 64);
       return session;
   }
   ```

3. **Change Log Storage**
   ```c
   typedef struct {
       uint64_t lamport_ts;
       uint32_t client_id;
       op_type_t operation;
       long position;
       char *text;
       long text_len;
   } change_log_entry_t;

   void log_change(script_context_t *ctx, op_type_t op, long pos, char *text, long len) {
       change_log_t *log = ctx->reserved[1];
       change_log_entry_t entry = {
           .lamport_ts = lamport_tick(ctx->reserved[0]),
           .client_id = ctx->my_client_id,
           .operation = op,
           .position = pos,
           .text = strdup(text),
           .text_len = len
       };
       append_to_log(log, entry);
   }
   ```

4. **Populate reserved fields in `script_context_t`**
   ```c
   script_context_t* script_context_acquire(uint32_t flags) {
       script_context_t *ctx = calloc(1, sizeof(script_context_t));
       ctx->refcount = 1;
       ctx->version = 0;
       ctx->flags = flags;

       // Phase 6: Allocate CRDT infrastructure
       ctx->reserved[0] = calloc(1, sizeof(lamport_clock_t));  // Lamport clock
       ctx->reserved[1] = calloc(1, sizeof(change_log_t));     // Change log
       ctx->reserved[3] = calloc(1, sizeof(session_info_t));   // Session info

       return ctx;
   }
   ```

**Success Criteria**:
- ✅ Lamport timestamps increment on every operation
- ✅ Client ID assigned and tracked
- ✅ Change log records all script edits
- ✅ Reserved fields populated correctly

#### Weeks 6-8: Script Sync Prototype

**Architecture**:
```
┌─────────────┐         ┌─────────────┐
│  Client A   │         │  Client B   │
│             │         │             │
│ ┌─────────┐ │         │ ┌─────────┐ │
│ │ Script  │ │         │ │ Script  │ │
│ │ Context │ │         │ │ Context │ │
│ └────┬────┘ │         │ └────┬────┘ │
│      │      │         │      │      │
│  ┌───▼────┐ │         │  ┌───▼────┐ │
│  │ CRDT   │ │         │  │ CRDT   │ │
│  │ Engine │ │         │  │ Engine │ │
│  └───┬────┘ │         │  └───┬────┘ │
└──────┼──────┘         └──────┼──────┘
       │                       │
       └───────────┬───────────┘
                   │
          ┌────────▼────────┐
          │  Sync Server    │
          │                 │
          │ ┌─────────────┐ │
          │ │ Change Log  │ │
          │ │ Distributor │ │
          │ └─────────────┘ │
          └─────────────────┘
```

**Tasks**:

1. **Two-Client Test Harness**
   ```c
   // Test: Two clients edit same script concurrently
   void test_concurrent_script_edit(void) {
       // Client A setup
       script_context_t *ctx_a = script_context_acquire(0);
       ctx_a->my_client_id = 1;

       // Client B setup
       script_context_t *ctx_b = script_context_acquire(0);
       ctx_b->my_client_id = 2;

       // Client A: Insert "Hello" at position 0
       scriptins_ctx(ctx_a, 0, "Hello", 5);

       // Client B: Insert "World" at position 0 (concurrent!)
       scriptins_ctx(ctx_b, 0, "World", 5);

       // Merge changes
       merge_contexts(ctx_a, ctx_b);

       // Verify: Both clients see same result
       assert(strcmp(ctx_a->text, ctx_b->text) == 0);
       assert(strcmp(ctx_a->text, "HelloWorld") == 0);  // Or "WorldHello" depending on CRDT
   }
   ```

2. **Concurrent Changes Automatically Merged**
   ```c
   void merge_contexts(script_context_t *local, script_context_t *remote) {
       change_log_t *local_log = local->reserved[1];
       change_log_t *remote_log = remote->reserved[1];

       // Find changes remote has that local doesn't
       for (int i = 0; i < remote_log->num_entries; i++) {
           change_log_entry_t *entry = &remote_log->entries[i];

           // If local hasn't seen this change, apply it
           if (entry->lamport_ts > local->last_seen_ts) {
               apply_change(local, entry);
           }
       }

       // Update local's view of remote
       local->last_seen_ts = remote->lamport_ts;
   }
   ```

3. **Version History Queryable**
   ```c
   void query_history(script_context_t *ctx, uint64_t from_version, uint64_t to_version) {
       change_log_t *log = ctx->reserved[1];

       printf("Changes between version %llu and %llu:\n", from_version, to_version);
       for (int i = 0; i < log->num_entries; i++) {
           change_log_entry_t *entry = &log->entries[i];

           if (entry->lamport_ts >= from_version && entry->lamport_ts <= to_version) {
               printf("  [%llu] Client %u: %s at pos %ld\n",
                      entry->lamport_ts, entry->client_id,
                      op_type_name(entry->operation), entry->position);
           }
       }
   }
   ```

4. **Conflict Detection Working**
   ```c
   typedef enum {
       CONFLICT_NONE,
       CONFLICT_INSERT_INSERT,  // Both insert at same position
       CONFLICT_DELETE_MODIFY,  // One deletes, one modifies
       CONFLICT_MOVE_DELETE,    // One moves, one deletes
   } conflict_type_t;

   conflict_type_t detect_conflict(change_log_entry_t *a, change_log_entry_t *b) {
       // Same Lamport timestamp = concurrent operations
       if (a->lamport_ts != b->lamport_ts) {
           return CONFLICT_NONE;  // Not concurrent, no conflict
       }

       // Both insert at same position
       if (a->operation == OP_INSERT && b->operation == OP_INSERT &&
           a->position == b->position) {
           return CONFLICT_INSERT_INSERT;
       }

       // One deletes, one modifies same range
       if ((a->operation == OP_DELETE && b->operation == OP_MODIFY) ||
           (a->operation == OP_MODIFY && b->operation == OP_DELETE)) {
           if (ranges_overlap(a->position, a->text_len, b->position, b->text_len)) {
               return CONFLICT_DELETE_MODIFY;
           }
       }

       return CONFLICT_NONE;
   }
   ```

**Test scenarios**:
- ✅ Two users insert at different positions → auto-merge
- ✅ Two users insert at same position → CRDT resolves
- ✅ One user offline, other edits, first reconnects → auto-merge
- ✅ Three concurrent users editing → all merge correctly
- ✅ Rapid-fire edits (100 ops/sec) → no data loss

**Success Criteria**:
- Scripts editable by 2+ users concurrently
- Edits merged correctly (no data loss)
- Version history queryable
- Conflicts detected and handled
- Performance acceptable (< 100ms latency)

**Decisions to Make**:
1. **CRDT Library**: Use Yjs/Automerge or implement custom?
   - **Recommendation**: Start with Yjs, fallback to custom if needed
2. **Storage**: In-memory during operation or persist to disk?
   - **Recommendation**: In-memory during edit, persist on save
3. **History**: Full history for all scripts or prune old versions?
   - **Recommendation**: Keep unlimited in Phase 6, evaluate pruning in Phase 7
4. **Conflicts**: Auto-merge or conflict resolution UI?
   - **Recommendation**: Auto-merge first, UI escalation for unresolvable conflicts

### Phase 6 Continued: Script CRDT to Production (4-6 weeks)

**Goal**: Ship collaborative script editing (first user-facing collaboration)

**Tasks**:

1. **Full Test Coverage**
   - Unit tests: Version bumping, refcount, context lifecycle
   - Integration tests: 2-3 concurrent editors
   - Stress tests: 10+ concurrent users, hour-long sessions
   - Performance tests: Merge 10,000 operations in < 1s

2. **UI Enhancements**
   - Show other users' cursors/selections
   - Display user names/avatars
   - Conflict resolution dialog (if needed)
   - History browser (query past versions)

3. **Production-Grade Performance**
   - Profile merge algorithm
   - Optimize change log storage
   - Implement efficient diff algorithm
   - Cache frequently accessed data

4. **Audit Trail**
   - Log all operations to database
   - Queryable via UserTalk: `script.getHistory(from, to)`
   - Export to OPML/JSON for compliance

**Success Criteria**:
- ✅ 10+ concurrent users editing same script
- ✅ Merge time < 100ms for 100 operations
- ✅ Zero silent data loss (100% of changes preserved)
- ✅ Version history queryable in < 500ms
- ✅ User acceptance testing with Dave Winer

### Phase 7: Multi-Type Collaboration Foundation (8-12 weeks)

**Goal**: Extend pattern to all ODB types, unified CRDT layer

#### Week 1-2: WPText Context

```c
typedef struct wp_context_t {
    _Atomic uint32_t refcount;
    _Atomic uint64_t version;
    uint32_t flags;

    // WPText-specific fields
    hdlwptextrecord wptext;
    uint32_t paragraph_index;
    uint32_t run_offset;
    void *format_cache;

    // Reserved for Phase 6+
    void *reserved[8];
} wp_context_t;
```

**Challenges**:
- Rich text formatting (preserve during merge)
- Paragraph-level granularity (coarser than character-level)
- RTF unpacking/repacking (expensive operation)

**Strategy**:
- Use CRDT for plain text content
- Separate conflict resolution for formatting
- Cache formatted runs to avoid repacking

#### Week 3-4: Table Context

```c
typedef struct table_context_t {
    _Atomic uint32_t refcount;
    _Atomic uint64_t version;
    uint32_t flags;

    // Table-specific fields
    hdltablerecord table;
    bigstring current_key;
    void *iteration_state;

    // Reserved for Phase 6+
    void *reserved[8];
} table_context_t;
```

**Challenges**:
- Hash table structure (no total order)
- Row-level versioning (per-key change tracking)
- Merging hash tables (key collisions)

**Strategy**:
- Version each key-value pair independently
- Use last-write-wins for key conflicts
- Tombstone deleted keys (don't remove immediately)

#### Week 5: Menu Context

```c
typedef struct menu_context_t {
    _Atomic uint32_t refcount;
    _Atomic uint64_t version;
    uint32_t flags;

    // Menu-specific fields
    hdlmenurecord menu;
    uint32_t selected_item;

    // Reserved for Phase 6+
    void *reserved[8];
} menu_context_t;
```

**Challenges**: Minimal (menus rarely edited concurrently)

**Strategy**: Simple versioning, mostly for identity tracking

#### Week 6: Picture Context

```c
typedef struct pict_context_t {
    _Atomic uint32_t refcount;
    _Atomic uint64_t version;
    uint32_t flags;

    // Picture-specific fields
    hdlpictrecord picture;
    uint32_t zoom_level;
    uint32_t pan_x;
    uint32_t pan_y;

    // Reserved for Phase 6+
    void *reserved[8];
} pict_context_t;
```

**Challenges**: Bitmap editing (not CRDT-friendly)

**Strategy**: Reference-based versioning (not content CRDT)

#### Week 7-10: Unified CRDT Layer

**Goal**: Abstract CRDT interface that works for all types

```c
typedef struct crdt_interface_t {
    // Type-agnostic CRDT operations
    void* (*create_state)(void);
    void (*destroy_state)(void *state);
    void (*apply_operation)(void *state, change_log_entry_t *entry);
    void (*merge_states)(void *local, void *remote);
    conflict_type_t (*detect_conflict)(change_log_entry_t *a, change_log_entry_t *b);
    void (*resolve_conflict)(void *state, conflict_context_t *conflict);
} crdt_interface_t;

// Each ODB type registers its CRDT implementation
extern crdt_interface_t script_crdt;
extern crdt_interface_t wptext_crdt;
extern crdt_interface_t table_crdt;
extern crdt_interface_t menu_crdt;
extern crdt_interface_t pict_crdt;
```

**Benefits**:
- Consistent API across all types
- Type-specific optimizations
- Easy to swap CRDT libraries
- Testable in isolation

#### Week 11-12: Cross-Type Operations

**Scenario**: User edits script that references table values

```c
// Script: "return system.config.serverPort;"
// Concurrent edits:
// - User A: Edits script text
// - User B: Changes system.config.serverPort from 8080 to 9000

// Challenge: Script's semantic meaning changed even though script text unchanged
```

**Strategy**:
- Version tracking across object references
- Invalidate cached values when referenced object changes
- Optional: Track dependencies (script → table linkage)

**Success Criteria**:
- ✅ All six ODB types have context pattern
- ✅ Each type tested with 2+ concurrent users
- ✅ Unified CRDT layer handles all types
- ✅ Performance remains acceptable

### Phase 8: Production Collaboration (4-6 weeks)

**Goal**: Ship collaborative ODB editing (full Google Docs parity)

**Tasks**:

1. **Multi-User UI Enhancements**
   - Real-time presence indicators
   - User cursors/selections in all object types
   - Activity feed (who changed what)
   - Conflict resolution UI

2. **Audit Trails for All Operations**
   - `system.internal.auditLog.<object_id>`
   - Query API: `object.getHistory(from, to)`
   - Export to JSON/CSV for compliance

3. **Performance Optimization**
   - Profile all CRDT operations
   - Optimize change log storage
   - Implement efficient diff algorithms
   - Cache frequently accessed data

4. **API Documentation and Best Practices**
   - UserTalk examples for context usage
   - Migration guide (global state → context)
   - Performance tuning guide

**Success Criteria**:
- ✅ All ODB types support concurrent editing
- ✅ 10+ users editing different objects simultaneously
- ✅ Conflict resolution UI refined
- ✅ Audit trails for all operations
- ✅ User acceptance testing (Dave Winer, Automattic)

---

## Real-World Scenarios

### Scenario 1: Concurrent Outline Edits (Different Branches)

**Setup**:
- User A editing "System Preferences" branch
- User B editing "User Settings" branch
- Both branches in same outline database

**Operations**:
```
Time 0: Both users see version 100

User A operations:
  T+1s: Insert node "Font Settings" under "System Preferences"
        → version 101, UUID abc123
  T+2s: Set node text to "Configure system fonts"
        → version 102

User B operations:
  T+1s: Insert node "Theme" under "User Settings"
        → version 101, UUID def456
  T+3s: Set node color to red
        → version 102
```

**Conflict detection**:
```
Version vectors:
  User A: {A:102, B:100}  // "I've seen my version 102, User B's version 100"
  User B: {A:100, B:102}  // "I've seen my version 102, User A's version 100"

Server merges:
  - User A's version 101 (insert node abc123)
  - User B's version 101 (insert node def456)
  - Both concurrent, but different nodes → NO CONFLICT
  - User A's version 102 (text change)
  - User B's version 102 (color change)
  - Both concurrent, but different fields → NO CONFLICT

Final state:
  "System Preferences"
    └─ "Font Settings" (abc123)
  "User Settings"
    └─ "Theme" (def456, red)
```

**Outcome**: Auto-merged successfully, both users see same result

### Scenario 2: Concurrent Outline Edits (Same Node)

**Setup**:
- User A editing node "Server Config" (UUID abc123)
- User B editing same node "Server Config" (UUID abc123)

**Operations**:
```
Time 0: Node text = "Configure server settings"

User A operations:
  T+1s: Set node text to "Configure server preferences"
        → version 101

User B operations:
  T+1s: Set node text to "Configure server options"
        → version 101 (concurrent!)
```

**Conflict detection**:
```
Version vectors:
  User A: {A:101, B:100}
  User B: {A:100, B:101}

Conflict detected:
  - Both operations modify same field (headstring)
  - Both have concurrent Lamport timestamp 101
  - Both target same UUID (abc123)
  - Conflict type: FIELD_LEVEL_CONFLICT
```

**Resolution strategy**:
```
Option 1: Last-Write-Wins (deterministic)
  - Compare client IDs: A=1, B=2
  - User B wins (higher client ID)
  - Final text: "Configure server options"

Option 2: CRDT Merge (intelligent)
  - Diff User A's change: "settings" → "preferences"
  - Diff User B's change: "settings" → "options"
  - Merge: "Configure server preferences-options" (combined)

Option 3: User Resolution (escalate)
  - Show conflict dialog to both users
  - "User A changed text to 'preferences', User B changed to 'options'"
  - Let users choose or merge manually
```

**Chosen strategy**: Option 1 (Last-Write-Wins) for Phase 6, Option 3 (User Resolution) for Phase 8

**Outcome**: User B's change wins, User A sees notification "Your change was overridden by User B"

### Scenario 3: Concurrent Script Edits (Merge Algorithm)

**Setup**:
- User A and User B editing same script

**Operations**:
```
Time 0: Script text = "function hello() {\n  return 'Hi';\n}"

User A operations:
  T+1s: Insert " console.log('A');" at line 2
        → Script = "function hello() {\n  console.log('A');\n  return 'Hi';\n}"

User B operations:
  T+1s: Insert " console.log('B');" at line 2 (concurrent!)
        → Script = "function hello() {\n  console.log('B');\n  return 'Hi';\n}"
```

**CRDT Merge (Yjs approach)**:
```
1. Both inserts at line 2 (concurrent)
2. Yjs orders by client ID: A < B
3. User A's insert comes first: "console.log('A');"
4. User B's insert comes second: "console.log('B');"
5. Final script:
   function hello() {
     console.log('A');
     console.log('B');
     return 'Hi';
   }
```

**Outcome**: Both users see same merged script, no data loss

### Scenario 4: Delete vs Modify Conflict (Escalate to User)

**Setup**:
- User A deletes node "Old Config" (UUID abc123)
- User B modifies node "Old Config" text (UUID abc123)

**Operations**:
```
Time 0: Node "Old Config" exists

User A operations:
  T+1s: Delete node abc123
        → version 101

User B operations:
  T+1s: Set node text to "Updated config"
        → version 101 (concurrent!)
```

**Conflict detection**:
```
Conflict type: DELETE_MODIFY_CONFLICT
  - User A: OP_DELETE on UUID abc123
  - User B: OP_MODIFY on UUID abc123
  - Cannot auto-resolve (semantic conflict)
```

**Resolution strategy**:
```
Escalate to User A:
  "User B modified node 'Old Config' that you deleted.
   Keep User B's changes?"
   [Keep Changes] [Discard Changes]

If User A chooses [Keep Changes]:
  - Restore node abc123 with User B's text
  - Final state: Node "Updated config" exists

If User A chooses [Discard Changes]:
  - Ignore User B's modification
  - Final state: Node deleted, User B sees notification
```

**Outcome**: User makes decision, system respects choice

### Scenario 5: Operational Transformation (Insert Order)

**Setup**:
- User A and User B editing script
- Concurrent inserts at different positions

**Operations**:
```
Time 0: Script text = "Hello World"
                       0123456789A

User A operations:
  T+1s: Insert "Beautiful " at position 6
        → "Hello Beautiful World"

User B operations:
  T+1s: Insert "Cruel " at position 6 (concurrent!)
        → "Hello Cruel World"
```

**Operational Transformation**:
```
1. User A's insert: position=6, text="Beautiful "
2. User B's insert: position=6, text="Cruel "
3. Concurrent operations at same position

Transform User B's operation against User A's:
  - User A inserted "Beautiful " (length=10) at position 6
  - User B's position 6 is now shifted to position 16
  - Adjusted User B operation: position=16, text="Cruel "

Final state:
  "Hello Beautiful Cruel World"
  (Not "Hello Cruel Beautiful World" - order is deterministic)
```

**Outcome**: Consistent merge across all clients

### Scenario 6: Network Partition (Offline Editing)

**Setup**:
- User A loses network connection
- User A makes 10 local edits while offline
- User B makes 5 server edits during same time
- User A reconnects

**Operations**:
```
Time 0: Both at version 100

User A (offline):
  T+1s: Edit 1 → local version 101
  T+2s: Edit 2 → local version 102
  ...
  T+10s: Edit 10 → local version 110

User B (online):
  T+1s: Edit 1 → server version 101
  T+3s: Edit 2 → server version 102
  ...
  T+9s: Edit 5 → server version 105

User A reconnects at T+11s
```

**Sync protocol**:
```
1. User A: "I have versions 101-110, what do you have?"
2. Server: "I have versions 101-105"
3. User A sends operations 101-110 to server
4. Server sends operations 101-105 to User A
5. Both merge:
   - User A merges server's 101-105 into local 101-110
   - Server merges User A's 101-110 into server 101-105
6. Final version: 115 (110 + 5 from server)
```

**Outcome**: User A's offline work preserved, merged with server state

---

## Implementation Examples

### Example 1: Inserting Text with CRDT

```c
/**
 * Insert text into script with CRDT tracking
 */
boolean scriptins_ctx_crdt(script_context_t *ctx, long pos, char *text, long len) {
    // 1. Validate context
    if (ctx == NULL || text == NULL || len <= 0) {
        return false;
    }

    // 2. Get CRDT infrastructure from reserved fields
    lamport_clock_t *clock = ctx->reserved[0];
    change_log_t *log = ctx->reserved[1];
    session_info_t *session = ctx->reserved[3];

    // 3. Tick Lamport clock
    lamport_tick(clock);

    // 4. Create change log entry
    change_log_entry_t entry = {
        .lamport_ts = clock->timestamp,
        .client_id = session->my_client_id,
        .operation = OP_INSERT,
        .position = pos,
        .text = strndup(text, len),
        .text_len = len
    };

    // 5. Append to change log
    append_to_log(log, &entry);

    // 6. Apply operation locally
    boolean result = scriptins_internal(ctx->script, pos, text, len);

    // 7. Bump version counter
    script_context_version_bump(ctx);

    // 8. Broadcast to other clients (if connected)
    if (ctx->is_connected) {
        broadcast_operation(ctx, &entry);
    }

    return result;
}
```

### Example 2: Merging Concurrent Operations

```c
/**
 * Merge remote operations into local script
 */
void merge_remote_operations(script_context_t *local, change_log_t *remote_log) {
    lamport_clock_t *local_clock = local->reserved[0];
    change_log_t *local_log = local->reserved[1];

    // Find operations remote has that local doesn't
    for (int i = 0; i < remote_log->num_entries; i++) {
        change_log_entry_t *remote_op = &remote_log->entries[i];

        // Skip operations local already has
        if (find_operation(local_log, remote_op->lamport_ts, remote_op->client_id)) {
            continue;
        }

        // Check for conflicts
        for (int j = 0; j < local_log->num_entries; j++) {
            change_log_entry_t *local_op = &local_log->entries[j];

            conflict_type_t conflict = detect_conflict(local_op, remote_op);
            if (conflict != CONFLICT_NONE) {
                handle_conflict(local, local_op, remote_op, conflict);
                continue;
            }
        }

        // No conflict, apply remote operation
        apply_operation(local, remote_op);

        // Update local's Lamport clock
        lamport_update(local_clock, remote_op->lamport_ts);

        // Add to local log
        append_to_log(local_log, remote_op);

        // Bump version
        script_context_version_bump(local);
    }
}
```

### Example 3: Conflict Detection

```c
/**
 * Detect conflicts between two concurrent operations
 */
conflict_type_t detect_conflict(change_log_entry_t *a, change_log_entry_t *b) {
    // Not concurrent = no conflict
    if (a->lamport_ts != b->lamport_ts) {
        return CONFLICT_NONE;
    }

    // Same operation at same position = conflict
    if (a->operation == b->operation && a->position == b->position) {
        switch (a->operation) {
            case OP_INSERT:
                return CONFLICT_INSERT_INSERT;
            case OP_DELETE:
                return CONFLICT_DELETE_DELETE;
            case OP_MODIFY:
                return CONFLICT_MODIFY_MODIFY;
        }
    }

    // Delete vs modify on overlapping range = conflict
    if ((a->operation == OP_DELETE && b->operation == OP_MODIFY) ||
        (a->operation == OP_MODIFY && b->operation == OP_DELETE)) {
        if (ranges_overlap(a->position, a->text_len, b->position, b->text_len)) {
            return CONFLICT_DELETE_MODIFY;
        }
    }

    return CONFLICT_NONE;
}

/**
 * Handle detected conflict
 */
void handle_conflict(script_context_t *ctx,
                     change_log_entry_t *local_op,
                     change_log_entry_t *remote_op,
                     conflict_type_t conflict) {

    switch (conflict) {
        case CONFLICT_INSERT_INSERT: {
            // Use CRDT ordering: lower client ID wins position
            if (local_op->client_id < remote_op->client_id) {
                // Local insert stays at position, remote shifts right
                remote_op->position += local_op->text_len;
            } else {
                // Remote insert stays at position, local shifts right
                local_op->position += remote_op->text_len;
            }

            // Apply both operations
            apply_operation(ctx, local_op);
            apply_operation(ctx, remote_op);
            break;
        }

        case CONFLICT_DELETE_MODIFY: {
            // Escalate to user
            conflict_context_t *conflict_ctx = ctx->reserved[2];
            conflict_ctx->strategy = CONFLICT_USER_RESOLVE;
            conflict_ctx->local_state = local_op;
            conflict_ctx->remote_state = remote_op;

            // Show conflict resolution UI
            show_conflict_dialog(ctx, conflict_ctx);
            break;
        }

        case CONFLICT_MODIFY_MODIFY: {
            // Last-write-wins (deterministic)
            if (local_op->client_id > remote_op->client_id) {
                // Local wins
                apply_operation(ctx, local_op);
            } else {
                // Remote wins
                apply_operation(ctx, remote_op);
            }
            break;
        }

        default:
            break;
    }
}
```

### Example 4: Vector Clock Comparison

```c
/**
 * Compare two vector clocks (causality)
 */
typedef enum {
    VC_EQUAL,       // A == B (same state)
    VC_BEFORE,      // A happened before B
    VC_AFTER,       // A happened after B
    VC_CONCURRENT,  // A and B are concurrent (no causal relationship)
} vector_clock_relation_t;

vector_clock_relation_t compare_vector_clocks(version_vector_t *a, version_vector_t *b) {
    boolean a_less = false;
    boolean b_less = false;

    // Compare each client's version
    for (int i = 0; i < max(a->num_clients, b->num_clients); i++) {
        uint64_t a_version = (i < a->num_clients) ? a->entries[i].version_number : 0;
        uint64_t b_version = (i < b->num_clients) ? b->entries[i].version_number : 0;

        if (a_version < b_version) {
            a_less = true;
        } else if (a_version > b_version) {
            b_less = true;
        }
    }

    // Determine relationship
    if (!a_less && !b_less) {
        return VC_EQUAL;  // All versions equal
    } else if (a_less && !b_less) {
        return VC_BEFORE;  // A happened before B
    } else if (!a_less && b_less) {
        return VC_AFTER;  // A happened after B
    } else {
        return VC_CONCURRENT;  // Both have unique versions (concurrent)
    }
}

/**
 * Example usage
 */
void sync_example(void) {
    version_vector_t *local_vc = ...;   // {A:5, B:3, C:0}
    version_vector_t *remote_vc = ...;  // {A:4, B:3, C:2}

    vector_clock_relation_t relation = compare_vector_clocks(local_vc, remote_vc);

    switch (relation) {
        case VC_BEFORE:
            // Local state is older, apply all remote operations
            apply_remote_operations(local, remote);
            break;

        case VC_AFTER:
            // Local state is newer, send all local operations to remote
            send_local_operations(local, remote);
            break;

        case VC_CONCURRENT:
            // Both have unique operations, merge both ways
            merge_bidirectional(local, remote);
            break;

        case VC_EQUAL:
            // Same state, no sync needed
            break;
    }
}
```

### Example 5: Query Version History

```c
/**
 * Query change log for version history
 */
void show_history(script_context_t *ctx, uint64_t from_version, uint64_t to_version) {
    change_log_t *log = ctx->reserved[1];

    printf("Version history (%llu to %llu):\n", from_version, to_version);
    printf("====================================\n");

    for (int i = 0; i < log->num_entries; i++) {
        change_log_entry_t *entry = &log->entries[i];

        // Filter by version range
        if (entry->lamport_ts < from_version || entry->lamport_ts > to_version) {
            continue;
        }

        // Print entry
        printf("[v%llu] Client %u: ", entry->lamport_ts, entry->client_id);

        switch (entry->operation) {
            case OP_INSERT:
                printf("INSERT \"%.*s\" at position %ld\n",
                       (int)entry->text_len, entry->text, entry->position);
                break;

            case OP_DELETE:
                printf("DELETE %ld characters at position %ld\n",
                       entry->text_len, entry->position);
                break;

            case OP_MODIFY:
                printf("MODIFY at position %ld: \"%.*s\"\n",
                       entry->position, (int)entry->text_len, entry->text);
                break;
        }
    }
}

/**
 * UserTalk API: script.getHistory(fromVersion, toVersion)
 */
boolean script_get_history_func(tyvaluerecord *v) {
    hdlscriptrecord hscript;
    uint64_t from_version, to_version;

    // Get parameters
    if (!getscriptvalue(v, 1, &hscript)) return false;
    if (!getlongvalue(v, 2, &from_version)) return false;
    if (!getlongvalue(v, 3, &to_version)) return false;

    // Get script context (or create temporary one)
    script_context_t *ctx = get_or_create_script_context(hscript);

    // Show history (could return as table instead of printing)
    show_history(ctx, from_version, to_version);

    return true;
}
```

---

## Technical Deep Dive

### CRDT Algorithm Choice: Yjs vs Automerge vs Custom OT

#### Yjs (Recommended for Phase 6)

**Architecture**:
```
Yjs uses a unique CRDT approach called "YATA" (Yet Another Transformation Approach):
- Operations are ordered by (Lamport timestamp, client ID)
- Each character has a unique ID: (client_id, seq_num)
- Insertions reference left/right neighbors
- Deletions are tombstones (not removed)
```

**Example**:
```
Initial state: "Hello"
  [H(A,0)] [e(A,1)] [l(A,2)] [l(A,3)] [o(A,4)]

User A inserts "Beautiful " after "Hello":
  [H(A,0)] [e(A,1)] [l(A,2)] [l(A,3)] [o(A,4)] [B(A,5)] [e(A,6)] ...

User B inserts "Cruel " after "Hello" (concurrent):
  [H(A,0)] [e(A,1)] [l(A,2)] [l(A,3)] [o(A,4)] [C(B,0)] [r(B,1)] ...

Merge:
  - Both insert after [o(A,4)]
  - Order by client ID: A < B
  - Final: "Hello Beautiful Cruel "
    [H(A,0)] [e(A,1)] [l(A,2)] [l(A,3)] [o(A,4)] [B(A,5)] ... [C(B,0)] ...
```

**Pros**:
- ✅ Proven at scale (used in production by many companies)
- ✅ Rich text support (formatting, embeds)
- ✅ Efficient (O(log n) insert/delete)
- ✅ Active community and documentation

**Cons**:
- ❌ JavaScript-first (need C bindings)
- ❌ Requires WebSocket server for sync
- ❌ Larger memory footprint

**Frontier Integration**:
```c
// Yjs integration (pseudocode)
#include <yjs/yjs.h>

typedef struct {
    YDoc *ydoc;          // Yjs document
    YText *ytext;        // Yjs text object
    YState *ystate;      // Yjs state
} yjs_crdt_state_t;

void* create_yjs_state(void) {
    yjs_crdt_state_t *state = malloc(sizeof(yjs_crdt_state_t));
    state->ydoc = yDocNew();
    state->ytext = yTextNew(state->ydoc, "script");
    state->ystate = yStateNew(state->ydoc);
    return state;
}

void apply_yjs_operation(void *state, change_log_entry_t *entry) {
    yjs_crdt_state_t *yjs = state;

    switch (entry->operation) {
        case OP_INSERT:
            yTextInsert(yjs->ytext, entry->position, entry->text, entry->text_len);
            break;
        case OP_DELETE:
            yTextDelete(yjs->ytext, entry->position, entry->text_len);
            break;
    }
}

void merge_yjs_states(void *local, void *remote) {
    yjs_crdt_state_t *local_yjs = local;
    yjs_crdt_state_t *remote_yjs = remote;

    // Get state vector from remote
    YStateVector *remote_sv = yStateGetVector(remote_yjs->ystate);

    // Get missing operations
    YUpdate *update = yDocGetUpdate(local_yjs->ydoc, remote_sv);

    // Apply update to local
    yDocApplyUpdate(local_yjs->ydoc, update);
}
```

#### Automerge (Backup Choice)

**Architecture**:
```
Automerge uses a more traditional CRDT approach:
- JSON-like document model
- Operations are immutable (append-only log)
- Conflict resolution built-in
- Offline-first (no server required)
```

**Pros**:
- ✅ Pure CRDT (no central server)
- ✅ C library available (automerge-c)
- ✅ Well-documented
- ✅ Offline-first (works without network)

**Cons**:
- ❌ Heavier than Yjs (more memory)
- ❌ Complex merge algorithm (harder to debug)
- ❌ Slower for large documents

**Frontier Integration**:
```c
#include <automerge-c/automerge.h>

typedef struct {
    AMDoc *doc;
    AMChangeHash *head;
} automerge_crdt_state_t;

void* create_automerge_state(void) {
    automerge_crdt_state_t *state = malloc(sizeof(automerge_crdt_state_t));
    state->doc = AMDocCreate();
    state->head = AMDocGetHeads(state->doc);
    return state;
}

void apply_automerge_operation(void *state, change_log_entry_t *entry) {
    automerge_crdt_state_t *am = state;

    AMTransaction *txn = AMTransactionCreate(am->doc);

    switch (entry->operation) {
        case OP_INSERT:
            AMTextInsert(txn, "script", entry->position, entry->text);
            break;
        case OP_DELETE:
            AMTextDelete(txn, "script", entry->position, entry->text_len);
            break;
    }

    AMTransactionCommit(txn);
}

void merge_automerge_states(void *local, void *remote) {
    automerge_crdt_state_t *local_am = local;
    automerge_crdt_state_t *remote_am = remote;

    // Get changes from remote
    AMChanges *changes = AMDocGetChanges(remote_am->doc, local_am->head);

    // Apply changes to local
    AMDocApplyChanges(local_am->doc, changes);

    // Update head
    local_am->head = AMDocGetHeads(local_am->doc);
}
```

#### Custom OT (Operational Transformation) (Last Resort)

**Architecture**:
```
OT transforms operations to handle concurrency:
- Operations are transformed against each other
- Transform function: transform(op_a, op_b) → op_a'
- Maintains consistency: apply(op_a', apply(op_b, state)) == apply(op_b', apply(op_a, state))
```

**Example**:
```
Initial state: "Hello"

User A: Insert "Beautiful " at position 5
User B: Insert "Cruel " at position 5 (concurrent)

Transform B against A:
  - A inserted 10 characters at position 5
  - B's position 5 is now shifted to position 15
  - Transformed B: Insert "Cruel " at position 15

Final state: "Hello Beautiful Cruel "
```

**Pros**:
- ✅ Lighter weight than CRDT
- ✅ Full control over implementation
- ✅ No external dependencies

**Cons**:
- ❌ Hard to implement correctly
- ❌ More complex to test
- ❌ More failure modes

**Frontier Implementation**:
```c
typedef struct {
    op_type_t type;
    long position;
    char *text;
    long text_len;
} ot_operation_t;

/**
 * Transform operation B against operation A
 */
void transform_operation(ot_operation_t *b, ot_operation_t *a) {
    if (a->type == OP_INSERT) {
        // A inserted text, shift B's position if needed
        if (b->position >= a->position) {
            b->position += a->text_len;
        }
    } else if (a->type == OP_DELETE) {
        // A deleted text, adjust B's position
        if (b->position >= a->position + a->text_len) {
            b->position -= a->text_len;
        } else if (b->position >= a->position) {
            // B's position is inside deleted range
            b->position = a->position;
        }
    }
}

/**
 * Apply transformed operations
 */
void apply_ot_operations(script_context_t *ctx, ot_operation_t *local, ot_operation_t *remote) {
    // Transform remote against local
    ot_operation_t remote_prime = *remote;
    transform_operation(&remote_prime, local);

    // Transform local against remote
    ot_operation_t local_prime = *local;
    transform_operation(&local_prime, remote);

    // Apply both transformed operations
    apply_operation(ctx, &local_prime);
    apply_operation(ctx, &remote_prime);
}
```

### Database Format Changes: v7 vs v7.5

#### v7 Format (Current)

```c
// Outline node (no UUID)
typedef struct tyheadrecord {
    struct tyheadrecord **headlinkdown, **headlinkup, **headlinkleft, **headlinkright;
    short headlevel;
    // ... bit flags ...
    byte reserved_identity[16];  // ZEROED, not used
    Handle hrefcon;
    Handle headstring;
} tyheadrecord;

// Disk format: No change log, no UUIDs
```

#### v7.5 Format (Phase 6+)

```c
// Outline node (with UUID)
typedef struct tyheadrecord {
    struct tyheadrecord **headlinkdown, **headlinkup, **headlinkleft, **headlinkright;
    short headlevel;
    // ... bit flags ...
    byte node_uuid[16];  // UUID v4 (128-bit) - RENAMED from reserved_identity
    Handle hpermalink;   // Optional user-defined permalink (NEW)
    Handle hrefcon;
    Handle headstring;
} tyheadrecord;

// Disk format additions:
// - UUID stored with each node (16 bytes overhead per node)
// - Permalink stored in refcon-like handle (optional)
// - Change log stored in system.internal.changeLogs.<outline_id>
```

**Migration strategy (v7 → v7.5)**:

```c
/**
 * Migrate outline from v7 to v7.5 (add UUIDs)
 */
boolean migrate_outline_v7_to_v75(hdloutlinerecord ho) {
    // Traverse all nodes
    opflatten(ho);  // Get all nodes in flat list

    for (hdlheadrecord hnode = opfirstnode(ho); hnode != NULL; hnode = opnextnode(hnode)) {
        // Generate UUID for this node
        uuid_t uuid;
        uuid_generate_random(uuid);  // UUID v4

        // Copy UUID to node
        memcpy((**hnode).node_uuid, uuid, 16);

        // Leave permalink as NULL (user can set later)
        (**hnode).hpermalink = NULL;
    }

    // Update database version
    dbsetversion(7.5);

    return true;
}
```

**Compatibility**:
- v7 reader can read v7.5 files (ignores UUIDs)
- v7.5 reader can read v7 files (generates UUIDs on load)
- No breaking changes (backward compatible)

---

## Risk Assessment and Mitigations

### Risk 1: CRDT Library Integration Complexity

**Likelihood**: MEDIUM
**Impact**: HIGH

**Description**: Yjs/Automerge may not integrate cleanly with Frontier's C codebase

**Mitigation**:
- **Phase 6 Week 2**: Prototype with Yjs, measure integration effort
- **Fallback Plan**: If Yjs doesn't work, try Automerge
- **Last Resort**: Implement custom OT (Phase 7 work)
- **Decision Point**: End of Phase 6 Week 3 (go/no-go on CRDT library)

### Risk 2: Performance Overhead

**Likelihood**: MEDIUM
**Impact**: MEDIUM

**Description**: CRDT operations may introduce unacceptable latency

**Mitigation**:
- **Profiling**: Measure merge time for 100, 1000, 10000 operations
- **Optimization**: Cache frequently accessed data (e.g., formatted runs in WPText)
- **Pruning**: Limit change log size (keep last N days, archive older)
- **Target**: < 100ms merge time for 100 operations (Phase 6 goal)

**Benchmark targets**:
```
Operation         Target Latency
---------------------------------
Insert char       < 1ms
Delete char       < 1ms
Merge 10 ops      < 10ms
Merge 100 ops     < 100ms
Merge 1000 ops    < 1s
```

### Risk 3: Storage Growth

**Likelihood**: HIGH
**Impact**: MEDIUM

**Description**: Change logs grow unbounded, database size explodes

**Mitigation**:
- **Pruning Strategy**: Keep last 90 days of changes, archive older
- **Compression**: Compress change logs before persisting
- **Tiered Storage**: Hot changes in memory, cold changes on disk
- **User Control**: UserTalk API to query/prune change logs

**Storage estimates**:
```
Change log entry size: ~100 bytes
10,000 edits/day: 1 MB/day
90 days: 90 MB per object
With 1000 objects: 90 GB total (acceptable for modern systems)
```

### Risk 4: Conflict Resolution UI Complexity

**Likelihood**: MEDIUM
**Impact**: HIGH

**Description**: Users confused by conflict resolution dialogs

**Mitigation**:
- **Auto-Resolve First**: Only show UI for semantic conflicts (delete vs modify)
- **Clear Messaging**: "User B modified node 'Config' that you deleted. Keep their changes?"
- **Undo Support**: Let users revert conflict resolution if wrong
- **User Testing**: A/B test UI with Dave Winer and partners

**UI design principles**:
- Show both versions side-by-side
- Highlight differences
- Provide "Keep Mine", "Keep Theirs", "Merge Both" options
- Log conflict resolution decisions (audit trail)

### Risk 5: v7.5 Format Incompatibility

**Likelihood**: HIGH (if we bump format)
**Impact**: HIGH

**Description**: v7.5 databases can't be read by v7 runtime

**Mitigation**:
- **Backward Compatibility**: v7 runtime ignores UUID fields (treats as reserved)
- **Forward Compatibility**: v7.5 runtime generates UUIDs for v7 files on load
- **Migration Tool**: Provide `frontier-migrate v7-to-v75` command
- **Documentation**: Clear upgrade path for users

**Format versioning strategy**:
```
v7.0: Original format (no UUIDs, no change logs)
v7.5: Add UUIDs and change logs (Phase 6+)
  - v7 runtime: Can read v7.5, ignores UUIDs (degrades gracefully)
  - v7.5 runtime: Can read v7, generates UUIDs on load
v8.0: (Future) Add vector clocks, CRDT metadata
  - v7.5 runtime: Can read v8, ignores CRDT metadata
  - v8 runtime: Can read v7/v7.5, upgrades on load
```

### Risk 6: Multi-User Testing Complexity

**Likelihood**: HIGH
**Impact**: HIGH

**Description**: Hard to test concurrent scenarios reliably

**Mitigation**:
- **Property-Based Testing**: Use QuickCheck-style testing for CRDT properties
- **Stress Testing**: Simulate 10+ concurrent users with automated scripts
- **Real-World Testing**: Dogfood with Dave Winer, Automattic partners
- **Monitoring**: Log all operations in production, analyze for bugs

**Test scenarios**:
```
1. Two users insert at same position (100 iterations)
2. One user deletes, one modifies (100 iterations)
3. Three users edit different parts (100 iterations)
4. Network partition: offline editing, reconnect (10 iterations)
5. Rapid-fire edits: 100 ops/sec for 10 seconds
```

---

## Conclusion

The operation context pattern implemented in Phase 3 is the critical foundation that makes CRDT-based collaboration possible. By reserving space for future features and establishing a consistent API pattern, Frontier is positioned to incrementally add collaborative editing without a full rewrite.

**Key Takeaways**:
1. **Incremental approach**: Start with scripts (Phase 6), expand to all types (Phase 7)
2. **Proven libraries**: Prefer Yjs/Automerge over custom OT (unless necessary)
3. **Backward compatibility**: v7 runtime must read v7.5 files (graceful degradation)
4. **User-centric**: Auto-resolve when possible, escalate only semantic conflicts
5. **Monitoring and testing**: Rigorous testing before production deployment

**Next Steps**:
- Complete Phase 4-5 (v7 parity)
- Begin Phase 6 Week 1 (script context foundation)
- Make CRDT library decision by Phase 6 Week 3
- Ship collaborative script editing by end of Phase 6

**Long-Term Vision**:
By Phase 8, Frontier will support Google Docs-style collaborative editing across all six ODB types, with zero concurrency code required in UserTalk scripts. This will enable Dave Winer's multi-user system and Automattic's enterprise collaboration features.

---

## References

### Academic Papers
- [Conflict-free Replicated Data Types (Shapiro et al., 2011)](https://hal.inria.fr/inria-00609399v1/document)
- [A comprehensive study of CRDTs (Shapiro et al., 2011)](https://hal.inria.fr/inria-00555588/document)
- [Yjs: A CRDT Framework for Shared Editing (Jahns, 2019)](https://github.com/yjs/yjs)

### Implementation References
- [Yjs Documentation](https://docs.yjs.dev/)
- [Automerge Documentation](https://automerge.org/)
- [Operational Transformation FAQ](http://www3.ntu.edu.sg/home/czsun/projects/otfaq/)

### Frontier Internal Documents
- `CONTEXT_PATTERN_FOR_ODB_COLLABORATION.md` - Operation context pattern
- `OUTLINE_OPERATION_CONTEXT.md` - Phase 3 implementation
- `NODE_IDENTITY_ARCHITECTURE_ASSESSMENT.md` - UUID infrastructure
- `../CRDT_FOUNDATION_ROADMAP.md` - Strategic vision

### Related Issues
- Issue #135: Outline context refactoring (Phase 3 foundation)
- Issue #XXX: Script context implementation (Phase 6, future)
- Issue #XXX: CRDT library integration (Phase 6, future)

---

**Document Version**: 1.0
**Last Updated**: 2025-12-25
**Next Review**: End of Phase 5 (before Phase 6 kickoff)
