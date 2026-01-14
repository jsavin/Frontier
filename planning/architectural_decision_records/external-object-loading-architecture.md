# External Object Loading Architecture

**Created**: 2025-12-24
**Status**: ARCHITECTURAL REFERENCE (Active)
**Related Issues**: #136 (Push/pop anti-pattern audit), #123 (Migration)
**Knowledge Source**: v6→v7 migration refactoring (Phase 2, December 2025)

---

## Overview

This document establishes the architectural patterns for loading and materializing external objects (menus, WPText, outlines, scripts, pictures, tables) in the Frontier runtime. It consolidates patterns discovered during v6→v7 database migration work and provides a framework for refactoring work under issue #136.

**Key Principle**: External object loading should follow a **single decision point pattern** where loading decisions are made in one place (not scattered across type-specific code), and context information is passed explicitly rather than managed through global mode stacks.

---

## 1. External Object Types and Characteristics

### 1.1 Type Classification

**By Loading Strategy**:

| Type | ID | Materialization | Loading Pattern | Memory | Count | Notes |
|------|----|----|---|---|---|---|
| **Outline** | `idoutlineprocessor` (2) | Eager | In-memory | Low | Few | Usually loaded immediately |
| **Script** | `idscriptprocessor` (10) | Eager | In-memory | Low | Few | Compiled to bytecode in memory |
| **WPText** | `idwordprocessor` (4) | **Deferred** | On-demand | **High** | **Many** | Can exceed available RAM if eager |
| **Menu** | `idmenuprocessor` (1) | **Deferred** | On-demand | Medium | **Many** | Similar memory concerns as WPText |
| **Picture** | `idpictprocessor` (8) | Eager | In-memory | Medium | Medium | Icon, bitmap data |
| **Table** | `idtableprocessor` (3) | **Recursive** | Nested structure | Variable | Variable | Tables contain nested externals |
| **IPCServer** | `idserverprocessor` (11) | N/A | Not loaded | None | Few | Network resource, not materialized |
| **HTML** | `idhtmlprocessor` (12) | Deferred | Lazy | Medium | Few | Static content, parsed on demand |

### 1.2 Memory Characteristics

**Eager-Load Types** (Load immediately, low total memory):
- Outlines: Compact tree structure, typically 10-500KB each
- Scripts: Compiled bytecode, typically 50-200KB each
- Pictures: Icon/bitmap data, 100KB-10MB each (but few in production)

**Deferred-Load Types** (Load on-demand, high total memory):
- WPText: Paige memory structures + styles + graphics, **5-50MB per object**, hundreds in production
- Menus: Menu records + callbacks + strings, **1-5MB per object**, dozens to hundreds in production

**Why Deferred?**
- Production databases may contain **200+ WPText objects** requiring **500MB-2GB** if all loaded eagerly
- Loading all WPText upfront causes memory exhaustion and hangs
- Lazy loading trades CPU (on-demand deserialization) for RAM (load only what's needed)
- Pattern validated on real Frontier.root databases with hundreds of WPText objects

### 1.3 The External Variable State Machine

```
State 1: On-Disk (flinmemory=0)
├─ variabledata = database address (32-bit v6 or 64-bit v7 BE)
├─ oldaddress = original database address (or nildbaddress if cleared)
├─ Memory cost: ZERO (just metadata)
└─ Actions: Can load into memory, can pack to new address

State 2: In-Memory (flinmemory=1)
├─ variabledata = native memory pointer (64-bit)
├─ oldaddress = original disk address (to detect if address changed)
├─ Memory cost: FULL (entire object in RAM)
└─ Actions: Can pack, can unload to disk, can serialize to v7 format

State 3: Materialized for Migration (adapter_repack=1)
├─ flinmemory=1 (in memory)
├─ oldaddress = nildbaddress (cleared to force fresh allocation)
├─ New address = fresh v7 database block
└─ Result: v6 address → v7 address migration complete
```

---

## 2. Loading Patterns

### 2.1 Single Decision Point Pattern

**Principle**: Loading decisions are made in ONE place, NOT scattered across type-specific code paths.

**Location**: `langexternalpack_internal()` in `Common/source/langexternal.c`

**Decision Tree**:
```
langexternalpack_internal(hv):
├─ Is flinmemory=1? (already in memory)
│  └─ YES: Skip loading, go to packing
│
└─ NO: Must load from disk
   ├─ Is adapter_repack=1? (migrating)
   │  ├─ YES: Use v6 reader to load from source database
   │  └─ NO: Use current database reader
   │
   └─ Call ensure_external_in_memory()
      └─ Type dispatcher selects loader:
         ├─ opverbinmemory() for outlines/scripts
         ├─ wpverbinmemory() for WPText
         ├─ menuverbinmemory_context() for menus
         ├─ tableverbinmemory() for tables
         └─ pictverbinmemory() for pictures
```

**Code Structure**:
```c
boolean langexternalpack_internal(...) {
    // SINGLE DECISION POINT: Load if needed
    if (adapter_repack && !(**hv).flinmemory) {
        // Set v6 read mode
        db_context_apply(&legacy_context);  // Mode = v6

        // Load external (type dispatcher)
        ensure_external_in_memory(hv);

        // Switch to v7 write mode
        db_context_apply(&working_context);  // Mode = v7
    }

    // Pack with mode set correctly by above decision point
    // Type-specific pack functions: NO mode changes!
    switch ((**hv).id) {
        case idoutlineprocessor:
            opverbpack_internal(...);  // Uses v7 mode set above
            break;
        // ... other types
    }
}
```

**Benefits**:
1. **Deterministic**: Exactly one place makes the loading decision
2. **Auditable**: Single breakpoint location for mode debugging
3. **Maintainable**: Adding new types = one new case in `ensure_external_in_memory()`
4. **Thread-safe**: No race conditions from multiple decision points

### 2.2 Eager Loading Pattern (Outlines, Scripts, Pictures)

**When**: Loading small, non-memory-intensive objects where eager load is acceptable

**Code Pattern**:
```c
// In opverbinmemory():
static boolean opverbinmemory(const db_context *ctx, hdlexternalvariable hv) {
    if ((**hv).flinmemory)
        return true;  // Already loaded

    dbaddress adr = (dbaddress) (**hv).variabledata;
    hdloutlinerecord ho;

    // Load outline from disk (uses current mode set by caller)
    if (!oploadoutlinerecord_internal(&adr, &ho))
        return false;

    // Update state
    (**hv).variabledata = (long) ho;
    (**hv).oldaddress = adr;  // Remember original address
    (**hv).flinmemory = true;  // Now in memory

    return true;
}
```

**Characteristics**:
- Accepts `const db_context *ctx` parameter for information
- Assumes caller has set correct read mode (v6 for migration, current for normal ops)
- No mode management (uses global mode set by caller)
- Updates `flinmemory`, `variabledata`, `oldaddress` atomically
- Returns false if load fails (corrupt data, I/O error)

**Applicable To**: Outlines, scripts, pictures (low memory overhead)

### 2.3 Deferred Loading Pattern (WPText, Menus)

**When**: Loading objects that can accumulate to excessive memory (hundreds of MB or GB)

**Architecture Decision**: Load on-demand during packing rather than upfront during materialization

**Rationale**:
1. Production databases may contain 200-500 WPText objects
2. Each object can consume 5-50MB (Paige memory + styles + embedded graphics)
3. Eager loading upfront causes memory exhaustion (tested: system runs out of RAM and hangs)
4. Deferred loading trades CPU cost (deserialize on-demand) for RAM (load only packed items)
5. Pattern validated on real-world databases with hundreds of WPText objects

**Code Pattern**:
```c
// In db_format.c during materialization phase:
db_format_force_materialize_external_tables_recursive() {
    // For WPText and menus: DON'T load, just clear oldaddress
    case idwordprocessor:  // WPText
    case idmenuprocessor:  // Menu
        (**hv).oldaddress = nildbaddress;  // Force fresh v7 allocation
        log_debug(..., "deferred loading for type=%d", var_id);
        // Do NOT call ensure_external_in_memory() here
        break;
}

// Later, during packing phase:
langexternalpack_internal() {
    // Packing code calls ensure_external_in_memory()
    // Only objects being packed are loaded into memory
    // Objects not packed are never loaded (saves RAM)
}
```

**On-Demand Loading**:
```c
// In ensure_external_in_memory():
case idwordprocessor:
    return wpverbinmemory(ctx, (hdlwordprocessor) hv);

// In wpverbinmemory():
boolean wpverbinmemory(const db_context *ctx, hdlwordprocessor hv) {
    if ((**hv).flinmemory)
        return true;

    dbaddress adr = (dbaddress) (**hv).variabledata;
    Handle htext;

    // Load WPText from disk (uses caller's mode - v6 for migration)
    if (!wploadtext_internal(&adr, &htext))
        return false;  // Corrupt or missing

    // Update state and return
    (**hv).variabledata = (long) htext;
    (**hv).oldaddress = adr;
    (**hv).flinmemory = true;
    return true;
}
```

**Error Handling**:
```
If loading fails during deferred phase:
├─ ensure_external_in_memory() returns false
├─ Packing operation detects failure immediately
├─ Migration aborts with diagnostic showing which object failed
└─ User can investigate corrupt object in source database
```

**Advantages**:
- **Memory efficient**: Only loaded objects in memory at any time
- **Fails fast**: Corrupt objects detected during packing, not upfront
- **Selective**: Migration can pack subset of database if needed
- **Real-world validated**: Works with 200+ WPText objects

**Applicable To**: WPText, menus (high memory overhead per object)

### 2.4 Recursive Loading Pattern (Tables)

**Special Case**: Tables are externals that contain nested externals

**Loading Strategy**:
```c
case idtableprocessor:
    // Load the table itself
    if (!tableverbinmemory_internal(ctx, hv))
        return false;

    // Recursively materialize nested externals in that table
    // This handles system.verbs.globals, system.compiler.files, etc.
    if (!db_format_force_materialize_external_tables_recursive(
            (hdlhashtable)(**hv).variabledata)) {
        return false;
    }
    break;
```

**Implementation Note**: `db_format_force_materialize_external_tables_recursive()` walks the table's external variables and materializes **nested table externals** (recursive) but **clears oldaddress for leaf externals** (menus, WPText, pictures) without loading them.

---

## 3. Context Passing vs. Mode Stack Management

### 3.1 The Problem: Push/Pop Anti-Pattern

**What Doesn't Work**:
```c
// WRONG: Global mode stack management scattered across functions
void functionA() {
    db_format_mode_push(&v6_mode);
    {
        functionB();  // What mode will B see?
        functionC();  // What about C?
    }
    db_format_mode_pop();
}

void functionB() {
    db_format_mode_push(&v7_mode);  // Pushes again?
    {
        read_data();  // Is this v6 or v7?
    }
    db_format_mode_pop();  // Or restores wrong thing?
}
```

**Issues**:
1. **Implicit state**: Functions don't know what mode they'll see
2. **Unbalanced pops**: Easy to forget pop if function returns early
3. **Mode flip-flop**: Each function might push again, creating unpredictable stack
4. **Non-local bugs**: Problem manifests in unrelated code that sees wrong mode

### 3.2 The Solution: Explicit Context Passing

**What Works**:
```c
// CORRECT: Context passed explicitly, functions document assumptions
boolean load_from_database(const db_context *ctx, hdldata hd) {
    /*
     * Preconditions:
     *   - ctx->mode specifies read format (v6 or v7)
     *   - caller has validated mode is correct
     * Postconditions:
     *   - If returns true: data loaded into memory
     *   - Global mode unchanged
     */

    // Use context for information, don't change mode
    dbaddress adr = lookup_address_in_context(ctx);

    // Actual read uses current global mode (set by caller)
    return read_data(&adr);
}
```

**Benefits**:
1. **Explicit**: Caller passes what context object needs
2. **Documentable**: Preconditions/postconditions clearly stated
3. **Defensive**: Function validates assumptions about context
4. **Auditable**: Easy to trace context flow through call chain

### 3.3 Implementation Checklist for Refactoring

When refactoring external loading code (issue #136):

**For Type-Specific Loaders** (opverbinmemory, wpverbinmemory, etc.):
- [ ] Add `const db_context *ctx` parameter
- [ ] Document preconditions: "Assumes caller set correct read mode"
- [ ] **NEVER** call `db_format_mode_push/pop()`
- [ ] **NEVER** call `db_context_apply()`
- [ ] Use context for **information only** (don't use to apply mode)
- [ ] Let global mode (set by caller) control actual I/O

**For Packing Functions** (opverbpack_internal, wpverbpack_internal, etc.):
- [ ] Add `const db_context *ctx` parameter for information
- [ ] **NEVER** call `db_format_mode_push/pop()`
- [ ] **NEVER** call `db_context_apply()`
- [ ] Assume `flinmemory=1` (precondition: caller loaded)
- [ ] Use current global mode (set by caller) for packing

**For Mode Manager** (langexternalpack_internal):
- [ ] This is the **ONE place** where mode changes
- [ ] Set v6 mode before calling `ensure_external_in_memory()`
- [ ] Switch to v7 mode before calling type-specific pack functions
- [ ] Clear on error paths: always restore mode even on failure

---

## 4. Migration-Specific Patterns

### 4.1 Address Space Transitions

During v6→v7 migration, external variables traverse four address spaces:

**Space #1**: On-disk v6 (32-bit LE)
- Stored in v6 database file: `0x0062b8d9`
- External state: `flinmemory=0, variabledata=0x0062b8d9`

**Space #2**: In-memory (64-bit native)
- Loaded into RAM: `0x600001234567`
- External state: `flinmemory=1, variabledata=(pointer)`

**Space #3**: Serialized for v7 (64-bit aligned structures)
- Packed in memory ready for v7 format
- Intermediate state before writing to v7 file

**Space #4**: On-disk v7 (64-bit BE)
- Written to v7 database: `0x000000000062b8d9`
- External state: `oldaddress=(v7 address)`

### 4.2 The Critical Transition: Space #1 → Space #2

**What Must Happen**:
```
v6 disk address (Space #1)
    ↓ (read v6 format with v6 reader mode)
in-memory object (Space #2)
    ↓ (pack to v7 format with v7 writer mode)
v7 disk address (Space #4)
```

**What Can Go Wrong**:
1. **Missing loader**: External refuses to load (menu case before fix)
2. **Wrong mode during load**: Read v6 address with v7 reader → garbage
3. **Wrong mode during pack**: Pack v7 data with v6 writer → truncated/corrupted
4. **Mode not restored**: Next operation sees wrong mode → cascade failures

### 4.3 Materialization Strategy

**Approach A**: Eager Materialization (outlines, scripts, pictures)
```
During materialization phase:
├─ Load all externals into memory
├─ Clear oldaddress (force fresh v7 allocation)
└─ Result: All objects ready for packing, no disk reads during packing
```

**Approach B**: Deferred Materialization (WPText, menus)
```
During materialization phase:
├─ Clear oldaddress (force fresh v7 allocation)
├─ DO NOT load into memory
└─ During packing phase:
   ├─ Check flinmemory=0
   ├─ Load from v6 disk (v6 mode)
   ├─ Pack to v7 (v7 mode)
   └─ Result: Only packed objects loaded, memory-efficient
```

**Approach C**: Recursive Materialization (tables)
```
During materialization phase:
├─ Load table into memory
├─ Recursively materialize nested externals
│  ├─ For nested tables: recurse
│  ├─ For leaf externals: clear oldaddress (defer loading)
│  └─ No circular infinite recursion (tables never nest themselves)
└─ Result: Table structure loaded, leaf objects deferred
```

---

## 5. Known Issues and Solutions

### Issue #1: Static Function Visibility

**Problem**: Helper functions like `menuverbinmemory()` are static, not accessible from `langexternal.c`

**Solution**: Export as public `_context` variant:
```c
// In menuverbs.h
boolean menuverbinmemory_context(const db_context *ctx, hdlmenuvariable hv);

// In menuverbs.c
boolean menuverbinmemory_context(const db_context *ctx, hdlmenuvariable hvariable) {
    // Implementation that accepts context parameter
}
```

**Applied To**: All external types (opverbinmemory, wpverbinmemory, tableverbinmemory, menuverbinmemory_context, pictverbinmemory)

### Issue #2: Mode Flip-Flopping

**Problem**: Multiple functions managing mode independently cause unpredictable state

**Solution**: Single decision point in `langexternalpack_internal()`, child functions accept context but never call `db_context_apply()`

**Validation**: Migration log should show mode changes ONLY at decision point:
```
Initial state: use_64bit=0 (v6 reader)
Decision: Load external from v6 → call ensure_external_in_memory()
Decision: Pack to v7 → switch to use_64bit=1
Pack functions: use v7 mode (set by decision point)
```

NOT:
```
... mode=v6 ...
... mode=v7 ...
... mode=v6 ...
... mode=v7 ... (repeated flip-flopping = WRONG)
```

### Issue #3: Incomplete Materialization

**Problem**: `db_format_force_materialize_external_tables_recursive()` only loaded table externals, not leaf externals like menus

**Solution**: Clear `oldaddress` for leaf externals without loading them, rely on deferred loading during packing

**Result**: Selective materialization strategy - only load what's actually needed

### Issue #4: Missing Menu Externals in Migration

**Problem**: `ensure_external_in_memory()` had explicit `return false` for menus

**Root Cause**: Function marked as incomplete (TODO comment), but nobody connected menu loading code to migration path

**Solution**: Export `menuverbinmemory_context()` and call it from `ensure_external_in_memory()`

**Code Change** (3 locations):
1. `menuverbs.h`: Add declaration for `menuverbinmemory_context()`
2. `menuverbs.c`: Rename/export function to accept `const db_context *ctx`
3. `langexternal.c`: Call it from `ensure_external_in_memory()` menu case

---

## 6. Type-Specific Implementation Details

### 6.1 Outlines (idoutlineprocessor)

**Loading**: Eager (low memory, load immediately)
```c
opverbinmemory(const db_context *ctx, hdlexternalvariable hv) {
    // Load outline record from disk
    // Update flinmemory=1, variabledata=(pointer)
}
```

**Packing**: Uses outline record in memory, packs structure
```c
opverbpack_internal(const db_context *ctx, ...) {
    // Assumes flinmemory=1
    // Calls opverbpackoutline() to serialize
    // Assigns to database block
}
```

**File References**:
- `Common/source/opverbfunc.c` (loading)
- `Common/source/opverbpack.c` (packing)

### 6.2 Scripts (idscriptprocessor)

**Loading**: Eager (low memory, compiled bytecode)
```c
// Similar pattern to outlines
opverbinmemory() for scripts
```

**Packing**: Scripts pack as bytecode
```c
// Script packing during migration
```

**File References**:
- `Common/source/opverbfunc.c`
- `Common/source/opverbpack.c`

### 6.3 WPText (idwordprocessor)

**Loading**: Deferred (high memory, load on-demand)
```c
wpverbinmemory(const db_context *ctx, hdlwordprocessor hv) {
    // Load WPText from disk using Paige library
    // Handle Paige memory structures (graphics, styles, etc.)
    // Update flinmemory=1, variabledata=(Handle to text)
}
```

**Packing**: Serializes Paige structures + RTF data
```c
wpverbpack_internal(const db_context *ctx, ...) {
    // Assumes flinmemory=1
    // Calls wpverbpacktext() to serialize + RTF
    // Assigns to database block
}
```

**Migration Specifics**:
- v6 format: Paige native format
- v7 format: RTF in UTF-8 (no font/size/style except in RTF)
- Deferred loading: Only load if packing (don't load all 200+ objects upfront)

**File References**:
- `Common/source/wpverbfunc.c` (loading)
- `Common/source/wpverbpack.c` (packing)

### 6.4 Menus (idmenuprocessor)

**Loading**: Deferred (medium memory, load on-demand)
```c
menuverbinmemory_context(const db_context *ctx, hdlmenuvariable hv) {
    // Load menu record from disk
    // Load menu callbacks, strings, hierarchy
    // Update flinmemory=1, variabledata=(hdlmenurecord)
}
```

**Packing**: Serializes menu structure
```c
menuverbpack(hv, hpacked, flnewdbaddress) {
    // Assumes flinmemory=1 (caller loaded via ensure_external_in_memory)
    // Pack menu record structure
    // Write to database block
}
```

**Special Notes**:
- Menu callbacks are stored as database addresses (must be repointed during migration)
- Menu strings are embedded in menu record
- Hierarchical structure (submenus)

**File References**:
- `Common/source/menuverbs.c` (loading + packing)
- `Common/headers/menuverbs.h` (export declarations)

### 6.5 Pictures (idpictprocessor)

**Loading**: Eager (medium memory, low count)
```c
pictverbinmemory(const db_context *ctx, hdlpictprocessor hv) {
    // Load picture record (icon/bitmap data)
    // Update flinmemory=1
}
```

**Packing**: Serializes picture data
```c
pictverbpack(hv, hpacked, flnewdbaddress) {
    // Assumes flinmemory=1
    // Serialize picture data (may be embedded or external)
}
```

**File References**:
- `Common/source/pictverbs.c`

### 6.6 Tables (idtableprocessor)

**Loading**: Recursive (load table + nested externals)
```c
tableverbinmemory_internal(const db_context *ctx, hdlexternalvariable hv) {
    // Load table record (hashtable structure)
    // Table itself is in-memory (hashtable)
    // Recursive: Walk table's external variables
    //   ├─ For nested tables: recursively load + materialize
    //   └─ For leaf externals: clear oldaddress (deferred)
}
```

**Packing**: Recursively packs table structure
```c
tableverbpack_internal(const db_context *ctx, hv, ...) {
    // Assumes flinmemory=1 (table loaded)
    // Recursively pack nested externals
    // Serialize table structure
}
```

**File References**:
- `Common/source/tableverbfunc.c` (loading)
- `Common/source/tablepack.c` (packing)

---

## 7. Implementation Roadmap for Issue #136

### Phase 1: Context Passing Conversion (Immediate)

**Goal**: Convert all type-specific loaders to accept `const db_context *ctx`

**Files to Modify**:
1. `Common/source/opverbfunc.c` - opverbinmemory()
2. `Common/source/wpverbfunc.c` - wpverbinmemory()
3. `Common/source/menuverbs.c` - menuverbinmemory_context() (export public version)
4. `Common/source/tableverbfunc.c` - tableverbinmemory()
5. `Common/source/pictverbs.c` - pictverbinmemory()

**Changes**:
- Add `const db_context *ctx` parameter
- Remove all `db_format_mode_push/pop()` calls
- Remove all `db_context_apply()` calls
- Document precondition: "Assumes caller set correct read mode"

**Validation**: Compile without errors, existing tests pass

### Phase 2: Centralize Loading Dispatcher (Follow-up)

**Goal**: Ensure `ensure_external_in_memory()` is the single decision point

**Files to Modify**:
1. `Common/source/langexternal.c` - ensure_external_in_memory()

**Validation**: All external types load through single function

### Phase 3: Eliminate Type-Specific Mode Management (Cleanup)

**Goal**: Remove obsolete push/pop code scattered through menuverbpack, pictverbpack, etc.

**Example**:
```c
// BEFORE (wrong pattern):
boolean menuverbpack(hdlmenuvariable hv, ...) {
    if (adapter_repack && !(**hv).flinmemory) {
        db_format_mode_push(&v6_mode);  // ← WRONG
        menuverbinmemory(hv);
        db_format_mode_pop();
    }
    // Pack menu
}

// AFTER (correct pattern):
boolean menuverbpack(hdlmenuvariable hv, ...) {
    // Precondition: flinmemory=1 (caller ensured)
    assert((**hv).flinmemory);

    // Pack menu (mode already set correctly by caller)
}
```

**Validation**: Zero push/pop calls in type-specific pack functions, tests pass

### Phase 4: Comprehensive Testing (Validation)

**Goal**: Ensure all external types work correctly with new architecture

**Test Coverage**:
- [ ] Outline externals load + pack
- [ ] Script externals load + pack
- [ ] WPText externals deferred load + pack
- [ ] Menu externals deferred load + pack
- [ ] Picture externals load + pack
- [ ] Table externals with nested externals load + pack
- [ ] Migration: all types migrate v6→v7
- [ ] Lazy loading: externals load on-demand in v7

**Validation**: `./tools/run_headless_tests.sh` passes, zero regressions

---

## 8. Migration Knowledge Applied

The patterns in this document were discovered and validated during v6→v7 database migration work (December 2025):

### Key Insights
1. **Deferred Loading**: WPText and menus cannot be eagerly loaded (memory exhaustion on real databases)
2. **Single Decision Point**: Multiple mode managers cause unpredictable cascading failures
3. **Explicit Context**: Context parameters are clearer and safer than global mode stacks
4. **Materialization Strategy**: Different external types need different loading strategies

### Validation
- Migration tested on production Frontier.root (200+ WPText objects, 100+ menu objects)
- Successfully migrates without memory exhaustion
- All external types materialize correctly
- Pattern scales to larger databases

### Issues Fixed
- #123 (External table migration crash) - Fixed by implementing mode management properly
- #137 (Save-as flag not reset) - Fixed by proper cleanup

---

## 9. Relevant Code Locations

### Core Files
- **`Common/source/langexternal.c`** - Single decision point (langexternalpack_internal, ensure_external_in_memory)
- **`Common/source/db_format.c`** - Materialization strategy (db_format_force_materialize_external_tables_recursive)
- **`Common/source/opverbfunc.c`** - Outline loading
- **`Common/source/wpverbfunc.c`** - WPText loading
- **`Common/source/menuverbs.c`** - Menu loading
- **`Common/source/tableverbfunc.c`** - Table loading
- **`Common/source/pictverbs.c`** - Picture loading

### Test Files
- **`tests/save_migration_tests.c`** - Migration validation
- **`tests/refcon_cli_tests/phase2_serialization.c`** - Refcon serialization tests

### Planning/Documentation
- **`planning/phase3/MIGRATION_FAILURE_ANALYSIS.md`** - Section 13: Deferred loading pattern rationale
- **`planning/architectural_decision_records/mode_management_single_decision_point.md`** - Mode management architecture
- **This document** - Comprehensive external object loading architecture

---

## 10. Key Takeaways for Issue #136

When auditing external object processing for push/pop anti-patterns (#136):

1. **Look For**: `db_format_mode_push()` and `db_format_mode_pop()` calls in type-specific loaders
   - These should ONLY appear in `langexternalpack_internal()`, nowhere else

2. **Target**: Move mode management out of individual loader functions
   - Consolidate in single decision point

3. **Pattern**: Convert to explicit context passing
   - Add `const db_context *ctx` parameters
   - Document preconditions clearly

4. **Strategy**: Different external types may need different loading approaches
   - Eager: Small objects (outlines, scripts, pictures)
   - Deferred: Large/numerous objects (WPText, menus)
   - Recursive: Complex structures (nested tables)

5. **Validation**: Pattern works at production scale
   - Tested with real databases containing hundreds of large externals
   - No memory exhaustion, regressions, or data corruption

---

**Document Status**: Active reference for issue #136 refactoring work
**Last Updated**: 2025-12-24
**Next Review**: After issue #136 implementation
