# ADR-008: Processor Table Lifecycle Workaround

**Status**: `[SUPERSEDED]` - Workaround removed. Global efptable eliminated via static isolation with accessor functions (Issue #292).
**Date**: 2026-01-12 (superseded 2026-04-15)
**Author**: System Architect
**Relates to**: Issue #135 (Collaborative ODB), ADR-005 (Thread-Safety)
**Superseded by**: Issue #292 — efptable converted from extern global to module-private static with `get_efptable()`/`set_efptable()` accessor functions and backward-compatible `#define efptable (get_efptable())` macro. The `save_headless_efptable()`/`get_headless_efptable()` workaround was dead code (caller removed in Issue #352) and has been deleted.

## Executive Summary

Database loading in headless mode overwrites the global `efptable` variable with a table containing tokenvaluetype entries from the database, breaking processor resolution. This ADR documents the temporary workaround using `get_headless_efptable()` to preserve the original runtime-created processor tables with working valueroutines.

**This is a SYMPTOM of the broader global state management problem documented in CLAUDE.md ("BURN THE GLOBALS WITH FIRE").**

## Context

### The Problem

When Frontier headless runtime loads a database (e.g., `Frontier.root`), the following sequence occurs:

1. **Runtime Initialization** (`db_format_prepare_runtime()`):
   - Creates processor tables (op, string, file, etc.) with working valueroutines
   - Stores reference in global `efptable` pointing to `system.compiler.kernel`
   - These are live, functional processor tables

2. **Database Loading** (`settablestructureglobals()` → `checktablestructure()`):
   - Walks loaded database structure looking for system tables
   - Finds `system.compiler.kernel` in the database
   - **OVERWRITES** global `efptable` with this database table
   - Database table contains **tokenvaluetype entries** (dead references from when DB was saved)
   - These tokens have NO valueroutines - they're stale serialized references

3. **Processor Resolution Fails**:
   - Code tries to resolve processor tables via `langexternalgettable()`
   - Looks up processor in `efptable`
   - Finds tokenvaluetype entry instead of working processor table
   - `tablevaltotable()` fails because tokens don't have valueroutines
   - Verb dispatch fails

**Root Cause**: Global mutable state (`efptable`) gets clobbered by database loading, and there's no lifecycle management to distinguish "runtime-created processors" from "database-serialized processors."

### Code References

**Workaround Implementation** (`Common/source/langexternal.c:301-357`):
```c
#if defined(FRONTIER_HEADLESS)
    /* Use headless-created efptable if available, not database efptable.
       When database is loaded, global efptable gets overwritten with tokenvaluetype entries. */
    extern hdlhashtable get_headless_efptable(void);
    hdlhashtable efp_to_search = get_headless_efptable();
    if (efp_to_search == nil) {
        efp_to_search = efptable;  /* Fallback if not saved yet */
    }

    /* Skip tokenvaluetype entries - they're stale DB references without valueroutines */
    if (val.valuetype == tokenvaluetype) {
        log_trace(LOG_COMP_EXTERNAL, "langexternalgettable: tokenvaluetype skipped");
        /* Fall through to roottable fallback */
    }
#endif
```

**Preservation** (`Common/source/tablestructure.c:124-133`):
```c
static hdlhashtable headless_efptable_original = nil;

void save_headless_efptable(void) {
    headless_efptable_original = efptable;
}

hdlhashtable get_headless_efptable(void) {
    return headless_efptable_original;
}
```

**Lifecycle Hook** (`Common/source/db_format.c:273-274`):
```c
/* Save reference to headless-created efptable before database loading */
extern void save_headless_efptable(void);
save_headless_efptable();
```

**Where Database Overwrites** (`Common/source/tablestructure.c:1102-1122`):
```c
boolean settablestructureglobals (Handle hvariable, boolean flcreatesubs) {
    cleartablestructureglobals();  /* Clears efptable and other globals */
    roottable = ht;
    return (checktablestructure(flcreatesubs));  /* Walks DB, reassigns efptable */
}
```

### Why tokenvaluetype Exists in Databases

When processor tables are saved to databases:
1. Processor tables themselves don't serialize (they're runtime constructs)
2. Instead, references to them become **tokenvaluetype** entries
3. These tokens represent "this was a processor table when DB was saved"
4. On load, legacy Mac GUI would re-create processors and fix up references
5. **Headless runtime** doesn't have this fixup mechanism yet

### Strategic Context

**North Star: Collaborative ODB Editing (Phase 6+)**

From CLAUDE.md:
> Multiple users edit different ODB objects simultaneously. Runtime handles all concurrency, locking, and conflict resolution transparently.

**Launch Requirement**: Global mutable state creates race conditions and violates thread-safety requirements for Automattic partnership.

**Related Refactoring Work**:
- **ADR-005**: Thread-local parameter state migration (proven pattern)
- **Issue #135**: Outline context refactoring (explicit context pattern)
- **CLAUDE.md**: "BURN THE GLOBALS WITH FIRE" - comprehensive global state elimination

## Decision

**Adopt the `get_headless_efptable()` workaround as a TEMPORARY solution** to unblock processor resolution in headless mode.

**This is NOT the long-term fix.** The proper solution requires eliminating global table variables and implementing explicit processor table lifecycle management.

### Rationale

1. **Unblocks Development**: Processor resolution works correctly in headless mode, enabling verb implementation work to proceed.

2. **Low Risk**: Workaround is isolated to headless builds (`#if defined(FRONTIER_HEADLESS)`), doesn't affect legacy Mac GUI.

3. **Diagnostic Value**: Explicitly logs when tokenvaluetype entries are skipped, making the problem visible for future refactoring.

4. **Temporary by Design**: Code comments clearly mark this as a workaround, not architectural solution.

5. **Doesn't Preclude Better Solution**: When global state elimination happens (Phase 6+), this workaround will be removed as part of that refactoring.

## Implementation

### Files Modified

1. **`Common/source/tablestructure.c`** (lines 124-133):
   - Added `headless_efptable_original` static storage
   - Added `save_headless_efptable()` to preserve runtime-created table
   - Added `get_headless_efptable()` accessor

2. **`Common/source/db_format.c`** (lines 273-274):
   - Call `save_headless_efptable()` after runtime processor creation
   - Before `linksystemtablestructure()` which can trigger database loading

3. **`Common/source/langexternal.c`** (lines 301-357):
   - Use `get_headless_efptable()` instead of global `efptable` for processor lookup
   - Skip tokenvaluetype entries (dead DB references)
   - Fall through to roottable fallback for working processors

### How It Works

**Initialization Sequence**:
```
1. db_format_prepare_runtime()
   ├─> Create processor tables (op, string, file, etc.)
   ├─> efptable points to system.compiler.kernel (runtime-created)
   ├─> save_headless_efptable()  ← PRESERVE REFERENCE
   └─> linksystemtablestructure()
       └─> May load database, overwriting efptable

2. Database Load (if --system-root specified)
   ├─> settablestructureglobals()
   ├─> checktablestructure()
   └─> efptable now points to DB version (with tokenvaluetype entries)

3. Processor Resolution (langexternalgettable)
   ├─> Call get_headless_efptable() ← USE SAVED REFERENCE
   ├─> Search in original runtime-created table
   ├─> Skip any tokenvaluetype entries
   └─> Return working processor with valueroutines
```

**Fallback Chain**:
1. Try `get_headless_efptable()` (original runtime table)
2. Skip tokenvaluetype entries
3. Fall through to roottable search
4. Fall through to systemtable search

### Testing Strategy

**Verification**:
1. Load database with `--system-root databases/Frontier.root`
2. Execute verb requiring processor resolution (e.g., `op.insert()`, `string.upper()`)
3. Verify processor found via `get_headless_efptable()`, not broken DB efptable
4. Check logs for "tokenvaluetype skipped" messages

**Log Output** (diagnostic):
```
[trace] langexternalgettable: searching efptable=0x123 (headless=0x456 db=0x123)
[trace] langexternalgettable: tokenvaluetype skipped, falling through
[trace] langexternalgettable: found in roottable fallback
```

## Consequences

### Positive

1. **Processor Resolution Works**: Verbs execute correctly even after database loading.

2. **Diagnostic Visibility**: Logs clearly show when workaround is active and why.

3. **Non-Invasive**: Isolated to headless builds, no risk to legacy Mac GUI.

4. **Unblocks Development**: Verb implementation work can proceed without waiting for global state refactoring.

### Negative

1. **Technical Debt**: This is a band-aid, not a fix. Adds complexity that must be removed later.

2. **Doesn't Solve Root Cause**: Global mutable state still exists, still causes problems elsewhere.

3. **Headless-Only Solution**: Mac GUI may have similar issues that aren't addressed here.

4. **Hidden Complexity**: Future developers may not realize efptable has two versions (runtime vs database).

### Risks & Mitigation

**Risk 1: Workaround becomes permanent**
- Mitigation: Clearly marked as temporary in code comments
- Mitigation: Linked to Issue #135 for eventual removal
- Mitigation: This ADR documents the problem and proper solution

**Risk 2: Database and runtime tables diverge**
- Mitigation: Runtime always creates complete processor tables
- Mitigation: Fallback chain ensures processors found even if workaround fails

**Risk 3: Mac GUI has similar issue**
- Mitigation: Workaround is headless-only, doesn't break Mac GUI
- Mitigation: Mac GUI may have existing fixup mechanism we haven't discovered yet

## Alternatives Considered

### Option 1: Fix Database Serialization (NOT CHOSEN)

**Description**: Change database serialization to save full processor tables, not tokenvaluetype entries.

**Rejected Because**:
- ❌ Major change to database format (requires migration)
- ❌ Processors are runtime constructs, shouldn't be serialized
- ❌ Mac GUI compatibility issues (legacy format expectations)
- ❌ Doesn't solve the global state problem

### Option 2: Re-create Processors on Database Load (NOT CHOSEN)

**Description**: After database load, detect tokenvaluetype entries and replace with fresh processors.

**Rejected Because**:
- ❌ Requires walking entire database looking for tokens
- ❌ Expensive operation on every database load
- ❌ Still relies on global state (efptable)
- ❌ Doesn't address thread-safety for collaborative ODB

### Option 3: Explicit Processor Context (PROPER LONG-TERM SOLUTION)

**Description**: Eliminate global `efptable`, pass processor context explicitly to verb dispatch.

**Not Implemented Now Because**:
- ⏰ Requires comprehensive refactoring (Phase 6+ work)
- ⏰ Must coordinate with outline context refactoring (Issue #135)
- ⏰ Part of larger "BURN THE GLOBALS WITH FIRE" effort
- ✅ But this IS the correct long-term solution

**Why This Is The Proper Fix**:
- ✅ No global mutable state
- ✅ Thread-safe by design
- ✅ Clear lifecycle management
- ✅ Processors live in explicit context, not globals
- ✅ Database loading can't clobber runtime state

## Future Work

### Phase 6+: Eliminate Global Processor Tables

**Target Architecture** (based on ADR-005 and Issue #135 patterns):

```c
/* Processor tables in explicit context, not globals */
typedef struct processor_context {
    hdlhashtable op_processor;
    hdlhashtable string_processor;
    hdlhashtable file_processor;
    /* ... all processors ... */
    int refcount;  /* For collaborative ODB */
} processor_context;

/* No more global efptable */
// hdlhashtable efptable = nil;  ← DELETE

/* Verb dispatch uses explicit context */
boolean langfunctionvalue(processor_context *ctx, hdltreenode hparam1, tyvaluerecord *vreturned);
```

**Migration Steps**:
1. Create `processor_context` structure
2. Move processor creation to context initialization
3. Thread context through verb dispatch chain
4. Update all verb processors to accept context parameter
5. Remove global `efptable`, `stringtable`, etc.
6. Remove `get_headless_efptable()` workaround

**Coordination**:
- Must align with outline context refactoring (Issue #135)
- Must align with thread-local globals migration (ADR-005)
- Consider unified "execution context" with processors + outline + db + thread state

### Immediate Follow-up (This PR)

1. ✅ Document this workaround in ADR-008 (this document)
2. ✅ Add comments in code referencing this ADR
3. ✅ Link to Issue #135 for eventual removal
4. ✅ Update CLAUDE.md with reference to this ADR

### Phase 4-5: Audit Other Global Tables

**Known Global Tables** (may have similar issues):
- `langtable` - Language runtime tables
- `builtinstable` - Built-in functions
- `verbstable` - Verb table
- `systemtable` - System table

**Audit Questions**:
1. Do these get overwritten by database loading?
2. Do they contain tokenvaluetype entries from databases?
3. Should they be in explicit context instead of globals?

## Success Metrics

### Workaround Verified When:
- [x] Processor resolution works after database load
- [x] Verbs execute correctly (op.insert, string.upper, file.create, etc.)
- [x] Logs show tokenvaluetype entries being skipped
- [x] No segfaults or "processor not found" errors

### Workaround Removed When (Phase 6+):
- [ ] Processor tables moved to explicit context
- [ ] No global `efptable` variable
- [ ] Database loading doesn't affect processor resolution
- [ ] Thread-safe processor access for collaborative ODB
- [ ] All tests pass without workaround

## References

### Internal Documentation
- **CLAUDE.md**: "BURN THE GLOBALS WITH FIRE" (global state elimination)
- **CLAUDE.md**: "Collaborative ODB Editing - North Star Vision"
- **ADR-005**: Parameter State Thread-Safety (thread-local pattern)
- **Issue #135**: Outline context refactoring (explicit context pattern)
- **PR #291**: Where this workaround was implemented and reviewed

### Code References
- `Common/source/langexternal.c:301-357` - Workaround implementation
- `Common/source/tablestructure.c:124-133` - Preservation mechanism
- `Common/source/db_format.c:273-274` - Lifecycle hook
- `Common/source/tablestructure.c:1102-1122` - Where database overwrites globals

### Related ADRs
- **ADR-001**: Multi-Database Context (db_context pattern)
- **ADR-002**: Context-Based Format Versioning
- **ADR-005**: Parameter State Thread-Safety (proven migration pattern)
- **ADR-006**: Outline Push/Pop Elimination (global state refactoring)

### Related Issues
- **Issue #135**: Outline context refactoring (proper context-based architecture)
- **PR #291**: XML verb implementation (where workaround was discovered)

---

## Appendix: Why This Matters for Collaborative ODB

From CLAUDE.md:
> Frontier should support Google Docs/Sheets-style collaborative editing of ODB objects:
> - Multiple users edit different ODB objects simultaneously
> - Runtime handles all concurrency, locking, and conflict resolution transparently
> - Developers write functionally single-threaded code

**Global mutable state makes this impossible:**
- Thread A loads database → overwrites `efptable` → Thread B's verb dispatch breaks
- No locking protects `efptable` access → race conditions
- Database loading is not atomic → partial state visible to other threads

**Explicit context makes it possible:**
- Each thread/user has own `processor_context`
- Database loading creates new context, doesn't clobber globals
- Reference counting ensures processors stay valid during concurrent access
- Thread-safe by design, no locks needed

**This workaround buys time** to implement collaborative ODB properly, but it's not thread-safe itself. The proper fix (explicit context) is required for launch.
