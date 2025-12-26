# Verb Global State Dependency Analysis

**Status:** In Progress (Phase 1: Mapping Complete, Phase 2: Dependency Analysis Starting)

**Goal:** Determine which verbs depend on global state to inform whether Issue #86 (Global Runtime Context Refactoring) is a prerequisite for verb porting at scale.

**Target Coverage:** ~85% of verbs (595-600 of 707 total)

---

## Phase 1: Verb Landscape Mapping ✅ COMPLETE

### Total Verb Count
- **Total Verbs in kernelverbs.rc:** 707
- **GUI-Required Verbs:** 273 (39%)
- **Headless-Compatible Verbs:** 434 (61%)
- **Implemented Verbs:** 522 (74%)
- **Not Implemented:** 37 (5%)

### Verb Distribution by Family

| Family | Count | Implementation | Type | Status |
|--------|-------|-----------------|------|--------|
| file | 86 | fileverbs.c | mature | headless-ready |
| string | 60 | stringverbs.c | mature | headless-ready |
| lang | 58 | langverbs.c | core | headless-ready |
| op | 45 | opverbs.c | mature | GUI-heavy |
| table | 18 | tableverbs.c | mature | headless-ready |
| window | 31 | shellwindowverbs.c | UI | GUI-only |
| mysql | 27 | langmysql.c | external | headless-ready |
| html | 23 | langhtml.c | generation | headless-ready |
| tcp | 23 | langhtml.c | network | headless-ready |
| menu | 14 | menuverbs.c | UI | GUI-only |
| xml | 14 | langxml.c | parsing | headless-ready |
| db | 13 | dbverbs.c | core | headless-ready |
| script | 13 | opverbs.c | editor | GUI-heavy |
| frontier | 14 | shellsysverbs.c | integration | unknown |
| sqlite | 17 | langsqlite.c | external | headless-ready |
| thread | 17 | langthread.c | runtime | unknown |
| sys | 16 | shellsysverbs.c | system | unknown |
| date | 30 | langverbs.c (Pattern D) | utility | headless-ready |
| clock | 7 | langverbs.c (Pattern D) | utility | headless-ready |
| rez | 15 | langverbs.c (Pattern D) | resources | GUI-heavy |
| regex | 10 | langregexp.c | utility | headless-ready |
| crypt | 5 | langcrypt.c | utility | headless-ready |
| pict | 4 | pictverbs.c | graphics | GUI-only |
| math | 3 | langmath.c | utility | headless-ready |
| Other | 46 | Various | mixed | analysis-needed |

### Key Implementation Patterns

**Pattern A (Separate Processor Files):** opverbs.c, stringverbs.c, fileverbs.c, etc.
- Dedicated file per processor
- Separate enum for token dispatch
- Standard switch-case dispatch

**Pattern B (Shared Processor File):** shellsysverbs.c
- Multiple processors in one file
- Example: sys + frontier in same file

**Pattern C (Irregular Naming):** 24+ exceptions in verb_exceptions.py
- Prefix/infix adjustments for verb names
- Affects file, op, table, string, crypt families

**Pattern D (Consolidated in langverbs.c):** 16 processors
- Dispatch through single langfunctionvalue() switch
- 174+ cases in one function (3,616 lines)
- Includes: clock, date, dialog, kb, mouse, point, rectangle, rgb, speaker, target, bit, semaphore, base64, dll, rez, py

**Pattern E (Headless Stubs):** Auto-generated from stub_config.py
- 31 configured stubs
- Error stubs for GUI-required verbs
- Forward stubs for real C implementations
- Silent no-ops for safe operations

### Reference Files Located
- Master definitions: `/Common/resources/Win32/kernelverbs.rc` (707 verbs, 51 processors)
- Processor IDs: `/Common/headers/kernelverbdefs.h`
- Initialization: `/generated/kernel_verbs_init.c` (26 processors)
- Headless dispatch: `/Common/source/kernel_verbs_headless.c`
- RC parser: `/tools/kernelverbs_parser/parse_kernelverbs.py`
- Stub config: `/tools/kernelverbs_parser/stub_config.py`
- Exception mappings: `/tools/kernelverbs_parser/verb_exceptions.py`

---

## Phase 2: Global State Dependency Analysis 🔄 IN PROGRESS

### Analysis Approach

For each verb family, we will:
1. **Identify global state accessed** - scan implementation files for globals
2. **Categorize dependency type** - database context, target/window, execution state, etc.
3. **Mark GUI-only verbs** - skip and document
4. **Rate headless compatibility** - works as-is, needs context wrapping, or infeasible
5. **Flag for #86 dependency** - does this verb require context refactoring first?

### Global State Categories

#### Core Runtime Globals (Potential #86 Impact)
- **Database context** - current database, open databases
- **Target/Window context** - current target window
- **Outline context** - current outline being edited (op_context)
- **Execution state** - variable scope, call stack

#### Implementation-Specific Globals
- **File handling** - file I/O state
- **Networking state** - connection management
- **Threading state** - thread-local storage
- **Resource state** - graphics contexts, menus

### Findings by Family (ANALYSIS STARTING)

**NOTE:** This section will be populated as analysis progresses. Updates will be committed to track progress.

---

## Phase 3: Safe Headless Noops

List of operations that can be safely implemented as no-ops in headless mode:
(To be populated during analysis)

---

## Phase 4: User Decision Questions

Questions requiring user expertise and decision-making:
(To be populated at end of analysis)

---

## Timeline & Progress

- [x] Phase 1: Verb landscape mapping (Complete - comprehensive map of 707 verbs)
- [ ] Phase 2: Global state dependency analysis (In Progress - starting with file/string/lang families)
- [ ] Phase 3: Safe noop identification (Pending)
- [ ] Phase 4: User decision questions (Pending)
- [ ] Final report (Pending)

---

## Document History
- **2025-12-25:** Started Phase 1 mapping (complete landscape of verb implementations)
- **2025-12-25:** Progressing to Phase 2 detailed dependency analysis

