# Lang Verbs Phase 2: Memory & Utility Implementation Plan

## Phase Overview

**Goal:** Implement memory/utility verbs that provide type introspection and memory information
**Status:** Not Started
**Estimated Effort:** 2-3 hours
**Priority:** HIGH (foundational for scripting)

## Background

Phase 1 completed 5 type conversion verbs (lang.double, lang.single, lang.fixed, lang.direction, lang.string4).
Phase 2 focuses on introspection and utility functions that scripts use to inspect object types, sizes, and memory.

**Note:** The original plan in `lang_verbs_implementation_plan.md` listed different verbs for Phase 2 (lang.memavail, lang.flushmemory, lang.abs, lang.random). After analysis, we're updating Phase 2 to focus on **type introspection verbs** which are more foundational.

## Verbs to Implement (4 verbs)

### 1. `lang.typeof` - Get type name of a value

**Existing Implementation:** ✅ **ALREADY EXISTS** as built-in function `typeof()`
**Location:** `Common/source/langvalue.c:typefunc()`
**Token:** `typeoffunc` (in langtokens.h)

**Analysis:**
- `typeof()` is a built-in function, NOT a lang.* verb
- Returns OSType representing the value's type (e.g., 'long', 'stng', 'bool')
- Implementation:
  ```c
  static boolean typefunc (hdltreenode hparam1, tyvaluerecord *vreturned) {
      tyvaluerecord v;
      flnextparamislast = true;

      if (!getreadonlyparamvalue (hparam1, 1, &v))
          return (false);

      if (v.valuetype == externalvaluetype)
          v.valuetype = (tyvaluetype) (outlinevaluetype + langexternalgettype (v));

      setostypevalue (langgettypeid (v.valuetype), vreturned);
      return (true);
  }
  ```

**Action:** ❌ No implementation needed - use `typeof()` built-in

### 2. `lang.sizeOf` - Get size of a value

**Existing Implementation:** ✅ **ALREADY EXISTS** as built-in function `sizeOf()`
**Location:** `Common/source/langvalue.c:sizefunc()`
**Token:** `sizeoffunc` (in langtokens.h)

**Analysis:**
- `sizeOf()` is a built-in function, NOT a lang.* verb
- Returns size based on type:
  - Strings: character count
  - Lists/Records: item count
  - Binary: byte count (excluding 4-byte type header)
  - Scalars: byte size (sizeof)
- Implementation delegates to `langgetvalsize()` in `Common/source/langops.c`

**Implementation:**
```c
static boolean sizefunc (hdltreenode hparam1, tyvaluerecord *vreturned) {
    tyvaluerecord v;
    long size;

    flnextparamislast = true;

    if (!getreadonlyparamvalue (hparam1, 1, &v))
        return (false);

    if (!langgetvalsize (v, &size)) {
        langerror (cantsizeerror);
        return (false);
    }

    return (setlongvalue (size, vreturned));
}
```

**Action:** ❌ No implementation needed - use `sizeOf()` built-in

### 3. `lang.countObjects` - Count number of handles/objects in memory

**Status:** ❌ **NOT IN ORIGINAL FRONTIER**
**Coverage Report:** Not listed in stubbed lang verbs

**Analysis:**
- This verb does NOT exist in original Frontier codebase
- No token definition found in langtokens.h
- No stub in langverbs.c
- Memory tracking exists (memory.track.c) but no public counting API

**Recommendation:**
- **SKIP** - Not a standard Frontier verb
- If needed, could be added as future enhancement

### 4. `lang.objectSize` - Get memory size of an object

**Status:** ❌ **NOT IN ORIGINAL FRONTIER**
**Coverage Report:** Not listed in stubbed lang verbs

**Analysis:**
- This verb does NOT exist in original Frontier codebase
- No token definition found in langtokens.h
- Similar to `sizeOf()` but potentially measures heap allocation size
- `gethandlesize()` exists for getting handle sizes

**Recommendation:**
- **SKIP** - Not a standard Frontier verb
- Use `sizeOf()` for value sizes
- Use handle inspection functions for low-level memory

---

## Revised Phase 2 Scope

Based on analysis, **Phase 2 should implement actual missing utility verbs from the coverage report:**

### Revised Verbs List (4 verbs from original plan)

#### 1. `lang.memavail` - Return available memory

**Status:** Stubbed (in coverage report)
**Priority:** MEDIUM

**Implementation Approach:**
- Use portable memory query (may be no-op on modern systems)
- Return reasonable default or actual system memory
- Consider headless vs GUI mode

**Signature:**
```usertalk
lang.memavail() → long  // Returns bytes of available memory
```

#### 2. `lang.flushmemory` - Flush/compact memory

**Status:** Stubbed (in coverage report)
**Priority:** LOW

**Implementation Approach:**
- Likely NO-OP on modern systems (no manual compaction needed)
- Log call for debugging
- Return success

**Signature:**
```usertalk
lang.flushmemory() → boolean  // Returns true (may be no-op)
```

#### 3. `lang.abs` - Absolute value

**Status:** Stubbed (in coverage report)
**Priority:** HIGH

**Implementation Approach:**
- Handle all numeric types (int, long, double, fixed)
- Use existing numeric coercion infrastructure
- Simple math operation

**Signature:**
```usertalk
lang.abs(x) → number  // Returns absolute value, preserving type
```

#### 4. `lang.random` - Random number generator

**Status:** Stubbed (in coverage report)
**Priority:** HIGH

**Implementation Approach:**
- Use existing random infrastructure (if available)
- Or use standard C `rand()` / `random()`
- Consider seeding and range

**Signature:**
```usertalk
lang.random(min, max) → long  // Returns random long in [min, max]
```

---

## Implementation Strategy

### Step 1: Analyze Existing Helper Functions

**For lang.memavail:**
- Check for existing memory query functions in memory.c
- Look for portable memory APIs
- Consider platform-specific implementations (macOS, Linux)

**For lang.abs:**
- Look for existing absolute value helpers
- Check numeric type coercion patterns from Phase 1
- Handle all value types (long, int, double, fixed)

**For lang.random:**
- Search for existing random number generator
- Check if seed management exists
- Verify thread-safety for headless mode

### Step 2: Implement Wrapper Functions

Follow Phase 1 pattern:

```c
static boolean langabsfunc (hdltreenode hparam1, tyvaluerecord *vreturned) {
    tyvaluerecord v;

    flnextparamislast = true;

    if (!getreadonlyparamvalue (hparam1, 1, &v))
        return (false);

    // Handle different numeric types
    switch (v.valuetype) {
        case longvaluetype:
            if (v.data.longvalue < 0)
                v.data.longvalue = -v.data.longvalue;
            break;

        case doublevaluetype:
            // ... handle double
            break;

        // ... other types
    }

    *vreturned = v;
    return (true);
}
```

### Step 3: Wire Up in langverbfunc()

Add cases to the lang processor switch statement in `Common/source/langverbs.c`:

```c
case abstoken:
    return (langabsfunc (hparam1, vreturned));

case randomtoken:
    return (langrandomfunc (hparam1, vreturned));

// ... etc
```

### Step 4: Update Stub Configuration

Add entries to `tools/kernelverbs_parser/stub_config.py`:

```python
# Phase 2: Memory & Utility Verbs
("lang", "abs", STUB_FORWARD),
("lang", "random", STUB_FORWARD),
("lang", "memavail", STUB_FORWARD),
("lang", "flushmemory", STUB_FORWARD),
```

### Step 5: Write Integration Tests

Create test cases in `tests/integration/test_cases/lang_verbs.yaml`:

```yaml
# Phase 2: Memory & Utility Verbs

- name: "lang.abs - positive long"
  script: 'lang.abs(42)'
  expected_result: "42"

- name: "lang.abs - negative long"
  script: 'lang.abs(-42)'
  expected_result: "42"

- name: "lang.abs - double"
  script: 'lang.abs(-3.14)'
  expected_result: "3.14"

- name: "lang.random - in range"
  script: |
    local (x = lang.random(1, 100));
    return (x >= 1) and (x <= 100)
  expected_result: "true"

- name: "lang.memavail - returns positive"
  script: 'lang.memavail() > 0'
  expected_result: "true"

- name: "lang.flushmemory - succeeds"
  script: 'lang.flushmemory()'
  expected_result: "true"
```

---

## Key APIs to Investigate

### Type Introspection (for reference)
- `langgettypeid(tyvaluetype)` - Get OSType from type enum
- `langgetvaluetype(OSType)` - Get type enum from OSType
- `langgetvalsize(tyvaluerecord, long*)` - Get size of value
- `langheaptype(tyvaluetype)` - Check if type is heap-allocated

### Memory Functions
- `haveheapspace(long)` - Check available heap
- `testheapspace(long)` - Test if allocation would succeed
- `gethandlesize(Handle)` - Get size of handle

### Random Numbers
- Search for existing random infrastructure
- May need to implement wrapper around C `random()` or `rand()`

---

## Edge Cases and Special Handling

### lang.abs
- **Zero:** `abs(0)` → `0`
- **Type preservation:** `abs(long)` → long, `abs(double)` → double
- **Fixed point:** Need to handle fixed-point arithmetic correctly
- **Overflow:** `abs(LONG_MIN)` overflows (LONG_MAX + 1) - document behavior

### lang.random
- **Range validation:** Ensure min ≤ max
- **Seeding:** Consider if/how to seed (system time? fixed seed for tests?)
- **Thread safety:** Use thread-local random state if needed
- **Edge cases:** random(x, x) → x, random(0, 0) → 0

### lang.memavail
- **Headless mode:** May not have accurate memory info
- **Platform differences:** macOS vs Linux vs Windows
- **Modern systems:** With virtual memory, concept is less meaningful
- **Return value:** Consider returning reasonable default (e.g., 1GB) if unavailable

### lang.flushmemory
- **No-op acceptable:** Document that modern systems auto-compact
- **Logging:** May log call for debugging purposes
- **Legacy compatibility:** Original Frontier used manual compaction

---

## Testing Strategy

### Unit Tests
- Add C unit tests for numeric edge cases (abs overflow, etc.)
- Test random number distribution (basic sanity check)

### Integration Tests
- Test all four verbs with YAML integration tests
- Verify type preservation for abs
- Verify range constraints for random
- Verify positive returns for memavail

### Manual Testing
```bash
# Test abs
./frontier-cli/frontier-cli -e "lang.abs(-42)"  # → 42
./frontier-cli/frontier-cli -e "lang.abs(-3.14)"  # → 3.14

# Test random
./frontier-cli/frontier-cli -e "lang.random(1, 100)"  # → random in [1,100]

# Test memory
./frontier-cli/frontier-cli -e "lang.memavail()"  # → positive number
./frontier-cli/frontier-cli -e "lang.flushmemory()"  # → true
```

---

## Success Criteria

- ✅ All 4 verbs implemented with proper error handling
- ✅ Integration tests pass for all verbs
- ✅ Manual testing confirms correct behavior
- ✅ No regressions in existing tests
- ✅ Stub configuration updated
- ✅ Code review by bar-raiser agent passes

---

## Next Steps

1. **Investigate APIs:** Search codebase for random, memory query functions
2. **Implement lang.abs:** Straightforward numeric operation
3. **Implement lang.random:** Wrapper around system random
4. **Implement lang.memavail:** Query system memory or return default
5. **Implement lang.flushmemory:** No-op with logging
6. **Write tests:** YAML integration tests for all 4 verbs
7. **Manual testing:** Verify with CLI
8. **Code review:** Bar-raiser review
9. **Create PR:** Phase 2 complete

---

## Conclusion

**Key Finding:** The user's original request mentioned `lang.typeof`, `lang.sizeOf`, `lang.countObjects`, and `lang.objectSize`, but:
- `typeof()` and `sizeOf()` are **built-in functions**, not lang.* verbs
- `lang.countObjects` and `lang.objectSize` don't exist in Frontier

**Recommended Phase 2 Scope:**
Implement the 4 utility verbs from the original plan:
1. lang.abs (HIGH priority)
2. lang.random (HIGH priority)
3. lang.memavail (MEDIUM priority)
4. lang.flushmemory (LOW priority - likely no-op)

This provides better foundational utility for UserTalk scripts.
