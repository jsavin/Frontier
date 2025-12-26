# Architectural Decision Record: Outline Operation Context

**Status:** Implemented (Phase 2)
**Date:** 2025-12-25
**Deciders:** Core Team
**Related Issues:** #135

## Context and Problem Statement

Frontier's outline object database (ODB) currently performs mutations through functions that rely on implicit global state. This creates several challenges:

1. **No mutation tracking**: There's no way to know when or how an outline was modified
2. **Difficult debugging**: No audit trail of what operations occurred
3. **No version control**: Can't detect concurrent modifications or implement optimistic locking
4. **Future collaboration blockers**: Can't build CRDT-based collaborative editing without operation metadata
5. **Thread-safety barriers**: Global state prevents multi-threaded access patterns

## Decision

We introduce an **operation context** (`op_context_t`) that is explicitly passed to all outline mutation operations. This context:

- Tracks operation metadata (version counter, flags, debug info)
- Is operation-scoped (created, used, destroyed per mutation)
- Uses atomic refcounting for safe sharing across call stacks
- Reserves space for future CRDT/sync features (Phase 6+)
- Maintains backward compatibility through wrapper functions

### Core Pattern

All mutation functions now have two variants:

```c
// Context-aware variant (new)
boolean opinsertstructure_ctx (op_context_t *ctx, hdlheadrecord hnode, tydirection dir);

// Backward-compatible wrapper (existing signature)
boolean opinsertstructure (hdlheadrecord hnode, tydirection dir) {
    op_context_t *ctx = op_context_acquire(OP_CONTEXT_NORMAL);
    boolean result = opinsertstructure_ctx(ctx, hnode, dir);
    op_context_release(ctx);
    return result;
}
```

Every mutation increments the context's version counter via `op_context_version_bump(ctx)`.

## Implementation Summary

### Phase 1: Foundation (Completed)

**1A: Data Structures**
- `op_context_t` structure in `Common/headers/op_context.h`
  - Atomic refcount and version counter
  - 64 bytes reserved space for future features
  - Debug tracking (source file/line in DEBUG builds)

**1B: Lifecycle Functions** (`Common/source/op_context.c`)
- `op_context_acquire()` - Create context with refcount=1
- `op_context_retain()` - Increment refcount
- `op_context_release()` - Decrement refcount, free at 0
- `op_context_version_bump()` - Atomic increment version
- `op_context_version_get()` - Read version
- `op_context_validate()` - Debug validation

### Phase 2: API Surface Integration (Completed)

**2A: Core Structure Operations** (`Common/source/opstructure.c`)
- `opinsertstructure_ctx` / `opinsertstructure`
- `opdeletenode_ctx` / `opdeletenode`
- `opdeletesubs_ctx` / `opdeletesubs`
- `opdelete_ctx` / `opdelete`
- `oppromote_ctx` / `oppromote`
- `opdemote_ctx` / `opdemote`

**2B: Expand/Collapse Operations** (`Common/source/opexpand.c`)
- `opcollapse_ctx` / `opcollapse`
- `opexpand_ctx` / `opexpand`

**2C: Insert Operations** (`Common/source/opverbs.c`, `Common/source/opstructure.c`)
- `opinserthandle_ctx` / `opinserthandle`
- `opinsertheadline_ctx` / `opinsertheadline`

**2D: Attribute Operations** (`Common/source/opops.c`, `Common/source/oprefcon.c`)
- `opsetheadtext_ctx` / `opsetheadtext`
- `opsetrefcon_ctx` / `opsetrefcon`

**2E: Header Declarations**
- Updated `Common/headers/op.h` and `Common/headers/opinternal.h`
- Added `#include "op_context.h"` to both headers

### Phase 3: Testing (Completed)

All headless tests pass with no regressions:
- ✅ `runtime_tests` - Language, OPML, serializer round-trips
- ✅ `save_migration_tests` - v6→v7 database migration
- ✅ All existing test suites remain green

### Build System Changes

Added `op_context.c` to `LANG_RUNTIME_SOURCES` in `tests/Makefile` to ensure proper linking.

Removed `#include "memory.h"` from `op_context.c` to avoid Mac-specific type dependencies in headless builds.

## Design Rationale

### Why Explicit Context Parameters?

**Alternative Rejected:** Thread-local storage or global context stack

**Reasoning:**
- Explicit parameters make data flow visible in code
- No hidden state to reason about
- Call stacks can easily pass contexts to nested operations
- Debugger can inspect context at any stack frame
- No implicit initialization/cleanup to forget

### Why Refcounting Instead of Ownership Transfer?

**Reasoning:**
- Nested operations may need to retain context beyond parent scope
- Refcounting prevents dangling pointers
- Atomic refcount prepares for future thread-safety
- Simple retain/release pattern familiar from Cocoa/COM

### Why Version Counter Per-Context?

**Reasoning:**
- Enables optimistic concurrency control (Phase 6+)
- Cheap to increment (atomic add)
- Can detect if outline changed during long-running operation
- Foundation for CRDT operation ordering

### Why 64-Byte Reserved Space?

**Reasoning:**
- Future CRDT features will need:
  - Lamport timestamp / vector clock (16 bytes)
  - Client ID / session ID (16 bytes)
  - Change log pointer (8 bytes)
  - Conflict resolution context (8 bytes)
  - Transaction ID (8 bytes)
  - Owner tracking (8 bytes)
- Better to reserve now than expand struct later (ABI break)
- Validated to be NULL in Phase 3 to catch misuse

## Consequences

### Positive

- **Traceability**: Every mutation now tracked with version number
- **Debuggability**: Context records source file/line in DEBUG builds
- **Future-proof**: Reserved space enables CRDT without struct changes
- **Backward compatible**: Existing code continues working unchanged
- **No performance impact**: Context allocation is ~64 bytes on stack
- **Clean headless support**: No Mac-specific dependencies

### Negative

- **API duplication**: Every mutation function now has two variants
- **Temporary overhead**: Wrappers allocate/free context per call (until Phase 5 refactor)
- **Code size increase**: Each _ctx variant adds ~10 lines of boilerplate

### Neutral

- **Migration path clear**: Future work will remove wrappers, keep only _ctx variants
- **CRDT foundation laid**: Phase 6+ can add collaborative editing without API changes

## Future Evolution

### Phase 4: ADR Documentation (Current)
Document the decision and pattern in this ADR.

### Phase 5: Call-Site Migration (Future)
Convert internal Frontier code to use `_ctx` variants directly, eliminating wrapper overhead.

### Phase 6: CRDT Integration (Future)
Populate reserved fields with:
- Vector clocks for operation ordering
- Client IDs for multi-user collaboration
- Change logs for merge resolution
- Conflict detection/resolution metadata

### Phase 7: Multi-Threading (Future)
Leverage atomic refcounting for safe context sharing across threads.

## Related Patterns

### Comparison to Other Systems

**Git:**
- Similar: Operation contexts like Git commits (metadata + content)
- Different: Git is immutable; we're mutable with versioning

**Automerge CRDT:**
- Similar: Operation-based CRDT with version vectors
- Different: We're preparing for CRDT, not yet implementing

**Core Data (Apple):**
- Similar: Managed object contexts for change tracking
- Different: Their contexts are heavyweight; ours are lightweight

## Validation

All tests pass:
```bash
$ ./tools/run_headless_tests.sh
🎉 ALL TESTS PASSED! 🎉
```

Key validations:
- ✅ OPML roundtrip maintains fidelity
- ✅ Database migration v6→v7 succeeds
- ✅ No memory leaks detected in refcon tests
- ✅ Runtime tests exercise all _ctx variants

## References

- Issue #135: Phase 2 - API Surface Integration
- `Common/headers/op_context.h`: Structure definition and lifecycle API
- `Common/source/op_context.c`: Implementation
- `planning/phase3/` - Future work documentation

## Notes

### Build System Gotcha

When adding `op_context.c` to the build, we initially included `memory.h` which pulled in Mac-specific headers (`RgnHandle`, `ControlHandle`, etc.) that broke headless builds. Removed `memory.h` and used only standard C library headers (`stdlib.h`, `stdio.h`, `string.h`, `assert.h`).

### Version Bump Placement

The `op_context_version_bump(ctx)` call is placed **before** the mutation logic, not after. This ensures:
- Version increments even if operation fails midway
- Failure leaves outline in known version state
- Consistent with "version = operation attempt counter" semantics

### Headless Mode Compatibility

The `opcollapse_ctx` and `opexpand_ctx` functions maintain headless compatibility by:
- Performing data mutations unconditionally (flexpanded, ctexpanded)
- Guarding display code with `if (opdisplayenabled())`
- This ensures outline state changes even when UI is disabled
