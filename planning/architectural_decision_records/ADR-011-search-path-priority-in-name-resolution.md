# ADR-011: Prioritize system.paths Over Local Context in Name Resolution

**Status**: `[IMPLEMENTED]` - Search order corrected, 38 integration tests passing (PR #342)

## Context
Prior to PR #342, UserTalk name resolution checked local context (builtinstable, local variables) before searching system.paths. This caused `defined(webserver.init)` to return false because:

1. The EFP table (builtinstable) had a 7-item webserver stub
2. The full builtins.webserver table (21+ items) was in system.paths but checked later
3. The stub didn't contain the .init script, causing defined() to fail

This violated user expectations that system.paths entries should be the primary namespace.

## Decision
We changed the search order in `langgetdotparams()` to:

1. **Special tables** (root, target, etc.) - unchanged
2. **system.paths** - NEW: moved before local context
3. **Local context** (builtinstable, local variables) - now fallback
4. **Special symbols** - unchanged (last resort)

Implementation:
- Added `langdirecttablelookup()` callback to avoid EFP contamination
- Fixed `langsearchpathvisit()` to properly resolve address values
- Added `fllocaldotparamsonly` recursion guard

## Consequences

### Positive
- ✅ Fixes defined(webserver.init) and similar regressions
- ✅ Makes system.paths the primary namespace (matches user expectations)
- ✅ Ensures full implementations are found instead of stubs
- ✅ 38 new integration tests validate the fix

### Negative
- ⚠️ Performance impact: O(n) scan through system.paths (n ≈ 14 entries)
  - Acceptable for current usage, but monitor for growth
- ⚠️ Complexity: Search order now has 4 stages with flag interactions
  - Mitigated by comprehensive tests and code comments

### Neutral
- Backward compatibility maintained (fallback to local context preserved)
- No database format changes required

## Alternatives Considered

1. **Deep refactoring of handle management** (proper fix)
   - Would fix root cause (table instance identity)
   - Rejected: Too risky, requires architectural changes
   - Tracked for future work

2. **Keep search order, fix handle identity**
   - Would make builtinstable and system.paths resolve to same instance
   - Rejected: Same architectural complexity as #1

3. **Explicit builtins check before system.paths**
   - Simpler than reordering all searches
   - Rejected: Band-aid that doesn't fix the underlying priority issue

## Related
- PR #342: Fix langgettableval regression
- PR #337: Introduced the regression (nested table lookup fix)
- PR #336: system.paths address resolution
- Issue #339: Pascal string logging corruption (discovered during investigation)

## Notes
This ADR documents the decision to use a focused workaround instead of deep refactoring. The underlying handle management issue remains for future architectural work.

Performance baseline established in tests/integration/test_cases/builtins_priority.yaml.
