# Frontier Development Guide

## Project Leadership

**The user is both TPM (Technical Product Manager) and CTO of this project.** This means:
- Strategic vision (2.0 collaborative ODB, partnerships with Dave Winer and Automattic) comes from TPM perspective
- Architectural decisions and technical risk management come from CTO perspective
- When the user asks for trade-off analysis, they're looking for both product and technical viewpoints
- Technical debt decisions are made with full product context in mind

---

## Technical Decision-Making Principles

**When evaluating multiple approaches to solve a problem, default to the proper, maintainable, long-term solution.**

This project is building foundational infrastructure for collaborative ODB editing that will serve as the basis for multi-user systems and partnerships with Dave Winer and Automattic. Quick fixes and workarounds accumulate as technical debt that becomes costly to unwind later.

**Decision Framework:**

When presented with options like:
- **Option 1: Header Guards** (Quick fix - 90% reduction)
- **Option 2: Centralize Types** (Proper fix - 100% elimination)
- **Option 3: Suppress Warnings** (Temporary workaround)

**Default to the proper fix (Option 2) unless:**
- User explicitly requests quick fix for time constraints
- Proper fix would block critical path work (then quick fix + filed issue)
- Quick fix is genuinely the right long-term solution (rare)

**In 90% of cases, recommend the "Proper fix" or "maintainable long-term solution" approach.**

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
rm -f databases/Frontier-v7.root
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  --system-root databases/Frontier-v6.root -e "1"

# Verb coverage analysis
cd tools/kernelverbs_parser && python3 cli.py report

# Create PR (after pushing branch)
# Use pull-request agent, then:
./tools/monitor_pr_review.sh <PR_NUMBER>
```

### Documentation Quick Links

- **[Verb Implementation Guide](docs/VERB_IMPLEMENTATION_GUIDE.md)** - Implementing kernel verbs in C
- **[Testing Guide](docs/TESTING_GUIDE.md)** - CLI usage, testing patterns, database migration
- **[CLI Usage Guide](docs/CLI_USAGE_GUIDE.md)** - Complete frontier-cli reference (600+ lines)
- **[Logging Standards](docs/LOGGING_STANDARDS.md)** - Structured logging requirements

---

## Strategic Roadmap

**Master todo list**: https://drummer.land/me@jakesav.in/JakeShare.opml
- User's longer-term vision in chronological order
- Dave Winer (original Frontier designer) is key strategic partner for collaborative ODB
- Consult this list to understand how tasks fit into broader roadmap
- Source of truth for strategic direction and milestones

**Planning Documentation**:
- `planning/INDEX.md` - Navigation for active and archived workstreams
- `planning/phase_overview.md` - Overview of all phases
- `planning/CRDT_FOUNDATION_ROADMAP.md` - Collaborative ODB foundation (Phase 2.0)

**Key Project Context**:
- Frontier has "guest databases" - any databases opened that aren't system root. Top-level items in guest databases are in global scope (managed via `system.compiler.files`)
- Current "target" is generally a window (database or editor window for non-scalars like scripts, outlines, WPText/RTF)
- Legacy Frontier source: `/Users/jake/dev/tedchoward/Frontier`
- When fixing critical areas (serialization, database format, byte alignment): 1) search `planning/`, 2) confirm alignment with user, 3) proceed
- v7 database format should NOT contain font/style info (except within stored RTF objects)
- **Milestone Commits MUST Use PR Workflow** ⚠️:
  1. Create feature branch: `git checkout -b feature/description`
  2. Commit work (multiple commits OK)
  3. Push to origin
  4. Use pull-request agent to create PR
  5. **Run `./tools/monitor_pr_review.sh <PR>` after EVERY push**
  6. Address bot feedback (minor: auto, critical: user approval)
  7. Merge only after bot approval AND monitoring confirms no follow-up
- Always create branch for new development work when on develop
- Never delete branches without user confirmation
- Never work on develop directly for larger changes

---
## Multi-Session Stability Patterns

**Working across multiple terminal sessions simultaneously requires explicit coordination to prevent conflicts.**

### Before Starting Any Work - Decision Tree

**Step 1: Check current branch**
```bash
git branch --show-current
```

**Step 2: Assess task complexity**
- ✅ **Trivial** (typo fix, single-line change, quick doc update) → OK to work on develop
- ❌ **Non-trivial** (feature, bug fix, refactoring, multi-file change) → Create worktree

**Step 3: If non-trivial and on develop:**
```bash
# Create worktree for new feature (see Worktree Location below for naming)
cd /Users/jake/dev/jsavin/Frontier
git worktree add ../Frontier-<feature-name> -b feature/<feature-name>
cd ../Frontier-<feature-name>
# Now start work here
```

**Step 4: If already on feature branch in worktree:**
```bash
# Verify you're in right worktree
pwd && git branch
# Continue work
```

---

### Key Stability Principles

1. **Worktrees Are Your Foundation**
   - Use git worktrees for parallel work: `git worktree add feature-branch-name`
   - Each worktree has its own working directory, branch state, and build artifacts
   - This allows multiple git branches to be active simultaneously without conflicts
   - Example: Main Frontier directory on develop, separate worktree on feature/new-work

2. **One Feature Branch = One Worktree**
   - Create a worktree when starting new feature development
   - Keep worktrees on feature branches, never on develop
   - When feature is complete, merge PR, then remove worktree: `git worktree remove feature-branch-name`

3. **Develop is the Integration Point**
   - develop branch should only change through merged PRs (never direct commits)
   - All feature work happens on feature branches in worktrees
   - This prevents collisions when multiple sessions touch develop

4. **Database State is Per-Session**
   - Database files (Frontier-v6.root, test_*.root) may be modified by test runs
   - Don't assume database state is consistent across sessions
   - If testing depends on specific database state, commit clean reference databases to git
   - Use `git checkout databases/Frontier-v6.root` to restore reference state between tests

5. **Build Artifacts Are Not Shared**
   - Keep `frontier-cli/frontier-cli` and test executables in their worktree/directory
   - Each session has its own build
   - Don't rely on build artifacts from one terminal in another terminal's build

6. **Communication Protocol for Blocked Work**
   - If Session A blocks Session B (e.g., Session A pushes to develop while B is working on develop):
     - Session B should immediately rebase: `git rebase origin/develop`
     - Session B's worktree automatically reflects the new develop
   - This is why commits to develop MUST go through PR workflow (ensures visibility and proper ordering)

---

### Worktree Location and Naming Convention

**Recommended: Sibling directories to main Frontier directory**

```
/Users/jake/dev/jsavin/
  ├── Frontier/                      (main repo, on develop)
  ├── Frontier-table-verbs-headless/ (worktree for feature/table-verbs-headless)
  └── Frontier-build-fix/            (worktree for fix/build-fix)
```

**Why sibling directories:**
- ✅ Complete isolation (build artifacts, git state, databases)
- ✅ No git interference (worktrees don't appear in main repo's `git status`)
- ✅ IDE-friendly (each appears as separate project)
- ✅ Clear naming pattern makes purpose obvious
- ✅ Easy cleanup when done

**Naming Convention:**
```bash
# For feature branches:
feature/table-verbs-headless  →  Frontier-table-verbs-headless

# For fix branches:
fix/database-corruption       →  Frontier-database-corruption

# Pattern: Strip prefix (feature/, fix/), prepend 'Frontier-'
```

**Creating a New Worktree:**

```bash
# From main Frontier directory on develop
cd /Users/jake/dev/jsavin/Frontier
git worktree add ../Frontier-<feature-name> -b feature/<feature-name>
cd ../Frontier-<feature-name>

# Verify setup
pwd && git branch
# Should show: /Users/jake/dev/jsavin/Frontier-<feature-name>
#              * feature/<feature-name>

# Build and work here
make clean && make
./tools/run_headless_tests.sh
```

**Cleaning Up After PR Merged:**

```bash
# After PR is merged to develop
cd /Users/jake/dev/jsavin
rm -rf Frontier-<feature-name>
cd Frontier
git worktree prune  # Clean up worktree metadata
git branch -d feature/<feature-name>  # Delete local branch (optional)
```

---

### Recommended Setup for This Project

**Session 1 (Feature Development):**
```bash
cd /Users/jake/dev/jsavin/Frontier-build-fix  # worktree on feature branch
git branch -a  # verify you're on feature/*, not develop
# Do work, test locally with ./tools/run_headless_tests.sh
# Create PR when ready, let bot review
```

**Session 2 (Other Work):**
```bash
cd /Users/jake/dev/jsavin/Frontier  # main directory on develop
git checkout develop  # verify you're on develop
# Work on separate feature branch, or research tasks that don't modify code
# Coordinate if you need to push to develop (ask Session 1 first)
```

**Parallel Development Rules:**
- Session 1 (worktree): Feature work on feature/issues-171-159-167
- Session 2 (main dir): Only research, analysis, or separate feature work
- **Never both sessions push to develop simultaneously** - use PR workflow for visibility
- If Session 2 wants to commit to develop, check if Session 1 has open PRs first
- Session 1 should merge and clean up worktree before Session 2 does major develop work

### Pre-Work Checklist

Before starting major work in any session:
1. ✅ Verify which worktree/directory you're in: `pwd && git branch`
2. ✅ Check for uncommitted changes: `git status` (should show "working tree clean")
3. ✅ Sync with origin: `git fetch origin` (see if develop has changed)
4. ✅ If you're on develop, check recent commits: `git log -3`
5. ✅ Ask yourself: "Am I about to work on the right branch for this task?"

### Common Multi-Session Gotchas

**Gotcha 1: Building wrong binary**
- You're in Session 1's worktree, run tests, then switch to Session 2's directory
- Session 2 has stale CLI binary from old build
- **Fix**: Each session rebuilds its own binary, or remove old one: `rm frontier-cli/frontier-cli`

**Gotcha 2: Database corruption from parallel test runs**
- Session 1 runs migration test, updates Frontier-v7.root
- Session 2 runs test at same time, expects old database state
- **Fix**: Don't run tests in parallel; use `git checkout` to reset databases between test runs

**Gotcha 3: Develop branch changes while working on feature**
- Session 1 is on feature branch, hasn't fetched in a while
- Session 2 merges PR to develop
- Session 1's PR conflicts because develop moved
- **Fix**: Session 1 runs `git fetch origin && git rebase origin/develop` before push

**Gotcha 4: Worktree gets "stuck" on merged branch**
- Feature branch was merged, worktree is still pointing to that branch
- Attempting to push fails with "branch no longer exists"
- **Fix**: Delete worktree when feature is merged: `git worktree remove feature-branch-name`

### When Something Goes Wrong

If parallel sessions cause conflicts:

1. **Both sessions on same branch?** Only one should push
   - Coordinate via chat/discussion
   - One session rebases onto latest origin before pushing
   - Other session pulls/rebases after first push succeeds

2. **Database state inconsistent?**
   - Restore: `git checkout databases/*.root`
   - Rebuild: `make -C tests clean && make -C tests save_migration_tests`
   - This resets to known-good state

3. **Worktree "detached" or in bad state?**
   - Delete and recreate: `git worktree remove <name> && git worktree add <name> origin/<branch>`

---

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

## Testing & CLI Usage

**Full Guide:** See [`docs/TESTING_GUIDE.md`](docs/TESTING_GUIDE.md)

### Essential Commands

```bash
# Run full test suite
./tools/run_headless_tests.sh

# Test CLI inline
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli -e "1+1"

# Multi-line UserTalk
./frontier-cli/frontier-cli -e $'lang.new(tableType, @t);\nt.a=1;\nreturn t.a'

# With database loaded
./frontier-cli/frontier-cli --system-root databases/Frontier-v7.root -e "sizeOf(system)"
```

### UserTalk Syntax Gotcha ⚠️

**CRITICAL**: Double quotes for strings, single quotes for character constants!

```usertalk
sizeOf("hello")  // ✅ CORRECT - double quotes for strings
sizeOf('hello')  // ❌ WRONG - syntax error (single quotes = char constant)
```

This is **opposite** of JavaScript/Python where `'x'` and `"x"` are equivalent!

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

### Timestamp Type Migration - uint32_t Audit Required ⚠️

**Context**: Frontier migrated to 64-bit timestamps (`frontier_time_t` = `int64_t`) to avoid the Year 2038 problem. However, legacy code may still use `uint32_t` for timestamps, defeating this migration.

**When pulling new source files into headless builds, ALWAYS audit for uint32_t timestamp usage.**

#### Pre-Merge Checklist for New Files

Before adding any file to headless builds (frontier-cli/Makefile), run this audit:

```bash
# Search for potential timestamp fields
grep -n "uint32_t.*time\|uint32_t.*date\|uint32_t.*second" <new_file>.c
```

For each match, determine if it's:
1. **Disk format structure** (OK - for backward compatibility with legacy databases)
2. **In-memory state** (MUST migrate to `frontier_time_t`)
3. **API parameters** (MUST use `int64_t`/`frontier_time_t`)

#### Example: Correct Pattern

```c
/* Disk format (legacy v4) - OK to keep uint32_t */
typedef struct legacy_diskheader {
    uint32_t timecreated;    // ✅ OK - reading old database format
    uint32_t timelastsave;   // ✅ OK - with conversion to frontier_time_t
} legacy_diskheader;

/* In-memory state - MUST use frontier_time_t */
typedef struct runtime_state {
    frontier_time_t timecreated;    // ✅ Correct - 64-bit in memory
    frontier_time_t timelastsave;   // ✅ Correct - 64-bit in memory
} runtime_state;

/* Conversion when reading disk format */
state.timecreated = (frontier_time_t)disk_header.timecreated;  // ✅ Widen to 64-bit
```

### Automated DateTime Type Checking ✅

**Pre-Merge Enforcement**: The test suite automatically checks for datetime type issues.

```bash
# Runs automatically as part of:
./tools/run_headless_tests.sh

# Or run manually:
./tools/check_datetime_types.sh
```

**What It Checks**:
1. `long` or `unsigned long` used with timestamp field names (timecreated, timemodified, timelastsave)
2. `int32_t`/`uint32_t` with timestamp fields (warnings for manual review)
3. Function parameters using `long` for date/time values

**Whitelisted Files** (Mac GUI only, not in headless):
- `Common/headers/claybrowser.h`
- `Common/source/claybrowserexpand.c`
- `portable/shelltypes_portable.h`
- `portable/wptext_runtime.c` (legacy wp_diskheader disk format)

**When You See Warnings About uint32_t**:
- ✅ **Legacy v4/v6 disk format structures** → OK (backward compatibility for reading old databases)
- ❌ **Modern v7 (BE64) disk format structures** → BAD (use uint64_t)
- ❌ **In-memory structures** → BAD (use frontier_time_t / int64_t)
- ❌ **API parameters** → BAD (use int64_t / frontier_time_t)

**Rule of Thumb**:
- Legacy readers (`Common/source/legacy/`, v4/v6 disk formats): uint32_t OK
- Everything else: Use int64_t or frontier_time_t

**See**: `planning/phase3/datetime_handling_audit.md` for complete findings.

---

**References**:
- `docs/frontier_time_t_standard.md` - 64-bit time standard
- PR #231 - Discovered during file verb implementation
- Issue #167 - Original time_t portability bug

---

### Database Format Debugging & Corruption Detection ⚠️

**CRITICAL LESSONS FROM PR #185**

#### Database File Corruption in Git

Database files can become corrupted in git if migration code has bugs. **Always validate database files before using them**:

```bash
# Check database version (first 2 bytes: 0006 for v6, 0007 for v7)
xxd databases/Frontier-v6.root | head -1
# CORRUPT if v6 file shows: 0007 0000... (v7 header with v6 addresses)
```

**If corrupted**:
1. Find last known-good commit: `git log --oneline -- databases/Frontier-v6.root`
2. Restore: `git show <commit>:databases/Frontier-v6.root > databases/Frontier-v6.root`
3. Verify with `xxd`

#### Git Bisect for Database Format Issues

When debugging database format bugs, **you MUST re-migrate at each bisect step**. Testing with pre-migrated v7 database gives false results if bug is in migration code.

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

## Collaborative ODB Editing - North Star Vision 🎯

**Strategic Context:**

Frontier's Object Database (ODB) is being positioned as the backend for next-generation collaborative editing:
- Dave Winer's outline-centric workflow (currently non-collaborative)
- Multi-user server config management (Automattic partnership)
- Concurrent source code workflows (GitHub integration patterns)
- Any ODB object type (outlines, scripts, WPText, tables, menus, etc.)

**The Vision (Frontier 2.0):**

Frontier should support **Google Docs/Sheets-style collaborative editing of ODB objects**:
- Multiple users edit different ODB objects simultaneously (potentially same object concurrently)
- Developers write functionally single-threaded code (no concurrency awareness required)
- Runtime handles all concurrency, locking, and conflict resolution transparently
- Stability guaranteed even with dozens of concurrent operations

**What This Means NOW (Frontier 1.0)**:

This is a **foundational architectural decision**, not a future feature. Every design choice must accommodate this trajectory:

1. **Reference Counting for All External Object Contexts**
   - Outline context, script context, WPText context must support multiple concurrent references
   - ODB objects stay valid while ANY thread holds a reference
   - Foundation applies to all external object types

2. **Single-Threaded Developer Model**
   - UserTalk scripts should NOT see concurrent modifications from other users
   - Runtime isolates each developer's operations (transactional semantics or versioning)
   - Conflict resolution happens automatically
   - Developers never write `lock(object)` or `await(lock)`

3. **Stable Data Under Concurrent Load**
   - Multiple users editing same ODB objects = stable, correct results
   - No data corruption, race conditions, or mysterious failures
   - Launch-blocking requirement for Automattic partnership

**Known Strategic Partnerships**:
- **Dave Winer** - Multi-user 2.0 with full ODB collaboration
- **Automattic ecosystem** - WordPress, WordPress.com (~40% of public web)

See Issue #135 (outline context refactoring) - where collaborative ODB foundation gets built.

---

## Knowledge Capture and Documentation

Whenever you discover something significant about this project or its implementation that isn't already documented, **create or update the appropriate documentation**:

- Architectural insights and design patterns → `planning/architectural_decision_records/`
- Phase 3 implementation details → `planning/phase3/`
- Completed work and historical context → `planning/archive/`
- General development guides → `docs/`

**Rationale**: The most valuable resource is knowledge accumulated through solving hard problems. Documenting learning prevents it from being lost and makes it discoverable.

---

## Database Context Debugging Pattern

When investigating database-related bugs:

1. **Always verify if crashes are pre-existing** using git bisect-style testing
   - Checkout commits before/after suspected changes
   - Rebuild and test at each point

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

---

## Logging Standards ⚠️

All debug and diagnostic output must use structured logging macros - **never use `fprintf(stderr, ...)`**.

### Rule: No fprintf(stderr) in New Code

- ❌ NEVER: `fprintf(stderr, "message\n")`
- ✓ ALWAYS: `log_trace(LOG_COMP_DB, "message")` or `log_error()`, `log_debug()`, etc.

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
