# Kernel Verb Port Planning - Index

## Overview

This directory contains comprehensive planning documents for porting ~500+ kernel verbs from the legacy Frontier GUI runtime to the headless CLI runtime.

**Current Status**: Headless runtime is failing at startup due to missing `frontier.getFilePath` and `file.folderFromPath` verbs.

**Goal**: Systematically port all required kernel functionality while maintaining ZERO UI/Carbon dependencies.

## Quick Start

**If you want to understand the overall strategy** → Read **Summary** first
**If you want to start implementing** → Read **Implementation Guide** first
**If you want the full architectural details** → Read **Architecture** document
**If you want to run analysis tools** → Read **Assessment Tools** document

## Document Index

### 1. Executive Summary (START HERE)
**File**: `kernel_verb_port_summary.md` (10 KB)
**Purpose**: High-level overview of the four-pattern strategy and phased approach

**Read this if you want:**
- Quick understanding of the problem and solution
- Overview of the four porting patterns
- Timeline and effort estimates
- Key architectural decisions

**Time to read**: 10 minutes

---

### 2. Full Architecture Document
**File**: `kernel_verb_port_architecture.md` (38 KB)
**Purpose**: Complete architectural specification for the entire porting effort

**Read this if you want:**
- Detailed assessment strategy
- Dependency isolation patterns
- Complete implementation phases
- Code organization standards
- Quality assurance requirements
- Risk assessment and mitigation

**Time to read**: 45-60 minutes

**Key Sections**:
- Assessment Strategy (Phase 1-4 analysis)
- Dependency Isolation Architecture (4 patterns)
- Implementation Phases (Week 1-4 plan)
- Code Organization (directory structure)
- Quality Standards (testing, checks)

---

### 3. Assessment Tools & Scripts
**File**: `kernel_verbs_assessment_tools.md` (26 KB)
**Purpose**: Python scripts and tools for analyzing and tracking verb implementation

**Read this if you want:**
- Generate verb inventory from kernelverbs.rc
- Check for UI dependencies automatically
- Track implementation progress
- Analyze startup script dependencies

**Key Tools**:
- `tools/analyze_kernel_verbs.py` - Generate inventory
- `tools/check_headless_deps.sh` - Verify no UI deps
- `tools/track_verb_progress.py` - Progress tracking

**Time to read**: 20-30 minutes

---

### 4. Startup Verbs Implementation Guide (IMMEDIATE ACTION)
**File**: `startup_verbs_implementation_guide.md` (19 KB)
**Purpose**: Step-by-step guide to implement the two blocking verbs RIGHT NOW

**Read this if you want:**
- Immediate implementation instructions
- Complete code examples
- Testing procedures
- Integration points

**Implements**:
1. `frontier.getFilePath` - Return database path
2. `file.folderFromPath` - Parse POSIX paths

**Time to read**: 30 minutes
**Time to implement**: 2-3 hours

---

## The Four-Pattern Strategy

### Pattern 1: Pure Extraction (40% of verbs)
Copy directly from legacy → headless (no dependencies)
- String operations
- Date operations
- Math/bit operations
- Clock operations

### Pattern 2: Platform Abstraction (20% of verbs)
Create abstraction interface for platform-specific code
- File operations → `file_portable.h`
- TCP networking → `network_portable.h`
- System info → `system_portable.h`

### Pattern 3: UI Adapter (5% of verbs)
Provide alternative non-UI implementation
- Dialog operations → Print to stderr
- Clipboard → File-based or error

### Pattern 4: Stub with Error (35% of verbs)
Return clear error for UI-only operations
- Window operations
- Outline operations
- Menu operations

## Implementation Timeline

### Week 1: Startup-Critical (CURRENT WEEK)
- [ ] `frontier.getFilePath`
- [ ] `file.folderFromPath`
- [ ] String verbs (most used)
- [ ] Date verbs (most used)
- [ ] Lang type conversions

**Success**: `clock.now()` executes successfully

### Week 2: Core File I/O
- [ ] File metadata operations
- [ ] File path operations
- [ ] File copy/move/delete operations
- [ ] Volume operations

**Success**: All file.* verbs functional

### Week 3: Extended Functionality
- [ ] TCP networking (23 verbs)
- [ ] System operations
- [ ] XML operations
- [ ] HTML encoding

**Success**: All Tier 2 verbs functional

### Week 4: Polish & Testing
- [ ] Cross-platform testing
- [ ] Memory leak testing
- [ ] Performance testing
- [ ] Documentation

**Success**: Production-ready headless runtime

## Verb Inventory Summary

**Total**: ~500+ verbs across 46 processors

**By Category**:
- Portable (A): ~200 verbs (40%)
- Platform-specific (B): ~100 verbs (20%)
- UI-dependent (C): ~180 verbs (36%)
- Optional (D): ~20 verbs (4%)

**By Priority Tier**:
- Tier 0 (Startup-critical): ~20 verbs
- Tier 1 (Core functionality): ~100 verbs
- Tier 2 (Extended functionality): ~200 verbs
- Tier 3 (Optional): ~50 verbs
- Tier 4 (UI-only, stub): ~180 verbs

**Current Status**:
- Implemented: ~20 verbs (4%)
- Stubbed: ~180 verbs (36%)
- Missing: ~300 verbs (60%)

## Key Architectural Principles

### 1. Zero UI Dependencies
**CRITICAL CONSTRAINT**: No Carbon, QuickDraw, Window Manager, Menu Manager, or Dialog Manager code.

**Enforcement**:
- Automated checks: `tools/check_headless_deps.sh`
- Code review checklist
- Link-time verification (no Carbon framework)

### 2. Platform Abstraction
**For platform-specific code**: Use abstraction layer

**Example**:
```c
// Common/headers/file_portable.h
boolean portable_folderfrompath(const bigstring path, bigstring folder);

// Common/source/file_portable_posix.c
boolean portable_folderfrompath(...) { /* POSIX impl */ }

// Common/source/file_portable_win32.c (future)
boolean portable_folderfrompath(...) { /* Windows impl */ }
```

### 3. Separate Headless Code
**GUI and headless code are separate** to prevent accidental dependencies.

**Organization**:
- `Common/source/` - Shared portable logic
- `tests/headless_*_verbs.c` - Headless verb implementations
- `Common/source/*_portable*.c` - Platform abstractions

### 4. Phased Implementation
**Focus on startup-critical verbs first** to unblock development.

**Priorities**:
1. Verbs blocking startup (frontier.getFilePath, file.folderFromPath)
2. Verbs called by system.startup scripts
3. Core functionality (file I/O, string ops)
4. Extended functionality (networking, XML)
5. Optional functionality (databases, regex)

## Testing Strategy

### Unit Tests
- Test each ported verb individually
- Target: 80% coverage
- Example: `tests/test_file_portable.c`

### Integration Tests
- Real UserTalk scripts
- System.startup execution
- Example: `tests/scripts/verb_integration_tests.txt`

### Memory Testing
- Valgrind on Linux
- Address Sanitizer (ASAN) on macOS
- Target: Zero leaks

### Cross-Platform
- macOS ARM64 (primary)
- macOS x86_64 (Intel Mac)
- Linux x86_64 (Ubuntu 22.04)
- Linux ARM64 (Raspberry Pi)

## Quality Checklist

Before merging any verb implementation:

- [ ] No Carbon/UI includes
- [ ] No UI types (WindowPtr, GrafPtr, etc.)
- [ ] No UI function calls
- [ ] Platform-specific code in abstraction layer
- [ ] Unit test written and passing
- [ ] Integration test passing
- [ ] Memory leak check passing
- [ ] Cross-platform testing (if applicable)
- [ ] Documentation updated

## File Structure

```
Frontier/
├── Common/
│   ├── headers/
│   │   ├── file_portable.h         # File abstraction
│   │   ├── network_portable.h      # Network abstraction
│   │   ├── process_portable.h      # Process abstraction
│   │   └── dialog_adapter.h        # Dialog adapter
│   └── source/
│       ├── file_portable_posix.c   # POSIX file impl
│       └── network_portable_posix.c # POSIX network impl
├── tests/
│   ├── headless_frontier_verbs.c   # frontier.* verbs
│   ├── headless_file_verbs.c       # file.* verbs
│   ├── headless_string_verbs.c     # string.* verbs
│   ├── headless_clock_verbs.c      # clock.* verbs
│   ├── headless_date_verbs.c       # date.* verbs
│   └── headless_ui_stubs.c         # UI-only verb stubs
├── tools/
│   ├── analyze_kernel_verbs.py     # Generate inventory
│   ├── check_headless_deps.sh      # Check UI deps
│   └── track_verb_progress.py      # Track progress
└── planning/phase3/
    ├── README_KERNEL_VERB_PORT.md           # This file
    ├── kernel_verb_port_summary.md          # Executive summary
    ├── kernel_verb_port_architecture.md     # Full architecture
    ├── kernel_verbs_assessment_tools.md     # Analysis tools
    └── startup_verbs_implementation_guide.md # Implementation guide
```

## Next Actions

### For System Architect
1. ✓ Review architecture documents
2. ✓ Approve strategy
3. → Provide feedback on patterns and approach

### For Legacy-C-Application-Expert Agent
1. → Analyze `Common/source/langverbs.c` for UI dependencies
2. → Analyze `Common/source/shellsysverbs.c` for UI dependencies
3. → Document verb implementation patterns
4. → Build dependency graph for startup verbs

### For Implementation Team (IMMEDIATE)
1. → Implement `frontier.getFilePath` (see implementation guide)
2. → Implement `file.folderFromPath` (see implementation guide)
3. → Test system.startup execution
4. → Continue with Week 1 plan

## Related Documentation

- `planning/_CURRENT_STATUS.md` - Overall project status
- `planning/phase3/adapter_mode_isolation.md` - Database format strategy
- `planning/phase3/no_ui_linkage_policy.md` - UI dependency policy
- `planning/phase3/frontier_root_headless_plan.md` - Headless runtime plan

## Success Criteria

### Immediate Success (This Week)
- [ ] `frontier.getFilePath` returns correct path
- [ ] `file.folderFromPath` parses POSIX paths correctly
- [ ] `system.startup` executes without errors
- [ ] `clock.now()` test passes
- [ ] Zero Carbon dependencies verified

### Phase 1 Success (Week 1)
- [ ] All Tier 0 verbs implemented
- [ ] String operations working
- [ ] Date operations working
- [ ] Lang type conversions working
- [ ] Zero memory leaks

### Final Success (Week 4)
- [ ] All 500+ verbs categorized and implemented/stubbed
- [ ] Headless runtime executes arbitrary UserTalk scripts
- [ ] Zero UI/Carbon dependencies
- [ ] Tests pass on macOS and Linux
- [ ] Documentation complete

## Questions or Issues?

**For architectural questions**: Review the full architecture document
**For immediate implementation**: Follow the startup verbs guide
**For tool usage**: See the assessment tools document
**For status updates**: Check `planning/_CURRENT_STATUS.md`

---

**Created**: 2025-12-03
**Status**: Planning Complete - Ready for Implementation
**Next Review**: After Week 1 implementation

**START HERE**: Read `kernel_verb_port_summary.md` for the big picture!
