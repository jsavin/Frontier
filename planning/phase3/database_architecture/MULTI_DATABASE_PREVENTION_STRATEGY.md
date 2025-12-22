# Multi-Database Context: Prevention Strategy for Migration Bugs

## Status
- State: Approved
- Phase: 3
- Last Updated: 2025-12-21
- Notes: Strategy for preventing multiple concurrent database instances


**Date**: 2025-12-17
**Status**: Implementation Plan (Proposed)
**Related**: planning/architectural_decision_records/ADR-001-multi-database-context.md, PR #117, Issues #118, #119

---

## Overview

This document outlines a comprehensive strategy to prevent bugs like the external database handle mismatch (#117) from happening again. The strategy includes immediate fixes, testing improvements, code patterns, and architectural enhancements.

The external handle bug occurred because:
1. Systems captured `databasedata` global at **creation time**
2. During migration, two databases are open simultaneously (source and destination)
3. Externals captured v6 source handle but were written with v7 addresses
4. Result: Fatal mismatch preventing external object access

This prevention strategy ensures this bug class never happens again through layered defenses:
- **Testing**: Catch regressions immediately
- **Code Patterns**: Make correct behavior the default
- **Architecture**: Make bug class impossible
- **Documentation**: Preserve knowledge for future developers

---

## Root Cause Summary

| Aspect | Details |
|--------|---------|
| **Bug** | External variables captured v6 source database handle during load, but were written to v7 destination with v7 addresses |
| **Why** | `langnewexternalvariable()` captures `databasedata` at creation time; during migration, `databasedata` = source |
| **Impact** | ALL external object access failed post-migration (silent failures) |
| **Fix** | Added `db_format_fixup_external_handles()` to correct handles during migration |
| **Deeper Issue** | Codebase designed for single active database; migration requires simultaneous source + destination |

---

## Prevention Strategy: Four Layers

```
Layer 4: Architectural Changes
  └─ Eliminate creation-time captures, use deferred binding

Layer 3: Code Patterns & Guardrails
  └─ Context routing mandatory, read-only protection, static analysis

Layer 2: Testing Infrastructure
  └─ Smoke tests, multi-database tests, invariant checks

Layer 1: Documentation
  └─ ADRs, code comments, migration checklist
```

---

## Layer 1: Documentation (IMMEDIATE - Week 1)

### 1.1 Create Architectural Decision Records

**Status**: ✅ DONE - planning/architectural_decision_records/ADR-001-multi-database-context.md created

**Purpose**: Document why multi-database context matters and what patterns to follow

**Content**:
- Why single-database assumption breaks during migration
- Decision to use context routing and read-only protection
- Code patterns developers should follow
- References to implementation details

### 1.2 Update Project CLAUDE.md

**Action**: Add section on multi-database patterns

**Content to add**:
```markdown
## Multi-Database Context Management

When working on migration code or database operations:
- NEVER assume a single active database during save-as operations
- ALWAYS use db_context_for_saveas_destination() before writing
- Document if your code captures databasedata at creation time
- Check that source databases are NOT modified during migration

See: planning/architectural_decision_records/ADR-001-multi-database-context.md
```

### 1.3 Create Migration Code Comments

**Action**: Add critical section markers to db.c, db_format.c, langexternal.c

**Pattern**:
```c
/**
 * CRITICAL SECTION: Multi-Database Context
 *
 * This function may be called during save-as when BOTH source and
 * destination databases are active. MUST use db_context routing.
 *
 * See: planning/architectural_decision_records/ADR-001-multi-database-context.md
 */
```

---

## Layer 2: Testing Infrastructure (Week 1-2)

### 2.1 Migration Smoke Test

**File**: `tests/migration_smoke_test.sh` (NEW)

**Purpose**: Run after every migration-related change, catch regressions

**Tests**:
```bash
1. Source database hash unchanged after migration
2. Destination database created with v7 header
3. Destination file size > source file size (new allocations)
4. No corruption in destination (header valid)
```

**Integration**: Add to CI pipeline, runs after every merge to develop

**Estimated execution**: < 1 second

### 2.2 Multi-Database Context Tests

**File**: `tests/db_format_tests.c` (ADD FUNCTIONS)

**Function 1: `test_migration_preserves_source_database()`**
```c
void test_migration_preserves_source_database(void) {
    // 1. Open source v6 database
    // 2. Capture hash/size before migration
    // 3. Run migration to v7
    // 4. Verify source hash unchanged
    // 5. Verify destination created correctly
    // Assert source == hash_before
}
```

**Function 2: `test_external_variables_bind_to_correct_database()`**
```c
void test_external_variables_bind_to_correct_database(void) {
    // 1. Create external in db1 context
    // 2. Create external in db2 context
    // 3. Verify ext1.hdatabase == db1
    // 4. Verify ext2.hdatabase == db2
    // 5. Modify in db1, verify db2 unchanged
}
```

**Function 3: `test_migration_address_space_separation()`**
```c
void test_migration_address_space_separation(void) {
    // 1. Open v6 source, capture size (N bytes)
    // 2. Migrate to v7
    // 3. For each address allocated in v7:
    //    - Address must be >= N (after source space)
    //    - OR address must be valid reuse within v7
    // 4. No destination address should exist in source space
}
```

**Estimated implementation**: 4-6 hours per test, high confidence

### 2.3 Migration Test Checklist

**File**: Add to PR template for all migration-related changes

**Checklist items**:
```markdown
## Migration Testing Checklist

- [ ] Source database file hash unchanged
- [ ] Source database file size unchanged
- [ ] No writes to source database address space
- [ ] Destination addresses in correct range
- [ ] External variables reference destination database
- [ ] All migration tests pass
- [ ] No regressions in non-migration operations
```

---

## Layer 3: Code Patterns & Guardrails (Week 2-3)

### 3.1 Mandatory Context Pattern

**Rule**: No database write without context check

**Implementation**:

1. **Code Pattern** - All `db*()` write functions must check context:
```c
// In dbassign(), dballocate(), etc.
db_context ctx_storage;
boolean using_destination = false;
db_context *ctx = db_context_for_saveas_destination(&ctx_storage, &using_destination);
if (ctx == NULL)
    ctx = db_context_refresh_default();
return dbassign_context(ctx, ...);
```

2. **Static Analysis** - Add linter rule:
```bash
# In CI pipeline
# Check for direct databasedata mutations in write functions
rg "databasedata\s*=" Common/source/db.c \
   | grep -v "db_context" \
   | grep -v "//" \
   && echo "ERROR: Unguarded databasedata mutation" && exit 1
```

3. **Debug Assertions** - Validate context at runtime:
```c
#ifdef DEBUG
#define ASSERT_DB_CONTEXT() \
    do { if (fldatabasesaveas) \
        assert(databasedata == databasedestination); } while(0)
#else
#define ASSERT_DB_CONTEXT() ((void)0)
#endif
```

**Audit Scope**:
- [ ] `dbassign()` - Already fixed
- [ ] `dballocate()` - Already correct
- [ ] `dbcopy()` - Check if needs fixing
- [ ] `dbrelease()` - Check if writes
- [ ] `dbrefhandle()` - Verify read direction
- [ ] `dbsetview()` - Check if writes to destination

### 3.2 Read-Only Source Database Protection

**Implementation**:

1. **OS-Level Flag** - Add to database record:
```c
typedef struct tydatabaserecord {
    // ... existing fields ...
    boolean flreadonly;  // NEW: file opened read-only
} tydatabaserecord;
```

2. **During Migration** - Open source read-only:
```c
// In migrate_internal()
hdldatabaserecord source = open_database_readonly(source_path);
```

3. **Assert on Write** - Fail fast:
```c
boolean dbassign_internal(...) {
    assert(!(**databasedata).flreadonly);  // Crash if writing to RO
    // ... continue with write ...
}
```

**Effect**: Any code that tries to write to source database will crash immediately in tests, preventing silent data corruption.

### 3.3 Database Handle Lifecycle Annotations

**Purpose**: Document which systems capture `databasedata` at creation time (risky) vs use time (safe)

**Annotations**:
```c
#define DB_HANDLE_CAPTURED   /* Captured at creation time - risky in multi-db */
#define DB_HANDLE_DEFERRED   /* Resolved at use time - safe */
#define DB_HANDLE_EXPLICIT   /* Passed as parameter - best */

// Existing (risky):
typedef struct tyexternalvariable {
    DB_HANDLE_CAPTURED hdldatabaserecord hdatabase;
    // ...
} tyexternalvariable;

// Mark all similar structures for audit
```

**Audit scope**:
- External variables (CAPTURED - known issue)
- Records (UNKNOWN - needs audit)
- Lists (UNKNOWN - needs audit)
- Scripts (UNKNOWN - needs audit)
- Outlines (UNKNOWN - needs audit)

---

## Layer 4: Architectural Improvements (Month 2)

### 4.1 Deferred Database Binding for Externals

**Objective**: Make externals resolve database at use time instead of creation time

**Design**:
```c
// New flag in tyexternalvariable
typedef struct tyexternalvariable {
    hdldatabaserecord hdatabase;        // Captured handle (legacy)
    boolean fldefer_db_binding;         // NEW: resolve at use time
    // ...
} tyexternalvariable;

// Creation function
boolean langnewexternalvariable_deferred(...) {
    item.hdatabase = nil;
    item.fldefer_db_binding = true;     // Resolve when accessed
    // ...
}

// Use function - resolve at access time
static hdldatabaserecord external_resolve_database(hdlexternalvariable hv) {
    if ((**hv).fldefer_db_binding)
        return databasedata;            // Use current context
    return (**hv).hdatabase;            // Use captured value
}
```

**Benefit**: Externals automatically get correct database context, migration fixup no longer needed.

### 4.2 Systematic Audit & Fix

**Scope**:
1. Find all systems that capture `databasedata` at creation time
2. Evaluate if deferred binding would help
3. Implement deferred binding where applicable
4. Add tests for each system

**Candidates**:
- External variables (KNOWN)
- Records
- Lists
- Scripts/Outlines
- Tables

### 4.3 Future: Explicit Context Parameters

**Long-term goal**: Eliminate global `databasedata` mutations entirely

**Pattern**:
```c
// Current (global):
dbassign(padr, size, pdata);

// Future (explicit):
dbassign_context(context, padr, size, pdata);
```

**Timeline**: Phase this in gradually, not immediate

---

## Implementation Timeline

### Week 1: Foundation & Documentation
- ✅ ADR-001-multi-database-context.md (DONE)
- [ ] Migration smoke test script (2 hours)
- [ ] Update CLAUDE.md with patterns (1 hour)
- [ ] Add critical section markers to code (2 hours)
- **Subtotal**: 5 hours

### Week 2: Testing Infrastructure
- [ ] Implement `test_migration_preserves_source_database()` (3 hours)
- [ ] Implement `test_external_variables_bind_to_correct_database()` (3 hours)
- [ ] Implement `test_migration_address_space_separation()` (3 hours)
- [ ] Add migration test checklist to PR template (1 hour)
- **Subtotal**: 10 hours

### Week 3-4: Code Patterns & Guardrails
- [ ] Add read-only file flag to tydatabaserecord (2 hours)
- [ ] Implement read-only protection in migration (2 hours)
- [ ] Add ASSERT_DB_CONTEXT() macro to debug builds (1 hour)
- [ ] Add static analysis linter rule (1 hour)
- [ ] Audit db.c functions for context routing (3 hours)
- [ ] Fix any functions missing context (4 hours)
- **Subtotal**: 13 hours

### Month 2: Architectural Improvements
- [ ] Add deferred binding flag to tyexternalvariable (2 hours)
- [ ] Implement langnewexternalvariable_deferred() (2 hours)
- [ ] Implement external_resolve_database() (1 hour)
- [ ] Systematic audit of other systems (8 hours)
- [ ] Fix identified systems (varies)
- **Subtotal**: 13+ hours

### Ongoing: Documentation
- [ ] Maintain ADRs (as needed)
- [ ] Code comments in critical sections (done with each fix)
- [ ] Architecture guide for future developers (4 hours)

**Total Estimated Effort**: ~40-50 hours over 6-8 weeks

---

## Success Metrics

### By End of Week 1
- [ ] ADR-001 created and merged
- [ ] Smoke test integrated into CI
- [ ] CLAUDE.md updated

### By End of Week 2
- [ ] All three multi-database tests written and passing
- [ ] Migration test checklist in PR template
- [ ] Zero failing tests

### By End of Week 4
- [ ] Read-only protection implemented
- [ ] Static analysis linter working
- [ ] All db.c functions audited
- [ ] No unguarded databasedata mutations

### By End of Month 2
- [ ] Deferred binding for externals implemented
- [ ] All other systems audited
- [ ] No similar bugs in code review

### Ongoing
- [ ] Zero external handle bugs reported
- [ ] Zero source database corruption during migration
- [ ] New developers understand multi-database context (from ADRs)

---

## Risk Mitigation

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|-----------|
| Tests too strict, break unrelated code | Medium | Low | Run full test suite on all changes |
| Static analysis generates false positives | Medium | Low | Whitelist known exceptions initially |
| Read-only protection causes issues | Low | Medium | Test on v6.root before deploying |
| Deferred binding breaks existing externals | Low | High | Implement with backward compatibility flag |
| Takes longer than estimated | Medium | Low | Prioritize smoke test + multi-db tests first |

---

## Dependencies & Prerequisites

### Required
- ADR-001 (this document) approved and merged
- Existing migration tests passing
- CI pipeline accessible

### Optional but Helpful
- Static analysis tool setup for C code
- Performance profiling tools (for read-only overhead)
- Code coverage tools (to verify test completeness)

---

## References

**Related Issues**:
- PR #117: External handle fix (MERGED)
- #118 (P1): Functional tests for external access
- #119 (P2): Mode lock documentation

**Documentation**:
- `planning/architectural_decision_records/ADR-001-multi-database-context.md` - Architectural decision
- `planning/phase3/kernel_verb_porting/DATABASE_HANDLE_MISMATCH_CONFIRMED.md` - Bug analysis
- `planning/phase3/kernel_verb_porting/EXTERNAL_HANDLE_FIX_SUMMARY.md` - Fix implementation
- `CLAUDE.md` - Project conventions (to be updated)

**Code**:
- `Common/source/db.c` - Context routing
- `Common/source/db_format.c` - Migration
- `Common/source/langexternal.c` - External creation
- `tests/db_format_tests.c` - Existing tests
- `tests/migration_smoke_test.sh` - (to be created)

---

## Sign-Off

**Document Status**: Ready for team review and approval

**Next Steps**:
1. Review and discuss with team
2. Prioritize immediate actions (Week 1)
3. Create tickets for each phase
4. Begin implementation
5. Report progress weekly

---

## Appendix: Code Audit Checklist

### Critical Functions to Audit

**Database Operations** (Common/source/db.c):
- [ ] dbassign() - Status: ✅ Already uses context routing
- [ ] dballocate() - Status: ✅ Already correct
- [ ] dbcopy() - Status: ⚠️ Check if needs routing
- [ ] dbrelease() - Status: ❓ Verify doesn't write
- [ ] dbrefhandle() - Status: ✅ Read-only operation
- [ ] dbsetview() - Status: ❓ Check if writes

**Table Operations** (Common/source/tablestructure.c):
- [ ] tablenewtable() - Status: ❓ Check database capture
- [ ] tablepacktable() - Status: ❓ Check context
- [ ] tableunpacktable() - Status: ❓ Check context

**External Operations** (Common/source/langexternal.c):
- [ ] langnewexternalvariable() - Status: ❌ Captures at creation time
- [ ] langexternalunpack() - Status: ⚠️ Check fixup
- [ ] Table external creation - Status: ❓ Audit needed
- [ ] Script external creation - Status: ❓ Audit needed
- [ ] Outline external creation - Status: ❓ Audit needed
- [ ] Picture external creation - Status: ❓ Audit needed
- [ ] WP external creation - Status: ❓ Audit needed
- [ ] Menu external creation - Status: ❓ Audit needed

**List/Record Operations** (Common/source/langsymbols.c, langlist.c):
- [ ] List creation functions - Status: ❓ Check capture
- [ ] Record creation functions - Status: ❓ Check capture

---

## Glossary

- **ADR**: Architectural Decision Record - documents important architectural decisions
- **Context**: The current active database and operation mode (normal vs save-as)
- **Deferred Binding**: Resolving database at use time rather than creation time
- **Save-as**: Database migration operation requiring simultaneous source + destination
- **Source Database**: The v6 database being read during migration
- **Destination Database**: The v7 database being written to during migration
- **Creation-time Capture**: Capturing `databasedata` value when object is created
- **Use-time Resolution**: Looking up current `databasedata` when object is accessed
