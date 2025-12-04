# Kernel Verb Port Architecture - Executive Summary

## Status
- State: Planning Complete
- Phase: Headless Runtime Completion
- Created: 2025-12-03
- Owner: System Architect

## Quick Overview

The Frontier headless runtime needs ~500+ kernel verbs ported from legacy GUI code. This document summarizes the architectural strategy.

## The Problem

**Current State:**
- Headless CLI fails during startup: `frontier.getFilePath` and `file.folderFromPath` missing
- Only ~20 verbs implemented (4%)
- ~180 verbs stubbed (36%)
- ~300 verbs missing (60%)

**Critical Constraint:**
- ZERO UI/Carbon dependencies allowed in headless code
- Must be completely portable (POSIX-compatible)

## The Solution: Four-Pattern Architecture

### Pattern 1: Pure Extraction (40% of verbs)
**For verbs with no dependencies**

Copy directly from legacy → headless:
- String operations (60 verbs)
- Date operations (30 verbs)
- Math/bit operations (11 verbs)
- Clock operations (7 verbs)
- Base64/crypto (7 verbs)

**Effort**: Low (copy + test)

### Pattern 2: Platform Abstraction (20% of verbs)
**For platform-specific but non-UI verbs**

Create abstraction interface:
- File operations (86 verbs) → `file_portable.h` + POSIX impl
- TCP networking (23 verbs) → `network_portable.h` + socket impl
- System info (16 verbs) → `system_portable.h` + platform detection
- Process launch (5 verbs) → `process_portable.h` + fork/exec

**Effort**: Medium (design interface + implement per platform)

### Pattern 3: UI Adapter (5% of verbs)
**For verbs that need UI in GUI but not headless**

Provide alternative implementation:
- `dialog.alert()` → Print to stderr
- `dialog.ask()` → Read from env var or return empty
- `clipboard.get/put()` → File-based or error

**Effort**: Low-Medium (simple alternatives)

### Pattern 4: Stub with Error (35% of verbs)
**For verbs that cannot work headless**

Return clear error message:
- All window operations (window.*)
- All outline operations (op.*)
- All word processor (wp.*)
- All menu operations (menu.*)
- Keyboard/mouse state (kb.*, mouse.*)

**Effort**: Minimal (return error)

## Implementation Phases

### Week 1: Startup-Critical (5 days)
**Goal**: Get `system.startup` working

**Deliverables:**
- ✓ `frontier.getFilePath` - Return database path
- ✓ `file.folderFromPath` - Parse POSIX paths
- ✓ String verbs (most used)
- ✓ Date verbs (most used)
- ✓ Lang type conversions

**Success**: `./frontier-cli -e "clock.now()"` works

### Week 2: Core File I/O (5 days)
**Goal**: Complete file verb implementation

**Deliverables:**
- ✓ File metadata (created, modified, size, exists)
- ✓ File path operations (fullPath, fileFromPath, folderFromPath)
- ✓ File operations (copy, move, delete, rename)
- ✓ Volume operations (freeSpace, volumeSize)

**Success**: All file.* verbs functional

### Week 3: Extended Functionality (5 days)
**Goal**: Implement networking and system verbs

**Deliverables:**
- ✓ TCP networking (23 verbs)
- ✓ System operations (sys.*, launch.*)
- ✓ XML operations (14 verbs)
- ✓ HTML encoding (urlEncode, urlDecode, etc.)

**Success**: All Tier 2 verbs functional

### Week 4: Polish & Testing (5 days)
**Goal**: Testing, documentation, stabilization

**Deliverables:**
- ✓ Cross-platform testing (macOS, Linux)
- ✓ Memory leak testing (Valgrind/ASAN)
- ✓ Performance testing
- ✓ Documentation updates

**Success**: Production-ready headless runtime

## Code Organization

```
Frontier/
├── Common/
│   ├── headers/
│   │   ├── file_portable.h         # Platform abstractions
│   │   ├── network_portable.h
│   │   ├── process_portable.h
│   │   └── dialog_adapter.h        # UI adapters
│   └── source/
│       ├── file_portable_posix.c   # POSIX implementations
│       └── network_portable_posix.c
├── tests/
│   ├── headless_frontier_verbs.c   # Verb implementations
│   ├── headless_file_verbs.c
│   ├── headless_string_verbs.c
│   ├── headless_clock_verbs.c
│   ├── headless_date_verbs.c
│   ├── headless_tcp_verbs.c
│   ├── headless_dialog_adapter.c   # UI adapters
│   └── headless_ui_stubs.c         # Error stubs
└── planning/phase3/
    ├── kernel_verb_port_architecture.md     # Full architecture (this doc's parent)
    ├── kernel_verbs_assessment_tools.md     # Analysis scripts
    └── kernel_verb_port_summary.md          # This document
```

## Quality Assurance

### Automated Checks
```bash
# Check for UI dependencies (runs in CI)
./tools/check_headless_deps.sh

# Generate verb inventory
python3 tools/analyze_kernel_verbs.py

# Track implementation progress
python3 tools/track_verb_progress.py
```

### Testing Requirements
- **Unit tests**: 80% coverage of ported verbs
- **Integration tests**: Real UserTalk scripts
- **Memory testing**: Zero leaks (Valgrind/ASAN)
- **Cross-platform**: macOS + Linux

### Code Review Checklist
For each ported verb:
- [ ] No Carbon/QuickDraw includes
- [ ] No UI types (WindowPtr, GrafPtr, etc.)
- [ ] No UI function calls (GetNextEvent, etc.)
- [ ] Platform-specific code in abstraction layer
- [ ] Unit test written
- [ ] Integration test passes

## Success Criteria

### Phase 1 Success (Week 1)
- [ ] `system.startup` executes without errors
- [ ] `frontier.getFilePath` returns correct path
- [ ] `file.folderFromPath` works on POSIX paths
- [ ] `clock.now()` returns valid timestamp
- [ ] Zero Carbon dependencies
- [ ] Tests pass on macOS

### Final Success (Week 4)
- [ ] All 500+ verbs categorized and implemented/stubbed appropriately
- [ ] Headless runtime executes arbitrary UserTalk scripts
- [ ] Zero UI/Carbon dependencies in headless code
- [ ] Tests pass on macOS and Linux
- [ ] Zero memory leaks
- [ ] Documentation complete

## Key Architectural Decisions

### Decision 1: Four-Pattern Strategy
**Rationale**: Different verb types require different porting strategies. One-size-fits-all would be inefficient.

**Trade-off**: More complexity in organization, but cleaner separation and easier maintenance.

### Decision 2: Platform Abstraction Layer
**Rationale**: Need to support multiple platforms (macOS, Linux, future Windows).

**Trade-off**: Extra abstraction layer adds code, but prevents platform-specific ifdefs throughout codebase.

### Decision 3: Separate Headless Files
**Rationale**: Keep GUI and headless code completely separate to avoid accidental UI dependencies.

**Trade-off**: Some code duplication, but maximum safety and clarity.

### Decision 4: Phased Implementation by Priority
**Rationale**: Focus on startup-critical verbs first to unblock testing and development.

**Trade-off**: Some rarely-used verbs implemented late, but gets basic functionality working ASAP.

## Risk Assessment

### High Risks
1. **Hidden UI Dependencies** (Likelihood: Medium, Impact: High)
   - *Mitigation*: Automated dependency checker, thorough code review

2. **Platform Behavior Differences** (Likelihood: High, Impact: Medium)
   - *Mitigation*: Comprehensive platform abstraction layer, extensive testing

### Medium Risks
3. **Verb Interdependencies** (Likelihood: Medium, Impact: Medium)
   - *Mitigation*: Build dependency graph, implement in order

4. **Testing Coverage Gaps** (Likelihood: High, Impact: Medium)
   - *Mitigation*: Systematic test suite, real-world script testing

### Low Risks
5. **Performance Differences** (Likelihood: Low, Impact: Low)
   - *Mitigation*: Profile after implementation, optimize as needed

## Next Steps

### For System Architect (You)
1. Review and approve this architectural plan
2. Provide feedback on four-pattern strategy
3. Identify any missing considerations
4. Approve to proceed to implementation

### For Legacy-C-Application-Expert Agent
1. Analyze legacy verb implementations in detail
2. Identify UI dependencies in `langverbs.c` and `shellsysverbs.c`
3. Document implementation patterns and gotchas
4. Provide detailed porting guide for startup-critical verbs
5. Build dependency graph for verb calls

### For Implementation Team
1. Set up directory structure
2. Create platform abstraction headers
3. Implement startup-critical verbs (Week 1)
4. Follow phased implementation plan

## Resources

### Documentation
- **Full Architecture**: `planning/phase3/kernel_verb_port_architecture.md`
- **Assessment Tools**: `planning/phase3/kernel_verbs_assessment_tools.md`
- **Current Status**: `planning/_CURRENT_STATUS.md`

### Tools
- **Inventory Generator**: `tools/analyze_kernel_verbs.py`
- **Dependency Checker**: `tools/check_headless_deps.sh`
- **Progress Tracker**: `tools/track_verb_progress.py`

### Related Planning
- `planning/phase3/adapter_mode_isolation.md` - Database adapter strategy
- `planning/phase3/no_ui_linkage_policy.md` - UI dependency policy
- `planning/phase3/frontier_root_headless_plan.md` - Headless runtime plan

## Questions for Review

1. **Is the four-pattern strategy appropriate?** Should we consolidate or add patterns?

2. **Is the phased approach realistic?** Are the time estimates reasonable?

3. **Is the code organization clear?** Should we organize differently?

4. **Are the quality standards sufficient?** Do we need additional checks?

5. **Are there any architectural concerns?** Missing abstractions, wrong boundaries?

## Conclusion

This architecture provides a systematic, phased approach to porting 500+ kernel verbs from legacy GUI code to a headless runtime while maintaining strict separation from UI dependencies. The four-pattern strategy addresses different verb categories appropriately, and the phased implementation focuses on startup-critical functionality first.

**Estimated Timeline**: 4 weeks for core functionality
**Estimated Effort**: 1-2 developers full-time
**Risk Level**: Medium (manageable with proper testing and code review)

**Recommendation**: Proceed with implementation starting with Phase 1 (Startup-Critical Verbs).

---

**Document Version**: 1.0
**Created**: 2025-12-03
**Status**: Ready for Review and Approval
