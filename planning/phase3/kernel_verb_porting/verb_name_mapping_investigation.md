# Verb Name Mapping Investigation

**Created:** 2025-12-14
**Status:** Active Investigation
**Parent Project:** Automatic Verb Binding Architecture
**Goal:** Establish reliable mapping between RC verb names and C implementation function names

---

## Problem Statement

The automatic verb binding analyzer can detect ~13 of 51 processors (26%). The primary blocker is inconsistent naming patterns between:

1. **RC file verb definitions** (canonical source of truth)
2. **C enum token names** (used in switch/case statements)
3. **Actual C implementation function names**

### Examples of Inconsistency

**Table Processor:**
- RC: `"move\0"`
- C enum: `movefunc`
- Pattern: Simple `{verb}func` suffix

**Op Processor:**
- RC: `"getlinetext\0"` → C enum: `linetextfunc` (dropped "get")
- RC: `"level\0"` → C enum: `levelfunc` (no transformation)
- RC: `"subsexpanded\0"` → C enum: `getexpandedfunc` (added "get", changed "subsexpanded" to "expanded")

**File Processor:**
- RC: `"created\0"` → C enum: `filecreatedfunc` (added processor prefix)
- Uses processor-prefixed pattern unlike most others

**Dialog Processor:**
- Has `langdialog.c` implementation file
- No enum found in file (uses different pattern entirely?)

---

## Current State

### What Works (13 processors detected)
- clock, crypt, db, file, html, lang, math, mysql, op, sqlite, string, table, xml
- 295 of 707 verbs detected (41%)
- These processors follow patterns the analyzer can recognize

### What Doesn't Work (38 processors undetected)
- base64, bit, clipboard, date, dialog, dll, editmenu, filemenu, frontier, htmlcontrol, inetd, kb, launch, mainwindow, menu, mouse, mrcalendar, opattributes, osa, pict, point, python, re, rectangle, rez, rgb, script, search, searchengine, semaphore, speaker, statusbar, sys, target, tcp, thread, webserver, window

Reasons for failure:
- No enum found in implementation file
- Enum exists but uses unpredictable naming
- Only headless stub implementations exist
- File discovery fails

---

## Investigation Plan

### Phase 1: Data Gathering (Current Phase)

**Goal:** Understand the actual patterns across all processors

#### Step 1.1: Extract All Verb Names from RC File ✅
- Update `parse_kernelverbs.py` to parse verb name strings
- Return list of (processor, verb_name, token_index) tuples
- **Status:** Ready to implement

#### Step 1.2: Sample 5-10 Processors Across Different Categories
**Categories:**
1. **Working processors** (1-2): file, table
2. **Failing with Common/source** (2-3): dialog, menu, sys
3. **Headless-only** (2-3): kb, mouse, point
4. **Mixed/unclear** (1-2): frontier, window

**For each processor:**
- Document RC verb names
- Document actual C file structure (enum vs. other)
- Document actual case labels or function names
- Create mapping table showing transformation rules

#### Step 1.3: Categorize All 51 Processors
Create taxonomy:
- **Pattern A:** Processor prefix + verb + func (e.g., `filecreatedfunc`)
- **Pattern B:** Verb + func (e.g., `movefunc`)
- **Pattern C:** Transformed verb + func (e.g., `linetextfunc` from "getlinetext")
- **Pattern D:** No enum (different dispatch mechanism)
- **Pattern E:** Headless stub only

**Deliverable:** `verb_mapping_taxonomy.md` with all 51 processors categorized

---

### Phase 2: Solution Design

**Goal:** Decide on approach for each category

#### Option A: Normalize C Code (Refactor)
- Establish ONE canonical pattern (probably Pattern B: `{verb}func`)
- Rename all enum tokens to follow pattern
- Rename all case labels to match
- **Pros:** Clean, consistent, future-proof
- **Cons:** Large refactoring effort, risk of breaking things

#### Option B: Build Mapping Heuristics
- Use RC verb name as source
- Try multiple transformation rules to find case label
- **Pros:** No C code changes needed
- **Cons:** Complex, fragile, may still miss edge cases

#### Option C: Hybrid Approach
- Normalize the "easy" processors (patterns A/B/C)
- Build heuristics for edge cases
- Document which processors use which pattern
- **Pros:** Balanced effort vs. consistency
- **Cons:** Still leaves some inconsistency

#### Step 2.1: Recommendation Based on Data
After Phase 1 data gathering, recommend specific approach

---

### Phase 3: Implementation

**TBD based on Phase 2 decision**

Possible tasks:
- Update RC parser to extract verb names
- Build transformation heuristics
- Refactor C enums/case labels
- Update analyzer to use RC verb names
- Add verification tests

---

## Questions to Answer

### Immediate Questions
1. ✅ Can we parse verb names from RC file reliably?
2. What percentage of processors use each pattern?
3. Are there processors with NO case-based dispatch at all?
4. Do headless stub files follow consistent patterns?

### Design Questions
1. Should we prioritize C code consistency or analyzer flexibility?
2. Is it worth refactoring legacy C code for this?
3. Can we build a reliable heuristic without refactoring?
4. Should different processor types use different patterns?

### Long-term Questions
1. How do we prevent drift in the future?
2. Should new processors follow a strict naming convention?
3. Can we enforce patterns with CI checks?

---

## Working Notes

### 2025-12-14: Initial Investigation

**Discovered patterns:**
- File processor: Has enum with processor prefix (`filecreatedfunc`)
- Table processor: Has enum without prefix (`movefunc`)
- Op processor: Has enum with unpredictable transformations (`getlinetext` → `linetextfunc`)
- Dialog processor: No enum found in `langdialog.c`

**Current analyzer approach:**
- Strategy 1: Try processor-prefixed patterns
- Strategy 2: Try all `*func` tokens in enum
- Problem: Doesn't work when there's no enum or when RC names don't match enum names

**Next step:** Parse RC file to get canonical verb names, then compare against what's actually in C files

---

## Success Criteria

### Minimum Success
- Detect 40+ processors (80% coverage)
- Understand why remaining processors fail
- Have clear path to 100% coverage

### Ideal Success
- Detect all 51 processors automatically
- C code follows consistent naming pattern
- Zero manual whitelist maintenance needed

---

## Related Documents

- `automatic_verb_binding_architecture.md` - Parent architecture
- `automatic_verb_binding_implementation.md` - Implementation checklist
- `automatic_verb_binding_project_plan.md` - Project plan

---

## Next Steps

**Immediate (Today):**
1. Update `parse_kernelverbs.py` to extract verb names from RC file
2. Pick 5 sample processors to investigate deeply
3. Document actual C patterns for those 5 processors
4. Create initial taxonomy

**This Week:**
1. Complete Phase 1 data gathering
2. Make Phase 2 recommendation
3. Get your approval on approach
4. Start Phase 3 implementation
