# Phase 4 Overview: Architecture & Patterns

**Purpose**: Common architecture patterns and resources used across all sub-phases

---

## The Mission

**Eliminate ALL global mutable state from Frontier runtime**

**Why**:
- Thread-safety required for launch (Automattic partnership)
- Foundation for collaborative ODB (Frontier 2.0)
- Eliminate contamination bugs from shared global state

---

## The Numbers

| Metric | Count |
|--------|-------|
| **Total Globals Found** | 200+ |
| **P0 (Launch-Blocking)** | 55 globals |
| **P1 (Multi-User)** | 25 globals |
| **P2 (Cleanup)** | 120+ globals |
| **Files to Modify** | ~60 files |
| **Estimated Timeline** | 12-18 weeks |

---

## The Two Proven Patterns

### Pattern 1: Thread-Local Storage (Primary)

**Use for**: Per-thread execution state (scope, parser, control flow, errors)

**How it works**:
```c
// Add to tythreadglobals structure
typedef struct tythreadglobals {
    hdlhashtable currenthashtable;  // Variable scope
    hdltreenode yylval;              // Parser state
    boolean flbreak;                 // Control flow
} tythreadglobals;

// Backward-compatible macros (zero API changes)
#define currenthashtable ((**hthreadglobals).currenthashtable)
#define yylval ((**hthreadglobals).yylval)
#define flbreak ((**hthreadglobals).flbreak)
```

**Benefits**:
- ✅ Zero API changes (macros provide transparency)
- ✅ Proven in ADR-005 (parameter state) and ADR-006 (outline context)
- ✅ Simple to implement (3 files modified per migration)
- ✅ Automatic thread isolation

**When to use**: State logically "owned" by a thread's execution context.

---

### Pattern 2: Explicit Context Structures (Secondary)

**Use for**: Per-operation/session state (system tables, databases)

**How it works**:
```c
// System context (all system tables)
typedef struct system_context {
    hdlhashtable roottable;
    hdlhashtable systemtable;
    hdlhashtable efptable;  // Processor tables
    // ... 20+ system tables
    _Atomic uint32_t refcount;
} system_context;

// Phase 1: Global accessor (backward compatible)
system_context* get_system_context(void);
#define roottable (get_system_context()->roottable)

// Phase 2: Thread-local caching (optional)
_Thread_local system_context* tls_system_context = NULL;

// Phase 3: Explicit parameter (future, optional)
langfunctionvalue(system_context *ctx, ...);
```

**Benefits**:
- ✅ Clear ownership and lifecycle
- ✅ Enables multi-user (different contexts per user)
- ✅ Reference counting prevents premature disposal
- ✅ Gradual migration path

**When to use**: State representing a "session" or "operation scope".

---

### Pattern 3: Reference Counting (Phase P1)

**Use for**: Objects that outlive their creating thread or are shared

**How it works**:
```c
typedef struct tyoutlinerecord {
    // ... existing fields ...
    _Atomic uint32_t refcount;
} tyoutlinerecord;

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
            opdisposeoutline(ho);  // Last reference
        }
    }
}
```

**Apply to**:
- Outline objects (tyoutlinerecord)
- Hash tables (tyhashtable)
- System context (system_context)
- Database handles (hdldatabaserecord)

**Benefits**:
- ✅ Thread-safe object sharing
- ✅ Prevents dangling pointers
- ✅ Foundation for collaborative ODB

---

## Migration Decision Tree

```
Is this global per-thread execution state?
  (scope, parser, control flow, errors)
  ├─ YES → Use Pattern 1 (Thread-Local Storage)
  └─ NO ──→ Is it per-session or shared state?
            (system tables, database connections)
            ├─ YES → Use Pattern 2 (Explicit Context)
            └─ NO ──→ Is it read-only after init?
                      (built-in tables, configuration)
                      ├─ YES → Document as "read-only shared state"
                      └─ NO ──→ Audit carefully, likely needs Pattern 1 or 2
```

---

## Zero Breaking Changes Guarantee

**All migrations maintain 100% backward compatibility**:

1. **Macros for thread-local globals** - Same syntax, different storage
2. **Global accessors for contexts** - Transparent to existing code
3. **Reference counting automatic** - Existing APIs work unchanged

**UserTalk scripts work identically before and after migration.**

---

## Documentation in This Directory

- **[quick_reference.md](quick_reference.md)** - Developer checklist (from docs/)
- **[patterns.md](patterns.md)** - Detailed pattern implementations
- **[risks.md](risks.md)** - Risk assessment and mitigation
- **[api_compatibility.md](api_compatibility.md)** - UserTalk compatibility analysis
- **[alternatives.md](alternatives.md)** - Rejected approaches and why
- **[tracking_template.md](tracking_template.md)** - Progress tracking template

---

## Key Principles

1. **Proven Patterns Only** - ADR-005 and ADR-006 already validated
2. **Zero API Breaks** - Macros and accessors maintain compatibility
3. **Gradual Migration** - Phase by phase, not all at once
4. **Comprehensive Testing** - Unit + integration + thread sanitizer + stress
5. **Clear Documentation** - Every migration references an ADR

---

**See**: [../INDEX.md](../INDEX.md) for sub-phase navigation
