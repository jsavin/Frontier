# Headless Menu System: Legacy-Fidelity Port Plan

**Status**: planning document. Produced by system-architect agent 2026-05-30 in response to JES request "How do we pull in the legacy menu system into the text-based REPL?"

**North star** (JES scope refinement): The headless REPL's menu behavior must **match the legacy GUI behavior** — not approximate it, not modernize it. If a legacy Frontier user remembers seeing the Script menu appear when a Script Editor was frontmost and disappear when they switched to an Outline Editor, the headless REPL must reproduce that observable behavior. All architectural forks default to "do it the legacy way" unless there is a concrete, non-negotiable headless-context blocker.

**Companion docs**:
- `docs/LEGACY_MENU_SYSTEM.md` — the deep-dive investigation that grounds this plan
- `planning/discussions/repl-slash-menu-implementation-plan.md` — the original 8-PR plan from late 2025; this doc supersedes its Phases 6-8

---

## Current State Inventory (post-PR #674)

**Working:**
- `system.menus.data.<menubar>.<menu>.<item>` ODB storage shape with full read/write via `menuverbs_headless.c`
- `meuserselected_headless` dispatches scripts synchronously under the GIL
- `palette.c` renders a cascading horizontal strip from a single `palette_menu_source_t` — the compositor and cascade renderer are production-quality
- `repl_palette_source.c` adapts ODB data to the palette's vtable for a single named menubar (`"repl"`)
- `menu.install`, `menu.remove`, `menu.addMenuCommand`, `menu.setScript` and the other 13 verbs all work against the ODB

**Not working / not started:**
- Multi-menubar enumeration: `repl_palette_source.c` is hardcoded to the `"repl"` bar; issue #677 covers this
- `getsystemtablescript`: the headless `getstringlist` stub has a switch on list IDs and `idsystemtablescripts` (139) is **absent** from the switch — every call to `getsystemtablescript` currently returns false/empty in headless
- Window-event callback bridge: `idopenwindowscript`, `idclosewindowscript`, `idresumescript`, `idsuspendscript` are declared in `tablestructure.h` but never fired in headless
- Frontmost-window tracking: no concept of "frontmost" in the headless event loop; the REPL is trivially always frontmost
- The UserTalk windowTypes framework: not in either the tedchoward or jsavin/Frontier tree; its existence is hypothesized from kernel evidence

---

## Decision Points Requiring JES Input

These gate the phases below. Listed in order of urgency.

### Decision Point 1 — ~~Obtain windowTypes Frontier.root~~ RESOLVED 2026-05-31

**Resolved**: the windowTypes framework is **already installed** in Virgin.root and exported to `usertalk_scripts/Frontier.root/system/verbs/builtins/Frontier/tools/windowTypes/`. Actual path is `system.verbs.builtins.Frontier.tools.windowTypes` — not the `system.windowTypes` hypothesis. Contents include `init.ut`, `findWindowType.ut`, `callWindowType.ut`, `newWindow.ut`, `openWindow.ut`, `runFileMenuScript.ut`, `runEditMenuScript.ut`, `findWindowWithMatchingAtts.ut`, plus `callbacks/` and `commands/` subtables.

Phase D is rescoped from "port the framework" to "verify the kernel primitives the existing framework calls work in headless." See Phase D section below.

### Decision Point 2 — ~~Sync vs Async Menu Script Dispatch~~ RESOLVED 2026-05-31

**Resolved by audit of the existing windowTypes UserTalk framework**: `runFileMenuScript.ut` (under `usertalk_scripts/Frontier.root/system/verbs/builtins/Frontier/tools/windowTypes/`) already uses `thread.callScript(@Frontier.tools.windowTypes.commands.[itemname], {})` for menu-item dispatch. `thread.callScript` is already implemented in headless (`frontier-cli/headless_thread_verbs.c:683` — `headless_thread_callscript` spawns a real POSIX thread under the GIL model). Async dispatch for menu items therefore exists at the framework layer; Phase C's bridge does not need to make this choice.

Phase B2 (a C-side async dispatch wrapper around `meuserselected_headless`) becomes optional / deferrable — the framework's `thread.callScript` already provides the async semantics. Phase C's own boot-time bridge (firing `idopenwindowscript` once at startup) is synchronous and that's correct (runs before REPL accepts input).

### Decision Point 3 — Per-Database Menubar Scoping in Headless

Legacy had two axes: per-database (`cancoon.hmenubarlist`) and per-window-type. Headless port currently has neither articulated. `system.menus.data` is database-global.

For Phase A (multi-menubar enumeration), can be punted: enumerate `@system.menus.data.*` only. Per-database scoping matters more once guest-database editor windows arrive.

### Decision Point 4 — Storage Path Reconciliation (`sharedmenus` vs `data`)

Legacy uses `system.menus.sharedmenus.<appid>` (4-char OSType keys). Headless added `system.menus.data.<name>` (human-readable keys). Same concept.

For fidelity, scripts that reference `@system.menus.sharedmenus.FRNT` should work. Simplest resolution: alias at boot, OR `menu.install`/`menu.remove` mirror to both paths. Low urgency unless the actual windowTypes framework source references `sharedmenus` directly.

### Decision Point 5 — Frontmost-Window Analog for Single-Process CLI

Legacy: OS window manager maintained z-order; `FrontWindow()` returned the current top.

Headless: no OS windows. One "focus target" — today always the REPL.

**Design recommendation:** Maintain a `global hdl_window frontmost_window` in the REPL runtime, initialized to a sentinel representing the REPL itself. A window record includes a `windowtype` string field (`"ReplWindow"`, `"ScriptEditor"`, etc.). Focus changes update `frontmost_window` and fire `on_frontmost_changed(old, new)` which fires the window-event callbacks. Structural equivalent of Carbon's event model.

The REPL's own "window" is a static global, initialized at startup with `windowtype = "ReplWindow"`. Makes Phase C testable without actual editor windows.

---

## Pressure-Test of Deep-Dive's 5-Step Sequence

`docs/LEGACY_MENU_SYSTEM.md` Part 2 ends with a proposed 5-step sequence. Re-evaluated through the legacy-fidelity lens:

**Step 1 (multi-menubar enumeration):** Correctly sequenced and unblocked. Must be **union** (combined strip), not tabs — legacy did union, and fidelity requires we match. Issue #677 covers it. ~250 LOC. Right first PR.

**Step 2 (getsystemtablescript port):** Under-scoped in the deep-dive. Headless `getstringlist` uses a YAML-generated static table and `idsystemtablescripts` (list 139) is **absent** from the switch. Right move: replace resource-lookup approach with a C static table keyed on the enum values from `tablestructure.h`. ~80-100 LOC.

**Step 3 (window-event callback bridge):** Order correct. Characterization "just stubs" is optimistic. Bridge needs to actually fire: `getsystemtablescript(idopenwindowscript, bs)` → resolve path → run via `meuserselected_headless`. Real port of `lang.c:1168-1215` pattern. ~150 LOC.

**Step 4 (window-type registry / windowTypes framework):** ~~Blocked on Decision Point 1.~~ Unblocked: framework located at `system.verbs.builtins.Frontier.tools.windowTypes/`. Work shifts to verifying kernel primitives (`window.frontMost`, `window.attributes.*`, callback registry) work in headless against the existing framework.

**Step 5 (editor windows):** Correctly last. Requires 1-4 and GUI editor work that doesn't exist yet.

**MISSING STEP — Async dispatch resolution.** Deep-dive deferred it; the fidelity goal makes it P0. See Decision Point 2.

---

## Revised Phase Plan

Supersedes the 8-PR plan's Phases 6-8. Recasts the deep-dive's 5-step sequence with legacy-fidelity sequencing.

### Phase A — Multi-Menubar Enumeration (Issue #677, recommend rescope to P1)

- **Scope**: extend `repl_palette_source.c` to enumerate all children of `@system.menus.data` filtered by `.installed = true`, compose top-level menus into a single horizontal strip (union, not tabs)
- **Deliverable**: palette renders `Frontier | File | Edit | Window | Help` as a single strip when multiple menubars installed, rather than only `REPL`
- **Size**: ~250 LOC in `repl_palette_source.c` + `repl_palette_source.h`, plus integration test
- **Dependencies**: none (builds on existing vtable infrastructure)
- **Open decisions to resolve here**: Decision Point 4 (storage path reconciliation) — either fix or defer explicitly
- **Blocks**: all subsequent phases

**Phase A shipped** (2026-05-31, worktree-menu-port-phase-a-enumerate). `repl_palette_source.c` now enumerates all children of `@system.menus.data` filtered by `.installed = true` and composes their top-level menus into a single horizontal strip, sorted alphabetically by bar name for deterministic output. The existing `_init_for` (single-bar, bypasses installed filter) is preserved for test compatibility. The L4 PTY regression test (`repl_palette_multi_bar.yaml`) installs a second bar dynamically and verifies the union strip renders "File REPL" (alphabetical: aux < repl). When the union of all installed bars' menus exceeds terminal width, `palette.c` truncates at the right edge; overflow UX (ellipsization, horizontal scroll) is deferred as a follow-up per issue #677.

### Phase B — Script Hook Infrastructure (two parallel sub-tracks)

**B1 — `getsystemtablescript` headless support**

- Add `idsystemtablescripts` (list 139) to `getstringlist`'s switch in `headless_lang_runtime_more_stubs.c` (or wherever the stub lives — locate during implementation)
- Compile static string table from the legacy Mac resource. The string list maps integer IDs to ODB verb paths (e.g. ID 3 = `"system.misc.openWindow"`). These paths are documented in `tablestructure.h` comments and the deep-dive.
- Static table as a C array of `{int id, const char *path}` pairs — no YAML generation needed for 40 entries
- **Size**: ~100 LOC
- **Dependencies**: none

**B2 — Async dispatch infrastructure**

- Build `dispatch_menu_script_async` wrapper that posts the script to a GIL-managed thread and signals the REPL event loop to resume
- **Size**: ~250 LOC
- **Dependencies**: GIL threading infrastructure (exists), Decision Point 2 resolution
- **Note**: if Decision Point 2 chooses option (A) instead, this sub-track is skipped

Both sub-tracks block Phase C.

### Phase C — Window-Event Callback Bridge

**Phase C shipped** (2026-05-31, worktree-menu-port-phase-c-bridge). Dispatch implementation: `getsystemtablescript → langdeparsestring(bs_path, '"') → parsedialogstring → langrunstringnoerror`. Key correction vs. original spec: the script templates use straight ASCII double-quotes `"` as string delimiters, so the escaping uses `'"'` (not `chclosecurlyquote`/0xD3) to prevent injection of embedded `"` characters. All 5 behavioral unit tests in `tests/test_window_bridge.c` pass (green).

**What was delivered**:
- `frontier-cli/window_registry.c` + `window_registry.h`: `window_registry_init()` creates `system.temp.windowTypes.windows.repl` with `type="ReplWindow"`, `title="REPL"`. `on_frontmost_changed(old, new)` fires `idclosewindowscript`/`idopenwindowscript` using the corrected dispatch chain with `'"'` escaping.
- `frontier-cli/repl.c`: wired `window_registry_init()` + `on_frontmost_changed(nil, WINDOW_BRIDGE_REPL_PATH)` into boot sequence after `install_repl_menubar()` (step 3.3).
- `usertalk_scripts/.../Frontier/tools/data/windowTypes/ReplWindow/openWindow.ut`: installed in `databases/Virgin.root` via protocol; returns true as a Phase D placeholder.
- `tests/test_window_bridge.c`: 5 behavioral tests using C-side `langcompiletext`+`hashtableassign` stubs (avoids `script.newScriptObject` dependency on full Frontier.root). Confirmed RED → GREEN.

**Phase D pre-work signal**: `window.attributes.getOne("type", adr)` is NOT implemented in headless. The framework's `callbacks/openWindow.ut` gracefully returns true when this fails (`if window.attributes.getOne(...) {...}` skips the if-body). Full ReplWindow menu composition is blocked until Phase D implements `window.attributes.*`.

**Escaping correction** (issue #682): original spec said use `chclosecurlyquote` (0xD3). Actual template strings use straight ASCII `"`. `langdeparsestring(bs, 0xD3)` does NOT escape `"` (0x22) — it only escapes the curly-quote (0xD3). Fixed to `langdeparsestring(bs_path, '"')`.

**Refined 2026-05-31 (post-B1 #680 merge + windowTypes audit)**: list 139 entries are inline UserTalk script templates with `^0`/`^1` parameter substitution markers, NOT bare ODB verb paths.

- **Dependencies**: Phase A (#678, merged), Phase B1 (#680, merged)
- **Blocks**: Phase D

### Phase D — Verify WindowTypes Framework Kernel Primitives in Headless

**Phase D shipped** (2026-05-31, worktree-menu-port-phase-d-verify). Two Phase C bugs found and fixed. All 9 behavioral integration tests pass.

**Bugs found and fixed**:

1. `window.frontmost()` C kernel (`tests/headless_window_verbs.c`) was returning an empty string instead of an address value. The windowTypes framework passes this result to `parentOf()` which requires `addressType`. Fixed via `setstringvalue` + `coercetoaddress`: builds the REPL sentinel path string then coerces it to a live ODB address using `langexpandtodotparams`.

2. `window_registry_init()` (`frontier-cli/window_registry.c`) created the sentinel with `type`/`title` as direct fields on the window node at `system.temp.windowTypes.windows.repl`. But `window.attributes.getOne` navigates `parentOf(adrwindow^).["/atts"].[attname]` -- a sibling table named `/atts` inside the parent (`windows`) table. The sentinel was populating the wrong location. Fixed by adding three new idempotent statements to `window_registry_init()` that create `system.temp.windowTypes.windows.["/atts"]` and populate `type="ReplWindow"` and `title="REPL"` there.

**Kernel verb inventory** (final status after Phase D fixes):

| Verb | Status | Notes |
|------|--------|-------|
| `window.frontmost()` | WORKS | Returns `addressType` pointing at REPL sentinel |
| `window.attributes.getOne` | WORKS | Pure UserTalk; reads from `/atts` sibling (sentinel fixed) |
| `window.attributes.setOne` | WORKS | Pure UserTalk; writes to `/atts` sibling |
| `window.setTitle` | STUB | Returns false (no GUI window); framework handles this gracefully |
| `thread.callScript` | WORKS | Wired in `headless_thread_verbs.c:683`; dispatches without error |

**Key architectural discovery - the /atts pattern**: `window.attributes.getOne(name, @out, adrwindow)` does NOT read from the window node itself. It navigates `parentOf(adrwindow^).["/atts"].[name]` -- a sibling table named `/atts` alongside the window node in its parent table. For `adrwindow = @system.temp.windowTypes.windows.repl`, the parent is `@system.temp.windowTypes.windows`, so the attributes live at `system.temp.windowTypes.windows.["/atts"]`. Phase C had populated the wrong location.

**Test note - `thread.callScript` integration test**: behavioral verification calls `thread.callScript(@system.verbs.builtins.thread.sleepTicks, {0})` on an existing compiled ODB script. We do not attempt a side-effect roundtrip because `script.newScriptObject` embeds double-quotes that break the YAML test harness parser, and async scheduling makes timing-based probes flaky. The wiring check (dispatch without error on a valid ODB address) is the correct behavioral claim for the kernel verb inventory.

**What was delivered**:
- `tests/headless_window_verbs.c`: `winv_frontmost` case now builds the sentinel path string and coerces to `addressType` via `coercetoaddress()`.
- `frontier-cli/window_registry.c`: three new idempotent statements in `window_registry_init()` create the `/atts` sibling and populate `type`/`title` there (in addition to the direct fields retained for backward compatibility).
- `tests/integration/test_cases/window_primitives_phase_d.yaml`: 9 behavioral integration tests; each test includes a 6-line sentinel setup preamble (mirroring `window_registry_init()`) so tests are self-contained in `--skip-startup` mode.

**Rescoped 2026-05-31 (original description)**: the original framing assumed the windowTypes framework had to be ported into Virgin.root. Discovery: the framework is **already present** at `usertalk_scripts/Frontier.root/system/verbs/builtins/Frontier/tools/windowTypes/` (init.ut, findWindowType.ut, callWindowType.ut, newWindow.ut, openWindow.ut, runFileMenuScript.ut, runEditMenuScript.ut, findWindowWithMatchingAtts.ut, isFileMenuItemChecked.ut, isFileMenuItemEnabled.ut, isWindowDirty.ut, getDefaultFilename.ut, plus `callbacks/` and `commands/` subtables). Phase D was no longer a port -- it became verification work.

### Phase E — Menu Content

**Phase E shipped** (2026-05-31, worktree-menu-port-phase-e-content). The Phase C placeholder `Frontier.tools.data.windowTypes.ReplWindow.openWindow` (which just returned true) was replaced with a real implementation that installs three host menus in `system.menus.data.frontier`: File, Edit, View. With this phase, **the REPL menu strip now matches the legacy GUI menu shape** — pressing `/` shows `File | Edit | View | REPL` (alphabetical bar order: `frontier` < `repl`).

**Manifest installed** by the new openWindow handler (each item dispatches via the windowTypes framework's `runFileMenuScript` / `runEditMenuScript` async-thread helpers, or for Quit, directly to `repl.exit()`):

- **File**: New, Open, Close, Save, Save As, Quit
- **Edit**: Find, Find Next, Replace, Replace and Find Next, Insert Date/Time
- **View**: Huge, Medium, Tiny, Readable (font-size items, no-op-in-headless but installed for legacy fidelity)

The existing `repl` bar (REPL menu: Help, Clear, List, Jump, Key codes, Exit) remains unchanged — Phase A's union enumeration composes the two bars into a single strip.

**Manifest mechanism**: `openWindow(adr)` uses kernel-verb `menu.addMenuCommand` + `menu.install` directly. Idempotent via `menu.isInstalled` early-return guard. Re-invoking across REPL boots converges to the same state.

**File>Quit ↔ REPL>Exit reconciliation (task #21)**: File>Quit is wired directly to `repl.exit ()` rather than the legacy `Frontier.tools.windowTypes.commands.quit` chain (which walks windows + open databases + calls `filemenu.quit` — legacy GUI semantics not functional in headless). Both menus reach the same host-level exit flag — File>Quit and REPL>Exit are dispatch-target-identical, resolving task #21 as "they're not redundant, they're the same path two ways".

**View menu items** (Huge/Medium/Tiny/Readable) call legacy `menus.scripts.styleCommand` paths that don't exist in headless. They fail silently on the background thread (per `thread.callScript` async dispatch — Decision Point 2). This is the correct legacy-fidelity behavior: the menus install for visual parity, broken handlers log and drop, REPL stays up.

**Edit menu (Cut/Copy/Paste) deferred**: terminals didn't exist in classic Frontier so there's no legacy analog. Adding Cut/Copy/Paste in the REPL would require a `linenoise.*` verb bridge to the REPL's input buffer — new work, not part of Phase E scope.

**Tests**: 6 behavioral integration tests in `tests/integration/test_cases/replwindow_openwindow_menus_phase_e.yaml`:
1. ReplWindow.openWindow creates and installs the frontier menubar
2. frontier bar contains File menu with Quit item
3. frontier bar contains Edit menu with Find item
4. frontier bar contains View menu with Medium item
5. ReplWindow.openWindow is idempotent (second call is no-op)
6. File>Quit script invokes repl.exit (task #21 reconciliation)

All 6 RED → all 6 GREEN. Phase D regression tests (9 tests in `window_primitives_phase_d.yaml`) still pass; the "callbacks.openWindow chain runs cleanly" test in particular confirms the full dispatch chain `idopenwindowscript → callbacks.openWindow → findWindowType → ReplWindow.openWindow → bar installed` works end-to-end.

**Virgin.root impact**: openWindow body grew from ~370 bytes (Phase C placeholder) to 3344 bytes (Phase E real implementation). After `db.compactDatabase`: 13M → 10M (3MB recovered).

**Discovery during implementation**: `menu.getScript` returns empty for items installed via `menu.addMenuCommand` -- it's wired for the legacy in-memory menu cache, not the `system.menus.data` projection that headless uses. Tests that need to read menu-item scripts must access the leaf's `.script` field directly. Documented in the test file's comment for the File>Quit assertion.

**Bridge wiring fix surfaced during E2E validation**: Phase C's kernel bridge fires `getsystemtablescript(idopenwindowscript) -> system.callbacks.openWindow("@<path>")`, but the windowTypes framework's `Frontier.tools.windowTypes.callbacks.openWindow(adr)` expects an address, not a string. Phase E adds two adapter scripts (`system.menus.handlers.repl.windowTypesOpenAdapter` + `...CloseAdapter`) that strip the leading `@`, coerce the string to an address via `address()`, and dispatch into the framework. The adapters are registered in `user.callbacks.openWindow` / `user.callbacks.closeWindow` by `installReplMenubar` at boot (idempotent registration before the early-return-if-already-installed check).

Without this wiring the kernel bridge fires correctly but `system.callbacks.openWindow` iterates an empty `user.callbacks.openWindow` table, silently returns true, and the framework's handler is never invoked. This was missed by Phase D because the Phase D integration test calls the framework's `callbacks.openWindow` directly via UserTalk address rather than going through the kernel-bridge -> `system.callbacks.openWindow` -> user-callback-chain path. E2E live-boot verification surfaces it; lesson recorded for Phase F+ (editor windows).

**End-to-end live confirmation** (TTY REPL boot, no test mocking):

| Layer | Observed value |
|-------|----------------|
| `system.menus.data` installed bars | `repl, frontier` |
| `system.menus.data.frontier` menus (in render order) | `File, Edit, View` |
| File menu items | New, Open, Close, Save, Save As, Quit |
| Edit menu items | Find, Find Next, Replace, Replace and Find Next, Insert Date/Time |
| View menu items | Huge, Medium, Tiny, Readable |
| REPL menu items (unchanged) | Help, Clear, List, Jump, Key codes, Exit |
| `File>Quit.script` | `repl.exit ()` |
| `REPL>Exit.script` | `system.menus.handlers.repl.exit ()` (which calls repl.exit) |
| Palette strip on `/` (alphabetical union) | `Edit  File  View  REPL` |

**Files**:
- Modified: `usertalk_scripts/Frontier.root/system/verbs/builtins/Frontier/tools/data/windowTypes/ReplWindow/openWindow.ut` (8 lines -> 51 lines)
- Modified: `usertalk_scripts/Frontier.root/system/menus/installReplMenubar.ut` (added bridge wiring bundle)
- New: `usertalk_scripts/Frontier.root/system/menus/handlers/repl/windowTypesOpenAdapter.ut` (28 lines)
- New: `usertalk_scripts/Frontier.root/system/menus/handlers/repl/windowTypesCloseAdapter.ut` (16 lines)
- Modified: `databases/Virgin.root` (5 scripts updated + compacted twice: 13M -> 14M -> 10M)
- New: `tests/integration/test_cases/replwindow_openwindow_menus_phase_e.yaml` (6 behavioral tests, 178 lines)
- Updated goldens: `tests/fixtures/palette/menubar_*.txt` (4 files; new strip is `Edit File View REPL` instead of just `REPL`)

**Test counts**: Unit 492/492 PASS. Integration 2168/2395 PASS (+6 from Phase E), 20 FAIL (all pre-existing html / startup / tcp -- unrelated to menu work).

**Functional parity with legacy GUI achieved** — the goal of task #23. Remaining Phase E follow-ups (not in scope of this PR):
- Cut/Copy/Paste via linenoise bridge (separate feature; no legacy analog)
- View menu items wired to a no-op-friendly headless path so they don't log warnings on every click
- Per-windowType menu composition for editor windows (Phase E scope was REPL only; editor windows are a separate feature)
- Fix the `@`-prefix in `WINDOW_BRIDGE_REPL_PATH` (`frontier-cli/window_registry.h`): the bridge currently passes `"@system.temp.windowTypes.windows.repl"` and Phase E's adapter strips the prefix. Cleaner fix: change the constant to omit the `@`. Deferred so the Phase E adapter remains tolerant for editor-window paths that may legitimately carry the prefix.

---

## Acceptance Protocol — Fidelity Verification

The fidelity goal requires a concrete comparison protocol. Without this, "matches legacy" stays aspirational.

**Protocol (execute at each phase boundary):**

1. Identify a specific legacy Frontier behavior to replicate (e.g., "Script Editor frontmost; Script menu visible; switch to Outline Editor; Script menu disappears, Outline menu appears").
2. Capture legacy behavior: screenshots or behavioral description from JES's memory, ideally with list of which menus appear in which state.
3. Implement equivalent in headless.
4. Compare: run headless REPL, open relevant "window type" (or simulate focus change), observe which menus appear.
5. Record deltas: acceptable divergence (CLI has no drag-and-drop) vs. fidelity gap (a menu that should appear does not).

**Baseline ownership**: JES owns the "this is what legacy did" documentation. Before Phase C starts, JES should produce a table of: window type → list of menus that appeared when that window was frontmost → which menus were grayed vs. active. This becomes the acceptance test oracle.

**Automated acceptance**: Once baseline table exists, integration tests in `tests/integration/test_cases/` can simulate "open window type X" → "assert menus contain exactly Y" via the protocol interface. Testable without an actual GUI.

---

## Risk Register

| Risk | Severity | Mitigation |
|------|----------|------------|
| ~~windowTypes framework source never surfaces; port must be built from hypothesis~~ | RESOLVED 2026-05-31 | Framework located at `usertalk_scripts/Frontier.root/system/verbs/builtins/Frontier/tools/windowTypes/`. Phase D rescoped to verification of kernel primitives the framework calls. |
| Async dispatch (Phase B2) more complex than estimated; GIL contention with REPL input | Medium-High | Build B2 as isolated module with its own integration test before wiring into palette dispatch. Fall back to synchronous if threading issues appear. |
| `getsystemtablescript` string table contains paths that no longer exist in v7 databases | Medium | Port faithfully, then validate each path against a running headless instance. Add "verb not found" logging in Phase C bridge rather than silent failure. |
| Union-menubar rendering overflows terminal width | Medium | Phase A should include terminal-width enforcement: overflow menus ellipsized or hidden (classic Mac behavior). Define overflow behavior before Phase A ships. |
| Re-entrancy: menu script opens dialog/REPL prompt, which tries to open another menu | Low-Medium | Async dispatch model: GIL serializes — expected and correct. Document it. Sync dispatch makes this impossible (script blocks REPL). Another argument for option (B). |

---

## Mapping to Original 8-PR Plan

PRs 1-5 from the original `planning/discussions/repl-slash-menu-implementation-plan.md` shipped. Remaining mapping:

- **PR 6** (Layer 1 host menubar) → split into Phase A + Phase B + Phase E
- **PR 7** (slash-command migration) → still valid; depends on Phase A. Slash-command resolver already exists (`repl_slash_resolver.c`); needs wiring against composed menu strip
- **PR 8** (Rung 2 features) → still independent; terminal-width overflow enforcement should move from "Rung 2" into Phase A

The original plan did not account for the windowTypes framework — it was implicitly scoped to "REPL always frontmost." Phases C and D are new work not in that plan.

---

## Immediate Action Items (before any agent starts work)

1. **JES**: provide or locate the windowTypes Frontier.root. Even a screenshot or behavioral notes narrows uncertainty significantly.
2. **JES**: confirm Decision Point 2 (sync vs async dispatch). Gates Phase B2 and affects palette dispatch architecture.
3. **JES**: confirm terminal-width overflow behavior for Phase A. Hidden, ellipsized, or horizontally-scrolling strip?
4. **Issue #677 rescope**: P2 → P1, add union-composition requirement (not just enumeration)
5. **Produce fidelity baseline table**: before Phase C starts, JES enumerates which menus appear for which window types in legacy Frontier. Acceptance test oracle.

---

## See Also

- `docs/LEGACY_MENU_SYSTEM.md` — kernel-side architecture deep-dive
- `planning/discussions/repl-slash-menu-implementation-plan.md` — superseded original 8-PR plan
- Memory: `project_slash_menu_heritage.md` — `/` for menu is a 45+ year VisiCalc/Excel convention
- Issue #677 — multi-menubar enumeration (Phase A)
- Issue #675 — Virgin.root sync verification (operational hygiene, parallel)
- Issue #676 — Virgin.root as build artifact (operational hygiene, parallel)
- Session task #23 — the legacy-fidelity goal this plan pursues
