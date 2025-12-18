# ADR-001: Multi-Database Context Management

**Date**: 2025-12-17
**Status**: Accepted
**Author**: System Architecture Analysis
**Related**: PR #117 (external database handle mismatch fix)
**Implementation Plan**: planning/MULTI_DATABASE_PREVENTION_STRATEGY.md

---

## Context

Legacy Frontier was designed for a single active database at a time. All database state was captured in a global variable `databasedata` that pointed to "the" current database.

During v6→v7 migration, this assumption breaks down:
- **Source database** (v6) must be open for reading
- **Destination database** (v7) must be open for writing
- Systems that captured `databasedata` at creation time now have ambiguous references

**The Bug**: External variables call `langnewexternalvariable()` which captures `databasedata` at creation time:
```c
item.hdatabase = databasedata;  // Captures global
```

During migration, this captured the v6 source handle, but after migration the externals had v7 addresses with v6 handles - creating a fatal mismatch that broke all external object access.

**Root Cause**: Foundational systems like externals, records, tables, and scripts were designed around the assumption of a single active database. They capture global state at creation time rather than resolving it at use time.

---

## Decision

We will establish these patterns for multi-database context management:

### 1. **Database Context Routing for All Writes**
All functions that modify database state MUST check for save-as context before writing. Use the `db_context_for_saveas_destination()` pattern already implemented in `db.c`.

**Pattern**:
```c
// In dbassign(), dballocate(), and all database write operations
db_context ctx_storage;
boolean using_destination = false;
db_context *ctx = db_context_for_saveas_destination(&ctx_storage, &using_destination);
if (ctx == NULL)
    ctx = db_context_refresh_default();
return dbassign_context(ctx, ...);
```

### 2. **Creation-Time vs Use-Time Database Binding**
Document which systems bind databases at creation time (risky) vs use time (safe). Prefer use-time binding for new code.

**Documented Binding Strategy**:
- **Creation-time capture** (existing): External variables, records, lists (require fixup during migration)
- **Use-time resolution** (safer): Functions resolve current `databasedata` when accessed
- **Explicit parameter** (best): Database handle passed as function parameter

### 3. **Source Database Read-Only Protection**
During save-as operations, open source databases with file-level read-only protection. This prevents writes at the OS level, failing fast for any code that violates the constraint.

### 4. **Architectural Decision Records (ADRs)**
Capture architectural decisions that affect multiple subsystems in `/planning/adr/` directory. This ensures knowledge preservation as the codebase evolves.

---

## Consequences

### Positive
- Prevents class of bugs where operations target the wrong database
- Backward compatible: existing code works via default context
- Enables safe multi-database operations beyond migration
- Creates clear patterns for future multi-database features
- Fail-fast with read-only source protection

### Negative
- Requires context parameter threading through call chains
- Adds small performance overhead (context lookup per write)
- Existing code needs audit to ensure routing compliance

### Trade-offs
- **Simple + Safe**: Context routing with read-only files prevents bugs by design
- **Performance**: Acceptable for migration operations (not on hot path)
- **Backward Compatibility**: Maintained through default contexts

---

## Alternatives Considered

### Alternative A: Full Async Context Refactor
Eliminate global `databasedata` entirely, pass contexts explicitly everywhere.

**Pros**: Cleanest architecture, impossible to get wrong
**Cons**: Massive codebase changes, high risk of regressions, breaks legacy code

**Decision**: Reject for now. Implement incrementally after immediate stabilization.

### Alternative B: Database Handle Registry
Central registry of all open databases with reference counting.

**Pros**: Could enable more sophisticated multi-database scenarios
**Cons**: Complex to implement, overkill for current needs

**Decision**: Defer as future enhancement.

### Alternative C: Runtime Database Binding Override
Resolve database at runtime instead of creation time for all systems.

**Pros**: Fixes multiple systems at once
**Cons**: Requires changes to core structs (externals, records, lists)

**Decision**: Implement for externals first (deferred binding), then generalize if needed.

---

## Implementation Plan

### Phase 1: Prevent Known Bug Class (Week 1)
- [ ] Add read-only file protection for source databases during migration
- [ ] Implement migration smoke test in CI
- [ ] Add source database immutability assertions to debug builds

### Phase 2: Comprehensive Testing (Week 2)
- [ ] Add `test_migration_preserves_source_database()`
- [ ] Add `test_external_variables_bind_to_correct_database()`
- [ ] Add `test_migration_address_space_separation()`
- [ ] Create migration test checklist for PR reviews

### Phase 3: Systematic Audit (Week 3-4)
- [ ] Audit all functions in `db.c` for context routing
- [ ] Audit `tablestructure.c` for global captures
- [ ] Audit `langexternal.c` for binding strategy
- [ ] Document findings and create tickets

### Phase 4: Strategic Improvements (Month 2)
- [ ] Implement deferred database binding for externals
- [ ] Add context parameter to core database functions
- [ ] Add static analysis rules to CI

### Phase 5: Documentation (Ongoing)
- [ ] Create ADR-001 (this document)
- [ ] Document critical sections in code comments
- [ ] Update `CLAUDE.md` with multi-database patterns
- [ ] Create architecture guide

---

## Validation Strategy

### Automated Tests
- Migration smoke test: Source database hash unchanged
- Multi-database isolation tests: Writes don't cross databases
- Address space separation: Destination addresses don't collide with source

### Code Patterns
- Static analysis: No direct `databasedata` mutations in db write functions
- Assertion checks: `ASSERT_DB_CONTEXT()` in debug builds
- Logging: Diagnostic output when context switches occur

### Manual Reviews
- All migration-related PRs must follow migration test checklist
- Code review explicitly checks for context routing compliance
- Changes to `db.c`, `tablestructure.c`, `langexternal.c` require audit

---

## Code Patterns to Establish

### Pattern 1: Context-Aware Database Operations
```c
// Header annotation - document binding strategy
DB_HANDLE_CAPTURED hdldatabaserecord hdatabase;  /* Captured at creation time */

// Safe migration handling
if (fldatabasesaveas && databasedestination != nil) {
    // Switch context for operation
    db_context ctx_storage;
    db_context *ctx = db_context_for_saveas_destination(&ctx_storage, NULL);
    // Perform operation with correct context
}
```

### Pattern 2: Deferred Database Binding
```c
// New externals resolve database at use time, not creation time
boolean external_resolve_database(hdlexternalvariable hv,
                                  hdldatabaserecord *result) {
    if ((**hv).fldefer_db_binding) {
        *result = databasedata;  // Use current context
    } else {
        *result = (**hv).hdatabase;  // Use captured value
    }
    return true;
}
```

### Pattern 3: Source Database Protection
```c
// Open source read-only during migration
boolean db_open_readonly(FSSpec *fs, hdldatabaserecord *hdb) {
    // Open with read-only permissions
    (**hdb).flreadonly = true;  // Mark in-memory flag
    return true;
}

// Assert when writing
boolean dbassign_internal(...) {
    assert(!(**databasedata).flreadonly);  // Fail if writing to RO
    // ... implementation ...
}
```

---

## References

**Related Issues**:
- #117: External database handle mismatch during migration (FIXED)
- #118 (P1): Add functional tests for external access post-migration
- #119 (P2): Document mode lock permanence in test infrastructure

**Documentation**:
- `planning/phase3/kernel_verb_porting/DATABASE_HANDLE_MISMATCH_CONFIRMED.md` - Root cause analysis
- `planning/phase3/kernel_verb_porting/EXTERNAL_HANDLE_FIX_SUMMARY.md` - Implementation summary
- `planning/MULTI_DATABASE_PREVENTION_STRATEGY.md` - Comprehensive prevention strategy

**Code**:
- `Common/source/db.c` - Database operations and context routing
- `Common/source/db_format.c` - Migration implementation
- `Common/source/langexternal.c` - External variable creation
- `tests/db_format_tests.c` - Migration tests

---

## Related ADRs

(Future ADRs that will build on this foundation)
- ADR-002: Eliminating Global Database State (future)
- ADR-003: Database Handle Lifecycle Management (future)
- ADR-004: Multi-Database Testing Framework (future)

---

## Questions for Future Review

1. Should we add a database ownership field to hash nodes to track which database created them?
2. Are there other systems beyond externals that capture `databasedata` at creation time?
3. Should read-only protection be permanent or only during migration?
4. How should thread safety be handled if multi-threaded access is added?

---

**Approval**: Ready for team discussion and implementation
