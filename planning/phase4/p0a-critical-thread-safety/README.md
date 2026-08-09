# P0a: Critical Thread-Safety (Weeks 1-3)

**Status**: LAUNCH BLOCKING
**Timeline**: 3 weeks
**Goal**: Eliminate globals causing immediate thread-safety violations

---

## Status correction (2026-08-09)

P0a is **partially landed**, not unstarted. The hash-table context work listed in "Scope" below has
already moved, and it moved only halfway:

- **`currenthashtable` — DONE.** Migrated to thread-local storage in PR #536 (`f46fa7abd`,
  2026-04-15); the accessor macro lives at `Common/headers/processinternal.h:312`.
- **`hashtablestack` — STILL GLOBAL.** Its thread-local macro is commented out at
  `Common/headers/processinternal.h:295`. This is a deliberate constraint, not a missed step:
  bootstrap runs before `hthreadglobals` exists and requires direct access to the stack.
- **`hmagictable`** — unchanged from the original scope.

**Why this matters:** the resulting split-brain — half the hash-table context thread-local, half of
it global — is the direct cause of bug **#706**. Whoever picks P0a back up is resolving that
bootstrap-ordering constraint, not starting a fresh migration. Treat the "Scope" and week-by-week
sections below as the original plan rather than as current status.

Direction of record for current work: `product/VISION_1_0.md` and
`product/plans/2026-08-09-phase-0-1-execution-plan.md`.

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

## IMPORTANT: P0a Includes General-Purpose Callback Infrastructure

**IMPLEMENTATION COMPLETE (2026-01-20)**: P0a includes **general-purpose parameterized callback infrastructure** - NOT just for TCP!

**Status**: ✅ **IMPLEMENTED** - API ready for TCP Phase 3 integration

**Documentation**:
- [CALLBACK_INFRASTRUCTURE.md](./CALLBACK_INFRASTRUCTURE.md) - Architecture and design analysis
- [docs/CALLBACK_API.md](../../../docs/CALLBACK_API.md) - C Developer API guide with examples

**Implementation**:
- Location: `Common/source/lang.c:1253-1381`, `Common/headers/lang.h:766`
- Function: `langruncallbackwithparams()`
- Thread-safe: Yes (uses `grabthreadglobals`/`oppushoutline` pattern)
- Parameters: Accessible in UserTalk as param1, param2, param3, etc.

**Key Capabilities**:
- ✅ Type-safe parameter passing (long, string, boolean, double, addresses)
- ✅ Thread-safe execution from worker threads
- ✅ Backward compatible with parameterless callbacks
- ✅ Supports unlimited parameters
- ✅ Returns result value to caller

**Use Cases Enabled**:
- TCP `tcp.listenStream()` callbacks with (stream_id, remote_addr, remote_port)
- Window callbacks with parameters (e.g., `closeWindow(title)`)
- Database operation callbacks with object addresses
- System lifecycle callbacks with context
- This is a **PLATFORM CAPABILITY**, not TCP-specific

**Impact**: Makes P0a MORE VALUABLE - unblocks TCP, window operations, and all future parameterized callbacks.

**Next Step**: Integrate with `tcp.listenStream()` implementation in TCP Phase 3

---

## Model Selection Guide

Each week uses the appropriate Claude model based on task complexity:

| Week | Scope | Globals | Complexity | Model | Rationale |
|------|-------|---------|------------|-------|-----------|
| **Week 1** | Hash Table Context | 3 | Medium | 🟡 Sonnet | Architectural refactoring, proven pattern (ADR-005) but requires understanding hash scope resolution |
| **Week 2** | Parser State | 3 | High | 🟡 Sonnet | Parser modification is complex/risky, Bison-generated code, careful analysis needed |
| **Week 3** | Control Flow & Error | 4 | Low | 🟢 Haiku | Pattern-following (same as Week 1-2), simpler flag state, established template |

**Using This Guide**:
- **🟢 Haiku**: Pattern-following with clear template, straightforward implementation
- **🟡 Sonnet**: Architectural understanding required, complex system interaction, proven patterns need adaptation

**When to Escalate to Sonnet**:
- Unexpected complexity during implementation
- Test failures that require architectural debugging
- Need to understand cross-module interactions

---

## Week-by-Week Plan

### Week 1: Hash Table Context Migration

**Model**: 🟡 **Sonnet** - Architectural refactoring with proven pattern (ADR-005)

**Globals to Migrate**:
- `currenthashtable` (langhash.c:835) - Variable scope resolution
- `hmagictable` (lang.c:71) - Eval communication
- `hashtablestack` (related) - Table scope stack

**Implementation** (🟡 Sonnet for all tasks):
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

**Model**: 🟡 **Sonnet** - Parser modification is complex/risky, Bison-generated code

**Globals to Migrate**:
- `yylval` (langparser.h:33) - Parser token value
- `yyval` (langparser.h:33) - Parser result value
- `langparser_result` (langparser.h:35) - Final parse result

**Implementation** (🟡 Sonnet for all tasks):
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

**Model**: 🟢 **Haiku** - Pattern-following (same template as Week 1-2), simpler flag state

**Globals to Migrate**:
- `flbreak` (langinternal.h:333) - Break statement flag
- `flcontinue` (langinternal.h:335) - Continue statement flag
- `lasterror` (error.c) - Last OS error
- `lasterrormessage` (error.c) - Last error message

**Note**: `flreturn`, `fllangerror` already in tythreadglobals (lines 184, 196)

**Implementation** (🟢 Haiku for all tasks):
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

**Last Updated**: 2026-01-19
**Status**: Ready to start Week 1 (with model selection guidance)
