# PR 2 Design Plan — `meuserselected_headless` + `menu.list` / `menu.describe`

**Branch**: `worktree-slash-menu-pr2-meuserselected-headless`
**Predecessor**: PR 1 (`b6b90b365` — headless menu storage; 9 P0 verbs + cmdkey pair)
**Predecessor doc**: `planning/discussions/repl-palette-test-harness.md` (Plan 1)
**Follows ADR**: `planning/architectural_decision_records/ADR-016-headless-menu-system-projection.md`

---

## 0. Headline architectural decisions

1. **Land `menu.list` and `menu.describe` first** (sub-PR 2a). Pure storage reads against `system.menus.data.*`, share PR 1's `menudata_ensure_root()` plumbing, unblock the palette enumeration spec (PR 5) without depending on the `newprocess`/`addprocess` lifecycle. **`meuserselected_headless` is sub-PR 2b** — touches the runtime, can be reasoned about independently, only piece with GIL semantics worth thinking about.
2. **`meuserselected_headless` accepts the script *handle* directly, not an `hdlheadrecord`.** The palette already fetched the leaf's `script` field. Decoupling from outlines is the whole point of the sibling — it doesn't need `op_get_outlinedata`, `processkilledroutine = &meprocesscallback`, or `shellforcemenuadjust`. Those are Mac-UI scaffolding the palette will replace with its own dispatch hook in PR 5.
3. **Stay inside the existing dual-dispatcher pattern**. PR 1 left `Common/source/menuverbs.c::menufunctionvalue` (full kernel dispatcher) and `tests/headless_menu_verbs.c::menu_valueproc` (CLI runtime stub) as the two dispatch heads. PR 2 keeps that split: adds `case menulistfunc / menudescribefunc` to *both*, and `meuserselected_headless` to `Common/source/meprograms.c`. No re-architecture; PR 1's GIL invariants carry forward unchanged.

---

## 1. What changes, in one paragraph

In `Common/source/meprograms.c` add a new sibling `meuserselected_headless(Handle hScript)` that runs `langpusherrorcallback` → `scriptbuildtree` → `langpoperrorcallback` → `newprocess(…, true, mescripterrorroutine, 0L, …)` → `addprocess`, with no `op_get_outlinedata` / `shellforcemenuadjust` / `meprocesscallback` calls and no use of the global `menudata`. In `Common/source/menuverbs.c` extend `tymenutoken` with `menulistfunc` and `menudescribefunc`; add their handlers as **first-switch verbs** so they bypass the `headless_resolve_menu_target` dance. `menu.list(adrApp)` walks `system.menus.data` (or one app's subtree) via `hashtablevisit` and returns a **list of address-values** to leaf items. `menu.describe(adrItem)` reads the leaf record's 9 fields and returns a **record value** with defaults for absent fields. Mirror the new verbs in `tests/headless_menu_verbs.c::menu_valueproc` (the CLI runtime path) and add UserTalk reference scripts under `usertalk_scripts/Frontier.root/system/verbs/builtins/menu/list.ut` + `describe.ut`.

---

## 2. Order of work — risk-staged sub-PRs

### Sub-PR 2a — `menu.list` + `menu.describe` (storage reads only)

Lower blast radius. Nothing here can crash a process or wedge a worker. Malformed data → verb returns false with `bserror` set; same failure mode as the existing PR 1 verbs.

1. Add tokens `menulistfunc`, `menudescribefunc` to `tymenutoken` (`menuverbs.c:75-106`).
2. Add cases to the **first switch** in `menufunctionvalue` (`menuverbs.c:2058-2151`) so they don't go through `headless_resolve_menu_target` (which requires `target.set` of a menu external — neither verb wants that).
3. Implement `menulistverb(hparam1, v)` and `menudescribeverb(hparam1, v)` as static helpers in `menuverbs.c`. List walks `system.menus.data` — re-find the chain via `findnamedtable` from `roottable` (PR 1's `menudata_ensure_root` already ensures it exists; we only read here). For each leaf encountered, format the address and append to a `tylistrecord`.
4. Mirror in `tests/headless_menu_verbs.c::menu_valueproc`. CLI runtime path needs the same case statements with the same UserTalk-visible behavior.
5. Add UserTalk wrapper scripts at `usertalk_scripts/Frontier.root/system/verbs/builtins/menu/list.ut` and `describe.ut` matching the one-liner pattern in existing builtins.
6. Add unit tests to `tests/menudata_headless_tests.c` (or new sibling `tests/menudata_list_describe_tests.c` if file is getting long): empty data tree, single app/menu/item, multiple apps, missing optional fields → defaults.
7. Add integration tests to `tests/integration/test_cases/menu_data_verbs.yaml` exercising round-trip `addMenuCommand → list → describe → setScript → describe`.

**Acceptance**: PR 2a green when `menu.list(nil)` returns a non-empty list after `menu.addMenuCommand`, and `menu.describe` round-trips every field added/set by the existing PR 1 verbs.

### Sub-PR 2b — `meuserselected_headless`

Touches runtime lifecycle. Keep small, ship after 2a is green.

1. Add prototype to `Common/headers/meprograms.h` (sibling to `meuserselected` line 40).
2. Implement in `Common/source/meprograms.c` immediately after `meuserselected` (~line 316). Body sketch — no Mac-UI calls:
   - Validate `hScript != nil`. Compute `signature` (script's source signature; for headless we pass `'land'` if not present).
   - `langpusherrorcallback(&mescripterrorroutine, 0L)`.
   - `fl = scriptbuildtree(hScript, signature, &hcode)`.
   - `langpoperrorcallback()`.
   - On parse failure: return false (caller surfaces via the error callback).
   - `langerrorclear()`.
   - `newprocess(hcode, true /*floneshot*/, &mescripterrorroutine, 0L /*errorrefcon*/, &hprocess)`.
   - **No** `(**hp).processkilledroutine = &meprocesscallback`. Default `truenoop` is fine.
   - **No** `shellforcemenuadjust` call.
   - `addprocess(hp)` — schedules onto one-shot list, calls `shellforcebackgroundtask`. Returns true.
3. Update CLI-side stub `frontier-cli/stubs/headless_menu_stubs.c` line 327 (`meuserselected` returns false). Decision: link the real `meuserselected_headless` into the CLI directly (it's already compiled in `Common/source/meprograms.c`, just unused).
4. Tests: see §3.

**Acceptance**: PR 2b green when a script handle pulled from `system.menus.data.<app>.<menu>.<item>.script` runs to completion under `meuserselected_headless` *without a target window*, and the value it produces is observable (e.g., a `system.temp.x = 1` mutation persists).

---

## 3. Test plan

### 3.1 Integration tests (`tests/integration/test_cases/menu_data_verbs.yaml`)

Add a new section "menu.list / menu.describe / meuserselected_headless" at the bottom. Cases:

1. **list-empty**: brand-new root + `menu.list(nil)` → list with 0 elements.
2. **list-after-add**: `menu.addMenuCommand(...)` → `menu.list(nil)` → list contains the address of the new leaf.
3. **list-scoped**: install items under two apps; `menu.list(@system.menus.data.AppA)` returns only AppA's leaves.
4. **describe-roundtrip**: `addMenuCommand → describe.script == "the script"`.
5. **describe-defaults**: leaf with only `label` + `script` → `describe.enabled == true`, `describe.hidden == false`, `describe.shortcut == ""`, `describe.accepts_args == false`.
6. **describe-with-cmdkey**: after `menu.setCommandKey('Q')`, `describe.cmdkey == 'Q'`.
7. **dispatch-roundtrip** *(2b)*: install a menu item whose script is `system.temp.dispatched = 42`, fetch its script handle, invoke a new test-only verb that calls `meuserselected_headless`, then poll `system.temp.dispatched == 42` after a short yield.

### 3.2 Unit tests

Existing file: `tests/menudata_headless_tests.c` covers `menudata_ensure_root`. Add a sibling test executable or a section:

- `test_menulist_empty` — fresh root, `menulistverb` returns empty list.
- `test_menulist_one_leaf` — manually populate `system.menus.data.App.Menu.Item`, list returns one address.
- `test_menulist_filter_by_app` — populate two apps, scoped list returns only the requested subtree.
- `test_menudescribe_all_fields_present` — all leaf fields populated, every field is correct.
- `test_menudescribe_defaults_on_missing` — only `script` present, defaults reported.
- `test_menudescribe_invalid_address` — non-existent address → returns false, sets `bserror`.

For `meuserselected_headless`: drive exclusively from integration tests where the runtime is real. A unit test that mocks `newprocess` would not exercise anything real.

---

## 4. GIL concerns — what changes vs PR 1

PR 2 changes none of PR 1's GIL invariants:

- `menu.list` and `menu.describe` go through the **first switch** (alongside `buildmenubarfunc`, `clearmenubarfunc`, `installfunc`) — they never call `mepushmenudata`. No `flpushed`. No save-slot interaction.
- `meuserselected_headless` runs entirely under the caller's GIL: `palette button press → palette.c calls meuserselected_headless → newprocess → addprocess → return; later the scheduler picks up the one-shot process inside a normal processtimeslice.` This is the chain `meuserselected` already used; we're just removing two Mac-UI calls. `addprocess` itself calls `_entercriticalprocesssection` which is a **no-op macro on the headless path** (`process.c:88`) — thread safety is GIL-based.
- **Output contract**: scripts dispatched by `addprocess` run on the one-shot scheduler when `processruncode` next ticks. In headless CLI mode, `dialog.alert` / `msg` reach the dialog sink in `tests/headless_dialog_verbs.c`. For PR 2 acceptance, we test by side-effect on `system.temp.*` (an ODB write, not a UI write).

**Conclusion**: no new locking primitives, no new invariants. PR 2 can adopt PR 1's GIL comment block verbatim.

---

## 5. Risk register

| # | Risk | Likelihood | Impact | Mitigation |
|---|------|------------|--------|------------|
| 1 | `meuserselected_headless` script accesses `op_get_outlinedata()` indirectly. | Med | Med | Document explicitly that the dispatch path is for menu scripts that don't assume a frontmost outline window. Layer 1 / Layer 2 menu scripts are written this way. |
| 2 | One-shot process schedule races the next REPL prompt. | Med | Low | Pure cosmetic — scrollback pane in PR 5 fixes it. Use `system.temp.*` side-effects for PR 2 acceptance. |
| 3 | `mescripterrorroutine` with `errorrefcon == 0L` deref's. | Low | High | Read its body and verify the 0-case. If it deref's, pass `(long)hScript` instead. Add a unit test that compiles a syntactically-bad script. |
| 4 | `menu.describe` field defaults disagree with palette's renderer. | Med | Low | Define defaults in one place: a static table in the dispatcher, also documented in the UserTalk wrapper script comments and ADR-016. |
| 5 | `menu.list` returns address strings whose components contain dots/backticks. | Low | Med | Use the existing `langgetdisplaystring` / `langaddrtoexternal` formatter. Don't hand-roll dot-joining. |
| 6 | Integration test contamination — same shared-staged-DB issue that already skipped 16 PR 1 tests. | Med | Med | Make every new yaml test stand-alone: create its own `system.temp.testMenu`, exercise, `delete(@system.temp.testMenu)`, no carryover. |
| 7 | Stub-table parity drift between `tests/headless_menu_verbs.c` and the kernel dispatcher. | Med | Low | Add a comment block at the top of the stub listing every kernel verb token. PR 2 adds `menv_list` and `menv_describe` to both enums. |

---

## 6. Acceptance criteria

PR 2 is mergeable when **all** of:

1. The 7 new yaml integration tests pass under `cd tests && make test-integration`.
2. The 6 new C unit tests pass under `./tools/run_headless_tests.sh`.
3. `menu.list(nil)` from a fresh REPL returns an empty list. After one `menu.addMenuCommand`, returns a 1-element list whose member is `@system.menus.data.<app>.<menu>.<item>`.
4. `menu.describe(@<addr>)` returns a record with all 9 fields; absent fields return documented defaults; unknown address returns false with `bserror` populated.
5. A test that dispatches a known script via `meuserselected_headless` and asserts an ODB side-effect passes.
6. The 16 PR 1 still-skipped tests are still skipped (no regression). The 5 PR 1 green-passing test sections still green.
7. The CLI builds cleanly (`make -C frontier-cli`).
8. UserTalk wrappers `list.ut` and `describe.ut` exist and are exercised by at least one yaml test.

---

## 7. Critical files

### Read (no changes)

- `planning/discussions/repl-slash-menu-implementation-plan.md` — sections 1.2, 1.5, 4.
- `planning/architectural_decision_records/ADR-016-headless-menu-system-projection.md` — sections "Decision / What this means for verbs / What this means for `meuserselected`".
- `planning/discussions/repl-palette-test-harness.md` — §"Layer 4 integration".
- `Common/source/meprograms.c` lines 256-315 — reference body for `meuserselected`.
- `Common/source/process.c` lines 304-424 (`newprocess`), 2783-2846 (`addprocess`).
- `Common/source/menudata_headless.c` — PR 1's storage helper.
- `Common/source/scripts.c` lines 540-770 — `hashtablevisit`-driven walk pattern.

### Modified

- `Common/source/menuverbs.c` — add `menulistfunc` / `menudescribefunc` to `tymenutoken` enum; add their case statements to the first switch; add static helpers `menulistverb` / `menudescribeverb`.
- `Common/source/meprograms.c` — add `meuserselected_headless` definition immediately after `meuserselected`.
- `Common/headers/meprograms.h` — add `extern boolean meuserselected_headless (Handle);`.
- `tests/headless_menu_verbs.c` — add `menv_list` / `menv_describe` enum entries; add their `case` blocks.
- `tests/integration/test_cases/menu_data_verbs.yaml` — append the 7 new cases.

### Created

- `tests/menudata_list_describe_tests.c` (or extend `menudata_headless_tests.c`).
- `usertalk_scripts/Frontier.root/system/verbs/builtins/menu/list.ut`.
- `usertalk_scripts/Frontier.root/system/verbs/builtins/menu/describe.ut`.

---

## 8. Open questions

1. **Should `menu.list(nil)` enumerate all five layers, or only Layer 2?** ADR-016 §"What this means for the REPL" describes the palette doing layer composition at render time. **Recommendation**: PR 2's `menu.list` enumerates everything under `system.menus.data.*` — Layer 1 (host-anchored) and Layer 2 (apps/Tools) both live there per ADR-016. The palette does layer composition, not the verb. Confirm before PR 5.
2. **Canonical address-string format `menu.list` returns**: address-values vs strings. **Recommendation**: return `addressvaluetype` values so caller can pass them straight to `menu.describe(@addr)` without re-parsing.
3. **Does `meuserselected_headless` need to honor `processkilledroutine`?** `meuserselected` sets it to `meprocesscallback` which calls `shellforcemenuadjust` — pure Mac UI. PR 2 ships with no callback; PR 5 may want to add one for palette refresh. **Decision**: defer to PR 5.
4. **`accepts_args` semantics in v1**: returning the boolean is well-defined; the wire format for passing args to the script is not. **Decision needed before PR 5; PR 2 ships `accepts_args` as a passthrough boolean and worries about wire format later.**
5. **Integration runner choice**: protocol/socket runner (deterministic for ODB asserts) vs pexpect-style (more end-to-end). **Recommend**: protocol runner — PR 2 tests kernel semantics, not REPL UX.

---

## 9. Surprises in the existing code

- **`meuserselected` line 279 reads `menudata` from `op_get_outlinedata()` *before* checking `megetnodelangtext`** — a hidden global mutation. The CLI's stub at `frontier-cli/stubs/headless_menu_stubs.c:327` short-circuits this. `meuserselected_headless` MUST NOT touch `menudata`; the script handle is self-contained.
- **`addprocess` calls `_entercriticalprocesssection()` which is a no-op macro on the headless path**. All process-table mutation under headless relies on the GIL.
- **The CLI runtime stub dispatcher (`tests/headless_menu_verbs.c`) is a separate dispatcher** registered via `newfunctionprocessor` — not the same `loadfunctionprocessor(idmenuverbs, &menufunctionvalue)` the kernel dispatcher uses. The two dispatchers don't share their token enum. PR 2 must add the new verbs to both enums independently.
- **PR 1's `headless_resolve_menu_target` requires an externally-set `target.set(@menu)`** to operate. `menu.list` and `menu.describe` deliberately do NOT need it — they take an explicit address parameter. This is why they belong in the first switch with `installfunc`, not the second switch with `getscriptfunc`.
- **`langipcmenus.c` and `osamenus.c` exist but are not called in the headless build** per ADR-016 Resolved Q7 (Suites/IPC deferred). Cleaner to use the `hashtablevisit` pattern from `scripts.c` directly.
