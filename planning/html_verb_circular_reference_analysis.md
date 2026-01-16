# HTML Verb Circular Reference Analysis

**Date**: 2026-01-15
**Investigation**: html.refglossary, html.getonedirective, html.normalizename, html.drawcalendar

## Summary

**TL;DR**: These 4 verbs are **NOT circular references** - they are **intentional hybrid implementations** where C code calls UserTalk scripts. This is a legitimate architectural pattern for the html verb category.

## Architecture Pattern: C Helper → UserTalk Script

### The Pattern Explained

1. **C Helper Functions** (e.g., `htmlrefglossary()`) are internal utilities used by other C code during html processing
2. **C Helper Calls UserTalk** via `langrunscript()` to invoke the script implementation
3. **UserTalk Script** (e.g., `system.verbs.builtins.html.refGlossary`) does the actual work
4. **NO Kernel Case Statement** - these verbs have no `case refglossaryfunc:` because the C code never dispatches them directly

### Why This Is Not Circular

The flow is **unidirectional**:

```
C macro processing code
  ↓
  calls htmlrefglossary() C helper
    ↓
    calls langrunscript("html.refglossary")
      ↓
      executes UserTalk script at system.verbs.builtins.html.refGlossary
        ↓
        does glossary lookup (pure UserTalk, no C callback)
      ↓
    returns string result to C
  ↓
continues C processing
```

**There is NO callback from UserTalk back to C** - the UserTalk script is pure UserTalk code.

## Evidence from Code

### 1. C Helper Function Pattern

**File**: `Common/source/langhtml.c:587-737`

```c
static boolean htmlrefglossary (typrocessmacrosinfo *pmi, Handle hreference,
                                 bigstring perrorstring, Handle *hresult) {
    // ... setup ...

    // Call UserTalk script directly
    fl = langrunscript (BIGSTRING ("\x10" "html.refglossary"), &vparams, nil, &vresult)
         && strongcoercetostring (&vresult);

    // ... handle result ...
}
```

**Comment from dmb (5.0.2b14)**:
> "we used to frontTextScriptCall to call html.data.processmacroscallback, which called html.refGlossary.
> now we call html.refGlossary directly. I looked at kernelizing the whole thing here, but it would be
> a lot of work. plus, these type of lookups aren't that slow in UserTalk."

### 2. C Helper Is Called During Macro Processing

**Used in two places**:
- Line 958: Processing quoted strings in macros
- Line 1818: Processing glossary references in text

**Both are internal to C html processing** - user-facing verb dispatch never reaches these helpers.

### 3. Verb Case Statements Are Commented Out

**File**: `Common/source/langhtml.c:9843-9866`

```c
//case refglossaryfunc:
//  return (refglossaryverb (hp1, v));

//case getonedirectivefunc:
//  return (getonedirectiveverb (hp1, v));

//case normalizenamefunc:
//  return (normalizenameverb (hp1, v));
```

**Meaning**: These verbs are NEVER dispatched by the kernel switch statement. They only exist as UserTalk scripts.

### 4. Legacy Frontier Also Has This Pattern

**File**: `/Users/jake/dev/tedchoward/Frontier/Common/source/langhtml.c`

The same pattern exists in legacy source - these case statements were commented out in original Frontier.

### 5. UserTalk Implementations Exist

**Files**:
- `usertalk_scripts/Frontier.root/system/verbs/builtins/html/refGlossary.ut`
- `usertalk_scripts/Frontier.root/system/verbs/builtins/html/getOneDirective.ut`
- `usertalk_scripts/Frontier.root/system/verbs/builtins/html/normalizeName.ut`

All are pure UserTalk scripts with no kernel callbacks.

### 6. Documentation Confirms Script Implementation

**File**: `docs/usertalk/docserver/html/refGlossary.txt`

> "Notes: This verb is implemented as a script."

**File**: `docs/usertalk/docserver/html/normalizeName.txt`

> "Notes: This verb is implemented as a script."

## The Four Verbs

### 1. html.refglossary

- **Status**: Script implementation (CORRECT)
- **C Helper**: `htmlrefglossary()` calls UserTalk script
- **Used By**: Internal C macro processing (lines 958, 1818 in langhtml.c)
- **UserTalk Script**: Pure glossary lookup logic (150+ lines)
- **Annotation**: `@SCRIPT_IMPLEMENTED`

### 2. html.getonedirective

- **Status**: Script implementation (CORRECT)
- **C Helper**: None found (may be called directly from UserTalk)
- **UserTalk Script**: Parses directives from text/outlines
- **Annotation**: `@SCRIPT_IMPLEMENTED`

### 3. html.normalizename

- **Status**: Script implementation (CORRECT)
- **C Helper**: May be called via `langrunscript()` in C (need to verify)
- **UserTalk Script**: Normalizes file names based on site prefs
- **Annotation**: `@SCRIPT_IMPLEMENTED`

### 4. html.drawcalendar

- **Status**: **MISSING IMPLEMENTATION** ⚠️
- **C Helper**: None
- **Case Statement**: Never existed (not in langhtml.c at all)
- **UserTalk Script**: DOES NOT EXIST
- **Verb Registration**: Registered in kernel_verbs_headless.c:1736
- **Annotation**: Should be `@MISSING` or `@TODO`

**html.drawcalendar is a GHOST VERB** - registered but never implemented anywhere!

## Recommendation: Correct Annotations

### Option 1: Use @SCRIPT_IMPLEMENTED Annotation

Add to `stub_config.py`:

```python
# HTML verbs with intentional C→UserTalk hybrid pattern
'html.refglossary': StubConfig(
    annotation='@SCRIPT_IMPLEMENTED',
    comment='C helper htmlrefglossary() calls UserTalk script during macro processing'
),
'html.getonedirective': StubConfig(
    annotation='@SCRIPT_IMPLEMENTED',
    comment='Pure UserTalk implementation for directive parsing'
),
'html.normalizename': StubConfig(
    annotation='@SCRIPT_IMPLEMENTED',
    comment='Pure UserTalk implementation for filename normalization'
),
'html.drawcalendar': StubConfig(
    annotation='@MISSING',
    comment='Verb registered but never implemented - ghost verb from legacy Frontier'
),
```

### Option 2: Document in Planning

If we don't want to annotate these specially, document the pattern in `planning/verb_implementation_patterns.md`.

## Why This Pattern Exists

From dmb's comment (5.0.2b14), the reason for keeping these in UserTalk:

1. **Complex logic**: Glossary lookup involves hierarchical search through multiple tables
2. **Performance acceptable**: "these type of lookups aren't that slow in UserTalk"
3. **Maintenance**: Easier to modify glossary behavior in UserTalk than C
4. **Flexibility**: Website framework logic changes frequently

**This is intentional design, not technical debt.**

## Action Items

1. ✅ **DO NOT kernelize these verbs** - they are correctly implemented as scripts
2. ✅ **Add @SCRIPT_IMPLEMENTED annotations** to prevent confusion
3. ✅ **Document the C→UserTalk hybrid pattern** for future maintainers
4. ⚠️ **Investigate html.drawcalendar** - determine if it should be removed or implemented

## Related Files

- `Common/source/langhtml.c` - C helper functions
- `usertalk_scripts/Frontier.root/system/verbs/builtins/html/*.ut` - UserTalk implementations
- `docs/usertalk/docserver/html/*.txt` - Verb documentation
- `Common/source/kernel_verbs_headless.c` - Verb registration
