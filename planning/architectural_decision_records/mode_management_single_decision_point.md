# Mode Management: Single Decision Point Principle

**Created**: 2025-12-20
**Status**: CRITICAL ARCHITECTURAL PRINCIPLE

## The Four Address Spaces

During v6→v7 migration, there are four distinct address spaces:

1. **On-disk v6 addresses** (32-bit LE): Addresses stored in v6 database files (`variabledata=0x58`)
2. **In-memory pointers** (64-bit): When `flinmemory=1`, variabledata is a memory pointer
3. **Expanded/aligned structures**: Objects in memory prepared for v7 format
4. **On-disk v7 addresses** (64-bit BE): New addresses written to v7 database

## Single Decision Point Rule

**If there are multiple code branches making read/write format decisions, the design is WRONG.**

- **ONE decision point for reading format**: Decides "use v6 32-bit LE reader" vs "use v7 64-bit BE reader"
- **ONE decision point for writing format**: Decides "use v6 writer" vs "use v7 writer"

## Mode Management Hierarchy

### Top-Level Caller (ONE place that manages mode)
Example: `langexternalpack_internal()`

**Responsibilities**:
- Sets mode ONCE for the entire operation
- Temporarily switches mode ONLY when crossing address space boundaries
- Restores mode after boundary crossing

**Example**:
```c
boolean langexternalpack_internal(const db_context *ctx, hdlexternalhandle h,
                                   Handle *hpacked, boolean *flnewdbaddress) {
    db_context working_context, legacy_context;
    boolean adapter_repack;

    if (ctx != NULL) {
        working_context = *ctx;  // Inherit output format from caller
    } else {
        db_context_init(&working_context);
    }

    adapter_repack = db_format_adapter_force_repack();

    // ==================================================================
    // SINGLE DECISION POINT FOR READING
    // ==================================================================
    if (adapter_repack && needs_to_load_from_v6) {
        // Crossing address space boundary: memory → on-disk v6
        legacy_context = working_context;
        legacy_context.mode.use_64bit_format = false;
        legacy_context.mode.adapter_repack = false;  // Pure read mode
        db_context_apply(&legacy_context);

        // Load from v6 (address space #1)
        load_external_from_disk(...);

        // Back to output format (address space #4)
        db_context_apply(&working_context);
    }

    // ==================================================================
    // SINGLE DECISION POINT FOR WRITING
    // ==================================================================
    // Mode is already set to output format by working_context
    // All child pack functions use this mode - they DON'T change it

    switch ((**hv).id) {
        case idoutlineprocessor:
            // Pass context for information, but DON'T let child change mode
            ok = opverbpack_internal(&working_context, hv, hpacked, flnewdbaddress);
            break;
        case idwordprocessor:
            ok = wpverbpack_internal(&working_context, hv, hpacked, flnewdbaddress);
            break;
        case idtableprocessor:
            ok = tableverbpack_internal(&working_context, hv, hpacked, flnewdbaddress);
            break;
    }

    return ok;
}
```

### Child Functions (ZERO mode management)
Examples: `opverbpack_internal()`, `wpverbpack_internal()`, `tableverbpack_internal()`

**Responsibilities**:
- Accept context parameter for information
- NEVER call `db_context_apply()`
- Operate with whatever mode is currently set globally
- Trust that caller has set the correct mode

**Example**:
```c
boolean opverbpack_internal(const db_context *ctx, hdlexternalvariable h,
                             Handle *hpacked, boolean *flnewdbaddress) {
    // NO db_context_apply() calls!
    // NO mode switching!
    // Just operate with current global mode

    if (!(**hv).flinmemory) {
        // Caller has already set mode for loading
        if (!opverbinmemory(hv))
            return (false);
    }

    // Pack using current mode (set by caller)
    if (!opverbpackoutline(ho, &hpackedoutline))
        return (false);

    // Write address using current mode (set by caller)
    return pushlongondiskhandle(adr, *hpacked);
}
```

## Why This Matters

### Problem: Mode Flip-Flopping
**Before (WRONG)**:
```
langexternalpack_internal:  mode = v7
├─ opverbpack_internal:     mode = v6 (for read)
│  └─ opverbinmemory:       mode = v6 ✓
│  mode = v7 (restore)
│  └─ pushlongondiskhandle: mode = v7 ✓
├─ wpverbpack_internal:     mode = v6 (for read)
│  └─ wpverbinmemory:       mode = v6 ✓
│  mode = v7 (restore)
│  └─ pushlongondiskhandle: mode = ??? (WRONG! Next function might see v6!)
```

**After (CORRECT)**:
```
langexternalpack_internal:  mode = v7 (SET ONCE)
├─ (mode = v6 for loading)
├─ opverbinmemory:          mode = v6 ✓
├─ (mode = v7 restored)
├─ opverbpack_internal:     mode = v7 (uses global, doesn't change)
│  └─ pushlongondiskhandle: mode = v7 ✓
├─ wpverbpack_internal:     mode = v7 (uses global, doesn't change)
│  └─ pushlongondiskhandle: mode = v7 ✓
```

### Benefits
1. **Deterministic**: Mode is predictable at every point
2. **Debuggable**: Single place to set breakpoint for mode changes
3. **Thread-safe**: No race conditions from multiple mode setters
4. **Clear responsibility**: Caller manages, children operate

## Concrete Implementation Pattern

### Helper Function: ensure_external_in_memory()

Create a type dispatcher in `langexternal.c` that handles loading without mode management:

```c
static boolean ensure_external_in_memory(hdlexternalvariable hv) {
    /*
     * Type-specific loading - assumes caller has already set correct read mode
     * Returns true if external is now in memory (flinmemory=1)
     */
    if ((**hv).flinmemory)
        return true;  /* Already loaded */

    switch ((**hv).id) {
        case idoutlineprocessor: case idscriptprocessor:
            return opverbinmemory((hdloutlinevariable)hv);

        case idwordprocessor:
            return wpverbinmemory(hv);

        case idtableprocessor:
            return tableverbinmemory(hv, HNoNode);

        case idmenuprocessor:
            return menuverbinmemory(hv);

        case idpictprocessor:
            return pictverbinmemory(hv);

        default:
            return false;
    }
}
```

**Benefits**:
- Centralizes type dispatch for loading
- No mode management (pure operation)
- Easy to add new types
- Clear precondition: caller must set read mode first

### langexternalpack_internal: The Mode Manager

```c
boolean langexternalpack_internal(const db_context *ctx, hdlexternalhandle h,
                                   Handle *hpacked, boolean *flnewdbaddress) {
    hdlexternalvariable hv = (hdlexternalvariable) h;
    db_context working_context, legacy_context;
    boolean adapter_repack;
    boolean ok = false;

    /* Initialize from caller or global state */
    if (ctx != NULL) {
        working_context = *ctx;
    } else {
        db_context_init(&working_context);
    }

    adapter_repack = db_format_adapter_force_repack();

    /* Setup disk header */
    tydiskexternalhandle rec;
    rec.versionnumber = conditionalshortswap(externaldiskversionnumber);
    rec.id = (byte) (**hv).id;
    if (!newfilledhandle(&rec, sizeof(rec), hpacked))
        return false;

    /* ==================================================================
     * SINGLE DECISION POINT: Load from v6 if needed, then switch to v7
     * ==================================================================
     */
    if (adapter_repack && !(**hv).flinmemory) {
        /* Set v6 read mode for loading from source database */
        legacy_context = working_context;
        legacy_context.mode.use_64bit_format = false;
        legacy_context.mode.adapter_repack = false;  /* Pure read */
        db_context_apply(&legacy_context);

        /* Load external into memory (uses current v6 mode) */
        if (!ensure_external_in_memory(hv)) {
            db_context_apply(&working_context);  /* Restore even on error */
            return false;
        }

        /* Switch to v7 write mode for packing to destination */
        working_context.mode.use_64bit_format = true;
        working_context.mode.adapter_repack = true;
        db_context_apply(&working_context);
    }

    /* ==================================================================
     * Pack with v7 write mode - children don't change mode
     * ==================================================================
     */
    switch ((**hv).id) {
        case idoutlineprocessor: case idscriptprocessor:
            ok = opverbpack_internal(&working_context, hv, hpacked, flnewdbaddress);
            break;

        case idwordprocessor:
            ok = wpverbpack_internal(&working_context, hv, hpacked, flnewdbaddress);
            break;

        case idtableprocessor:
            ok = tableverbpack_internal(&working_context, hv, hpacked, flnewdbaddress);
            break;

        case idmenuprocessor:
            ok = menuverbpack(hv, hpacked, flnewdbaddress);
            break;

        case idpictprocessor:
            ok = pictverbpack(hv, hpacked, flnewdbaddress);
            break;

        default:
            ok = false;
            break;
    }

    return ok;
}
```

### Child Pack Functions: Pure Operations

```c
boolean opverbpack_internal(const db_context *ctx, hdlexternalvariable h,
                             Handle *hpacked, boolean *flnewdbaddress) {
    /*
     * Pure packing function - NO mode management
     * Preconditions:
     *   - flinmemory=1 (caller loaded external into memory)
     *   - Global mode set to output format (v7 during migration)
     * Postconditions:
     *   - Packed data appended to *hpacked
     *   - Global mode unchanged
     */
    hdloutlinevariable hv = (hdloutlinevariable) h;
    hdloutlinerecord ho;
    Handle hpackedoutline;
    dbaddress adr;

    /* Sanity check: external should be in memory */
    if (!(**hv).flinmemory) {
        /* This is a programming error - caller should have loaded it */
        #if defined(FRONTIER_HEADLESS)
        fprintf(stderr, "[headless] opverbpack_internal: PRECONDITION VIOLATED - flinmemory=0\n");
        #endif
        return false;
    }

    ho = (hdloutlinerecord) (**hv).variabledata;
    adr = (**hv).oldaddress;

    /* Pack outline structure (uses current global mode for internal addresses) */
    if (!opverbpackoutline(ho, &hpackedoutline))
        return false;

    /* Assign to database (uses current global mode for block format) */
    if (!dbassignhandle(hpackedoutline, &adr)) {
        disposehandle(hpackedoutline);
        return false;
    }

    disposehandle(hpackedoutline);

    /* Update variable state */
    if (!fldatabasesaveas) {
        *flnewdbaddress = ((**hv).oldaddress != adr);
        (**hv).oldaddress = adr;
    } else {
        *flnewdbaddress = true;
    }

    /* Write address to packed stream (uses current global mode) */
    return pushlongondiskhandle(adr, *hpacked);
}
```

**Key Properties**:
- NO `db_context_apply()` calls
- NO `db_format_mode_push/pop()` calls
- NO mode inspection (doesn't care what mode is set)
- Clear precondition: `flinmemory=1`
- Clear postcondition: mode unchanged
- Pure function: same inputs → same outputs

## Implementation Checklist

- [ ] Create `ensure_external_in_memory()` helper in `langexternal.c`
- [ ] Update `langexternalpack_internal()` to use helper and manage mode ONCE
- [ ] Simplify `opverbpack_internal()` - remove ALL mode management
- [ ] Simplify `wpverbpack_internal()` - remove ALL mode management
- [ ] Simplify `tableverbpack_internal()` - remove ALL mode management
- [ ] Add precondition assertions: `assert((**hv).flinmemory)`
- [ ] Test migration produces deterministic output
- [ ] Verify no mode warnings in logs
- [ ] Verify mode changes only appear in langexternalpack_internal

## Validation

After implementing, the migration log should show:
```
[headless] db_format_mode_apply use_64bit=0 adapter_repack=0 drop_cancoon=0  # Initial v6 read
[headless] db_format_mode_apply use_64bit=1 adapter_repack=1 drop_cancoon=0  # Switch to v7 write
# ... NO MORE MODE CHANGES during packing ...
```

**NOT**:
```
[headless] db_format_mode_apply use_64bit=1 adapter_repack=1 drop_cancoon=0
[headless] db_format_mode_apply use_64bit=0 adapter_repack=1 drop_cancoon=0  # WRONG!
[headless] db_format_mode_apply use_64bit=1 adapter_repack=1 drop_cancoon=0
[headless] db_format_mode_apply use_64bit=0 adapter_repack=1 drop_cancoon=0  # WRONG!
```
