# Verb Resolution Planning Documents

This directory contains planning documents for the string verb resolution fix implemented in January 2026.

---

## Issue Summary

**Problem**: Calling verbs with terse (non-dotted) names failed when the verb existed as a script in path entries like `system.verbs.globals`.

**Examples**:
- `string(123)` - Failed with "unknown function"
- `defined(string)` - Returned false
- `string.mid(...)` - Worked correctly (searches for table)

---

## Document Reading Order

For understanding the complete journey of this fix, read in this order:

1. **`STRING_VERB_RESOLUTION_FAILURE.md`** - Problem statement, investigation timeline, and resolution summary
2. **`INVESTIGATION_REPORT.md`** - Initial investigation findings and the incorrect fix approach
3. **`EXECUTION_PLAN.md`** - Detailed (but incorrect) implementation plan - **DEPRECATED**
4. **`/Users/jake/dev/jsavin/Frontier-fix-string-verb-resolution/LEGACY_INVESTIGATION_REPORT.md`** - Correct approach based on legacy code analysis

---

## Key Takeaways

### The Journey

1. **Initial Investigation**: Identified that the callback only returned tables, not scripts
2. **First Attempt**: Tried adding `is_final_component` parameter to make callback return parent tables
3. **Discovery**: Legacy investigation revealed this approach was architecturally wrong
4. **Correct Fix**: Restore legacy semantics - callback returns tables only, identifier lookup happens elsewhere

### Architectural Insight

The critical misunderstanding was about **separation of concerns**:

- **Path search** (callback's job): Find which table contains a name
- **Identifier lookup** (caller's job): Find the actual value in that table

The callback should NEVER try to return parent tables for non-table items. The identifier lookup happens in `langgethandlercode` using the `intable` parameter.

### Lesson Learned

**Always investigate legacy implementation first** before attempting fixes based on assumptions. The legacy code worked for 25+ years and represents the proven design.

---

## Status

**Issue**: Resolved
**Fix**: Implemented based on legacy investigation findings
**Branch**: `feature/fix-string-verb-resolution`
**PR**: (to be created after testing)

---

## Files

| File | Purpose | Status |
|------|---------|--------|
| `STRING_VERB_RESOLUTION_FAILURE.md` | Problem statement and resolution summary | Current |
| `INVESTIGATION_REPORT.md` | Initial investigation findings | Historical (includes update about incorrect approach) |
| `EXECUTION_PLAN.md` | Initial fix attempt (4-parameter approach) | **DEPRECATED** |
| `/LEGACY_INVESTIGATION_REPORT.md` | Correct implementation based on legacy code | **SOURCE OF TRUTH** |

---

## Related Files

- `/Users/jake/dev/jsavin/Frontier-fix-string-verb-resolution/Common/source/langvalue.c` - Implementation file
- `/Users/jake/dev/tedchoward/Frontier/Common/source/langvalue.c` - Legacy reference implementation
- `/Users/jake/dev/jsavin/Frontier-fix-string-verb-resolution/tests/integration/test_cases/string_verb_resolution_fix.yaml` - Integration tests

---

**Directory Created**: 2026-01-25
**Author**: Claude (via system-architect agent)
