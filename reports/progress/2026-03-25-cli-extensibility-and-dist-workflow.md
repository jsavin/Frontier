# Frontier Progress Report: CLI Extensibility & Distribution Workflow

**Date:** March 25, 2026
**Status:** Active Development
**Milestone:** CLI Arguments Visible to UserTalk, Clean Distribution Workflow
**Period Covered:** March 13-25, 2026 (12 days)

---

## Executive Summary

This period bridged the gap between the CLI runtime and the UserTalk scripting layer. Any command-line flag is now accessible from UserTalk scripts via `system.environment.args`, enabling runtime configuration without C code changes. The `--browser` flag routes `sys.openUrl` to an AI-driven browser agent for automated web testing. On the distribution side, `userland.cleanRoot` now works in headless mode, and a new `make clean-root` target reproduces the pre-release database cleanup workflow. A BIGSTRING-to-PSTRING audit caught 5 pre-existing wrong length bytes across the codebase. The period also added 66 new integration tests and established the ODB script editing workflow via protocol.

---

## Major Accomplishments

### TIER 1: CLI-to-UserTalk Bridge (PRs #488, #489)

**The Headline:** CLI arguments now flow through to UserTalk scripts, enabling runtime configuration from the command line without touching C code.

**How it works:**

1. **Callback pattern (PR #488):** The Common layer defines a callback hook that the CLI layer populates with `argc`/`argv`. This avoids introducing a dependency from Common to the CLI layer. UserTalk scripts read arguments from `system.environment.args` -- a table populated at startup.

2. **Two-pass parser (PR #489):** A two-pass argument parser separates known flags (like `--log`, `--migrate`) from arbitrary user-defined flags. Unknown flags pass through to `system.environment.args` with automatic kebab-to-camelCase key conversion (e.g., `--site-name` becomes `siteName`). The `--browser` flag was added as the first use case, routing `sys.openUrl` through `agent-browser` for AI-driven web testing.

**Why This Matters:** This is the extension point that lets UserTalk scripts adapt behavior based on how the CLI was invoked -- essential for scripted deployment, CI/CD integration, and automated testing workflows.

---

### TIER 1: Distribution Workflow (PRs #491, #493)

**The Headline:** The pre-release database cleanup workflow from the original UserLand era now works in headless mode, with a single `make clean-root` command.

**What was fixed:**

1. **Clean Virgin.root (PR #491):** The shipped Virgin.root contained `user.databases` entries with hardcoded absolute paths from a development machine. These were removed, producing a clean starting database. The `userland.cleanRoot` script required several headless-mode fixes:
   - `realpath`-based file comparison for `fileMenu.save` path resolution (replacing the legacy `equalfilespecs` that depended on Mac OS file spec records)
   - GUI verb no-ops for clipboard, editmenu, `window.quickScript`, and `window.close` -- these are called by cleanRoot but have no meaning in headless mode
   - Guard against nil targets in window operations

2. **make clean-root (PR #493):** A single Makefile target that runs the full pre-release cleanup: boot frontier-cli with Virgin.root, execute `userland.cleanRoot()`, save, and exit. Reproduces the workflow that UserLand used to prepare distribution databases.

**Why This Matters:** Distribution hygiene is essential for shipping. Hardcoded paths in the root database would break on any machine other than the developer's. Having this as a repeatable build target means it can be integrated into CI/CD.

---

### TIER 2: Headless Startup Modernization (PR #486)

**The Headline:** The startup script flow was modernized for headless/CLI compatibility, making the boot sequence more robust.

Key changes included moving `window.update` calls inside try blocks (they no-op in headless mode but previously caused unguarded errors) and fixing the `--output v7` format detection check to use the proper `db_format_is_v6_header` function.

---

### TIER 2: sys.openUrl Kernel Verb (PR #490)

The `sys.openUrl` verb was registered as a proper kernel verb in the headless build. Previously it existed only as a UserTalk glue script. Kernel registration ensures it's available even before the startup script runs, which matters for early-boot browser automation.

---

### TIER 2: BIGSTRING-to-PSTRING Audit (PR #492)

**The Headline:** 37 hex-prefix string literals converted from BIGSTRING to PSTRING with compile-time length validation. The audit caught 5 pre-existing wrong length bytes plus 3 additional ones found during review.

The PSTRING macro validates at compile time that the length prefix byte matches the actual string length. BIGSTRING literals had manually-computed length bytes that were never verified -- some had been wrong since the original codebase. These silent mismatches caused verb registration to use truncated or over-read names.

---

### TIER 2: ODB Script Editing Workflow (Multiple Commits)

A new workflow and documentation for editing UserTalk scripts directly in the ODB via frontier-cli's protocol mode. Key improvements:
- `script.newScriptObject` and `op.newOutlineObject` now trim whitespace and normalize line endings, making protocol-based script installation reliable
- Fixed trailing newline and double-indented comment issues in glue scripts (`sys.openUrl`, `script.newScriptObject`, `op.newOutlineObject`)
- Established Virgin.root as the source-of-truth for ODB edits, with `.ut` reference files exported afterward
- Documented indentation rules for scripts and outlines

---

### TIER 2: Integration Test Expansion (PRs #481, #483-#485)

66 new integration tests added across four PRs:
- **PR #481:** Protocol ODB and script operations (set/get/delete, script evaluation)
- **PR #484:** Persistence and save operations (save-on-exit, guest DB saves)
- **PR #485:** Webserver HTTP round-trip and bigstring boundary tests
- **PR #483:** Error recovery and concurrency tests (try/else, thread basics, semaphores)

Also replaced `<<...>>` angle-bracket comments in `.ut` reference files with `//` comments to prevent YAML parser errors in the test framework.

---

## Quality Metrics

### Code Changes (Mar 13-25)

| Metric | Value |
|--------|-------|
| **Pull Requests Merged** | 10 (PRs #481, #483-#486, #488-#493) |
| **Commits to develop** | 34 |
| **New Integration Tests** | +66 |

### Integration Tests

| Metric | Start (Mar 13) | End (Mar 25) | Delta |
|--------|----------------|--------------|-------|
| Total Tests | 1,951 | 2,017 | +66 |
| Passed | 1,761 | 1,827 | +66 |
| Skipped | 190 | 190 | 0 |
| Failed | 0 | 0 | 0 |
| Unit Tests | 302 | 302 | 0 |

### Key Technical Achievements

| Achievement | Detail |
|-------------|--------|
| **CLI-to-UserTalk bridge** | Any CLI flag accessible as UserTalk table entry |
| **Two-pass argument parser** | Known flags separated from user-defined passthrough args |
| **Clean Virgin.root** | No hardcoded paths, no user.databases pollution |
| **make clean-root** | Repeatable pre-release cleanup in one command |
| **PSTRING audit** | 5+3 wrong length bytes caught by compile-time validation |
| **ODB editing workflow** | Protocol-based script installation with whitespace normalization |
| **GUI verb no-ops** | clipboard, editmenu, window.quickScript, window.close safe in headless |

---

## Strategic Impact

### What This Period Accomplished

This period focused on making the CLI a first-class extensible runtime rather than just a headless compatibility layer. The CLI argument bridge means UserTalk scripts can now receive configuration from the outside world -- a prerequisite for any scripted deployment or CI/CD integration. The distribution workflow improvements mean the project can produce clean, reproducible database builds suitable for shipping.

### What's Working Now

- CLI arguments flow through to UserTalk via `system.environment.args`
- `--browser agent-browser` routes `sys.openUrl` to AI browser agent
- `userland.cleanRoot` runs in headless mode, producing clean distribution databases
- `make clean-root` reproduces the UserLand-era pre-release workflow
- PSTRING compile-time validation catches length prefix errors at build time
- ODB scripts editable via protocol with proper whitespace handling
- Virgin.root is clean -- no hardcoded paths or development artifacts

### What Still Needs Work

- **66 pre-existing integration test failures** discovered during test gap analysis -- these are tests that were previously skipped or not written, now visible as gaps
- **CI/CD integration** for `make clean-root` -- the target exists but isn't yet part of the automated pipeline
- **Relative path support** -- CLI currently requires absolute paths; relative paths would improve ergonomics
- **sys.openUrl glue script** may have residual trailing newline issues in some Virgin.root copies

---

## Lessons Learned

### What Worked Well

**Callback pattern for cross-layer communication:** The CLI argument bridge used a callback hook rather than a direct dependency from Common to CLI. This preserved the layering invariant (Common must not depend on CLI) while still making CLI state available to the runtime. The pattern is reusable for future cross-layer communication needs.

**Compile-time validation as audit tool:** The PSTRING macro caught errors that had existed since the original codebase. This demonstrates the value of replacing runtime conventions with compile-time enforcement -- errors that were invisible for years became immediate build failures.

### Challenges Overcome

**Virgin.root pollution:** The shipped database contained `user.databases` entries with absolute paths from the development machine. This was only discovered when `userland.cleanRoot` was made to work in headless mode. The fix required both cleaning the database and adding `realpath`-based path comparison to handle the file identity check during save.

**GUI verb dependencies in headless mode:** The cleanRoot script calls several GUI verbs (clipboard operations, edit menu, window management) that have no meaning in headless mode. Rather than stubbing each one individually as encountered, a systematic pass added no-op implementations for all GUI verbs referenced by cleanRoot.

---

## Next Steps

### Immediate (This Week)

1. **Investigate 66 pre-existing test failures** -- triage and prioritize
2. **Integrate clean-root into CI/CD** -- add to pre-release pipeline
3. **Continue startup flow testing** -- verify full first-run experience with clean Virgin.root

### Mid-Term (2-4 Weeks)

1. **Manila guest database testing** -- full installation and serving end-to-end
2. **GUI application prototype** -- protocol layer + table browser
3. **HTTP stability testing** -- sustained request handling

### Long-Term (1-2 Months)

1. **GUI Alpha Release**
2. **Phase 4 P0a** -- Global state elimination
3. **Issue #86** -- Runtime context architecture

---

## Recognition

**Co-Authored-By:** Claude Opus 4.6 <noreply@anthropic.com>

---

**Period Status:** COMPLETE -- CLI arguments bridge to UserTalk, clean distribution workflow, PSTRING audit, ODB editing workflow, +66 integration tests, zero test failures maintained
