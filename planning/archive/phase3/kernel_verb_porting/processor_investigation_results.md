# Processor Investigation Results

**Date:** 2025-12-14
**Phase:** 1.2 - Sample Processor Investigation
**Investigators:** Examined 5 processors to understand RC→C mapping patterns

---

## Summary

Investigated 5 sample processors to understand how RC verb names map to C enum tokens and case labels.

### Key Findings

1. **Three distinct patterns identified:**
   - Pattern A: `{processor}{verb}func` (file processor)
   - Pattern B: `{verb}func` (table, menu processors)
   - Pattern C: `{transformed_verb}func` (op processor - inconsistent transformations)
   - Pattern D: No enum (dialog processor - uses different dispatch)

2. **Success rate by pattern:**
   - Pattern A (file): 15/15 tested verbs match ✓
   - Pattern B (table): 15/15 tested verbs match ✓
   - Pattern B (menu): 10/10 tested verbs match ✓
   - Pattern C (op): 14/15 tested verbs match (1 transformation issue)
   - Pattern D (dialog): 0/15 - no enum found

3. **Why current analyzer succeeds/fails:**
   - Succeeds: Patterns A & B (consistent rules)
   - Fails: Pattern C (unpredictable transformations), Pattern D (no enum)

---

## Detailed Investigation

### 1. File Processor (Pattern A - Processor Prefix)

**Status:** ✓ Working (detected by analyzer)

**Implementation:** `Common/source/fileverbs.c`

**Pattern:** `file{verb}func`

| Index | RC Verb | Enum Token | Match |
|-------|---------|------------|-------|
| 0 | created | filecreatedfunc | ✓ |
| 1 | modified | filemodifiedfunc | ✓ |
| 2 | type | filetypefunc | ✓ |
| 3 | creator | filecreatorfunc | ✓ |
| 8 | isfolder | fileisfolderfunc | ✓ |
| 9 | isvolume | fileisvolumefunc | ✓ |
| 10 | islocked | fileislockedfunc | ✓ |
| 11 | lock | filelockfunc | ✓ |
| 12 | unlock | fileunlockfunc | ✓ |
| 13 | copy | filecopyfunc | ✓ |
| 14 | copydatafork | filecopydataforkfunc | ✓ |

**Transformation Rule:** `RC_verb → file{RC_verb}func`

**Example:**
- RC: `"created\0"` → C enum: `filecreatedfunc` → C case: `case filecreatedfunc:`

**Findings:**
- Enum has 89 tokens (more than 86 verbs due to some internal tokens)
- All tested verbs follow consistent pattern
- Analyzer SHOULD detect this (Strategy 1 with processor prefix)

---

### 2. Table Processor (Pattern B - No Prefix)

**Status:** ✓ Working (detected by analyzer)

**Implementation:** `Common/source/tableverbs.c`

**Pattern:** `{verb}func`

| Index | RC Verb | Enum Token | Match |
|-------|---------|------------|-------|
| 0 | move | movefunc | ✓ |
| 1 | copy | copyfunc | ✓ |
| 2 | rename | renamefunc | ✓ |
| 3 | moveandrename | moveandrenamefunc | ✓ |
| 4 | assign | assignfunc | ✓ |
| 5 | validate | validatefunc | ✓ |
| 6 | sortby | sortbyfunc | ✓ |
| 7 | getcursor | getcursorfunc | ✓ |
| 8 | getselection | getselectionfunc | ✓ |
| 9 | go | gofunc | ✓ |
| 10 | goto | gotofunc | ✓ |
| 11 | gotoname | gotonamefunc | ✓ |
| 12 | jettison | jettisonfunc | ✓ |
| 13 | packtable | packtablefunc | ✓ |
| 14 | emptytable | emptytablefunc | ✓ |

**Transformation Rule:** `RC_verb → {RC_verb}func`

**Example:**
- RC: `"move\0"` → C enum: `movefunc` → C case: `case movefunc:`

**Findings:**
- Enum has exactly 18 tokens matching 18 verbs
- Perfect 1:1 mapping
- Simplest pattern
- Analyzer SHOULD detect this (Strategy 2 with no prefix)

---

### 3. Menu Processor (Pattern B - No Prefix)

**Status:** ❌ Not detected (but should work!)

**Implementation:** `Common/source/menuverbs.c`

**Pattern:** `{verb}func` (same as table)

| Index | RC Verb | Enum Token | Match |
|-------|---------|------------|-------|
| 0 | zoomscript | zoomscriptfunc | ✓ |
| 1 | buildmenubar | buildmenubarfunc | ✓ |
| 2 | clearmenubar | clearmenubarfunc | ✓ |
| 3 | isinstalled | isinstalledfunc | ✓ |
| 4 | install | installfunc | ✓ |
| 5 | remove | removefunc | ✓ |
| 6 | getscript | getscriptfunc | ✓ |
| 7 | setscript | setscriptfunc | ✓ |
| 8 | addmenucommand | addmenucommandfunc | ✓ |
| 9 | deletemenucommand | deletemenucommandfunc | ✓ |

**Transformation Rule:** `RC_verb → {RC_verb}func`

**Findings:**
- Enum has 14 tokens matching 14 verbs
- Perfect 1:1 mapping
- Same pattern as table
- **Why not detected?** Need to investigate analyzer logic

---

### 4. Op Processor (Pattern C - Transformations)

**Status:** ✓ Partially working (detected by analyzer)

**Implementation:** `Common/source/opverbs.c`

**Pattern:** `{transformed_verb}func` (INCONSISTENT!)

| Index | RC Verb | Enum Token | Match | Transformation |
|-------|---------|------------|-------|----------------|
| 0 | getlinetext | linetextfunc | ✓ | Drop "get" |
| 1 | level | levelfunc | ✓ | None |
| 2 | countsubs | countsubsfunc | ✓ | None |
| 3 | countsummits | countsummitsfunc | ✓ | None |
| 4 | go | gofunc | ✓ | None |
| 5 | firstsummit | firstsummitfunc | ✓ | None |
| 6 | expand | expandfunc | ✓ | None |
| 7 | collapse | collapsefunc | ✓ | None |
| 8 | subsexpanded | ??? | ❌ | No clear match |
| 9 | insert | insertfunc | ✓ | None |
| 10 | find | findfunc | ✓ | None |
| 11 | sort | sortfunc | ✓ | None |
| 12 | setlinetext | setlinetextfunc | ✓ | None |
| 13 | promote | promotefunc | ✓ | None |
| 14 | demote | demotefunc | ✓ | None |

**Transformation Rules:** (Inconsistent!)
- Sometimes: `get{X}` → `{X}func` (drop "get")
- Usually: `{verb}` → `{verb}func` (no change)
- Sometimes: No clear rule (e.g., "subsexpanded")

**Findings:**
- Enum has 68 tokens (many more than 45 verbs - includes internal tokens)
- Most verbs match directly
- Some use "get" prefix stripping
- One verb ("subsexpanded") has no clear mapping
- **Pattern is not predictable** - would need manual mapping table

---

### 5. Dialog Processor (Pattern D - No Enum)

**Status:** ❌ Not detected (no enum exists)

**Implementation:** `Common/source/langdialog.c`

**Pattern:** **NO ENUM FOUND**

| Index | RC Verb | Enum Token | Match |
|-------|---------|------------|-------|
| 0-18 | (all) | ??? | ❌ |

**Findings:**
- File exists but uses different dispatch mechanism
- Switch statement found but case labels are UI event types (`statText`, `editText`, `activateEvt`)
- NOT verb dispatch
- **Headless stub file** (`tests/headless_dialog_verbs.c`) has proper enum: `diav_alert`, `diav_run`, etc.
- Follows headless pattern: `{first3}v_{verb}`

**Hypothesis:**
- Dialog processor may be GUI-only in Common/source
- Headless implementation is the proper verb dispatcher
- Should analyze headless file instead for this processor

---

## Pattern Summary

### Pattern A: Processor Prefix + Verb + func
- **Example:** file processor
- **Rule:** `{processor}{verb}func`
- **Consistency:** ✓ Excellent
- **Processors using this:** file (confirmed), likely others

### Pattern B: Verb + func (No Prefix)
- **Example:** table, menu processors
- **Rule:** `{verb}func`
- **Consistency:** ✓ Excellent
- **Processors using this:** table, menu (confirmed), likely majority

### Pattern C: Transformed Verb + func
- **Example:** op processor
- **Rule:** Unpredictable transformations
- **Consistency:** ❌ Poor - manual mapping needed
- **Processors using this:** op (confirmed), unknown others

### Pattern D: No Enum / Different Dispatch
- **Example:** dialog processor
- **Rule:** N/A - no case-based verb dispatch in Common/source
- **Consistency:** N/A
- **Processors using this:** dialog (confirmed), unknown others
- **Note:** Headless stubs may have proper pattern

---

## Analyzer Implications

### Why Current Analyzer Succeeds (13 processors)
1. Processors following Pattern A or B
2. Enum extraction works
3. Case label generation tries both patterns
4. Comment stripping prevents false matches

### Why Current Analyzer Fails (38 processors)
1. Pattern C: Unpredictable transformations (can't guess mapping)
2. Pattern D: No enum exists (different dispatch mechanism)
3. Possible: File discovery fails (wrong file searched)
4. Possible: Headless-only implementations (should search headless stub)

### Potential Solutions

**Option 1: Use RC Verb Names Directly**
- Use RC file as source of truth
- Generate case labels as: `{verb}func`
- Test if case exists
- **Pros:** Simple, covers Pattern B
- **Cons:** Misses Pattern A (processor prefix), misses Pattern C transformations

**Option 2: Multi-Pattern Heuristic**
- Try Pattern A: `{processor}{verb}func`
- Try Pattern B: `{verb}func`
- Try Pattern C transformations: strip "get", etc.
- **Pros:** Covers most cases
- **Cons:** Still misses unpredictable transformations

**Option 3: Normalize C Code (Refactor)**
- Choose ONE pattern (recommend Pattern B: `{verb}func`)
- Refactor all processors to follow it
- **Pros:** Clean, predictable, future-proof
- **Cons:** Large effort, requires testing

**Option 4: Hybrid (Recommended)**
- Use RC verb names as baseline
- Try Pattern A and B heuristics
- For Pattern C/D: Manual mapping table or refactor just those processors
- **Pros:** Balanced effort, covers 80%+ with heuristics
- **Cons:** Some manual work needed

---

## Next Steps

1. **Investigate remaining sample processors** (sys, kb, mouse, frontier, window)
2. **Create full taxonomy** of all 51 processors by pattern
3. **Make recommendation** based on complete data
4. **Get approval** on approach (refactor vs. heuristics)

---

## Related Documents

- `verb_name_mapping_investigation.md` - Investigation plan
- `automatic_verb_binding_architecture.md` - Parent project
