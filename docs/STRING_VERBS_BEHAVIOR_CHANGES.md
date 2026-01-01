# String Verbs Behavior Changes - Phase 3

**Status**: Implemented in PR #221
**Date**: 2025-12-29
**Impact**: Breaking change for scripts relying on permissive bounds handling

---

## Overview

String verb implementations in headless mode now include **strict bounds checking** that was previously absent in the original implementations. This is an **intentional improvement** that makes error conditions explicit rather than silently returning empty results.

---

## Affected Verbs

### string.mid()

**Location**: `Common/source/stringverbs.c:1592-1597`

**Behavior Change**:
- **Before**: Out-of-bounds start position silently returned empty string
- **After**: Returns error "start position out of bounds"

**Example**:
```usertalk
// String: "hello" (length 5)
string.mid("hello", 10, 5)

// OLD behavior: Returns "" (empty string)
// NEW behavior: Error - "start position out of bounds"
```

**Rationale**: Bounds errors indicate programming mistakes that should be caught, not silently ignored. Empty string could be a valid result, making errors indistinguishable from intentional behavior.

---

### string.nthCharacter()

**Location**: `Common/source/stringverbs.c:1629-1632`

**Behavior Change**:
- **Before**: Out-of-bounds position silently returned empty string or character 0
- **After**: Returns error "position out of bounds"

**Example**:
```usertalk
// String: "test" (length 4)
string.nthCharacter("test", 10)

// OLD behavior: Returns "" or '\0'
// NEW behavior: Error - "position out of bounds"
```

**Rationale**: Accessing beyond string bounds is always a logic error.

---

## Migration Guide

### For Scripts That Expect Forgiving Behavior

If your scripts rely on the old forgiving behavior (out-of-bounds returns empty), you'll need to add explicit bounds checking:

**Old Pattern** (relied on permissive behavior):
```usertalk
local(sub = string.mid(s, pos, len));
if sub == "" {
    // Could mean out-of-bounds OR intentionally empty
}
```

**New Pattern** (explicit bounds check):
```usertalk
if (pos >= 1) and (pos <= sizeof(s)) {
    local(sub = string.mid(s, pos, len));
    // sub is valid
} else {
    // Handle out-of-bounds explicitly
}
```

### For New Scripts

The new behavior makes errors explicit, which is **preferred** for new code:
```usertalk
try {
    local(sub = string.mid(s, pos, len));
    // Use sub
}
else {
    // Handle bounds error
}
```

---

## Implementation Details

### string.mid() Bounds Checking

**Code** (`Common/source/stringverbs.c:1592-1597`):
```c
if (start < 1 || start > stringlength(bs)) {
    langerrormessage(BIGSTRING("\x1cstart position out of bounds"));
    return false;
}
```

### string.nthCharacter() Bounds Checking

**Code** (`Common/source/stringverbs.c:1629-1632`):
```c
if (position < 1 || position > stringlength(bs)) {
    langerrormessage(BIGSTRING("\x16position out of bounds"));
    return false;
}
```

---

## Compatibility Considerations

### Risk Assessment

**Low-Moderate Risk**:
- Scripts that **intentionally** rely on out-of-bounds returning empty will break
- Scripts with **latent bugs** (accessing out-of-bounds unintentionally) will now surface errors
- Well-written scripts with proper bounds checking are **unaffected**

### Testing Against Existing Databases

**Recommended Actions**:
1. Test critical scripts in v6/v7 databases with new string verb implementations
2. Look for error logs mentioning "out of bounds" in string operations
3. Review and fix scripts that relied on permissive behavior
4. Consider this a **code quality improvement** - exposed bugs are better than silent failures

---

## Design Decision: Why Strict Bounds Checking?

### Arguments For Strict Checking (Chosen Approach)

1. **Errors Should Be Explicit**: Programming mistakes should fail loudly, not silently
2. **Ambiguity Reduction**: Empty string is a valid result - shouldn't also mean "error"
3. **Consistency**: Most modern languages error on out-of-bounds access
4. **Debugging**: Easier to find bugs when they produce errors vs silent failures
5. **Future-Proof**: Better foundation for collaborative ODB (concurrent access needs clear error boundaries)

### Arguments Against (Why Not Permissive)

1. **Breaking Changes**: Could break existing scripts (but exposes latent bugs)
2. **Compatibility**: Not 100% compatible with original behavior
3. **Forgiving UX**: Some users prefer permissive "do what I mean" behavior

**Decision**: **Strict checking wins** for long-term maintainability and security.

---

## Alternative Considered: Compatibility Flag

We **considered but rejected** adding a compatibility mode:

```c
// REJECTED APPROACH:
if (config.strict_string_bounds) {
    return error;
} else {
    return empty_string;  // Legacy behavior
}
```

**Why Rejected**:
- Adds complexity without proportional benefit
- Creates two code paths to maintain and test
- Delays inevitable migration
- Technical debt accumulation

**Better Approach**: Document breaking change, provide migration guide, move forward with strict checking.

---

## Future Work

### Remaining String Verbs To Audit

Other string verbs may benefit from similar bounds checking:
- `string.delete()` - check start/count bounds
- `string.insert()` - check position bounds
- `string.replace()` - check start/count bounds

See `planning/phase3/STRING_VERBS_AUDIT.md` for tracking.

---

## References

- **PR #221**: String verb dispatcher implementation (includes bounds checking)
- **Bot Review Comment**: PR #224 (table verbs) - raised question about string bounds checking
- **Original Implementation**: `Common/source/stringverbs.c` (pre-Phase 3)
- **Testing**: `tests/integration/test_cases/string_verbs.yaml`

---

**Last Updated**: 2026-01-01
**Maintainer**: String verb workstream
**Status**: Finalized - behavior change is intentional and permanent
