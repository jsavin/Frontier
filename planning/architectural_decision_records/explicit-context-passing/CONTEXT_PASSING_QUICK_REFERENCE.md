# Explicit Context Passing: Quick Reference

**Full Plan**: `EXPLICIT_CONTEXT_PASSING_REFACTORING_PLAN.md`
**Call Graph**: `CALL_GRAPH_CONTEXT_THREADING.md`

---

## The Bug (Issue #147)

```c
// Parent packs table with v7 context
hashpacktable_internal(ctx_v7, htable, ...)
  → hashpackexternal(hv)
    → langexternalpack_internal(NULL, hv, ...)  // ❌ NULL context!
      → db_context_init(&ctx)
        → db_format_mode_current()  // ❌ Reads v6 from global stack!
      → tablepacktable_internal(ctx_v6, ...)
        → hashpacktable_internal(ctx_v6, child_table, ...)  // ❌ Wrong mode!
          → Writes v4 header instead of v5
```

**Result**: Migrated v7 database has mixed table header versions → corruption.

---

## The Fix (Phase 1)

### 1. Add context field to typackinforecord

```c
// common/source/langhash.c, line ~2590
typedef struct typackinforecord {
    handlestream s1;
    handlestream s2;
    boolean flmustsave;
    boolean use_64bit;
    const db_context *context;  // ADD THIS LINE
} typackinforecord;
```

### 2. Set context in hashpacktable_internal()

```c
// common/source/langhash.c, line ~3752
clearbytes(&packrec, sizeof(packrec));
packrec.flmustsave = *flmustsave;
packrec.use_64bit = use_64bit;
packrec.context = (ctx != NULL) ? ctx : &working_context;  // ADD THIS LINE
```

### 3. Update hashpackexternal() signature

```c
// common/source/langhash.c, line ~2780
static boolean hashpackexternal(handlestream *s, hdlexternalvariable h,
                                 int32_t *ix, boolean *flnewdbaddress,
                                 const db_context *ctx) {  // ADD THIS PARAM
    // ... existing code ...

    if (flexternalmemorypack)
        fl = langexternalmemorypack(h, &hpacked, HNoNode);
    else
        fl = langexternalpack_internal(ctx, h, hpacked, flnewdbaddress);  // PASS ctx

    // ... rest unchanged ...
}
```

### 4. Update hashpackvisit_v7() to pass context

```c
// common/source/langhash.c, line ~3243
case externalvaluetype:
    if (!hashpackexternal(&lpi->s2,
                          (hdlexternalvariable) val.data.externalvalue,
                          &data_index, &flnewdbaddress,
                          lpi->context))  // ADD THIS ARG
        HASH_PACK_FAIL("hashpackexternal");
    break;
```

### 5. Also update hashpackvisit_v7() around line ~3624

```c
// common/source/langhash.c, line ~3624
if (!hashpackexternal(&lpi->s2,
                      (hdlexternalvariable) val.data.externalvalue,
                      &data_index, &flnewdbaddress,
                      lpi->context))  // ADD THIS ARG
    HASH_PACK_FAIL("hashpackexternal");
```

---

## Testing

### Before Fix
```bash
./tools/run_headless_tests.sh 2>&1 | grep "hashpacktable_internal"
# Expected: Will see use_64bit=0 for child tables (BUG)
```

### After Fix
```bash
./tools/run_headless_tests.sh 2>&1 | grep "hashpacktable_internal"
# Expected: All lines show use_64bit=1 during migration (CORRECT)
```

### Validation
```bash
# Test external table variables work
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  --system-root tests/test_save_migration-v7.root \
  -e "sizeOf(system.verbs.globals)"
# Expected: Returns table size, not error
```

---

## Files Modified

| File | Lines Changed | Changes |
|------|---------------|---------|
| `common/source/langhash.c` | ~30 | Add context field, thread through calls |

---

## Success Criteria

- [ ] All tests pass
- [ ] Migration creates v7 database with ALL tables having version=5 headers
- [ ] External table variables accessible: `sizeOf(system.verbs.globals)` works
- [ ] Logs show consistent `use_64bit=1` during migration (no flip-flopping)
- [ ] No mode stack push/pop during recursive table packing

---

## Risk Mitigation

**Risk**: Breaking recursive packing operations
**Mitigation**:
- Add assertions for NULL context
- Test with deeply nested tables
- Validate table header versions in migrated database

**Rollback**: Simple - revert the 5 changes above. Takes 1 hour.

---

## Estimated Effort

**Implementation**: 2-3 hours
**Testing**: 4-6 hours
**Review & Merge**: 1-2 hours
**Total**: 3-5 days (including validation)

---

## Next Steps After Phase 1

**Phase 2** (optional): Remove `use_64bit` boolean from `typackinforecord`, derive from context everywhere (~1 week)

**Phase 3** (optional): Make context mandatory, remove global mode fallback (~3 days)

**Phase 4** (deferred): Extend pattern to menu/pict operations (~1 week)

---

## Why Phases 2-4 Are Optional (for Correctness)

### Phase 1 is Sufficient Because:

**Critical Bug Fixed**: Phase 1 explicitly threads context to child table packing operations, eliminating the root cause of Issue #147:
- Parent context (`use_64bit=1` for v7) is now passed down explicitly
- Child table packing receives parent's explicit context, not inherited from global mode stack
- Result: All tables written with correct v7 format during migration

**No Staleness Issue**: The `use_64bit` boolean cached in `typackinforecord` (kept in Phase 1) is set once per table in `hashpacktable_internal()` and never changes during that table's packing:
- Set from context at line 3762 (v7 path) or 3791 (v6 path)
- Read only by child visitors (`hashpackvisit_v7`)
- Never modified during packing
- No risk of staleness since it's immutable during a single table pack operation

**Global Mode Fallback is Safe**: When `hashpacktable_internal()` is called with `ctx=NULL` (legacy API):
- We use `db_format_mode_current()` to decide format for THAT table only
- Context is passed (NULL) to children
- Children also use global mode if context is NULL
- Result: Legacy API continues to work, uses existing (correct) global mode behavior
- No correctness issue - only if global mode is set incorrectly (prevented by Phase 3)

### When to Implement Phases 2-4

**Phase 2 becomes required if**:
- We discover scenarios where `use_64bit` boolean staleness causes bugs
- Performance profiling shows context passing is significantly faster than deriving from global mode

**Phase 3 becomes required if**:
- NULL context scenarios fail (context incorrectly set to NULL at top level)
- We want to eliminate legacy API paths that depend on global mode

**Phase 4 becomes required if**:
- Menu/pict operations are used during migration and encounter same mode-stack bug
- Architectural consistency demand (all table operations use explicit context)

---

### Phase 1 Scope Decision

**Decision**: Ship Phase 1 as self-contained fix for Issue #147
- Fixes confirmed bug: v7 migration with corrupted table headers
- Minimal risk: Only adds context field and threads it through one call path
- Preserves backward compatibility: Legacy API paths unchanged
- Clear upgrade path: Phases 2-3 can be added later if needed
- No performance regression: Negligible overhead from passing one pointer

---

## Quick Grep Commands

```bash
# Find all hashpackexternal calls
grep -n "hashpackexternal" common/source/langhash.c

# Find typackinforecord definition
grep -n "typackinforecord" common/source/langhash.c

# Verify context threading
grep -n "lpi->context" common/source/langhash.c

# Check for mode stack usage
grep -n "db_format_mode_current()" common/source/langhash.c
```

---

## Approval Checklist

Before implementing:
- [ ] User confirms Phase 1 approach is acceptable
- [ ] User confirms 3-5 day timeline is acceptable
- [ ] User confirms testing strategy is sufficient
- [ ] User approves incremental delivery (Phase 1 ships independently)

Before merging:
- [ ] All tests pass
- [ ] Code review complete
- [ ] Migration validation passes
- [ ] Logs confirm deterministic mode usage

---

**Status**: Ready for implementation
**Blockers**: None
**Dependencies**: None (self-contained change)
