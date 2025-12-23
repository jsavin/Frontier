# Explicit Context Passing Refactoring

This directory contains the architectural analysis and refactoring plan for converting table pack/unpack operations from global mode state to explicit database context passing.

**Related GitHub Issue**: [#147](https://github.com/jsavin/Frontier/issues/147)

**Related Issues**:
- [#135](https://github.com/jsavin/Frontier/issues/135): Database migration produces tables with wrong format
- [#123](https://github.com/jsavin/Frontier/issues/123): External table variable address format corruption during migration
- [#136](https://github.com/jsavin/Frontier/issues/136): Audit codebase for push/pop mode anti-pattern

## Documents

1. **[EXPLICIT_CONTEXT_PASSING_REFACTORING_PLAN.md](EXPLICIT_CONTEXT_PASSING_REFACTORING_PLAN.md)** - Main comprehensive plan
   - 4-phase implementation strategy
   - Detailed function signature changes
   - Risk assessment and mitigation
   - Testing strategy
   - Scope analysis

2. **[CALL_GRAPH_CONTEXT_THREADING.md](CALL_GRAPH_CONTEXT_THREADING.md)** - Visual architecture
   - Call graphs showing the bug scenario
   - Before/after comparisons
   - Tier-by-tier architecture breakdown
   - Testing log examples

3. **[CONTEXT_PASSING_QUICK_REFERENCE.md](CONTEXT_PASSING_QUICK_REFERENCE.md)** - Implementation guide
   - Phase 1 code changes (copy-paste ready)
   - 5 specific function modifications
   - Testing commands
   - Success criteria checklist

## Quick Summary

**Problem**: Global `db_format_mode_current()` state causes bugs when legacy reader mode is pushed during migration, causing child table packing to inherit wrong mode and write v4 headers instead of v5 headers in v7 output.

**Solution**: Explicitly pass database context through function signatures (deterministic, no push/pop anti-pattern).

**Scope**:
- Phase 1 (minimal fix): ~30 lines in 1 file, 3-5 days
- All phases: ~380 lines across 15 files, 3-4 weeks

**Recommendation**: Implement Phase 1 to fix the immediate bug independently.
