# Global Migration Quick Reference Card

**For**: Developers implementing global state migrations
**See Also**:
- `planning/GLOBAL_STATE_ELIMINATION_PLAN.md` (full plan)
- `planning/GLOBAL_STATE_ELIMINATION_SUMMARY.md` (executive summary)
- ADR-005 (thread-local pattern template)
- ADR-006 (outline context example)

---

## Pattern 1: Thread-Local Storage Migration

**Use for**: Per-thread execution state (scope, parser, control flow, errors)

### 5-Step Template

#### Step 1: Add Field to tythreadglobals
```c
// File: Common/headers/processinternal.h
typedef struct tythreadglobals {
    // ... existing fields ...

    // [YOUR_FEATURE]: [DESCRIPTION]
    hdlhashtable currenthashtable;    // Variable scope resolution
    hdltreenode yylval;               // Parser token value
    boolean flbreak;                  // Break statement flag
} tythreadglobals;
```

#### Step 2: Create Backward-Compatible Macro
```c
// File: Common/headers/processinternal.h (after struct definition)
// OR: Common/headers/[feature].h (if feature-specific)

// [YOUR_FEATURE]: Thread-local [DESCRIPTION] (backward-compatible macro)
#define currenthashtable ((**hthreadglobals).currenthashtable)
#define yylval ((**hthreadglobals).yylval)
#define flbreak ((**hthreadglobals).flbreak)
```

#### Step 3: Update copythreadglobals()
```c
// File: Common/source/process.c (around line 1503)
void copythreadglobals(hdlthreadglobals hglobals) {
    // ... existing code ...

    // [YOUR_FEATURE]: Save state to thread globals
    (**hg).currenthashtable = currenthashtable;
    (**hg).yylval = yylval;
    (**hg).flbreak = flbreak;
}
```

#### Step 4: Update swapinthreadglobals()
```c
// File: Common/source/process.c (around line 1620)
void swapinthreadglobals(hdlthreadglobals hglobals) {
    // ... existing code ...

    // [YOUR_FEATURE]: Restore state from thread globals
    currenthashtable = (**hg).currenthashtable;
    yylval = (**hg).yylval;
    flbreak = (**hg).flbreak;
}
```

#### Step 5: Update newthreadglobals()
```c
// File: Common/source/process.c (inside newthreadglobals function)
boolean newthreadglobals(hdlthreadglobals *hglobals) {
    // ... existing code ...

    // [YOUR_FEATURE]: Initialize state
    (**hg).currenthashtable = nil;
    (**hg).yylval = nil;
    (**hg).flbreak = false;

    return true;
}
```

#### Step 6: Remove Global Declarations
```c
// File: Common/headers/[feature].h
// OLD - DELETE:
extern hdlhashtable currenthashtable;
extern hdltreenode yylval;
extern boolean flbreak;

// File: Common/source/[feature].c
// OLD - DELETE:
hdlhashtable currenthashtable = nil;
hdltreenode yylval = nil;
boolean flbreak = false;
```

#### Step 7: Update Headless Stubs (if needed)
```c
// File: tests/headless_threadglobals.c
void headless_init_threadglobals(void) {
    // ... existing code ...

    // [YOUR_FEATURE]: Initialize headless thread globals
    headless_threadglobals_data.currenthashtable = nil;
    headless_threadglobals_data.yylval = nil;
    headless_threadglobals_data.flbreak = false;
}
```

### Testing Checklist

- [ ] Run `./tools/run_headless_tests.sh` - PASS
- [ ] Run `cd tests && make test-integration` - PASS
- [ ] Run with thread sanitizer: `make TSAN=1 && ./tests/run_all_tests` - CLEAN
- [ ] No performance regression (< 5%)
- [ ] All 3 thread functions updated (copy, swapin, new)
- [ ] Backward-compatible macros defined
- [ ] Global declarations removed
- [ ] Code comments reference ADR

---

## Pattern 2: Explicit Context Structure

**Use for**: Per-operation/session state (system tables, databases, processors)

### 5-Step Template

#### Step 1: Define Context Structure
```c
// File: Common/headers/[feature]_context.h
typedef struct system_context {
    hdlhashtable roottable;
    hdlhashtable systemtable;
    hdlhashtable efptable;
    // ... all related state ...

    // Reference counting for multi-user (Phase P1)
    _Atomic uint32_t refcount;
} system_context;
```

#### Step 2: Create Lifecycle Functions
```c
// File: Common/headers/[feature]_context.h
system_context* system_context_create(void);
system_context* system_context_retain(system_context *ctx);
void system_context_release(system_context *ctx);

// File: Common/source/[feature]_context.c
system_context* system_context_create(void) {
    system_context *ctx = (system_context*)malloc(sizeof(system_context));
    if (ctx != NULL) {
        memset(ctx, 0, sizeof(system_context));
        atomic_init(&ctx->refcount, 1);
    }
    return ctx;
}

system_context* system_context_retain(system_context *ctx) {
    if (ctx != NULL) {
        atomic_fetch_add(&ctx->refcount, 1);
    }
    return ctx;
}

void system_context_release(system_context *ctx) {
    if (ctx != NULL) {
        uint32_t prev = atomic_fetch_sub(&ctx->refcount, 1);
        if (prev == 1) {
            // Last reference - dispose
            // Free resources...
            free(ctx);
        }
    }
}
```

#### Step 3: Global Accessor (Phase 1 - Backward Compatibility)
```c
// File: Common/source/[feature]_context.c
static system_context* g_system_context = NULL;

system_context* get_system_context(void) {
    return g_system_context;
}

void set_system_context(system_context *ctx) {
    g_system_context = ctx;
}
```

#### Step 4: Backward-Compatible Macros
```c
// File: Common/headers/[feature].h
#define roottable (get_system_context()->roottable)
#define systemtable (get_system_context()->systemtable)
#define efptable (get_system_context()->efptable)
```

#### Step 5: Thread-Local Context (Phase 2 - Optional)
```c
// File: Common/source/[feature]_context.c
_Thread_local system_context* tls_system_context = NULL;

system_context* get_system_context(void) {
    if (tls_system_context == NULL) {
        tls_system_context = system_context_retain(g_system_context);
    }
    return tls_system_context;
}
```

#### Step 6: Explicit Parameter Passing (Phase 3 - Future, Optional)
```c
// File: Common/headers/[feature].h
// New API with explicit context
boolean langfunctionvalue_context(system_context *ctx, hdltreenode hparam1, tyvaluerecord *vreturned);

// Old API (backward compatible, calls new API)
boolean langfunctionvalue(hdltreenode hparam1, tyvaluerecord *vreturned) {
    return langfunctionvalue_context(get_system_context(), hparam1, vreturned);
}
```

### Migration Path

1. **Phase 1**: Global accessor + macros (zero changes to calling code)
2. **Phase 2**: Thread-local context (thread isolation)
3. **Phase 3**: Explicit parameters (full explicit context - optional)

---

## Common Gotchas & Solutions

### Gotcha 1: Forgot to Update Thread Swap Functions

**Symptom**: State doesn't persist across thread swaps, random bugs

**Solution**: Use checklist - ALL 3 functions must be updated:
- [ ] `copythreadglobals()` - save state
- [ ] `swapinthreadglobals()` - restore state
- [ ] `newthreadglobals()` - initialize state

### Gotcha 2: Macro Definition Order

**Symptom**: Compile error "hthreadglobals not defined"

**Solution**: Define macros AFTER tythreadglobals structure definition
```c
// WRONG:
#define currenthashtable ((**hthreadglobals).currenthashtable)
typedef struct tythreadglobals { ... };  // Error!

// RIGHT:
typedef struct tythreadglobals { ... };
#define currenthashtable ((**hthreadglobals).currenthashtable)
```

### Gotcha 3: Headless Build Failures

**Symptom**: Headless builds fail with undefined symbols

**Solution**: Update `tests/headless_threadglobals.c` initialization
```c
void headless_init_threadglobals(void) {
    // Initialize your new fields here
    headless_threadglobals_data.your_field = initial_value;
}
```

### Gotcha 4: Reference Count Leaks

**Symptom**: Memory leaks, objects not disposed

**Solution**: Every `retain()` must have matching `release()`
```c
// WRONG:
context = system_context_retain(ctx);
// ... use context ...
// Forgot to release - LEAK!

// RIGHT:
context = system_context_retain(ctx);
// ... use context ...
system_context_release(context);  // Always release
```

### Gotcha 5: Atomic Initialization

**Symptom**: Race conditions on refcount, crashes

**Solution**: Use `atomic_init()` for first initialization
```c
// WRONG:
ctx->refcount = 1;  // Not atomic!

// RIGHT:
atomic_init(&ctx->refcount, 1);  // Atomic initialization
```

---

## Testing Strategy

### Unit Tests
```bash
./tools/run_headless_tests.sh
```
- Verify basic functionality
- Test thread-local state isolation
- Test reference counting lifecycle

### Integration Tests
```bash
cd tests && make test-integration
```
- 600+ YAML-based verb tests
- Real-world UserTalk execution
- Database loading/saving

### Thread Sanitizer
```bash
make TSAN=1 && ./tests/run_all_tests
```
- Detect race conditions
- Verify thread-safety
- Must be CLEAN (no warnings)

### Performance Benchmarks
```bash
./tests/benchmark_suite
```
- Compare before/after
- Must be < 5% regression
- Profile hot paths if needed

---

## Code Review Checklist

### Before Submitting PR

- [ ] All 3 thread functions updated (copy, swapin, new)
- [ ] Backward-compatible macros defined
- [ ] Global declarations removed (headers + source)
- [ ] Headless stubs updated (if needed)
- [ ] Unit tests pass
- [ ] Integration tests pass
- [ ] Thread sanitizer clean
- [ ] No performance regression
- [ ] Code comments reference ADR
- [ ] CLAUDE.md updated (if new pattern)

### Review Criteria

- [ ] Follows ADR-005/ADR-006 pattern
- [ ] No new global mutable state introduced
- [ ] Zero API changes (macros provide compatibility)
- [ ] Reference counting correct (if applicable)
- [ ] Documentation updated

---

## Quick Decision Tree

### "Should I use thread-local or explicit context?"

```
Is this state logically owned by a thread's execution?
├─ YES: Use thread-local storage (Pattern 1)
│  └─ Examples: parser state, control flow, error state, current scope
│
└─ NO: Is this state shared across operations?
   ├─ YES: Use explicit context (Pattern 2)
   │  └─ Examples: system tables, database connections, processors
   │
   └─ UNSURE: Start with thread-local, migrate to context if needed
```

### "Should I add reference counting?"

```
Will this object be shared across threads?
├─ YES: Add reference counting (Phase P1)
│  └─ Examples: outlines, hash tables, system context
│
└─ NO: Simple lifecycle (create/dispose)
   └─ Examples: thread-local state, temporary buffers
```

### "Should I use atomics or locks?"

```
Is this a simple counter or flag?
├─ YES: Use atomics (C11 _Atomic)
│  └─ Examples: refcount, boolean flags
│
└─ NO: Complex state that needs transactions?
   └─ Use locks (pthread_mutex_t)
      └─ Examples: complex data structures, multi-field updates
```

---

## Reference Examples

### Thread-Local Migration
- **ADR-005**: Parameter state migration (flnextparamislast, etc.)
- **ADR-006**: Outline context migration (outlinedata, outlinestack)
- **File**: `Common/headers/processinternal.h` (lines 118-250)
- **File**: `Common/source/process.c` (copythreadglobals, swapinthreadglobals)

### Explicit Context
- **db_context**: Database context pattern
- **File**: `Common/headers/db_context.h`
- **File**: `Common/source/db_format.c`

### Reference Counting
- **Future**: See planning/GLOBAL_STATE_ELIMINATION_PLAN.md Section 2.3

---

## Emergency Contacts

### If Something Goes Wrong

1. **Compilation Fails**: Check macro definition order, check headless stubs
2. **Tests Fail**: Check thread swap functions (all 3), check initialization
3. **Race Conditions**: Run thread sanitizer, check atomic operations
4. **Memory Leaks**: Check refcount balance (retain/release pairs)
5. **Performance Issues**: Profile hot paths, check macro expansion

### Documentation References

- **Full Plan**: `planning/GLOBAL_STATE_ELIMINATION_PLAN.md`
- **Summary**: `planning/GLOBAL_STATE_ELIMINATION_SUMMARY.md`
- **ADR-005**: Thread-local pattern (parameter state)
- **ADR-006**: Thread-local pattern (outline context)
- **ADR-008**: Processor table workaround (to be removed)
- **CLAUDE.md**: Strategic context

---

**Remember**: When in doubt, follow the proven pattern from ADR-005 or ADR-006. Zero API changes is the goal!
