# typeof() - The Sacred Function

Complete guide to typeof() behavior, history, and why it must never change.

---

## Current Behavior (DO NOT CHANGE)

`typeof()` MUST always return OSType codes (4-byte constants), NEVER string names.

**Correct behavior:**
```usertalk
typeof("hello")           => 'TEXT'    (OSType code)
typeof(filespecValue)     => 'fss '    (OSType code)
typeof(tableValue)        => 'tabl'    (OSType code)
```

**How comparisons work:**
```usertalk
/* Constants come from system.compiler.language.constants */
if typeof(x) == stringType { ... }     /* stringType = 'TEXT' */
if typeof(obj) == filespecType { ... } /* filespecType = 'fss ' */
```

---

## Why This Can Never Change

The entire UserTalk codebase relies on typeof() returning OSType codes:

1. **Constants are 4-byte values** - All type constants in `system.compiler.language.constants` are OSType codes
2. **All comparisons use OSType codes** - Every `typeof(x) == someType` comparison expects 4-byte values
3. **Production code depends on this** - Changing to strings would break ALL typeof() comparisons
4. **System tables rely on types** - Database serialization and type checking use OSType codes

**Example of production code that would break:**
```usertalk
// This code exists in production UserTalk scripts
local(t = typeof(obj))
if t == stringType {    // stringType = 'TEXT' (4-byte OSType)
    // Handle string
}
else if t == tableType {  // tableType = 'tabl' (4-byte OSType)
    // Handle table
}
```

If typeof() returned strings instead of OSType codes:
- `typeof(obj)` would return `"string"` instead of `'TEXT'`
- Comparison `"string" == 'TEXT'` would always be false
- All type checking would fail silently
- Production scripts would break everywhere

---

## Historical Incident (2026-01-02)

### What Happened

An attempt was made to "improve" typeof() to return human-readable string names instead of OSType codes:

```usertalk
// Proposed (WRONG) behavior:
typeof("hello")       => "string"    // ❌ WRONG
typeof(filespecValue) => "filespec"   // ❌ WRONG
typeof(tableValue)    => "table"      // ❌ WRONG
```

### Why This Was Proposed

- OSType codes like `'TEXT'` and `'fss '` seem cryptic
- String names like `"string"` and `"filespec"` are more readable
- Modern languages return string names from typeof()

### Why This Would Have Been Catastrophic

**Impact analysis:**
- ❌ Every typeof() comparison in production code would break
- ❌ System table serialization would fail (expects OSType codes)
- ❌ Type constant lookups would fail (system.compiler.language.constants stores OSType codes)
- ❌ Database migration would corrupt type information
- ❌ All UserTalk scripts using type checking would fail silently

**Estimated damage:**
- 1000+ UserTalk scripts in production
- Every script using typeof() would break
- No easy migration path (can't change all scripts at once)
- System tables would be corrupted

### How It Was Caught

1. Integration tests failed (typeof() expectations didn't match)
2. Code review flagged the breaking change
3. Checked production scripts - found widespread typeof() usage
4. Realized this would break everything
5. Reverted immediately

**Git Reference:**
- Incident documented in commit [5456c5eb](https://github.com/jsavin/Frontier/commit/5456c5eb6627b660c579cfdab4c2fd9e95b875fc)
- Branch: `feature/file-verbs-debug` (never merged)
- Date: 2026-01-02 (caught in review)
- Resolution: Documentation added to prevent recurrence (2026-01-05)

### The Lesson

**When typeof() tests fail, fix the TEST expectations, not typeof() behavior.**

The correct approach:
1. Check what typeof() currently returns (OSType code)
2. Verify test expectations match current behavior
3. Fix test to expect OSType code, not string name
4. Never change typeof() to return anything except OSType codes

---

## Implementation Details

For developers working on the type system:

### Source Files

- `typeof()` implementation: `Common/source/langvalue.c`, function `typefunc()`
- Type mappings: `Common/source/langops.c`, `typeinfo[]` array and `langgettypeid()`
- String→type conversion: `langgetvaluetype()` converts OSType codes to `tyvaluetype` enum

### OSType Code Format

OSType codes are 4-byte (32-bit) constants:
```c
'TEXT' = 0x54455854  // String type
'tabl' = 0x7461626C  // Table type
'fss ' = 0x66737320  // Filespec type (note trailing space)
```

### Type Constant Lookup

```usertalk
// Constants defined in system.compiler.language.constants
stringType = 'TEXT'      // 0x54455854
tableType = 'tabl'       // 0x7461626C
filespecType = 'fss '    // 0x66737320
```

These constants are used throughout UserTalk code for type comparisons.

---

## Testing typeof()

### Correct Test Pattern

```yaml
# Integration test
- name: "typeof() returns OSType code for string"
  script: 'typeof("hello")'
  expected_result: "'TEXT'"  # OSType code, not "string"
  expected_success: true
```

### Wrong Test Pattern

```yaml
# ❌ WRONG - expects string name
- name: "typeof() returns type name"
  script: 'typeof("hello")'
  expected_result: '"string"'  # WRONG - would break production
  expected_success: true
```

### When Tests Fail

If a typeof() test fails:

1. **Check current behavior** - What does typeof() actually return?
2. **Check test expectation** - Does test expect OSType code or string name?
3. **Fix the test** - Update test to expect OSType code
4. **NEVER change typeof()** - Changing typeof() breaks production

---

## Type System Reference

### Common OSType Codes

| Type | OSType Code | Hex Value | String Constant |
|------|-------------|-----------|-----------------|
| String | `'TEXT'` | 0x54455854 | `stringType` |
| Table | `'tabl'` | 0x7461626C | `tableType` |
| Number | `'long'` | 0x6C6F6E67 | `longType` |
| Boolean | `'bool'` | 0x626F6F6C | `booleanType` |
| Filespec | `'fss '` | 0x66737320 | `filespecType` |
| Binary | `'bina'` | 0x62696E61 | `binaryType` |
| List | `'list'` | 0x6C697374 | `listType` |
| Outline | `'outl'` | 0x6F75746C | `outlineType` |

### Usage in Production Code

```usertalk
// Typical production pattern
local(obj = someValue)
local(t = typeof(obj))

if t == stringType {
    // Handle string
}
else if t == tableType {
    // Handle table
}
else if t == filespecType {
    // Handle filespec
}
else {
    // Unknown type
}
```

This pattern exists in thousands of production UserTalk scripts and MUST continue to work.

---

## Summary

**DO:**
- ✅ Always return OSType codes from typeof()
- ✅ Use type constants from system.compiler.language.constants
- ✅ Fix test expectations to match OSType behavior
- ✅ Document this history so future developers understand

**DON'T:**
- ❌ NEVER change typeof() to return string names
- ❌ NEVER "improve" the type system without understanding impact
- ❌ NEVER assume modern language conventions apply to typeof()
- ❌ NEVER change typeof() behavior without checking production code

**Remember:** typeof() returning OSType codes is not a bug - it's fundamental to UserTalk's type system and production code depends on it.

---

## See Also

- **Syntax reference:** [SYNTAX.md](SYNTAX.md)
- **File operations:** [FILE_AND_DB.md](FILE_AND_DB.md)
- **Type constant definitions:** `system.compiler.language.constants` table
