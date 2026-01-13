# P0a: Critical Thread-Safety (Weeks 1-3)

**Status**: LAUNCH BLOCKING
**Timeline**: 3 weeks
**Goal**: Eliminate globals causing immediate thread-safety violations

---

## Scope

**What We're Fixing**: 30 critical globals that create race conditions

**Categories**:
- Hash table context (currenthashtable, hmagictable, hashtablestack)
- Parser state (yylval, yyval, langparser_result)
- Control flow flags (flbreak, flcontinue)
- Error state (lasterror, lasterrormessage)

**Why P0a**: These globals are accessed on EVERY script execution. Race conditions here = immediate crashes.

---

## Week-by-Week Plan

### Week 1: Hash Table Context Migration

**Globals to Migrate**:
- `currenthashtable` (langhash.c:835) - Variable scope resolution
- `hmagictable` (lang.c:71) - Eval communication
- `hashtablestack` (related) - Table scope stack

**Implementation**:
1. Add fields to `tythreadglobals` structure
2. Create backward-compatible macros
3. Update thread swap functions (copythreadglobals, swapinthreadglobals, newthreadglobals)
4. Remove global declarations

**Testing**:
```bash
./tools/run_headless_tests.sh
cd tests && make test-integration
```

**Success**: Hash table scope is thread-safe, no race conditions on variable lookups

**Deliverable**: PR #1 - Hash Table Context Thread-Safety

---

### Week 2: Parser State Migration

**Globals to Migrate**:
- `yylval` (langparser.h:33) - Parser token value
- `yyval` (langparser.h:33) - Parser result value
- `langparser_result` (langparser.h:35) - Final parse result

**Implementation**:
1. Add fields to `tythreadglobals`
2. Create backward-compatible macros
3. Update thread swap functions
4. Remove global declarations from langparser.h

**Testing**:
```bash
./tools/run_headless_tests.sh
# Test UserTalk parsing specifically
./frontier-cli/frontier-cli -e 'lang.new(tableType, @t); return typeof(t)'
```

**Success**: Parser can run concurrently in multiple threads

**Deliverable**: PR #2 - Parser State Thread-Safety

---

### Week 3: Control Flow & Error State

**Globals to Migrate**:
- `flbreak` (langinternal.h:333) - Break statement flag
- `flcontinue` (langinternal.h:335) - Continue statement flag
- `lasterror` (error.c) - Last OS error
- `lasterrormessage` (error.c) - Last error message

**Note**: `flreturn`, `fllangerror` already in tythreadglobals (lines 184, 196)

**Implementation**:
1. Add remaining control flow fields to `tythreadglobals`
2. Create backward-compatible macros
3. Update thread swap functions
4. Remove global declarations

**Testing**:
```bash
./tools/run_headless_tests.sh
# Test loop control specifically
./frontier-cli/frontier-cli -e 'loop {if (true) {break}}; return "ok"'
```

**Success**: Control flow and error state are thread-safe

**Deliverable**: PR #3 - Control Flow Thread-Safety

---

## Success Criteria (P0a Complete)

- ✅ No race conditions on hash scope, parser state, control flow
- ✅ Multiple threads can execute UserTalk simultaneously
- ✅ All tests pass (unit + integration)
- ✅ Thread sanitizer clean (`make TSAN=1`)
- ✅ Zero API changes visible to existing code

---

## Files Modified (Estimated)

**Core Thread State**:
- `Common/headers/processinternal.h` - tythreadglobals structure
- `Common/source/process.c` - Thread swap functions

**Hash Table Context**:
- `Common/headers/lang.h` - Macro definitions
- `Common/source/langhash.c` - Remove global declarations

**Parser State**:
- `Common/headers/langparser.h` - Macro definitions, remove globals

**Control Flow**:
- `Common/headers/langinternal.h` - Macro definitions, remove globals

**Total**: ~6-8 files modified across 3 PRs

---

## Developer Resources

**Pattern Template**: See [../00-overview/quick_reference.md](../00-overview/quick_reference.md)

**Reference ADRs**:
- [ADR-005](../../architectural_decision_records/ADR-005-parameter-state-thread-safety.md) - Thread-local pattern
- [ADR-006](../../architectural_decision_records/../../architectural_decision_records/ADR-006-outline-context-stack-refactoring.md) - Proven implementation

**Testing Checklist**: Use checklist from quick reference for each PR

---

## Risks & Mitigation

**Risk**: Forgot to update one of the thread swap functions
**Mitigation**: Use checklist, test thoroughly, code review

**Risk**: Parser state more complex than expected
**Mitigation**: Week 2 buffer (can extend if needed)

**Risk**: Breaking existing control flow logic
**Mitigation**: Comprehensive loop/break/continue tests

---

## Next Phase

After P0a completion → **[P0b: Launch Requirements](../p0b-launch-requirements/README.md)**

P0b will establish system context pattern and migrate system tables.

---

**Last Updated**: 2026-01-13
**Status**: Ready to start Week 1
