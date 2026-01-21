# Frontier Development Guide

## Technical Decision-Making Principles

**Default to proper, maintainable, long-term solutions.** Quick fixes accumulate as technical debt that becomes costly to unwind. Unless the user explicitly requests a quick fix for time constraints, recommend the approach that solves the problem correctly rather than suppressing symptoms.

**When in doubt:** Apply the proper fix, not temporary workarounds.

---

## Quick Reference

### Essential Commands

```bash
# Run full test suite (unit tests)
./tools/run_headless_tests.sh

# Run integration tests (Python/YAML-based verb tests)
cd tests && make test-integration

# Run all tests (unit + integration)
cd tests && make test-all

# Database migration (v6 → v7)
rm -f databases/Frontier.root7
./frontier-cli/frontier-cli --system-root databases/Frontier.root -e "1"

# Verb coverage analysis
cd tools/kernelverbs_parser && python3 cli.py report

# Export integration tests to OPML
python3 tools/export_tests_to_opml.py
# Output: reports/integration_tests.opml

# Install git hooks
./tools/install_git_hooks.sh

# Create PR (after pushing branch)
# 1. Use pull-request agent to create PR, OR create manually with:
gh pr create --title "..." --body "..." --base develop
# 2. Start background monitor (auto-backgrounds itself - always non-blocking):
./tools/monitor_pr_review.sh <PR_NUMBER>
# 3. Watch with: tail -f tests/tmp/pr_monitor_<PR_NUMBER>.log
# (monitor_pr_review.sh ALWAYS runs in background, regardless of invocation method)
```

**Test Output Directory Structure**:
- `tests/tmp/unit/` - C unit test outputs
- `tests/tmp/integration/` - Integration test outputs
- `tests/tmp/migration/` - Migration test artifacts
- `tests/tmp/results/` - Test logs and CLI runtime artifacts

Use `$(./tools/get_test_temp_path.sh)` for manual testing paths.

### Documentation Quick Links

- **[Verb Implementation Guide](docs/VERB_IMPLEMENTATION_GUIDE.md)** - Implementing kernel verbs in C
- **[Testing Guide](docs/TESTING_GUIDE.md)** - CLI usage, testing patterns, database migration
- **[CLI Usage Guide](docs/CLI_USAGE_GUIDE.md)** - Complete frontier-cli reference (600+ lines)
- **[Logging Standards](docs/LOGGING_STANDARDS.md)** - Structured logging requirements
- **[UserTalk Documentation](docs/usertalk/docserver/)** - DocServer verb reference (75+ categories, source markup from docserver.userland.com)

---

## ⚠️ MANDATORY: Pre-Work Location Verification

**STOP AND VERIFY** before starting any work:

```bash
pwd && git branch --show-current
```

**Quick Decision**:
- ✅ **Trivial work** (typo, single-line fix, quick doc update) → OK on develop
- ❌ **Non-trivial work** (feature, bug fix, multi-file change) → MUST create worktree

**If non-trivial AND on develop**:
```bash
cd /Users/jake/dev/jsavin/Frontier
git worktree add ../Frontier-<feature-name> -b feature/<feature-name>
cd ../Frontier-<feature-name>
```

**If committing**: Verify branch is `feature/*` in worktree, never `git push origin develop` directly.

**Full Guide**: See [`docs/WORKTREE_WORKFLOW.md`](docs/WORKTREE_WORKFLOW.md) and [`docs/PR_MONITOR_BLOCKING_ISSUE.md`](docs/PR_MONITOR_BLOCKING_ISSUE.md)

---

## Communication Standards

### Privacy & Entity References

**Entity and Person Mention Policy**:
- **NEVER mention specific people or entities** (partnerships, companies, individuals, etc.) unless the user explicitly asks
- This includes in commit messages, PR descriptions, code comments, and documentation
- Keep communications focused on technical details, not partnerships or strategic relationships
- Exception: When user directly asks about strategic context or partnerships

**Rationale**: Strategic relationships and partnerships are user-managed information. Technical work should focus on implementation details, not business context.

---

## Permanent Branches (Never Merge to Develop)

Several archive branches exist independently and should **never be merged** to develop:
- `archive/codex-sessions` - AI session transcripts (~950k lines)
- `archive/portable-refactoring` - Refactoring experiments (348 files, 98k lines)
- `archive/carbon-migration` - Legacy documentation (85 files)

**Full details:** See [`docs/PERMANENT_BRANCHES.md`](docs/PERMANENT_BRANCHES.md) for descriptions, purposes, and git notes.

---

## Strategic Roadmap

**Planning Documentation** (source of truth for strategic direction):
- `planning/INDEX.md` - Navigation for active and archived workstreams
- `planning/phase_overview.md` - Overview of all phases
- `planning/CRDT_FOUNDATION_ROADMAP.md` - Collaborative ODB foundation roadmap

**Key Project Context**:
- Frontier has "guest databases" - any databases opened that aren't system root. Top-level items in guest databases are in global scope (managed via `system.compiler.files`)
- Current "target" is generally a window (database or editor window for non-scalars like scripts, outlines, WPText/RTF)
- Legacy Frontier source: `/Users/jake/dev/tedchoward/Frontier`
- When fixing critical areas (serialization, database format, byte alignment): 1) search `planning/`, 2) confirm alignment with user, 3) proceed
- v7 database format should NOT contain font/style info (except within stored RTF objects)
- **Milestone Commits MUST Use PR Workflow** ⚠️:
  1. Create feature branch: `git checkout -b feature/description`
  2. Commit work (multiple commits OK)
  3. **BEFORE FIRST PUSH: Run full test suite** ⚠️:
     - `./tools/run_headless_tests.sh` (unit tests)
     - `cd tests && make test-integration` (integration tests)
     - Fix any failures before pushing
  4. Push to origin: `git push origin feature/<branch-name>`
     - **CRITICAL**: Push the FEATURE BRANCH, NEVER push to origin/develop directly
     - **NEVER run `git push origin develop`** without explicit user instruction
     - All changes to develop MUST go through PR review process
  5. Use pull-request agent to create PR
  6. **CRITICAL: Run PR monitor in BACKGROUND** ⚠️⚠️⚠️:
     - **NEVER use `./tools/monitor_pr_review.sh` directly** (blocks session for 15 minutes)
     - **ALWAYS use**: `./tools/monitor_pr_review_bg.sh <PR_NUMBER>`
     - This returns immediately and runs monitoring in background
     - Monitor automatically detects merge conflicts and exits with error (rebase required)
     - Watch log with: `tail -f tests/tmp/pr_monitor_<PR_NUMBER>.log`
     - See `docs/PR_MONITOR_BLOCKING_ISSUE.md` for full details
  7. **ALWAYS discuss bot feedback with user before addressing** - Never make changes autonomously
  8. **NEVER merge PRs without explicit user approval** - User must review and approve merge
- Always create branch for new development work when on develop
- Never delete branches without user confirmation
- Never work on develop directly for larger changes


## Working with Agents

### Available Agents

| Agent | Use When | Capabilities |
|-------|----------|--------------|
| **Explore** | Multi-file codebase exploration, understanding architecture | Fast search, pattern matching, architectural context |
| **Plan** | Designing implementation plans, architectural decisions | Step-by-step planning, file identification, trade-off analysis |
| **system-architect** | C domain work, runtime architecture, memory management | Technical design, implementation alternatives, complex decisions |
| **usertalk-engineer** | UserTalk scripting, verb implementations in UserTalk domain | UserTalk expertise, scripting logic, runtime behavior |
| **odb-database-expert** | Database format, corruption issues, migration | Database internals, format expertise, debugging |
| **logging-expert** | Logging infrastructure, standards compliance | Logging patterns, structured logging |
| **code-review-bar-raiser** | Pre-merge quality review of significant implementations | Rigorous review, security, performance, maintainability |
| **refactoring-consultant** | Planning and executing refactoring work | Code structure, modernization, cleanup |
| **pull-request** | Creating PR summaries, pushing to origin | PR description generation, commit analysis |
| **frontier-sdet** | Test infrastructure, test strategy | Testing expertise, test framework design |
| **claude-code-guide** | Questions about Claude Code, SDK, or API | Documentation lookup, feature explanations |

### When to Use Agents

- **Exploration**: Multi-file searches, architectural understanding → `Explore` agent
- **Planning**: Implementation design before coding → `Plan` or `system-architect`
- **Code Review**: Pre-merge quality gates → `code-review-bar-raiser`
- **Complex Analysis**: Trade-off studies, architectural decisions → `system-architect`

Don't do complex analysis or design work manually when an agent can do it better and faster.

### Error Type Delegation Patterns

**When encountering specific error types, delegate to specialized agents rather than attempting fixes yourself:**

- **UserTalk syntax errors** → `usertalk-engineer` agent
  - Missing parentheses, incorrect escape sequences, wrong verb names
  - Example: `\r\n` vs `char(13) + char(10)`, `sizeOf()` vs `string.length()`

- **Integration test framework issues** → `frontier-sdet` agent
  - Test harness reporting incorrect exit codes
  - Test infrastructure bugs (not test content issues)
  - Example: Manual CLI works but test framework reports failure

- **Test content verification** → `frontier-sdet` or `usertalk-engineer` agent
  - Verify tests match expected behavior before blaming implementation
  - Example: Test expects wrong result, implementation is actually correct

- **Architecture/design decisions** → `system-architect` agent
  - Thread-safety patterns, memory management strategies
  - Complex refactoring requiring architectural insight

**Rationale**: Specialized agents have domain expertise and can fix issues more reliably than attempting manual fixes. This conserves context in the main conversation and leads to better outcomes.

### Delegating to Pull-Request Agent - Test Efficiency ⚠️

**IMPORTANT**: Avoid redundant test runs to conserve tokens.

When delegating to the pull-request agent:
- ✅ **If tests were already run**: Mention test results in the delegation prompt
  - Example: "Unit tests passed (./tools/run_headless_tests.sh), integration tests passed (cd tests && make test-integration)"
- ✅ **If tests haven't been run**: Let the agent know so it can verify test status
  - Example: "Tests haven't been run yet - agent should verify before creating PR"
- ✅ The pull-request agent will check conversation history for test results before running tests
- ❌ **Don't** re-run tests just before delegating if they were already run earlier in the session

### Agent Verification Requirements ⚠️

**CRITICAL**: Agents must verify fixes work end-to-end, not just fix one piece:

**Example - Verb Dispatch (Issue #166)**:
- ❌ **Wrong**: Fix callback mechanism, assume verb works
- ✅ **Right**: Fix callback, verify `lang.new(tableType, @t)` actually works
- **Lesson**: Test the complete chain: name resolution → dispatch → implementation

**Example - Database Context Review**:
- ✅ Read full planning context BEFORE reviewing
- ✅ Use git bisect to verify crash pre-existence
- ✅ Trace full call chains to verify global state reliability
- ✅ Don't stop at surface-level fixes

### Pull-Request Agent - Auto-Backgrounding Monitor ✅

**SOLUTION IMPLEMENTED**: `monitor_pr_review.sh` auto-backgrounds itself - it is ALWAYS non-blocking.

**How It Works**:
- If called from foreground → automatically re-execs itself in background and returns immediately
- If already backgrounded → continues with monitoring logic
- **Result**: No matter how it's invoked (by agent, manually, or script), always non-blocking

**Standard Workflow**:

1. Create PR:
   ```bash
   gh pr create --title "..." --body "..." --base develop
   ```

2. Start background monitor (returns immediately):
   ```bash
   ./tools/monitor_pr_review.sh <PR_NUMBER>
   ```

3. Watch asynchronously (Ctrl-C to stop tail):
   ```bash
   tail -f tests/tmp/pr_monitor_<PR_NUMBER>.log
   ```

**Key Property**: The auto-background behavior makes it **impossible** to accidentally block, even if:
- Agent calls the script
- Script is called without explicit `&` or `nohup`
- Called from any context

**Deprecation Note**: `monitor_pr_review_bg.sh` has been removed - use `monitor_pr_review.sh` directly.

---

## Implementing Kernel Verbs

**Full Guide:** See [`docs/VERB_IMPLEMENTATION_GUIDE.md`](docs/VERB_IMPLEMENTATION_GUIDE.md)

### Quick Pattern

```c
// Add case in appropriate verb function
case yourverb:
    getstringvalue(hparam1, 1, varname);  // Extract params
    flnextparamislast = true;              // Mark last param

    // Create value record (NEVER pass handles directly!)
    tyvaluerecord val;
    setheapvalue(hstring, stringvaluetype, &val);
    hashtableassign(htable, varname, val);  // Assign to ODB

    return setbooleanvalue(true, vreturned);
```

### Critical Gotchas ⚠️

- **DON'T** pass handles directly to `hashtableassign()` - wrap in `tyvaluerecord` first
- **DO** use `setXXXvalue()` functions (setheapvalue, setlongvalue, etc.)
- **Mark last param**: Set `flnextparamislast = true` before final parameter
- **Test**: Run `./tools/run_headless_tests.sh` after implementation

---

## Verb Testing Requirements ⚠️ MANDATORY

**Policy**: PRs or commits that implement new verbs or modify existing verb functionality **MUST include integration tests** that validate the new or changed behavior.

**When This Applies**:
- ✅ Adding new kernel verbs
- ✅ Modifying verb behavior in C code
- ✅ Fixing verb bugs
- ✅ Updating verb implementations

**What Must Be Included**:
1. **Integration tests in YAML format** (in `tests/integration/test_cases/`)
   - Test the verb from UserTalk (end-to-end)
   - Cover happy path, edge cases, and error conditions
   - Tests must PASS before code is merged to develop
2. **Unit tests in C** (if appropriate for the complexity)
   - Test C helper functions directly
   - Test error handling and boundary conditions

**Why This Matters**:
- Verbs are the public API - they must work reliably
- Integration tests prevent regression of verb functionality
- Tests serve as usage documentation and examples
- We cannot ship verb code without proof that it works

**Cannot Merge Without Passing Tests**:
- Unit tests MUST pass: `./tools/run_headless_tests.sh`
- Integration tests MUST pass: `cd tests && make test-integration`
- All test output must be reviewed as part of PR feedback
- If tests fail, the PR is incomplete - fix tests and verb together

**Reference**: See `docs/TESTING_GUIDE.md` and `tests/integration/test_cases/` for examples of well-written verb integration tests.

---

## Testing & CLI Usage

**Full Guide:** See [`docs/TESTING_GUIDE.md`](docs/TESTING_GUIDE.md)

### Essential Commands

```bash
# Run full test suite
./tools/run_headless_tests.sh

# Test CLI inline (startup scripts skipped by default)
./frontier-cli/frontier-cli -e "1+1"

# Multi-line UserTalk
./frontier-cli/frontier-cli -e $'lang.new(tableType, @t);\nt.a=1;\nreturn t.a'

# With database loaded
./frontier-cli/frontier-cli --system-root databases/Frontier.root7 -e "sizeOf(system)"

# Run system.startup scripts (opt-in, rarely needed)
FRONTIER_HEADLESS_RUN_STARTUP=1 ./frontier-cli/frontier-cli -e "1+1"
```

**Note on Startup Scripts**: By default, frontier-cli skips `system.startup` scripts for faster execution and cleaner testing. This is the correct behavior for development and testing. Only set `FRONTIER_HEADLESS_RUN_STARTUP=1` if you specifically need startup scripts to run.

## UserTalk Critical Facts ⚠️

### Syntax: String Quotes

**CRITICAL**: Double quotes for strings, single quotes for character constants!

```usertalk
sizeOf("hello")  // ✅ CORRECT - double quotes for strings
sizeOf('hello')  // ❌ WRONG - syntax error (single quotes = char constant)
```

This is **opposite** of JavaScript/Python where `'x'` and `"x"` are equivalent!

### typeof() - ABSOLUTELY NOT TO BE CHANGED ⚠️⚠️⚠️

**CRITICAL**: `typeof()` MUST always return OSType codes (4-byte constants), NEVER string names.

**CORRECT BEHAVIOR**:
```usertalk
typeof("hello")           => 'TEXT'    (OSType code)
typeof(filespecValue)     => 'fss '    (OSType code)
typeof(tableValue)        => 'tabl'    (OSType code)

/* Comparisons use system.compiler.language.constants */
if typeof(x) == stringType { ... }    /* where stringType = 'TEXT' */
if typeof(obj) == filespecType { ... } /* where filespecType = 'fss ' */
```

**WRONG - BREAKS PRODUCTION**:
```usertalk
typeof("hello")      => "string"       ❌ WRONG - breaks all comparisons
typeof(filespecValue) => "filespec"     ❌ WRONG - code expects 'fss '
```

**WHY THIS MATTERS**:
- Entire UserTalk codebase relies on typeof() returning OSType codes
- Constants are looked up in `system.compiler.language.constants` to get the 4-byte values
- Changing to string names breaks ALL typeof() comparisons in production code
- Even "improving" the type system by returning strings would catastrophically break code

**HISTORICAL INCIDENT** (2026-01-02):
- Attempt made to "fix" typeof() to return string names like "filespec"
- Would have caused production failure in all UserTalk code using typeof()
- Caught and reverted immediately - this must NEVER happen again

**IMPLEMENTATION**:
- `typeof()` implementation: Common/source/langvalue.c, function `typefunc()`
- Type mappings: Common/source/langops.c, `typeinfo[]` array and `langgettypeid()`
- String to type conversion: `langgetvaluetype()` converts OSType codes back to tyvaluetype enum

**LESSON**: When fixing typeof() test failures, check the test expectations first - don't change typeof() behavior. The correct approach is to fix the test to match the correct typeof() behavior.

### File Path Requirements

**CRITICAL**: All UserTalk file/database verbs require FULL/ABSOLUTE paths—the runtime has NO cwd awareness at the UserTalk level.

**Affected Verbs**: `db.new()`, `db.open()`, `file.create()`, `file.write()`, `file.read()`, and all file.* operations

**Correct Usage**:
```usertalk
// ✅ CORRECT
db.new("/Users/jake/test.root")
local(fullPath = file.getcwd() + "/test.root")
db.new(fullPath)

// ❌ WRONG - relative paths fail
db.new("test.root")
```

**Testing**: Use `{FRONTIER_TEST_TMP_DIR}` template in integration tests or `$(./tools/get_test_temp_path.sh)` for CLI testing.

**Note**: Load system root with `--system-root databases/Frontier.root` to initialize `system.paths` and enable `target.*` verbs.

### UserTalk Coding Style for Tests ⚠️

**CRITICAL RULES** for writing UserTalk test scripts:

1. **Inline Comments NOT Supported Inside Blocks**
   - ❌ WRONG: `if true { // comment ... }`
   - ❌ WRONG: `try { // comment ... }`
   - ❌ WRONG: `on handler() { // comment ... }`
   - ✅ CORRECT: `// comment` at top level (outside blocks)
   - **Reason**: UserTalk parser limitation - inline comments only work at file level, not inside code blocks

2. **Blank Lines Inside Blocks Must Have Matching Indentation**
   - ❌ WRONG: Blank line with no indentation inside indented block
   - ❌ WRONG: Blank line with wrong indentation level
   - ✅ CORRECT: Blank lines must match the indentation level of surrounding statements
   - ✅ SIMPLEST: Avoid blank lines inside blocks entirely (use compact formatting)
   - **Reason**: UserTalk file parser requires indentation level to match previous line, even for blank lines
   - **Practical advice**: Tests should use compact formatting without blank lines inside handlers/blocks

3. **Test Data Storage: Use system.temp, NOT system.verbs**
   - ❌ WRONG: `system.verbs.tcp.test.foo = "bar"`  (modifies system table)
   - ✅ CORRECT: `new(tableType, @system.temp.tcpTest); system.temp.tcpTest.foo = "bar"`
   - **Rule**: NEVER modify `system` table in tests - always use `system.temp.*`
   - **Cleanup**: Always `delete(@system.temp.tcpTest)` at end of test

4. **Prefer Flat, Simple Structure**
   - Avoid complex multi-line blocks where possible
   - Keep blocks short and obvious
   - UserTalk was designed for outline editing, not complex text-based nesting
   - **Historical Context**: Original Frontier used outline editor where indentation was automatic - braces `{`, `}`, and `;` were rarely typed

**Example - Proper Test Pattern**:
```usertalk
new(tableType, @system.temp.tcpTest);
system.temp.tcpTest.result = false;

on tcpHandler(streamID, addr, port) {
  system.temp.tcpTest.result = true;
  tcp.closeStream(streamID);
  return true
};

local(listenID = tcp.listenStream(9000, 5, @tcpHandler, 0, 0));
local(clientID = tcp.openAddrStream(0x7F000001, 9000));

thread.sleepFor(100);

local(success = system.temp.tcpTest.result);
tcp.closeStream(clientID);
tcp.closeListen(listenID);
delete(@system.temp.tcpTest);

return success
```

---

## Critical Testing Constraints ⚠️

### macOS Sandbox /tmp Restriction

**CRITICAL**: frontier-cli runs in the macOS sandbox and **CANNOT access `/tmp`**.

**When Testing File Operations**:
- ❌ NEVER use `/tmp`, `/var/tmp`, or system temp directories
- ✅ ALWAYS use project-relative paths in .gitignore'd subdirectories
- ✅ Use `{FRONTIER_TEST_TMP_DIR}` template in integration tests (auto-replaced)
- ✅ Use `$(./tools/get_test_temp_path.sh)` for manual CLI testing
- ✅ Use `tests/tmp/unit/` or similar project subdirectories for testing

**Examples**:
```bash
# ❌ WRONG - Will fail in sandbox
./frontier-cli/frontier-cli -e 'file.write("/tmp/test.txt", "data")'

# ✅ CORRECT - Project-relative path
mkdir -p tests/tmp/unit  # .gitignore'd directory
./frontier-cli/frontier-cli -e 'file.write("tests/tmp/unit/test.txt", "data")'

# ✅ CORRECT - Using helper script
TESTDIR=$(./tools/get_test_temp_path.sh)
./frontier-cli/frontier-cli -e "file.write(\"$TESTDIR/test.txt\", \"data\")"
```

**Integration Test Pattern**:
```yaml
# Use template - framework replaces with safe path
tests:
  - name: "file.write - create file"
    script: 'file.write("{FRONTIER_TEST_TMP_DIR}/test.txt", "data")'
    expected_success: true
```

**Agent Delegation Template**:

When delegating file operations work to agents, ALWAYS include:
```
CRITICAL CONSTRAINT: frontier-cli runs in macOS sandbox and CANNOT access /tmp.
Use project-relative paths in .gitignore'd subdirectories for testing.
Use $(./tools/get_test_temp_path.sh) to get a safe temp directory.
```

---

## Outline Structure - Critical Architectural Fact ⚠️

**All new outlines start with a single empty summit headline.**

This is fundamental to Frontier's outline implementation:

```usertalk
lang.new(outlineType, @outline);  // Creates outline with 1 empty headline
target.set(@outline);
op.insert("First Item", down);     // Adds BELOW empty summit (now 2 lines)
```

**Key Implications**:
- `op.insert()` **adds** to the outline, does NOT replace the empty summit
- To replace empty summit: use `op.setLineText("text")` on first operation
- To skip empty summit: use `op.deleteLine()` before first insert
- Line counts include the empty summit (2-node parent+child = 3 total lines)
- `op.firstSummit()` lands on the empty summit (line 1)

**Documentation**: See `docs/OUTLINE_STRUCTURE.md` for complete details.

---

## Memory Logging False Alarms - Not Corruption ⚠️

**Symptom**: `loadfromhandle fail: ix=0 ct=24 size=12` errors during list operations

**Cause**: These are NOT memory corruption - they're **verbose format detection logging**.

The `langunpackvalue()` function tries to unpack values in two formats:
1. **Old format** (24-byte header) - tried first, logs error if handle too small
2. **New format** (4-byte header) - tried second, succeeds

**Example error log:**
```
[general-ERROR] memory.c:1480: loadfromhandle fail: ix=0 ct=24 size=12 caller=langunpackdata
```

**Translation**: "Tried old format (24 bytes), handle only has 12 bytes, falling back to new format"

**Expected behavior**: Operations complete successfully despite the error logs.

**Fix**: These errors can be suppressed by adjusting log level, but are harmless.

**File**: `Common/source/langpack.c` lines 505-539 (langunpackvalue format detection)

---

## Architectural Patterns to Avoid

### Hash Table Lookup API - Null Pointer Gotcha ⚠️

**Issue #199 Root Cause**: Frontier has two hash lookup functions with subtle differences:

**Correct API**:
```c
// When you need BOTH value and node:
tyvaluerecord val;
hdlhashnode node;
if (hashtablelookup(htable, name, &val, &node)) { /* Safe */ }

// When you ONLY need the node:
hdlhashnode node;
if (hashtablelookupnode(htable, name, &node)) { /* Safe */ }
```

**WRONG - Causes Segfault**:
```c
// ❌ NEVER pass nil for vreturned:
if (hashtablelookup(htable, name, nil, &node)) {  // CRASHES!
    // hashtablelookup unconditionally dereferences vreturned
}
```

**Why**: `hashtablelookup()` unconditionally writes `*vreturned = value` without checking if `vreturned` is non-null.

**The Fix**: Use `hashtablelookupnode()` when you only need the node, not the value.

---

### Mode Stack Push/Pop Issues ⚠️

The `db_format_mode_current()` push/pop pattern has caused multiple bugs:

**Problem**: When you push a different mode (e.g., legacy reader for v6 tables), recursive operations inherit that mode. If you forget to pop, or if recursive calls don't pop properly, child operations see wrong format state.

**Example (Issue #123)**: During migration, we pushed `legacy_load.use_64bit_format = false` to read v6 tables, but recursive child table packing inherited this mode and wrote v4 headers instead of v5.

**Best Practices**:
1. Use explicit context guards (`db_context_guard`) when switching modes for recursive operations
2. Never rely on mode stack state being restored automatically
3. Consider using explicit context parameters instead of global mode state
4. When in doubt, check `db_format_mode_current()` at the point where it's used

---

### Reader/Writer Fork Architecture

Frontier has separate legacy (v6, 32-bit) and modern (v7, 64-bit BE) reader/writer code paths.

**Gotcha 1**: DATABASE format mode and TABLE header version are NOT the same:
- `dbopenfile()` sets `db_format_mode.use_64bit_format` based on DATABASE version
- `hashunpacktable()` used to check only TABLE header version, not database mode
- Result: Root table could unpack with wrong reader even if database is v7

**Gotcha 2**: Table packing must respect OUTPUT database format:
- Don't rely on mode stack state inherited from earlier operations
- Explicitly push modern mode before packing if writing to v7 database
- Always validate header versions (version=5 for v7, version=4 for v6)

---

### External Table Variable Migration

External table variables store either:
- Memory pointers (`flinmemory=1`) - no migration issues
- Database addresses (`flinmemory=0`) - **addresses are format-dependent**

**Critical**: If `flinmemory=0` tables migrated with wrong address format:
- v6 addresses (32-bit) stored in v7 database don't point to valid blocks
- `dbnormalizeaddress()` fails when trying to access them
- Error: `dbnormalizeaddress failed for adr=0x62bb33`

**Safe approach**: Force external tables into memory (`flinmemory=1`) during migration.

See `docs/external_table_variable_management.md` for migration patterns.

---

### Global Mutable State - CRITICAL FOR LAUNCH ⚠️⚠️⚠️

**BURN THE GLOBALS WITH FIRE. EVERYWHERE.**

Frontier has multiple global mutable state variables that must be eliminated before launch:

**Known Problem Areas:**
- `outlinedata` and `outlinestack` (oppushoutline/oppopoutline) - outline context
- `databasedata` and legacy database globals - database context (partially fixed)
- `flnextparamislast` and parameter-related globals - **PATTERN ESTABLISHED** (see ADR-005)
- Any static buffers or caches that aren't guarded by locks

**Why This Matters**: This code MUST be thread-safe before launch. Global mutable state makes thread safety impossible.

**Refactoring Patterns (proven to work)**:

**Pattern 1: Thread-Local Storage (for per-thread state)**
1. Add field to `tythreadglobals` structure
2. Update thread swap functions (copythreadglobals, swapinthreadglobals)
3. Replace global with macro accessor for backward compatibility
4. Zero API changes - transparent to existing code

**Pattern 2: Explicit Context (for per-operation state)**
1. Create explicit context structure (e.g., `op_context`, `db_context`)
2. Thread context through function parameters instead of relying on globals
3. Maintain backward-compatible wrappers using default context
4. Gradually eliminate global variable access

**When to Use Which**:
- **Thread-Local**: Per-thread execution state (flnextparamislast, flscriptrunning, current outline)
- **Explicit Context**: Per-operation state (database operations, outline operations)

**Documentation**:
- **ADR-005**: Parameter state thread-safety (thread-local pattern reference)
- **docs/THREAD_LOCAL_GLOBALS_PATTERN.md**: Step-by-step migration template
- **Issue #135**: Outline context refactoring (explicit context pattern reference)

---

### Timestamp Type Migration ⚠️

Frontier uses 64-bit timestamps (`frontier_time_t` = `int64_t`) to avoid Year 2038. When pulling new source, audit for uint32_t usage. Test suite automatically checks this.

**Rule**: Legacy disk readers (v4/v6) can use uint32_t; everything else must use int64_t or `frontier_time_t`.

See [`docs/TIMESTAMP_AUDIT.md`](docs/TIMESTAMP_AUDIT.md) and `docs/frontier_time_t_standard.md`

---

### Database Debugging Patterns ⚠️

**CRITICAL LESSONS FROM PR #185**

#### When Investigating Database-Related Bugs

1. **Always verify if crashes are pre-existing** using git bisect-style testing
   - Checkout commits before/after suspected changes
   - Rebuild and test at each point
   - **For format bugs**: Re-migrate at each bisect step (pre-migrated v7 database gives false results)

2. **Trace global state reliability through full call chains**
   - Don't assume globals are wrong without verification
   - Globals may be set correctly at higher levels even if not explicitly passed

3. **Check context propagation patterns**
   - Look for `db_context *ctx` parameters in unpacking functions
   - Verify context→global assignments

4. **Key files for database context debugging**:
   - `Common/source/db_format.c` - Context→global assignments during unpacking
   - `Common/source/langhash.c` - Hash table unpacking flow
   - `Common/source/tablepack.c` - Table unpacking implementation
   - `Common/source/langexternal.c` - External variable creation and lifecycle

#### Database File Corruption in Git

Database files can become corrupted in git if migration code has bugs. **Always validate database files before using them**:

```bash
# Check database version (first 2 bytes: 0006 for v6, 0007 for v7)
xxd databases/Frontier.root | head -1
# CORRUPT if v6 file shows: 0007 0000... (v7 header with v6 addresses)
```

**If corrupted**:
1. Find last known-good commit: `git log --oneline -- databases/Frontier.root`
2. Restore: `git show <commit>:databases/Frontier.root > databases/Frontier.root`
3. Verify with `xxd`

#### Context Guard Pattern is CORRECT ✅

**IMPORTANT**: The `db_context_guard` pattern is **NOT** the same as the problematic push/pop anti-pattern. Context guards are the **CORRECT** solution:

```c
boolean dbrefhandle_context(const db_context *context, dbaddress adr, Handle *h) {
    db_context_guard guard;
    db_context_guard_enter(context, &guard);  // Saves state
    boolean ok = dbrefhandle(adr, h);
    db_context_guard_exit(&guard);            // Restores state
    return ok;
}
```

**Why guards are correct**:
- Explicitly save state on entry
- Explicitly restore state on exit
- Scoped to single operation (no inheritance to recursive calls)
- Deterministic cleanup even on error paths

#### Global State Must Be Restored

When temporarily changing global state, **failure to restore previous value causes cascading failures**:

```c
// BROKEN CODE (removed db_context_guard):
boolean dbrefhandle_context(const db_context *context, dbaddress adr, Handle *h) {
    databasedata = context->database;  // Sets global
    return dbrefhandle(adr, h);        // Returns WITHOUT restoring!
}
// Next operation sees WRONG database → segfault
```

**Rule**: Any function that temporarily modifies global state MUST restore previous value before returning.

---

## Collaborative ODB Editing - Architectural Foundation

Frontier's Object Database (ODB) is designed to support multi-user collaborative editing at scale. This is a **foundational architectural decision**, not a future feature—every design choice must accommodate this trajectory.

**Key Requirements**:
1. **Reference Counting for All External Object Contexts** - ODB objects stay valid while any thread holds a reference
2. **Single-Threaded Developer Model** - UserTalk scripts see isolated, transactional operations without explicit locking
3. **Stable Data Under Concurrent Load** - No data corruption, race conditions, or mysterious failures with dozens of concurrent operations

See Issue #135 (outline context refactoring) and `planning/CRDT_FOUNDATION_ROADMAP.md`

---

## Knowledge Capture and Documentation

Whenever you discover something significant about this project or its implementation that isn't already documented, **create or update the appropriate documentation**:

- Architectural insights and design patterns → `planning/architectural_decision_records/`
- Phase 3 implementation details → `planning/phase3/`
- Completed work and historical context → `planning/archive/`
- General development guides → `docs/`

**Rationale**: The most valuable resource is knowledge accumulated through solving hard problems. Documenting learning prevents it from being lost and makes it discoverable.

---


## Anti-Pattern: Auto-Generated Files Requiring Hand-Edits

**CRITICAL ANTI-PATTERN**: Never create automated tools that generate stub files if those stubs will need to be manually edited in production.

### Why This is a Problem

**Bad Outcomes**:
1. ❌ Manual edits get overwritten if generator is re-run
2. ❌ Developers forget this is generated and waste time trying to fix it
3. ❌ Git history becomes confusing
4. ❌ Future developers don't know which version is authoritative
5. ❌ Creates false sense of "this is done" when stub isn't complete

### How to Fix This Pattern

**Option 1: Make The Generator Complete** ✅ **PREFERRED**
- Update generator to emit correct, production-ready code
- No hand-edits needed - regenerate when requirements change
- Example: Updated `stub_config.py` with `STUB_FORWARD` mode

**For Future Work**: When adding verbs that need real implementations:
- Add entry to `stub_config.py` with correct implementation strategy
- Update generator if needed
- **Don't hand-edit the output** - fix the generator instead

### When You Encounter a Generated File That Needs Custom Logic

**CRITICAL DECISION TREE**: If you find a generated file that needs custom implementations:

**Option 1: Fix the Generator** (Preferred)
1. Create wrapper functions in appropriate source files (e.g., `shellsysverbs.c`)
2. Add entries to `stub_config.py` with `STUB_FORWARD` pointing to wrappers
3. Regenerate the file - implementations stay in sync with generator

**Option 2: Stop and Ask**
- If custom logic is too complex to auto-generate
- If you're unsure whether generator can handle it
- STOP and consult with user/maintainers
- Do NOT hand-edit the generated file

**❌ NEVER: Mark Generated File as Production**
- Do NOT change header to say "PRODUCTION IMPLEMENTATION"
- Do NOT add "DO NOT regenerate" warnings to generated files
- This creates a non-scalable pattern and causes confusion
- If file already has production code, it's a special case (see Option 2)

**Why This Matters**:
- Hand-editing generated files is not scalable
- Future developers won't know if file can be regenerated
- Creates confusion about which version is authoritative
- Violates the principle of keeping generator as source of truth

---

## Logging Standards ⚠️

All debug and diagnostic output must use structured logging macros - **never use `fprintf(stderr, ...)`**.

**Exception:** User-facing terminal output (lang.msg, dialog prompts) may use `fputs()`/`fprintf()` to stdout/stderr for interactive terminal UI. Diagnostic/debug output must use `log_*()` macros.

### Rule: No fprintf(stderr) in New Code

- ❌ NEVER: `fprintf(stderr, "message\n")`
- ✓ ALWAYS: `log_trace(LOG_COMP_DB, "message")` or `log_error()`, `log_debug()`, etc.
- ✅ EXCEPTION: `fputs("user message", stdout)` or `fprintf(stderr, "prompt")` for terminal UI (not diagnostic logging)

### Logging Macros (Priority Order)

1. `log_error(component, ...)` - Critical failures (always shown)
2. `log_warn(component, ...)` - Unexpected but recoverable conditions
3. `log_info(component, ...)` - Startup/shutdown milestones
4. `log_debug(component, ...)` - Diagnostic information
5. `log_trace(component, ...)` - Maximum verbosity

### Logging Components

Use appropriate LOG_COMP_* constant:
- `LOG_COMP_DB` - Database layer
- `LOG_COMP_HASH` - Hash tables
- `LOG_COMP_TABLE` - Table operations
- `LOG_COMP_LANG` - Language runtime
- (See `Common/headers/logging.h` for full list)

### Enforcement

- Script: `./tools/check_fprintf.sh` detects violations
- Details: `./tools/check_fprintf.sh --fix` shows fixes
- Documentation: `docs/LOGGING_STANDARDS.md`

**Reference**:
- API: `Common/headers/logging.h`
- Standards: `docs/LOGGING_STANDARDS.md`
- Plan: `planning/phase3/LOGGING_INFRASTRUCTURE_PLAN.md`
