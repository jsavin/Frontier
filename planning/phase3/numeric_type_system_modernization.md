# Numeric Type System Modernization Plan

**Status:** P0 Decision Document (Implementation Ready)
**Created:** 2025-12-06
**Owner:** Frontier Core Team

---

## Executive Summary

This document captures the decision to modernize Frontier's numeric type system from a fragmented 16/32-bit legacy model to a clean, consistent 64-bit precision architecture. This change is fundamental to the runtime implementation and must be finalized before processor verb work begins.

---

## Legacy System Analysis

### Historical Type Architecture (from `tedchoward/Frontier/Common/headers/lang.h`)

The legacy `tyvaluedata` union defined these numeric types:

```c
typedef union tyvaluedata {
    short intvalue;              // intvaluetype (2) - 16-bit signed
    long longvalue;              // longvaluetype (3) - 32-bit signed
    float singlevalue;           // singlevaluetype (23) - 32-bit float
    double **doublevalue;        // doublevaluetype (11) - 64-bit double (Handle)
    // ... plus olddoublevaluetype (24) for compatibility
} tyvaluedata;
```

### Problems with Legacy Architecture

| Issue | Impact |
|-------|--------|
| **16-bit int + 32-bit long** | Inconsistent, confusing type system; two integer types with no clear distinction |
| **32-bit float + 64-bit double** | Two floating point types; 32-bit "single" was for space savings (rare optimization) |
| **No 64-bit integers** | 2040 timestamp overflow; 49-day daemon wraparound on tick/millisecond counters |
| **Mixed precision** | Scripts forced to reason about type widths; error-prone arithmetic mixing |
| **Handle-based doubles** | Inefficient storage for double values |

**Note:** In the modern (v7) on-disk format, `singlevaluetype` payloads are stored in the 64-bit `doublebits` slot (BE64) to keep disk layout contiguous; runtime semantics remain 32-bit to callers.

---

## P0 Decision: 64-Bit Precision Architecture

### Decision 1: All Signed Integers → 64-bit

**Recommendation:** YES ✅ **Proceed with 64-bit signed integers as default**

**Rationale:**
- `clock.now()` must return 64-bit to prevent 2040 timestamp overflow
- `clock.ticks()` and `clock.milliseconds()` must be 64-bit to prevent daemon wraparound (~24-49 days on 32-bit)
- Eliminates integer overflow bugs across entire scripting ecosystem
- Simplifies type system (single `int` type, not int/long distinction)
- Aligns with modern language design (Python 3, JavaScript, Go all use 64-bit by default)
- Maintains backward compatibility: 32-bit values work fine in 64-bit context

**What Changes:**
- ✅ Runtime arithmetic operations: add, subtract, multiply, divide, modulo, comparisons
- ✅ `system.compiler.language.constants.infinity`: 2,147,483,647 → 9,223,372,036,854,775,807
- ✅ Type coercion and implicit conversions
- ✅ Database format: Type storage widths change (handled in v6→v7 migration)
- ❌ Script code: Transparent upgrade (no user code changes needed)

### Decision 2: Floating Point Type Strategy

**Recommendation:** Unify all floating point to 64-bit IEEE 754 double

**Rationale:**
- **Historical reality:** Legacy Frontier defaulted float literals to extended precision (80-bit on Mac/Carbon SANE format, 64-bit IEEE on Windows/GCC due to platform limitations)
- **Platform inconsistency:** Extended precision required conditional compilation and platform-specific storage formats, with Windows already using 64-bit double as fallback
- **No production dependency:** No known UserLand-run production apps relied on extended precision semantics vs. 64-bit precision
- **Portability:** 64-bit IEEE 754 double is universally supported across all platforms (Windows, Mac, Linux, embedded), while extended precision is disappearing from modern hardware
- **Simplicity:** Eliminates platform-specific code paths and type system complexity (no more 32-bit float, 64-bit double, 80-bit extended)
- **Standards alignment:** 64-bit double is the de facto default in modern languages (Python, JavaScript, Go, Rust)

**Breaking change (documented):** Mac users upgrading from extended precision will see minor precision loss in edge cases (extended precision has ~20 decimal digits; 64-bit double has ~15). This is acceptable because:
  - The precision loss is negligible for typical applications
  - Windows was already using 64-bit, so cross-platform scripts expect this precision level
  - The gain in portability and simplicity outweighs the precision trade-off

**What Changes:**
- ✅ Default floating point type: 64-bit IEEE 754 double (unified across all platforms)
- ✅ Deprecate 32-bit float (remove unless legacy compatibility specifically requires it)
- ✅ Remove extended precision support (platform-specific code paths)
- ✅ Database format: Type storage widths change (handled in v6→v7 migration)
- ✅ Script code: Transparent for most code; documented minor precision change on Mac

### Decision 3: Short Integer Arithmetic Promotion

**Recommendation:** All integer arithmetic (including `short` values) uses 64-bit signed integer semantics

**Rationale:**
- **Historical context:** In legacy Frontier, the default integer type was `long` (32-bit), not `short` (16-bit). Testing confirms `local (n=1)` returns a 32-bit long, indicating short was never the primary integer type.
- **No production dependency:** No known UserLand-run production apps relied on 16-bit overflow semantics for short values.
- **Simplicity & safety:** One consistent integer arithmetic path eliminates subtle bugs from mixing 16-bit and 64-bit operations.
- **Backward compatibility:** The `short` type remains valid as a declaration/storage type; it simply promotes to 64-bit during arithmetic.

**Breaking change (documented):** Bitwise operations (`bit.shiftLeft`, `bit.shiftRight`, etc.) on short values will operate on the promoted 64-bit representation. Scripts expecting 16-bit wraparound behavior will see different results. This is acceptable because:
  - Bitwise operations on shorts are rare edge cases
  - Anyone explicitly using bitwise ops likely expects modern semantics anyway
  - The change is an intentional design decision with clear rationale

**What Changes:**
- ✅ All integer arithmetic: 64-bit operations regardless of source type (short/long)
- ✅ Bitwise operations: Operate on 64-bit promoted values
- ✅ Database format: Type storage widths change (handled in v6→v7 migration)
- ✅ Script code: Transparent for arithmetic; documented change for bitwise ops

---

## Implementation Scope

### 64-bit Integer Work

**Scope:**
1. Update runtime arithmetic operations to use 64-bit signed integer math
   - Addition, subtraction, multiplication, division, modulo
   - Comparisons (==, !=, <, >, <=, >=)
   - Bitwise operations (if applicable)

2. Update `system.compiler.language.constants.infinity`
   - Change from: 2,147,483,647 (max 32-bit signed)
   - Change to: 9,223,372,036,854,775,807 (max 64-bit signed)
   - Used in functions like `string.mid(s, 11, infinity)` for "extend to end" semantics
   - Generated in the code that builds in-memory language constants

3. Testing
   - Comprehensive integer arithmetic edge cases
   - Overflow, underflow, comparison boundary conditions
   - Interaction with floating point conversions
   - Script execution with large values

4. Documentation
   - Update type system documentation
   - Explain integer semantics to script developers
   - Document migration path from legacy (if applicable)

### Floating Point Work

**Scope:**
1. Verify double-precision is default in arithmetic operations
2. Optionally deprecate single-precision `float` (or keep for compatibility)
3. Test floating point arithmetic and conversions
4. Document floating point type strategy

---

## Timeline & Sequencing

| Phase | Timing | Work |
|-------|--------|------|
| **Decide** | Now | Finalize this P0 decision ✅ |
| **Implement** | Early Phase 1 | Update runtime arithmetic operations |
| **Test** | Early Phase 1 | Comprehensive edge case testing |
| **Proceed** | Phase 1 | Begin processor verb implementation |

**Blocking:** This decision blocks processor verb implementation. Must complete before starting verb work.

**Non-Blocking:** Floating point decision is parallel to integer work but has less urgency.

---

## Impact Analysis

### Database Format
- **Storage changes:** Yes — type widths change (short 2→8 bytes, long 4→8 bytes, float 4→8 bytes, exte 10→8 bytes on Mac)
- **Versioning:** No new format version bump; changes handled as part of v6→v7 migration
- **Migration path:** v6 reader must handle legacy byte widths; v7 reader/writer use new 8-byte sizes
- **Legacy compatibility:** v6 format support remains in legacy reader path; v7+ format uses modernized numeric storage

### Scripts
- **User code:** Transparent upgrade (no changes required)
- **Behavior:** Integer operations now overflow/wrap at higher bounds
- **Testing:** Scripts using large numbers may behave differently
- **Edge cases:** Very large integer math (> 2^31-1) now works correctly

### Runtime
- **Arithmetic:** All integer math is now 64-bit
- **Constants:** `infinity` is ~9.2 quintillion instead of ~2.1 billion
- **Performance:** 64-bit ops slightly cheaper on 64-bit platforms (majority case)
- **Portability:** Safe on both 32-bit and 64-bit systems

### Specific Verb Impact

**clock processor:**
- `clock.now()` → Safe timestamps past 2040 ✅
- `clock.ticks()` → No wraparound on long-running daemons ✅
- `clock.milliseconds()` → No wraparound on long-running daemons ✅

**string processor:**
- `string.mid(s, start, infinity)` → Works with 64-bit infinity ✅
- End-of-string operations → Consistent behavior ✅

**math processor:**
- All operations → 64-bit precision throughout ✅

---

## Known Unknowns

- [ ] **Floating point storage:** How are 64-bit doubles currently stored in the runtime?
- [ ] **Serialization format:** How are integers/floats serialized in v7 database format?
- [ ] **Legacy compatibility:** Any code that depends on 32-bit integer wraparound?
- [ ] **Performance impact:** Measurable difference from 32-bit to 64-bit on target platforms?

These can be investigated during implementation.

---

## Related Decisions & Dependencies

**Depends On:** None (foundational)

**Unlocks:**
- Processor verb implementation
- Clock processor implementation
- Math processor implementation
- Any script operations using large integers or timestamps

**Cascades To:**
- Phase 3 Date/Time Representation Modernization
- Phase 3 Hash Table Modernisation (may have numeric optimization implications)
- All future arithmetic operations

---

## Alternatives Considered

### Alternative 1: Keep 32-bit integers, add explicit int64 type
- **Pros:** Minimal runtime changes
- **Cons:** Type system complexity (int vs int64 mixing, casting rules), script errors, 2040 problem remains
- **Decision:** REJECTED

### Alternative 2: Move to platform-specific integer widths
- **Pros:** "Natural" for each platform
- **Cons:** Non-portable, confusing for scripts, doesn't solve 2040 problem
- **Decision:** REJECTED

### Alternative 3: Keep float, upgrade to double where needed
- **Pros:** Minimal changes to float code
- **Cons:** Mixed precision everywhere, type confusion, same issues as int/long
- **Decision:** REJECTED (for floats)

---

## Success Criteria

- [ ] All arithmetic operations work with 64-bit integers
- [ ] `system.compiler.language.constants.infinity` is 64-bit max value
- [ ] Comprehensive test suite for integer edge cases passes
- [ ] Scripts with large numbers work correctly
- [ ] Clock verbs (now, ticks, milliseconds) return correct 64-bit values
- [ ] No database format version change required
- [ ] Documentation updated

---

## References

**Source Code Analysis:**
- Legacy types: `tedchoward/Frontier/Common/headers/lang.h` (lines 187-340)
- Union definition: `tedchoward/Frontier/Common/headers/lang.h` (lines 295-339)
- Type enum: `tedchoward/Frontier/Common/headers/lang.h` (lines 187-245)

**Related Documentation:**
- `planning/TODO_future_improvements.md` - Overall roadmap
- `planning/phase3/processor_audits/clock.md` - Clock processor audit (2040 issue)
- `planning/phase3/date_time_format_standard.md` - Timestamp handling

**Issue Tracking:**
- 2040 timestamp overflow
- Daemon wraparound on 32-bit tick/millisecond counters
- Type system complexity (int vs long, float vs double)

---

## Approval & Sign-Off

**Decision Status:** ✅ Ready for Implementation

**P0 Priority:** YES - This blocks all processor verb work

**Next Steps:**
1. ✅ Finalize this decision document
2. ⏳ Implement 64-bit arithmetic in runtime
3. ⏳ Update infinity constant generation
4. ⏳ Comprehensive testing
5. ⏳ Begin processor verb implementation

---

**Document History:**
- 2025-12-06: Created based on audit findings and user decisions
- Status: Ready for team review and approval
