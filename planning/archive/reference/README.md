# Reference Material Archive

This directory contains historical reference documents that provide context for design decisions, patterns, and approaches used throughout the project's evolution.

## Purpose

Documents here are:
- **Historical**: Not actively used for current planning, but valuable for understanding past decisions
- **Reference**: Provide context for why certain patterns exist
- **Superseded**: May describe approaches that were replaced, but kept for learning

## Contents

### File Locking & Collaboration (Historical Context)

- **file_locking_design.md** - Early multi-user file locking design (28K)
- **file_locking_summary.md** - Summary of file locking approaches (4.8K)

**Status**: Superseded by CRDT foundation approach
**Context**: Early exploration of collaboration patterns before CRDT strategy was adopted
**Reference Value**: Shows evolution of thinking about multi-user editing

### HTML Verb Analysis (Historical)

- **html_verb_circular_reference_analysis.md** - Analysis of HTML verb dependencies (6.8K)

**Status**: Reference for understanding HTML verb complexity
**Context**: Documents circular dependencies in HTML processing
**Reference Value**: Informs future HTML verb implementation decisions

### Labeling Strategy (Historical)

- **labeling-strategy-proposal.md** - GitHub issue/PR labeling proposal (18K)

**Status**: Historical proposal
**Context**: Process documentation from early development
**Reference Value**: Shows thought process behind project organization

## Active vs Archived

**Active reference docs remain in planning/**:
- `CRDT_FOUNDATION_ROADMAP.md` - Current multi-user collaboration strategy
- `EXTERNAL_ATOMICITY_AND_COLLABORATION_ROADMAP.md` - Active roadmap
- `DATABASE_CORRUPTION_PREVENTION.md` - Active safety guidelines
- `legacy_glossary.md` - Actively used terminology reference

**These docs move here when**:
- They're primarily historical context (not active planning)
- They describe superseded approaches (kept for learning)
- They're reference material consulted occasionally (not daily)

## Related Archives

- **planning/archive/completed-workstreams/** - Finished implementation work
- **planning/archive/completed-phases/** - Phase milestones
- **planning/archive/experimental/** - Tried approaches that didn't ship
