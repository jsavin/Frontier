# P0b: Launch Requirements (Weeks 4-6)

**Status**: LAUNCH BLOCKING
**Timeline**: 3 weeks
**Goal**: Complete launch requirements, establish system context pattern

---

## Scope

**What We're Fixing**: 25 globals blocking concurrent operations on system tables

**Categories**:
- System table context (roottable, systemtable, efptable, 20+ tables)
- Shell configuration (config, iddefaultconfig)
- Dialog state (dialogstack)

**Why P0b**: System tables accessed by ALL operations. Global system state = no concurrent user operations possible.

---

## Model Selection Guide

Each week uses the appropriate Claude model based on task complexity:

| Week | Scope | Complexity | Model | Rationale |
|------|-------|------------|-------|-----------|
| **Week 4** | System Context Structure | High | 🟡 Sonnet | Architectural design, reference counting patterns, new context structure |
| **Week 5** | System Table Migration | High | 🟡 Sonnet | Complex migration, removing ADR-008 workaround, system-wide impact |
| **Week 6** | Shell Config & Dialog State | Low | 🟢 Haiku | Pattern-following from P0a, established thread-local template |

**Using This Guide**:
- **🟢 Haiku**: Pattern-following with clear template from P0a Week 1-3
- **🟡 Sonnet**: Architectural design, complex system interactions, critical infrastructure

**When to Escalate to Sonnet**:
- System context lifecycle proves more complex than expected
- ADR-008 removal breaks processor resolution
- Test failures requiring architectural debugging

---

## Week-by-Week Plan

### Week 4: System Context Structure

**Model**: 🟡 **Sonnet** - Architectural design, reference counting patterns, new context structure

**Goal**: Design and implement `system_context` structure

**What**: Create container for all system tables with reference counting

**Implementation** (🟡 Sonnet for all tasks):
```c
typedef struct system_context {
    // Core tables
    hdlhashtable roottable;
    hdlhashtable systemtable;
    hdlhashtable internaltable;
    hdlhashtable langtable;

    // Processor tables
    hdlhashtable efptable;
    hdlhashtable builtinstable;
    hdlhashtable verbstable;

    // Environment
    hdlhashtable pathstable;
    hdlhashtable environmenttable;

    // Infrastructure
    hdlhashtable runtimestacktable;
    hdlhashtable semaphoretable;
    hdlhashtable threadtable;

    // Reference counting
    _Atomic uint32_t refcount;
} system_context;
```

**API**:
```c
system_context* system_context_create(void);
system_context* system_context_retain(system_context *ctx);
void system_context_release(system_context *ctx);
system_context* get_system_context(void);  // Global accessor
```

**Deliverable**: PR #4 - System Context Foundation

---

### Week 5: System Table Migration

**Model**: 🟡 **Sonnet** - Complex migration, removing ADR-008 workaround, system-wide impact

**Goal**: Migrate all system tables to system_context

**Implementation** (🟡 Sonnet for all tasks):
1. Move global system table pointers into system_context
2. Update `settablestructureglobals()` to use context
3. Create backward-compatible macros:
   ```c
   #define roottable (get_system_context()->roottable)
   #define systemtable (get_system_context()->systemtable)
   #define efptable (get_system_context()->efptable)
   ```
4. **Remove ADR-008 workaround** (`get_headless_efptable()`)
5. Remove global declarations

**Testing**:
```bash
./tools/run_headless_tests.sh
cd tests && make test-integration
# Verify processor resolution works
./frontier-cli/frontier-cli --system-root databases/Frontier.root -e "sizeOf(system)"
```

**Success**: System tables in context, no efptable workaround

**Deliverable**: PR #5 - System Table Context Migration

---

### Week 6: Shell Configuration & Dialog State

**Model**: 🟢 **Haiku** - Pattern-following from P0a, established thread-local template

**Goal**: Thread-safe shell configuration and dialog nesting

**Globals to Migrate**:
- `config` (config.h:113) - Window configuration
- `iddefaultconfig` (config.h:115) - Default window type
- `dialogstack[maxnesteddialogs]` (langdialog.c:57) - Dialog nesting
- `topdialogstack` - Stack pointer

**Implementation**:
1. Add to `tythreadglobals`: config, iddefaultconfig, dialogstack
2. Migrate `shellpushdefaultglobals()` / `shellpopglobals()` pattern
3. Update thread swap functions
4. Create backward-compatible macros

**Testing**:
```bash
./tools/run_headless_tests.sh
# Shell configuration tests (if applicable in headless)
```

**Success**: Shell config thread-safe, no contamination between threads

**Deliverable**: PR #6 - Shell Config Thread-Safety

---

## Success Criteria (P0b Complete)

- ✅ No global mutable state blocking concurrent operations
- ✅ System tables in explicit context (no workarounds)
- ✅ Shell configuration thread-safe
- ✅ All tests pass (unit + integration + thread sanitizer)
- ✅ **READY FOR LAUNCH** 🚀

---

## Files Modified (Estimated)

**System Context**:
- `Common/headers/tablestructure.h` - system_context definition
- `Common/source/tablestructure.c` - Context lifecycle, remove globals

**Shell Configuration**:
- `Common/headers/processinternal.h` - tythreadglobals additions
- `Common/source/process.c` - Thread swap functions
- `Common/headers/config.h` - Macros, remove globals

**Dialog State**:
- `Common/headers/langdialog.h` - Macros
- `Common/source/langdialog.c` - Remove globals

**ADR-008 Cleanup**:
- Remove `get_headless_efptable()` workaround
- Update references in verb processors

**Total**: ~8-10 files modified across 3 PRs

---

## Developer Resources

**Pattern**: See [../00-overview/README.md](../00-overview/README.md) - Pattern 2 (Explicit Context)

**Reference**:
- [ADR-008](../../architectural_decision_records/ADR-008-efp-workaround-processor-table-global.md) - Workaround to eliminate
- [db_context pattern](../../phase3/database_context_refactoring.md) - Proven context pattern

---

## Risks & Mitigation

**Risk**: System context lifecycle complex
**Mitigation**: Start simple (single global instance), add thread-local in P1

**Risk**: Breaking system table initialization
**Mitigation**: Comprehensive database loading tests

**Risk**: ADR-008 removal breaks processor resolution
**Mitigation**: Test all verb processors thoroughly

---

## Launch Readiness Checklist

After P0b completion, verify:
- [ ] No global mutable state blocks concurrent operations
- [ ] Thread sanitizer clean under concurrent load
- [ ] Database loading/saving works
- [ ] Processor resolution works (all verb categories)
- [ ] Shell configuration doesn't contaminate between threads
- [ ] All integration tests pass (600+ tests)

**When complete**: Frontier runtime is **LAUNCH READY** for thread-safe operations

---

## Next Phase

After P0b completion → **[P1a: Multi-User Foundation](../p1a-multi-user-foundation/README.md)**

P1a adds reference counting for multi-user object sharing.

---

**Last Updated**: 2026-01-19
**Status**: Starts after P0a completion (Week 4, with model selection guidance)
