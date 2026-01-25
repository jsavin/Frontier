# ADR-004: Dynamic Verb Binding Architecture

**Status**: `[NOT STARTED]` - Strategic design document for future implementation
**Date**: 2025-12-27
**Author**: Codex
**Related Issues**: #166 (bare verb resolution), verb binding evolution
**Related ADRs**: ADR-003 (address value resolution)

## Context

Currently, bare verb calls like `new(tableType, @t)` fail because `system.paths` entries point to database tables (no valueroutines) instead of EFP (External Function Processor) tables that contain C implementation callbacks.

However, this raises a deeper architectural question: **What should the relationship be between kernel-level verb implementations (C) and UserTalk-level verb bindings?**

### Current Architecture Problems

1. **Hard-coded locations**: EFP tables are created at `system.compiler.["kernel"].lang` regardless of database structure
2. **Special table status**: Multiple tables have implicit special status beyond `root.system`
3. **Limited dynamism**: Cannot easily move verb implementations between databases
4. **Coupling**: Database structure and kernel bindings are tightly coupled

### User's Architectural Vision

> "I want literally everything except keywords and lang to be dynamic. I don't want any table except root.system to have special status."

**Key Requirements:**
- Only `root.system` should be architecturally special (the bootstrap anchor)
- `lang` and `keywords` are fundamental (kernel-bound at startup)
- Everything else should be discoverable, movable, dynamic
- Support multiple databases (system.root + User.root + others)
- Guest databases contribute to global scope via `system.compiler.files`
- Tables can move between databases freely (e.g., root.user → User.root)

## Options

### Option A: Database-Driven Augmentation (Following system.paths)

**Concept**: EFP implementations follow the data, not hard-coded locations.

**Architecture:**
```
During linksystemtablestructure():
1. Read system.paths to discover WHERE verb tables live (data-driven)
2. For each path (e.g., "@system.compiler.lang"):
   - Find or create the table at that location
   - Find matching EFP implementation (if exists)
   - Augment persistent table with EFP entries
   - Mark EFP entries with fldontsave (C pointers don't persist)
3. Result: Database tables + C implementations merged at runtime
```

**Verb Resolution Flow:**
```
Bare verb call: new(...)
  → langsearchpathlookup() walks system.paths
  → Finds system.compiler.lang (persistent table)
  → Table contains "new" entry with valueroutine callback
  → Calls kernel implementation ✓
```

**Persistence:**
```
User adds custom verb:
  system.compiler.lang.myVerb = {custom implementation}

On save:
  - myVerb persists (no valueroutine, pure UserTalk)
  - Built-in verbs DON'T persist (marked fldontsave)

On next load:
  - myVerb still there
  - linksystemtablestructure() re-augments with C implementations
```

**Benefits:**
- ✅ Data-driven: system.paths is source of truth
- ✅ No hard-coded table locations (beyond root.system)
- ✅ User can reorganize system.paths, kernel follows
- ✅ Works with existing v6 databases
- ✅ Mixed C + UserTalk verbs in same table
- ✅ Extensible: add new EFPs without code changes

**Trade-offs:**
- ⚠️ Still have kernel implementations for all verb categories (file, string, etc.)
- ⚠️ C implementations cannot be easily replaced by UserTalk
- ⚠️ Larger kernel surface area
- ⚠️ More code to maintain in C

**Implementation Complexity**: Medium

### Option B: Minimal Kernel + UserTalk Libraries

**Concept**: Only `lang` (and `keywords`) in kernel. Everything else is UserTalk libraries.

**Architecture:**
```
Kernel bindings (C implementations):
  - lang.* (fundamental object operations: new, delete, typeof, etc.)
  - keywords.* (if, while, loop, return, etc.)
  - THAT'S IT

UserTalk libraries (pure UserTalk or thin C wrappers):
  - string.* → UserTalk library or minimal C helpers
  - file.* → UserTalk library calling OS primitives
  - xml.* → Pure UserTalk
  - etc.

During startup:
1. Link internaltable (lang + keywords only)
2. Load standard libraries from system.verbs.* or similar
3. Libraries are just UserTalk code (discoverable, replaceable)
```

**Verb Resolution Flow:**
```
Bare verb call: string.upper(...)
  → langsearchpathlookup() walks system.paths
  → Finds system.verbs.string (UserTalk library)
  → Calls UserTalk implementation (or thin C wrapper)
  → No kernel coupling ✓
```

**Persistence:**
```
Everything persists naturally:
  - Libraries are just tables in the database
  - User can modify standard libraries
  - User can add new libraries
  - No special fldontsave logic needed
```

**Benefits:**
- ✅ Minimal kernel surface (only lang + keywords)
- ✅ Maximum flexibility: everything is UserTalk-visible
- ✅ Libraries can be in ANY database (loaded via system.compiler.files)
- ✅ Easy to move tables between databases
- ✅ User can replace standard libraries entirely
- ✅ Smaller C codebase to maintain
- ✅ Aligns with "only root.system is special" vision

**Trade-offs:**
- ⚠️ Performance: UserTalk slower than C for some operations
- ⚠️ Migration effort: Convert existing C verbs to UserTalk
- ⚠️ Compatibility: Might break assumptions in legacy code
- ⚠️ Capability: Some OS operations need C (file I/O, etc.)

**Hybrid Approach:**
Keep C implementations available but make them **optional**:
```
C implementations exist in tests/headless_*_verbs.c
  - Available for performance-critical operations
  - Can be linked into kernel if needed
  - But NOT required for basic functionality

UserTalk libraries are the default:
  - Pure UserTalk where possible
  - Thin C wrappers where necessary (file I/O, etc.)
  - User can override with custom implementations
```

**Implementation Complexity**: High (requires significant refactoring)

## Comparison Matrix

| Aspect | Option A (Augmentation) | Option B (Minimal Kernel) |
|--------|-------------------------|---------------------------|
| **Kernel Surface** | Large (all verb categories) | Minimal (lang + keywords only) |
| **Dynamism** | Good (data-driven locations) | Excellent (everything is UserTalk) |
| **Performance** | Fast (C implementations) | Slower (UserTalk, unless C wrappers) |
| **Migration Effort** | Low (works with existing code) | High (rewrite verbs as UserTalk) |
| **Maintainability** | More C code | Less C code, more UserTalk |
| **Flexibility** | Mixed C + UserTalk | Pure UserTalk (replaceable) |
| **Aligns with Vision** | Partially | Fully |
| **Implementation Time** | Days | Weeks/months |

## Recommendation

**Recommended Path: Hybrid Approach Based on Option B**

### Phase 1: Fix Immediate Issue (Option A - Short Term)
Implement database-driven augmentation to get bare `new()` working:
- Read system.paths to find verb table locations
- Augment persistent tables with EFP implementations
- Mark EFP entries as fldontsave
- **Rationale**: Unblocks current work, minimal risk

**Estimated effort**: 1-2 days

### Phase 2: Refactor to Minimal Kernel (Option B - Long Term)

**Step 1: Extract UserTalk Libraries**
- Create `system.verbs.string.*` as UserTalk implementations
- Create `system.verbs.file.*` as thin C wrappers + UserTalk
- Keep C implementations in kernel for performance-critical operations
- Make kernel implementations **pluggable** (can be replaced by UserTalk)

**Step 2: Update Verb Binding**
- Only `lang.*` and `keywords.*` bound at kernel level
- Everything else loaded as UserTalk libraries
- Libraries discoverable via system.compiler.files (multi-database support)

**Step 3: Multi-Database Support**
- Guest databases (User.root, etc.) contribute to global scope
- Tables can move between databases freely
- Only root.system has special architectural status

**Estimated effort**: 4-6 weeks

## Decision Criteria

Choose **Option A** if:
- Need quick fix for bare verb resolution
- Want minimal disruption to existing architecture
- Performance is critical priority
- Limited development time available

Choose **Option B** if:
- Committed to maximum dynamism and flexibility
- Willing to invest in significant refactoring
- Want minimal kernel surface area
- Vision is multi-database, modular architecture

## User Preference

> "I slightly prefer option B honestly, but I want to keep the kernel-level implementations even though we would do only the lang bindings."

**Interpretation**:
- Prefer minimal kernel (lang + keywords only)
- But keep C implementations available for performance
- Make them pluggable/optional, not required

**This suggests**: Hybrid approach - minimal kernel binding, but C implementations available as plugins.

## Implementation Strategy (Recommended)

### Immediate (This Workstream):
1. Implement Option A (database-driven augmentation)
2. Get bare `new()` working
3. Complete UserTalk integration tests (Issue #166)
4. **Validate the pattern** before committing to larger refactor

### Near Term (Next Phase):
1. Create planning doc for Option B transition
2. Prototype UserTalk library for one verb category (e.g., string.*)
3. Design pluggable C implementation architecture
4. Test performance impact

### Long Term (Future):
1. Migrate all non-lang verbs to UserTalk libraries
2. Make C implementations optional plugins
3. Support multi-database architecture
4. Document patterns for custom verb libraries

## Open Questions

1. **Which verb categories are performance-critical?**
   - Candidates: file I/O, database operations, crypto
   - May need C implementations as plugins

2. **How to handle OS-specific operations?**
   - File paths, process management, etc.
   - Thin C wrappers with UserTalk interfaces?

3. **Backward compatibility with v6 databases?**
   - Do we need to support legacy verb table structures?
   - Migration path for existing databases?

4. **Multi-database verb precedence?**
   - If User.root and system.root both define `string.upper()`, which wins?
   - Load order? Explicit precedence in system.compiler.files?

## Success Criteria

**For Option A (Short Term):**
- ✅ Bare `new()` works
- ✅ No hard-coded table locations (data-driven)
- ✅ Existing tests pass
- ✅ System.paths can be reorganized without kernel changes

**For Option B (Long Term):**
- ✅ Only lang + keywords bound at kernel level
- ✅ All other verbs are UserTalk libraries
- ✅ Multi-database support working (User.root example)
- ✅ Tables freely movable between databases
- ✅ Performance acceptable (within 2x of C for most operations)
- ✅ C implementations available as plugins for critical paths

## Related Work

- ADR-003: Address value resolution (enables data-driven verb binding)
- Issue #166: UserTalk integration tests (validates verb resolution)
- `planning/phase3/verb_binding_alternatives.md`: Earlier exploration of verb binding patterns
- Multi-database context work: Foundation for guest database support

## Next Steps

**If Option A approved:**
1. Implement database-driven augmentation in `tablestructure.c`
2. Update `linksystemtablestructure()` to read system.paths
3. Create augmentation logic for EFP tables
4. Test with bare `new()` and other verbs
5. Validate with UserTalk integration tests

**If Option B approved:**
1. Create detailed transition plan
2. Prototype UserTalk library for one verb category
3. Design pluggable C implementation architecture
4. Benchmark performance impact
5. Plan migration timeline

**If hybrid approved:**
1. Do Option A immediately (this workstream)
2. Plan Option B transition (next phase)
3. Design plugin architecture for C implementations
4. Document patterns for future work
