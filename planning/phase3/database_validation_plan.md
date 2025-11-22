# Database Validation Plan

Status
- State: Planning
- Phase: 3 (Headless Runtime)
- Last Updated: 2025-11-20
- Owner: Codex
- Notes: Validation strategy for v6→v7 database traversal/migration; update when milestones complete.

## Context

We have successfully:
- ✅ Fixed migration to create non-destructive `-v7.root` files
- ✅ Fixed scanner to correctly find strings/records split in v6 tables
- ✅ Verified root tables are intentionally minimal (1-2 entries as entry points)
- ✅ Documented database architecture and UserTalk addressing

However, we have **not yet proven**:
- ❓ External table traversal works correctly (following type=13 references)
- ❓ All value types can be read from v6 databases
- ❓ Migration preserves the entire database tree structure
- ❓ Headless runtime can actually load and navigate v6 databases

## Problem Statement

From user feedback:
> "I'm not confident that the headless code reads the v6 databases correctly, nor am I confident in the code that migrates to v7 as neither has been proven to work 100% yet."

We need to validate the entire database reading and migration pipeline by:
1. Traversing complete database trees (not just root tables)
2. Reading all value types at all depths
3. Comparing v6 vs migrated v7 databases for structural equivalence
4. Testing runtime database access in headless mode

## Goals

### Primary Goals
1. Prove we can correctly read complete v6 database trees
2. Validate migration produces structurally identical v7 databases
3. Identify any value types or edge cases we're not handling
4. Document the actual database content and structure

### Secondary Goals
- Generate comprehensive type usage statistics
- Create test fixtures for regression testing
- Build confidence in the headless database implementation

## Plan

### Phase 1: Enhanced Scanner (Deep Traversal)
**Duration**: 1 day

Enhance `scripts/scan_database_types.py` to:

**Features:**
- Follow external table references (type=13, externalvaluetype)
- Recursively traverse the entire database tree to maximum depth
- Track depth histogram and nesting statistics
- Report all value types encountered at all levels
- Show table structure with indentation/tree view
- Generate checksums for validation purposes

**Output Example:**
```
Scanning Frontier.root (5.8MB)...

Database Structure:
  Root table (1 entry) @ 0x005d1063
    └─ "" [ostype] → system table tree root
       ├─ temp [table] @ 0x00234567 (depth 1)
       │  ├─ databases [table] @ 0x00345678 (depth 2)
       │  │  └─ (empty table)
       │  ├─ stack [table] @ 0x00456789 (depth 2)
       │  └─ ... (48 more entries)
       ├─ verbs [table] @ 0x00567890 (depth 1)
       │  ├─ builtins [table] @ 0x00678901 (depth 2)
       │  │  ├─ string [table] @ 0x00789012 (depth 3)
       │  │  │  ├─ upper [code] (depth 4)
       │  │  │  ├─ lower [code] (depth 4)
       │  │  │  └─ ... (30 more entries)
       │  │  └─ ... (20 more tables)
       │  └─ ... (150 more entries)
       └─ ... (200 more top-level entries)

Statistics:
  Total entries scanned: 1,247
  Max depth: 5 levels
  Total tables: 423

  Value Type Distribution:
    externalvaluetype (table): 423 (34%)
    stringvaluetype:           312 (25%)
    longvaluetype:             198 (16%)
    booleanvaluetype:          87  (7%)
    codevaluetype:             65  (5%)
    ostypevaluetype:           45  (4%)
    ... (15 more types)

  External Type Distribution:
    idtableprocessor:          423 (100%)
    idoutlineprocessor:        0
    idscriptprocessor:         0
    ... (other external types)

  Depth Histogram:
    Depth 0: 1 table   (root)
    Depth 1: 12 tables (top-level)
    Depth 2: 145 tables
    Depth 3: 198 tables
    Depth 4: 67 tables
    Depth 5: 0 tables
```

**Implementation:**
- Add `_scan_external_at()` recursive logic (already partially exists)
- Track visited addresses to prevent infinite loops
- Add depth limiting (max depth 10 to prevent runaway)
- Use indented output for tree visualization
- Calculate checksums of table contents for validation

**Files Modified:**
- `scripts/scan_database_types.py`

### Phase 2: Migration Validation
**Duration**: 0.5 days

Compare v6 databases with their migrated v7 counterparts:

**Process:**
1. Run enhanced scanner on v6 database → save structure + checksums
2. Migrate v6 → v7 using current migration code
3. Run enhanced scanner on v7 database → save structure + checksums
4. Compare outputs for equivalence

**Validation Checks:**
- Same number of total entries
- Same value type distribution
- Same depth histogram
- Same table structure (tree shape)
- Equivalent content checksums (accounting for header differences)

**Files:**
- Create `scripts/validate_migration.py` (uses enhanced scanner)
- Test with: Frontier.root, prefs.root, manila.root

### Phase 3: Headless Runtime Test
**Duration**: 0.5 days

Validate that frontier-cli can actually load and navigate v6 databases:

**Test Cases:**
1. Load v6 database: `frontier-cli --database prefs.root`
2. Navigate tree: Eval `sizeof(prefs)`, `typeof(prefs)`
3. Read values at depth: Eval `prefs.windowPosition`
4. Access system tables: Eval `sizeof(system.verbs.builtins)`

**Success Criteria:**
- No segfaults or errors during load
- Can enumerate table contents
- Can read values at all depths
- Values match expected types

**Files:**
- Create test script: `tests/integration/test_v6_database_access.sh`
- Document findings in: `planning/phase3/v6_database_validation_results.md`

## Value Type Reference

### Primary Value Types (tyvaluetype)
From `Common/headers/lang.h`:
```c
uninitializedvaluetype = -1
novaluetype = 0
charvaluetype = 1
intvaluetype = 2
longvaluetype = 3
oldstringvaluetype = 4
binaryvaluetype = 5
booleanvaluetype = 6
tokenvaluetype = 7
datevaluetype = 8
addressvaluetype = 9
codevaluetype = 10
doublevaluetype = 11
stringvaluetype = 12
externalvaluetype = 13       ← Follow these for tree traversal!
directionvaluetype = 14
passwordvaluetype = 15
ostypevaluetype = 16
unused2valuetype = 17
pointvaluetype = 18
rectvaluetype = 19
patternvaluetype = 20
rgbvaluetype = 21
fixedvaluetype = 22
singlevaluetype = 23
olddoublevaluetype = 24
objspecvaluetype = 25
filespecvaluetype = 26
aliasvaluetype = 27
enumvaluetype = 28
listvaluetype = 29
recordvaluetype = 30
```

### External Value Types (tyexternalid)
From `Common/headers/langexternal.h`:
```c
idoutlineprocessor = 0
idwordprocessor = 1
idheadrecord = 2
idtableprocessor = 3         ← Most common (tables)
idscriptprocessor = 4
idmenuprocessor = 5
idpictprocessor = 6
```

## Success Criteria

1. ✅ Scanner can traverse full database trees without errors
2. ✅ All value types present in test databases are correctly read
3. ✅ Migration produces structurally equivalent databases
4. ✅ Headless runtime can load and navigate v6 databases
5. ✅ Findings documented for future reference

## Risks & Mitigation

### Risk: Infinite Loops
**Mitigation**: Track visited addresses, limit max depth to 10

### Risk: Unknown Value Types
**Mitigation**: Scanner reports unknown types, we handle them incrementally

### Risk: Corruption in Test Databases
**Mitigation**: Test multiple databases (Frontier.root, prefs.root, manila.root)

### Risk: Format Misunderstandings
**Mitigation**: Reference original code at d37f634 when issues arise

## Next Steps After Validation

1. If issues found: Fix and re-validate
2. If successful: Move to runtime database operations (save, create, modify)
3. Document any limitations or known issues
4. Create regression test suite from findings

## Related Documents

- `docs/database_architecture.md` - Database structure and addressing
- `docs/legacy_frontier_bootstrap.md` - Legacy table format details
- `planning/phase3/v6_to_v7_migration_gaps.md` - Known migration issues
- `planning/phase3/headless_legacy_table_loader.md` - Table loading design

---

**Next Action**: Begin Phase 1 - Enhance scanner for deep traversal
