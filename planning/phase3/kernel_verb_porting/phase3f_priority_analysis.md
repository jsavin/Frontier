# Phase 3.F: High-Priority Verb Implementation Analysis

**Date:** 2025-12-16
**Status:** In Progress
**Goal:** Identify 20-50 high-value stubbed verbs for Phase 4 implementation

---

## Executive Summary

**Current State:**
- 479/707 verbs implemented (67%)
- 228/707 verbs stubbed (32%)

**Analysis Scope:**
Categorize 228 stubbed verbs by:
1. Headless compatibility (GUI required?)
2. Implementation complexity (easy/medium/hard)
3. Likely usage frequency (high/medium/low)
4. Dependencies (other systems required?)

**Target Output:**
A prioritized backlog of 20-50 verbs ranked P0-P2 for Phase 4.

---

## Categorization Framework

### Headless Compatibility
- **✅ Headless-OK**: Can work without GUI/window system
- **🖥️ GUI-Required**: Needs window/dialog/menu infrastructure
- **⚠️ Hybrid**: Some functionality works headless, some doesn't

### Implementation Complexity
- **🟢 Easy**: Simple logic, minimal dependencies (1-2 hours)
- **🟡 Medium**: Moderate logic, some research needed (4-8 hours)
- **🔴 Hard**: Complex algorithm, significant research (1-3 days)

### Usage Frequency (from DocServer docs + common patterns)
- **🔥 High**: Core functionality, used in most scripts
- **📊 Medium**: Specialized but common use cases
- **🔬 Low**: Edge cases, rarely used

### Dependencies
- **None**: Self-contained
- **File I/O**: Requires portable file layer
- **Network**: Requires TCP/socket layer
- **External**: Requires specific library/system

---

## Analysis by Processor

### Priority 1: Nearly Complete Processors (Quick Wins)

#### 1. **clock** - 1 verb missing (85% → 100%)
- `clock.set` - ❌ GUI-Required (system time setting)
  - Already has error stub: "Can't set system time because it requires administrator privileges"
  - **Decision**: Keep as error stub (not headless-compatible)

#### 2. **date** - 4 verbs missing (86% → 100%)
Missing verbs to investigate:
- `date.monthtostring`
- `date.dayofweektostring`
- `date.versionlessthan`
- `date.netstandardstring`

Need to check current implementation status for these.

#### 3. **file** - 3 verbs missing (96% → 100%)
Missing verbs need investigation - likely trivial to add.

#### 4. **html** - 1 verb missing (95% → 100%)
One verb away from complete - investigate which one.

#### 5. **op** - 1 verb missing (97% → 100%)
One verb away from complete - investigate which one.

#### 6. **sys** - 1 verb missing (93% → 100%)
One verb away from complete - investigate which one.

#### 7. **xml** - 1 verb missing (92% → 100%)
One verb away from complete - investigate which one.

### Priority 2: High-Value Headless-Compatible Processors

#### 8. **table** - 8 verbs missing (55% → 100%)
**Status**: CRITICAL - Core data structure
**Headless**: ✅ Yes (hash table operations)
**Complexity**: 🟡 Medium (table manipulation logic)
**Usage**: 🔥 High (fundamental to UserTalk)

Missing verbs need investigation - likely includes:
- Table navigation/iteration
- Table value manipulation
- Table metadata operations

**Priority**: **P0** - Essential infrastructure

#### 9. **lang** - 13 verbs missing (77% → 100%)
**Status**: Core language runtime
**Headless**: ⚠️ Hybrid (some OSA/AppleScript verbs can't work)
**Complexity**: 🟡 Medium to 🔴 Hard
**Usage**: 🔥 High

Need to separate headless-OK from GUI/OSA verbs.

#### 10. **base64** - 2 verbs (0% → 100%)
**Status**: Not started
**Headless**: ✅ Yes (encoding/decoding)
**Complexity**: 🟢 Easy (standard algorithm)
**Usage**: 📊 Medium (web/API integration)

Verbs:
- `base64.encode`
- `base64.decode`

**Priority**: **P1** - Common utility, easy implementation

#### 11. **bit** - 8 verbs (0% → 100%)
**Status**: Not started
**Headless**: ✅ Yes (bitwise operations)
**Complexity**: 🟢 Easy (standard bit ops)
**Usage**: 📊 Medium (low-level manipulation)

Verbs: and, or, xor, not, shiftleft, shiftright, etc.

**Priority**: **P1** - Easy wins, useful utility

#### 12. **semaphore** - 2 verbs (0% → 100%)
**Status**: Not started
**Headless**: ✅ Yes (synchronization primitives)
**Complexity**: 🟡 Medium (thread safety concerns)
**Usage**: 📊 Medium (concurrency control)

Verbs:
- `semaphore.lock`
- `semaphore.unlock`

**Priority**: **P1** - Useful for multi-threaded scripts

### Priority 3: GUI-Dependent (Defer or Stub)

#### **dialog** - 5 verbs missing (73% complete)
**Status**: Partially implemented
**Headless**: 🖥️ GUI-Required
**Decision**: Keep existing stubs with clear error messages

#### **editmenu** - 16 verbs (0%)
**Headless**: 🖥️ GUI-Required (clipboard/edit operations in UI)
**Decision**: Error stubs only

#### **filemenu** - 10 verbs (0%)
**Headless**: 🖥️ GUI-Required (file menu operations)
**Decision**: Error stubs only

#### **htmlcontrol** - 8 verbs (0%)
**Headless**: 🖥️ GUI-Required (HTML rendering in UI)
**Decision**: Error stubs only

#### **launch** - 5 verbs (0%)
**Headless**: 🖥️ GUI-Required (app launching with UI)
**Decision**: Error stubs only

#### **mainwindow** - 7 verbs (0%)
**Headless**: 🖥️ GUI-Required (main window operations)
**Decision**: Error stubs only

#### **menu** - 2 verbs missing (85%)
**Headless**: 🖥️ GUI-Required
**Decision**: Keep as stubs

#### **search** - 6 verbs (0%)
**Headless**: 🖥️ GUI-Required (search UI)
**Decision**: Error stubs only

#### **statusbar** - 5 verbs (0%)
**Headless**: 🖥️ GUI-Required (status bar UI)
**Decision**: Error stubs only

#### **target** - 3 verbs (0%)
**Headless**: 🖥️ GUI-Required (window targeting)
**Decision**: Error stubs only

#### **window** - 5 verbs missing (83%)
**Headless**: 🖥️ GUI-Required
**Decision**: Keep as stubs

### Priority 4: Specialized/External Dependencies

#### **dll** - 4 verbs (0%)
**Headless**: ⚠️ Platform-specific (Windows DLL loading)
**Complexity**: 🔴 Hard (cross-platform challenges)
**Usage**: 🔬 Low (Windows-specific)
**Decision**: Low priority / platform-specific

#### **inetd** - 1 verb (0%)
**Headless**: ✅ Yes (network daemon)
**Complexity**: 🔴 Hard (server infrastructure)
**Usage**: 🔬 Low (specialized)
**Decision**: Defer to later phase

#### **mrcalendar** - 11 verbs (0%)
**Headless**: 🖥️ GUI-Required (calendar UI control)
**Decision**: Error stubs only

#### **osa** - 2 verbs (0%)
**Headless**: ❌ No (AppleScript/OSA integration)
**Decision**: Error stubs with clear message

#### **python** - 1 verb (0%)
**Headless**: ✅ Yes (Python integration)
**Complexity**: 🔴 Hard (embedding Python interpreter)
**Usage**: 📊 Medium (scripting bridge)
**Decision**: Defer - complex external dependency

#### **re** - 10 verbs (0%)
**Headless**: ✅ Yes (regular expressions)
**Complexity**: 🟡 Medium (already implemented in langregexp.c)
**Usage**: 🔥 High (text processing)

**Wait - check if these are already implemented!**

#### **rez** - 15 verbs (0%)
**Headless**: ⚠️ Mac-specific (Resource fork operations)
**Complexity**: 🔴 Hard (legacy Mac format)
**Usage**: 🔬 Low (legacy compatibility)
**Decision**: Low priority

#### **script** - 13 verbs (0%)
**Headless**: ⚠️ Hybrid (compilation OK, debugging needs UI)
**Complexity**: 🟡 Medium to 🔴 Hard
**Usage**: 📊 Medium (script management)
**Decision**: Needs detailed analysis

#### **searchengine** - 5 verbs (0%)
**Headless**: ✅ Yes (text indexing/searching)
**Complexity**: 🔴 Hard (search algorithm)
**Usage**: 📊 Medium (content indexing)
**Decision**: Defer - complex implementation

#### **tcp** - 23 verbs (0%)
**Headless**: ✅ Yes (network I/O)
**Complexity**: 🟡 Medium (socket programming)
**Usage**: 🔥 High (web services, APIs)
**Decision**: **P0-P1** - Critical for headless web scripts

#### **thread** - 17 verbs (0%)
**Headless**: ✅ Yes (threading primitives)
**Complexity**: 🔴 Hard (thread safety, synchronization)
**Usage**: 📊 Medium (concurrency)
**Decision**: **P1** - Important but complex

#### **webserver** - 7 verbs (0%)
**Headless**: ✅ Yes (HTTP server)
**Complexity**: 🔴 Hard (server infrastructure)
**Usage**: 🔥 High (headless web services)
**Decision**: **P0** - Core headless use case

#### **clipboard** - 2 verbs (0%)
**Headless**: ⚠️ Hybrid (system clipboard access)
**Complexity**: 🟡 Medium (platform-specific)
**Usage**: 🔬 Low (headless has no clipboard)
**Decision**: Error stubs for headless

---

## Action Items - Next Steps

1. **Verify `re` processor status** - Report shows 0%, but langregexp.c exists
2. **Investigate nearly-complete processors** - Get exact missing verb names
3. **Prioritize table verbs** - 8 missing verbs in core data structure
4. **Assess tcp/webserver** - Critical for headless web use cases
5. **Generate final Phase 4 backlog** - Ranked list of 20-50 verbs

---

## Questions to Resolve

1. Are `re` verbs actually stubbed or is the analyzer missing them?
2. Which specific verbs are missing from nearly-complete processors?
3. What's the dependency between tcp and webserver verbs?
4. Can any script/thread verbs work safely in headless mode?
5. What's the actual implementation status of base64 verbs?

