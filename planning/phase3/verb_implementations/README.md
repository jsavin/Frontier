# Verb Implementation Plans and Guides

This directory contains implementation plans, status tracking, and guides for implementing Frontier kernel verbs in headless mode.

## Contents

### Implementation Plans
- **`lang_verbs_implementation_plan.md`** - Detailed 5-phase plan for implementing 56 lang verbs
- **`kernel_verbs_implementation_plan.md`** - Overall kernel verbs implementation strategy
- **`phase4a_table_implementation.md`** - Table verbs implementation plan
- **`verb_processor_implementation_plan.md`** - Verb processor implementation approach

### Status & Tracking
- **`verb_implementation_status.md`** - Current implementation status across all processors
- **`verb_status_quick_reference.md`** - Quick reference for verb coverage
- **`phase4_implementation_backlog.md`** - Backlog of verbs to implement

### Guides
- **`startup_verbs_implementation_guide.md`** - Guide for implementing startup-related verbs
- **`target_verbs_headless_port.md`** - Guide for porting target verbs to headless

## Implementation Strategy

Verb implementations are organized in phases with test gates and PRs between each phase:

1. **Phase 1:** Type conversion verbs (highest priority)
2. **Phase 2:** Memory and utility verbs
3. **Phase 3:** Core language operations
4. **Phase 4:** Date/time verbs
5. **Phase 5:** Binary and advanced types

Each phase includes:
- Implementation of verb functions
- Integration test coverage
- Code review with bar-raiser agent
- PR creation and merge

## Related Documentation

- **Architecture:** See `planning/phase3/kernel_verb_porting/` for architectural decisions
- **Testing:** See `docs/TESTING_GUIDE.md` for test infrastructure
- **Verb Implementation:** See `docs/VERB_IMPLEMENTATION_GUIDE.md` for C implementation patterns
