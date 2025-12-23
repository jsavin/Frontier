# Mode Stack Refactor - Quick Start Guide

**Last Updated**: 2025-12-23
**Detailed Phase 1 Plan**: See `planning/phase3/MODE_STACK_REFACTOR_PHASE1_DETAILED_v2.md` (production-ready for Sonnet)
**Full Plan**: See `MODE_STACK_REFACTOR_PLAN.md`

## TL;DR

We're replacing the global mode stack (`db_format_mode_push/pop`) with explicit context passing to fix the root cause of all Issue #123 bugs. This is a **full refactor** (Option B), not incremental, because stability is paramount.

## Current Status

- **Branch**: Need to create `refactor/explicit-context-no-mode-stack`
- **Phase**: Not started (ready to begin Phase 1)
- **LOE**: 8-13 days (2-3 weeks)
- **Blockers**: None

## The Problem

```c
// Current (BAD): Mode is global, implicitly inherited
void migrate_table() {
    db_format_mode_push(&legacy_mode);  // Read v6
    load_table(...);

    // BUG: Forgot to pop! Child operations use wrong mode
    pack_child_table(...);  // Writes v4 instead of v5

    db_format_mode_pop();
}
```

```c
// Future (GOOD): Mode is explicit, no hidden state
void migrate_table(const db_context *src_ctx, const db_context *dest_ctx) {
    // Read with v6 mode
    load_table(src_ctx, ...);

    // Write with v7 mode - can't use wrong mode by accident
    pack_child_table(dest_ctx, ...);
}
```

## Implementation Phases

### Phase 1: Core Serialization (2-3 days) ← START HERE

**Goal**: Make hash/table pack/unpack deterministic

**Files**:
- `Common/source/db_format.c` - Add context init functions
- `Common/source/langhash.c` - Convert 15 call sites
- `Common/source/tablepack.c` - Convert 10 call sites

**Commands**:
```bash
# Create branch
git checkout -b refactor/explicit-context-no-mode-stack

# Run baseline
./tools/run_headless_tests.sh
make -C tests save_migration_tests && ./tests/save_migration_tests

# After changes
./tools/run_headless_tests.sh  # Should still pass
```

### Phase 2: Database Operations (2-3 days)

**Files**: `Common/source/db.c` (30 call sites)

### Phase 3: Format Readers/Writers (1-2 days)

**Files**: `db_reader_legacy.c`, `db_reader_v7.c`, `db_writer_v7.c`

### Phase 4: Migration Code (1-2 days)

**Files**: `Common/source/db_format.c`

### Phase 5: Cleanup (2-3 days)

**Goal**: Delete all mode stack infrastructure

## Key Architecture Points

### Context Structure
```c
typedef struct db_context {
    db_format_mode mode;              // v6 vs v7
    hdldatabaserecord database;       // Which DB
    db_saveas_state saveas;           // Migration state
    tyerrorcode last_error;           // Debugging
    const char *error_context;
    int recursion_depth;              // Safety
    int max_recursion_depth;
} db_context;
```

### Propagation Rules
1. **Stack-allocated** - never heap
2. **Pass by const pointer** - `const db_context *ctx`
3. **Clone for mode changes** - `db_context_clone_with_mode()`
4. **Same context through recursion** - no mode inheritance bugs
5. **Guards for legacy compat** - during transition only

## Testing Strategy

After each phase:
```bash
# Full suite
./tools/run_headless_tests.sh

# Migration
make -C tests save_migration_tests && ./tests/save_migration_tests

# Migrated DB access
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  --system-root tests/test_save_migration-v7.root \
  -e "sizeOf(system.verbs.colors)"

# No mode stack left
grep -r "db_format_mode_push\|db_format_mode_pop" Common/source/*.c
```

## Success Criteria

- ✅ Zero mode stack references in codebase
- ✅ Migration is deterministic (same input → same output)
- ✅ External tables accessible (Issue #123 fixed permanently)
- ✅ <5% performance overhead
- ✅ All tests pass

## What Could Go Wrong

| Risk | Mitigation |
|------|-----------|
| Mixed mode stack + context state | Migrate complete phases, not individual functions |
| Performance regression | Benchmark at each phase, target <5% overhead |
| Breaking external code | Maintain legacy wrappers during transition |
| Forgot to clone context | Provide clear helpers, add assertions |

## Rollback Plan

- **After Phase 1-2**: Can rollback individual phase
- **After Phase 3**: No rollback needed (core is stable)
- **After Phase 5**: Mode stack is gone forever ✨

## Quick Reference: Context API

```c
// Initialize
void db_context_init(db_context *ctx);
void db_context_init_with_mode(db_context *ctx, const db_format_mode *mode);
void db_context_init_legacy_read(db_context *ctx, hdldatabaserecord db);
void db_context_init_modern_write(db_context *ctx, hdldatabaserecord db);

// Clone
void db_context_clone_with_mode(const db_context *src, db_context *dst,
                                 const db_format_mode *mode);

// Guard (temporary, for legacy compat)
void db_context_guard_enter(const db_context *ctx, db_context_guard *guard);
void db_context_guard_exit(db_context_guard *guard);
```

## Files Changed Per Phase

**Phase 1** (serialization):
- `Common/headers/db_format.h` - Enhanced context
- `Common/source/db_format.c` - Context init/clone
- `Common/source/langhash.c` - hashpack/unpack
- `Common/source/tablepack.c` - tablepack/unpack
- `Common/source/langexternal.c` - external pack/unpack

**Phase 2** (DB ops):
- `Common/source/db.c` - All dbassign/ref/copy callers

**Phase 3** (readers/writers):
- `Common/source/db_reader_legacy.c`
- `Common/source/db_reader_v7.c`
- `Common/source/db_writer_v7.c`

**Phase 4** (migration):
- `Common/source/db_format.c` - migrate_internal

**Phase 5** (cleanup):
- Delete mode stack functions
- Remove legacy wrappers
- Update docs

## Next Action

**When ready to start**:
```bash
git checkout -b refactor/explicit-context-no-mode-stack
# Edit Common/source/db_format.c - add context init functions
# See MODE_STACK_REFACTOR_PLAN.md for detailed implementation
```

---

**Remember**: Stability is paramount. Test after every change. This refactor permanently fixes the root cause of Issue #123 and eliminates an entire class of bugs.
