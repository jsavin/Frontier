# Verb Binding Quick Wins - Test-Guided Implementation Plan

**Status**: Ready to implement
**Created**: 2025-12-31
**Priority**: P0 - Unlocks major functionality

## Executive Summary

**Critical Discovery**: The bottleneck is NOT missing implementations - it's missing bindings. Many verbs are fully implemented in `Common/source/*.c` but aren't accessible because the auto-generated stub files in `tests/headless_*_verbs.c` don't forward to them.

**Opportunity**: Increase verb coverage from 13% → 25% in 3-6 hours by adding bindings for verbs that already have:
- ✅ Complete C implementations
- ✅ Passing tests (or tests ready to run)
- ❌ Missing bindings only

## Current State

From corrected analyzer report (`reports/coverage/verb-binding/2025-12-31-09.md`):
- **97/710 verbs implemented (13%)**
- **613 verbs stubbed (86%)**

Major processors at 0% implementation:
- file: 0% (86/86 verbs stubbed)
- op: 0% (45/45 verbs stubbed)
- table: 0% (18/18 verbs stubbed)
- db: 0% (13/13 verbs stubbed)
- string: 5% (57/60 verbs stubbed)
- sys: 6% (15/16 verbs stubbed)

## Phase 1: Quick Wins (3-6 hours)

**PRIORITY ORDER**: String → Table → File

### 1. String Verbs (Highest Priority - Universal Utility)

**Impact**: Basic string operations needed everywhere, unblocks integration tests

**Target verbs (10)**:
1. `string.length()` - Basic operation (integration test ready)
2. `string.mid()` - Substring extraction (integration test ready)
3. `string.nthCharacter()` - Character access (integration test ready)
4. `string.patternMatch()` - String search (integration test ready)
5. `string.trimWhitespace()` - String cleaning (integration test ready)
6. `string.delete()` - String manipulation
7. `string.insert()` - String manipulation
8. `string.replace()` - String manipulation
9. `string.countfields()` - Parsing
10. `string.nthfield()` - Parsing

**Implementation location**: `tests/headless_string_verbs.c`

**Pattern**: Many string operations are in `Common/source/langverbs.c` already, need to add bindings

**Integration test coverage**: 10/21 string tests currently failing due to missing bindings - immediate ROI

### 2. Table Verbs (Second Priority - Core Functionality)

**Impact**: Enable basic table manipulation from UserTalk

**Target verbs (5)**:
1. `table.assign()` - Tests: `test_table_operations.c` (PASSING)
2. `table.getcursor()` - Tests: `test_table_operations.c`
3. `table.goto()` - Tests: `test_table_operations.c`
4. `table.emptytable()` - Used in many tests
5. `table.packtable()` - Core serialization

**Implementation location**: `tests/headless_table_verbs.c`

**Pattern**: Forward to existing implementations in `Common/source/tableverbs.c` or table operations in `Common/source/tableops.c`

### 3. File Verbs (Third Priority - I/O Operations)

**Impact**: Unlock `file_verb_tests` which are currently skipped

**Target verbs (7)**:
1. `file.exists()` - Tests: `test_file_operations.c`
2. `file.readwholefile()` - Tests: `test_file_operations.c`
3. `file.writewholefile()` - Tests: `test_file_operations.c`
4. `file.delete()` - Tests: `test_file_operations.c`
5. `file.rename()` - Tests: `test_file_operations.c`
6. `file.newfolder()` - Tests: `test_file_operations.c`
7. `file.size()` - Tests: `test_file_operations.c`

**Implementation location**: `tests/headless_file_verbs.c`

**Pattern**: Update auto-generated stubs to forward to `portable/file_portable.c`:
```c
case filefunc:
    /* file.exists - forward to real implementation */
    return fileexistsfunc(hparam1, vreturned);
```

**Blocker check**: Need to verify which file operations are actually implemented in `portable/file_portable.c`

## Phase 2: Medium Effort (6-12 hours)

### Op (Outline) Verbs

**Target verbs** (from tests):
- `op.insert()`, `op.setlinetext()`, `op.getlinetext()`
- `op.go()`, `op.firstsummit()`
- `op.countsubs()`, `op.level()`

**Implementation location**: `tests/headless_op_verbs.c`

### DB Verbs

**Target verbs**:
- `db.open()`, `db.close()`, `db.save()`
- `db.getvalue()`, `db.setvalue()`
- `db.defined()`, `db.delete()`

**Implementation location**: `tests/headless_db_verbs.c`

**Note**: Database verbs likely need more infrastructure work

### Sys Verbs

**Special case**: `sys.unixshellcommand()` has implementation but test shows "UserTalk glue not updated"

**Questions**:
- Should we update UserTalk glue or C binding?
- What's the current implementation status?

## Implementation Strategy

### Pattern 1: Direct Forward (Simplest)

For verbs with complete implementations, just forward:

```c
case file_exists:
    /* file.exists - forward to real implementation */
    return fileexistsfunc(hparam1, vreturned);
```

### Pattern 2: Thin Wrapper

For verbs needing parameter extraction:

```c
case string_length:
    /* string.length - get length of string */
    if (!getreadonlytextvalue(hparam1, 1, &htext)) return false;
    return setlongvalue(gethandlesize(htext), vreturned);
```

### Pattern 3: Update Glue Script

For verbs implemented in UserTalk:
- Update `usertalk_scripts/Frontier.root/system/verbs/builtins/*/`
- Example: `file.getSpecialFolderPath` (see Issue #219)

## Testing Strategy

### Current Test Infrastructure

1. **Unit tests**: `tests/test_*.c` files
2. **Runtime tests**: `tests/runtime_tests/*.c`
3. **Test runner**: `tools/run_headless_tests.sh`

### Verification Process

For each binding added:
1. Run specific test: `./tests/test_file_operations` (if available)
2. Run full test suite: `./tools/run_headless_tests.sh`
3. Manual CLI test: `./frontier-cli/frontier-cli -e "file.exists(\"/tmp\")"`

### Success Criteria

- Test suite passes with new bindings
- CLI accepts and executes bound verbs
- No regressions in existing tests

## Effort Estimates

**Priority Order**: String → Table → File

| Phase | Verbs | Time | Coverage Impact | Notes |
|-------|-------|------|-----------------|-------|
| String verbs | 10 | 1-2 hrs | +1.4% | **FIRST** - Unblocks 10 integration tests |
| Table verbs | 5 | 1-2 hrs | +0.7% | **SECOND** - Core ODB functionality |
| File verbs | 7 | 1-2 hrs | +1% | **THIRD** - I/O operations |
| **Phase 1 Total** | **22** | **3-6 hrs** | **+3.1%** (13% → 16%) | |
| Op verbs | 10 | 3-4 hrs | +1.4% | Phase 2 |
| DB verbs | 7 | 3-4 hrs | +1% | Phase 2 |
| Sys verbs | 5 | 2-3 hrs | +0.7% | Phase 2 |
| **Phase 2 Total** | **22** | **8-11 hrs** | **+3.1%** (16% → 19%) | |

**Note**: These are conservative estimates. With established pattern, throughput may be 10-20 verbs/hour for simple forwards.

## Questions for User

### 1. Priority

Which processors matter most for your workflow?
- File operations? (reading/writing files)
- Table operations? (ODB manipulation)
- Outline operations? (working with outlines)
- String operations? (text processing)

### 2. Testing Approach

- Should we write new tests for each verb?
- Or rely on existing tests and manual verification?
- What's the threshold for "good enough" test coverage?

### 3. Binding Pattern

For file verbs, should we:
- **Option A**: Update `tests/headless_file_verbs.c` to forward to `portable/file_portable.c`
- **Option B**: Implement directly in `tests/headless_file_verbs.c`
- **Option C**: Move implementations to `Common/source/` and link them

### 4. UserTalk Glue

For verbs like `sys.unixshellcommand()` that have C implementations but tests show "UserTalk glue not updated":
- Should we update the glue scripts?
- Or change the binding to call C directly?
- What's the long-term strategy here?

## Next Steps

### Immediate (Proof of Concept)

1. Implement 3-5 file verbs as proof-of-concept
2. Get `file_verb_tests` passing
3. Validate the approach and refine estimates

### After POC Success

1. Complete all Phase 1 quick wins
2. Generate new verb binding coverage report
3. Assess Phase 2 priorities based on user feedback

## Dependencies

- None identified - implementations already exist
- Main blocker is time/effort to add bindings
- Testing infrastructure is in place

## Success Metrics

- **Phase 1 complete**: Coverage increases from 13% → 16%
- **File tests passing**: `file_verb_tests` no longer skipped
- **No regressions**: Existing tests continue to pass
- **Pattern established**: Clear template for future verb bindings

## Related Documents

- Verb binding coverage report: `reports/coverage/verb-binding/2025-12-31-09.md`
- Verb implementation guide: `docs/VERB_IMPLEMENTATION_GUIDE.md`
- Testing guide: `docs/TESTING_GUIDE.md`
- File path implementation plan: `planning/phase3/SPECIAL_FOLDER_PATHS_IMPLEMENTATION.md` (Issue #219)

## Notes

- This plan was generated from system-architect agent analysis on 2025-12-31
- Based on intersection of passing tests and verb binding coverage report
- Conservative estimates - actual throughput may be higher once pattern is established
- Many "stubbed" verbs are actually implemented, just not bound to UserTalk
