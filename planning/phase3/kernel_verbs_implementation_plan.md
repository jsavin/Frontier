# Phase 3: Kernel Verb Implementation & Validation Plan

## Overview

This plan outlines a systematic approach to pull in remaining kernel verb implementations from stub status and validate them with comprehensive unit tests. The goal is to move the 37 stub processors (407 verbs) toward real implementations, categorizing them as "working", "gui-dependent", "platform-specific", or "incompatible" in headless mode.

**Current State:**
- 51/51 processors initialized with stubs
- 14 processors with real implementations (~300 verbs)
- 37 processors with stub implementations (~407 verbs)
- All auto-generated and linked

---

## Critical Reference: UserTalk Documentation

**Source:** `docs/usertalk/docserver.userland.com/` (70+ processor categories with comprehensive verb documentation)

The DocServer documentation is **essential** for all verb implementations. It provides:
- Complete verb signatures with all parameter types
- Return value specifications and behavior
- Usage examples and best practices
- Platform-specific considerations (Windows vs. Mac)
- Character encoding standards and conversions
- RFC compliance notes where applicable
- Warning about edge cases and limitations

### Key Patterns from Documentation

**Verb Naming Conventions:**
- Accessor pairs: `processor.get()` / `processor.set()`
- Type converters: `processor.toType()` / `processor.fromType()`
- Action verbs: Simple names like `delete`, `create`, `replace`
- Boolean queries: Simple names returning true/false (e.g., `file.exists()`)

**Implementation Considerations from Docs:**

1. **Base64**: RFC 1521 compliant; `encode()` accepts any data type with optional line-wrapping (0 = no wrap)
2. **Bit**: 32-bit operations only (bits 0-31); all operations work with 32-bit numbers
3. **Clock**: Includes both duration waits (`sleepFor()`) and tick/timer operations; `ticks()` returns system ticks since startup
4. **Date**: 35+ verbs; extensive component access (day, month, year, dayOfWeek); conversions to string formats; calculations (tomorrow, nextMonth, etc.); ISO 8601 support
5. **File**: `exists()` works for both files and folders; always check existence before operations; platform-specific path handling
6. **String**: 70+ verbs covering case conversion, substring ops, text modification, encoding/decoding, validation, formatting, and parsing
7. **Character Encoding**: Verbs support multiple standards - ANSI, UTF-8, UTF-16, Mac Roman, Latin-Windows; critical for string operations
8. **GUI Processors**: Dialog, window, menu, target verbs fail without GUI context - cannot execute in headless mode

**Documentation Location Map:**
- Root index: `docs/usertalk/docserver.userland.com/index.html`
- Alphabetical reference: `docs/usertalk/docserver.userland.com/alphabeticalIndex.html`
- Individual processors: `docs/usertalk/docserver.userland.com/{processor}/index.html`
- Individual verbs: `docs/usertalk/docserver.userland.com/{processor}/{verb}.html`

---

## Phase 3 Structure: 5 Major Stages

### Stage 1: Categorization & Assessment (Foundation)

**Goal:** Create a comprehensive inventory of what each stub processor needs.

#### 1.1 Audit Remaining 37 Stub Processors
- **Create:** `planning/phase3/processor_audit.md`
- **For each processor:** Document
  - Processor name and EFP ID
  - Verb count
  - **Expected implementation status:**
    - ✅ Core functionality available
    - ⚠️ Partial (some verbs work, others need GUI/OS)
    - ❌ Requires GUI (dialog, menu, window, etc.)
    - ❌ Requires OS-specific code (file metadata, volumes, etc.)
  - **Dependencies on other verbs or systems**
  - **Complexity estimate** (low/medium/high)
  - **Known issues or blockers**
  - **Documentation reference:** Link to UserTalk docs for processor overview

**Key References:**

1. **UserTalk Verb Documentation** (ESSENTIAL):
   - `docs/usertalk/docserver.userland.com/{processor}/index.html` - Complete processor documentation with all verbs
   - Read processor overview and verb listings during audit
   - Note character encoding requirements, platform-specific concerns, and RFC compliance notes

2. **Implementation Patterns:**
   - `tests/headless_string_verbs.c` - String operations (70+ verbs, completed)
   - `tests/headless_file_verbs.c` - File I/O (~86 verbs, completed)
   - `tests/headless_table_verbs.c` - Table operations (~18 verbs, completed)
   - `tests/headless_db_verbs.c` - Database ops (~13 verbs, completed)

#### 1.2 Identify Low-Hanging Fruit (Quick Wins)
- Processors with simple, non-GUI logic
- No external dependencies
- Clear implementation from Frontier source
- Target: 5-10 processors suitable for quick implementation

**Candidates to evaluate:**
- `base64` (2 verbs) - Encoding/decoding
- `rgb` (2 verbs) - Color values
- `bit` (8 verbs) - Bitwise operations
- `semaphore` (2 verbs) - Synchronization primitives
- `clock` (7 verbs) - Time operations
- `speaker` (3 verbs) - Audio (may require OS bindings)

#### 1.3 Create Implementation Priority Matrix
- **Create:** `planning/phase3/implementation_priority.md`
- Rank processors by:
  - **Impact:** How many other systems depend on this?
  - **Effort:** Time to implement properly
  - **Feasibility:** Can it work in headless mode?
  - **Testing:** How easy is it to test?

---

### Stage 2: Unit Testing Framework Setup

**Goal:** Establish comprehensive testing patterns for verb implementations.

#### 2.1 Create Test Infrastructure
- **Create directory:** `tests/unit/` if not exists
- **Baseline:** Review existing test patterns
  - Look at `tests/unit/test_*.c` files for existing patterns
  - Document conventions for:
    - Test function naming (`test_<processor>_<verb_name>`)
    - Setup/teardown patterns
    - Error case handling
    - Edge case coverage

#### 2.2 Establish Testing Standards Document
- **Create:** `planning/phase3/verb_testing_standards.md`
- Define for each verb:
  - **Input validation tests:**
    - Valid inputs (multiple cases)
    - Invalid types/ranges
    - Boundary conditions
  - **Output validation tests:**
    - Return value correctness
    - Side effects (file creation, DB changes, etc.)
    - State consistency
  - **Error handling tests:**
    - Error messages
    - Graceful failure
    - Resource cleanup
  - **Integration tests:**
    - Interaction with other verbs
    - State dependencies

#### 2.3 Create Test Template Generator
- **Create:** `tools/test_generator/generate_verb_tests.py`
- **Purpose:** Auto-generate test file skeleton for each processor
- **Output:** `tests/unit/test_<processor>_verbs.c`
- **Includes:**
  - Test stubs for each verb
  - Common setup/teardown for processor
  - Example assertions and patterns

#### 2.4 Set Up Test Runner Integration
- **Update:** CI/CD pipeline or Makefile
- Commands:
  - `make test-verbs` - Run all verb tests
  - `make test-verbs-verbose` - With detailed output
  - `make test-coverage` - Coverage report
- **Integration:** With existing test infrastructure

---

### Stage 3: Implementation of Low-Hanging Fruit (Quick Wins)

**Goal:** Implement 5-10 simpler processors to establish patterns and build momentum.

#### 3.1 Implement Priority Group 1 (0-2 complexity)

**Target processors:** base64, rgb, bit, semaphore, clock

**For each processor:**

1. **Read UserTalk documentation** (CRITICAL FIRST STEP):
   - Open `docs/usertalk/docserver.userland.com/{processor}/index.html`
   - Read processor overview
   - Read each verb's documentation page to understand:
     - Exact parameter types and counts
     - Return value type and semantics
     - Usage examples from documentation
     - Edge cases and platform-specific notes
   - Note RFC compliance requirements (e.g., base64 = RFC 1521)
   - Understand character encoding implications if applicable

2. **Review implementation sources:**
   - Legacy Frontier at `/Users/jake/dev/tedchoward/Frontier`
   - Existing stub in `tests/headless_<processor>_verbs.c`
   - Look for patterns in similar completed processors

3. **Implement verbs:**
   - Replace stub function bodies with real logic following documentation
   - Use utility functions from existing codebase
   - Handle errors consistently
   - Match return types and behavior exactly as documented

4. **Write comprehensive tests:**
   - Use test template from Stage 2.3
   - Minimum 3 test cases per verb
   - Cover happy path + error cases from documentation
   - Test parameter boundary conditions mentioned in docs

5. **Validate:**
   - Compile without warnings
   - Tests pass 100%
   - No memory leaks (valgrind check)
   - Behavior matches documentation examples

6. **Document:**
   - Code comments for non-obvious logic
   - Update `planning/phase3/processor_audit.md` status
   - Note any deviations from documentation and why

#### 3.2 Code Review & Merge
- Self-review using code-review-bar-raiser agent
- Check for:
  - Style consistency
  - Error handling completeness
  - Test coverage
  - No new TODOs or FIXMEs

#### 3.3 Create Implementation Pattern Document
- **Create:** `planning/phase3/implementation_patterns.md`
- Document recurring patterns:
  - How to call utility functions
  - Error handling conventions
  - Memory management patterns
  - Type conversions

---

### Stage 4: Medium-Complexity Implementations

**Goal:** Implement 15-20 processors with moderate complexity.

#### 4.1 Categorize Medium-Complexity Processors

**Examples to evaluate:**
- `math` (3 verbs) - Mathematical operations
- `point`, `rectangle` (2-2 verbs) - Geometric types
- `rgb` continuation - Color operations
- `date` (30 verbs) - Date/time operations
- `keyboard`, `mouse` (4-2 verbs) - Input (may be GUI-dependent)
- `httpcontrol` (8 verbs) - HTTP client
- `tcp` (23 verbs) - Network operations

#### 4.2 Implementation Process (Batch Workflow)
- Break into batches of 3-4 processors
- Each batch follows same workflow:
  1. Implement all verbs in batch
  2. Write all tests
  3. Review as batch
  4. Merge as batch

#### 4.3 Establish Continuous Testing
- Run full test suite after each batch
- Ensure no regressions in previous implementations
- Maintain coverage percentage

---

### Stage 5: High-Complexity & Categorization

**Goal:** Handle remaining processors, categorizing those that can't be fully implemented.

#### 5.1 High-Complexity Processors (Real Implementation Needed)

**Examples:**
- `window` (31 verbs) - GUI, mostly incompatible
- `dialog`, `menu` (19, 14 verbs) - GUI-only
- `target` (3 verbs) - GUI context
- `pict` (4 verbs) - Image handling
- `htmlcontrol` (8 verbs) - GUI HTML renderer
- `statusbar` (5 verbs) - GUI element
- `mainwindow` (31 verbs) - GUI window management
- `search` (18 verbs) - May need UI for search dialogs
- `filemenu`, `editmenu` (14, 14 verbs) - GUI menus
- `inetd` (8 verbs) - Network daemon (may be feasible)
- `webserver` (10+ verbs) - HTTP server (feasible)

#### 5.2 Classification Process

For each remaining processor:

1. **Determine category:**
   - ✅ **Implementable:** Has no GUI dependencies, clear logic
   - ⚠️ **Partial:** Some verbs work, others need GUI
   - ❌ **GUI-Only:** All or most verbs require interactive UI
   - ❌ **Platform-Specific:** Needs deep OS integration
   - ❌ **External Service:** Requires third-party service

1. **Document in:** `planning/phase3/processor_classification.md`
   - Why classification decision was made
   - Which specific verbs are blockers (if partial)
   - Suggested workarounds

3. **Implement workarounds** (if applicable):
   - Stub functions that gracefully fail
   - Return meaningful error messages
   - Document in `planning/phase3/headless_limitations.md`

#### 5.3 Create Headless Mode Documentation

**Create:** `planning/phase3/headless_limitations.md`

Document for users/developers:
- Which processors/verbs work in headless mode
- Which don't and why
- Suggested alternatives or workarounds
- How to detect headless mode and adapt code

---

### Stage 6: Comprehensive Validation

**Goal:** Ensure all implementations are robust and well-tested.

#### 6.1 Test Suite Completion
- Unit tests for all 300+ implemented verbs
- Minimum coverage: 85% of new code
- Integration tests for verb interactions

#### 6.2 Regression Testing
- Test suite runs cleanly
- No memory leaks across all verbs
- Performance acceptable (document benchmarks)

#### 6.3 Documentation Completion
- **Create:** `planning/phase3/kernel_verbs_reference.md`
  - For each implemented processor
  - List of verbs with brief descriptions
  - Links to unit tests
  - Known limitations or caveats

#### 6.4 Update Main README
- Document verb implementation status
- Link to comprehensive reference
- Note about headless limitations

---

## Implementation Timeline Guidance

**Recommended approach** (no time estimates, just sequence):

1. **First:** Complete Stage 1 (categorization) - establishes the full picture
2. **Parallel:** Stage 2 (testing framework) - prepare infrastructure
3. **Then:** Stage 3 (quick wins) - build momentum with simple wins
4. **Next:** Stage 4 (medium complexity) - systematic batch processing
5. **Final:** Stage 5 & 6 (hard problems & validation) - finish strong

---

## Key Files & Locations

**UserTalk Verb Documentation** (ESSENTIAL REFERENCE):
- `docs/usertalk/docserver.userland.com/` - Complete processor documentation (70+ processors)
- Index: `docs/usertalk/docserver.userland.com/index.html`
- By processor: `docs/usertalk/docserver.userland.com/{processor}/` (e.g., `base64/`, `string/`, `date/`)
- By verb: `docs/usertalk/docserver.userland.com/{processor}/{verb}.html`
- Alphabetical index: `docs/usertalk/docserver.userland.com/alphabeticalIndex.html`

**Existing implementations** (reference patterns):
- `tests/headless_string_verbs.c` - String operations (70+ verbs)
- `tests/headless_file_verbs.c` - File I/O (~86 verbs)
- `tests/headless_table_verbs.c` - Table operations (~18 verbs)
- `tests/headless_db_verbs.c` - Database operations (~13 verbs)
- `tests/headless_xml_verbs.c` - XML operations (~14 verbs)
- `tests/headless_html_verbs.c` - HTML operations (~23 verbs)

**Verb definitions:**
- `Common/resources/Win32/kernelverbs.rc` - Master verb list (707 verbs across 51 processors)

**Test templates:**
- `tests/unit/test_*.c` - Existing test patterns

**Parser & generation:**
- `tools/kernelverbs_parser/parse_kernelverbs.py`
- `tools/kernelverbs_parser/generate_processor_stubs.py`

**Legacy reference:**
- `/Users/jake/dev/tedchoward/Frontier` - Original implementations

---

## Success Criteria

✅ **Phase 2 Complete When:**
- All 37 stub processors have been reviewed and categorized
- 20+ processors have real implementations
- All implementations have comprehensive unit tests (85%+ coverage)
- All unit tests pass
- No memory leaks detected
- Headless limitations documented for users
- README and reference docs updated
- No compiler warnings for verb-related code

---

## Appendix: Processor Categories

### Likely Implementable (20+ processors)
`base64`, `bit`, `clock`, `date`, `rgb`, `point`, `rectangle`, `math`, `string` (completed), `file` (completed), `table` (completed), `db` (completed), `xml` (completed), `html` (completed), `re`, `crypt`, `sqlite` (completed), `mysql` (completed), `tcp`, `inetd`, `webserver`, `python`, `dll`, `rez`, `launch`, `clipboard`, `thread`, `mrcalendar`, `searchengine`

### Likely GUI-Dependent (7 processors)
`dialog`, `menu`, `window`, `target`, `pict`, `filemenu`, `editmenu`, `htmlcontrol`, `mainwindow`, `statusbar`

### Partially Feasible (10 processors)
`search` (may need UI), `kb`/`mouse`/`speaker` (input-related), `osa` (Apple events), `lang` (partially done), `sys`, `launch`, `frontier`, `script`

### Under Review
`op`, `opattributes` (core operations), others pending detailed analysis

