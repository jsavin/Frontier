# Architectural Decision Records

This directory contains architectural decisions and design patterns that affect current and future development work on the Frontier runtime.

---

## Implementation Status Summary

| ADR | Status | Description |
|-----|--------|-------------|
| ADR-001 | `[IMPLEMENTED]` | Multi-Database Context Management - Core routing in place |
| ADR-002 | `[IN PROGRESS]` | Context-Based Format Versioning - Picture/WPText done, ongoing |
| ADR-003 | `[IMPLEMENTED]` | Address Value Resolution - Migration complete (PR #336) |
| ADR-004 | `[NOT STARTED]` | Dynamic Verb Binding - Strategic design for future |
| ADR-005 | `[IN PROGRESS]` | Parameter State Thread-Safety - Pattern established |
| ADR-006 | `[IMPLEMENTED]` | Outline Push/Pop Elimination - Complete (PR #261) |
| ADR-007 | `[NOT STARTED]` | REST API - Decision made, implementation deferred |
| ADR-008 | `[IMPLEMENTED]` | Processor Table Workaround - Temporary fix in place |
| ADR-009 | `[IMPLEMENTED]` | REPL Hash Table Stack - QuickScript model (PR #304) |
| ADR-010 | `[IN PROGRESS]` | Thread Testing - Phase 1 done (PR #318), Phase 2 planned |
| ADR-011 | `[IMPLEMENTED]` | Search Path Priority - Fixed (PR #342) |
| ADR-012 | `[PROPOSED]` | Main Thread Dispatch Queue - TCP-specific prototype (PR #363), generalization needed |
| ADR-013 | `[ACCEPTED]` | REPL Event Loop Architecture - non-blocking REPL so TCP callbacks run while awaiting input (2026-01-30) |
| ADR-014 | `[ACCEPTED]` | GIL-Based Cooperative Threading for headless mode - real pthreads serialized by a single lock (2026-02-10) |
| ADR-015 | `[ACCEPTED]` | Verb Registration Consolidation - single generated registration path for tests and CLI (2026-02-14) |
| ADR-016 | `[ACCEPTED]` | Headless Projection of the Frontier Menu System - commits to a model, not an implementation (2026-05-05) |
| ADR-017 | `[GO — exploration]` | Filesystem-Canonical ODB Sources - `.root` becomes a build artifact (2026-05-09) |

*Index extended 2026-08-09 to cover ADR-012 through ADR-017 (previously stopped at ADR-011).*

**Legend**:
- `[IMPLEMENTED]` - Fully implemented and in production use
- `[IN PROGRESS]` - Partially implemented, ongoing work
- `[NOT STARTED]` - Design accepted, implementation not yet begun
- `[ACCEPTED]` - Decision agreed; implementation state tracked in the ADR itself
- `[PROPOSED]` - Written up, decision not yet ratified
- `[GO — exploration]` - Direction approved, in exploration phase
- `[SUPERSEDED]` - Replaced by a newer ADR or approach

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
**Status**: `[IMPLEMENTED]` - Core context routing working
**Implementation**: Ongoing refinements to database operations

**Scope**:
- Supporting multiple concurrent databases (system root + guest databases)
- Context structs for database selection
- Thread-safe context handling

---

### Address Value Resolution
**File**: `ADR-003-address-value-resolution.md`
**Status**: `[IMPLEMENTED]` - Lazy/eager resolution complete (PR #336)

**Scope**:
- How database addresses are resolved during unpacking
- Legacy vs modern address handling
- Context propagation patterns

**Implementation Notes**:
- Fixed system.paths migration corruption (PR #336)
- Two-phase resolution working correctly
- Address values migrate cleanly from v6→v7

---

### Dynamic Verb Binding Architecture
**File**: `ADR-004-dynamic-verb-binding-architecture.md`
**Status**: `[NOT STARTED]` - Strategic design document
**Related Issues**: #166 (Verb dispatch)

**Scope**:
- Runtime verb resolution and dispatch mechanism
- Callback registration for processor tables
- External function processor (efptable) lookup

**Key Takeaway**: Design for minimal kernel + UserTalk libraries; awaits implementation in future phase.

---

### Parameter State Thread-Safety
**File**: `ADR-005-parameter-state-thread-safety.md`
**Status**: `[IN PROGRESS]` - Foundation pattern established
**Related Issues**: Issue #135 (Collaborative ODB)

**Scope**:
- Thread-local storage for parameter handling globals
- Migration pattern from global to thread-local state
- Foundation for collaborative ODB editing (Phase 6+)

**Key Takeaway**: Establishes proven pattern for migrating globals to thread-local storage using existing `tythreadglobals` infrastructure. Zero API changes via macro accessors. Used by ADR-006, ADR-009.

**Implementation Notes**:
- Pattern validated through multiple migrations
- Ongoing global elimination using this template
- Foundation for Phase 6+ collaborative ODB

---

### Outline Push/Pop Elimination
**File**: `ADR-006-outline-push-pop-elimination.md`
**Status**: `[IMPLEMENTED]` - Thread-local migration complete (PR #261)
**Related Issues**: Issue #135 (Outline context refactoring)

**Scope**:
- Replacing outline push/pop pattern with explicit context
- Reference counting for outline lifecycle
- Foundation for multi-user outline editing

**Implementation Notes**:
- 677 call sites migrated to type-safe accessors
- All tests passing (unit + integration)
- Thread-safe outline context access achieved
- Stale pointer footgun eliminated

---

### Processor Table Lifecycle Workaround
**File**: `ADR-008-processor-table-lifecycle-workaround.md`
**Status**: `[IMPLEMENTED]` - Temporary workaround active (PR #291)
**Related Issues**: Issue #135 (Collaborative ODB)

**Scope**:
- Database loading overwrites runtime-created processor tables
- Workaround using `get_headless_efptable()` to preserve original tables
- Symptom of broader global state management problem
- Proper fix requires explicit processor context (Phase 6+)

**Key Takeaway**: This is a TEMPORARY workaround, not the long-term solution. Documents the problem and links to proper fix (eliminating global `efptable`).

**Implementation Notes**:
- Unblocks processor resolution in headless mode
- Will be replaced with explicit context in Phase 6+
- Clearly marked as temporary in code

---

### REPL Hash Table Stack Management
**File**: `ADR-009-repl-hash-table-stack-management.md`
**Status**: `[IMPLEMENTED]` - QuickScript model deployed (PR #304)
**Related Issues**: PR #300 (REPL Phase 1)

**Scope**:
- REPL workspace persistence architecture
- Thread-local hash table stack migration
- QuickScript model (evaluation-scoped locals)

**Implementation Notes**:
- QuickScript model chosen over workspace persistence workarounds
- Thread-local infrastructure in place for future use
- Foundation supports Phase 6+ explicit context architecture

---

### Deterministic Thread Testing
**File**: `ADR-010-DETERMINISTIC_THREAD_TESTING.md`
**Status**: `[IN PROGRESS]` - Phase 1 foundation complete (PR #318)
**Related Issues**: PR #317 (Thread Registry)

**Scope**:
- Controlled tick injection for repeatable thread tests
- Test harness infrastructure
- Integration test patterns for thread verbs

**Implementation Notes**:
- Phase 1: Test harness + 10 integration tests (complete)
- Phase 2: Controlled timing and multi-thread tests (planned)
- Foundation for thread-safety validation before launch

---

### Search Path Priority
**File**: `ADR-011-search-path-priority-in-name-resolution.md`
**Status**: `[IMPLEMENTED]` - Search order corrected (PR #342)

**Scope**:
- Prioritize system.paths over local context
- Fix builtins table resolution
- Ensure complete implementations found

**Implementation Notes**:
- Fixed defined(webserver.init) regression
- 38 integration tests validate fix
- Performance acceptable (O(n) where n ≈ 14)

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

- **Last Updated**: 2026-01-25 (Implementation status review)
- **Created By**: Migration refactoring session (Dec 2025)
- **Maintained By**: Development team
- **Status Review**: Based on work completed 2026-01-16 to 2026-01-25 (31 commits, 13 PRs)

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
