# Frontier Current TODO List

**Last Updated**: 2025-12-20
**Context**: Strategic planning complete, ready to begin implementation

---

## Immediate Next Steps

### 1. Strategic Plan Approval Status

**IFDEF Cleanup Plan**:
- ✅ **APPROVED** (2025-12-20) - `planning/phase3/code-cleanup/IFDEF_CLEANUP_STRATEGY.md`
- Ready for Phase 1 execution

**Logging Infrastructure Plan**:
- ✅ **APPROVED** (2025-12-20) - `planning/phase3/LOGGING_INFRASTRUCTURE_PLAN.md`
- Plain-text default + JSON output support
- Future improvements tracked in GitHub Issue #129

**Dead Code Removal Plan**:
- ⏳ **Awaiting approval** - `planning/phase3/code-cleanup/DEAD_CODE_REMOVAL_STRATEGY.md`
- Decision: Approve 5-phase approach (explicit markers → obsolete platforms → GUI stubbing)?

**Decisions still needed**:
- [ ] Approve dead code removal phasing (explicit markers → GUI stubbing → legacy formats)
- [ ] Confirm PIKE variant removal (29 blocks for different product)
- [ ] Confirm optional database backend flags (FRONTIER_SQLITE, FRONTIER_MYSQL, FRONTIER_PYTHON)

---

## Phase 1: Dead Code Removal - Explicit Markers (READY TO START)

**Timeline**: Week 1
**Risk**: ZERO - code explicitly marked as dead
**Prerequisites**: None

### 1A. Remove "xxx"-Prefixed Blocks

**Estimated**: 2-3 hours

- [ ] Create branch: `cleanup/remove-xxx-prefixed-dead-code`
- [ ] Remove `#ifdef xxxWIN95VERSION` blocks (11 occurrences)
  - [ ] strings.c:1339
  - [ ] shellwindow.c:59
  - [ ] langpack.c:868
  - [ ] (8 more files - see `planning/DEAD_CODE_REMOVAL_STRATEGY.md`)
- [ ] Remove `#ifdef xxxPIKE` block (1 occurrence)
  - [ ] shellwindowmenu.c:82
- [ ] Remove `#ifdef xxxfldebug` block (1 occurrence)
  - [ ] claybrowserexpand.c:504
- [ ] Test: `make clean && make && ./tools/run_headless_tests.sh`
- [ ] Commit: `cleanup: Remove xxx-prefixed dead code blocks (Phase 1A)`

**Expected result**: ~150 lines removed, 13 blocks eliminated

---

### 1B. Remove Explicit Dead Code Markers

**Estimated**: 1-2 hours

- [ ] Remove `#ifdef OBSOLETE` block (whirlpool.c:618)
  - [ ] Delete 1000+ line obsolete crypto lookup table
- [ ] Remove `#ifdef NEVER` block (langevaluate.c:897)
  - [ ] Delete unused error formatting code (~20 lines)
- [ ] Convert `#ifdef NeverDefine_For_Reference` to comment (WinSockNetEvents.c:50)
  - [ ] Convert to /* reference doc */ comment block
- [ ] Test: `make clean && make && ./tools/run_headless_tests.sh`
- [ ] Commit: `cleanup: Remove OBSOLETE and NEVER dead code markers (Phase 1B)`

**Expected result**: ~1,220 lines removed, 3 blocks eliminated

---

### 1C. Phase 1 Completion

- [ ] Verify all tests pass
- [ ] Update `planning/DEAD_CODE_REMOVAL_PROGRESS.md` (create if needed)
- [ ] Push branch to origin
- [ ] Create PR: "Dead Code Removal Phase 1: Explicit Markers"
- [ ] Merge after approval

**Phase 1 total**: ~1,370 lines removed, 16 blocks eliminated, ZERO risk

---

## Phase 2: Logging Infrastructure Foundation (READY TO START)

**Timeline**: Week 1 (after Phase 1)
**Risk**: LOW - no migration of existing code yet
**Prerequisites**: None

### Week 1: Infrastructure Implementation

**Estimated**: 4-8 hours

- [ ] Create branch: `feature/logging-infrastructure`
- [ ] Create `Common/headers/logging.h`
  - [ ] Define log levels (ERROR, WARN, INFO, DEBUG, TRACE)
  - [ ] Define log components (DB, HASH, TABLE, PACK, PARSE, etc.)
  - [ ] Define public API (log_init, log_error, log_debug, etc.)
  - [ ] Define convenience macros
- [ ] Create `Common/source/logging.c`
  - [ ] Implement log_init() (parse env vars)
  - [ ] Implement log_set_level()
  - [ ] Implement log_set_component_enabled()
  - [ ] Implement log_is_enabled()
  - [ ] Implement log_write()
- [ ] Update Makefile
  - [ ] Add logging.c to SOURCES
  - [ ] Add logging.h to HEADERS
- [ ] Add `log_init()` call to frontier-cli main()
- [ ] Test: Build and verify no regressions
- [ ] Create unit tests: `tests/test_logging.c`
  - [ ] Test log level filtering
  - [ ] Test component filtering
  - [ ] Test env var parsing
- [ ] Test: `make clean && make && ./tools/run_headless_tests.sh`
- [ ] Commit: `feat: Add logging infrastructure with runtime control`
- [ ] Push and create PR: "Logging Infrastructure Foundation"

**Expected result**: ~200 lines of logging code, zero impact on existing code

---

## Phase 3: Logging Migration (After Foundation Merged)

**Timeline**: Weeks 2-10 (incremental)
**Risk**: MEDIUM - requires testing after each file
**Prerequisites**: Logging infrastructure foundation merged

### Week 2: High-Volume Files

- [ ] Migrate langhash.c (58 fprintf statements)
  - [ ] Replace with `log_debug(LOG_COMP_HASH, ...)`
  - [ ] Remove `#ifdef fldebug` blocks
  - [ ] Test after migration
- [ ] Commit: `refactor: Migrate langhash.c to logging infrastructure`

### Week 3: Database Layer

- [ ] Migrate db.c (47 fprintf statements)
- [ ] Migrate db_format.c (46 fprintf statements)
  - [ ] Replace with `log_debug(LOG_COMP_DB, ...)`
  - [ ] Remove `#ifdef DATABASE_DEBUG` blocks
  - [ ] Test after migration
- [ ] Commit: `refactor: Migrate db.c and db_format.c to logging infrastructure`

### Week 4: Table Operations

- [ ] Migrate tableexternal_common.c (25 statements)
- [ ] Migrate tablepack.c (19 statements)
  - [ ] Replace with `log_debug(LOG_COMP_TABLE, ...)`
  - [ ] Test after migration
- [ ] Commit: `refactor: Migrate table operations to logging infrastructure`

### Weeks 5-10: Remaining Files (Incremental)

- [ ] Migrate remaining files (20+ files, 3-5 files per week)
- [ ] Test after each file migration
- [ ] Commit after each file or small group

### Week 10: Cleanup

- [ ] Verify all `#ifdef fldebug` blocks removed
- [ ] Verify all `#ifdef DATABASE_DEBUG` blocks removed
- [ ] Verify all `#ifdef DEBUG_SERIALIZER` blocks removed
- [ ] Update CONTRIBUTING.md with logging guidelines
- [ ] Final PR: "Complete logging infrastructure migration"

**Expected result**: 352 fprintf → 0, 76 debug ifdefs → 0 (22% of total ifdef count)

---

## Phase 4: Dead Code Removal - Obsolete Platforms (After Logging Foundation)

**Timeline**: Week 2-3 (can run parallel with logging migration)
**Risk**: LOW - platforms not supported
**Prerequisites**: Phase 1 complete

- [ ] Create branch: `cleanup/remove-obsolete-platforms`
- [ ] Remove `#ifdef oldMACVERSION` blocks (langhash.c, 3 blocks)
  - [ ] Mac OS Classic alias handling (no longer needed in v7)
- [ ] Remove commented `#ifdef WIN95VERSION` blocks (2 blocks)
  - [ ] langtrace.c:59 (trace file logging)
  - [ ] frontierwindows.c:128 (window display)
- [ ] Keep active `#ifdef WIN95VERSION` block (shellsysverbs.c:610)
  - [ ] Environment variable setting (still needed for Windows builds)
- [ ] Test: `make clean && make && ./tools/run_headless_tests.sh`
- [ ] Verify v7 migration still works
- [ ] Commit: `cleanup: Remove obsolete platform code (Phase 2)`
- [ ] Push and create PR

**Expected result**: ~540 lines removed, 5 blocks eliminated

---

## Future Phases (Deferred Pending Completion of Above)

### GUI Code Stubbing (Weeks TBD)
- Requires detailed dependency analysis
- Awaiting user approval on stubbing strategy
- See `planning/DEAD_CODE_REMOVAL_STRATEGY.md` Phase 3

### Legacy Format Writers (Weeks TBD)
- Remove v6 writer functions (keep v6 readers)
- See `planning/DEAD_CODE_REMOVAL_STRATEGY.md` Phase 4

### Stub Function Cleanup (Weeks TBD)
- Remove obvious stub functions
- See `planning/DEAD_CODE_REMOVAL_STRATEGY.md` Phase 5

### Feature Flag Consolidation (Months TBD)
- PIKE removal (pending approval)
- Optional database backends documentation
- See `planning/IFDEF_CLEANUP_STRATEGY.md` Phase 4

---

## Testing Checklist (After Each Phase)

**Run after every commit**:

```bash
# 1. Clean build
make clean && make

# 2. Run headless tests
./tools/run_headless_tests.sh

# 3. Verify migration
make -C tests clean && make -C tests save_migration_tests
./tests/save_migration_tests

# 4. Check verb bindings
cd tools/kernelverbs_parser && python3 cli.py analyze
```

**Expected**: All tests passing, no regressions

---

## Progress Tracking

### Completed
- ✅ Static analysis (tools installed, codebase analyzed)
- ✅ Strategic planning documents created
- ✅ Mode stack refactor (Issue #123 resolved)

### In Progress
- ⏳ User review of strategic plans

### Upcoming
- ⏳ Phase 1: Dead code explicit markers (Week 1)
- ⏳ Phase 2: Logging infrastructure foundation (Week 1)
- ⏳ Phase 2-3: Logging migration (Weeks 2-10)
- ⏳ Phase 2: Obsolete platform removal (Week 2-3)

### Blocked
- None currently

---

## Questions for User

### Strategic Direction

1. **Sequencing**: Start with dead code Phase 1 (quick win), or logging infrastructure (bigger impact)?
   - Recommendation: Dead code Phase 1 first (1 day, zero risk, immediate cleanup)

2. **Parallel work**: Should logging infrastructure and dead code removal proceed in parallel?
   - Recommendation: Phase 1 dead code → logging foundation → parallel (dead code Phase 2 + logging migration)

3. **PIKE removal**: Approve removal of 29 PIKE variant blocks?
   - Context: PIKE was a different product, not needed for headless Frontier
   - Recommendation: Remove (simplifies codebase significantly)

4. **GUI stubbing**: Approve wrapping GUI code with `#ifdef FRONTIER_HEADLESS` vs. immediate deletion?
   - Recommendation: Wrap first (safer), delete later after 6+ months of stability

### Tactical Decisions

5. **Logging components**: Are 11 components sufficient, or need more granularity?
   - Current: DB, HASH, TABLE, PACK, PARSE, EVAL, OP, LANG, EXTERNAL, STARTUP, GENERAL
   - Recommendation: Start with 11, add more if needed

6. **Testing rigor**: Is the proposed testing strategy (test after each file migration) sufficient?
   - Recommendation: Yes, but can adjust based on risk tolerance

---

## Open Issues (None Blocking)

All previously blocking issues resolved:
- ✅ Issue #123: External table variable management (RESOLVED via PR #125)
- ✅ Mode stack refactor (COMPLETE)

---

## Related Documentation

- Strategic Plans:
  - `planning/IFDEF_CLEANUP_STRATEGY.md`
  - `planning/LOGGING_INFRASTRUCTURE_PLAN.md`
  - `planning/DEAD_CODE_REMOVAL_STRATEGY.md`

- Analysis Reports:
  - `reports/static-analysis/2025-12-20-ifdef-inventory.md`
  - `reports/static-analysis/dead-code/2025-12-20-dead-code-categories.md`
  - `reports/static-analysis/logging/2025-12-20-logging-patterns.md`

- Status:
  - `_CURRENT_STATUS.md` - Current project state
  - `_STATUS_ARCHIVE.md` - Historical completions

---

*Last updated: 2025-12-20*
*Next review: After user approval of strategic plans*
