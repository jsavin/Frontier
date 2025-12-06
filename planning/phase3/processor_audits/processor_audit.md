# Phase 3 Stage 1: Processor Audit - Master Index

**Objective:** Comprehensive categorization of all 50 kernel verb processors to establish implementation priorities and resource requirements.

**Status:** In Progress - Individual audits in `processor_audits/` directory

**Last Updated:** 2025-12-05

**Audit Location:** Individual processor audits are in `processor_audits/{processor}.md`

**Master Index:** See `processor_audits/README.md` for complete audit status

---

## Executive Summary

**Total Kernel Verb Processors:** 50
**Total Verbs:** 705

| Category                         | Count  | Verbs    | Priority    | Headless Potential  |
| -------------------------------- | ------ | -------- | ----------- | ------------------- |
| ✅ Core Functionality             | ~15    | ~150     | HIGH        | ✅ Full              |
| ⚠️ Partial (Some verbs work)     | ~10    | ~200     | HIGH        | ✅ Partial           |
| 📱 GUI-Dependent (Headless-Safe) | ~5     | ~50      | MEDIUM-HIGH | ✅ Stdio Alternative |
| 🖥️ GUI-Dependent (UI-Only)      | ~12    | ~150     | LOW         | ❌ Not feasible      |
| ⚠️ Platform-Specific (Required)  | ~5     | ~100     | MEDIUM      | ⚠️ Per-OS code      |
| ⚠️ Platform-Specific (Optional)  | ~3     | ~10      | LOW         | ⚠️ If available     |
| **TOTAL**                        | **50** | **~705** | —           | —                   |

**Key Insights:**
1. **Quick Wins Identified:** 10 processors (24 verbs) with LOW complexity and no dependencies - can be implemented in 20-30 hours total
2. **Core Functionality:** ~15 processors are pure logic with full headless compatibility (bit, math, crypt, re, base64, etc.)
3. **GUI-Safe Stdio Alternatives:** Dialog, file picker verbs can use stdio (printf/scanf) for headless mode
4. **No External Service Dependencies:** All 50 processors can be self-contained or gracefully degrade
5. **Platform-Specific Isolation:** OS-dependent code limited to sys, file (metadata), launch, clipboard, dll, python, osa
6. **Obsolete Processors:** rez (resource forks) is Mac Classic legacy - low priority for modern headless

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

## Audit Progress

**Completed Audits:** 12/50 processors (24%)

### Quick-Win Processors (Tier 1-3) - ✅ Audited (Embedded Below)

These 10 processors have detailed audits embedded in this file (see sections below starting at line 107):
1. rgb (2 verbs) - Core Functionality - LOW
2. point (2 verbs) - Core Functionality - LOW
3. rectangle (2 verbs) - Core Functionality - LOW
4. math (3 verbs) - Core Functionality - LOW
5. semaphore (2 verbs) - Core Functionality - LOW
6. clipboard (2 verbs) - Platform-Specific (Required) - MEDIUM
7. base64 (2 verbs) - Core Functionality - LOW-MEDIUM
8. kb (4 verbs) - Platform-Specific (Optional) - MEDIUM
9. mouse (2 verbs) - Platform-Specific (Optional) - MEDIUM
10. speaker (3 verbs) - Platform-Specific (Optional) - MEDIUM-HIGH

**Total Quick-Win Verbs:** 24 verbs
**Estimated Implementation Time:** 20-30 hours

### Core Functionality Processors - 🔄 In Progress (Individual Files)

Individual audit files created:
- ✅ [bit.md](bit.md) - 8 verbs - LOW complexity - **Ready for implementation** (2-3 hours)
- ✅ [clock.md](clock.md) - 7 verbs - LOW-MEDIUM complexity - **Ready for implementation** (3-4 hours)
- ⏳ date.md - 30 verbs - MEDIUM complexity
- ⏳ crypt.md - 5 verbs - MEDIUM complexity
- ⏳ re.md - 10 verbs - MEDIUM complexity

### Remaining Processors - ⏳ Pending (38 processors)

See [README.md](README.md) for prioritization and next steps.

---

## Original Quick-Win Audits (Lines 68-731)

The following detailed audits remain in this file for reference. Future audits will be individual files in `processor_audits/`.

---

## Quick-Win Processors (Tier 1-3)

### Processor: `rgb`

**EFP ID:** `1005` (lang block 9) | **Verb Count:** `2` | **Status:** ✅

**Documentation:** No dedicated rgb/ directory found in docs

---

#### Category Assessment

**Category:** Core Functionality

**Rationale:**
RGB values are just structured data (red, green, blue components 0-65535). These verbs pack/unpack RGB values into/from a single long integer or structure. Pure data manipulation with no GUI dependency.

**Headless Compatibility:** Full

**Blocking Verbs:** None

---

#### Implementation Analysis

**Complexity:** LOW

**Dependencies:**
- Other processors: None
- External services: None
- OS-specific functionality: None (pure arithmetic)
- GUI/window context: NO

**Known Issues/Limitations:**
- Need to determine exact RGB data type representation in UserTalk
- Likely stores as long integer or tyvaluerecord with rgb subtype

**Estimated Implementation Effort:** 1-2 hours

---

#### UserTalk Documentation

**Key Patterns Observed:**
- `rgb.set(r, g, b)` - Create RGB value from components
- `rgb.get(rgbvalue, @r, @g, @b)` - Extract components from RGB value

**Special Considerations:**
- Component range: 0-65535 (16-bit per channel)
- Need to validate component ranges
- May need to support both long integer and structured RGB type

**Verb List:**
1. `rgb.get (rgbvalue, @red, @green, @blue)` - Extract RGB components
2. `rgb.set (red, green, blue) -> rgbvalue` - Create RGB value

---

#### Priority & Sequencing

**Priority:** QUICK WIN

**Recommended Implementation Order:** 1

**Blockers/Prerequisites:** None - pure data manipulation

**Quick Win Justification:**
Trivial arithmetic operations, no dependencies, can implement and test in <2 hours. Perfect starting point for Phase 3.

---

### Processor: `point`

**EFP ID:** `1005` (lang block 7) | **Verb Count:** `2` | **Status:** ✅

**Documentation:** [`point`](docs/usertalk/docserver.userland.com/point/index.html)

---

#### Category Assessment

**Category:** Core Functionality

**Rationale:**
Point values represent 2D coordinates (h, v). Like RGB, this is pure data structure manipulation with no GUI dependency, despite being used for GUI positioning.

**Headless Compatibility:** Full

**Blocking Verbs:** None

---

#### Implementation Analysis

**Complexity:** LOW

**Dependencies:**
- Other processors: None
- External services: None
- OS-specific functionality: None (pure data structure)
- GUI/window context: NO

**Known Issues/Limitations:**
- Points are QuickDraw-style (h=horizontal, v=vertical)
- Values are typically 16-bit signed integers

**Estimated Implementation Effort:** 1-2 hours

---

#### UserTalk Documentation

**Key Patterns Observed:**
- `point.set(h, v)` - Create point from coordinates
- `point.get(pt, @h, @v)` - Extract coordinates from point

**Special Considerations:**
- Coordinate values: typically -32768 to 32767 (signed 16-bit)
- May store as long integer (packed) or structured point type
- Used throughout GUI code but has no GUI dependency itself

**Verb List:**
1. `point.get (pointvalue, @h, @v)` - Extract horizontal and vertical coordinates
2. `point.set (h, v) -> pointvalue` - Create point value

---

#### Priority & Sequencing

**Priority:** QUICK WIN

**Recommended Implementation Order:** 2

**Blockers/Prerequisites:** None

**Quick Win Justification:**
Nearly identical to RGB in complexity. Simple data structure operations. Essential for other geometry verbs.

---

### Processor: `rectangle`

**EFP ID:** `1005` (lang block 8) | **Verb Count:** `2` | **Status:** ✅

**Documentation:** [`rectangle`](docs/usertalk/docserver.userland.com/rectangle/index.html)

---

#### Category Assessment

**Category:** Core Functionality

**Rationale:**
Rectangle values represent 2D bounding boxes (top, left, bottom, right). Pure data structure like point and RGB.

**Headless Compatibility:** Full

**Blocking Verbs:** None

---

#### Implementation Analysis

**Complexity:** LOW

**Dependencies:**
- Other processors: Possibly point (rectangles are defined by two points)
- External services: None
- OS-specific functionality: None
- GUI/window context: NO

**Known Issues/Limitations:**
- kernelverbs.rc only defines get/set (documentation shows more verbs)

**Estimated Implementation Effort:** 1-2 hours

---

#### UserTalk Documentation

**Key Patterns Observed:**
- `rectangle.set(top, left, bottom, right)` - Create rectangle
- `rectangle.get(rect, @top, @left, @bottom, @right)` - Extract coordinates

**Special Considerations:**
- QuickDraw-style coordinates (top < bottom, left < right)
- Four 16-bit integers

**Verb List:**
1. `rectangle.get (rectvalue, @top, @left, @bottom, @right)` - Extract coordinates
2. `rectangle.set (top, left, bottom, right) -> rectvalue` - Create rectangle

---

#### Priority & Sequencing

**Priority:** QUICK WIN

**Recommended Implementation Order:** 3

**Blockers/Prerequisites:** None

**Quick Win Justification:**
Same pattern as point and RGB. Simple data structure operations.

---

### Processor: `math`

**EFP ID:** `1024` | **Verb Count:** `3` | **Status:** ✅

**Documentation:** No dedicated math/ directory found

---

#### Category Assessment

**Category:** Core Functionality

**Rationale:**
Simple mathematical operations: min, max, sqrt. Pure computation with no dependencies.

**Headless Compatibility:** Full

**Blocking Verbs:** None

---

#### Implementation Analysis

**Complexity:** LOW

**Dependencies:**
- Other processors: None
- External services: None
- OS-specific functionality: None (standard C library)
- GUI/window context: NO

**Known Issues/Limitations:**
- Need to handle integer vs floating-point inputs
- Sqrt of negative numbers should error

**Estimated Implementation Effort:** 1-2 hours

---

#### UserTalk Documentation

**Key Patterns Observed:**
- Standard mathematical operations
- Likely accept numeric types (long, double)

**Special Considerations:**
- Type coercion: handle both integer and floating-point
- Error handling: sqrt of negative, max/min edge cases

**Verb List:**
1. `math.min (a, b, ...) -> number` - Return minimum value
2. `math.max (a, b, ...) -> number` - Return maximum value
3. `math.sqrt (x) -> double` - Return square root

---

#### Priority & Sequencing

**Priority:** QUICK WIN

**Recommended Implementation Order:** 4

**Blockers/Prerequisites:** None

**Quick Win Justification:**
Trivial wrappers around standard C library functions. Can be implemented in 1-2 hours.

---

### Processor: `semaphore`

**EFP ID:** `1005` (lang block 12) | **Verb Count:** `2` | **Status:** ✅

**Documentation:** [`semaphore`](docs/usertalk/docserver.userland.com/semaphore/index.html)

---

#### Category Assessment

**Category:** Core Functionality

**Rationale:**
Synchronization primitives for thread-safe access. Pure logic with no GUI/window dependencies.

**Headless Compatibility:** Full

**Blocking Verbs:** None

---

#### Implementation Analysis

**Complexity:** LOW

**Dependencies:**
- Other processors: None
- External services: None
- OS-specific functionality: Threading/mutex primitives (POSIX/Win32)
- GUI/window context: NO

**Known Issues/Limitations:**
- Need thread-safe hash table to track named semaphores
- Must handle deadlock prevention

**Estimated Implementation Effort:** 2-4 hours

---

#### UserTalk Documentation

**Key Patterns Observed:**
- Named semaphores: semaphore.lock(name) / semaphore.unlock(name)
- String parameter for semaphore name
- Boolean return values

**Special Considerations:**
- Thread-safety critical
- Named semaphores must be process-global
- Need cleanup on process exit

**Verb List:**
1. `semaphore.lock (name)` - Lock a named semaphore
2. `semaphore.unlock (name)` - Unlock a named semaphore

---

#### Priority & Sequencing

**Priority:** QUICK WIN

**Recommended Implementation Order:** 5

**Blockers/Prerequisites:** Thread-safe hash table, platform threading library

**Quick Win Justification:**
Simple mutex-based implementation, well-defined behavior, useful for multi-threaded scripts.

---

### Processor: `clipboard`

**EFP ID:** `1015` | **Verb Count:** `2` | **Status:** ✅

**Documentation:** [`clipboard`](docs/usertalk/docserver.userland.com/clipboard/index.html)

---

#### Category Assessment

**Category:** Platform-Specific (Required)

**Rationale:**
Requires OS-specific APIs but implementation is straightforward per-platform.

**Headless Compatibility:** Full (platform-specific but headless-safe)

**Blocking Verbs:** None

---

#### Implementation Analysis

**Complexity:** MEDIUM

**Dependencies:**
- Other processors: None
- External services: None
- OS-specific functionality: YES (macOS Pasteboard, Win32 clipboard, X11)
- GUI/window context: NO

**Known Issues/Limitations:**
- Must handle text encoding (UTF-8 conversion)
- Start with text-only, expand to binary later

**Estimated Implementation Effort:** 4-6 hours

---

#### UserTalk Documentation

**Key Patterns Observed:**
- `clipboard.get ()` - Get clipboard text
- `clipboard.put (text)` - Put text on clipboard

**Special Considerations:**
- Text encoding conversions
- Thread-safety considerations

**Verb List:**
1. `clipboard.get () -> string` - Get clipboard text
2. `clipboard.put (string)` - Put text on clipboard

---

#### Priority & Sequencing

**Priority:** QUICK WIN

**Recommended Implementation Order:** 6

**Blockers/Prerequisites:** Platform clipboard APIs

**Quick Win Justification:**
Only 2 verbs, well-defined platform APIs, text-only implementation is straightforward.

---

### Processor: `base64`

**EFP ID:** `1005` (lang block 13) | **Verb Count:** `2` | **Status:** ✅

**Documentation:** [`base64`](docs/usertalk/docserver.userland.com/base64/index.html)

---

#### Category Assessment

**Category:** Core Functionality

**Rationale:**
Pure algorithmic transformation. Well-defined standard (RFC 4648).

**Headless Compatibility:** Full

**Blocking Verbs:** None

---

#### Implementation Analysis

**Complexity:** LOW-MEDIUM

**Dependencies:**
- Other processors: None
- External services: None
- OS-specific functionality: None
- GUI/window context: NO

**Known Issues/Limitations:**
- Must handle binary data integrity
- Padding rules

**Estimated Implementation Effort:** 3-4 hours

---

#### UserTalk Documentation

**Key Patterns Observed:**
- `base64.encode (data)` - Encode to base64
- `base64.decode (base64string)` - Decode from base64

**Special Considerations:**
- Binary data preservation
- Standard base64 alphabet

**Verb List:**
1. `base64.encode (data) -> string` - Encode to base64
2. `base64.decode (base64string) -> data` - Decode from base64

---

#### Priority & Sequencing

**Priority:** QUICK WIN

**Recommended Implementation Order:** 7

**Blockers/Prerequisites:** Binary data type support

**Quick Win Justification:**
Well-defined algorithm, essential for web/API work.

---

### Processor: `kb`

**EFP ID:** `1005` (lang block 5) | **Verb Count:** `4` | **Status:** ⚠️

**Documentation:** [`kb`](docs/usertalk/docserver.userland.com/kb/index.html)

---

#### Category Assessment

**Category:** Platform-Specific (Optional) / Partial Implementation

**Rationale:**
Keyboard modifier state queries. Can return default values in headless mode.

**Headless Compatibility:** Partial (stub with false in headless)

**Blocking Verbs:** All require input system for meaningful results

---

#### Implementation Analysis

**Complexity:** MEDIUM

**Dependencies:**
- Other processors: None
- External services: None
- OS-specific functionality: YES (platform input APIs)
- GUI/window context: DEPENDS

**Known Issues/Limitations:**
- In headless mode, no keyboard events occur
- Can stub to return false for all modifiers

**Estimated Implementation Effort:** 2-3 hours (stub) / 6-8 hours (full)

---

#### UserTalk Documentation

**Verb List:**
1. `kb.optionKey () -> boolean` - Option/Alt key pressed?
2. `kb.cmdKey () -> boolean` - Command/Ctrl key pressed?
3. `kb.shiftKey () -> boolean` - Shift key pressed?
4. `kb.controlKey () -> boolean` - Control key pressed?

---

#### Priority & Sequencing

**Priority:** MEDIUM (LOW for headless)

**Recommended Implementation Order:** 8 (stub version)

**Blockers/Prerequisites:** Platform input APIs (optional)

**Quick Win Justification:**
Stub implementation is trivial (return false). Low priority for headless.

---

### Processor: `mouse`

**EFP ID:** `1005` (lang block 6) | **Verb Count:** `2` | **Status:** ⚠️

**Documentation:** [`mouse`](docs/usertalk/docserver.userland.com/mouse/index.html)

---

#### Category Assessment

**Category:** Platform-Specific (Optional) / Partial Implementation

**Rationale:**
Mouse state verbs. Can return default values in headless mode.

**Headless Compatibility:** Partial (stub with defaults in headless)

**Blocking Verbs:** Both require GUI context for meaningful results

---

#### Implementation Analysis

**Complexity:** MEDIUM

**Dependencies:**
- Other processors: point (for location)
- External services: None
- OS-specific functionality: YES
- GUI/window context: YES

**Known Issues/Limitations:**
- In headless mode, no meaningful values
- Can stub to return (0,0) and false

**Estimated Implementation Effort:** 2-3 hours (stub) / 8-12 hours (full)

---

#### UserTalk Documentation

**Verb List:**
1. `mouse.button () -> boolean` - Is mouse button pressed?
2. `mouse.location () -> point` - Get mouse coordinates

---

#### Priority & Sequencing

**Priority:** MEDIUM (LOW for headless)

**Recommended Implementation Order:** 9 (stub version)

**Blockers/Prerequisites:** point processor

**Quick Win Justification:**
Can implement stub version quickly for headless mode.

---

### Processor: `speaker`

**EFP ID:** `1005` (lang block 10) | **Verb Count:** `3` | **Status:** ⚠️

**Documentation:** [`speaker`](docs/usertalk/docserver.userland.com/speaker/index.html)

---

#### Category Assessment

**Category:** Platform-Specific (Optional)

**Rationale:**
Audio output. Can be stubbed to silently succeed in headless mode.

**Headless Compatibility:** Full (stub or platform audio)

**Blocking Verbs:** None (all can gracefully degrade)

---

#### Implementation Analysis

**Complexity:** MEDIUM-HIGH

**Dependencies:**
- Other processors: None
- External services: None
- OS-specific functionality: YES (platform audio APIs)
- GUI/window context: NO

**Known Issues/Limitations:**
- Audio system may not be available in headless environments
- Can no-op in headless mode

**Estimated Implementation Effort:** 1-2 hours (stub) / 12-16 hours (full)

---

#### UserTalk Documentation

**Verb List:**
1. `speaker.beep ()` - System beep
2. `speaker.sound (frequency, duration, amplitude)` - Play tone
3. `speaker.playNamedSound (name)` - Play named sound

---

#### Priority & Sequencing

**Priority:** LOW (MEDIUM for desktop)

**Recommended Implementation Order:** 10 (stub for headless)

**Blockers/Prerequisites:** Platform audio APIs (optional)

**Quick Win Justification:**
Stub implementation is trivial (no-op functions). Low priority for Phase 3.

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
