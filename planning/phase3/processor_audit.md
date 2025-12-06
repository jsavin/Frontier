# Phase 3 Stage 1: Processor Audit

**Objective:** Comprehensive categorization of all 37 stub processors to establish implementation priorities and resource requirements.

**Status:** Template/Skeleton for Review

**Last Updated:** 2025-12-05

---

## Executive Summary

| Category                         | Count  | Verbs    | Priority    | Headless Potential  |
| -------------------------------- | ------ | -------- | ----------- | ------------------- |
| ✅ Core Functionality             | TBD    | TBD      | HIGH        | ✅ Full              |
| ⚠️ Partial (Some verbs work)     | TBD    | TBD      | HIGH        | ✅ Partial           |
| 📱 GUI-Dependent (Headless-Safe) | TBD    | TBD      | MEDIUM-HIGH | ✅ Stdio Alternative |
| 🖥️ GUI-Dependent (UI-Only)      | TBD    | TBD      | LOW         | ❌ Not feasible      |
| ⚠️ Platform-Specific (Required)   | TBD    | TBD      | MEDIUM      | ⚠️ Per-OS code      |
| ⚠️ Platform-Specific (Optional)   | TBD    | TBD      | LOW         | ⚠️ If available      |
| **TOTAL**                        | **37** | **~407** | —           | —                   |

**Key Insights:**
1. GUI-Dependent verbs that provide simple dialogs, file pickers, and input prompts can often be implemented using stdio alternatives, making them feasible for headless mode and daemon scenarios.
2. **No external service dependencies found.** All 37 processors can be self-contained or gracefully degrade.
3. Platform-Specific (Optional) processors can gracefully fail if their platform/runtime is unavailable (e.g., Python, Windows DLLs, macOS OSA).
4. Obsolete processors (rez - resource forks) are excluded from headless scope.

---

## How to Use This Document

### For Each Processor Section:

1. **Basic Info**
   - Processor name and EFP ID
   - Verb count
   - Documentation link

2. **Status Assessment**
   - Category (Core, Partial, GUI-Only, Platform-Specific, External)
   - Why it fits this category
   - Which specific verbs are blockers (if applicable)

3. **Implementation Analysis**
   - Complexity estimate: LOW / MEDIUM / HIGH
   - Dependencies (other processors, external services, OS calls)
   - Known issues or limitations
   - Estimated implementation effort

4. **UserTalk Documentation**
   - Link to docserver documentation
   - Key patterns observed
   - Special considerations

5. **Implementation References**
   - Link to similar completed processor (for patterns)
   - Legacy source reference (if available)
   - Test requirements

6. **Priority & Sequencing**
   - Implementation priority (QUICK WIN / HIGH IMPACT / MEDIUM / LOW)
   - Recommended implementation order
   - Blockers that must be resolved first

---

## Processor Audit Template

### Processor: `[processor_name]`

**EFP ID:** `[ID]` | **Verb Count:** `[N]` | **Status:** [✅/⚠️/❌]

**Documentation:** [`processor`](docs/usertalk/docserver.userland.com/processor/index.html)

---

#### Category Assessment

**Category:** [Core Functionality / Partial / GUI-Dependent (Headless-Safe) / GUI-Dependent (UI-Only) / Platform-Specific / External Service]

**Rationale:**
[Explain why this processor falls into this category. What makes it implementable or not?]

**Headless Compatibility:**
- **Headless-Safe:** Can be implemented with stdio alternatives (dialogs, file pickers, etc.)
- **UI-Only:** Requires window context that can't be replicated (window management, rendering, menus)
- **Partial GUI:** Some verbs Headless-Safe, others UI-Only

**Blocking Verbs (if applicable):**
- [List verbs that prevent headless execution or create dependencies]

**Stdio Implementation Potential (if GUI-Dependent):**
[For GUI-dependent verbs, document if/how they could be implemented using stdio:]
- Simple dialogs (alert, confirm, ask): [YES/NO - if YES, describe approach]
- Input dialogs (getint, getpassword, getstring): [YES/NO - if YES, describe approach]
- File picker dialogs (getfile, putfile, getfolder): [YES/NO - if YES, describe approach]
- Complex dialogs (modal, modeless, custom): [YES/NO - if YES, describe approach]

---

#### Implementation Analysis

**Complexity:** [LOW / MEDIUM / HIGH]

**Dependencies:**
- [ ] Other processors: [list, e.g., "string.*, file.*"]
- [ ] External services: [list, e.g., "HTTP server", "database"]
- [ ] OS-specific functionality: [list, e.g., "file metadata", "system calls"]
- [ ] GUI/window context: [YES / NO]

**Known Issues/Limitations:**
- [List any documented limitations or gotchas from source or docs]

**Estimated Implementation Effort:**
- [hours/days, or "TBD if research needed"]

---

#### UserTalk Documentation

**Key Patterns Observed:**
- [Document any verb naming patterns, parameter conventions, return value patterns]

**Special Considerations:**
- [Character encoding, type coercion, thread-safety, side effects, etc.]

**Verb Grouping:**
- [If verbs have logical groupings, document them]

**Examples from Documentation:**
- [Note any examples that clarify usage patterns]

---

#### Implementation References

**Similar Completed Processor:**
- [`processor_name`](tests/headless_`processor_name`_verbs.c) - [why this is a good pattern reference]

**Legacy Source:**
- `/Users/jake/dev/tedchoward/Frontier` - [specific files if known]

**Test Requirements:**
- Minimum test cases: [estimate, e.g., "3-5 per verb"]
- Special test scenarios: [e.g., "encoding edge cases", "null handling"]

---

#### Priority & Sequencing

**Priority:** [QUICK WIN / HIGH IMPACT / MEDIUM / LOW]

**Recommended Implementation Order:** [N/A if not suitable for Phase 3, or sequence number]

**Blockers/Prerequisites:**
- [List any processors or infrastructure that must be done first]

**Quick Win Potential:**
- [If LOW complexity: what makes this a quick win?]
- [If MEDIUM/HIGH: what's the minimum viable subset to get value?]

---

## Audit Results by Category

### ✅ Core Functionality (Expected: 5-8 processors)

[Processors with simple logic, no GUI, clear headless implementation path]

- [ ] Processor placeholder 1
- [ ] Processor placeholder 2

---

### ⚠️ Partial Implementation (Expected: 5-8 processors)

[Some verbs work, others need GUI or OS-specific handling; some may have stdio alternatives]

- [ ] Processor placeholder 1
- [ ] Processor placeholder 2

---

### 📱 GUI-Dependent (Headless-Safe) (Expected: 4-6 processors)

[Verbs that traditionally require GUI but can be implemented using stdio alternatives]

**Examples:**
- **dialog:** alert, confirm, ask, getint, getpassword, getstring, getuserinfo
- **file:** getfile, putfile, getfolder (file pickers via stdio)
- **window:** some message/status operations (msg, notify)

**Stdio Implementation Strategies:**
- Simple prompts: Use println/readln for alert, confirm, ask
- Input dialogs: Use prompt with echo control for passwords
- File pickers: Use directory listing and user selection
- Notifications: Use stderr or dedicated logging for messages

- [ ] Processor placeholder 1
- [ ] Processor placeholder 2

---

### 🖥️ GUI-Dependent (UI-Only) (Expected: 4-6 processors)

[All or most verbs require window/rendering context; cannot be replicated with stdio]

**Examples:**
- **window:** Management, positioning, focus, rendering operations
- **menu:** Menu bar operations and event handling
- **target:** Window context operations
- **pict:** Picture rendering and manipulation
- **htmlcontrol:** HTML rendering widget
- **statusbar:** Status bar rendering
- **mainwindow:** Main window UI operations
- **filemenu/editmenu:** Menu bar integration

- [ ] Processor placeholder 1
- [ ] Processor placeholder 2

---

### ⚠️ Platform-Specific (Required) (Expected: 5-8 processors)

[Require OS integration (file metadata, system calls, etc.); need per-OS implementation for full functionality]

**Examples:**
- **sys:** OS-specific operations (processes, environment, shell commands)
- **file:** File metadata operations (dates, types, creators, locks)
- **launch:** Application launching (varies by OS)

- [ ] Processor placeholder 1
- [ ] Processor placeholder 2

---

### ⚠️ Platform-Specific (Optional) (Expected: 3-4 processors)

[Optional runtime dependencies; can gracefully degrade if platform/language not available]

**Examples:**
- **python:** Requires Python interpreter (gracefully fail if not available)
- **dll:** Windows DLL calling (Windows-only, can stub on other platforms)
- **osa:** macOS Open Scripting Architecture (macOS-only, can stub on other platforms)

**Handling Strategy:** Return error/unsupported message rather than crashing

- [ ] Processor placeholder 1
- [ ] Processor placeholder 2

---

## Quick Wins Candidates (Priority Candidates for Stage 3)

Processors with LOW complexity, clear implementation path, no GUI dependencies:

| Processor | Verbs | Why It's a Quick Win | Implementation Effort |
|-----------|-------|---------------------|----------------------|
| [name] | [N] | [reason] | [estimate] |
| [name] | [N] | [reason] | [estimate] |

---

## High-Impact Processors (Phase 3 Priority if Feasible)

Processors with MEDIUM complexity but HIGH impact (many scripts depend on them):

| Processor | Verbs | Impact | Dependencies | Challenge |
|-----------|-------|--------|--------------|-----------|
| [name] | [N] | [why high impact] | [blocking issues] | [main challenge] |
| [name] | [N] | [why high impact] | [blocking issues] | [main challenge] |

---

## Implementation Sequencing Recommendations

### Phase 3 Stage 3 (Quick Wins) - Recommended Order

1. [Processor] - [reason]
2. [Processor] - [reason]
3. [Processor] - [reason]

### Phase 3 Stage 4 (Medium Complexity) - Recommended Order

1. [Processor] - [reason, dependencies]
2. [Processor] - [reason, dependencies]

### Phase 3 Stages 5+ (Hard Problems) - Documented as Incompatible

- [List GUI-only and platform-specific processors]
- [Document workarounds or limitations]

---

## Cross-Processor Dependencies

### Dependency Graph

```
[Processor A]
  └─ depends on: [Processor B], [Processor C]

[Processor B]
  └─ depends on: [Processor D]

etc.
```

### Critical Path Analysis

[Identify which processors must be implemented first to unblock others]

---

## Summary Statistics

**By Implementation Status:**
- Total processors: 37
- Core functionality: [count]
- Partial: [count]
- GUI-dependent (Headless-Safe): [count]
- GUI-dependent (UI-Only): [count]
- Platform-specific (Required): [count]
- Platform-specific (Optional): [count]
- ~~External service required:~~ **0 (none found)**

**Headless Implementation Potential:**
- Fully implementable in headless: [count] ([~N] verbs)
- Partially implementable (with stdio alternatives): [count] ([~N] verbs)
- Requires platform/runtime availability: [count] ([~N] verbs)
- Requires GUI context (truly UI-only): [count] ([~N] verbs)
- Obsolete/not applicable: [count] ([~N] verbs)

**By Complexity:**
- Low: [count] ([~N] verbs)
- Medium: [count] ([~N] verbs)
- High: [count] ([~N] verbs)

**By Priority:**
- Quick Win candidates: [count]
- High impact (medium effort): [count]
- Medium priority: [count]
- Low priority: [count]

---

## Notes & Observations

- [General patterns observed across processors]
- [Common blockers or dependencies]
- [Recommendations for implementation strategy]
- [Any surprises or findings from the audit]

---

## Appendix: Reference Links

**UserTalk Documentation:**
- Main index: `docs/usertalk/docserver.userland.com/`
- Alphabetical: `docs/usertalk/docserver.userland.com/alphabeticalIndex.html`

**Implementation References:**
- Completed processors: `tests/headless_*_verbs.c`
- Verb definitions: `Common/resources/Win32/kernelverbs.rc`
- Runtime architecture: `docs/Frontier - The Definitive Guide - by Matt Neuberg/`

**Codebase Locations:**
- Verb implementations: `Common/source/lang*.c`, `Common/source/*verbs.c`
- Legacy source: `/Users/jake/dev/tedchoward/Frontier`
- Tests: `tests/unit/test_*_verbs.c`

---

**Next Step:** Populate this template by reviewing each of the 37 stub processors and filling in the audit data for each.
