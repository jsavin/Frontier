# Global State Audit: Thread-Safety for Collaborative ODB

**Purpose**: Identify ALL global mutable state that violates thread-safety requirements for collaborative ODB editing (Phase 6+).

**Status**: Initial audit (2026-01-02)
**Pattern Established**: ADR-005 (thread-local storage migration)
**Migration Template**: docs/THREAD_LOCAL_GLOBALS_PATTERN.md

---

## Executive Summary

**Critical Finding**: Frontier has 20+ global mutable variables that will cause race conditions in multi-threaded collaborative ODB environment.

**Recommended Action**: Migrate all per-thread execution state to `tythreadglobals` using ADR-005 pattern.

**Timeline**:
- **Phase 3 (Immediate)**: Migrate parameter-related globals (ADR-005)
- **Phase 4-5**: Migrate remaining execution state globals
- **Phase 6**: Verify thread-safety before enabling collaborative ODB

---

## Categorization Framework

### Category 1: Thread-Local Execution State (MIGRATE)
Per-thread state that must be isolated across concurrent threads.

**Migration Pattern**: ADR-005 (add to `tythreadglobals`)

**Examples**:
- `flnextparamislast` - Parameter flag (affects verb dispatch)
- `flscriptrunning` - Thread execution state (already migrated)
- `currentprocess` - Active process handle (already migrated)

---

### Category 2: Explicit Context (ALREADY HANDLED)
Per-operation state passed via context parameters.

**Pattern**: `db_context`, `op_context` (ADR-001, ADR-002, ADR-004)

**Examples**:
- Database operations → `db_context *ctx`
- Outline operations → `op_context_t *ctx`

---

### Category 3: Shared Mutable State (REQUIRES LOCKING)
Truly shared data that MUST be the same across all threads.

**Pattern**: Add locks/mutexes, or convert to ref-counted handles

**Examples**:
- Caches (if truly shared)
- Global lookup tables (if mutable)

---

### Category 4: Read-Only After Init (OK)
Globals initialized once at startup, never modified.

**Pattern**: No migration needed, but document as immutable

**Examples**:
- `hbuiltinfunctions` - Built-in function table
- Configuration constants

---

## Detailed Audit Results

### Parameter Handling Globals (langvalue.c)

**Status**: **PHASE 3 - IMMEDIATE MIGRATION (ADR-005)**

| Global | Type | Category | Migration Status | Notes |
|--------|------|----------|-----------------|-------|
| `flnextparamislast` | boolean | Thread-Local | ⏳ ADR-005 | Parameter flag persistence bug |
| `flparamerrorenabled` | boolean | Thread-Local | ⏳ ADR-005 | Error control flag |
| `flcoerceexternaltostring` | boolean | Thread-Local | ⏳ ADR-005 | Type coercion control |
| `flinhibitnilcoercion` | boolean | Thread-Local | ⏳ ADR-005 | Type coercion control |
| `fllocaldotparamsonly` | boolean | Thread-Local | ⏳ ADR-005 | Lookup control (static) |
| `bsfunctionname` | bigstring | Thread-Local | ⏳ ADR-005 | Error message state |
| `functiontoken` | tyfunctype | Thread-Local | ⏳ ADR-005 | Error message state (static) |

**Impact**: HIGH - Affects ALL verb processors (48+ files)
**Risk**: Cross-call contamination (confirmed bug), race conditions in multi-user

---

### Value Protection Globals (lang.h)

**Status**: **PHASE 3 - IMMEDIATE MIGRATION (ADR-005)**

| Global | Type | Category | Migration Status | Notes |
|--------|------|----------|-----------------|-------|
| `fllanghashassignprotect` | boolean | Thread-Local | ⏳ ADR-005 | Hash assignment guard |
| `fllangexternalvalueprotect` | boolean | Thread-Local | ⏳ ADR-005 | External value guard |

**Impact**: MEDIUM - Affects hash table and external value operations
**Risk**: Race conditions during concurrent hash/external value ops

---

### Script Execution Globals (lang.h, process.c)

**Status**: **✅ ALREADY MIGRATED**

| Global | Type | Category | Migration Status | Notes |
|--------|------|----------|-----------------|-------|
| `flscriptrunning` | boolean | Thread-Local | ✅ DONE | Already in tythreadglobals (line 1456) |
| `flscriptresting` | boolean | Thread-Local | ✅ DONE | Already in tythreadglobals (line 1458) |
| `currentprocess` | hdlprocessrecord | Thread-Local | ✅ DONE | Already in tythreadglobals (line 1442) |
| `currenthashtable` | hdlhashtable | Thread-Local | ✅ DONE | Already in tythreadglobals (line 1444) |

**Impact**: N/A - Already handled correctly
**Risk**: None

---

### Outline Context Globals (opinternal.h, process.c)

**Status**: **✅ ALREADY MIGRATED**

| Global | Type | Category | Migration Status | Notes |
|--------|------|----------|-----------------|-------|
| `outlinedata` | hdloutlinerecord | Thread-Local | ✅ DONE | Already in tythreadglobals (line 1469) |
| `outlinestack` | hdloutlinerecord[] | Thread-Local | ✅ DONE | Already in tythreadglobals (line 1473) |
| `topoutlinestack` | short | Thread-Local | ✅ DONE | Already in tythreadglobals (line 1471) |

**Impact**: N/A - Already handled correctly
**Risk**: None (but see Issue #135 for Phase 6+ context refactoring)

---

### Error Handling Globals (lang.h, process.c)

**Status**: **MIXED - Some migrated, some pending**

| Global | Type | Category | Migration Status | Notes |
|--------|------|----------|-----------------|-------|
| `fllangerror` | boolean | Thread-Local | ✅ DONE | Already in tythreadglobals (line 1491) |
| `langerrordisable` | ushort | Thread-Local | ✅ DONE | Already in tythreadglobals (line 1493) |
| `herrornode` | hdltreenode | Thread-Local | ✅ DONE | Already in tythreadglobals (line 1483) |
| `tryerror` | Handle | Thread-Local | ✅ DONE | Already in tythreadglobals (line 1495) |
| `tryerrorstack` | Handle | Thread-Local | ✅ DONE | Already in tythreadglobals (line 1497) |
| `errorhooks[]` | callback[] | Thread-Local | ✅ DONE | Already in tythreadglobals (line 1462) |
| `cterrorhooks` | short | Thread-Local | ✅ DONE | Already in tythreadglobals (line 1460) |

**Impact**: N/A - Already handled correctly
**Risk**: None

---

### Database Globals (db.c, langdb.c)

**Status**: **✅ ALREADY HANDLED (Explicit Context Pattern)**

| Global | Type | Category | Migration Status | Notes |
|--------|------|----------|-----------------|-------|
| `databasedata` | db_context | Explicit Context | ✅ DONE | ADR-001, ADR-002 (context parameter) |
| Database file handles | varies | Explicit Context | ✅ DONE | Managed via db_context pattern |

**Impact**: N/A - Already handled via explicit context (not thread-local)
**Risk**: None (context guards proven correct in PR #185)

---

### Built-In Function Table (langvalue.c)

**Status**: **✅ READ-ONLY AFTER INIT**

| Global | Type | Category | Migration Status | Notes |
|--------|------|----------|-----------------|-------|
| `hbuiltinfunctions` | hdlhashtable | Read-Only | ✅ OK | Initialized once, never modified |

**Impact**: N/A - Read-only shared data
**Risk**: None (immutable after initialization)

---

### Shell/Window Globals (process.c, shellprivate.h)

**Status**: **✅ ALREADY MIGRATED**

| Global | Type | Category | Migration Status | Notes |
|--------|------|----------|-----------------|-------|
| `shellwindow` | WindowPtr | Thread-Local | ✅ DONE | Already in tythreadglobals (line 1464) |
| `pmodaldialog` | DialogPtr | Thread-Local | ✅ DONE | Already in tythreadglobals (field exists) |

**Impact**: N/A - Already handled correctly
**Risk**: None

---

### Threading Globals (process.c)

**Status**: **MIXED**

| Global | Type | Category | Migration Status | Notes |
|--------|------|----------|-----------------|-------|
| `flthreadkilled` | boolean | Thread-Local | ✅ DONE | Already in tythreadglobals (line 1446) |
| `fldisableyield` | ushort | Thread-Local | ✅ DONE | Already in tythreadglobals (line 1448) |
| `processstack` | typrocessstack | Thread-Local | ✅ DONE | Already in tythreadglobals (line 1452) |
| `globalsstack` | tyglobalsstack | Thread-Local | ✅ DONE | Already in tythreadglobals (line 1454) |
| `hthreadglobals` | hdlthreadglobals | Special | ✅ OK | Thread-local pointer to current thread |
| `processthreadlist` | hdlthreadlist | Shared | ⚠️ NEEDS LOCK | List of all threads (shared state) |

**Impact**: HIGH (processthreadlist) - Shared across all threads
**Risk**: Race conditions if threads added/removed concurrently
**Recommendation**: Add mutex around processthreadlist access

---

### Scan State Globals (process.c)

**Status**: **✅ ALREADY MIGRATED**

| Global | Type | Category | Migration Status | Notes |
|--------|------|----------|-----------------|-------|
| `ctscanlines` | ushort | Thread-Local | ✅ DONE | Already in tythreadglobals (line 1479) |
| `ctscanchars` | ushort | Thread-Local | ✅ DONE | Already in tythreadglobals (line 1481) |

**Impact**: N/A - Already handled correctly
**Risk**: None

---

### Control Flow Globals (process.c)

**Status**: **✅ ALREADY MIGRATED**

| Global | Type | Category | Migration Status | Notes |
|--------|------|----------|-----------------|-------|
| `flreturn` | boolean | Thread-Local | ✅ DONE | Already in tythreadglobals (line 1485) |
| `flbreak` | boolean | Thread-Local | ✅ DONE | Already in tythreadglobals (line 1487) |
| `flcontinue` | boolean | Thread-Local | ✅ DONE | Already in tythreadglobals (line 1489) |

**Impact**: N/A - Already handled correctly
**Risk**: None

---

### Time Tracking Globals (process.c)

**Status**: **✅ ALREADY MIGRATED**

| Global | Type | Category | Migration Status | Notes |
|--------|------|----------|-----------------|-------|
| `timestarted` | ulong | Thread-Local | ✅ DONE | Already in tythreadglobals (line 130) |
| `sleepticks` | ulong | Thread-Local | ✅ DONE | Already in tythreadglobals (line 132) |
| `timebeginsleep` | ulong | Thread-Local | ✅ DONE | Already in tythreadglobals (line 134) |
| `timetowake` | ulong | Thread-Local | ✅ DONE | Already in tythreadglobals (line 136) |
| `timeswappedin` | ulong | Thread-Local | ✅ DONE | Already in tythreadglobals (line 138) |
| `timesliceticks` | ulong | Thread-Local | ✅ DONE | Already in tythreadglobals (line 140) |

**Impact**: N/A - Already handled correctly
**Risk**: None

---

### COM Initialization (process.c)

**Status**: **✅ ALREADY MIGRATED**

| Global | Type | Category | Migration Status | Notes |
|--------|------|----------|-----------------|-------|
| `flcominitialized` | boolean | Thread-Local | ✅ DONE | Already in tythreadglobals (line 1503) |

**Impact**: N/A - Already handled correctly
**Risk**: None (Windows-specific COM state per thread)

---

## Unknown / Requires Further Audit

### Static Buffers in Verb Processors

Many verb processors have static buffers for temporary string manipulation.

**Example** (hypothetical):
```c
static char tempbuffer[256];

boolean someverb(...) {
    strcpy(tempbuffer, ...);  // ⚠️ Race condition if multi-threaded!
    // ... use tempbuffer ...
}
```

**Status**: ⚠️ REQUIRES FILE-BY-FILE AUDIT

**Files to Audit** (48+ files):
- Common/source/fileverbs.c
- Common/source/stringverbs.c
- Common/source/tableverbs.c
- Common/source/langverbs.c
- Common/source/wpverbs.c
- Common/source/shellsysverbs.c
- Common/source/pictverbs.c
- Common/source/opverbs.c
- Common/source/menuverbs.c
- Common/source/langpack.c
- Common/source/langhtml.c
- Common/source/langmodeless.c
- Common/source/langcrypt.c
- Common/source/langxml.c
- Common/source/langsystem7.c
- Common/source/langsystypes.c
- Common/source/langipc.c
- Common/source/dbverbs.c
- Common/source/langregexp.c
- Common/source/langmath.c
- Common/source/langmysql.c
- Common/source/langpython.c
- Common/source/langdll.c
- Common/source/cancoonverbs.c
- Common/source/langsqlite.c
- Common/source/base64.c
- Common/source/shellwindowverbs.c
- Common/source/shellverbs.c
- Common/source/langdialog.c
- Common/source/langquicktime.c
- portable/fileverbs_portable.c
- portable/stringverbs_portable.c
- portable/tableverbs_portable.c
- (etc.)

**Audit Strategy**:
1. Search for `static.*buffer\|static.*bs\|static.*str` in each file
2. Determine if buffer is:
   - Read-only (const) → OK
   - Written once at init → OK (but document)
   - Written per call → ⚠️ THREAD-SAFETY ISSUE
3. Migrate to thread-local or stack allocation as appropriate

**Estimated Effort**: 2-3 days (systematic grep + analysis)

---

### Cache and Lookup Tables

**Unknown globals** in subsystems like:
- `Common/source/search.c` - Search state
- `Common/source/tablecache.c` - Table caching
- `Common/source/lang*.c` - Various language subsystems

**Audit Strategy**:
1. Grep for `static.*h[a-z]*\|static.*fl[a-z]*` in Common/source/
2. Categorize each as thread-local, shared+locked, or read-only
3. Migrate per ADR-005 pattern

**Estimated Effort**: 1-2 days

---

## Migration Priority

### Phase 3 (Immediate - ADR-005)
**Target**: 2026-Q1

- [x] Document pattern (ADR-005, THREAD_LOCAL_GLOBALS_PATTERN.md)
- [ ] **Migrate parameter handling globals** (7 globals in langvalue.c)
- [ ] **Migrate value protection globals** (2 globals in lang.h)
- [ ] Test single-threaded backward compatibility
- [ ] Document thread-safety guarantees

**Success Criteria**:
- All parameter-related globals in `tythreadglobals`
- Zero API changes (backward-compatible macros)
- All tests pass
- Thread-safety verified (manual inspection)

---

### Phase 4 (Follow-up)
**Target**: 2026-Q1

- [ ] Audit static buffers in verb processors (48+ files)
- [ ] Migrate or document each static buffer
- [ ] Identify any cache/lookup table globals
- [ ] Add `processthreadlist` mutex (shared state protection)

**Success Criteria**:
- No unprotected static buffers in verb processors
- All shared mutable state has locks
- Documentation complete for all globals

---

### Phase 5 (Validation)
**Target**: 2026-Q2

- [ ] Stress test with concurrent script execution
- [ ] Thread sanitizer clean (no race conditions detected)
- [ ] Document thread-safety guarantees for verb implementers
- [ ] Update ADRs with "Verified" status

**Success Criteria**:
- Thread sanitizer clean
- 10+ concurrent threads executing verbs without failures
- All globals categorized and documented

---

### Phase 6 (Collaborative ODB)
**Target**: 2026-Q3+

- [ ] Verify thread-safety sufficient for multi-user editing
- [ ] Add per-user context fields (user_id, session_id) to `tythreadglobals`
- [ ] Implement CRDT-based conflict resolution
- [ ] Production-grade multi-user testing

**Success Criteria**:
- Multiple users can execute scripts concurrently
- No race conditions or data corruption
- Conflict resolution works correctly

---

## Metrics and Tracking

### Current State (2026-01-02)

**Total Globals Audited**: 50+

| Category | Count | Status |
|----------|-------|--------|
| Thread-Local (migrated) | 30+ | ✅ DONE |
| Thread-Local (pending) | 9 | ⏳ ADR-005 |
| Explicit Context | 5+ | ✅ DONE (ADR-001, ADR-002) |
| Read-Only After Init | 5+ | ✅ OK |
| Shared Mutable (needs locks) | 1 | ⚠️ TODO (processthreadlist) |
| Unknown (requires audit) | 50+ | ❓ AUDIT NEEDED |

**Thread-Safety Score**: ~60% (30 of 50 known globals thread-safe)

**Target for Phase 6**: 100% (all globals categorized and handled)

---

## References

- **ADR-005**: Parameter state thread-safety (pattern established)
- **docs/THREAD_LOCAL_GLOBALS_PATTERN.md**: Migration template
- **CLAUDE.md**: "BURN THE GLOBALS WITH FIRE" (strategic context)
- **processinternal.h**: `tythreadglobals` structure definition
- **process.c**: Thread swap function implementations (copythreadglobals, swapinthreadglobals)

---

## Next Actions

**Immediate (This PR)**:
1. Implement ADR-005 migration (parameter handling globals)
2. Test backward compatibility
3. Update this audit with "DONE" status

**Follow-up (Next PR)**:
1. Audit static buffers in verb processors (grep-based systematic review)
2. Categorize findings (thread-local vs stack-local vs shared)
3. Migrate or document each buffer

**Long-term (Phase 6)**:
1. Verify 100% thread-safety coverage
2. Stress test concurrent execution
3. Enable collaborative ODB with confidence
