# GitHub Issue Labeling Strategy Proposal

**Date:** 2025-12-24
**Status:** Proposal
**Author:** Analysis of 66 open issues in Frontier repository

## Executive Summary

This proposal analyzes the current state of GitHub issue labels in the Frontier project and recommends improvements to enhance discoverability, prioritization, and project tracking. Out of 66 open issues, **20 issues (30%) have no labels at all**, and many critical domain areas lack dedicated labels. This proposal recommends adding 8 new labels and standardizing label application to improve organization.

## 1. Current Label Inventory

### 1.1 Existing Labels by Category

**Priority Labels** (4 labels)
- `priority/p0` - Critical - blocks other work, must fix immediately (6 issues)
- `priority/p1` - High priority - address soon (22 issues)
- `priority/p2` - Medium priority - nice to have (16 issues)
- **Missing:** `priority/p3` (backlog/someday-maybe items)

**Workstream Labels** (4 labels)
- `workstream/database-architecture` - Database format, migration, serialization (8 issues)
- `workstream/testing` - Test infrastructure and test cases (2 issues)
- `workstream/verb-porting` - Kernel verb implementation and bindings (2 issues)
- `workstream/carbon-migration` - Carbon dependency removal (1 issue)

**Type Labels** (3 labels)
- `enhancement` - New feature or request (10 issues)
- `documentation` - Improvements or additions to documentation (1 issue)
- `type/refactor` - Code quality and structure improvement (8 issues)

**Scope Labels** (2 labels)
- `scope/quick-win` - < 4 hours, obvious solution (1 issue)
- `scope/medium` - 1-2 weeks, requires design work (1 issue)

**Status Labels** (2 labels)
- `status/decision-needed` - Architectural decision required (1 issue)
- `status/deferred` - Intentionally postponed (1 issue)

### 1.2 Label Coverage Statistics

| Metric | Count | Percentage |
|--------|-------|------------|
| **Total open issues** | 66 | 100% |
| **Issues with NO labels** | 20 | 30% |
| **Issues with priority label** | 44 | 67% |
| **Issues with workstream label** | 13 | 20% |
| **Issues with type label** | 19 | 29% |
| **Issues with scope label** | 2 | 3% |
| **Issues with status label** | 2 | 3% |

**Key Finding:** Two-thirds of issues lack domain/workstream labels, making it difficult to filter by functional area.

## 2. Pattern Analysis

### 2.1 Well-Labeled Issue Patterns

**Example: Issue #122** (5 labels)
```
feat(P1): Implement TCP/Socket abstraction layer for networking verbs
Labels: enhancement, priority/p1, workstream/verb-porting, scope/medium, status/decision-needed
```
This issue is exemplary - it has priority, domain, type, scope, and status labels.

**Example: Issue #143** (3 labels)
```
Add error-path testing for cleanup_migration_database()
Labels: priority/p2, workstream/testing, type/refactor
```
Well-labeled with priority, workstream, and type.

### 2.2 Under-Labeled Issue Patterns

**Orphan Issues** (0 labels, 20 total):
- #159: "Consider Pascal string logging helper macro"
- #150: "Phase 2D: Migrate fprintf to logging macros in db.c and db_format.c"
- #128: "test: Complete full headless test suite validation for mode stack refactor"
- #126: "test: Fix disabled test_tableverbpack_writes_be64_when_modern"
- #118: "test: add functional tests for external object access post-migration"
- #71: "Verify headless verb registration has no ordering dependencies"
- #70: "Bitwise ops: verify bit bounds and test bit 63"
- #69: "Audit 64-bit promotion in setintvalue/setlongvalue call sites"
- #68: "Verify ensure_database_modern/db_format_mode_apply idempotence"
- #67: "Ensure langhash materialization trace restores path on all error exits"
- #66: "Remove FRONTIER_PACK_TWO workaround and restore alignment assertions"

**Analysis:** Most orphan issues fall into identifiable patterns:
- **Logging issues** (#159, #150) - missing `workstream/logging` label
- **Test issues** (#128, #126, #118, #71, #70, #79) - missing `workstream/testing` label
- **Bug fixes** (#66, #67, #68, #69, #70) - missing `type/bug` label

### 2.3 Title vs Label Inconsistencies

Several issues have priority in the title but lack priority labels:
- #159: "Consider Pascal string logging helper macro" - no priority label
- #150: "Phase 2D: Migrate fprintf to logging macros" - has phase marker but no priority
- #128: "test: Complete full headless test suite validation" - no priority

Issues with "P0/P1/P2" prefix in title should always have corresponding `priority/*` label.

### 2.4 Missing Domain Coverage

**Logging Domain** (7+ issues, no dedicated label):
- #159, #156, #150, #145, #129, #127 - all relate to logging infrastructure
- Currently scattered across `enhancement` or unlabeled

**Migration Domain** (5+ issues, no dedicated label):
- #141, #139, #138, #127, #118, #72 - all relate to v6→v7 migration
- Currently under `workstream/database-architecture` (too broad)

**Runtime/Core Domain** (10+ issues, no dedicated label):
- #106, #97, #94, #93, #91, #90, #89, #86, #84 - architectural/runtime concerns
- No way to filter these from verb implementation or testing work

## 3. Proposed New Labels

### 3.1 Domain/Workstream Labels (4 new labels)

#### `workstream/logging`
- **Description:** Logging infrastructure, macros, and output formatting
- **Color:** 0052CC (blue, matches other workstreams)
- **Applies to:** #159, #156, #150, #145, #129, #127 (6 issues)
- **Rationale:** Logging is a major infrastructure workstream in Phase 2-3. Currently these issues are scattered and hard to filter.

#### `workstream/migration`
- **Description:** Database v6→v7 migration logic and tooling
- **Color:** 0052CC (blue, matches other workstreams)
- **Applies to:** #141, #139, #138, #127, #118, #72, #143, #142 (8 issues)
- **Rationale:** Migration is a critical Phase 3 deliverable. Currently buried under broader `database-architecture` label.

#### `workstream/runtime-core`
- **Description:** Runtime architecture, memory, lifecycle, and global context
- **Color:** 0052CC (blue, matches other workstreams)
- **Applies to:** #106, #97, #94, #93, #91, #90, #89, #86, #84 (9+ issues)
- **Rationale:** Core runtime work is distinct from verb porting, database, or testing. Needs dedicated tracking.

#### `workstream/build-tooling`
- **Description:** Build system, dependencies, CMake, and vendoring
- **Color:** 0052CC (blue, matches other workstreams)
- **Applies to:** #81 (1 issue currently, more expected)
- **Rationale:** Build system work will expand as project matures. Separates tooling from runtime concerns.

### 3.2 Type Labels (2 new labels)

#### `type/bug`
- **Description:** Defect in existing functionality, needs fix
- **Color:** d73a4a (red, standard bug color)
- **Applies to:** #126, #70, #69, #68, #67, #66 (6+ issues)
- **Rationale:** Currently no way to distinguish bugs from enhancements. Critical for release planning.

#### `type/test`
- **Description:** Test infrastructure, test cases, or test-related changes
- **Color:** FFEAA7 (yellow-orange, matches type/refactor)
- **Applies to:** #143, #142, #132, #128, #126, #118, #79, #78, #72, #71, #70 (11+ issues)
- **Rationale:** Testing is a major activity but currently mixed with `workstream/testing`. This separates "test infrastructure" (workstream) from "writing tests" (type).

### 3.3 Scope Labels (1 new label)

#### `scope/large`
- **Description:** Multi-week effort, significant design and implementation
- **Color:** 84B6EB (light blue, matches other scope labels)
- **Applies to:** #122, #105, #102, #97, #94, #93, #91, #90, #89, #88, #87, #86, #85 (13+ issues)
- **Rationale:** Currently have `scope/quick-win` and `scope/medium`, but many P0/P1 architectural issues are multi-week efforts. Need to distinguish these for sprint planning.

### 3.4 Status Label (1 new label)

#### `status/blocked`
- **Description:** Cannot proceed until dependency is resolved
- **Color:** D4C5F9 (purple, matches other status labels)
- **Applies to:** None currently, but needed for project tracking
- **Rationale:** Some issues depend on others (e.g., verb porting blocked by runtime context #86). Need explicit tracking.

## 4. Label Consolidation Recommendations

### 4.1 Rename Recommendations

**Current:** `enhancement`
**Proposed:** `type/feature`
**Rationale:** Standardize on `type/*` prefix for all type labels. "Feature" is clearer than "enhancement".

**Current:** `documentation`
**Proposed:** `type/docs`
**Rationale:** Shorter, matches common GitHub conventions, fits `type/*` pattern.

### 4.2 Merge Recommendations

**Keep separate:** `workstream/database-architecture` and proposed `workstream/migration`
**Rationale:** Migration is a time-bound Phase 3 deliverable. Database architecture is ongoing. Keep separate for tracking migration completion.

**Keep separate:** `workstream/testing` and proposed `type/test`
**Rationale:** `workstream/testing` = test infrastructure and harness work. `type/test` = writing test cases. Different concerns.

### 4.3 Naming Convention Standards

**Established pattern:**
- Priority: `priority/p[0-3]` (lowercase, numeric)
- Workstream: `workstream/[domain]` (lowercase, hyphenated)
- Type: `type/[category]` (lowercase, single word or hyphenated)
- Scope: `scope/[size]` (lowercase, hyphenated)
- Status: `status/[state]` (lowercase, hyphenated)

**Recommendation:** Maintain this convention for all new labels. It's clear, filterable, and scales well.

## 5. Labeling Coverage Report

### 5.1 Current State

| Category | Labels Applied | Total Issues | Coverage |
|----------|----------------|--------------|----------|
| Priority | 44 | 66 | 67% |
| Workstream | 13 | 66 | 20% |
| Type | 19 | 66 | 29% |
| Scope | 2 | 66 | 3% |
| Status | 2 | 66 | 3% |

### 5.2 Projected State (After Implementation)

| Category | Labels Applied | Total Issues | Coverage |
|----------|----------------|--------------|----------|
| Priority | 66 | 66 | 100% |
| Workstream | 50+ | 66 | 76%+ |
| Type | 60+ | 66 | 91%+ |
| Scope | 20+ | 66 | 30%+ |
| Status | 5+ | 66 | 8%+ |

**Goal:** 100% priority coverage, 75%+ workstream coverage, 90%+ type coverage.

### 5.3 Orphan Issues Breakdown

**Critical orphans** (P0/P1 work with no labels):
- None currently - all P0/P1 issues have at least priority label

**Unlabeled but important:**
- #150: Phase 2D logging migration (should be `priority/p1`, `workstream/logging`, `type/refactor`)
- #128: Mode stack test validation (should be `priority/p1`, `workstream/testing`, `type/test`)
- #126: Disabled test fix (should be `priority/p1`, `workstream/testing`, `type/bug`)

**Low-priority unlabeled:**
- #71, #70, #69, #68, #67, #66 - all technical debt items (should be `priority/p2` or `priority/p3`)

## 6. Implementation Recommendation

### 6.1 Phase 1: Create New Labels (30 minutes)

Create labels in this order:
1. `type/bug` - needed immediately for defect tracking
2. `type/test` - high volume of test-related issues
3. `workstream/logging` - active Phase 2D work
4. `workstream/migration` - active Phase 3 work
5. `workstream/runtime-core` - distinguishes architectural work
6. `workstream/build-tooling` - small but distinct
7. `scope/large` - needed for sprint planning
8. `status/blocked` - needed for dependency tracking

### 6.2 Phase 2: Apply Labels to Orphan Issues (2 hours)

**Priority 1** - Issues with clear phase markers or P0/P1 in title:
- #150, #128, #126, #118 (4 issues)

**Priority 2** - All test-related issues:
- #79, #78, #73, #72, #71, #70 (6 issues)

**Priority 3** - Logging infrastructure issues:
- #159, #156, #127 (3 issues)

**Priority 4** - Bug fixes and technical debt:
- #69, #68, #67, #66 (4 issues)

### 6.3 Phase 3: Enhance Existing Labeled Issues (1 hour)

Add missing dimensions to well-labeled issues:
- Add `type/*` labels to issues that only have priority + workstream
- Add `scope/*` labels to P0/P1 issues for sprint planning
- Add `workstream/*` labels to type-only issues

### 6.4 Phase 4: Rename Legacy Labels (15 minutes)

**Caution:** Renaming labels breaks external references. Recommend doing this after initial labeling:
1. Rename `enhancement` → `type/feature`
2. Rename `documentation` → `type/docs`

### 6.5 Estimated Total Effort

- Create labels: 30 minutes
- Apply to orphans: 2 hours
- Enhance existing: 1 hour
- Rename legacy: 15 minutes
- **Total: ~4 hours**

Can be distributed across team members by domain expertise.

## 7. Example: Before/After

### Example 1: Issue #150 (Logging Migration)

**Before:**
```
Title: Phase 2D: Migrate fprintf to logging macros in db.c and db_format.c
Labels: (none)
```

**After:**
```
Title: Phase 2D: Migrate fprintf to logging macros in db.c and db_format.c
Labels: priority/p1, workstream/logging, type/refactor, scope/medium
```

**Impact:** Now discoverable when filtering for:
- P1 work for sprint planning
- Logging workstream issues
- Refactoring work (vs new features)
- Medium-sized tasks (1-2 weeks)

---

### Example 2: Issue #126 (Test Bug)

**Before:**
```
Title: test: Fix disabled test_tableverbpack_writes_be64_when_modern
Labels: (none)
```

**After:**
```
Title: test: Fix disabled test_tableverbpack_writes_be64_when_modern
Labels: priority/p1, workstream/testing, type/bug, scope/quick-win
```

**Impact:** Now discoverable when filtering for:
- P1 bugs blocking test coverage
- Testing infrastructure work
- Quick wins (< 4 hours) for new contributors
- Bugs vs enhancements

---

### Example 3: Issue #159 (Logging Helper)

**Before:**
```
Title: Consider Pascal string logging helper macro
Labels: (none)
```

**After:**
```
Title: Consider Pascal string logging helper macro
Labels: priority/p2, workstream/logging, type/feature, scope/quick-win
```

**Impact:** Now discoverable when filtering for:
- P2 nice-to-have improvements
- Logging infrastructure work
- Quick wins for developer experience
- Feature requests vs bugs

---

### Example 4: Issue #86 (Runtime Context - Already Well Labeled)

**Before:**
```
Title: P0: Global runtime context & lifecycle
Labels: priority/p0
```

**After:**
```
Title: P0: Global runtime context & lifecycle
Labels: priority/p0, workstream/runtime-core, type/feature, scope/large
```

**Impact:** Enhanced with domain and scope:
- Identifies this as core runtime architecture work
- Marks as large multi-week effort
- Distinguishes from verb porting or database work

## 8. Filtering Use Cases

With the proposed label scheme, teams can efficiently filter issues:

### Sprint Planning Queries

**Quick wins for new contributors:**
```
label:scope/quick-win -label:status/blocked
```

**Current sprint P0/P1 work:**
```
label:priority/p0,priority/p1 -label:status/blocked -label:status/deferred
```

**Logging workstream progress:**
```
label:workstream/logging is:open
```

### Domain-Specific Queries

**All migration work (for Phase 3 tracking):**
```
label:workstream/migration
```

**All runtime architecture decisions:**
```
label:workstream/runtime-core label:status/decision-needed
```

**Test coverage gaps:**
```
label:type/test label:workstream/testing
```

### Quality Tracking

**Open bugs by priority:**
```
label:type/bug label:priority/p0
label:type/bug label:priority/p1
```

**Technical debt (refactoring):**
```
label:type/refactor
```

## 9. Maintenance Guidelines

### 9.1 When Creating Issues

**Required labels:**
- At least 1 `priority/*` label
- At least 1 `type/*` label

**Recommended labels:**
- 1 `workstream/*` label if issue belongs to established domain
- 1 `scope/*` label for P0/P1 issues

**Optional labels:**
- `status/*` labels as needed (blocked, deferred, decision-needed)

### 9.2 Label Review Cadence

**Weekly:** Review newly created issues, ensure priority + type labels
**Monthly:** Audit for orphan issues, ensure workstream coverage
**Quarterly:** Review label usage, retire unused labels, propose new ones

### 9.3 Label Retirement Policy

A label can be retired if:
- No issues have used it in 3+ months
- The domain it represents has been completed (e.g., `workstream/migration` after Phase 3)
- It has been superseded by a better label

Before retiring, re-label affected issues and document reason in this file.

## 10. Conclusion

This proposal recommends:
- **8 new labels** across domain, type, scope, and status categories
- **2 label renames** for consistency
- **~4 hours of effort** to fully implement
- **Improved coverage** from 30% unlabeled to <5% unlabeled

**Next Steps:**
1. Review and approve this proposal
2. Create new labels in GitHub
3. Assign team members to label orphan issues by domain
4. Document labeling guidelines in CONTRIBUTING.md
5. Set up saved filter queries for common use cases

**Expected Outcomes:**
- Better sprint planning with accurate scope/priority filtering
- Improved discoverability of work by functional area
- Clearer separation of bugs, features, refactoring, and tests
- Easier onboarding for new contributors (quick-win filtering)
- Better tracking of Phase 3 migration and Phase 2D logging completion

---

**Appendix A: Complete Proposed Label Set**

| Label | Description | Color | Count (Projected) |
|-------|-------------|-------|-------------------|
| `priority/p0` | Critical - blocks other work | ff0000 | 6 |
| `priority/p1` | High priority - address soon | ff6600 | 25 |
| `priority/p2` | Medium priority - nice to have | ffbb00 | 25 |
| `priority/p3` | Backlog - someday/maybe | ffd700 | 10 |
| `workstream/database-architecture` | Database format, serialization | 0052CC | 8 |
| `workstream/testing` | Test infrastructure | 0052CC | 5 |
| `workstream/verb-porting` | Kernel verb implementation | 0052CC | 5 |
| `workstream/carbon-migration` | Carbon dependency removal | 0052CC | 1 |
| `workstream/logging` | NEW: Logging infrastructure | 0052CC | 7 |
| `workstream/migration` | NEW: v6→v7 migration | 0052CC | 8 |
| `workstream/runtime-core` | NEW: Runtime architecture | 0052CC | 15 |
| `workstream/build-tooling` | NEW: Build system | 0052CC | 2 |
| `type/feature` | New functionality (was: enhancement) | a2eeef | 20 |
| `type/bug` | NEW: Defect/fix | d73a4a | 10 |
| `type/refactor` | Code quality improvement | FFEAA7 | 15 |
| `type/docs` | Documentation (was: documentation) | 0075ca | 3 |
| `type/test` | NEW: Test cases | FFEAA7 | 12 |
| `scope/quick-win` | < 4 hours | 84B6EB | 5 |
| `scope/medium` | 1-2 weeks | 84B6EB | 8 |
| `scope/large` | NEW: Multi-week | 84B6EB | 15 |
| `status/decision-needed` | Architectural decision required | D4C5F9 | 3 |
| `status/deferred` | Intentionally postponed | D4C5F9 | 2 |
| `status/blocked` | NEW: Dependency blocking progress | D4C5F9 | 2 |

**Total: 23 labels** (8 new, 2 renamed, 13 existing)
