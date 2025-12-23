# Call Graph: Context Threading for Table Pack/Unpack Operations

**Related**: `EXPLICIT_CONTEXT_PASSING_REFACTORING_PLAN.md`
**Created**: 2025-12-22

---

## Legend

```
[Tier 1] - Mode Manager (sets context, manages transitions)
[Tier 2] - Context Consumer (uses context, doesn't change it)
[Tier 3] - Primitive (database operations)

✅ - Already implements explicit context passing
⚠️  - Needs context parameter added
❌ - Uses global mode state (bug source)
```

---

## Current State: Hash Pack Operation (Issue #147 Bug Path)

```
hashpacktable_internal(ctx)                                    [Tier 1] ⚠️
│
├─ Check: ctx ? ctx->mode : db_format_mode_current()           ❌ BUG SOURCE
│
├─ Setup typackinforecord:
│  ├─ use_64bit = (decision above)                             ❌ Cached value
│  └─ context = ???                                             ❌ NOT PASSED
│
└─ hashsortedinversesearch(htable, &hashpackvisit, &packrec)
   │
   └─ hashpackvisit(bsname, hnode, val, refcon)                [Tier 2] ❌
      │
      ├─ hashpackvisit_v7(bsname, hnode, val, refcon)          [Tier 2] ❌
      │  │
      │  ├─ use_64bit = lpi->use_64bit                         ❌ Uses cached value
      │  │
      │  ├─ IF valuetype == externalvaluetype:
      │  │  └─ hashpackexternal(&s2, hv, &ix, &flnewdbaddress) [Tier 2] ❌
      │  │     │
      │  │     ├─ IF flexternalmemorypack:
      │  │     │  └─ langexternalmemorypack(...)
      │  │     │
      │  │     └─ ELSE:
      │  │        └─ langexternalpack_internal(NULL, ...)      ❌ NO CONTEXT!
      │  │           │
      │  │           ├─ db_context_init(&working_context)      ❌ Gets WRONG mode!
      │  │           │  └─ Reads db_format_mode_current()      ❌ May be v6 read mode
      │  │           │
      │  │           ├─ IF adapter_repack && !flinmemory:
      │  │           │  ├─ Set mode = v6 for loading           ✅ Correct
      │  │           │  ├─ ensure_external_in_memory(...)      ✅ Loads OK
      │  │           │  └─ Set mode = v7 for packing           ✅ Correct
      │  │           │
      │  │           └─ tableverbpack_internal(ctx, ...)       ✅ Passes context
      │  │              └─ tablepacktable_internal(ctx, ...)   ✅ Passes context
      │  │                 └─ hashpacktable_context(ctx, ...)  ✅ Wrapper
      │  │                    └─ hashpacktable_internal(ctx, ...) 🔄 RECURSION
      │  │                       │
      │  │                       └─ Check: ctx ? ctx->mode : db_format_mode_current()
      │  │                          │
      │  │                          └─ ❌❌❌ BUG: Reads v6 mode from global stack!
      │  │                             Child table gets packed with v4 header!
      │  │
      │  ├─ hashpackstring(&s2, bsname, &ix)                   [Tier 3]
      │  ├─ hashpackdata(&s2, pdata, ctbytes, &ix)             [Tier 3]
      │  ├─ hashpackbinary(&s2, hbinary, &ix)                  [Tier 3]
      │  └─ hashpackscalar(&s2, hnode, &ix, use_64bit)         [Tier 3] ❌ Boolean param
      │
      └─ hashpackvisit_legacy(bsname, hnode, val, refcon)      [Tier 2] ❌
```

**Root Cause**: When `hashpackexternal()` calls `langexternalpack_internal(NULL, ...)`, the NULL context forces initialization from global mode state. If another operation pushed v6 mode onto the stack, the child table packing inherits v6 mode → writes v4 header instead of v5.

---

## Target State: After Phase 1 (Minimal Fix)

```
hashpacktable_internal(ctx)                                    [Tier 1] ✅
│
├─ IF ctx != NULL:
│  └─ working_context = *ctx                                   ✅ Explicit
│  ELSE:
│  └─ db_context_init(&working_context)                        ⚠️ Fallback (Phase 3 removes)
│
├─ use_64bit = working_context.mode.use_64bit_format           ✅ Derived from context
│
├─ Setup typackinforecord:
│  ├─ use_64bit = use_64bit                                    ✅ Consistent
│  └─ context = &working_context                               ✅ NEW: Context passed!
│
└─ hashsortedinversesearch(htable, &hashpackvisit, &packrec)
   │
   └─ hashpackvisit(bsname, hnode, val, refcon)                [Tier 2] ✅
      │
      └─ hashpackvisit_v7(bsname, hnode, val, refcon)          [Tier 2] ✅
         │
         ├─ ctx = lpi->context                                 ✅ Extract from refcon
         ├─ use_64bit = lpi->use_64bit                         ✅ Still cached (Phase 2 removes)
         │
         ├─ IF valuetype == externalvaluetype:
         │  └─ hashpackexternal(&s2, hv, &ix, &flnewdbaddress, ctx)  ✅ PASS CONTEXT
         │     │
         │     └─ langexternalpack_internal(ctx, ...)          ✅ Receives parent context!
         │        │
         │        ├─ IF ctx != NULL:
         │        │  └─ working_context = *ctx                 ✅ Inherits v7 mode
         │        │  ELSE:
         │        │  └─ db_context_init(...)                   ⚠️ Fallback (shouldn't happen)
         │        │
         │        ├─ IF adapter_repack && !flinmemory:
         │        │  ├─ Set legacy_context.mode = v6           ✅ Temporary for load
         │        │  ├─ ensure_external_in_memory(...)         ✅ Loads OK
         │        │  └─ Set working_context.mode = v7          ✅ Restore for pack
         │        │
         │        └─ tableverbpack_internal(ctx, ...)          ✅ Passes context
         │           └─ tablepacktable_internal(ctx, ...)      ✅ Passes context
         │              └─ hashpacktable_context(ctx, ...)     ✅ Wrapper
         │                 └─ hashpacktable_internal(ctx, ...)  🔄 RECURSION
         │                    │
         │                    └─ working_context = *ctx        ✅✅✅ FIX: Uses parent v7 context!
         │                       Child table gets packed with v5 header ✅
         │
         ├─ hashpackstring(&s2, bsname, &ix)                   [Tier 3]
         ├─ hashpackdata(&s2, pdata, ctbytes, &ix)             [Tier 3]
         ├─ hashpackbinary(&s2, hbinary, &ix)                  [Tier 3]
         └─ hashpackscalar(&s2, hnode, &ix, use_64bit)         [Tier 3] ⚠️ Phase 2 adds ctx
```

**Fix**: Context flows from parent `hashpacktable_internal()` → `hashpackvisit_v7()` → `hashpackexternal()` → `langexternalpack_internal()` → child `hashpacktable_internal()`. Recursive packing uses explicit parent context, not global mode.

---

## Target State: After Phase 2 (Complete Context Threading)

```
hashpacktable_internal(ctx)                                    [Tier 1] ✅
│
├─ working_context = *ctx  (assert ctx != NULL by Phase 3)     ✅
├─ use_64bit = working_context.mode.use_64bit_format           ✅
│
├─ Setup typackinforecord:
│  ├─ use_64bit = REMOVED                                      ✅ Phase 2 cleanup
│  └─ context = &working_context                               ✅ Only source of truth
│
└─ hashsortedinversesearch(htable, &hashpackvisit, &packrec)
   │
   └─ hashpackvisit_v7(bsname, hnode, val, refcon)             [Tier 2] ✅
      │
      ├─ ctx = lpi->context                                    ✅ Extract context
      ├─ use_64bit = ctx->mode.use_64bit_format                ✅ Derived, not cached
      │
      ├─ IF valuetype == externalvaluetype:
      │  └─ hashpackexternal(&s2, hv, &ix, &flnewdbaddress, ctx)  ✅
      │     └─ langexternalpack_internal(ctx, ...)             ✅
      │        └─ tableverbpack_internal(ctx, ...)             ✅
      │           └─ hashpacktable_internal(ctx, ...)          ✅
      │
      ├─ hashpackstring(&s2, bsname, &ix)                      [Tier 3]
      ├─ hashpackdata(&s2, pdata, ctbytes, &ix)                [Tier 3]
      ├─ hashpackbinary(&s2, hbinary, &ix)                     [Tier 3]
      └─ hashpackscalar(&s2, hnode, &ix, ctx)                  ✅ Phase 2: Context param
         └─ use_64bit = ctx->mode.use_64bit_format             ✅ Derived at point of use
```

**Improvement**: Eliminated cached `use_64bit` boolean. All format decisions derive from context at point of use. Reduces state duplication and prevents stale cached values.

---

## Target State: After Phase 3 (Context Mandatory)

```
hashpacktable_internal(const db_context *ctx, ...)             [Tier 1] ✅
│
├─ assert(ctx != NULL);                                        ✅ Context required
├─ use_64bit = ctx->mode.use_64bit_format                      ✅ Direct read, no fallback
│
├─ Setup typackinforecord:
│  └─ context = ctx                                            ✅ Pass through
│
└─ hashsortedinversesearch(htable, &hashpackvisit, &packrec)
   └─ hashpackvisit_v7(bsname, hnode, val, refcon)             ✅
      └─ (same as Phase 2)


// ALL CALLERS must initialize context:

tablepacktable_internal(ctx, ...)                              ✅ Already done
langhtml.c callers:                                            ⚠️ Needs update
  db_context ctx;
  db_context_init(&ctx);
  hashpacktable_internal(&ctx, ...)                            ✅ Explicit

tableverbs.c via hashpacktable_context():                      ✅ Already done
  boolean hashpacktable_context(const db_context *ctx, ...)
    return hashpacktable_internal(ctx, ...)

Legacy hashpacktable() wrapper:                                ✅ Compatibility
  boolean hashpacktable(htable, flmemory, hpacked, flmustsave) {
    db_context ctx;
    db_context_init(&ctx);
    return hashpacktable_internal(&ctx, htable, flmemory, hpacked, flmustsave);
  }
```

**Final State**: Context is mandatory everywhere. No fallback to global mode state. All callers explicitly initialize context. Pattern is consistent across codebase.

---

## Comparison: Before vs. After

### Before (Current - Buggy)

| Function | Context Source | Mode Decision | Bug Risk |
|----------|----------------|---------------|----------|
| `hashpacktable_internal()` | `ctx ? ctx : global` | Fallback to TLS | MEDIUM |
| `hashpackvisit_v7()` | Cached boolean | Stale cached value | LOW |
| `hashpackexternal()` | None | N/A | LOW |
| `langexternalpack_internal()` | `ctx ? ctx : global` | **Fallback to TLS** | **HIGH** 🔴 |
| Child `hashpacktable_internal()` | `ctx ? ctx : global` | **Reads wrong mode** | **CRITICAL** 🔴🔴 |

**Bug Scenario**: Parent packs with v7, child reads v6 from global stack → wrong table header version.

### After Phase 1 (Minimal Fix)

| Function | Context Source | Mode Decision | Bug Risk |
|----------|----------------|---------------|----------|
| `hashpacktable_internal()` | `ctx ? ctx : global` | Fallback to TLS (safe) | LOW |
| `hashpackvisit_v7()` | Refcon context | Explicit parent context | LOW |
| `hashpackexternal()` | **Refcon context** | **N/A** | **LOW** ✅ |
| `langexternalpack_internal()` | **`ctx` (explicit)** | **Explicit parent context** | **LOW** ✅ |
| Child `hashpacktable_internal()` | **`ctx` (explicit)** | **Correct v7 mode** | **LOW** ✅ |

**Fix**: Context flows explicitly through call chain. Children inherit parent context, not global stack.

### After Phase 3 (Complete)

| Function | Context Source | Mode Decision | Bug Risk |
|----------|----------------|---------------|----------|
| `hashpacktable_internal()` | `ctx` (required) | Explicit only | NONE |
| `hashpackvisit_v7()` | Refcon context | Explicit parent context | NONE |
| `hashpackexternal()` | Refcon context | N/A | NONE |
| `langexternalpack_internal()` | `ctx` (required) | Explicit parent context | NONE |
| Child `hashpacktable_internal()` | `ctx` (required) | Explicit parent context | NONE |

**Final**: No global mode reads. All context explicit. Bug class eliminated by design.

---

## External Pack Operations (Reference Implementation)

These operations **already implement** the Single Decision Point pattern:

```
langexternalpack_internal(const db_context *ctx, ...)          [Tier 1] ✅ DONE
│
├─ IF ctx != NULL:
│  └─ working_context = *ctx                                   ✅
│  ELSE:
│  └─ db_context_init(&working_context)                        ✅
│
├─ adapter_repack = db_format_adapter_force_repack()           ✅
│
├─ SINGLE DECISION POINT FOR READING:
│  IF adapter_repack && !flinmemory:
│  ├─ legacy_context = working_context
│  ├─ legacy_context.mode.use_64bit_format = false             ✅ v6 read mode
│  ├─ legacy_context.mode.adapter_repack = false               ✅ Pure read
│  ├─ ensure_external_in_memory(&legacy_context, hv)           ✅ Load with v6 context
│  ├─ working_context.mode.use_64bit_format = true             ✅ Switch to v7
│  └─ working_context.mode.adapter_repack = true               ✅ Write mode
│
└─ SINGLE DECISION POINT FOR WRITING:
   switch ((**hv).id):
   ├─ idoutlineprocessor:
   │  └─ opverbpack_internal(&working_context, ...)            ✅ Passes context
   ├─ idwordprocessor:
   │  └─ wpverbpack_internal(&working_context, ...)            ✅ Passes context
   └─ idtableprocessor:
      └─ tableverbpack_internal(&working_context, ...)         ✅ Passes context
         └─ tablepacktable_internal(&working_context, ...)     ✅ Passes context
            └─ hashpacktable_internal(&working_context, ...)   ✅ Passes context
```

**Key Properties**:
1. **ONE mode transition**: v6 read → v7 write (happens once at top level)
2. **Explicit context passing**: All children receive `&working_context`
3. **No mode stack changes in children**: Pure operations
4. **Deterministic**: Same input → same mode → same output

**This is the pattern we're extending to hash operations.**

---

## Database Operations (Already Context-Aware)

These primitives **already support** explicit context:

```
dbassign_context(const db_context *ctx, dbaddress *padr, ...)  [Tier 3] ✅
dbreference_context(const db_context *ctx, dbaddress adr, ...) [Tier 3] ✅
dbcopy_context(const db_context *ctx, dbaddress src, ...)      [Tier 3] ✅
dballocate_context(const db_context *ctx, long databytes, ...) [Tier 3] ✅
dbassignhandle_context(const db_context *ctx, Handle h, ...)   [Tier 3] ✅
dbrefhandle_context(const db_context *ctx, dbaddress adr, ...) [Tier 3] ✅
```

**No changes needed**: These operations already accept context. They'll use the explicit context once hash operations pass it down.

---

## Mode Stack Evolution (Phase by Phase)

### Current State: Mode Stack Used Everywhere

```
Thread-Local Mode Stack (WRONG - causes bugs):
┌─────────────────────────────────────┐
│ [v7 write mode] ← Migration context │ ← Pushed by migration
├─────────────────────────────────────┤
│ [v6 read mode]  ← Adapter load      │ ← Pushed for loading externals
├─────────────────────────────────────┤
│ [v7 write mode] ← Should restore... │ ← Popped after load (but might not!)
└─────────────────────────────────────┘
         ↑
         │ db_format_mode_current() reads TOP of stack
         │ If pop didn't happen, children see WRONG mode!
         └─ hashpacktable_internal() reads this ❌
```

### After Phase 1: Context Flows Explicitly

```
Thread-Local Mode Stack (still exists, but not used for child operations):
┌─────────────────────────────────────┐
│ [v7 write mode] ← Migration context │
└─────────────────────────────────────┘
         ↑
         │ Rarely read (only when ctx == NULL)
         └─ Fallback only

Explicit Context Flow (NEW - prevents bugs):
hashpacktable_internal(&ctx_v7)
  → hashpackvisit_v7(&ctx_v7 via refcon)
    → hashpackexternal(&ctx_v7 via refcon)
      → langexternalpack_internal(&ctx_v7)
        → [Load with v6 context, pack with v7 context]
        → tableverbpack_internal(&ctx_v7)
          → hashpacktable_internal(&ctx_v7) ✅ Correct!
```

### After Phase 3: Mode Stack Not Read

```
Thread-Local Mode Stack (exists for legacy code only):
┌─────────────────────────────────────┐
│ [Some mode] ← Legacy operations     │
└─────────────────────────────────────┘
         ↑
         │ Never read by hash/table operations
         └─ Only used by legacy code paths

Explicit Context Flow (REQUIRED - no fallback):
hashpacktable_internal(&ctx) [assert ctx != NULL]
  → All operations use explicit context
  → No db_format_mode_current() calls
  → No db_format_mode_push/pop() calls
  → Deterministic by design ✅
```

---

## Testing: Before vs. After Logs

### Before Phase 1 (Buggy Logs)

```
[headless] hashpacktable_internal use_64bit=1 (ctx=0x7ffeeb... ctx_mode=1 current_mode: use_64bit=1)
[headless] langexternalpack: loading external from v6 id=5 (explicit context)
[headless] db_format_mode_apply use_64bit=0 adapter_repack=0  ← Pushed v6 mode
[headless] hashpacktable_internal use_64bit=0 (ctx=(nil) current_mode: use_64bit=0)  ← ❌ BUG!
                                                ^^^^^^^^^^^ ← NULL context!
                                                              ^^^^^^^^^^^^^^^^^^^^^^^^^^ ← Wrong mode!
```

**Bug**: Child `hashpacktable_internal()` receives NULL context, reads global mode, gets v6 → writes v4 header.

### After Phase 1 (Fixed Logs)

```
[headless] hashpacktable_internal use_64bit=1 (ctx=0x7ffeeb... ctx_mode=1)
[headless] langexternalpack: loading external from v6 id=5 (explicit context)
[headless] langexternalpack: using v7 write context (no global mode set)
[headless] hashpacktable_internal use_64bit=1 (ctx=0x7ffeec... ctx_mode=1)  ← ✅ FIX!
                                                ^^^^^^^^^^^ ← Valid context pointer!
                                                              ^^^^^^^^^^^^ ← Correct v7 mode!
```

**Fix**: Child receives explicit context from parent, uses v7 mode → writes v5 header correctly.

### After Phase 3 (Clean Logs)

```
[headless] hashpacktable_internal use_64bit=1 (ctx=0x7ffeeb...)
[headless] langexternalpack: using v7 write context
[headless] hashpacktable_internal use_64bit=1 (ctx=0x7ffeec...)
```

**Clean**: No mode stack operations logged. Context is implicit in all operations. Deterministic.

---

## Summary: The Fix in One Diagram

```
                    BEFORE (BUGGY)                          AFTER (FIXED)
                    ──────────────                          ─────────────

hashpacktable(ht)                            hashpacktable(ht)
    ↓                                            ↓
hashpacktable_internal(NULL)                 hashpacktable_internal(NULL)
    ↓                                            ↓
use_64bit = db_format_mode_current() ❌      db_context_init(&ctx)
    = 1 (v7)                                 working_context = ctx
    ↓                                        use_64bit = ctx.mode.use_64bit_format
packrec.use_64bit = 1                            = 1 (v7)
packrec.context = ???  ❌ MISSING!               ↓
    ↓                                        packrec.use_64bit = 1
hashpackvisit_v7(refcon)                     packrec.context = &working_context ✅
    ↓                                            ↓
use_64bit = refcon->use_64bit (1)            hashpackvisit_v7(refcon)
    ↓                                            ↓
hashpackexternal(hv)                         ctx = refcon->context ✅
    ↓                                        use_64bit = refcon->use_64bit (1)
langexternalpack_internal(NULL) ❌               ↓
    ↓                                        hashpackexternal(hv, ctx) ✅
db_context_init(&ctx)                            ↓
    ↓                                        langexternalpack_internal(ctx) ✅
db_format_mode_current() ❌                      ↓
    [v6 mode was pushed!]                    working_context = *ctx ✅
    = 0 (v6)  ← BUG!                             = v7 (inherited from parent)
    ↓                                            ↓
tableverbpack_internal(ctx)                  tableverbpack_internal(&ctx) ✅
    ↓                                            ↓
hashpacktable_internal(NULL) ❌              hashpacktable_internal(&ctx) ✅
    ↓                                            ↓
use_64bit = db_format_mode_current()         use_64bit = ctx->mode.use_64bit_format
    = 0 (v6)  ← BUG!                             = 1 (v7)  ← CORRECT!
    ↓                                            ↓
Write v4 header ❌                           Write v5 header ✅
```

**Root Cause**: NULL context → global mode fallback → wrong mode → wrong header
**Fix**: Explicit context → no fallback → correct mode → correct header

---

## Conclusion

**Phase 1** adds ~4 lines of context threading to fix a critical bug:
1. Add `context` field to `typackinforecord`
2. Set `packrec.context = &working_context` in `hashpacktable_internal()`
3. Pass `ctx` to `hashpackexternal()` from `hashpackvisit_v7()`
4. Pass `ctx` to `langexternalpack_internal()` from `hashpackexternal()`

**Result**: Context flows explicitly through the entire call chain. No more mode stack bugs during recursive table packing.

---

**Status**: Design complete, ready for implementation
