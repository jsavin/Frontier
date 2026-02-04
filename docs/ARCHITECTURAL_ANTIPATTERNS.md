# Architectural Anti-Patterns

**Reference document for avoiding known pitfalls in Frontier codebase.**

Read this document when:
- Debugging crashes in hash table, database, or verb resolution code
- Modifying `langexternalgettable()`, `langgetdotparams()`, or `langhandlercall()`
- Working on migration code (v6→v7)
- Encountering "mode stack" or "context guard" patterns
- Adding or modifying global state
- Any significant refactor
- Any time you need to deep-dive on data structures

---

## Name Resolution vs Verb Dispatch - Different Code Paths ⚠️

**Added**: 2026-01-26 (PR #352)

**CRITICAL**: Frontier has two separate mechanisms that must not be confused:

1. **Name Resolution** (`langexternalgettable`, `langgetdotparams`)
   - Used for: `parentOf()`, `typeOf()`, `defined()`, address resolution
   - Should find DATABASE tables via `system.paths`
   - EFP tables should NOT be prioritized here

2. **Verb Dispatch** (`langhandlercall`)
   - Used for: Actually calling verbs like `string.mid()`, `file.exists()`
   - Has its own search order that checks efptable AFTER system.paths
   - This is where EFP valueroutine callbacks are needed

**The Bug Pattern (PR #352)**:
```c
// ❌ WRONG - searching EFP in name resolution function
boolean langexternalgettable(bigstring bs, hdlhashtable *htable) {
    // Explicit efptable search here breaks introspection
    if (hashtablelookup(efptable, bs, ...)) { ... }
}
```

**Why This Matters**:
- Introspection (`parentOf`, `typeOf`) needs database paths, not EFP internal paths
- EFP stubs are implementation details for verb dispatch
- Adding EFP search to name resolution causes: `parentOf(string.mid)` → `system.compiler.["kernel"].string` instead of `system.verbs.builtins.string`

**Rule**: Keep EFP table searches in `langhandlercall()` only. Don't add them to name resolution functions.

### Why This Bug Was Hard to Find (PR #352 Lessons)

**The mistake seemed reasonable at the time**:
- Headless mode needed kernel verbs to work without system.paths
- Adding explicit EFP search to `langexternalgettable()` seemed like a logical fix
- The ~60 lines of code looked correct in isolation
- Verb dispatch (calling `string.mid()`) continued to work correctly

**The actual problem**:
- Name resolution and verb dispatch are **separate concerns**
- Introspection needs database table structure, not EFP implementation details
- `langhandlercall()` already had correct EFP search for verb dispatch
- The explicit EFP search in `langexternalgettable()` was redundant AND wrong

**Key principle**: **EFP stubs are implementation details, not the canonical location**
- User code should see `system.verbs.builtins.string`, not `system.compiler.["kernel"].string`
- Only verb dispatch needs to know about EFP internal structure
- Introspection should reflect the logical database structure

**How to recognize this pattern**:
- If you're adding EFP table searches to name resolution functions → STOP
- If verb dispatch works but introspection is wrong → Check if EFP search is in wrong place
- If `parentOf()` or `typeOf()` returns EFP internal paths → EFP search is prioritized too early

**Files**:
- `Common/source/langexternal.c` - Name resolution (NO efptable search)
- `Common/source/langvalue.c` - `langhandlercall()` has explicit efptable search (correct)
- `docs/VERB_RESOLUTION_ARCHITECTURE.md` - Full architecture documentation

---

## Hash Table Lookup API - Null Pointer Gotcha ⚠️

**Issue #199 Root Cause**: Frontier has two hash lookup functions with subtle differences:

**Correct API**:
```c
// When you need BOTH value and node:
tyvaluerecord val;
hdlhashnode node;
if (hashtablelookup(htable, name, &val, &node)) { /* Safe */ }

// When you ONLY need the node:
hdlhashnode node;
if (hashtablelookupnode(htable, name, &node)) { /* Safe */ }
```

**WRONG - Causes Segfault**:
```c
// ❌ NEVER pass nil for vreturned:
if (hashtablelookup(htable, name, nil, &node)) {  // CRASHES!
    // hashtablelookup unconditionally dereferences vreturned
}
```

**Why**: `hashtablelookup()` unconditionally writes `*vreturned = value` without checking if `vreturned` is non-null.

**The Fix**: Use `hashtablelookupnode()` when you only need the node, not the value.

---

## Mode Stack Push/Pop Issues ⚠️

The `db_format_mode_current()` push/pop pattern has caused multiple bugs:

**Problem**: When you push a different mode (e.g., legacy reader for v6 tables), recursive operations inherit that mode. If you forget to pop, or if recursive calls don't pop properly, child operations see wrong format state.

**Example (Issue #123)**: During migration, we pushed `legacy_load.use_64bit_format = false` to read v6 tables, but recursive child table packing inherited this mode and wrote v4 headers instead of v5.

**Best Practices**:
1. Use explicit context guards (`db_context_guard`) when switching modes for recursive operations
2. Never rely on mode stack state being restored automatically
3. Consider using explicit context parameters instead of global mode state
4. When in doubt, check `db_format_mode_current()` at the point where it's used

---

## Reader/Writer Fork Architecture

Frontier has separate legacy (v6, 32-bit) and modern (v7, 64-bit BE) reader/writer code paths.

**Gotcha 1**: DATABASE format mode and TABLE header version are NOT the same:
- `dbopenfile()` sets `db_format_mode.use_64bit_format` based on DATABASE version
- `hashunpacktable()` used to check only TABLE header version, not database mode
- Result: Root table could unpack with wrong reader even if database is v7

**Gotcha 2**: Table packing must respect OUTPUT database format:
- Don't rely on mode stack state inherited from earlier operations
- Explicitly push modern mode before packing if writing to v7 database
- Always validate header versions (version=5 for v7, version=4 for v6)

---

## External Table Variable Migration

External table variables store either:
- Memory pointers (`flinmemory=1`) - no migration issues
- Database addresses (`flinmemory=0`) - **addresses are format-dependent**

**Critical**: If `flinmemory=0` tables migrated with wrong address format:
- v6 addresses (32-bit) stored in v7 database don't point to valid blocks
- `dbnormalizeaddress()` fails when trying to access them
- Error: `dbnormalizeaddress failed for adr=0x62bb33`

**Safe approach**: Force external tables into memory (`flinmemory=1`) during migration.

See `docs/external_table_variable_management.md` for migration patterns.

---

## Address Value Structure - Migration Gotcha ⚠️

**CRITICAL**: Address values have two parts that MUST be updated together:

```c
// Address value structure:
// [local_name_string][htable_pointer]
```

**The Bug Pattern**:
```c
// ❌ WRONG - only updates pointer, string part still has full path
hdlhashtable *phtable = (hdlhashtable *)((*hstring) + ixtable);
*phtable = htable_resolved;
// String part still has "system.macintosh.globals" instead of "globals"
// Packing will corrupt the path: ["system.macintosh.globals"]
```

**The Correct Pattern**:
```c
// ✅ CORRECT - dispose old value, create new with proper structure
disposehandle((Handle)val->data.addressvalue);
tyvaluerecord val_new;
setexemptaddressvalue(htable_resolved, bs_local_name, &val_new);
*val = val_new;
```

**Why This Matters**:
- `getaddresspath()` extracts the string part during packing
- If string has full path, packing adds brackets: `["system.macintosh.globals"]`
- Migration from v6→v7 corrupts all address values in `system.paths`

**Rule**: Always use `setexemptaddressvalue()` to create address values. Never manually modify address value handle memory.

**Files**:
- `Common/source/tablestructure.c:367-402` (resolve_system_paths)
- `Common/source/langvalue.c` (setexemptaddressvalue, getaddresspath)
- Issue #336, commit 9e59c0a9

---

## Table Lookup Search Order - langgettableval() ⚠️

**Pattern**: When resolving table children (e.g., `builtins.webserver.init`), search INSIDE the provided table first, then fall back to external lookup.

**The Bug Pattern**:
```c
// ❌ WRONG - only searches external tables
static boolean langgettableval(hdlhashtable htable, bigstring bsname, hdlhashtable *hval) {
    pushhashtable(htable);
    boolean fl = langexternalgettable(bsname, hval);  // Only external lookup
    pophashtable();
    return fl;
}
// This fails for nested children like "init" inside "builtins.webserver"
```

**The Correct Pattern**:
```c
// ✅ CORRECT - search inside table first, fall back to external
static boolean langgettableval(hdlhashtable htable, bigstring bsname, hdlhashtable *hval) {
    pushhashtable(htable);

    // First: search INSIDE the provided table
    if (hashtablelookup(htable, bsname, &val, &hnode)) {
        fl = tablevaltotable(val, hval, hnode);
    }
    else {
        // Fallback: external lookup (preserves backward compatibility)
        fl = langexternalgettable(bsname, hval);
    }

    pophashtable();
    return fl;
}
```

**Why Use hashtablelookup() Instead of langsymbolreference()**:
- `langsymbolreference()` raises errors if not found
- Errors prevent fallback to `langexternalgettable()`
- `hashtablelookup()` returns false silently, allowing fallback

**Use Cases**:
- Resolving terse EFP references: `defined(webserver.init)`
- Nested table lookups during verb resolution
- system.paths traversal

**Files**:
- `Common/source/langvalue.c:3651-3673` (langgettableval fix)
- Issue #336, commit 9e59c0a9

---

## Global Mutable State - CRITICAL FOR LAUNCH ⚠️⚠️⚠️

**BURN THE GLOBALS WITH FIRE. EVERYWHERE.**

Frontier has multiple global mutable state variables that must be eliminated before launch:

**Known Problem Areas:**
- `outlinedata` and `outlinestack` (oppushoutline/oppopoutline) - outline context
- `databasedata` and legacy database globals - database context (partially fixed)
- `flnextparamislast` and parameter-related globals - **PATTERN ESTABLISHED** (see ADR-005)
- Any static buffers or caches that aren't guarded by locks

**Why This Matters**: This code MUST be thread-safe before launch. Global mutable state makes thread safety impossible.

**Refactoring Patterns (proven to work)**:

**Pattern 1: Thread-Local Storage (for per-thread state)**
1. Add field to `tythreadglobals` structure
2. Update thread swap functions (copythreadglobals, swapinthreadglobals)
3. Replace global with macro accessor for backward compatibility
4. Zero API changes - transparent to existing code

**Pattern 2: Explicit Context (for per-operation state)**
1. Create explicit context structure (e.g., `op_context`, `db_context`)
2. Thread context through function parameters instead of relying on globals
3. Maintain backward-compatible wrappers using default context
4. Gradually eliminate global variable access

**When to Use Which**:
- **Thread-Local**: Per-thread execution state (flnextparamislast, flscriptrunning, current outline)
- **Explicit Context**: Per-operation state (database operations, outline operations)

**Documentation**:
- **ADR-005**: Parameter state thread-safety (thread-local pattern reference)
- **docs/THREAD_LOCAL_GLOBALS_PATTERN.md**: Step-by-step migration template
- **Issue #135**: Outline context refactoring (explicit context pattern reference)

---

## Timestamp Type Migration ⚠️

Frontier uses 64-bit timestamps (`frontier_time_t` = `int64_t`) to avoid Year 2038. When pulling new source, audit for uint32_t usage. Test suite automatically checks this.

**Rule**: Legacy disk readers (v4/v6) can use uint32_t; everything else must use int64_t or `frontier_time_t`.

See [`docs/TIMESTAMP_AUDIT.md`](docs/TIMESTAMP_AUDIT.md) and `docs/frontier_time_t_standard.md`

---

## Context Guard Pattern is CORRECT ✅

**IMPORTANT**: The `db_context_guard` pattern is **NOT** the same as the problematic push/pop anti-pattern. Context guards are the **CORRECT** solution:

```c
boolean dbrefhandle_context(const db_context *context, dbaddress adr, Handle *h) {
    db_context_guard guard;
    db_context_guard_enter(context, &guard);  // Saves state
    boolean ok = dbrefhandle(adr, h);
    db_context_guard_exit(&guard);            // Restores state
    return ok;
}
```

**Why guards are correct**:
- Explicitly save state on entry
- Explicitly restore state on exit
- Scoped to single operation (no inheritance to recursive calls)
- Deterministic cleanup even on error paths

**Global State Must Be Restored**:

When temporarily changing global state, **failure to restore previous value causes cascading failures**:

```c
// BROKEN CODE (removed db_context_guard):
boolean dbrefhandle_context(const db_context *context, dbaddress adr, Handle *h) {
    databasedata = context->database;  // Sets global
    return dbrefhandle(adr, h);        // Returns WITHOUT restoring!
}
// Next operation sees WRONG database → segfault
```

**Rule**: Any function that temporarily modifies global state MUST restore previous value before returning.

---

## Auto-Generated Files Requiring Hand-Edits

**CRITICAL ANTI-PATTERN**: Never create automated tools that generate stub files if those stubs will need to be manually edited in production.

### Why This is a Problem

**Bad Outcomes**:
1. ❌ Manual edits get overwritten if generator is re-run
2. ❌ Developers forget this is generated and waste time trying to fix it
3. ❌ Git history becomes confusing
4. ❌ Future developers don't know which version is authoritative
5. ❌ Creates false sense of "this is done" when stub isn't complete

### How to Fix This Pattern

**Option 1: Make The Generator Complete** ✅ **PREFERRED**
- Update generator to emit correct, production-ready code
- No hand-edits needed - regenerate when requirements change
- Example: Updated `stub_config.py` with `STUB_FORWARD` mode

**For Future Work**: When adding verbs that need real implementations:
- Add entry to `stub_config.py` with correct implementation strategy
- Update generator if needed
- **Don't hand-edit the output** - fix the generator instead

### When You Encounter a Generated File That Needs Custom Logic

**CRITICAL DECISION TREE**: If you find a generated file that needs custom implementations:

**Option 1: Fix the Generator** (Preferred)
1. Create wrapper functions in appropriate source files (e.g., `shellsysverbs.c`)
2. Add entries to `stub_config.py` with `STUB_FORWARD` pointing to wrappers
3. Regenerate the file - implementations stay in sync with generator

**Option 2: Stop and Ask**
- If custom logic is too complex to auto-generate
- If you're unsure whether generator can handle it
- STOP and consult with user/maintainers
- Do NOT hand-edit the generated file

**❌ NEVER: Mark Generated File as Production**
- Do NOT change header to say "PRODUCTION IMPLEMENTATION"
- Do NOT add "DO NOT regenerate" warnings to generated files
- This creates a non-scalable pattern and causes confusion
- If file already has production code, it's a special case (see Option 2)

**Why This Matters**:
- Hand-editing generated files is not scalable
- Future developers won't know if file can be regenerated
- Creates confusion about which version is authoritative
- Violates the principle of keeping generator as source of truth

---

## Disk Format Structs - Size Verification and When to Change Types ⚠️

**Added**: 2026-02-03 (Issue #386, PR #387)
**Updated**: 2026-02-04 (Comprehensive audit findings)

### The General Rule

Structs used for disk serialization should use fixed-width integer types (`int32_t`, `int64_t`) instead of platform-dependent types (`long`). However, **most existing disk structs in Frontier are already correct** and should NOT be modified.

### Why Most Disk Structs Don't Need Changes

**Audit Finding (2026-02-04)**: A comprehensive audit of disk format structs found that most already have correct sizes despite using `long`:

| Struct | Expected Size | Actual Size | Status |
|--------|---------------|-------------|--------|
| `tydatabaserecord` | 118 bytes | 118 bytes | ✅ Correct |
| `tydatabaserecord_64` | 90 bytes | 90 bytes | ✅ Correct |
| `tyversion1tablediskrecord` | 152 bytes | 152 bytes | ✅ Correct |
| `tywpheader` | varies | matches expected | ✅ Correct |
| `tydiskpictrecord` | varies | matches expected | ✅ Correct |

**Why They Work Despite `long` = 8 bytes**:

1. **Union Padding Absorbs Size Differences**:
   ```c
   union {
       char growthspace[50];  // This absorbs alignment differences
       struct { ... } extensions;
   } u;
   ```

2. **Explicit Padding Bytes**:
   ```c
   unsigned char _pad[2];  // Explicit padding for alignment
   ```

3. **`#pragma pack(2)` Tight Packing**: Fields are packed at 2-byte boundaries, not natural alignment

4. **Runtime Assertions Validate Sizes**:
   ```c
   assert(sizeof(tydatabaserecord) == 118);  // Validates at runtime
   ```

### The Exception: `tydisktreenode` (Issue #386)

The `tydisktreenode` struct in `langtree.c` WAS different because:
- No union padding or growth space
- Struct size was genuinely wrong on LP64 (16+ bytes instead of 12)
- Breaking the disk format silently

**The Fix Was Correct**:
```c
// ✅ CORRECT - Fixed in PR #387
#pragma pack(2)
typedef struct tydisktreenode {
    short nodetype;
    int32_t nodevalsize;  // Changed from long
    short lnum;
    short charnum;
    short paraminfo;
} tydisktreenode;

_Static_assert(sizeof(tydisktreenode) == 12, "tydisktreenode must be 12 bytes");
```

### Decision Tree: Should I Change `long` to `int32_t`?

**Step 1: Check if the struct has size validation**
```c
assert(sizeof(mystruct) == EXPECTED_SIZE);
```
If yes, and assertion passes → **DO NOT CHANGE**. The struct is already correct.

**Step 2: Check for union padding / growth space**
```c
union { char growthspace[N]; ... } u;
```
If present → **Probably DO NOT CHANGE**. Padding absorbs differences.

**Step 3: Calculate actual vs expected size**
```bash
# In LLDB or test program:
printf("sizeof(mystruct): %zu\n", sizeof(mystruct));
```
If actual == expected → **DO NOT CHANGE**.

**Step 4: Only if size is WRONG**
If struct size doesn't match disk format requirements → Change `long` to `int32_t` AND add `_Static_assert`.

### Anti-Pattern: Blindly Changing `long` to `int32_t`

**❌ WRONG - Breaks working code**:
```c
// Original: sizeof = 118 bytes (CORRECT!)
typedef struct tydatabaserecord {
    ...
    long fnumdatabase;   // 8 bytes on LP64
    long headerLength;   // 8 bytes on LP64
    ...
} tydatabaserecord;

// After blind change: sizeof = 110 bytes (BREAKS DISK FORMAT!)
typedef struct tydatabaserecord {
    ...
    int32_t fnumdatabase;  // 4 bytes
    int32_t headerLength;  // 4 bytes
    ...
} tydatabaserecord;
// Assertion fails: expected 118, got 110
```

**Why This Breaks Things**:
- Field offsets shift (e.g., `headerLength` moves from offset 56 to offset 52)
- Subsequent fields also shift
- Disk reads/writes access wrong byte positions
- Database corruption

### Rules for NEW Disk Format Structs

When creating NEW disk format structs:

1. **Use fixed-width types from `<stdint.h>`**:
   - `int32_t` / `uint32_t` for 4-byte fields
   - `int64_t` / `uint64_t` for 8-byte fields
   - `int16_t` / `uint16_t` for 2-byte fields

2. **Add static assertions**:
   ```c
   _Static_assert(sizeof(mystruct) == EXPECTED_SIZE, "mystruct size mismatch");
   ```

3. **Document size requirements**:
   ```c
   int32_t nodevalsize;  /* Must be 4 bytes for disk format */
   ```

### Files Reference

**Structs with existing size validation (DO NOT MODIFY)**:
- `Common/headers/db.h` - `tydatabaserecord`, `tydatabaserecord_64`
- `Common/headers/tableformats.h` - `tyversion1tablediskrecord`
- `Common/source/wpengine.c` - `tywpheader`, `tyOLD42wpheader`
- `Common/source/pict.c` - `tydiskpictrecord`, `tyOLD42diskpictrecord`

**Structs fixed in PR #387 (reference implementation)**:
- `Common/source/langtree.c` - `tydisktreenode`, `tydisktreerec`, `tyOLD42disktreenode`
