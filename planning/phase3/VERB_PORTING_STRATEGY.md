# Verb Porting Strategy - Action Items

**Based on:** Comprehensive global state dependency analysis (VERB_PORTING_GLOBAL_STATE_ANALYSIS.md)
**Decision Date:** 2025-12-25
**Status:** Ready for implementation

---

## Executive Decision

**PROCEED with verb porting now. Issue #86 is NOT a blocker for most verbs.**

- **343 of 403 Priority verbs (85%) are architecture-ready**
- **Only 49 verbs (12%) blocked by Issue #135 outline context work**
- **Remaining 11 verbs (3%) are GUI-only, can be safely stubbed**

---

## Immediate Action Plan (Week 1-2)

### Batch 1: Zero-Dependency Verbs (218 verbs)

These require NO architectural work - just implementation/binding.

| Family | Count | Status | Action |
|--------|-------|--------|--------|
| string | 60 | 100% implemented | Enable in headless (no changes needed) |
| file | 86 | 96% implemented | Port file dialog stubs (3 verbs) |
| db | 13 | 100% implemented | Already working (verify in headless) |
| xml | 14 | 92% implemented | Port missing 1 verb |
| math | 3 | 100% implemented | Enable in headless |
| crypt | 5 | 100% implemented | Enable in headless |
| kb | 4 | 100% implemented | Enable in headless |
| regex | 10 | Code exists, 0% bound | Add verb bindings (code ready) |
| frontier | 14 | 100% implemented | Enable in headless |
| **TOTAL** | **218** | | **~3-4 day effort** |

**Quick Win**: Most of these verbs likely already work in headless - just need to verify and enable them.

---

### Batch 2: Parameter-Based Context Verbs (115 verbs)

These have minimal global dependencies and can be completed with simple context isolation.

| Family | Count | Status | Action | Effort |
|--------|-------|--------|--------|--------|
| lang | 45 | 77% implemented | Port pure computation verbs (45) | 2 days |
| date | 26 | 86% implemented | Complete 4 stubbed verbs | 1 day |
| clock | 6 | 85% implemented | Complete 1 stubbed verb | 0.5 days |
| table | 10 | 55% implemented | Complete 8 stubbed verbs | 2 days |
| sys | 15 | 93% implemented | Port 15 verified verbs | 1 day |
| html | 22 | 95% implemented | Port 22 verbs | 1 day |
| dialog | 14 | 73% implemented | Port 14 implemented verbs | 2 days |
| **TOTAL** | **138** | | | **~9-10 days** |

**Note:** These verbs may have GUI components (dialogs, window display), but the core functionality is parameter-based and can be isolated with guards.

---

## Medium-term Action Plan (Week 3-4)

### Batch 3: Context-Isolated Verbs (20 verbs)

These require explicit context parameterization but are low-risk.

| Family | Count | Issue | Action |
|--------|-------|-------|--------|
| lang | 3 | Target context | Make target explicit parameter or guard with #ifndef FRONTIER_HEADLESS |
| menu | 10 | Menu structure (not outline) | Port 10 verbs that don't depend on outline context |
| table | 0 | | (Already counted in Batch 2) |
| **TOTAL** | **13** | | ~2-3 days |

---

### Batch 4: Explicitly Stub GUI Verbs (74 verbs)

These don't make sense in headless - stub them safely (return nil or empty, no errors).

| Family | Count | GUI Component | Stub Behavior |
|--------|-------|---------------|---------------|
| window | 31 | Window management | Return nil for queries, no-op for actions |
| dialog | 5 | Modeless dialogs | Return default/nil values |
| file | 3 | File picker dialogs | Raise error: "File picker not available in headless mode" |
| pict | 4 | Graphics objects | Return empty/default graphics |
| menu | 4 | Menu display/editing | Return nil/empty (structure operations should be in Batch 3) |
| target | 3 | Target window selection | Return nil in headless |
| clipboard | 2 | Clipboard I/O | Stub (raise error or return nil) |
| statusbar | 5 | Status display | No-op |
| mainwindow | 7 | App window | Stub |
| Others | 10 | DLL, OSA, LAUNCH, PYTHON, INETD, etc. | Context-specific stubs |
| **TOTAL** | **74** | | ~1-2 days documentation |

---

## BLOCKED UNTIL ISSUE #135 (49 verbs)

### OP Verbs (45 verbs) - Core Blocker

**Blocked by:** Issue #135 (outline context refactoring)

**Why:** These verbs use `oppushoutline`/`oppopoutline` pattern (stack-based global state).

**Files Affected:**
- opverbs.c (25 calls)
- opxml.c (21 calls)
- opops.c (15 calls)
- menupack.c (14 calls)
- And 19 other files

**Unfixable without refactoring:**
- Cannot make thread-safe until stack-based globals become reference-counted
- Cannot support concurrent outline editing (Frontier 2.0 vision)
- Porting now would introduce technical debt

**Action:** Wait for Issue #135 completion (estimated 1-2 weeks after Batch 1-2 complete).

### Menu Outline Operations (4 verbs)

**Dependent on:** OP verb architecture

**Action:** Port in same wave as OP verbs after Issue #135.

---

## Success Metrics

### By End of Week 1-2 (Batch 1+2 Complete)

✅ **343 verbs ported or actively working in headless**
✅ **Verb coverage improves from 68% → 95%+**
✅ **CLI runtime gains significant capability** (string ops, file I/O, XML, regex, math, date/time, basic table/db operations)
✅ **No architectural debt introduced**
✅ **Clear documentation of headless limitations** (OP verbs, GUI verbs)

### By End of Week 3-4 (Batch 3+4 Complete)

✅ **All non-outline verbs complete**
✅ **GUI-only verbs documented and safely stubbed**
✅ **Ready to block on Issue #135 for final 49 verbs**

### By Week 5 (After Issue #135)

✅ **All 707 verbs accounted for in some form**
✅ **Headless Frontier runtime fully operational**
✅ **Outline context work enables Frontier 2.0 collaborative architecture**

---

## Implementation Order (Recommended)

### Phase 1 (Immediate): Zero-Risk, High-Impact

1. **Enable existing verbs in headless** (STRING, FILE, DB, XML, etc.)
   - These are already implemented and just need headless enabling
   - ~1-2 days, pure gain
   - Gives immediate CLI capability boost

2. **Port REGEX verbs** (10 verbs)
   - Implementation exists in langregexp.c
   - Just needs verb binding
   - ~2-3 hours

3. **Complete DATE/CLOCK/MATH stubs** (7 verbs)
   - Last few missing verbs
   - ~1 day

### Phase 2 (Days 3-5): Core Language Runtime

4. **Port LANG pure computation verbs** (45 verbs)
   - Already 77% done
   - Low risk
   - ~2 days

5. **Port TABLE structure verbs** (10 verbs)
   - Parameter-based design
   - ~2 days

### Phase 3 (Days 6-10): Extended Coverage

6. **Port SYS, HTML, DIALOG verbs** (51 verbs)
   - High implementation percentage (73-95%)
   - ~3 days

7. **Complete context isolation for LANG/TABLE target verbs** (13 verbs)
   - Make target explicit
   - Guard GUI-only code
   - ~2 days

### Phase 4 (Days 11-14): Cleanup & Documentation

8. **Create stubs for GUI-only verbs** (74 verbs)
   - Document headless limitations
   - Provide clear error messages
   - ~2 days

9. **Create comprehensive verb coverage documentation**
   - List of what works in headless
   - List of what's stubbed and why
   - List of what's blocked by Issue #135
   - ~1 day

10. **Prepare for Issue #135 work**
    - Document outline context requirements
    - Create outline context structs
    - Plan reference counting approach
    - ~3 days (overlap with Phase 4)

---

## Risk Assessment

### Low Risk (Batch 1)

| Risk | Mitigation |
|------|-----------|
| Verb signatures change | Verify kernel verb interface compatibility |
| Existing headless tests break | Run headless test flow before/after each change |
| Missing verb implementations | Check verb binding analyzer coverage |

### Medium Risk (Batch 2)

| Risk | Mitigation |
|------|-----------|
| Context isolation incomplete | Add explicit parameter tests |
| GUI code leaking to headless | Use #ifndef FRONTIER_HEADLESS guards |
| Performance regression | Profile before/after |

### Low Risk (Batch 3-4)

| Risk | Mitigation |
|------|-----------|
| GUI stubs cause errors | Explicit "not available in headless" messages |
| Incomplete verb coverage | Update documentation clearly |

### High Risk (BLOCKED - Issue #135)

| Risk | Why |
|------|-----|
| Porting OP verbs without outline refactor | Technical debt; will need refactoring again for Frontier 2.0 |
| Threading issues if outline stays global | Cannot be made thread-safe in current architecture |
| Collaborative editing blocked | Requires reference counting first |

---

## Dependency Chain

```
[Week 1-2: Batch 1 (218 verbs)]
    ↓
[Week 1-2: Batch 2 (115 verbs)]
    ↓
[Week 3-4: Batch 3 (13 verbs)]
    ↓
[Week 3-4: Batch 4 (74 verbs - stubs)]
    ↓ (parallel with above)
[BLOCKED: Issue #135 (outline context refactoring)]
    ↓
[Week 5: Batch 5 (49 outline verbs after #135 complete)]
    ↓
[Complete: 707 verbs fully ported or documented]
```

---

## Decision Checklist

Before starting each batch, verify:

- [ ] **Batch 1:** Existing implementations verified working in headless
- [ ] **Batch 1:** Verb bindings complete for all 218 verbs
- [ ] **Batch 2:** Parameter-based design verified for all 115 verbs
- [ ] **Batch 2:** No new implicit globals added
- [ ] **Batch 3:** Target/window context made explicit
- [ ] **Batch 3:** All GUI code guarded with `#ifndef FRONTIER_HEADLESS`
- [ ] **Batch 4:** GUI stubs documented clearly
- [ ] **Batch 4:** User-facing error messages follow "Can't do X because Y" format
- [ ] **Issue #135:** Outline context struct designed and approved
- [ ] **Issue #135:** Reference counting approach documented
- [ ] **Batch 5:** Ready to port 49 outline verbs after #135 complete

---

## Questions Resolved by This Analysis

### "Does Issue #86 block verb porting?"

**No.** 85% of verbs are ready now. Only outline-related verbs (12%) are blocked, and that's by Issue #135 specifically, not the broader Issue #86.

### "How much work is left before we can run a real UserTalk script headless?"

**~2-3 weeks for core capability.** After Batch 1-2, most language operations will work. After Batch 3-4, nearly everything works except outline editing.

### "What's the minimum set of verbs we need for basic CLI support?"

**Batch 1 + half of Batch 2** (~280 verbs):
- Language runtime (math, type checking, control flow)
- String manipulation
- Basic file I/O
- Database access
- XML/regex
- System utilities

This gets us "hello world" + file operations + DB queries.

### "Can we start porting without Issue #135 complete?"

**Yes.** Start Batches 1-4 immediately. Issue #135 only blocks 49 verbs (OP + menu outline operations). Do parallel work on Issue #135 so it's ready when Batch 4 finishes.

### "Which verbs will still be missing after all porting?"

**74 GUI-only verbs** - window management, dialogs, clipboard, status bar, etc. These will be safely stubbed with clear error messages.

---

## Next Step

1. **Read:** `VERB_PORTING_GLOBAL_STATE_ANALYSIS.md` (full details)
2. **Review:** This strategy document with team
3. **Approve:** Decision to proceed with Batches 1-2 immediately
4. **Start:** Batch 1 verb enablement (low risk, high reward)
5. **Parallel:** Issue #135 outline context refactoring design work

---

**Prepared by:** Claude (Haiku 4.5)
**Analysis Scope:** 707 Frontier kernel verbs across 51 processors
**Code References:** Common/source/*.c (15 verb implementation files), planning docs
