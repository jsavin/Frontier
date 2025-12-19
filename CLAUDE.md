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
- When deciding where to track future work, use documents in the planning directory by default for work directly related to getting the headless Frontier runtime working on modern systems, and use GitHub issues (via the `gh` command) for future improvements beyond functional parity with the legacy Frontier runtime.
- Error messages exposed to end-users in the UserTalk realm always take the form of: "Can't do X because Y. [Try Z instead.]"
- Never delete a local or remote branch without confirming with the user first.
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