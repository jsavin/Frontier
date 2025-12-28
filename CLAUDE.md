## Project Leadership

**The user is both TPM (Technical Product Manager) and CTO of this project.** This means:
- Strategic vision (2.0 collaborative ODB, partnerships with Dave Winer and Automattic) comes from TPM perspective
- Architectural decisions and technical risk management come from CTO perspective
- When the user asks for trade-off analysis, they're looking for both product and technical viewpoints
- Technical debt decisions are made with full product context in mind

---

## Strategic Roadmap

**Master todo list**: https://drummer.land/me@jakesav.in/JakeShare.opml
- This OPML outline contains the user's longer-term vision in roughly chronological order
- Items evolve and are adjusted as we learn and complete work
- When prioritizing work, consult this list to understand how a task fits into the broader roadmap
- This is the source of truth for strategic direction and milestones

**Planning Documentation**:
- `planning/INDEX.md` - Navigation for active and archived workstreams
- `planning/phase_overview.md` - Overview of all phases (active and archived)
- `planning/Frontier_Refactoring_Plan.md` - Narrative goals and risks
- `planning/CRDT_FOUNDATION_ROADMAP.md` - Collaborative ODB foundation (Phase 2.0 vision)
- `planning/_PHASE4_MULTI_USER_PLAN.md` - Multi-user editing strategy (beyond Phase 1.0)

---

- Frontier has a concept of "guest databases" which are any databases that are opened that aren't the system root. All top-level items in guest databases are in global scope in the UserTalk domain. This is managed by the kernel leveraging the in-memory "table" at system.compiler.files.
- Frontier has the concept of the current "target" which is generally a window. That might be a database or it might be an editor window for a non-scalar like a script, outline, or WPText object (which we're now persisting as RTF in UTF-8).
- Legacy Frontier source code is available at /Users/jake/dev/tedchoward/Frontier
- When you're asked to fix something in a critical area (serialization, database format, byte alignment, byte ordering, etc.) you should always 1) first search the `planning/` directory for relevant documentation, 2) ask the user: "I found X in the planning docs - does this change align with that plan?" and 3) only proceed after user confirmation.
- When touching files in certain directories, the commit message should reference the relevant planning doc(s), to force conscious acknowledgment.
- Any change to a typedev struct with "disk" in the name should trigger a question to the user before implementation.
- The v7 database format should not contain any font, font size, or font style information *except* within stored RTF objects.
- Run the headless test flow with `./tools/run_headless_tests.sh` (rebuilds CLI, migrates `databases/Frontier-v6.root` to `databases/Frontier-v6-v7.root`, then runs `make -C tests test`); use this as the standard before/after change check.
- Run the verb binding analyzer with `cd tools/kernelverbs_parser && python3 cli.py analyze` (shows current verb detection: implemented vs stubbed). Use `python3 cli.py report` to generate detailed coverage reports. Use `python3 cli.py report -o -` for stdout output.
- **Milestone Commits MUST Use PR Workflow** ⚠️ CRITICAL PROCESS:
  1. Create a feature branch: `git checkout -b feature/description-of-work`
  2. Commit work to the feature branch (multiple commits OK)
  3. Push the branch to origin
  4. **Use the pull-request agent** to create and manage the PR (do NOT commit directly)
  5. Let the PR bot review the code
  6. Address any bot feedback (minor issues automatically, critical issues with user approval)
  7. Merge to develop only after bot approval
  - **Never commit directly to develop** - all milestones must go through PR review
  - Every completed feature/fix should be a separate PR
  - This ensures code quality gates and prevents regressions
- Whenever you're about to start new development work, always create a branch for that work if the local tree is currently on "develop".
- **Be cautious with branch switching:** You can switch branches normally, but avoid switching if:
  - Work might be lost (uncommitted changes on current branch)
  - Multiple operations are happening in parallel on the same branch (the user might be working in another terminal on the same branch)
  - Ask when unsure if parallel work is in progress on a branch
- You have permission to use the `gh` command.
- Don't ever create PRs that would merge with the tedchoward upstream fork.
- If you ever need to check how the legacy Frontier app implemented something in 32-bit-land, look at the code under `../tedchoward/Frontier/`.
- When the user asks you a question, always answer it first before jumping into work.
- Always ask the user first before pushing changes to origin/develop.
- After pushing a PR to origin, immediately run `./tools/monitor_pr_review.sh <pr_number>` to wait for bot code review feedback. Address minor issues (documentation, magic numbers, style, logging standards) automatically without user involvement. For critical issues or complex fixes, discuss with the user first before implementing.
- When deciding where to track future work, use documents in the planning directory by default for work directly related to getting the headless Frontier runtime working on modern systems, and use GitHub issues (via the `gh` command) for future improvements beyond functional parity with the legacy Frontier runtime.
- **Planning directory structure**:
  - `planning/phase3/` - Active Phase 3 implementation work and analysis
  - `planning/architectural_decision_records/` - Architectural decisions and design standards that affect current and future work (e.g., MODE_SINGLE_DECISION_POINT.md)
  - `planning/archive/` - Completed work and historical reference materials
  - When making architectural decisions that will affect multiple work areas, document them in `planning/architectural_decision_records/`
- **GitHub issue tagging:** When creating or updating issues, follow the labeling strategy documented in `planning/labeling-strategy-proposal.md`. Use priority labels (priority/p0-p3), workstream labels, and type labels to ensure issues are discoverable and properly categorized.
- Error messages exposed to end-users in the UserTalk realm always take the form of: "Can't do X because Y. [Try Z instead.]"
- Never delete a local or remote branch without confirming with the user first.
- Avoid using "magic numbers" in code. Instead create static constants (or variables if the language doesn't support static constants) with names that explain what the constant means to developers.
- When implementing new kernel verbs in C: (1) Add case statement in appropriate verb function (e.g., `sysverbfunc` in shellsysverbs.c), (2) Use `getstringvalue(hparam1, N, varname)` to extract parameters, (3) Convert Pascal strings to C strings with `nullterminate(varname)`, (4) Convert C strings back to Pascal with `copyctopstring(cstr, result)`, (5) Use `setstringvalue(result, v)` or `setlongvalue()` to return values, (6) Mark last parameter with `flnextparamislast = true`, (7) Run `./tools/run_headless_tests.sh` to verify no regressions.
- Creating new C test files that call UserTalk requires complex initialization (langinitverbs, environment setup, etc.). Defer detailed test infrastructure work to someone familiar with the test harness. Verify implementations work via `./tools/run_headless_tests.sh` instead.
- Currently, the UserTalk system.startup.startupScript is known to fail because not all of the verbs that it uses have bindings yet. Always test the bootstrapping of the CLI runtime using the `FRONTIER_HEADLESS_SKIP_STARTUP` environment variable that disables the startup scripts.

## Running frontier-cli

The frontier-cli executable must be run from the project root directory (NOT from within frontier-cli/ or tests/). Syntax:

```bash
# Execute inline UserTalk code (no database):
./frontier-cli/frontier-cli -e "1+1"

# Execute with system root database loaded:
./frontier-cli/frontier-cli --system-root databases/Frontier-v6-v7.root -e "sizeOf(system)"

# Skip startup scripts (use when testing bootstrapping):
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli -e "1+1"
```

### Testing Multi-line UserTalk Scripts

Multi-line scripts work in the CLI using bash `$'...'` syntax for proper newline handling:

```bash
# Multi-line script with $'...\n...' syntax:
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli -e $'lang.new(tableType, @t);\nt.key1 = "hello";\nt.key2 = 42;\nreturn "size:" + sizeOf(t) + " key1:" + t.key1'

# Output: size:2 key1:hello

# Single-line works too (statements separated by semicolons):
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli -e "lang.new(tableType, @t); t.key1 = \"hello\"; return t.key1"
```

### Testing lang.new() Verb

The `lang.new()` verb creates new UserTalk objects (tables, outlines, scripts, etc.) in memory:

```bash
# Create a table and verify it exists:
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli -e "lang.new(tableType, @t); return defined(t)"
# Output: true

# Create a table, add data, and read it back:
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli -e "lang.new(tableType, @t); t.key1 = \"hello\"; t.key2 = 42; t.key3 = true; return \"size:\" + sizeOf(t) + \" key1:\" + t.key1 + \" key2:\" + t.key2"
# Output: size:3 key1:hello key2:42

# Test with different object types:
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli -e "lang.new(tableType, @myTable); return typeof(myTable)"
# Output: tableType
```

**Known issues:**
- Empty error message `[lang-ERROR] langcallbacks.c:208:` may appear after successful execution (harmless, can be ignored)
- Multi-line scripts passed as plain strings (without `$'...'`) will fail due to shell parsing

## Database Migration (v6→v7)

**IMPORTANT: Always use clean migration before testing!**

Old migrated databases may be corrupted artifacts from earlier broken migrations. Always delete existing v7 databases and run a fresh migration before running tests:

```bash
# Clean migration workflow (ALWAYS do this before testing):
rm -f databases/Frontier-v6-v7.root test_save_migration*.root
make -C tests clean && make -C tests save_migration_tests
./tests/save_migration_tests

# Output: test_save_migration-v7.root (v7 migrated database in project root)
```

**Running migration:**
```bash
# Clean rebuild and run migration test:
make -C tests clean && make -C tests save_migration_tests
./tests/save_migration_tests

# Output: test_save_migration-v7.root (v7 migrated database)
```

**Testing migrated database:**
```bash
# Test database loads and system table is accessible:
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli --system-root test_save_migration-v7.root -e "defined(system)"

# Test external table variables (critical - tests Issue #123 fix):
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli --system-root test_save_migration-v7.root -e "sizeOf(system.verbs.globals)"

# Test workspace access:
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli --system-root test_save_migration-v7.root -e "defined(workspace)"
```

**Full integration test suite:**
```bash
# Runs migration + all headless tests:
./tools/run_headless_tests.sh
```

See `planning/phase3/MIGRATION_VALIDATION_REPORT.md` for detailed test procedures and known issues.

## Architectural Patterns to Avoid

### Mode Stack Push/Pop Issues ⚠️

The `db_format_mode_current()` push/pop pattern has proven problematic and has caused multiple bugs:

**Problem**: When you push a different mode (e.g., legacy reader mode for loading v6 tables), any recursive operations inherit that mode. If you forget to pop, or if recursive calls don't pop properly, child operations see wrong format state.

**Example from Issue #123**: During migration, we pushed `legacy_load.use_64bit_format = false` to read v6 tables, but recursive child table packing inherited this mode and wrote v4 headers instead of v5. This caused all tables to have wrong format.

**Best practices**:
1. Use explicit context guards (`db_context_guard`) when switching modes for recursive operations
2. Never rely on mode stack state being restored automatically
3. Consider using explicit context parameters instead of global mode state
4. When in doubt, check `db_format_mode_current()` at the point where it's used, don't assume it's what you set earlier

**See**: `planning/phase3/modern_reader_writer_split.md` - Known Issues section, Issue #123

### Reader/Writer Fork Architecture

Frontier has separate legacy (v6, 32-bit) and modern (v7, 64-bit BE) reader/writer code paths. This is intentional but creates gotchas:

**Gotcha 1**: The DATABASE format mode and TABLE header version are NOT the same thing:
- `dbopenfile()` sets `db_format_mode.use_64bit_format` based on DATABASE version
- `hashunpacktable()` used to check only TABLE header version, not database mode
- Result: Root table could unpack with wrong reader even if database is v7

**Gotcha 2**: Table packing must always respect the OUTPUT database format:
- Don't rely on mode stack state inherited from earlier operations
- Explicitly push modern mode before packing if writing to v7 database
- Always validate you're writing correct header versions (version=5 for v7, version=4 for v6)

**See**: `docs/external_table_variable_management.md` - Address format differences between v6 and v7

### External Table Variable Migration

External table variables store either:
- Memory pointers (`flinmemory=1`) - no migration issues
- Database addresses (`flinmemory=0`) - **addresses are format-dependent and fail if written wrong**

**Critical**: If `flinmemory=0` tables are migrated with wrong address format:
- v6 addresses (32-bit) stored in v7 database don't point to valid blocks
- `dbnormalizeaddress()` fails when trying to access them
- Error: `dbnormalizeaddress failed for adr=0x62bb33`

**Safe approach**: Force external tables into memory (`flinmemory=1`) during migration to avoid address format issues entirely.

**See**: `docs/external_table_variable_management.md` - Migration patterns section

### Global Mutable State - CRITICAL FOR LAUNCH ⚠️⚠️⚠️

**BURN THE GLOBALS WITH FIRE. EVERYWHERE.**

Frontier has multiple global mutable state variables that must be eliminated before launch:

**Known Problem Areas:**
- `outlinedata` and `outlinestack` (oppushoutline/oppopoutline) - outline context
- `databasedata` and legacy database globals - database context (partially fixed with db_context)
- Any static buffers or caches that aren't guarded by locks

**Why This Matters:**
This code MUST be thread-safe before launch. Global mutable state makes thread safety impossible.

**Current Status:**
- ✓ Database mode context partially addressed via `db_context` (see PR #125)
- ✓ Outline packing refactored to `opverbpack_internal` (follows single-decision-point pattern)
- ❌ Outline push/pop stack (`oppushoutline`/`oppopoutline`) still used throughout codebase (24+ files)
- ❌ Other global state pockets likely exist

**Refactoring Pattern (proven to work):**
1. Create explicit context structure (e.g., `op_context`, `db_context`)
2. Thread context through function parameters instead of relying on globals
3. Maintain backward-compatible wrappers using default context
4. Gradually eliminate global variable access
5. Document in architectural_decision_records/

**See:** Issue #135 (outline context refactoring)

**Test with:** Multi-threaded tests before launch to verify thread-safety

### Database Format Debugging & Corruption Detection ⚠️

**CRITICAL LESSONS FROM PR #185 (Database Context Segfault Fix)**

#### Database File Corruption in Git

Database files can become corrupted in git if migration code has bugs. **Always validate database files before using them for testing:**

```bash
# Check database version (first 2 bytes should be 0006 for v6, 0007 for v7)
xxd databases/Frontier-v6.root | head -1
# Expected for v6: 0006 0000 0000 0000 0000 005d 1063 0000
# CORRUPT if v6 file shows: 0007 0000... (v7 header with v6 addresses)
```

**If database is corrupted:**
1. Find last known-good commit: `git log --oneline -- databases/Frontier-v6.root`
2. Restore from good commit: `git show <commit>:databases/Frontier-v6.root > databases/Frontier-v6.root`
3. Verify version bytes with `xxd`

**Symptom of corruption**: `dbnormalizeaddress failed` errors during migration/loading with addresses that don't match the database version.

#### Git Bisect for Database Format Issues

When debugging database format bugs, **you MUST re-migrate the database at each bisect step**. Database format is version-dependent; testing with a pre-migrated v7 database will give false results if the bug is in migration code.

**Correct bisect workflow:**
```bash
# Create automated test script that:
# 1. Removes old v7 database
# 2. Restores pristine v6 database from git
# 3. Copies v6 to v7 location
# 4. Loads database (triggers automatic migration)
# 5. Tests if migration succeeded

# Run bisect with automated script
git bisect start
git bisect bad HEAD
git bisect good <known-good-commit>
git bisect run ./tools/bisect_test_db_load.sh
```

**See:** `tools/bisect_test_db_load.sh` for reference implementation

#### Context Guard Pattern is CORRECT ✅

**IMPORTANT CLARIFICATION**: The `db_context_guard` pattern is **NOT** the same as the problematic push/pop anti-pattern documented above. Context guards are the **CORRECT** solution for temporary context switches.

**Context Guard Pattern (CORRECT)**:
```c
boolean dbrefhandle_context(const db_context *context, dbaddress adr, Handle *h) {
    db_context_guard guard;
    db_context_guard_enter(context, &guard);  // Saves current state
    boolean ok = dbrefhandle(adr, h);
    db_context_guard_exit(&guard);            // Restores previous state
    return ok;
}
```

**Why guards are correct:**
- Explicitly save state on entry
- Explicitly restore state on exit
- Scoped to single operation (no inheritance to recursive calls)
- Deterministic cleanup even on error paths

**Push/Pop Anti-Pattern (INCORRECT)**:
```c
// BAD: State persists after function returns
db_format_mode_push(&mode);
some_operation();
// If pop is forgotten or error occurs, wrong mode persists!
db_format_mode_pop();
```

**See:** PR #185 - Removing context guards was the bug; restoring them was the fix

#### Global State Must Be Restored

When temporarily changing global state for an operation, **failure to restore the previous value causes cascading failures:**

**Example from PR #185:**
```c
// BROKEN CODE (removed db_context_guard):
boolean dbrefhandle_context(const db_context *context, dbaddress adr, Handle *h) {
    databasedata = context->database;  // Sets global
    return dbrefhandle(adr, h);        // Returns WITHOUT restoring!
}
// Next operation sees WRONG database → segfault
```

**Root cause**: Function set `databasedata` to a specific database for one operation but never restored the previous value. Subsequent operations saw the wrong database context, causing `dbnormalizeaddress()` failures.

**Rule**: Any function that temporarily modifies global state (`databasedata`, mode flags, etc.) MUST restore the previous value before returning, even on error paths.

## Collaborative ODB Editing - North Star Vision 🎯

**Strategic Context:**

Frontier's Object Database (ODB) is being positioned as the backend for next-generation collaborative editing, specifically supporting:
- Dave Winer's unique outline-centric workflow (currently non-collaborative)
- Multi-user server config management (Automattic partnership)
- Concurrent source code workflows (GitHub integration patterns)
- Any ODB object type (outlines, scripts, WPText, tables, menus, etc.)

**The Vision (Frontier 2.0):**

Frontier should support **Google Docs/Sheets-style collaborative editing of ODB objects** where:
- Multiple users can edit different ODB objects simultaneously (and potentially the same object concurrently)
- Developers write functionally single-threaded code (no concurrency awareness required)
- The runtime handles all concurrency, locking, and conflict resolution transparently
- Stability is guaranteed even with dozens of concurrent operations
- Frontier maintains its developer-facing flexibility (unique features, scripting capabilities)
- Developers should be able to assume their code will work correctly when multiple people are working with ODB data at the same time

**What This Means NOW (Frontier 1.0):**

This is a **foundational architectural decision**, not a future feature. Every design choice must accommodate this trajectory:

1. **Reference Counting for All External Object Contexts (Issue #135 + Beyond):**
   - Outline context (`op_context_t`), script context, WPText context, etc. must all support multiple concurrent references
   - ODB objects stay valid while ANY thread holds a reference to them
   - This is why full reference counting (not simplified stack-based) is required
   - Foundation applies to all external object types, not just outlines
   - See: `planning/architectural_decision_records/collaborative_odb_architecture.md` (when created)

2. **Single-Threaded Developer Model:**
   - A UserTalk script operating on ODB objects should NOT see concurrent modifications (from other users)
   - The runtime isolates each developer's operations (transactional semantics or versioning)
   - Conflict resolution happens automatically (operational transformation, CRDT, or version merging)
   - Developers should never need to write `lock(object)` or `await(lock)`
   - All concurrency complexity is hidden by the runtime

3. **Stable Data Under Concurrent Load:**
   - Multiple users editing same ODB objects = stable, correct results
   - No data corruption, race conditions, or mysterious failures
   - No "eventual consistency" - writes are immediately visible (last-write-wins OR conflict resolution)
   - This is a launch-blocking requirement for any Automattic partnership work
   - Users expect the same stability they get from Google Docs/Sheets

**How This Affects Architecture:**

| Component | 1.0 (Current) | 2.0 Vision | How We Get There |
|-----------|---------------|-----------|------------------|
| **Object Contexts** | Stack-based, single writer | Reference-counted, multi-writer | Full refcount in #135 + similar patterns for other types |
| **Conflict Resolution** | N/A (single writer) | Automatic (OT, CRDT, or merge) | Implement post-1.0 |
| **Developer Code** | Already single-threaded | Stays single-threaded | Transparent at runtime |
| **Database Layer** | Locking at DB level | Locking at object level | #135 foundation enables this |
| **UserTalk Verbs** | No concurrency awareness | No concurrency awareness | Runtime handles it |
| **External Objects** | Assume single access | Support concurrent access | Foundation work (reference counting) enables this |

**What 1.0 Must Get Right:**

1. ✓ Object context structures (reference counting, not stack allocation) - starting with #135 outlines
2. ✓ Thread-safe context lifecycle (acquire/release semantics)
3. ✓ Reference counting for object validity (prevents premature deallocation)
4. ✓ Locking at object granularity (not just DB-level locking)
5. ✓ Foundation extensible to all ODB object types (not just outlines)
6. ❌ Conflict resolution (OK to defer, but foundation must allow it)
7. ❌ Operational transformation (OK to defer, but foundation must allow it)

**Testing Implications:**

Even in 1.0, we must test:
- Multiple threads accessing different nodes in same ODB object (should work)
- One thread saving while another edits (should not corrupt)
- Outline operations with concurrent external object loading (should be safe)
- Reference counting correctness (object stays alive while referenced)
- Extension pattern tested with at least one other external type (script or WPText)

**Known Strategic Partnerships:**

- **Dave Winer** - Outline-centric system, currently single-user, wants multi-user 2.0 with full ODB collaboration
- **Automattic ecosystem** - WordPress, WordPress.com, and partners (managing ~40% of public web)
- These are not hypothetical - they are active, immediate opportunities post-1.0
- Success here unlocks entirely new product categories (collaborative data management)

**See:** Issue #135 (outline context refactoring) - this is where the collaborative ODB foundation gets built

## Knowledge Capture and Documentation

Whenever you discover or learn something significant or important about this project or its implementation that isn't already documented, **create or update the appropriate documentation** to capture that learning:
- For architectural insights and design patterns → `planning/architectural_decision_records/`
- For Phase 3 implementation details and technical analysis → `planning/phase3/`
- For completed work and historical context → `planning/archive/`
- For general development guides and patterns → `docs/`

This ensures:
- Future work can benefit from the learning without rediscovering it
- You can search the documentation hierarchy to find relevant context and past learnings
- Tacit knowledge becomes explicit and shareable
- The documentation tree stays synchronized with actual implementation

**Rationale:** The most valuable resource in this codebase is the knowledge accumulated through solving hard problems. Documenting that learning prevents it from being lost and makes it discoverable for future work.

## Agent Selection Guidelines

When choosing between specialized agents (usertalk-engineer vs system-architect):
- **Use system-architect** if the issue is primarily in the C domain or requires non-UserTalk scripting (C code changes, database format, runtime architecture, memory management, etc.)
- **Use usertalk-engineer** if the issue is primarily in the UserTalk domain (verb implementations that are mostly UserTalk, scripting logic, UserTalk runtime behavior, etc.)

This ensures the right agent with domain expertise handles the work.

## Using Sub-Agents for Complex Tasks

Whenever a task would benefit from specialized analysis or work that a sub-agent can handle autonomously, **use the appropriate Task tool with a sub-agent**:
- **Explore agent** - Understanding codebase structure, searching across multiple files, architectural context
- **system-architect** - Designing implementations, architectural alternatives, complex technical decisions
- **usertalk-engineer** - UserTalk scripting work, verb implementations in UserTalk domain
- **code-review-bar-raiser** - Thorough code review of significant implementations before merge
- **refactoring-consultant** - Planning and executing refactoring work
- **pull-request agent** - Creating comprehensive PR summaries and pushing to origin

Don't do complex analysis or design work manually when an agent can do it better and faster. This is especially true for:
- Multi-file exploration and understanding codebase patterns
- Architectural analysis and trade-off studies
- Design and planning before implementation
- Code review and quality assurance

Using agents frees you to focus on high-level decision-making and context.

## Agent Work Verification Requirements

**CRITICAL**: Agents must verify that fixes actually work end-to-end, not just fix one piece of the architecture:

### Verb Dispatch Example (Issue #166 preparation)

**Problem**: The system-architect agent correctly identified and fixed the verb callback mechanism (preventing `init_efp_1005()` from overwriting callbacks), but didn't verify that the actual verb implementation was wired up.

**Lesson**: When an agent fixes a dispatch/routing issue:
1. ✅ Fix the architectural problem (callbacks, registration, etc.)
2. ✅ Verify the wiring is correct (check that the right callback is set)
3. ✅ **TEST THE ACTUAL VERB END-TO-END** - Call the verb and verify it works
4. ✅ Don't assume the implementation is complete just because the dispatch mechanism is fixed

**What went wrong**: The agent fixed the callback mechanism but didn't catch that `headless_lang_verbs.c` contains a stub implementation that still returns "not implemented" for `lang.new()`.

**How to prevent**: Always test the actual user-facing functionality (in this case, `lang.new(tableType, @t)`) to verify the complete chain works:
- Name resolution → keyword lookup → callback dispatch → actual implementation

**Guidance for agents**:
- For routing/dispatch fixes: Test with actual calls to verify end-to-end functionality
- For infrastructure fixes: Test with real examples that depend on that infrastructure
- Don't stop at "I fixed the routing mechanism" - verify the mechanism actually routes to working code

## Anti-Pattern: Auto-Generated Files Requiring Hand-Edits

**CRITICAL ANTI-PATTERN**: Never create automated tools that generate stub files if those stubs will need to be manually edited in production.

### Why This is a Problem

**Example**: `tests/headless_lang_verbs.c` is marked as "AUTO-GENERATED - DO NOT EDIT BY HAND" but during Issue #166 work, we had to hand-edit the `lang.new()` case to wire up the real implementation.

**The Bad Outcomes**:
1. ❌ Manual edits get overwritten if the generator is re-run
2. ❌ Maintenance burden: developers forget this is generated and waste time trying to fix it
3. ❌ Git history becomes confusing (is this auto-generated or hand-written?)
4. ❌ Future developers don't know which version is authoritative (disk or generator)
5. ❌ Creates a false sense of "this is done" when the stub isn't actually complete

### How to Fix This Pattern

**Option 1: Make The Generator Complete** ✅ **PREFERRED**
- Update the generator to emit correct, production-ready code
- No hand-edits needed - regenerate when requirements change
- Example: Updated `stub_config.py` to handle `STUB_FORWARD` mode that calls real implementations

**Option 2: Don't Auto-Generate What Will Be Edited**
- If code will need hand-edits, don't mark it auto-generated
- Either hand-write it, or document that it's a hybrid
- Accept the maintenance burden explicitly

**Option 3: Separate Generated Template From Editable Code**
- Generate boilerplate/templates in one file
- Hand-editable implementations in separate file
- But this adds complexity and is often not worth it

### Lesson for This Project

For `headless_lang_verbs.c`:
1. **Before**: Auto-generated stub that always returned "not implemented"
2. **The Problem**: We had to hand-edit it for `lang.new()` to work
3. **The Fix**: Updated generator (`stub_config.py`) with new `STUB_FORWARD` category
4. **Regenerate**: Re-run generator to produce correct code (no hand-edits needed)
5. **Result**: Clean, maintainable, regenerable code

**For Future Work**: When adding new verbs that need real implementations:
- Add entry to `stub_config.py` with correct implementation strategy
- Update generator if needed
- **Don't hand-edit the output** - fix the generator instead

## Logging Standards ⚠️

All debug and diagnostic output must use structured logging macros - **never use `fprintf(stderr, ...)`**.

### Rule: No fprintf(stderr) in New Code

- ❌ NEVER: `fprintf(stderr, "message\n")`
- ✓ ALWAYS: `log_trace(LOG_COMP_DB, "message")` or `log_error()`, `log_debug()`, etc.

### Logging Macros (Priority Order)

1. **`log_error(component, ...)`** - Critical failures (always shown)
2. **`log_warn(component, ...)`** - Unexpected but recoverable conditions
3. **`log_info(component, ...)`** - Startup/shutdown milestones
4. **`log_debug(component, ...)`** - Diagnostic information
5. **`log_trace(component, ...)`** - Maximum verbosity (function entry/exit)

### Logging Components

Use the appropriate LOG_COMP_* constant matching the subsystem:
- `LOG_COMP_DB` - Database layer (db.c, db_format.c)
- `LOG_COMP_HASH` - Hash tables (langhash.c)
- `LOG_COMP_TABLE` - Table operations (tablepack.c, tableops.c)
- `LOG_COMP_LANG` - Language runtime (lang.c, langvalue.c)
- (See `Common/headers/logging.h` for full list)

### Special Cases

**Hex Dumps**: Use `log_hex_dump()` instead of streaming fprintf:
```c
// ✗ WRONG
fprintf(stderr, "bytes:");
for (int i = 0; i < len; i++) fprintf(stderr, " %02x", buf[i]);

// ✓ CORRECT
log_hex_dump(LOG_COMP_HASH, LOG_LEVEL_TRACE, buf, len, "bytes");
```

**Expensive Operations**: Guard with `log_enabled()`:
```c
if (log_enabled(LOG_LEVEL_DEBUG, LOG_COMP_DB)) {
    char path[512];
    expensive_path_construction(path, sizeof(path));
    log_debug(LOG_COMP_DB, "Full path: %s", path);
}
```

### Enforcement

- Script: `./tools/check_fprintf.sh` detects fprintf(stderr) violations
- Details: `./tools/check_fprintf.sh --fix` shows what to fix
- Documentation: `docs/LOGGING_STANDARDS.md` - comprehensive guide

### Reference

- Logging API: `Common/headers/logging.h`
- Standards: `docs/LOGGING_STANDARDS.md`
- Plan: `planning/phase3/LOGGING_INFRASTRUCTURE_PLAN.md`
