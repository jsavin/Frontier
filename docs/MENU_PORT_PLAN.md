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

### Decision Point 1 — Obtain windowTypes Frontier.root (BLOCKS PHASE D)

The windowTypes framework JES wrote is the entire window-type-to-menubar-manifest mapping. Without the actual source we don't know the exact table path (`system.windowTypes` is the working hypothesis), what a "window type" looks like as stored in a window record, or what fields the framework inspects on the window record vs. on a table.

**Options:**
- **(i) Get the actual .root from JES.** Export the relevant `system.windowTypes` subtable and its handlers as `.ut` files. Port verbatim. The only way to guarantee fidelity.
- (ii) Recreate from intent + kernel evidence. Equivalent to writing a compatible implementation, not porting the original.
- (iii) Fresh design with same conceptual goals.

**Recommendation: option (i) is required.** Fidelity goal makes (i) the only defensible path. (ii)/(iii) are acceptable only for scaffolding that JES will review and correct against the real source when it surfaces.

**Action required from JES**: export `system.windowTypes` from a running legacy Frontier instance, or provide the `.root` file. Even a screenshot or behavioral notes ("when Script Editor was frontmost, these menus were visible") narrows uncertainty.

### Decision Point 2 — Sync vs Async Menu Script Dispatch (BLOCKS PHASE B2)

Legacy `meuserselected` used `addprocess` to queue scripts as runtime processes; the menubar stayed live while the script ran. Headless `meuserselected_headless` runs `langruncode` synchronously — the REPL is unresponsive until the script returns.

**Options:**
- (A) Accept synchronous dispatch for MVP. Document the deviation; long-running scripts must explicitly yield.
- **(B) Spawn menu scripts on a GIL-managed background thread.** Matches legacy. Headless threading infrastructure already supports it (`headless_thread_verbs.c`, the GIL mutex). ~250 LOC at the spawn site + small REPL event-loop change.
- (C) Restructure REPL event loop to re-enter palette input during dispatch. Architecturally complex and fragile.

**Recommendation: option (B).** Matches legacy most closely. The GIL infrastructure already supports it. Not optional if fidelity is the goal — if JES wants to defer async dispatch and accept synchronous for now, it should be an explicit decision, not a default.

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

**Step 4 (window-type registry / windowTypes framework):** Blocked on Decision Point 1. Cannot proceed faithfully without the source. Scaffolding tables can be created early; handler scripts cannot.

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

- Implement `on_frontmost_changed(old, new)` in `repl.c`: fires `getsystemtablescript(idclosewindowscript)` for old, `getsystemtablescript(idopenwindowscript)` for new, resolves each to an ODB path, calls via `meuserselected_headless` (or async dispatch if Phase B2 shipped)
- Implement the static "REPL window" sentinel and wire as initial frontmost at boot
- At boot: fire `on_frontmost_changed(nil, repl_window)` to trigger `idopenwindowscript` — how the REPL's menus get installed in the windowTypes model
- **Size**: ~200 LOC in `repl.c` + a small new `window_registry.c`
- **Dependencies**: Phase A (multi-menubar enumeration to see effects), Phase B1 (getsystemtablescript)
- **Blocks**: Phase D

### Phase D — WindowTypes Framework Port

- **BLOCKED ON Decision Point 1** (JES providing the actual Frontier.root)
- Once source available: port `system.windowTypes` table structure and handler scripts verbatim into `Virgin.root` via the ODB script editing protocol
- `idopenwindowscript` and `idclosewindowscript` handlers installed by the framework will be triggered by Phase C's bridge
- Install a `"ReplWindow"` entry immediately to validate the model
- **Size**: unknown until source available (100-500 lines of UserTalk likely)
- **Acceptance**: execute fidelity verification protocol (see below)

### Phase E — Menu Content

- Port the legacy File / Edit / View / Window / Help menus with REPL-appropriate behavior
- Specific scripts for each item must be reviewed against what legacy Frontier ran for that item
- **Edit menu (Cut/Copy/Paste)** requires the `linenoise.*` verb bridge to the REPL's input buffer — new work with no legacy analog (terminals didn't exist in classic Frontier)
- **Dependencies**: Phase D (windowTypes framework, so menus appear in the right contexts)
- This is where the original plan's "Phase 6 detailed plan" (task #20) executes

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
| windowTypes framework source never surfaces; port must be built from hypothesis | High | Escalate as prerequisite, not nice-to-have. Block Phase D explicitly until resolved. |
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
