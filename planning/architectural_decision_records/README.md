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

### Address Value Resolution
**File**: `ADR-003-address-value-resolution.md`
**Status**: Active Decision

**Scope**:
- How database addresses are resolved during unpacking
- Legacy vs modern address handling
- Context propagation patterns

---

### Dynamic Verb Binding Architecture
**File**: `ADR-004-dynamic-verb-binding-architecture.md`
**Status**: Active Decision
**Related Issues**: #166 (Verb dispatch), PR #276 (Implementation)

**Scope**:
- Runtime verb resolution and dispatch mechanism
- Callback registration for processor tables
- External function processor (efptable) lookup

---

### Parameter State Thread-Safety
**File**: `ADR-005-parameter-state-thread-safety.md`
**Status**: Active Decision
**Related Issues**: Issue #135 (Collaborative ODB)

**Scope**:
- Thread-local storage for parameter handling globals
- Migration pattern from global to thread-local state
- Foundation for collaborative ODB editing (Phase 6+)

**Key Takeaway**: Establishes pattern for migrating globals to thread-local storage using existing `tythreadglobals` infrastructure. Zero API changes via macro accessors.

---

### Outline Push/Pop Elimination
**File**: `ADR-006-outline-push-pop-elimination.md`
**Status**: Active Decision
**Related Issues**: Issue #135 (Outline context refactoring)

**Scope**:
- Replacing outline push/pop pattern with explicit context
- Reference counting for outline lifecycle
- Foundation for multi-user outline editing

---

### Processor Table Lifecycle Workaround
**File**: `ADR-008-processor-table-lifecycle-workaround.md`
**Status**: Accepted (Temporary Workaround)
**Related Issues**: PR #291 (XML verbs), Issue #135 (Collaborative ODB)

**Scope**:
- Database loading overwrites runtime-created processor tables
- Workaround using `get_headless_efptable()` to preserve original tables
- Symptom of broader global state management problem
- Proper fix requires explicit processor context (Phase 6+)

**Key Takeaway**: This is a TEMPORARY workaround, not the long-term solution. Documents the problem and links to proper fix (eliminating global `efptable`).

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
| Resolving database addresses during unpacking | Address value resolution | ADR-003-address-value-resolution.md |
| Implementing verb dispatch and callbacks | Dynamic verb binding | ADR-004-dynamic-verb-binding-architecture.md |
| Migrating globals to thread-local storage | Thread-local pattern | ADR-005-parameter-state-thread-safety.md |
| Eliminating outline push/pop anti-pattern | Explicit outline context | ADR-006-outline-push-pop-elimination.md |
| Processor table not found after DB load | Headless efptable workaround | ADR-008-processor-table-lifecycle-workaround.md |

---

## Document Maintenance

- **Last Updated**: 2026-01-12
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
