# Plan: Stabilize Startup Flow — First Run to Web Setup

## Context

The UserTalk startup bootstrap (`system.startup.startupScript`) runs to completion but key subsystems don't fully initialize — particularly the first-run flow that installs mainResponder/manila, starts the HTTP server, and opens the setup page in a browser. The goal is to get from a clean `make dist` build to a fully working first-run experience: startup script completes all phases, the web server starts, and the browser opens `http://127.0.0.1:<port>/setupFrontier`.

**Critical insight**: `StartupTasks.root` (already in dist/) must be opened and `StartupTasksSuite.main()` must run successfully — it patches UserTalk glue scripts in Frontier.root that are required for `webBrowser.openUrl()` to work in the headless environment.

## Current State

The startup script runs and returns `false` (the value of `system.temp.Frontier.startingUp = false`), which is success. But we don't know which internal phases silently fail in try blocks vs. which succeed. The approach is to work through the startup sequence methodically, verifying each phase.

## Startup Script Phases (in order)

Reference: `usertalk_scripts/Frontier.root/system/startup/startupScript.ut`

1. **StartupTasks.root** (lines 38-49) — Opens StartupTasks.root from next to binary, runs `StartupTasksSuite.main()`, then closes it. Wrapped in try.
2. **Scheduler init** (lines 52-65) — `scheduler.init()`, reschedule pre-defined tasks
3. **About window + log** (lines 68-77) — `window.about()` (no-op stub), `log.startup()`
4. **External startup script** (lines 78-87) — `frontierStartupCommands.txt`, `Frontier.enableAgents()`
5. **Open guest databases** (lines 88-117) — Iterates `user.databases`, opens with `fileMenu.open`, runs `#startup` scripts
6. **Init subsystems** (lines 119-124) — `html.init()`, `webserver.init()`, `betty.init()`, `people.init()`, `custody.init()`, `soap.init()`
7. **Proxy init** (lines 126-138, 213) — Creates `user.webBrowser.proxy.*`
8. **First-run block** (lines 175-203) — If `user.prefs.firstRootRun` is true: `firstRootRun()`, `finishInstall()`, `infoDialog()`, license
9. **Menu build** (lines 205-206) — `system.menus.buildMenuBar()`, `buildSuitesSubmenu()` (no-op stubs)
10. **Frontier.tools.startup** (line 221) — Wrapped in try
11. **User callbacks** (line 223) — `system.callbacks.startup()`
12. **HTTP server start** (lines 225-227) — `inetd.start()` if `flEnableHttpServer`
13. **Open setup URL** (lines 229-235) — `webBrowser.openUrl(system.temp.installer.urlToOpen)` OR `trialVersionCheck()`
14. **Done** (line 237) — `system.temp.Frontier.startingUp = false`

## Implementation Plan

### Phase 1: Diagnostic Pass — Understand What's Actually Happening

**Goal**: Run the startup from a dist build and capture exactly what succeeds, what silently fails, and what errors occur.

**Step 1.1**: Build a fresh dist
```bash
make -C frontier-cli dist
```

**Step 1.2**: Run the CLI in REPL mode from the dist folder and observe startup
```bash
cd dist && ./frontier-cli --system-root Frontier.root
```

**Step 1.3**: After startup, check key state in the REPL:
- `defined(system.temp.Frontier)` — basic startup ran
- `system.temp.Frontier.startingUp` — should be false (startup completed)
- `defined(system.temp.Frontier.startupTime)` — timestamp was set
- `defined(StartupTasksSuite)` — was StartupTasks.root opened? (should be gone if closed)
- `defined(user.databases)` — guest db table exists
- `sizeof(user.databases)` — any databases registered?
- `defined(user.inetd.listens)` — was inetd.start() called?
- `inetd.isDaemonRunning(@user.inetd.config.http)` — is HTTP server running?
- `user.prefs.firstRootRun` — is this still true (first-run flow pending)?
- `defined(system.temp.installer.urlToOpen)` — was finishInstall successful?
- `defined(mainResponder)` — was mainResponder.root installed?
- `defined(manilaSuite)` — was manila.root installed?

This diagnostic pass tells us exactly where the startup gets stuck.

### Phase 2: Fix Issues Found (Iterative)

Based on diagnostic results, fix issues in priority order:

#### P1: StartupTasks.root loading
- **Expected**: StartupTasks.root found next to binary in dist/, opened, `StartupTasksSuite.main()` runs, then closed
- **Key verb**: `frontier.getProgramPath()` → path to CLI binary; `file.folderFromPath()` → dist folder
- **Risk**: If the binary is symlinked or the path resolves differently, `file.exists(startupRoot)` could fail
- **Verification**: Check `frontier.getProgramPath()` from REPL, manually verify path math
- **Files**: `tests/headless_frontier_verbs.c` (if getProgramPath needs fixing)

#### P2: First-run flow (`user.prefs.firstRootRun`)
- If `user.prefs.firstRootRun` is `true`, the startup calls:
  1. `userland.firstRootRun()` — initializes user.prefs, user.callbacks, betty, people, custody, soap, etc.
  2. `userland.finishInstall()` — installs mainResponder.root + manila.root via `userland.installApp()`, opens prefs.root, creates Admin membership group, sets `system.temp.installer.urlToOpen`
  3. `infoDialog()` — only if `finishInstall()` returns false (so we want it to succeed)
- **Key dependency**: `finishInstall()` calls `userland.trialVersionCheck()` (line 182) which needs `userland.isValidSerialNumber` — exists as UserTalk
- **Key dependency**: `finishInstall()` calls `userland.installApp("mainResponder.root")` which calls `fileMenu.open(f, true)` — needs the file at `Guest Databases/apps/mainResponder.root`
- **Key dependency**: `Frontier.getSubFolder("apps")` resolves to `<Frontier.pathString>/Guest Databases/apps/` — needs `Frontier.pathstring` set (line 51, uses `frontier.getFilePath()`)
- **Risk**: `wp.newTextObject()` called in finishInstall line 212 for mailTemplate — NOT implemented in headless. Wrapped in `if not defined` guard, so only called if template doesn't exist yet.
- **Files**:
  - `usertalk_scripts/Frontier.root/system/verbs/builtins/userland/finishInstall.ut`
  - `usertalk_scripts/Frontier.root/system/verbs/builtins/userland/installApp.ut`
  - `usertalk_scripts/Frontier.root/system/verbs/builtins/Frontier/getSubFolder.ut`

#### P3: Subsystem init functions (html, webserver, betty, people, custody, soap)
- These run BEFORE the first-run block
- **Risk**: `html.init()` calls `pikeRenderer.init()` at the end — if pikeRenderer table doesn't exist in the database, this fails and propagates (NOT in try)
- **Risk**: `betty.init()` accesses `betty.data.rpcHandlers` — if missing from DB, fails
- **Fix options**: Wrap risky calls in try blocks OR ensure tables exist in the migrated database
- **Files**: These are UserTalk scripts in the binary database, not .ut files we can edit directly. If they need changes, we'd need to modify them via the REPL or create patches in StartupTasks.root.

#### P4: HTTP server startup
- `inetd.start()` iterates `user.inetd.config`, calls `inetd.startOne()` for each daemon with `startup=true`
- `inetd.startOne()` calls `tcp.listenStream(port, count, @inetd.supervisor, port, addr)` — all implemented in C
- **Key**: `user.inetd.config.http` must exist with port, count, startup=true
- **Risk**: Port conflicts (e.g., port 80 requires root, port 5335 may be in use)
- **Verification**: Check `user.inetd.config.http` values, try `inetd.startOne(@user.inetd.config.http)` manually

#### P5: webBrowser.openUrl() for setup page
- After `finishInstall()` sets `system.temp.installer.urlToOpen`, line 232 calls `webBrowser.openUrl(system.temp.installer.urlToOpen)`
- Current `webBrowser.openUrl()` calls `webBrowser.launch()` which tries AppleEvents — fails in headless
- **This is where StartupTasks.root matters**: `StartupTasksSuite.main()` patches `webBrowser.openUrl()` (and possibly `launch()`) to work in headless mode (likely using `sys.unixShellCommand("open <url>")` or similar)
- **If StartupTasks.root runs first** (Phase 1 in startup), the patched version is available by the time line 232 executes
- **Verification**: After startup, call `webBrowser.openUrl("http://127.0.0.1:5335/setupFrontier")` from REPL

#### P6: Non-first-run path (`trialVersionCheck`)
- On subsequent startups (not first run), `trialVersionCheck()` runs instead
- Calls `userland.isValidSerialNumber()` — exists as UserTalk
- If trial expired, tries to open browser and disable UI — should be OK since serial numbers aren't relevant for open-source
- **Potential fix**: Set `user.prefs.serialNumber` to a non-trial value during first run, or bypass the check entirely

### Phase 3: Fix `wp.newTextObject` Stub (if needed)

If `finishInstall()` fails because `wp.newTextObject()` is unimplemented:
- Add a minimal headless implementation that creates a wptext value from a string
- **File**: `tests/headless_wp_verbs.c`
- This is needed for creating the Admin group's `mailTemplate` in members.root

### Phase 4: End-to-End Verification

1. `make -C frontier-cli clean && make -C frontier-cli dist`
2. `cd dist && rm -f Frontier.root7` (clean slate)
3. `./frontier-cli --system-root Frontier.root` (REPL mode)
4. Verify:
   - Startup completes without errors
   - `system.temp.Frontier.startingUp == false`
   - `defined(mainResponder)` is true (mainResponder installed)
   - `defined(manilaSuite)` is true (manila installed)
   - `inetd.isDaemonRunning(@user.inetd.config.http)` is true (web server running)
   - Browser opens to setupFrontier page
   - setupFrontier page loads and is functional
5. Run integration tests: `cd tests && make test-all`

## Key Files

| File | Role |
|------|------|
| `usertalk_scripts/Frontier.root/system/startup/startupScript.ut` | Main startup orchestrator |
| `usertalk_scripts/Frontier.root/system/verbs/builtins/userland/finishInstall.ut` | First-run installer |
| `usertalk_scripts/Frontier.root/system/verbs/builtins/userland/firstRootRun.ut` | First-run init |
| `usertalk_scripts/Frontier.root/system/verbs/builtins/userland/installApp.ut` | Guest DB installer |
| `usertalk_scripts/Frontier.root/system/verbs/builtins/userland/trialVersionCheck.ut` | Trial check |
| `usertalk_scripts/Frontier.root/system/verbs/builtins/Frontier/getSubFolder.ut` | Path resolver |
| `usertalk_scripts/Frontier.root/system/verbs/builtins/webBrowser/openURL.ut` | Browser open (patched by StartupTasks) |
| `usertalk_scripts/Frontier.root/system/verbs/builtins/inetd/start.ut` | HTTP server startup |
| `usertalk_scripts/Frontier.root/system/verbs/builtins/inetd/startOne.ut` | Individual daemon start |
| `tests/headless_frontier_verbs.c` | C impl of frontier.getProgramPath |
| `tests/headless_tcp_verbs.c` | C impl of tcp.listenStream |
| `tests/headless_inetd_verbs.c` | C impl of inetd.supervisor |
| `tests/headless_wp_verbs.c` | May need wp.newTextObject |
| `frontier-cli/Makefile` | dist target (lines 318-357) |
| `databases/StartupTasks.root` | Patches glue scripts for headless |

## Approach: Diagnostic-First, Then Iterate

This is fundamentally a **debugging and stabilization** task, not a greenfield implementation. Most of the infrastructure exists. The plan is:

1. **Observe** — Run from dist, capture what happens
2. **Identify** — Find the specific failures
3. **Fix** — Address each blocker in dependency order
4. **Verify** — Test end-to-end after each fix
5. **Repeat** — Until the full flow works

We do NOT pre-emptively add try blocks or stubs everywhere. We fix what's actually broken, verified by evidence.
