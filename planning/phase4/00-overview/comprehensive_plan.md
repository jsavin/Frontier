# Frontier Global State Elimination Plan

**Status**: Active Planning Document
**Date**: 2026-01-13
**Author**: System Architect
**Strategic Priority**: P0 (Launch-Blocking)
**Relates to**: Collaborative ODB (Phase 2.0), ADR-005, ADR-006, ADR-008, Issue #135, Issue #292

---

## Executive Summary

**Mission**: Eliminate ALL global mutable state from Frontier runtime to achieve thread-safety required for launch and enable collaborative ODB editing (Frontier 2.0).

**Scope**: 200+ global variables across 104 source files, categorized into P0 (launch-blocking), P1 (multi-user foundation), and P2 (cleanup).

**Timeline**: 12-18 weeks across 4 major phases
- **P0 (Weeks 1-6)**: Critical thread-safety (processor tables, hash tables, database context)
- **P1 (Weeks 7-12)**: Multi-user foundation (reference counting, context lifecycle)
- **P2 (Weeks 13-18)**: Comprehensive cleanup (remaining globals, static buffers)

**Overall Strategy**:
1. Thread-local storage for per-thread execution state (proven pattern from ADR-005, ADR-006)
2. Explicit context structures for per-operation state (proven pattern from `db_context`)
3. Reference counting for shared objects (enables multi-user Phase 6+)
4. Zero backward compatibility breaks (macros provide transparent migration)

**Success Criteria**:
- ✅ No global mutable state blocking concurrent operations
- ✅ Thread-safe runtime (multiple threads executing UserTalk simultaneously)
- ✅ Foundation for collaborative ODB (reference counting, context isolation)
- ✅ UserTalk API remains compatible (internal refactoring only)

---

## 1. Global State Inventory

### 1.1 Critical Globals (P0 - Launch Blocking)

**Thread-Safety Violations - Must Eliminate Before Launch**

| Global | File | Type | Impact | Current Status |
|--------|------|------|--------|----------------|
| `efptable` | tablestructure.c:118 | hdlhashtable | Processor dispatch | ⚠️ **Workaround in ADR-008** |
| `currenthashtable` | langhash.c:835 | hdlhashtable | Variable scope | ❌ **Needs Migration** (Issue #262) |
| `databasedata` | db.c | hdldatabaserecord | DB context | ⚠️ **Partially Fixed** (db_context pattern) |
| `roottable` | tablestructure.c:112 | hdlhashtable | System root | ❌ **Needs Context** |
| `systemtable` | tablestructure.c:114 | hdlhashtable | System table | ❌ **Needs Context** |
| `internaltable` | tablestructure.c:116 | hdlhashtable | Internal symbols | ❌ **Needs Context** |
| `langtable` | tablestructure.c:136 | hdlhashtable | Language runtime | ❌ **Needs Context** |
| `builtinstable` | tablestructure.c:146 | hdlhashtable | Built-in functions | ✅ **Read-only after init** |
| `hmagictable` | lang.c:71 | hdlhashtable | Eval communication | ❌ **Needs Thread-Local** |
| `hkeywordtable` | lang.h:710 | hdlhashtable | Keywords | ✅ **Read-only after init** |
| `hbuiltinfunctions` | lang.h:714 | hdlhashtable | Built-ins | ✅ **Read-only after init** |

**Control Flow Globals**

| Global | File | Type | Impact | Current Status |
|--------|------|------|--------|----------------|
| `flbreak` | langinternal.h:333 | boolean | Loop control | ❌ **Needs Thread-Local** |
| `flcontinue` | langinternal.h:335 | boolean | Loop control | ❌ **Needs Thread-Local** |
| `flreturn` | langinternal.h:337 | boolean | Return control | ✅ **In tythreadglobals:184** |
| `fllangerror` | langinternal.h:322 | boolean | Error state | ✅ **In tythreadglobals:196** |
| `langerrordisable` | langinternal.h:324 | uint16 | Error control | ✅ **In tythreadglobals:200** |
| `tryerror` | langinternal.h:326 | Handle | Exception state | ✅ **In tythreadglobals:202** |
| `tryerrorstack` | langinternal.h:328 | Handle | Exception stack | ✅ **In tythreadglobals:204** |

**Scanner/Parser State**

| Global | File | Type | Impact | Current Status |
|--------|------|------|--------|----------------|
| `ctscanlines` | lang.h:716 | ulong | Error reporting | ✅ **In tythreadglobals:156** |
| `ctscanchars` | lang.h:718 | uint16 | Error reporting | ✅ **In tythreadglobals:158** |
| `herrornode` | lang.h:695 | hdltreenode | Debugger state | ✅ **In tythreadglobals:160** |
| `yylval` | langparser.h:33 | hdltreenode | Parser token | ❌ **Needs Thread-Local** |
| `yyval` | langparser.h:33 | hdltreenode | Parser result | ❌ **Needs Thread-Local** |
| `langparser_result` | langparser.h:35 | hdltreenode | Parse result | ❌ **Needs Thread-Local** |

**Callback State**

| Global | File | Type | Impact | Current Status |
|--------|------|------|--------|----------------|
| `langcallbacks` | lang.h:720 | struct | Language hooks | ✅ **In tythreadglobals:136** |
| `launchcallbacks` | launch.h:48 | struct | Launch hooks | ❌ **Needs Analysis** |

**⚠️ NEW: General-Purpose Parameterized Callback Infrastructure (P0a Component)**

See: `planning/phase4/p0a-critical-thread-safety/CALLBACK_INFRASTRUCTURE.md`

**Key Finding (2026-01-20)**: Existing callback system (`system.callbacks.*`) has 22+ callbacks but `langopruncallbackscripts()` only supports PARAMETERLESS callbacks. Multiple use cases need PARAMETERIZED callbacks:
- TCP: `tcp.listenStream()` callbacks with (stream_id, remote_addr, remote_port)
- Window: `closeWindow(title)` callbacks
- Future: Any parameterized callback needs

**Status**: Part of P0a work - extends existing thread-safe callback pattern to support arbitrary parameters. NOT TCP-specific - this is a **platform capability** that unblocks TCP Phase 3, window operations, and all future parameterized callbacks.

### 1.2 High Priority Globals (P1 - Multi-User Foundation)

**System Configuration**

| Global | File | Type | Impact | Current Status |
|--------|------|------|--------|----------------|
| `config` | config.h:113 | tyconfigrecord | Window config | ❌ **Shell push/pop pattern** |
| `iddefaultconfig` | config.h:115 | short | Default window | ❌ **Needs Context** |
| `cancoondata` | cancooninternal.h:113 | hdlcancoonrecord | Cancoon state | ✅ **In tythreadglobals:125** |
| `cancoonwindowinfo` | cancooninternal.h:115 | hdlwindowinfo | Window info | ❌ **Needs Analysis** |
| `cancoonwindow` | cancooninternal.h:117 | WindowPtr | Window ptr | ❌ **GUI-only?** |

**Shell Globals Stack** (19 instances of push/pop pattern - see ADR-006 Phase 2)

| Function | Instances | Purpose | Migration |
|----------|-----------|---------|-----------|
| `shellpushdefaultglobals()` | 9 | Save shell config | ❌ **Needs Thread-Local** |
| `shellpopglobals()` | 10 | Restore shell config | ❌ **Needs Thread-Local** |

**Process/Thread State**

| Global | File | Type | Impact | Current Status |
|--------|------|------|--------|----------------|
| `processstack` | process.c:138 | typrocessstack | Process stack | ✅ **In tythreadglobals:128** |
| `hprocess` | tythreadglobals:130 | hdlprocessrecord | Current process | ✅ **Thread-local** |
| `htablestack` | tythreadglobals:134 | hdltablestack | Table stack | ✅ **Thread-local** |

**Table Structure Globals** (20+ system tables)

| Category | Count | Examples | Migration |
|----------|-------|----------|-----------|
| System tables | 6 | runtimestacktable, semaphoretable, threadtable | ❌ **Needs Context** |
| Environment | 4 | pathstable, environmenttable, charsetstable | ❌ **Needs Context** |
| IAC/AppleEvent | 2 | iacgluetable, iachandlertable | ❌ **Needs Context** |
| Resources | 3 | resourcestable, verbstable, filewindowtable | ❌ **Needs Context** |
| UI | 3 | menubartable, objectmodeltable, agentstable | ❌ **GUI-only?** |

### 1.3 Medium Priority (P2 - Cleanup)

**Static Buffers and Caches**

| File | Variable | Type | Impact | Migration |
|------|----------|------|--------|-----------|
| font.c | `cachedfontname`, `cachedfontnum` | Cache | Font lookup | ✅ **Thread-safe if read-only** |
| error.c | `herrorcushion` | Handle | Error buffer | ✅ **Per-thread cushion?** |
| error.c | `lasterror`, `lasterrormessage` | Error state | Error reporting | ❌ **Needs Thread-Local** |
| memory.c | `hsafetycushion` | Handle | Memory cushion | ❌ **Needs Analysis** |
| quickdraw.c | `portstack`, `clipstack`, `stylestack` | Stacks | Graphics state | ❌ **GUI-only?** |

**Mode Stacks** (already using _Thread_local)

| File | Variable | Type | Status |
|------|----------|------|--------|
| db_format.c:70-72 | `g_mode_stack`, `g_mode_state`, `g_mode_depth` | Mode stack | ✅ **Already thread-local (_Thread_local)** |

**Dialog State**

| File | Variable | Type | Impact | Migration |
|------|----------|------|--------|-----------|
| langdialog.c:57 | `dialogstack[maxnesteddialogs]` | Stack | Dialog nesting | ❌ **Needs Thread-Local** |
| dialogs.c:859 | `passworditem` | short | Password dialog | ❌ **GUI state?** |

**GUI-Only Globals** (Low Priority - Not in Headless)

| Category | Count | Examples | Migration |
|----------|-------|----------|-----------|
| Window state | 20+ | aboutwindow, statswindow, menubardata | ✅ **GUI-only, skip** |
| Graphics state | 15+ | portstack, clipstack, bitmapsenabled | ✅ **GUI-only, skip** |
| Platform-specific | 10+ | shellinstance, hwndMDIClient (Windows) | ✅ **Platform-specific** |

---

## 2. Target Architecture

### 2.1 Thread-Local Storage (Proven Pattern - ADR-005, ADR-006)

**Use for**: Per-thread execution state that doesn't need to be shared across threads.

**Pattern**:
```c
// Common/headers/processinternal.h - Expand tythreadglobals
typedef struct tythreadglobals {
    // ... existing fields (lines 118-237) ...

    /* Phase P0a: Critical Thread-Safety (Weeks 1-3) */

    // Hash table context
    hdlhashtable currenthashtable;       // Variable scope resolution
    hdlhashtable hmagictable;            // Eval communication
    hdltablestack hashtablestack;        // Table scope stack

    // Parser state
    hdltreenode yylval;                  // Parser token value
    hdltreenode yyval;                   // Parser result value
    hdltreenode langparser_result;       // Final parse result

    // Control flow flags
    boolean flbreak;                     // Break statement flag
    boolean flcontinue;                  // Continue statement flag

    // Error state (additional to existing)
    OSErr lasterror;                     // Last OS error
    bigstring lasterrormessage;          // Last error message

    /* Phase P0b: Complete P0 (Weeks 4-6) */

    // Shell configuration context
    tyconfigrecord config;               // Window configuration
    short iddefaultconfig;               // Default window type

    // Dialog state
    tydialogglobals dialogstack[maxnesteddialogs];
    short topdialogstack;

    /* Phase P1: Multi-User Foundation (Weeks 7-12) */

    // Reserved for future context fields
    void *context_reserved[8];

} tythreadglobals;

// Backward-compatible macros (zero API changes)
#define currenthashtable ((**hthreadglobals).currenthashtable)
#define hmagictable ((**hthreadglobals).hmagictable)
#define hashtablestack ((**hthreadglobals).hashtablestack)
#define yylval ((**hthreadglobals).yylval)
#define yyval ((**hthreadglobals).yyval)
#define langparser_result ((**hthreadglobals).langparser_result)
#define flbreak ((**hthreadglobals).flbreak)
#define flcontinue ((**hthreadglobals).flcontinue)
#define lasterror ((**hthreadglobals).lasterror)
#define lasterrormessage ((**hthreadglobals).lasterrormessage)
#define config ((**hthreadglobals).config)
#define iddefaultconfig ((**hthreadglobals).iddefaultconfig)
```

**Benefits**:
- ✅ Zero API changes (macros provide backward compatibility)
- ✅ Proven pattern (ADR-005 parameter state, ADR-006 outline context)
- ✅ Automatic thread isolation
- ✅ Simple to implement and test

**When to Use**: State that is logically "owned" by a thread's execution context (parser state, control flow, error state, current scope).

### 2.2 Explicit Context Structures (Proven Pattern - db_context)

**Use for**: Per-operation state that must be passed explicitly, especially for operations that might span multiple threads.

**Pattern**:
```c
// System Table Context (replaces global system table pointers)
typedef struct system_context {
    hdlhashtable roottable;
    hdlhashtable systemtable;
    hdlhashtable internaltable;
    hdlhashtable langtable;

    // Processor tables
    hdlhashtable efptable;               // External function processors
    hdlhashtable builtinstable;          // Built-in functions (read-only)
    hdlhashtable verbstable;             // Verb table

    // Environment tables
    hdlhashtable pathstable;
    hdlhashtable environmenttable;
    hdlhashtable charsetstable;

    // System infrastructure
    hdlhashtable runtimestacktable;
    hdlhashtable semaphoretable;
    hdlhashtable threadtable;
    hdlhashtable filewindowtable;

    // Resources
    hdlhashtable resourcestable;
    hdlhashtable agentstable;

    // IAC (if needed in headless)
    hdlhashtable iacgluetable;
    hdlhashtable iachandlertable;

    // UI tables (GUI builds only)
    #ifndef FRONTIER_HEADLESS
    hdlhashtable menubartable;
    hdlhashtable objectmodeltable;
    #endif

    // Reference counting for multi-user (Phase P1)
    _Atomic uint32_t refcount;

} system_context;

// Global accessor (for backward compatibility during migration)
system_context* get_system_context(void);

// Explicit API (for new code)
system_context* system_context_create(void);
system_context* system_context_retain(system_context *ctx);
void system_context_release(system_context *ctx);
```

**Migration Strategy**:
```c
// Phase 1: Global accessor (maintains existing behavior)
system_context* g_system_context = NULL;  // Single global instance

system_context* get_system_context(void) {
    return g_system_context;
}

// Phase 2: Macros for backward compatibility
#define roottable (get_system_context()->roottable)
#define systemtable (get_system_context()->systemtable)
#define efptable (get_system_context()->efptable)
// ... etc for all system tables

// Phase 3: Thread-local system context (P1)
_Thread_local system_context* tls_system_context = NULL;

system_context* get_system_context(void) {
    if (tls_system_context == NULL) {
        tls_system_context = system_context_retain(g_system_context);
    }
    return tls_system_context;
}

// Phase 4: Explicit parameter passing (P2 - optional)
// Gradually add system_context* parameters to functions
boolean langfunctionvalue(system_context *sysctx, hdltreenode hparam1, tyvaluerecord *vreturned);
```

**Benefits**:
- ✅ Clear ownership and lifecycle
- ✅ Enables multi-user (different contexts for different users)
- ✅ Reference counting prevents premature disposal
- ✅ Gradual migration path (backward compatibility maintained)

**When to Use**: State that represents a "session" or "operation scope" (system tables, database connections, processor tables).

### 2.3 Reference Counting for Shared Objects (Phase P1)

**Use for**: Objects that need to outlive their creating thread or be shared across operations.

**Pattern**:
```c
// Outline context with refcounting (extends ADR-006)
typedef struct tyoutlinerecord {
    // ... existing fields ...

    _Atomic uint32_t refcount;          // Atomic refcount for thread-safety
    hdlthread owner_thread;             // Thread that currently "owns" this outline
    void *lock_context;                 // CRDT lock state (Phase 6+)
} tyoutlinerecord;

// Lifecycle API
hdloutlinerecord op_outline_create(void);
hdloutlinerecord op_outline_retain(hdloutlinerecord ho);
void op_outline_release(hdloutlinerecord ho);

// Implementation
hdloutlinerecord op_outline_retain(hdloutlinerecord ho) {
    if (ho != NULL) {
        atomic_fetch_add(&(**ho).refcount, 1);
    }
    return ho;
}

void op_outline_release(hdloutlinerecord ho) {
    if (ho != NULL) {
        uint32_t prev = atomic_fetch_sub(&(**ho).refcount, 1);
        if (prev == 1) {
            // Last reference - dispose
            opdisposeoutline(ho);
        }
    }
}
```

**Apply to**:
- Outline objects (tyoutlinerecord)
- Hash tables (tyhashtable)
- System context (system_context)
- Database handles (hdldatabaserecord) - extend existing db_context pattern

**Benefits**:
- ✅ Thread-safe object sharing
- ✅ Prevents dangling pointers
- ✅ Foundation for collaborative ODB (Phase 6+)
- ✅ Familiar pattern (Cocoa, COM, C++ shared_ptr)

---

## 3. Phased Implementation Roadmap

### Phase P0a: Critical Thread-Safety (Weeks 1-3) - LAUNCH BLOCKING

**Goal**: Eliminate globals that cause immediate thread-safety violations.

**Scope**: Hash table context, parser state, control flow flags

**Deliverables**:

**Week 1: Hash Table Context Migration**
- [ ] Add to `tythreadglobals`: `currenthashtable`, `hmagictable`, `hashtablestack`
- [ ] Update thread swap functions (copythreadglobals, swapinthreadglobals)
- [ ] Update initialization (newthreadglobals)
- [ ] Create backward-compatible macros
- [ ] Remove global declarations
- [ ] Test: Run full test suite (`./tools/run_headless_tests.sh`)
- [ ] Deliverable: **PR #1 - Hash Table Context Thread-Safety**

**Week 2: Parser State Migration**
- [ ] Add to `tythreadglobals`: `yylval`, `yyval`, `langparser_result`
- [ ] Update thread swap functions
- [ ] Create backward-compatible macros
- [ ] Remove global declarations from langparser.h
- [ ] Test: Compile and parse test scripts
- [ ] Deliverable: **PR #2 - Parser State Thread-Safety**

**Week 3: Control Flow & Error State**
- [ ] Add to `tythreadglobals`: `flbreak`, `flcontinue`, `lasterror`, `lasterrormessage`
- [ ] Update thread swap functions
- [ ] Create backward-compatible macros
- [ ] Remove global declarations
- [ ] Test: Test loop break/continue, error handling
- [ ] Deliverable: **PR #3 - Control Flow Thread-Safety**

**Success Criteria (P0a)**:
- ✅ No race conditions on hash scope, parser state, control flow
- ✅ Multiple threads can execute UserTalk simultaneously
- ✅ All tests pass (unit + integration)
- ✅ Zero API changes visible to existing code

---

### Phase P0b: Complete Launch Requirements (Weeks 4-6)

**Goal**: Eliminate remaining P0 globals, establish system context pattern.

**Scope**: System table context, shell configuration, dialog state

**Deliverables**:

**Week 4: System Context Structure**
- [ ] Design `system_context` structure (all system tables)
- [ ] Implement create/retain/release lifecycle
- [ ] Create global accessor function `get_system_context()`
- [ ] Add backward-compatible macros for system tables
- [ ] Test: Initialize system context, verify table access
- [ ] Deliverable: **PR #4 - System Context Foundation**

**Week 5: System Table Migration**
- [ ] Migrate system tables to system_context
- [ ] Update `settablestructureglobals()` to use context
- [ ] Remove global system table declarations
- [ ] Fix ADR-008 workaround (remove `get_headless_efptable()`)
- [ ] Test: Database loading, processor resolution
- [ ] Deliverable: **PR #5 - System Table Context Migration**

**Week 6: Shell Configuration & Dialog State**
- [ ] Add to `tythreadglobals`: `config`, `iddefaultconfig`, `dialogstack`
- [ ] Migrate `shellpushdefaultglobals()` / `shellpopglobals()` pattern
- [ ] Update thread swap functions
- [ ] Create backward-compatible macros
- [ ] Test: Window creation, dialog nesting
- [ ] Deliverable: **PR #6 - Shell Config Thread-Safety**

**Success Criteria (P0b)**:
- ✅ No global mutable state blocking concurrent operations
- ✅ System tables in explicit context (no efptable workaround)
- ✅ Shell configuration thread-safe
- ✅ All tests pass (unit + integration)
- ✅ **READY FOR LAUNCH** (thread-safety milestone achieved)

---

### Phase P1a: Multi-User Foundation (Weeks 7-9)

**Goal**: Add reference counting and context lifecycle for multi-user operations.

**Scope**: Reference counting for outlines, hash tables, system context

**Deliverables**:

**Week 7: Reference Counting Infrastructure**
- [ ] Add `_Atomic uint32_t refcount` to tyoutlinerecord
- [ ] Implement `op_outline_retain()` / `op_outline_release()`
- [ ] Update `opdisposeoutline()` to check refcount
- [ ] Test: Outline refcount lifecycle
- [ ] Deliverable: **PR #7 - Outline Reference Counting**

**Week 8: Hash Table Reference Counting**
- [ ] Add refcount to tyhashtable structure
- [ ] Implement `hashtable_retain()` / `hashtable_release()`
- [ ] Update `hashtabledispose()` to check refcount
- [ ] Test: Hash table lifecycle
- [ ] Deliverable: **PR #8 - Hash Table Reference Counting**

**Week 9: System Context Reference Counting**
- [ ] Implement full refcount lifecycle for system_context
- [ ] Add thread-local system context caching
- [ ] Update initialization to use retained contexts
- [ ] Test: Multi-thread system context access
- [ ] Deliverable: **PR #9 - System Context Lifecycle**

**Success Criteria (P1a)**:
- ✅ Objects can be safely shared across threads
- ✅ No dangling pointers (refcount prevents premature disposal)
- ✅ Foundation for concurrent operations on shared objects

---

### Phase P1b: Multi-User Complete (Weeks 10-12)

**Goal**: Stress test concurrent operations, prepare for collaborative ODB.

**Scope**: Concurrent stress testing, lock state preparation

**Deliverables**:

**Week 10: Concurrent Operation Testing**
- [ ] Create multi-thread stress tests (10+ threads)
- [ ] Test concurrent UserTalk execution
- [ ] Test concurrent outline operations
- [ ] Verify no race conditions (thread sanitizer)
- [ ] Deliverable: **PR #10 - Concurrent Stress Tests**

**Week 11: Lock State Preparation**
- [ ] Add reserved fields to tythreadglobals (Phase 6+ prep)
- [ ] Design CRDT lock state structure
- [ ] Document lock state requirements
- [ ] Create placeholder functions
- [ ] Deliverable: **ADR-009 - Collaborative ODB Lock State Design**

**Week 12: Multi-User Documentation**
- [ ] Document multi-user architecture
- [ ] Update CLAUDE.md with refcounting patterns
- [ ] Create developer guide for thread-safe operations
- [ ] Document reserved fields for Phase 6+
- [ ] Deliverable: **docs/MULTI_USER_ARCHITECTURE.md**

**Success Criteria (P1b)**:
- ✅ Multiple threads can execute UserTalk concurrently (stress tested)
- ✅ No race conditions under load (thread sanitizer clean)
- ✅ Foundation complete for Phase 6+ collaborative ODB
- ✅ **READY FOR MULTI-USER FEATURES**

---

### Phase P2: Comprehensive Cleanup (Weeks 13-18)

**Goal**: Eliminate remaining P2 globals, complete global state elimination.

**Scope**: Static buffers, caches, remaining stacks

**Deliverables**:

**Week 13-14: Static Buffer Audit**
- [ ] Audit all static buffers in verb processors
- [ ] Migrate to thread-local or dynamic allocation
- [ ] Document legitimate shared state (locks, caches)
- [ ] Test: Verify no cross-thread contamination
- [ ] Deliverable: **PR #11 - Static Buffer Cleanup**

**Week 15-16: Cache Migration**
- [ ] Audit font cache, error cushions, memory cushions
- [ ] Migrate to thread-local where appropriate
- [ ] Implement thread-safe caching where needed
- [ ] Test: Cache correctness under concurrent load
- [ ] Deliverable: **PR #12 - Cache Thread-Safety**

**Week 17: Final Global Audit**
- [ ] Run comprehensive global audit script
- [ ] Document remaining globals (if any)
- [ ] Justify why remaining globals are acceptable
- [ ] Test: Full test suite (unit + integration + stress)
- [ ] Deliverable: **PR #13 - Final Global State Audit**

**Week 18: Documentation & Milestone**
- [ ] Update all ADRs with "Implemented" status
- [ ] Create comprehensive architecture documentation
- [ ] Document lessons learned
- [ ] Celebrate: **GLOBAL STATE ELIMINATION COMPLETE** 🎉
- [ ] Deliverable: **GLOBAL_STATE_ELIMINATION_COMPLETE.md**

**Success Criteria (P2)**:
- ✅ No global mutable state in Frontier runtime (except justified shared state)
- ✅ All static buffers thread-safe or eliminated
- ✅ Comprehensive documentation of architecture
- ✅ **READY FOR COLLABORATIVE ODB (Phase 6+)**

---

## 4. UserTalk API Compatibility Analysis

### 4.1 Zero Breaking Changes

**All migrations maintain 100% backward compatibility** through:

1. **Macros for Thread-Local Globals**:
   ```c
   // Old code (before migration):
   currenthashtable = htable;  // Direct global access

   // New code (after migration):
   #define currenthashtable ((**hthreadglobals).currenthashtable)
   // Same syntax, different implementation - zero code changes
   ```

2. **Global Accessors for Context Structures**:
   ```c
   // Old code:
   hashtablelookup(efptable, name, &val, &node);

   // New code (Phase 1 - global accessor):
   #define efptable (get_system_context()->efptable)
   hashtablelookup(efptable, name, &val, &node);  // Same syntax

   // Future (Phase 2 - explicit parameter - OPTIONAL):
   hashtablelookup_context(sysctx, name, &val, &node);  // New API, old API still works
   ```

3. **Reference Counting Transparent to Existing Code**:
   ```c
   // Old code:
   oppushoutline(ho);
   opinsert("text", down);
   oppopoutline();

   // New code (refcount automatic):
   oppushoutline(ho);  // Internally calls op_outline_retain(ho)
   opinsert("text", down);
   oppopoutline();  // Internally calls op_outline_release(ho)
   ```

### 4.2 UserTalk Semantics Preserved

**Implicit "Current" Model Maintained**:

UserTalk scripts rely on implicit current objects:
- Current outline: `op.insert("text", down)` - operates on target outline
- Current hash table: `@x = 5` - assigns in current scope
- Current database: `db.save()` - saves current database

**Our migration preserves this**:
- Thread-local storage maintains "current X" per-thread
- UserTalk scripts see no difference (still implicit current model)
- Underneath: thread-local replaces global, but API identical

**No Script Changes Required**:
```usertalk
/* This UserTalk script works identically before and after migration */
lang.new(outlineType, @outline)
target.set(@outline)
op.insert("First Item", down)
op.insert("Second Item", down)
return op.countlines()  /* Returns 3 (empty summit + 2 items) */
```

### 4.3 Migration Path for Future Breaking Changes (If Needed)

**If explicit context passing becomes necessary** (Phase 6+ decision):

1. **Add new API, keep old API**:
   ```c
   // Old API (deprecated but still works):
   boolean opinsert(bigstring bs, short dir);

   // New API (explicit context):
   boolean opinsert_context(op_context *ctx, bigstring bs, short dir);

   // Old API implementation (calls new API with default context):
   boolean opinsert(bigstring bs, short dir) {
       return opinsert_context(get_default_op_context(), bs, dir);
   }
   ```

2. **Gradual deprecation** (years, not months):
   - Year 1: New API available, old API works
   - Year 2: Warnings on old API usage
   - Year 3+: Consider removing old API (only if strong justification)

3. **Migration tool for UserTalk scripts** (if needed):
   ```bash
   # Hypothetical future tool
   ./tools/migrate_usertalk_scripts.py scripts/*.txt
   # Converts old API calls to new API where possible
   ```

**Decision**: Explicit context passing is **OPTIONAL** and deferred to Phase 6+ requirements analysis. Current plan (thread-local + global accessors) maintains full compatibility indefinitely.

---

## 5. Risk Assessment & Mitigation

### 5.1 Technical Risks

**Risk 1: Thread Swap Functions Incomplete**
- **Probability**: Medium
- **Impact**: High (thread state corruption)
- **Mitigation**:
  - Use ADR-005/ADR-006 template (proven pattern)
  - Comprehensive testing after each migration
  - Thread sanitizer to detect issues
  - Code review checklist for swap functions

**Risk 2: Forgot to Migrate a Global**
- **Probability**: High (200+ globals)
- **Impact**: Medium (race condition in specific path)
- **Mitigation**:
  - Comprehensive audit script (automated detection)
  - Phase-by-phase verification (don't migrate all at once)
  - Stress testing with thread sanitizer
  - Code review for each PR

**Risk 3: Performance Degradation**
- **Probability**: Low
- **Impact**: Medium
- **Mitigation**:
  - Thread-local access is fast (TLS optimized by compilers)
  - Macros inline to direct access (no overhead at -O2)
  - Benchmark before/after each phase
  - Profile hot paths if issues detected

**Risk 4: Reference Counting Leaks or Cycles**
- **Probability**: Medium
- **Impact**: Medium (memory leaks)
- **Mitigation**:
  - Start with simple refcount (no weak refs initially)
  - Comprehensive lifecycle testing
  - Memory leak detection tools
  - Document ownership patterns clearly

**Risk 5: Breaking Legacy Mac GUI**
- **Probability**: Low (macros maintain compatibility)
- **Impact**: High (can't ship Mac GUI)
- **Mitigation**:
  - Test both headless and Mac GUI builds
  - Use `#ifndef FRONTIER_HEADLESS` for GUI-specific globals
  - Mac GUI testing after each phase
  - Coordinate with Mac GUI developer if available

### 5.2 Timeline Risks

**Risk 1: Phase Dependencies Block Progress**
- **Probability**: Medium
- **Impact**: High (delays launch)
- **Mitigation**:
  - Phases designed to be independent where possible
  - P0a-P0b can proceed in parallel with different developers
  - P1 can start before P0b complete (different code areas)
  - Document dependencies clearly

**Risk 2: Testing Takes Longer Than Expected**
- **Probability**: Medium
- **Impact**: Medium (extends timeline)
- **Mitigation**:
  - Allocate 20% buffer time per phase
  - Automate testing (stress tests, sanitizers)
  - Parallelize testing across phases
  - Use /gate for quick pre-merge review feedback

**Risk 3: Scope Creep (More Globals Discovered)**
- **Probability**: High
- **Impact**: Medium (extends timeline)
- **Mitigation**:
  - Comprehensive audit upfront (done in this plan)
  - Triage new discoveries as P0/P1/P2
  - Defer P2 work if needed to hit launch
  - Document discoveries in tracking sheet

### 5.3 Compatibility Risks

**Risk 1: UserTalk Scripts Break**
- **Probability**: Low (macros maintain API)
- **Impact**: Critical (can't ship)
- **Mitigation**:
  - Comprehensive integration tests
  - Test real-world UserTalk scripts
  - Backward compatibility checklist for each PR
  - Zero tolerance for API breaks without justification

**Risk 2: Third-Party Code Dependencies**
- **Probability**: Low (limited external code)
- **Impact**: Medium
- **Mitigation**:
  - Audit external code dependencies
  - Test external code after each phase
  - Document any external API changes
  - Provide migration guide if needed

**Risk 3: Database Format Compatibility**
- **Probability**: Low
- **Impact**: High (can't load old databases)
- **Mitigation**:
  - Don't change serialization formats
  - Reference counting is in-memory only (not persisted)
  - Test database loading/saving after each phase
  - Migration tests with v6/v7 databases

---

## 6. Success Metrics & Verification

### 6.1 Phase Completion Criteria

**P0a Complete** (Week 3):
- [ ] No direct global access to `currenthashtable`, parser state, control flow
- [ ] All globals accessed via thread-local macros
- [ ] Full test suite passes
- [ ] Thread sanitizer clean
- [ ] PR reviews complete

**P0b Complete** (Week 6):
- [ ] System context structure implemented
- [ ] All system tables in context (no ADR-008 workaround)
- [ ] Shell configuration thread-safe
- [ ] Full test suite passes
- [ ] **LAUNCH READY** milestone

**P1a Complete** (Week 9):
- [ ] Reference counting implemented for outlines, hash tables, system context
- [ ] Retain/release lifecycle working
- [ ] No dangling pointer bugs
- [ ] Full test suite passes

**P1b Complete** (Week 12):
- [ ] Concurrent stress tests pass (10+ threads)
- [ ] Thread sanitizer clean under load
- [ ] Lock state design documented
- [ ] **MULTI-USER READY** milestone

**P2 Complete** (Week 18):
- [ ] No global mutable state (except justified shared state)
- [ ] All static buffers thread-safe
- [ ] Comprehensive architecture documentation
- [ ] **GLOBAL STATE ELIMINATION COMPLETE** milestone

### 6.2 Testing Strategy

**Unit Tests** (Run after each PR):
```bash
./tools/run_headless_tests.sh
```
- Verify basic functionality (verb execution, outline ops, hash tables)
- Test thread-local state isolation
- Test reference counting lifecycle

**Integration Tests** (Run after each PR):
```bash
cd tests && make test-integration
```
- 600+ YAML-based verb tests
- Real-world UserTalk script execution
- Database loading/saving
- Processor resolution

**Stress Tests** (Run after P1a):
```bash
./tests/stress_test_concurrent
```
- 10+ threads executing UserTalk simultaneously
- Concurrent outline operations
- Concurrent hash table access
- Memory leak detection

**Thread Sanitizer** (Run after each phase):
```bash
make TSAN=1 && ./tests/run_all_tests
```
- Detect race conditions
- Detect data races
- Verify thread-safety

**Performance Benchmarks** (Run after each phase):
```bash
./tests/benchmark_suite
```
- Compare before/after each phase
- Verify no regression (< 5% slowdown acceptable)
- Profile hot paths if issues detected

### 6.3 Verification Checklist (Per PR)

**Code Review**:
- [ ] ADR-005/ADR-006 pattern followed (thread-local)
- [ ] Thread swap functions updated (copythreadglobals, swapinthreadglobals, newthreadglobals)
- [ ] Backward-compatible macros defined
- [ ] Global declarations removed
- [ ] No new global mutable state introduced

**Testing**:
- [ ] Unit tests pass
- [ ] Integration tests pass
- [ ] No race conditions (thread sanitizer)
- [ ] No memory leaks (leak detector)
- [ ] No performance regression (< 5%)

**Documentation**:
- [ ] Code comments reference relevant ADR
- [ ] CLAUDE.md updated if pattern changes
- [ ] Migration notes documented
- [ ] Breaking changes documented (if any)

---

## 7. Documentation & Knowledge Capture

### 7.1 ADRs to Create/Update

**New ADRs**:
- [ ] **ADR-009**: System Context Lifecycle Management
- [ ] **ADR-010**: Reference Counting for Multi-User Operations
- [ ] **ADR-011**: Thread-Local vs Explicit Context Decision Framework

**Update Existing ADRs**:
- [ ] **ADR-005**: Mark as "Template for Thread-Local Migration"
- [ ] **ADR-006**: Mark as "Implemented - Outline Context"
- [ ] **ADR-008**: Mark as "Superseded by ADR-009" (remove workaround)

### 7.2 Documentation to Create

**Architecture Docs**:
- [ ] `docs/MULTI_USER_ARCHITECTURE.md` - Multi-user design
- [ ] `docs/REFERENCE_COUNTING_GUIDE.md` - Refcount lifecycle patterns
- [ ] `docs/SYSTEM_CONTEXT_GUIDE.md` - System context usage
- [ ] `docs/THREAD_SAFETY_GUARANTEES.md` - What's safe, what's not

**Developer Guides**:
- [ ] `docs/MIGRATING_GLOBALS_GUIDE.md` - Step-by-step migration template
- [ ] `docs/THREAD_LOCAL_PATTERNS.md` - Expand ADR-005 template
- [ ] `docs/CONTEXT_PATTERNS.md` - When to use explicit context

**Update Existing Docs**:
- [ ] `CLAUDE.md` - Update "BURN THE GLOBALS WITH FIRE" section
- [ ] `CLAUDE.md` - Add references to new ADRs
- [ ] `docs/TESTING_GUIDE.md` - Add concurrent testing section

### 7.3 Tracking & Progress Reporting

**GitHub Issues**:
- [ ] Create umbrella issue: "Global State Elimination (P0-P2)"
- [ ] Create phase issues: P0a, P0b, P1a, P1b, P2
- [ ] Create PR issues for each week's deliverables
- [ ] Use labels: `P0-launch-blocking`, `P1-multi-user`, `P2-cleanup`, `global-state`

**Progress Dashboard**:
Create `planning/GLOBAL_STATE_PROGRESS.md`:
```markdown
# Global State Elimination Progress

**Last Updated**: 2026-01-XX

## Phase P0a (Weeks 1-3): Critical Thread-Safety
- [x] Week 1: Hash Table Context ✅ (PR #XXX)
- [ ] Week 2: Parser State (In Progress)
- [ ] Week 3: Control Flow & Error State

## Phase P0b (Weeks 4-6): Launch Requirements
- [ ] Week 4: System Context Structure
- [ ] Week 5: System Table Migration
- [ ] Week 6: Shell Config & Dialog State

... etc
```

---

## 8. Alternatives Considered & Rejected

### 8.1 C11 Thread-Local Storage (_Thread_local) Everywhere

**Description**: Use C11 `_Thread_local` keyword for all per-thread globals instead of `tythreadglobals` structure.

```c
_Thread_local hdlhashtable currenthashtable = nil;
_Thread_local hdltreenode yylval = nil;
// ... etc
```

**Rejected Because**:
- ❌ Diverges from existing `tythreadglobals` pattern (ADR-005, ADR-006)
- ❌ Harder to inspect thread state (no central structure)
- ❌ Less portable (MSVC, older GCC support issues)
- ❌ Can't pass thread state to debugger as single object
- ✅ **Decision**: Use `tythreadglobals` for consistency, reserve `_Thread_local` for mode stacks (already done in db_format.c)

### 8.2 Lock Everything (Don't Eliminate Globals)

**Description**: Keep global mutable state, protect with locks/mutexes.

```c
pthread_mutex_t global_state_lock;

void set_currenthashtable(hdlhashtable ht) {
    pthread_mutex_lock(&global_state_lock);
    currenthashtable = ht;
    pthread_mutex_unlock(&global_state_lock);
}
```

**Rejected Because**:
- ❌ Doesn't solve contamination bugs (cross-call state leakage)
- ❌ Serializes all operations (no true concurrency)
- ❌ Deadlock risk (nested locks, complex call graphs)
- ❌ Performance overhead (every access requires lock)
- ❌ Doesn't enable multi-user (still single global state)
- ✅ **Decision**: Eliminate globals, don't lock them

### 8.3 Explicit Context Parameters Everywhere (Option 1 from ADR-006)

**Description**: Add explicit context parameters to ALL functions.

```c
boolean opinsert(op_context *ctx, bigstring bs, short dir);
boolean hashtablelookup(system_context *sysctx, hdlhashtable ht, ...);
// ... etc for 1000+ functions
```

**Rejected Because**:
- ❌ Massive API break (10,000+ LOC changes)
- ❌ Breaks UserTalk verb signatures
- ❌ Thread-local achieves same goal with less churn
- ❌ Proven overkill (db_context pattern had issues, see CLAUDE.md)
- ✅ **Decision**: Use thread-local for per-thread state, explicit context only for per-operation state (system tables)

### 8.4 Do Nothing (Keep Status Quo)

**Description**: Accept global mutable state as acceptable risk.

**Rejected Because**:
- ❌ Blocks launch (thread-safety required for Automattic partnership)
- ❌ Blocks collaborative ODB (Frontier 2.0 strategic goal)
- ❌ Contamination bugs continue (flnextparamislast, outline push/pop)
- ❌ Technical debt accumulates (workarounds like ADR-008)
- ✅ **Decision**: User explicitly said "BURN THE GLOBALS WITH FIRE" - proper fix required

---

## 9. Open Questions & Decisions Needed

### 9.1 Phase P0 Decisions

**Q1**: Should `builtinstable`, `hkeywordtable`, `hbuiltinfunctions` be migrated?
- **Current**: Treated as read-only after initialization
- **Risk**: If modified at runtime, race conditions
- **Recommendation**: Audit for runtime modifications, migrate if found
- **Decision**: Defer to Week 2 audit

**Q2**: Are GUI-only globals safe to skip?
- **Current**: 40+ GUI globals not in headless builds
- **Risk**: If Mac GUI becomes multi-threaded, need migration
- **Recommendation**: Skip for P0, document as "GUI-only, thread-unsafe"
- **Decision**: User decision needed

**Q3**: Should `databasedata` global be eliminated entirely?
- **Current**: Partially fixed with `db_context` pattern, but global still exists
- **Risk**: Database loading still uses global (ADR-008 workaround)
- **Recommendation**: Eliminate in P0b (Week 5) as part of system context
- **Decision**: **YES** - include in system_context

### 9.2 Phase P1 Decisions

**Q4**: Should reference counting use atomics or locks?
- **Options**:
  - A) `_Atomic uint32_t` (C11 atomics)
  - B) `pthread_mutex_t` (locks)
- **Recommendation**: Atomics (less overhead, no deadlock risk)
- **Decision**: **Atomics** (requires C11, already used in codebase)

**Q5**: Should weak references be implemented?
- **Use case**: Break reference cycles (outline→table→outline)
- **Complexity**: Adds significant implementation complexity
- **Recommendation**: Defer to Phase 6+ (if cycles become problem)
- **Decision**: Defer

**Q6**: Should system_context be per-thread or per-operation?
- **Per-thread**: Each thread has own system context
- **Per-operation**: System context passed explicitly
- **Recommendation**: Start per-thread (Phase 1), evaluate per-operation (Phase 2)
- **Decision**: **Per-thread initially** with option to add explicit passing

### 9.3 Phase P2 Decisions

**Q7**: Should static buffers be eliminated or thread-local?
- **Options**:
  - A) Eliminate (dynamic allocation every time)
  - B) Thread-local (static per-thread)
- **Trade-off**: Performance vs simplicity
- **Recommendation**: Thread-local for hot paths, dynamic elsewhere
- **Decision**: Case-by-case during audit

**Q8**: What is the definition of "justified shared state"?
- **Examples**: Built-in function tables (read-only), locks/mutexes (inherently shared)
- **Recommendation**: Document in ADR-011 decision framework
- **Decision**: Create checklist in Week 17

---

## 10. Lessons Learned (To Be Updated)

### 10.1 From ADR-005 (Parameter State Migration)

**What Worked**:
- ✅ Thread-local pattern simple and effective
- ✅ Macros provide zero-cost backward compatibility
- ✅ Single PR, low risk, high value

**What Didn't**:
- ⚠️ Forgot to initialize new fields in newthreadglobals() initially
- ⚠️ Thread swap functions easy to forget (copy/paste errors)

**Applied to This Plan**:
- Use checklist for thread swap functions
- Test initialization thoroughly (unit tests for newthreadglobals)

### 10.2 From ADR-006 (Outline Context Migration)

**What Worked**:
- ✅ Thread-local pattern scales to larger migrations (154 call sites)
- ✅ No API breaks, existing code continues working
- ✅ Push/pop pattern thread-safe without eliminating it

**What Didn't**:
- ⚠️ Shell globals discovered late (should have audited upfront)
- ⚠️ GUI-only globals ambiguous (headless vs Mac GUI)

**Applied to This Plan**:
- Comprehensive audit upfront (this document)
- Clear marking of GUI-only vs headless globals

### 10.3 From ADR-008 (Processor Table Workaround)

**What Worked**:
- ✅ Workaround unblocked development quickly
- ✅ Clear documentation of temporary nature

**What Didn't**:
- ❌ Workaround added complexity (another global!)
- ❌ Should have fixed root cause instead of patching

**Applied to This Plan**:
- No more workarounds - proper fixes only
- P0b explicitly removes ADR-008 workaround

### 10.4 To Be Learned (Updated After Each Phase)

This section will be updated after each phase completion with:
- Unexpected issues encountered
- Solutions that worked well
- Anti-patterns to avoid
- Recommendations for future similar work

---

## 11. Appendices

### Appendix A: Global Variable Inventory (Full List)

**See Section 1 for complete categorized inventory** (P0, P1, P2 globals with file locations, types, and migration status).

### Appendix B: Thread Swap Function Template

**See ADR-005 and docs/THREAD_LOCAL_GLOBALS_PATTERN.md** for step-by-step template.

### Appendix C: Reference Implementation Examples

**Hash Table Context Migration** (Example from P0a Week 1):
```c
// 1. Add to tythreadglobals (processinternal.h)
typedef struct tythreadglobals {
    // ... existing fields ...
    hdlhashtable currenthashtable;
    hdlhashtable hmagictable;
    hdltablestack hashtablestack;
} tythreadglobals;

// 2. Macros (processinternal.h)
#define currenthashtable ((**hthreadglobals).currenthashtable)
#define hmagictable ((**hthreadglobals).hmagictable)
#define hashtablestack ((**hthreadglobals).hashtablestack)

// 3. Update copythreadglobals() (process.c)
(**hg).currenthashtable = currenthashtable;
(**hg).hmagictable = hmagictable;
(**hg).hashtablestack = hashtablestack;

// 4. Update swapinthreadglobals() (process.c)
currenthashtable = (**hg).currenthashtable;
hmagictable = (**hg).hmagictable;
hashtablestack = (**hg).hashtablestack;

// 5. Update newthreadglobals() (process.c)
(**hg).currenthashtable = nil;
(**hg).hmagictable = nil;
(**hg).hashtablestack = nil;

// 6. Remove global declarations (lang.h, langhash.c)
// extern hdlhashtable currenthashtable;  <- DELETE
```

### Appendix D: Testing Checklist Template

**Per-PR Testing Checklist**:
```markdown
## Testing Checklist - PR #XXX (Phase P0a Week 1)

### Unit Tests
- [ ] Run `./tools/run_headless_tests.sh` - PASS
- [ ] All runtime tests pass
- [ ] New thread-local state tests pass

### Integration Tests
- [ ] Run `cd tests && make test-integration` - PASS
- [ ] All verb tests pass (600+ tests)
- [ ] No regression in existing behavior

### Thread Safety
- [ ] Run with thread sanitizer: `make TSAN=1 && ./tests/run_all_tests`
- [ ] No race conditions detected
- [ ] No data races reported

### Performance
- [ ] Run benchmarks: `./tests/benchmark_suite`
- [ ] No regression > 5%
- [ ] Hot paths profiled if regression detected

### Code Review
- [ ] ADR-005/ADR-006 pattern followed
- [ ] Thread swap functions updated (3 locations)
- [ ] Backward-compatible macros defined
- [ ] Global declarations removed
- [ ] No new globals introduced

### Documentation
- [ ] Code comments reference ADR
- [ ] CLAUDE.md updated if needed
- [ ] Migration notes in PR description
```

---

## 12. Summary & Next Steps

### 12.1 What This Plan Achieves

**P0 (Launch)**: Thread-safe runtime
- No race conditions on critical globals
- Multiple threads can execute UserTalk concurrently
- Foundation for Automattic partnership

**P1 (Multi-User)**: Collaborative ODB foundation
- Reference counting for shared objects
- Context lifecycle management
- Ready for Phase 6+ collaborative features

**P2 (Complete)**: Zero global mutable state
- All globals eliminated or justified
- Comprehensive architecture documentation
- Clean foundation for future development

### 12.2 Immediate Next Steps

**Week 1 (Starting 2026-01-XX)**:
1. **User Review**: User reviews this plan, approves approach
2. **Create Tracking Issues**: GitHub issues for each phase
3. **Create Feature Branch**: `feature/global-state-elimination-p0a`
4. **Start P0a Week 1**: Hash table context migration (see Section 3)

**Week 2-3**: Continue P0a execution
**Week 4-6**: P0b execution (launch readiness)
**Week 7+**: P1 and P2 execution (multi-user + cleanup)

### 12.3 User Decision Points

**Immediate Decisions Needed**:
- [ ] Approve overall plan and timeline
- [ ] Approve phased approach (P0→P1→P2)
- [ ] Decide on GUI-only globals (skip or migrate?)
- [ ] Approve thread-local + explicit context architecture

**Phase-Specific Decisions** (deferred to phase start):
- P0b: System context design review (Week 4)
- P1a: Reference counting implementation review (Week 7)
- P2: Final global audit results review (Week 17)

### 12.4 Success Criteria (Restated)

**This plan succeeds when**:
- ✅ Frontier runtime is thread-safe (P0 milestone)
- ✅ Multiple users can operate on ODB concurrently (P1 milestone)
- ✅ No global mutable state blocks collaborative ODB (P2 milestone)
- ✅ Zero UserTalk API breaks (backward compatibility maintained)
- ✅ Comprehensive documentation enables future development

**Time to BURN THE GLOBALS WITH FIRE!** 🔥

---

**Document Version**: 1.0
**Last Updated**: 2026-01-13
**Next Review**: After P0a Week 1 completion
