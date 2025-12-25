# Architectural Decision Records

This directory contains architectural decisions and design patterns that affect current and future development work on the Frontier runtime.

---

## Active Architectural Patterns

### External Object Loading Architecture
**File**: `external-object-loading-architecture.md`
**Status**: Active Reference (In-depth guide)
**Related Issues**: #136 (Push/pop anti-pattern audit), #123 (Migration)
**Knowledge Source**: v6→v7 migration refactoring (Dec 2025)

**Scope**:
- Loading strategies for different external types (menus, WPText, outlines, scripts, pictures, tables)
- Eager vs. deferred loading patterns
- Context passing vs. mode stack anti-patterns
- Single decision point principle for loading and packing
- Implementation checklist for refactoring issue #136

**Key Takeaway**: External object loading should be centralized in one decision point (langexternalpack_internal), with explicit context passing to child functions instead of scattered mode stack management.

---

### Mode Management: Single Decision Point Principle
**File**: `mode_management_single_decision_point.md`
**Status**: Core Principle
**Related Issues**: #123 (Fixed), #136 (Ongoing)

**Scope**:
- Problems with distributed mode stack management (push/pop anti-pattern)
- Single decision point architecture for mode changes
- Helper function pattern (ensure_external_in_memory)
- Implementation examples and validation criteria

**Key Takeaway**: Mode should be set in ONE place (the caller), and child functions should accept context parameters but never change mode.

---

### Context-Based Format Versioning
**File**: `ADR-002-context-based-format-versioning.md`
**Status**: Active Decision
**Related**: Database format handling during migration

**Scope**:
- Using explicit context structs instead of global state for format information
- Separate data readers and writers for v6 vs v7 formats
- Context passing through the call chain

---

### Multi-Database Context
**File**: `ADR-001-multi-database-context.md`
**Status**: Active Decision

**Scope**:
- Supporting multiple concurrent databases (system root + guest databases)
- Context structs for database selection
- Thread-safe context handling

---

## Explicit Context Passing Refactoring
**Directory**: `explicit-context-passing/`

A comprehensive refactoring effort to convert global state management to explicit context parameters throughout the codebase.

**Files**:
- `EXPLICIT_CONTEXT_PASSING_REFACTORING_PLAN.md` - Overall strategy
- `CALL_GRAPH_CONTEXT_THREADING.md` - Call graph analysis
- `CONTEXT_PASSING_QUICK_REFERENCE.md` - Quick implementation guide
- `README.md` - Directory overview

**Current Status**: Ongoing refactoring, with successful migrations in specific areas (external loading, menu handling)

---

## How to Use These Documents

### For New Feature Development
1. Check if your feature involves **external objects** → Read `external-object-loading-architecture.md`
2. Check if your feature involves **mode/format decisions** → Read `mode_management_single_decision_point.md`
3. Check if your feature involves **multiple databases** → Read `ADR-001-multi-database-context.md`

### For Refactoring Work (Issue #136)
1. **Start Here**: `external-object-loading-architecture.md` section 7 (Implementation Roadmap)
2. **Reference**: `mode_management_single_decision_point.md` for mode management patterns
3. **Details**: Section 3.3 of `external-object-loading-architecture.md` for context passing checklist

### For Bug Fixing
1. If bug involves **external loading** → Check patterns in `external-object-loading-architecture.md`
2. If bug involves **mode changes** → Check validation criteria in `mode_management_single_decision_point.md`
3. If bug affects **multiple code paths** → May indicate pattern violation (check ADRs)

---

## Related Planning Documents

- **Migration Work**: `../phase3/MIGRATION_FAILURE_ANALYSIS.md` - Detailed analysis of v6→v7 migration (Section 9 links to these ADRs)
- **Refactoring Tracking**: See `../phase3/` for ongoing work on specific components

---

## Quick Reference: Which Pattern Should I Use?

| Scenario | Pattern | Document |
|----------|---------|----------|
| Loading small external objects (outline, script) | Eager loading | external-object-loading-architecture.md § 2.2 |
| Loading large external objects (WPText, menu) | Deferred loading | external-object-loading-architecture.md § 2.3 |
| Deciding between v6/v7 format | Single decision point | mode_management_single_decision_point.md § 2 |
| Passing format/mode info through call chain | Context struct | ADR-002-context-based-format-versioning.md |
| Supporting multiple databases | Database context | ADR-001-multi-database-context.md |
| Converting code from push/pop to context | Explicit context pattern | explicit-context-passing/CONTEXT_PASSING_QUICK_REFERENCE.md |

---

## Document Maintenance

- **Last Updated**: 2025-12-24
- **Created By**: Migration refactoring session (Dec 2025)
- **Maintained By**: Development team

When adding new ADRs:
1. Name the file clearly (e.g., `ADR-NNN-short-title.md`)
2. Include Status (Active/Proposed/Deprecated)
3. Add to this README with quick description
4. Link to related issues (#XXX)
5. Cross-reference from related documents

---

## Legend

- **Active Reference**: Actively used in development, should be followed for new code
- **Core Principle**: Fundamental architectural decision affecting multiple areas
- **Active Decision**: Current best practice, may evolve over time
- **Proposed**: Under consideration, not yet implemented
- **Deprecated**: Superseded by newer pattern, kept for historical reference
