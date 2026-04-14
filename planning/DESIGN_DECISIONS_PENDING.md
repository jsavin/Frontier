# Pending Design Decisions

These issues require architectural decisions before implementation can proceed. Each is currently blocked on a design choice that affects multiple subsystems.

**Last updated**: 2026-04-03

---

## 1. Outline Node Identifiers (#259, #273)

### Problem

The outline processor uses inconsistent and fragile mechanisms to identify nodes across different verbs:

- `op.getCursor()` returns opaque binary markers (memory addresses cast to longs)
- `op.getExpansionState()` uses 1-based line numbers that shift when the outline changes
- `op.getSelection()` format is undocumented

Memory addresses are not serializable across sessions and have no semantic meaning to scripts. Line numbers break after any structural modification (insert, delete, move). There is no unified way to persistently reference a specific node, which blocks bookmarking, undo/redo, and collaborative editing.

The immediate bug (#259) -- `op.getCursor()` returning line numbers instead of handles -- has a straightforward fix (restore legacy handle-as-long behavior). The strategic question is what replaces handles long-term.

### Options

**A. Hierarchical Path Notation** (e.g., `"1.2.3"` = 3rd child of 2nd child of 1st top-level node)

- Pros: Zero memory overhead, human-readable, computable on demand, natural for UserTalk scripts
- Cons: Breaks when parent structure changes (move/reparent), ambiguous if siblings have identical structure

**B. UUID-Based Node Identifiers** (persistent UUID stored in node metadata)

- Pros: Globally unique, survives any reorganization, supports collaborative editing and CRDT integration
- Cons: Memory overhead per node, requires database format changes, breaks backward compatibility with scripts expecting longs

**C. Hybrid -- Monotonic Node ID + Validation** (sequential ID assigned at creation, stored in node metadata)

- Pros: Lightweight, serializable, survives reorganization, enables deleted-node detection
- Cons: Still requires schema change, IDs are opaque (less user-friendly than paths)

**D. Status Quo -- Opaque Handle-as-Long** (restore legacy behavior, no unified system)

- Pros: No schema changes, backward compatible, unblocks Phase 3 immediately
- Cons: Not serializable, not stable across sessions, does not unify expansion state and cursor APIs

### Recommendation

**Short term**: Restore legacy handle-as-long behavior (Option D) to unblock Phase 3.

**Long term**: Option C (monotonic node IDs) offers the best balance -- lightweight, serializable, stable across reorganization, and extensible to CRDT use cases. UUIDs (Option B) are worth considering if collaborative editing across independent replicas becomes a priority sooner than expected.

The key decision is whether to invest in a unified node identifier now (delaying verb completion) or ship handle-as-long and revisit during Phase 6 CRDT work.

### Blocked Issues

- [#259](https://github.com/jsavin/Frontier/issues/259) -- Outline cursor addressing system
- [#273](https://github.com/jsavin/Frontier/issues/273) -- Consistent node identifiers for outline verbs

---

## 2. Expansion State Persistence (#272)

### Problem

`op.getExpansionState()` returns a list of 1-based line numbers identifying which nodes are expanded. When the outline structure changes (insert, delete, move), those line numbers no longer point to the same nodes. Saving and restoring expansion state across any structural modification silently corrupts it.

This is a subset of the node identifier problem (#259/#273) but has its own API surface and backward-compatibility constraints. Legacy Frontier scripts and OPML interop both depend on line-number-based expansion state.

### Options

**A. Node-Handle-Based Expansion State** (use same opaque handles as `op.getCursor()`)

- Pros: Consistent with cursor API, survives reorganization within a session, no schema change
- Cons: Not serializable across sessions, ties expansion state to in-memory handle lifetime

**B. Node-ID-Based Expansion State** (depends on unified node identifiers from Decision #1)

- Pros: Serializable, survives reorganization, aligns with CRDT roadmap
- Cons: Blocked on Decision #1, requires schema change, migration path needed for existing scripts

**C. Content-Hash Fallback** (hash node text + depth as secondary identifier)

- Pros: No schema change, works across sessions for unique content
- Cons: Ambiguous when siblings have identical text, fragile under edits, not a real solution

**D. Keep Line Numbers + Document the Limitation**

- Pros: Zero implementation cost, backward compatible, matches legacy Frontier behavior
- Cons: Expansion state remains broken after structural changes, blocks test #10

### Recommendation

Option A (handle-based) as an immediate improvement that unifies the cursor and expansion APIs within a session. Transition to Option B once Decision #1 is resolved and persistent node IDs exist.

This decision is downstream of Decision #1 -- resolve node identifiers first, then expansion state follows naturally.

### Blocked Issues

- [#272](https://github.com/jsavin/Frontier/issues/272) -- Expansion state should use persistent node IDs

---

## 3. AppleEvent Cross-Platform Strategy (#284)

### Problem

Sixteen verbs related to system integration are stubbed with inconsistent error messages ("not implemented", "AppleScript is not available in headless mode"). These fall into three categories:

- **AppleEvent/OSA verbs** (10 verbs): IPC and system event functionality -- macOS-specific but represents a real capability gap for cross-platform Frontier
- **Legacy platform verbs** (3 verbs): `lang.callDLL`, `lang.callXCMD`, `lang.ddeevent` -- Windows/Classic Mac technologies with no modern equivalent
- **Window packing verbs** (2 verbs): `lang.packWindow`, `lang.unpackWindow` -- GUI state serialization

The strategic question: does Frontier 2.0 need an IPC layer, and if so, what does it look like?

### Options

**A. Permanent Stubs with Clear Errors** (all 16 verbs return "not supported on this platform")

- Pros: Zero implementation cost, unblocks verb coverage metrics, honest about current state
- Cons: Leaves an IPC gap that may matter for collaborative ODB and plugin systems

**B. Cross-Platform IPC Abstraction** (abstract AppleEvent semantics into portable message-passing)

- Pros: Enables plugin systems, server-to-client communication, collaborative editing support
- Cons: Significant design and implementation effort, risk of over-engineering for uncertain requirements

**C. Deprecate Legacy, Stub AppleEvent, Add New IPC Verbs Later** (hybrid approach)

- Pros: Permanently closes legacy verbs (DLL, XCMD, DDE), keeps door open for modern IPC without committing to AppleEvent compatibility
- Cons: Defers the real decision, new IPC verbs would need their own design process

### Recommendation

Option C. The legacy verbs (DLL, XCMD, DDE) are dead technologies -- deprecate them permanently. The AppleEvent verbs should get consistent "not supported" errors now, with a note that a modern IPC mechanism may replace them in a future phase. The window packing verbs could potentially be generalized to ODB object state serialization, but that is a separate design question.

The real decision point is whether collaborative ODB editing (Phase 6+) needs an IPC layer built into the verb system, or whether it will use a different communication mechanism entirely (e.g., WebSocket protocol, HTTP API). That decision can wait until the collaborative architecture is further along.

### Blocked Issues

- [#284](https://github.com/jsavin/Frontier/issues/284) -- Cross-platform support for AppleEvent and legacy integration verbs

---

## 4. ODB Hash Table Reference Counting (#332)

### Problem

Hash table handles (`hdlhashtable`) are pointers-to-pointers that can be relocated by Frontier's memory manager. When a background thread (e.g., TCP accept thread) holds a handle obtained on the main thread, the memory manager can relocate the underlying data, causing use-after-free crashes.

PR #330 (TCP Phase 3) includes temporary validation checks, but production multi-threaded networking requires a real solution. This is foundational infrastructure for the CRDT collaborative editing roadmap -- every ODB type will eventually need safe cross-thread access.

### Options

**A. Reference Counting on Hash Tables** (add refcount to `tyhashtable`, acquire/release API)

- Pros: Prevents use-after-free, memory manager defers cleanup while references exist, extends naturally to other ODB types, aligns with CRDT roadmap
- Cons: Adds complexity to every hash table operation, potential for reference leaks, performance overhead on acquire/release

**B. Handle Pinning** (pin handles in place while cross-thread references exist)

- Pros: Simpler than full refcounting, no API change for readers, memory manager skips pinned handles during compaction
- Cons: Fragments memory if many handles are pinned, doesn't solve the general ODB concurrent access problem, less extensible

**C. Copy-on-Access for Background Threads** (background threads work on snapshots)

- Pros: No shared mutable state, eliminates race conditions entirely, conceptually simple
- Cons: Memory cost of copies, stale data problem (snapshot diverges from live state), doesn't support write-back from background threads

**D. Thread-Local Handle Caches with GIL Synchronization** (background threads acquire GIL before accessing handles)

- Pros: Fits existing threading model (GIL already exists), no memory manager changes
- Cons: Serializes all cross-thread access (defeats purpose of threading), doesn't scale to collaborative editing

### Recommendation

Option A (reference counting). It is the most work upfront but is the only option that scales to the full CRDT vision where multiple ODB types need safe concurrent access. The phased approach in the issue is sound:

1. Design the API (lightweight, atomic refcount updates)
2. Implement for hash tables only
3. Integrate with TCP listener as first consumer
4. Extend to other ODB types as needed

Key design decisions within this option:

- **Atomic vs. lock-protected refcounts**: Atomic operations (`__atomic_fetch_add`) are preferred for performance, but must verify Frontier's memory manager is compatible
- **Opt-in vs. universal**: Reference counting should be opt-in (only for cross-thread access) to avoid overhead on single-threaded code paths
- **Cleanup semantics**: When refcount reaches zero, should the object be freed immediately or queued for deferred cleanup?

### Blocked Issues

- [#332](https://github.com/jsavin/Frontier/issues/332) -- ODB reference counting for hash tables across thread boundaries

---

## Decision Dependencies

The four decisions have the following dependency relationships:

```
Decision #1 (Node Identifiers)
    └── Decision #2 (Expansion State) -- depends on #1's outcome

Decision #3 (AppleEvent/IPC) -- independent

Decision #4 (Reference Counting) -- independent, but enables collaborative features that motivate #1
```

**Suggested resolution order**:

1. **#3 (AppleEvent)** -- Lowest stakes, clearest path. Deprecate legacy, stub AppleEvent, move on.
2. **#1 (Node Identifiers)** -- Foundational. Restore legacy handles now, design persistent IDs for Phase 6.
3. **#2 (Expansion State)** -- Falls out naturally once #1 is decided.
4. **#4 (Reference Counting)** -- Highest effort, can be deferred until TCP/collaborative features demand it.
