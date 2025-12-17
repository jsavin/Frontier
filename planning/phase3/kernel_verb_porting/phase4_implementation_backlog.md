# Phase 4: Implementation Backlog - Prioritized Verb List

**Date:** 2025-12-16
**Status:** Ready for Implementation
**Source:** Phase 3.F Analysis

---

## Executive Summary

**Target:** Implement 40-60 high-value headless-compatible verbs
**Current:** 479/707 verbs (67%) → Goal: 540+/707 verbs (76%+)

**Selection Criteria:**
1. ✅ Headless-compatible (no GUI dependency)
2. 🔥 High or medium usage frequency
3. 🟢 Easy to medium implementation complexity
4. No external dependencies (or dependencies already present)

---

## Priority 0: Runtime Foundations (21-29 verbs)

### **Table Processor** (8 missing verbs) - HIGHEST PRIORITY
**Why:** Hash table is THE fundamental UserTalk data structure
**Complexity:** 🟡 Medium (table manipulation logic)
**Usage:** 🔥 CRITICAL - Used in every non-trivial script

**Missing verbs to implement (need to identify exact ones):**
- Likely includes: iteration, key enumeration, table merging, value lookup
- Check `headless_table_verbs.c` for stub list

**Implementation approach:** Extend existing tablestructure.c/tableverbs.c
**Rationale:** Tables are the foundation of UserTalk's data model - must be complete

---

### **Script Processor** (13 verbs, ~5-8 headless-compatible) - CRITICAL
**Why:** Code compilation and execution infrastructure
**Complexity:** 🟡 to 🔴 (varies by verb)
**Usage:** 🔥 HIGH - Core runtime functionality

**Headless-compatible subset:**
- `script.compile` - Compile UserTalk source
- `script.run` - Execute compiled script
- `script.getcode` - Get compiled code
- `script.setcode` - Set compiled code
- `script.getsource` - Get source text
- `script.setsource` - Set source text
- `script.error` - Get error info
- `script.getattributes` - Get script metadata

**Skip (GUI-dependent):**
- Debugger verbs (breakpoints, stepping, etc.)
- Script editor integration

**Implementation approach:** Work with existing lang/compiler infrastructure
**Rationale:** Scripts are the executable unit in UserTalk - runtime depends on this

---

### **Thread Processor** (17 verbs, ~5-8 core subset) - CRITICAL
**Why:** Concurrency model for UserTalk runtime
**Complexity:** 🔴 Hard (thread safety, synchronization)
**Usage:** 🔥 HIGH - Multi-threaded script execution

**Core subset:**
- `thread.create` - Spawn new thread
- `thread.join` - Wait for thread completion
- `thread.kill` - Terminate thread
- `thread.getcurrent` - Get current thread ID
- `thread.sleep` - Sleep current thread
- `thread.yield` - Yield to scheduler
- `thread.getpriority` - Get thread priority
- `thread.setpriority` - Set thread priority

**Defer (advanced):**
- Message passing, condition variables, barriers

**Implementation approach:** Use portable threading primitives (pthread, etc.)
**Rationale:** Threading is fundamental to concurrent UserTalk execution model

---

## Priority 1: High-Value Utilities (20-25 verbs)

### **Regular Expressions** (10 verbs) - EASY WIN
**Why:** Text processing is common in scripts
**Complexity:** 🟢 EASY - **Implementation already exists in langregexp.c!**
**Usage:** 🔥 HIGH

**All 10 verbs:**
- `re.compile` - Compile regex pattern
- `re.match` - Match pattern
- `re.replace` - Replace matches
- `re.extract` - Extract groups
- `re.split` - Split by pattern
- `re.join` - Join with pattern
- `re.visit` - Iterate matches
- `re.grep` - Search in text
- `re.getpatterninfo` - Pattern metadata
- `re.expand` - Expand template

**Implementation approach:** **Link existing langregexp.c into headless build**
**Effort:** 1-2 hours to wire up registration

---

### **Base64** (2 verbs) - EASY WIN
**Why:** Common for web APIs, data encoding
**Complexity:** 🟢 EASY - Standard algorithm, may already have helper functions
**Usage:** 📊 MEDIUM

**Both verbs:**
- `base64.encode` - Encode to base64
- `base64.decode` - Decode from base64

**Implementation approach:** Check if base64 helpers exist in codebase, wire them up
**Effort:** 2-4 hours

---

### **Bit Operations** (8 verbs) - EASY
**Why:** Useful for low-level data manipulation
**Complexity:** 🟢 EASY - Standard bitwise ops
**Usage:** 📊 MEDIUM

**All 8 verbs:**
- `bit.and` - Bitwise AND
- `bit.or` - Bitwise OR
- `bit.xor` - Bitwise XOR
- `bit.not` - Bitwise NOT
- `bit.shiftleft` - Left shift
- `bit.shiftright` - Right shift
- `bit.test` - Test bit
- `bit.set` - Set bit

**Implementation approach:** Simple C bitwise operators
**Effort:** 4-6 hours

---

### **Semaphore** (2 verbs) - MEDIUM
**Why:** Thread synchronization for concurrent scripts
**Complexity:** 🟡 MEDIUM - Thread safety critical
**Usage:** 📊 MEDIUM

**Both verbs:**
- `semaphore.lock` - Acquire lock
- `semaphore.unlock` - Release lock

**Implementation approach:** Use platform threading primitives (pthread_mutex, etc.)
**Effort:** 4-6 hours (careful testing required)

---

### **Thread** (17 verbs) - SELECTIVE
**Why:** Concurrency support
**Complexity:** 🔴 HARD - Thread management, safety
**Usage:** 📊 MEDIUM

**Recommended subset (5-8 verbs):**
- `thread.create` - Create new thread
- `thread.join` - Wait for thread
- `thread.kill` - Terminate thread
- `thread.getcurrent` - Get current thread ID
- `thread.sleep` - Sleep current thread
- `thread.yield` - Yield to scheduler

**Defer:** Advanced synchronization, message passing
**Implementation approach:** Use portable threading layer
**Effort:** 2-3 days

---

## Priority 2: Quick Wins (10-15 verbs)

### **Nearly-Complete Processors** (7 verbs total)
**Why:** Low-hanging fruit, each is 1 verb away from 100%
**Complexity:** 🟢 EASY (presumably simple if processor is otherwise complete)
**Usage:** Varies

**Processors:**
- `file` (3 missing) - Need to identify which ones
- `html` (1 missing) - Need to identify
- `op` (1 missing) - Need to identify
- `sys` (1 missing) - Need to identify
- `xml` (1 missing) - Need to identify

**Implementation approach:** Check stub files, implement missing verbs
**Effort:** 1-2 hours each

---

### **Date** (4 missing verbs) - VERIFY STATUS
**Why:** Date/time is common in scripts
**Complexity:** 🟢 EASY (if they're simple conversions)
**Usage:** 🔥 HIGH

**Need to verify which 4 are missing - may already be implemented**

---

### **Lang** (13 missing) - SELECTIVE
**Why:** Core language functionality
**Complexity:** 🟡 to 🔴 (varies - some are OSA/AppleScript)
**Usage:** 🔥 HIGH

**Filter out:** OSA/AppleScript verbs (can't work headless)
**Implement:** Language runtime verbs (type conversion, etc.)
**Needs analysis:** Which of the 13 are headless-compatible?

---

## Priority 3: Defer or Stub

### **GUI-Required Processors** (~100 verbs)
- dialog, editmenu, filemenu, htmlcontrol, launch, mainwindow
- menu, search, statusbar, target, window
- mrcalendar

**Decision:** Keep as error stubs with clear messages

### **Platform-Specific** (~20 verbs)
- dll (Windows DLL loading)
- rez (Mac resource forks)
- osa (AppleScript)

**Decision:** Platform-specific error stubs

### **Complex/Deferred** (~30 verbs)
- python (Python embedding)
- inetd (network daemon infrastructure)
- searchengine (complex search algorithm)
- script (needs more analysis)

**Decision:** Defer to later phase

---

## Phase 4 Implementation Plan (REVISED)

### Sprint 1: Runtime Foundations (16-21 verbs)
**Priority:** Core UserTalk runtime infrastructure

1. **Table processor (8 verbs)** - Hash table operations
   - Essential data structure used throughout runtime
   - Missing: iteration, enumeration, merging, etc.
   - **Effort:** 1-2 days

2. **Script processor (13 verbs) - SELECTIVE**
   - Code compilation, execution, debugging
   - Filter: Headless-compatible only (skip UI debugging)
   - Target: ~5-8 verbs (compile, run, get/set attributes)
   - **Effort:** 2-3 days

3. **Thread processor (17 verbs) - SELECTIVE**
   - Concurrency primitives
   - Target: Core 5-8 verbs (create, join, sleep, yield, getcurrent)
   - Defer: Advanced synchronization
   - **Effort:** 2-3 days

**Result:** +21 verbs (table + script subset + thread subset), 67% → 70%

---

### Sprint 2: Quick Wins & Utilities (20-25 verbs)
**Priority:** Easy implementations, high value

1. **Link re processor** - 10 verbs (EASY: already implemented!)
2. **Implement base64** - 2 verbs
3. **Implement bit ops** - 8 verbs
4. **Complete nearly-done processors** - 7 verbs (file, html, op, sys, xml)

**Result:** +27 verbs, 70% → 74%

---

### Sprint 3: Network Infrastructure (17-19 verbs)
**Priority:** Headless web services

1. **TCP processor** - 10-12 core verbs
2. **Webserver processor** - 7 verbs

**Result:** +19 verbs, 74% → 76.7%

---

### Sprint 4: Polish & Refinement
1. Complete remaining thread verbs
2. Complete remaining script verbs
3. Semaphore (2 verbs)
4. Final testing & documentation

**Final:** ~65 verbs implemented, 479 → 544 (67% → 77%)

---

## Success Criteria

- ✅ All P0 verbs implemented and tested
- ✅ TCP/webserver working with basic HTTP server
- ✅ Table processor 100% complete
- ✅ Regular expressions fully functional
- ✅ All "nearly complete" processors at 100%
- ✅ Full test suite passes
- ✅ Documentation updated

---

## Next Steps

1. **Verify exact missing verbs** - Check stub files for file, html, op, sys, xml, date
2. **Create Phase 4.A plan** - Week 1 quick wins implementation
3. **Set up test infrastructure** - Unit tests for each new verb
4. **Begin implementation** - Start with re processor linkage

