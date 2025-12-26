## Project Leadership

**The user is both TPM (Technical Product Manager) and CTO of this project.** This means:
- Strategic vision (2.0 collaborative ODB, partnerships with Dave Winer and Automattic) comes from TPM perspective
- Architectural decisions and technical risk management come from CTO perspective
- When the user asks for trade-off analysis, they're looking for both product and technical viewpoints
- Technical debt decisions are made with full product context in mind

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
- Whenever you're about to start new development work, always create a branch for that work if the local tree is currently on "develop".
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

## Database Migration (v6→v7)

**Running migration:**
```bash
# Clean rebuild and run migration test:
make -C tests clean && make -C tests save_migration_tests
./tests/save_migration_tests

# Output: tests/test_save_migration-v7.root (v7 migrated database)
```

**Testing migrated database:**
```bash
# Test database loads and system table is accessible:
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli --system-root tests/test_save_migration-v7.root -e "defined(system)"

# Test external table variables (critical - tests Issue #123 fix):
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli --system-root tests/test_save_migration-v7.root -e "sizeOf(system.verbs.globals)"

# Test workspace access:
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli --system-root tests/test_save_migration-v7.root -e "defined(workspace)"
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
