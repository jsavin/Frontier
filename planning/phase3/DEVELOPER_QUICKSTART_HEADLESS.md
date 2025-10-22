# Developer Quickstart — Headless Runtime & Tests

Status
- State: In Progress
- Phase: 1–2
- Last Updated: 2025-10-22
- Notes: Headless tests are green; parser_tests included. CLI builds headless with script execution only (database/network modes pending).
  Current blocker: legacy `dbrefhandle` still needs the classic address→offset translation so the headless loader can hydrate `system.verbs.builtins` (e.g., pointer `0x00580006` lands inside the block we read, but unpacking still fails).

Related Docs
- planning/INDEX.md
- tests/README.md
- planning/headless_stubbed_behavior_matrix.md
- planning/no_ui_linkage_policy.md

Change Log
- 2025-10-22: Documented the outstanding headless loader blocker and where to find Codex transcripts.
- 2025-09-29: Initialized template sections (Status/Related Docs/Change Log).

What You Can Do
- Build and run core/runtime tests without a UI.
- Use `frontier-cli` to execute UserTalk scripts headless (database/network support is deferred).

Key Docs
- Tests overview: tests/README.md
- CLI overview: frontier-cli/README.md

Quick Steps (Typical)
- Build tests: `make -C tests`
- Run db format tests: `./tests/db_format_tests`
- Run core tests: `./tests/core_tests`
- Run runtime tests: `./tests/runtime_tests`
- Run parser tests: `./tests/parser_tests`
- Use CLI to run a script: `./frontier-cli -e "1 + 2"`
- Load the canonical system tables before a run (optional): `./frontier-cli --system-root databases/Guest\ Databases/Frontier.root -e "clock.now()"`

Notes
- Headless builds must not link AppKit/Carbon/Win32; if they do, see planning/no_ui_linkage_policy.md.
- Some UI-dependent verbs are stubbed in headless mode; see planning/headless_stubbed_behavior_matrix.md.
- Database migration:
  - Legacy v≤6 opens in legacy read mode; no silent rewrite.
  - Save implies migration to v7 (header‑only, with timestamped backup). Until Save‑path migration is fully wired, saving a legacy DB fails fast (no write) to prevent format mismatch.
  - CLI migration flags (`--auto-migrate`, `--query`, etc.) are planned but currently disabled in the headless build.
- `--system-root` opens the specified database read-only and exits if the system table cannot be loaded; use the sanitized sample under `databases/` for quick testing.
- Expect warnings about missing legacy tables (`system.misc`, `system.menus`, etc.) when loading the sanitized Frontier.root. The CLI hydrates temporary replacements so scripts still run, but the warnings are a reminder that full migration work remains.

Codex Session Logs
- Primary archive: `../Frontier-codex-sessions/codex_sessions`. `README.md` in that directory explains the workflow and `current_status.md` + `last_session_tail.txt` summarize the latest investigations.
- If you do not see the worktree, fetch and attach it locally:
  - `git fetch origin codex-sessions`
  - `git worktree add ../Frontier-codex-sessions codex-sessions`
- Browse online via https://github.com/jsavin/Frontier/tree/codex-sessions/codex_sessions when you only need a quick reference.

Parser Regeneration (Maintainers)
- You do not need Bison to build. The generated parser C is committed.
- To regenerate the parser locally (optional, maintainer task):
  - Dry run (diff only): `scripts/gen_langparser.sh`
  - Apply C only: `scripts/gen_langparser.sh --apply`
  - Apply C + header: `scripts/gen_langparser.sh --apply --apply-header`
  - Use a specific bison: `BISON=/path/to/bison3 scripts/gen_langparser.sh`
- Migration plan: see planning/phase5/bison3_migration_plan.md for the gated Bison 3 transition while keeping Bison 2.3 compatibility.

Logging (Headless/Tests)
- Compile-time defaults (tests/Makefile):
  - `LAND_GENERALLOG_LEVEL=3` (verbose) and `LAND_GENERALLOG_TARGET=LAND_LOGTARGET_FILE` enabled.
- Runtime overrides (environment variables):
  - `FRONTIER_LOG_LEVEL` = 0|1|2|3 (threshold; 0=off)
  - `FRONTIER_LOG_TARGET` = comma list of `file,about,dialog,debugger,none` (lowercase)
  - `FRONTIER_LOG_FILE` = path to log file (default: `frontierdebuglog.txt`)
  - `FRONTIER_HEADLESS_LOG` = 1 to enable headless init logs (quiet by default)
  - `FRONTIER_DEBUG_SCAN` = 1 to enable scanner identifier debug prints
- Example:
  - `FRONTIER_LOG_LEVEL=1 FRONTIER_LOG_TARGET=none ./tests/runtime_tests`
  - `FRONTIER_LOG_LEVEL=3 FRONTIER_LOG_TARGET=file FRONTIER_LOG_FILE=/tmp/frontier.log ./tests/runtime_tests`

Doc Link Checker
- Run: `python3 scripts/check_doc_links.py`
- Scope: planning/, tests/, frontier-cli/
- Checks: local Markdown links, anchors, and backticked file paths
- Configure: optional future `scripts/check_doc_links.config.json` (ignored if missing)
