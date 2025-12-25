#ifndef OP_CONTEXT_H
#define OP_CONTEXT_H

#include <stdatomic.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * Operation Context for Outline Operations
 *
 * This structure tracks metadata about outline modification operations.
 * It is designed to support future collaborative editing features while
 * maintaining single-threaded correctness for Phase 3.
 *
 * Design principles:
 * - Operation-owned, not stored in outline structure
 * - Atomic refcounting for safe sharing across call stacks
 * - Reserved space for Phase 6+ CRDT/sync features
 * - API assumes locks exist (even if unimplemented)
 *
 * See: planning/architectural_decision_records/OUTLINE_OPERATION_CONTEXT.md
 */
typedef struct op_context_t {
    /**
     * Reference count (atomic for thread-safety preparation)
     *
     * Managed via:
     * - op_context_acquire() - creates context with refcount=1
     * - op_context_retain()  - increments refcount
     * - op_context_release() - decrements, frees at 0
     */
    _Atomic uint32_t refcount;

    /**
     * Operation version (atomic increment on each mutation)
     *
     * Increments on:
     * - Node insert/delete/move
     * - Attribute changes (text, flags, etc.)
     * - Structural changes (promote/demote)
     *
     * NOT stored in outline - used for validation only.
     * In Phase 6+, enables optimistic conflict detection.
     */
    _Atomic uint64_t version;

    /**
     * Operation flags (future use)
     *
     * Reserved for:
     * - Read-only operations (no version bump)
     * - Undo/redo tracking
     * - Conflict resolution hints
     */
    uint32_t flags;

    /**
     * Debug tracking: Source file that created context
     * Only populated in DEBUG builds
     */
    const char *source_file;

    /**
     * Debug tracking: Line number that created context
     * Only populated in DEBUG builds
     */
    uint32_t source_line;

    /**
     * Reserved space for Phase 6+ features (64 bytes)
     *
     * Planned uses:
     * - Lamport timestamp / vector clock
     * - Client ID / session ID
     * - Change log pointer
     * - Conflict resolution context
     * - Transaction ID for multi-operation atomicity
     * - Owner tracking (read/write lock holder)
     * - Operation type metadata
     * - User identity information
     */
    void *reserved[8];

} op_context_t;

/**
 * Context Creation Flags
 */
typedef enum {
    OP_CONTEXT_NORMAL = 0,      // Standard mutation context
    OP_CONTEXT_READONLY = 1,    // No version bumps (future)
    OP_CONTEXT_UNDO = 2,        // Undo operation (future)
} op_context_flags_t;

/**
 * Acquire new operation context (refcount=1, version=0)
 *
 * @param flags - Operation flags (use OP_CONTEXT_NORMAL for now)
 * @return New context, or NULL on allocation failure
 *
 * Caller must call op_context_release() when done.
 */
#ifdef DEBUG
#define op_context_acquire(flags) \
    op_context_acquire_debug((flags), __FILE__, __LINE__)
op_context_t* op_context_acquire_debug(uint32_t flags, const char *file, uint32_t line);
#else
op_context_t* op_context_acquire(uint32_t flags);
#endif

/**
 * Retain context (increment refcount)
 *
 * @param ctx - Context to retain (may be NULL)
 * @return Same context pointer (for convenience)
 *
 * Use when passing context to nested functions that need to
 * keep the context alive beyond parent function scope.
 */
op_context_t* op_context_retain(op_context_t *ctx);

/**
 * Release context (decrement refcount, free at 0)
 *
 * @param ctx - Context to release (may be NULL)
 *
 * Safe to call multiple times, safe with NULL.
 * After last release, context memory is freed.
 */
void op_context_release(op_context_t *ctx);

/**
 * Bump operation version (atomic increment)
 *
 * @param ctx - Context to bump (required, asserts if NULL)
 * @return New version number
 *
 * Called at every outline mutation point.
 * In Phase 6+, enables optimistic concurrency control.
 */
uint64_t op_context_version_bump(op_context_t *ctx);

/**
 * Get current version (atomic read)
 *
 * @param ctx - Context to query (required)
 * @return Current version number
 */
uint64_t op_context_version_get(op_context_t *ctx);

/**
 * Validate context invariants (debug builds only)
 *
 * @param ctx - Context to validate
 * @return true if valid, false if corrupted
 *
 * Checks:
 * - Refcount > 0
 * - Reserved fields are NULL (Phase 3)
 */
bool op_context_validate(op_context_t *ctx);

#endif /* OP_CONTEXT_H */
