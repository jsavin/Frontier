# Verb Porting Analysis - Index & Quick Reference

**Date:** 2025-12-25
**Status:** Complete - Ready for review and decisions
**Analysis Coverage:** 707 Frontier kernel verbs across 51 processors

---

## Quick Answer: Is Issue #86 a Blocker?

**Short Answer:** NO

- **343 of 403 Priority verbs (85%) are architecture-ready NOW**
- **Only 49 verbs (12%) blocked by Issue #135 outline context**
- **Start verb porting this week - no architectural work needed for first 333 verbs**

---

## Documents in This Analysis

### 1. VERB_PORTING_GLOBAL_STATE_ANALYSIS.md (DETAILED RESEARCH)

**What it contains:**
- Comprehensive analysis of all 51 verb processors
- Global state dependency mapping
- Red flags (what blocks work) vs. Green flags (what's ready)
- File-by-file breakdown of 23 files using outline context
- Architectural patterns guide (good vs. bad patterns)
- Test coverage recommendations
- Complete verb family status tables

**When to read it:**
- Need detailed justification for decisions
- Want to understand architecture
- Planning long-term maintenance strategy
- Reviewing technical decisions

**Key sections:**
- Priority 1-3 Analysis (pages 8-35)
- Global State Dependency Map (pages 36-39)
- Detailed Verb Family Status (pages 40-47)
- Appendix A: Complete Verb Family Status Table (page 57)

---

### 2. VERB_PORTING_STRATEGY.md (ACTIONABLE PLAN)

**What it contains:**
- 5-batch implementation plan (Weeks 1-5)
- Risk assessment by batch
- Success metrics
- Implementation order (most efficient)
- Dependency chain visualization
- Decision checklist

**When to read it:**
- Ready to start implementation
- Need to plan development schedule
- Want timeline for delivery
- Planning resource allocation

**Quick reference sections:**
- Immediate Action Plan (Week 1-2): 218 + 115 verbs
- Medium-term Plan (Week 3-4): 13 + 74 verbs + Issue #135 prep
- Success Metrics (what we'll have accomplished)
- Implementation Order (step-by-step)

---

### 3. VERB_PORTING_INDEX.md (THIS DOCUMENT)

**Purpose:** Navigation and quick reference

---

## Quick Facts

### Verb Inventory

| Category | Count | Status | Effort | Timeline |
|----------|-------|--------|--------|----------|
| **Ready Now** | 218 | Zero dependencies | ~3-4 days | Week 1 |
| **Ready Soon** | 115 | Parameter-based | ~7-9 days | Week 1-2 |
| **Need Context Work** | 13 | Target context | ~2-3 days | Week 3 |
| **Stub for GUI** | 74 | GUI-only | ~1-2 days | Week 3-4 |
| **Blocked by #135** | 49 | Outline context | TBD | After Week 4 |
| **Unknown/Other** | 238 | TBD | TBD | TBD |
| **TOTAL** | **707** | | | |

### Top 5 Verb Families (by size)

1. **FILE (86 verbs)** - 96% implemented, zero global deps → **Port immediately**
2. **STRING (60 verbs)** - 100% implemented, pure functions → **Port immediately**
3. **LANG (58 verbs)** - 77% implemented, mostly pure → **Port Week 1-2**
4. **TABLE (18 verbs)** - 55% implemented, parameter-based → **Port Week 2**
5. **XML (14 verbs)** - 92% implemented, zero global deps → **Port immediately**

### Key Blockers

1. **Issue #135: Outline Context (49 verbs blocked)**
   - Caused by: `oppushoutline`/`oppopoutline` global pattern
   - Files affected: 23 files with 200+ uses
   - Required for: Thread-safe outline editing, Frontier 2.0
   - Timeline: ~2-3 weeks (after Batch 4)

2. **GUI-Only Verbs (74 verbs, safe to stub)**
   - Window management (31)
   - Graphics (4)
   - Dialogs (5)
   - Other (34)
   - Safe to stub: Return nil or empty, no errors

---

## By the Numbers

### Global State Dependencies Found

| Type | Count | Impact | Fix |
|------|-------|--------|-----|
| Outline context (global) | 200+ uses in 23 files | Prevents reentrancy | Issue #135 |
| Mode stack | 5 remaining calls | Mostly fixed | Mostly complete |
| Target/window (implicit) | 5-10 verbs | Isolated, guarded | Make explicit |
| File I/O (parameter-based) | 110+ refs | No global state | Already fixed |

### Verb Implementation Status

| Status | Count | % |
|--------|-------|---|
| Implemented | 486 | 68% |
| Stubbed | 221 | 31% |
| GUI-only | 74 | 10% |
| Blocked by #135 | 49 | 7% |

---

## Decision Matrix

### "Should I port these verbs NOW or WAIT?"

| Verb Family | Answer | Why |
|-------------|--------|-----|
| STRING | Port NOW | Zero dependencies |
| FILE | Port NOW | Parameter-based I/O |
| DB | Port NOW | Explicit context |
| REGEX | Port NOW | Code exists |
| MATH, CRYPT, KB | Port NOW | Pure functions |
| XML | Port NOW | Parameter-based |
| LANG (45 of 58) | Port NOW | Pure computation |
| TABLE (10 of 18) | Port NOW | Parameter-based |
| DATE, CLOCK | Port NOW | Stateless |
| LANG (3 verbs) | Port Week 3 | Make target explicit |
| MENU (10 of 14) | Port NOW | Non-outline ops |
| MENU (4 verbs) | WAIT #135 | Outline operations |
| **OP (45 verbs)** | **WAIT #135** | **Global outline state** |
| WINDOW | STUB | GUI-only, no point |
| PICT | STUB | GUI-only, no point |
| DIALOG (GUI subset) | STUB | GUI-only, no point |

---

## Implementation Roadmap

```
WEEK 1:
  Day 1-2: Batch 1 - Enable existing verbs (218 verbs)
           STRING, FILE, DB, XML, REGEX, MATH, CRYPT, KB, FRONTIER
           → Verb coverage 68% → 80%

  Day 3-4: Batch 2 Part 1 - Port pure LANG verbs (45 verbs)
           → Verb coverage 80% → 85%

WEEK 2:
  Day 5-7: Batch 2 Part 2 - Port TABLE, DATE, CLOCK, SYS, HTML (70 verbs)
           → Verb coverage 85% → 92%

  Day 8-9: Batch 2 Complete - Port DIALOG (14 verbs), remaining stubs
           → Verb coverage 92% → 95%

WEEK 3:
  Day 10-11: Batch 3 - Context isolation for LANG/TABLE target (13 verbs)
             → Verb coverage 95% → 95.5%

  Day 12: Batch 4 - Create stubs for GUI verbs (74 verbs)
          → Verb coverage 95.5% → 96%

  Day 13-14: Batch 4 - Documentation and prep for Issue #135
             → All non-outline work complete

WEEKS 4-5:
  Parallel: Issue #135 outline context refactoring
  + Batch 5: Port OP verbs (49 verbs) after #135 complete
  → Verb coverage 96% → 100%
```

---

## Key Decisions You Need to Make

### Decision 1: Proceed with Batch 1-4 Now?

**Recommendation:** YES

- Low risk (existing, proven code)
- High reward (coverage 68% → 96%)
- No architectural blockers
- Can be done in 3 weeks

**Cost of delay:** 3 weeks of verb functionality lost

### Decision 2: How to Handle Issue #135?

**Recommendation:** Start Issue #135 work in parallel with Batch 3-4

- Don't port OP verbs until outline context refactored
- Avoid double-refactoring work
- Enables Frontier 2.0 collaboration features
- Needed for thread safety

**Cost of starting Issue #135 early:** 2-3 weeks of architectural work now, saves 2-3 weeks later

### Decision 3: Which GUI Verbs to Stub vs. Defer?

**Recommendation:** Stub all GUI verbs (WINDOW, PICT, DIALOG display, etc.)

- Clear error messages: "Not available in headless mode"
- Don't error silently (fail fast)
- Document clearly in release notes
- Can add real GUI support later if needed

**Cost:** ~1-2 days for comprehensive stubbing + documentation

---

## Files Modified During Analysis

### Planning Documents Created

1. **VERB_PORTING_GLOBAL_STATE_ANALYSIS.md** (14,000 words)
   - Comprehensive research document
   - All verb families analyzed
   - Global state pattern mapping
   - Architectural guidance

2. **VERB_PORTING_STRATEGY.md** (5,000 words)
   - Actionable implementation plan
   - 5-batch schedule
   - Risk assessment
   - Timeline and effort estimates

3. **VERB_PORTING_INDEX.md** (this file)
   - Quick reference
   - Navigation guide
   - Decision matrix

### No Code Files Modified

This was pure research. No production code changed.

---

## How to Use These Documents

### For the CTO (That's You!)

1. Read VERB_PORTING_INDEX.md (this file) - 10 minutes
2. Review VERB_PORTING_STRATEGY.md - 20 minutes
3. Skim red/green flags section in ANALYSIS.md - 10 minutes
4. Make Decision 1-3 above
5. Share STRATEGY.md with team for implementation planning

**Total time: ~40 minutes**

### For Development Team

1. Start with VERB_PORTING_STRATEGY.md (actionable plan)
2. Reference specific sections of ANALYSIS.md as needed for architecture
3. Check DECISION MATRIX for each verb family before porting
4. Follow architectural patterns section for new code

### For Architecture Reviews

1. Read red flags vs. green flags in ANALYSIS.md
2. Review architectural patterns section
3. Check Frontier 2.0 vision context (CLAUDE.md + ADR docs)
4. Validate that new code follows parameter-based patterns

---

## Related Documents to Review

Before starting implementation, read:

1. **CLAUDE.md** (Project instructions)
   - Verb porting rules
   - Global state isolation goals
   - Frontier 2.0 vision context

2. **planning/phase3/global_state_isolation_plan.md**
   - Overall context strategy
   - DB context progress
   - Outline context requirements

3. **planning/architectural_decision_records/ADR-002-context-based-format-versioning.md**
   - How to use explicit context
   - Patterns that work

4. **_CURRENT_STATUS.md**
   - Latest progress on Issue #86
   - Mode stack refactor status
   - Next steps in pipeline

---

## FAQ: Answering Common Questions

**Q: Should I port the OP verbs (45) now?**
A: No. Wait until Issue #135 (outline context) is complete. Otherwise you'll refactor them twice.

**Q: Can I start without completing Issue #86?**
A: Yes! 85% of verbs don't depend on Issue #86. Start with Batch 1 now.

**Q: What about thread safety?**
A: Batch 1-4 achieve thread safety through parameter-based design. Batch 5 (after #135) enables concurrent outline editing.

**Q: How many verbs will still be stubbed after everything?**
A: ~74 GUI-only verbs (window, graphics, etc.) - this is expected and OK for headless mode.

**Q: What's the minimum set for "basic Frontier works"?**
A: 280 verbs (Batch 1 + half of Batch 2): language, strings, file I/O, db, xml, basic table ops.

**Q: Why not just refactor all globals now?**
A: Outline context (Issue #135) requires reference counting design. Better to defer and do it right than rush.

**Q: Can this work on Windows/Linux or just macOS?**
A: This analysis covers verb architecture only. Platform porting is separate work (already in progress).

---

## Next Steps

### Immediate (This Week)

- [ ] Review this analysis
- [ ] Decide: Proceed with Batch 1-4?
- [ ] If yes: Create PR template for verb porting
- [ ] If yes: Assign developer to Batch 1 work

### Short-term (Next 2 Weeks)

- [ ] Execute Batch 1 (218 verbs)
- [ ] Execute Batch 2 (115 verbs)
- [ ] Continuous testing with `./tools/run_headless_tests.sh`

### Medium-term (Weeks 3-4)

- [ ] Execute Batch 3-4 (13 + 74 verbs)
- [ ] Start Issue #135 outline context design
- [ ] PR reviews and integration

### Long-term (After Week 4)

- [ ] Complete Issue #135 outline context work
- [ ] Execute Batch 5 (49 OP verbs)
- [ ] Achieve 100% verb coverage
- [ ] Enable Frontier 2.0 collaboration features

---

## Estimated Timeline Summary

| Phase | Verbs | Effort | Timeline | Risk |
|-------|-------|--------|----------|------|
| Batch 1 | 218 | 3-4 days | Week 1 | Low |
| Batch 2 | 115 | 7-9 days | Week 1-2 | Low-Medium |
| Batch 3 | 13 | 2-3 days | Week 3 | Medium |
| Batch 4 | 74 | 1-2 days | Week 3-4 | Low |
| Issue #135 | - | 7-14 days | Weeks 2-4 (parallel) | High |
| Batch 5 | 49 | 3-5 days | Week 5 (after #135) | Medium |
| **TOTAL** | **469** | **23-38 days** | **5 weeks** | - |

Note: Includes Issue #135 outline context work (prerequisite for Batch 5)

---

## Document Version Info

- **Analysis Date:** 2025-12-25
- **Analyst:** Claude (Haiku 4.5)
- **Status:** Complete and ready for review
- **Next Update:** After Decision 1-3 made and Batch 1 planning complete

---

**For questions or clarification, refer to the full ANALYSIS.md document.**

**For implementation details, refer to STRATEGY.md document.**
