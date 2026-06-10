# REPL Schism Resolution -- Execution Plan

**Date**: 2026-06-10
**Status**: ACTIVE
**Approves**: `REPL_SCHISM_RESOLUTION.md` Path A
**Scope**: C.0.1 -> C.0.6 (boxen REPL parity) -> C.6 (default flip)

This plan operationalizes the schism-resolution proposal. Each milestone below is one PR, sized for autonomous /auto execution with /gate review. Order is sequential -- each milestone builds on the previous.

The plan deliberately does NOT pause for further design decisions inside milestones. If an open question surfaces during implementation, the agent surfaces it and asks; otherwise the plan is authoritative.

---

## 0. Cross-cutting context (read once)

### 0.1 Files agents will touch repeatedly

| File | Role | Touched in milestones |
|---|---|---|
| `frontier-cli/boxen_repl.c` | Boxen REPL implementation, ~1000 LOC | C.0.1, C.0.2, C.0.3, C.0.4, C.0.5, C.6 |
| `frontier-cli/boxen_repl.h` | Public API | C.0.1, C.0.5 |
| `frontier-cli/boxen_repl_internal.h` | State struct, hook seams | C.0.1, C.0.2, C.0.3, C.0.4, C.0.5 |
| `tests/boxen_repl_tests.c` | Behavioral unit tests | every milestone |
| `frontier-cli/main.c` | Mode dispatch | C.6 |
| `frontier-cli/cli_parser.c` | Flag parsing | C.6 |

### 0.2 Files agents will reuse (do NOT modify unless noted)

| File | What it provides | Used by |
|---|---|---|
| `frontier-cli/repl.c::dispatch_slash_command` | Slash dispatch, already exposed via `repl_slash_dispatch.h` (PR #758) | C.0.x all |
| `frontier-cli/repl.c::complete_slash_command_path` | Slash-command + ODB-path completion logic | C.0.2 |
| `frontier-cli/repl.c::repl_resolve_path_ex` | Path resolution against current REPL context | C.0.2 |
| `frontier-cli/palette.h::palette_*` | Palette state machine + render | C.0.3 |
| `frontier-cli/palette_arg_inject.c` | Arg-injection synthesis for slash dispatch | C.0.3 |
| `frontier-cli/repl_palette_source.c` | Slash-menu data source | C.0.3 |
| `frontier-cli/linenoise/*` | Reference for history file format only | C.0.1 |

### 0.3 Sub-agent dispatch discipline (from Phase B/C retrospective)

Every dispatched agent prompt MUST include:

1. **WORKING DIRECTORY** and **BRANCH** in the preamble (working dir doesn't inherit)
2. **Tests-first ordering**: tests come BEFORE implementation steps. RED -> verify reason -> GREEN
3. **Independent verification reminder**: after the agent claims a change, run `git diff` and grep to confirm it actually landed (recurring failure mode on this project)
4. **Wire-format verification rule** (`memory/feedback_wire_format_verification.md`) for anything that crosses a JSON or NDJSON boundary
5. **Project conventions reminder**: tabs, K&R, ASCII-only in comments, date-prefixed `/* YYYY-MM-DD JES #691 Phase C.0.x: ... */` headers
6. **Standing autonomy** if applicable: merge if /gate passes AND integration baseline character-for-character holds

### 0.4 /gate expectations per milestone

For each milestone:
- bar-raiser: ALWAYS
- security: ALWAYS (project default)
- concurrency: AUTO -- will trigger when patterns like `pthread_`, `GIL`, or `frontier-cli/**` appear in changed lines
- swiftui: excluded (C codebase)

Expect 1-2 fix-loop rounds per milestone based on Phase B/C experience with similar-sized PRs.

### 0.5 Integration baseline

`/tmp/integ-failures-bf51d18.txt` lists the 21 documented known-baseline integration failures (html / tcp / startup categories). Every milestone's integration test run must match this baseline character-for-character. New failures or fixed-but-not-yet-documented failures BLOCK merge.

### 0.6 Standing autonomy

For each milestone, /auto is pre-authorized to merge if:
1. /gate verdict is PASS or PASS WITH NOTES
2. Unit tests show baseline + N new tests (where N matches the plan), all passing
3. Integration shows 2186/21 baseline character-for-character (`diff /tmp/integ-failures-bf51d18.txt <current>` returns empty)
4. macOS CI green
5. No CHANGES_REQUESTED reviewer comment

Any deviation -> STOP and ask.

### 0.7 Scope creep guardrail

Each milestone has an explicit "out of scope" list. Agents are forbidden from adding features beyond it. If a tempting refactor surfaces ("while I'm here, let me also..."), it gets filed as a follow-up issue and deferred. The schism resolution depends on shippable increments, not big-bang refactors.

---

## 1. Milestone C.0.1 -- Persistent history with linenoise file format

### 1.1 Goal

Up/down arrow walks command history in the boxen REPL. History persists across sessions in `~/.frontier_history`, using the same file format linenoise uses, so users can switch between REPLs without losing history.

### 1.2 Files

| Action | File | Estimated LOC |
|---|---|---|
| Modify | `frontier-cli/boxen_repl.c` | +250 |
| Modify | `frontier-cli/boxen_repl_internal.h` | +25 (history ring + fields) |
| Modify | `frontier-cli/boxen_repl.h` | 0 (no public API change) |
| Modify | `tests/boxen_repl_tests.c` | +120 (2 new tests) |
| **Total** | | ~395 |

### 1.3 Implementation outline

**State additions on `boxen_repl_state_t`:**
- `char history[HISTORY_SIZE][BOXEN_REPL_INPUT_MAX]` -- ring of past commands
- `int history_count` -- valid entries
- `int history_head` -- next write index
- `int history_nav_idx` -- -1 = at fresh input; otherwise index into history being displayed
- `char history_saved_input[BOXEN_REPL_INPUT_MAX]` -- saved current input when nav started

**Behaviors:**
- On `boxen_repl_main` entry: read `~/.frontier_history` (one command per line; same format as linenoise). Populate `history[]`. Log error if unreadable but continue.
- After successful `submit_input` for a non-empty line: append to history ring (deduplicate if exactly equal to most recent entry). Set `history_nav_idx = -1`.
- New keybinds in `on_input`:
  - `BOXEN_KEY_UP`: if at -1, save current input to `history_saved_input` and load latest. Otherwise step back one entry. Replace `input_buf` and `input_cursor`.
  - `BOXEN_KEY_DOWN`: step forward; if past most-recent, restore `history_saved_input` and set nav_idx = -1.
- On clean exit (after the event loop ends, before state teardown): write merged history to `~/.frontier_history`. Use the same merge-with-existing-and-dedup logic as `repl.c::1782+` (read the file, merge with session history, deduplicate, write back, trim to HISTORY_SIZE).
- Reuse `HISTORY_FILE` (".frontier_history") and `HISTORY_SIZE` (1000) constants from `repl.c:85-86`. Re-declare in `boxen_repl.c` with a comment noting they MUST stay in sync with `repl.c` until linenoise REPL is removed (C.6).

**Out of scope for C.0.1:**
- Ctrl-R inverse search (DEFER to its own follow-up; not blocking)
- History deduplication beyond "skip if equal to most recent" (legacy REPL also doesn't do more than this)
- Multi-line history entries (DEFER to C.0.5)
- Reading legacy linenoise's history file from a non-default path (DEFER; HOME env is sufficient)

### 1.4 Tests

In `tests/boxen_repl_tests.c`, add hook seam if needed for HOME path injection. Two new behavioral tests:

1. `test_history_up_arrow_loads_previous` -- inject two submitted commands via the state's history ring directly, simulate UP arrow via the input callback, assert `input_buf` matches the latest. UP again -> earlier. UP again -> stays at earliest.

2. `test_history_persists_via_file_roundtrip` -- write a known history to a temp file, point HOME at the temp dir via test hook, call a public load function, assert ring populated. Append a command in the test, call save, re-read file, assert merged content.

Total new tests: 2. Expected unit count after: **778** (currently 776).

### 1.5 RED-phase failure expectation

Pre-implementation: both tests fail with "undefined identifier `history_count`" / "undeclared function `boxen_repl_load_history`". Quote the actual messages in the agent report.

### 1.6 /gate focus instructions

`/gate "round 1 review: focus on history ring concurrency (single-threaded under GIL OK?), file I/O error paths (read fails / write fails / disk full), HOME env injection seam"`

### 1.7 Risks

- **None significant**: well-understood pattern, mirror of existing repl.c logic, no concurrency new surface, no GIL change. LOW risk.

### 1.8 Done criteria

- [ ] Unit 778/778 (was 776; +2 new)
- [ ] Integration 2186/21 baseline character-for-character
- [ ] /gate PASS or PASS WITH NOTES
- [ ] `dist/frontier-cli --debug-tui`: type three commands, exit cleanly, relaunch, press UP, see most recent command
- [ ] `~/.frontier_history` updated with the three commands (verify via `tail ~/.frontier_history`)
- [ ] Linenoise REPL still reads/writes the same file unchanged

---

## 2. Milestone C.0.2 -- Tab completion for slash commands and ODB paths

### 2.1 Goal

Tab key triggers completion in the boxen REPL. Slash commands (`/h<TAB>` -> `/help`) and ODB paths (`@sys.<TAB>` -> list of `system.X` candidates) both work. Multiple candidates render in a transient popup; single candidate auto-completes inline.

### 2.2 Files

| Action | File | Estimated LOC |
|---|---|---|
| Modify | `frontier-cli/boxen_repl.c` | +200 |
| Modify | `frontier-cli/boxen_repl_internal.h` | +30 (completion state) |
| New | `frontier-cli/boxen_completion_popup.c` | +250 (popup window) |
| New | `frontier-cli/boxen_completion_popup.h` | +30 |
| Modify | `tests/boxen_repl_tests.c` | +200 (4 new tests) |
| Modify | `frontier-cli/Makefile` | +1 (new source) |
| **Total** | | ~711 |

### 2.3 Implementation outline

**Reuse without modification:**
- `frontier-cli/repl.c::complete_slash_command_path` (lines 1962-2070) -- builds a list of completion strings given a buffer and offset
- `frontier-cli/repl.c::repl_resolve_path_ex` -- already public per OVERVIEW Q4

**Adapter layer:** the existing function takes a `linenoiseCompletions *lc` and calls `linenoiseAddCompletion`. We need either:
- (a) A version that takes a `void (*add)(void *ctx, const char *s)` callback. Refactor the existing function to use a generic add-callback; provide a small linenoise-side adapter so `repl.c` still works.
- (b) A wrapper in `boxen_repl.c` that builds a fake `linenoiseCompletions` and harvests its results.

**Choose (a)**: cleaner end-state, no fake structures. This is a small, contained change to `repl.c` -- adapter pattern, not behavior change. Verify linenoise REPL tests still pass after.

**State additions on `boxen_repl_state_t`:**
- `boxen_window_t *completion_popup_win` -- modal popup window, NULL when no completions active
- `char completion_candidates[MAX_CANDIDATES][BOXEN_REPL_INPUT_MAX]` -- ring of candidates
- `int completion_count`
- `int completion_selected` -- index of currently-highlighted candidate

**Behaviors:**
- `BOXEN_KEY_TAB` in `on_input`:
  - If completion popup is open: step selection down (or accept if only one candidate)
  - If closed: call completion logic. 0 candidates -> beep (no-op). 1 candidate -> insert it inline. 2+ -> open popup.
- `BOXEN_KEY_ESCAPE` while popup open: close popup, don't accept anything.
- `BOXEN_KEY_ENTER` while popup open: accept current selection.
- `BOXEN_KEY_UP/DOWN` while popup open: navigate candidates.

**Popup window:** new boxen modal window at ~10 rows below the input bar, ~40 cols wide. Lists candidates, one per row, current selection highlighted with `BOXEN_ATTR_REVERSE`. Reuses the modal mechanism from `boxen_window_set_modal`.

**Out of scope for C.0.2:**
- Fuzzy matching (DEFER -- prefix matching only)
- Documentation preview pane (DEFER)
- Caching of verb signatures (DEFER -- recompute every Tab)
- Tab cycling without popup (some users like this) -- DEFER

### 2.4 Tests

4 new tests:

1. `test_tab_completes_single_slash_command` -- input `/he`, Tab, assert `input_buf == "/help"`, no popup.
2. `test_tab_opens_popup_with_multiple_candidates` -- input `/`, Tab, assert popup is open and has N>1 candidates.
3. `test_tab_completes_odb_path_prefix` -- input `system.te`, Tab, assert appropriate completion either inline (single) or popup (multiple) based on what `system.te*` resolves to.
4. `test_escape_dismisses_popup_without_accept` -- open popup, send Escape, assert popup closed and `input_buf` unchanged.

Total new tests: 4. Expected unit count after: **782**.

### 2.5 RED-phase failure expectation

Tab key isn't handled in `on_input` yet, so all four tests fail with assertion errors after the Tab simulation: popup is NULL when expected non-NULL, etc.

### 2.6 /gate focus instructions

`/gate "round 1 review: focus on the completion adapter refactor in repl.c (make sure linenoise REPL still works), popup window lifecycle (open/close paths, focus restoration to input bar), candidate buffer overflow on long ODB paths, popup z-order vs other modals"`

### 2.7 Risks

- **MEDIUM**: the adapter refactor in repl.c touches active code in the linenoise REPL. Linenoise integration tests are sparse, so a subtle regression could hide.
- Mitigation: agent must verify by interactive smoke-test of the legacy REPL (run linenoise REPL, type a slash command, complete with Tab, confirm same behavior as today).

### 2.8 Done criteria

- [ ] Unit 782/782 (was 778; +4 new)
- [ ] Integration baseline holds
- [ ] /gate PASS or PASS WITH NOTES
- [ ] Manual: `--debug-tui`, `/<TAB>` opens popup, `/h<TAB>` completes to `/help`, `system.<TAB>` shows ODB candidates
- [ ] Manual: default `frontier-cli` (linenoise) -- `/h<TAB>` still completes to `/help` (regression check)

---

## 3. Milestone C.0.3 -- Slash-menu palette as boxen modal

### 3.1 Goal

The `/` key in the boxen REPL opens the same slash-menu palette that the linenoise REPL has, but rendered as a boxen modal window instead of a raw termbox2 overlay. Same UserTalk handler invocation contract; same arg-injection behavior; same slot-key shortcuts.

### 3.2 Files

| Action | File | Estimated LOC |
|---|---|---|
| Modify | `frontier-cli/palette.c` | +150 (render backend abstraction) |
| Modify | `frontier-cli/palette.h` | +50 (new public API) |
| Modify | `frontier-cli/boxen_repl.c` | +120 (palette wiring) |
| Modify | `frontier-cli/boxen_repl_internal.h` | +15 (palette state field) |
| Modify | `tests/boxen_repl_tests.c` | +200 (3 new tests) |
| Modify | `tests/palette_*_tests.c` (likely several) | +50 each (validate render-backend agnostic) |
| **Total** | | ~600 production + ~150 test |

### 3.3 Implementation outline

**The hard part:** palette.c currently renders via raw termbox2 in `palette_render_state` (line 530). It opens/closes the terminal in raw mode itself (`palette_open` at line 404). In boxen mode the terminal is ALREADY in raw mode (boxen owns it), and we need rendering to go through boxen window APIs.

**Approach:** introduce a render-backend abstraction in palette.c. Two implementations:
- `palette_render_termbox2_t` -- the current rendering (legacy linenoise REPL)
- `palette_render_boxen_t` -- new rendering via boxen window

Both implement the same render interface: `paint(palette_state_t *st, void *backend_ctx)`. `palette_render_state` becomes a dispatcher.

**Public API additions in palette.h:**
- `bool palette_open_boxen(palette_state_t *st, boxen_window_t *parent, int slot_key_offset, ...);` -- opens palette as a boxen modal child of `parent`
- `void palette_close_boxen(palette_state_t *st);` -- closes the boxen window without resetting terminal mode (boxen owns it)

**Boxen REPL wiring:**
- New keybind: `/` (or `BOXEN_KEY_NONE && ch == '/'`) in `on_input` opens the palette via `palette_open_boxen` if input_buf is empty
- Match legacy REPL's 250ms delay behavior using the boxen modal API
- Palette events route through the existing palette_state state machine; the boxen REPL doesn't need to know palette internals
- On palette close, slot-key dispatch follows the existing path (`palette_arg_inject_into_script`, `dispatch_script_handle_from_record`, etc.)

**Out of scope for C.0.3:**
- New palette features (search, recents, etc.)
- Different look-and-feel from legacy palette
- Slot-key remapping configuration
- Palette inside an editor window (only opens from the REPL)

### 3.4 Tests

3 new tests in `boxen_repl_tests.c`:

1. `test_slash_key_opens_palette_modal` -- type `/` (alone), assert palette modal window opened
2. `test_palette_close_returns_focus_to_repl_input` -- open palette, send Escape, assert palette closed AND REPL input has focus
3. `test_palette_arg_inject_pipeline_unchanged` -- behavioral assertion that with the same input, palette_arg_inject_into_script receives the same arguments as it does from the linenoise REPL (use a mock dispatch hook to capture)

Plus updates to existing `palette_*_tests.c` to verify the render-backend abstraction doesn't change behavior for the termbox2 backend. Existing assertions stay; render backend is set to termbox2 in setUp.

Total new tests: 3 in boxen_repl_tests. Expected unit count after: **785** (assuming existing palette tests stay equal in count).

### 3.5 RED-phase failure expectation

Pre-implementation: `palette_open_boxen` and `palette_close_boxen` undeclared. Slash key not handled. Tests fail at compile time.

### 3.6 /gate focus instructions

`/gate "round 1 review: focus on the palette render-backend abstraction (does legacy termbox2 rendering still work byte-for-byte?), modal window lifecycle vs other modals (z-order), palette_arg_inject contract preservation, slot-key dispatch path identical to linenoise REPL"`

### 3.7 Risks

- **MEDIUM-HIGH**: palette.c is 960 LOC of complex state. The render-backend abstraction is the biggest refactor in this entire plan.
- Mitigation 1: Plan to split this milestone if it grows. Could become C.0.3a (render-backend abstraction in palette.c, with termbox2 backend still used by both linenoise REPL and a stub boxen path) + C.0.3b (actual boxen rendering).
- Mitigation 2: Heavy use of existing palette tests as regression coverage. Any change to legacy palette behavior is a regression.
- Mitigation 3: Manual verification in both REPLs side-by-side before merging.

### 3.8 Done criteria

- [ ] Unit 785/785 (or higher if some existing tests added assertions)
- [ ] Integration baseline holds
- [ ] /gate PASS or PASS WITH NOTES
- [ ] Manual: `--debug-tui`, type `/`, palette opens as a boxen modal, navigate with arrow keys, Escape closes
- [ ] Manual: default `frontier-cli`, type `/`, palette opens as legacy overlay -- IDENTICAL behavior to before
- [ ] Manual: palette slot-key shortcut dispatches the same script in both REPLs (test with a known leaf)

---

## 4. Milestone C.0.4 -- Async output during in-progress input

### 4.1 Goal

When a background thread (HTTP responder, agent task, debugger callback) writes to stdout while the user is typing a command, the output appears in the scrollback pane WITHOUT disturbing the in-progress input. Currently C.0 already captures stdout via pipe + dup2, but the drain only happens after `boxen_poll_event` returns and the input bar redraws are sometimes choppy.

### 4.2 Files

| Action | File | Estimated LOC |
|---|---|---|
| Modify | `frontier-cli/boxen_repl.c` | +80 (drain orchestration) |
| Modify | `tests/boxen_repl_tests.c` | +100 (2 new tests) |
| **Total** | | ~180 |

### 4.3 Implementation outline

The fundamental problem doesn't exist in boxen the way it does in linenoise. Linenoise needs `linenoiseEditStop` to release the terminal, write the line, then `linenoiseEditStart` to resume. Boxen ALWAYS owns the terminal; output goes through the scrollback pane regardless of input bar state.

**What's actually needed:**
- Make sure `drain_stdout_into_scrollback` runs frequently enough that long-running writers don't stall on the pipe buffer (~64KB default)
- Add a per-iteration drain after `boxen_dispatch_event` returns (currently only after `boxen_poll_event` returns)
- Increase the poll timeout adaptively if input is being typed quickly (fewer wasted CPU cycles checking the pipe when nothing's there)

**Out of scope for C.0.4:**
- A separate async-drain thread (not needed -- single-threaded GIL model handles this)
- stdout buffering tuning (line-buffered is fine)
- Capturing stderr separately into a different pane

### 4.4 Tests

2 new tests:

1. `test_stdout_drain_during_typing_does_not_corrupt_input` -- inject a stdout write between keypresses, assert `input_buf` unchanged and scrollback has the line
2. `test_long_output_does_not_deadlock_event_loop` -- inject a >100KB stdout write, ensure event loop continues processing (pipe drains incrementally rather than blocking the producer permanently)

Total new tests: 2. Expected unit count after: **787**.

### 4.5 RED-phase failure expectation

`test_long_output_does_not_deadlock` may pass already if the existing C.0 drain is robust; this test is more of a regression guard than a feature test. Be honest in the report if the test passes pre-implementation; in that case it's purely a hardening commit.

### 4.6 /gate focus instructions

`/gate "round 1 review: focus on the drain orchestration timing (is there a write-then-block-on-pipe-full scenario?), event-loop responsiveness during sustained output, no stdout-in-output-pane-mid-line tearing"`

### 4.7 Risks

- **LOW**: mostly already working; this milestone is hardening + testing.

### 4.8 Done criteria

- [ ] Unit 787/787 (+2 new)
- [ ] Integration baseline holds
- [ ] /gate PASS
- [ ] Manual: `--debug-tui`, run a script that does `msg("hello")` in a loop with 100ms sleep between, type a command at the same time -- output appears smoothly, input stays correct

---

## 5. Milestone C.0.5 -- Multi-line input

### 5.1 Goal

Allow multi-line input via either `\` line continuation (matching UserTalk source convention) or an explicit multi-line mode toggle. Mainly useful for inline scripts.

### 5.2 Files

| Action | File | Estimated LOC |
|---|---|---|
| Modify | `frontier-cli/boxen_repl.c` | +100 (multi-line state + render) |
| Modify | `frontier-cli/boxen_repl_internal.h` | +10 (lines field) |
| Modify | `frontier-cli/boxen_repl.h` | +5 (line-completion enum?) |
| Modify | `tests/boxen_repl_tests.c` | +80 (2 new tests) |
| **Total** | | ~195 |

### 5.3 Implementation outline

**Approach:** opt-in via trailing backslash. If `input_buf` ends with `\` when Enter is pressed, the line is appended to a multi-line buffer and a fresh prompt (with continuation indicator `..` instead of `>`) appears.

**State additions:**
- `char multiline_buf[MULTILINE_MAX]` -- accumulator
- `int multiline_lines` -- count of accumulated lines

**Behaviors:**
- Enter with `input_buf` ending in `\`: strip the `\`, append to `multiline_buf` with `\n`, render `..` prompt, clear `input_buf`
- Enter without trailing `\`: if `multiline_lines > 0`, append final line to `multiline_buf` and submit the whole buffer; otherwise submit `input_buf` as usual
- Ctrl-C while in multi-line mode: discard `multiline_buf`, return to single-line prompt

### 5.4 Tests

2 new tests:

1. `test_backslash_continuation_accumulates` -- submit `foo\`, then `bar\`, then `baz`, assert eval_hook received `foo\nbar\nbaz` (or whatever the UserTalk convention requires)
2. `test_ctrl_c_in_multiline_discards_buffer` -- accumulate two lines, send Ctrl-C, assert `input_buf` is empty, multiline_buf is empty, prompt is back to `>`

Total new tests: 2. Expected unit count after: **789**.

### 5.5 RED-phase failure expectation

Tests fail with assertions about `multiline_buf` content.

### 5.6 /gate focus instructions

`/gate "round 1 review: focus on multi-line buffer bounds (MULTILINE_MAX overflow), how Ctrl-C interacts with the existing global key handler from PR #763, prompt rendering correctness across continuations"`

### 5.7 Risks

- **LOW**: contained feature, well-understood pattern.

### 5.8 Done criteria

- [ ] Unit 789/789 (+2)
- [ ] Integration baseline holds
- [ ] /gate PASS
- [ ] Manual: `--debug-tui`, type a multi-line `on foo() {`, see continuation prompt, complete script, submit, get expected eval result

---

## 6. Milestone C.0.6 -- Parity verification + documentation

### 6.1 Goal

Document the boxen REPL as parity-complete. Make the feature-comparison table from `REPL_SCHISM_RESOLUTION.md` Section 1 actually verifiable. Update user-facing docs.

### 6.2 Files

| Action | File | Estimated LOC |
|---|---|---|
| New | `docs/BOXEN_REPL_PARITY.md` | ~150 |
| Modify | `docs/CLI_USAGE_GUIDE.md` | +100 |
| Modify | `README.md` | +30 (mention boxen REPL is now production-ready) |
| Modify | `planning/phase_c/PROPOSAL_REVISED.md` | +20 (mark C.0.x complete) |
| **Total** | | ~300 docs |

### 6.3 Implementation outline

This is a docs milestone. No code.

`docs/BOXEN_REPL_PARITY.md` lists every linenoise REPL feature and the boxen REPL equivalent. Each row has:
- Feature name
- Linenoise behavior (current default)
- Boxen behavior (post C.0.5)
- Status: IDENTICAL / SUPERSET / DIFFERENT-BUT-INTENTIONAL / GAP

Any GAP rows trigger a follow-up issue or a "we accept this difference" rationale.

`docs/CLI_USAGE_GUIDE.md` adds a section: "Running with `--debug-tui`: when to use which REPL." Foreshadows C.6 but doesn't recommend boxen as default yet.

`README.md` notes that `--debug-tui` is now feature-complete for daily use; default remains linenoise until C.6 ships.

`PROPOSAL_REVISED.md` updates the C.0 milestone row to show all C.0.x sub-milestones delivered.

### 6.4 Tests

No new tests; this is docs.

### 6.5 /gate focus instructions

`/gate "round 1 review: docs-only review, focus on parity-table accuracy and any intentional-difference rationale"` -- expect /gate to be light here.

### 6.6 Risks

- **LOW**: docs only.

### 6.7 Done criteria

- [ ] /gate PASS WITH NOTES (docs)
- [ ] Parity table is complete; any GAPS have follow-up issues filed
- [ ] CLI usage guide updated
- [ ] README.md updated

---

## 7. Milestone C.6 -- Flip default; deprecate `--debug-tui`

### 7.1 Goal

Default `frontier-cli` launches the boxen REPL. `--debug-tui` flag becomes a no-op alias (still accepted to avoid breaking existing scripts). `--plain` flag falls back to the legacy linenoise REPL.

### 7.2 Files

| Action | File | Estimated LOC |
|---|---|---|
| Modify | `frontier-cli/main.c` | +30 (dispatch reorder) |
| Modify | `frontier-cli/cli_parser.c` | +50 (--plain flag) |
| Modify | `frontier-cli/cli_parser.h` | +5 |
| Modify | `tests/integration/*.yaml` | scan for any tests that depend on legacy REPL behavior; update |
| Modify | `docs/CLI_USAGE_GUIDE.md` | +30 |
| Modify | `README.md` | +20 |
| **Total** | | ~135 + integration updates |

### 7.3 Implementation outline

**Behavior change in `main.c`:**
- Default (no flag) -> boxen REPL
- `--plain` -> linenoise REPL (current default)
- `--debug-tui` -> boxen REPL with a deprecation log_warn at startup ("--debug-tui is now the default; flag is a no-op and will be removed in a future release")

**Out of scope for C.6:**
- Deleting `repl.c` (defer to C.6.1 or a later cleanup PR; need a release cycle to make sure no scripts rely on linenoise default)
- Deleting `--debug-tui` flag entirely (defer same)
- Changing `--protocol` behavior (untouched)

### 7.4 Tests

Integration test sweep -- any test that runs `frontier-cli` interactively must be examined for assumptions about which REPL is active. Update as needed. Most tests use `--protocol` so should be unaffected.

Add `tests/integration/cli/test_plain_flag.yaml` -- one test that verifies `--plain` launches linenoise behavior, and one that verifies default is now boxen.

Expected unit count: unchanged (no new unit tests; integration covers the dispatch change).

### 7.5 /gate focus instructions

`/gate --all "round 1 review: focus on the mode dispatch change in main.c (is --protocol still unaffected?), --plain flag mutual exclusion with --debug-tui (both reject? or --plain wins?), --debug-tui deprecation log doesn't fire in scripts (would noise up CI logs)"`

### 7.6 Risks

- **MEDIUM**: user-visible change. Anyone scripting `frontier-cli` interactively will see a different REPL.
- Mitigation: deprecation log on `--debug-tui` makes the transition visible. `--plain` provides explicit fallback. Document in release notes prominently.

### 7.7 Done criteria

- [ ] Unit baseline + 0 new (unit-count unchanged unless tests grow elsewhere)
- [ ] Integration: baseline + new test files for `--plain` and default-is-boxen; no regressions in existing tests
- [ ] /gate PASS or PASS WITH NOTES
- [ ] Manual: default `frontier-cli` launches boxen REPL
- [ ] Manual: `--plain` launches linenoise REPL (legacy behavior)
- [ ] Manual: `--debug-tui` still works (no-op alias) and prints deprecation warning
- [ ] Release notes draft included in PR

---

## 8. Estimated total scope

| Milestone | LOC (prod) | New tests | Risk | Estimated PR rounds |
|---|---|---|---|---|
| C.0.1 history | 275 | 2 | LOW | 1-2 |
| C.0.2 completion | 511 | 4 | MEDIUM | 2-3 |
| C.0.3 palette | 335 + 150 docs | 3 | MEDIUM-HIGH | 2-3 (may split) |
| C.0.4 async output | 80 | 2 | LOW | 1-2 |
| C.0.5 multi-line | 115 | 2 | LOW | 1-2 |
| C.0.6 docs/parity | 300 docs | 0 | LOW | 1 |
| C.6 flip default | 135 + integration | 1-2 integration | MEDIUM | 1-2 |
| **Total** | ~1,950 prod + 600 docs | 15 unit + 1-2 integration | -- | 9-15 PRs |

Calendar estimate: 4-6 weeks of focused work at the pace of recent Phase C milestones.

---

## 9. Inter-milestone coordination

### 9.1 Sequencing dependencies

C.0.2 (completion) modifies the slash-completion adapter in `repl.c`. C.0.3 (palette) does NOT depend on this. They could run in parallel branches IF independent agents work simultaneously, BUT the merge order matters -- whichever ships second must rebase.

**Recommendation: strict sequential.** Avoids rebase pain and keeps the schism-resolution narrative coherent. Each PR is reviewed against the latest develop.

### 9.2 Branch hygiene

Each milestone runs in its own worktree under `.claude/worktrees/phase-c-0-N-<slug>`. Per Frontier merge-from-worktree memory: do not use `--delete-branch` from inside the worktree.

### 9.3 Standing autonomy boundary

`/auto` may merge each C.0.x PR autonomously IF the merge-safety check in `/auto` Phase 5 passes (CI green, /gate PASS, 0 P0/P1, baseline holds character-for-character). Anything else surfaces.

For C.6 specifically -- the user-visible default flip -- /auto must STOP and ask before merge, regardless of /gate verdict. The decision to flip the default is user-domain, not automation-domain.

### 9.4 If a milestone reveals deeper issues

Each milestone is small enough that if implementation surfaces unexpected complexity (e.g., palette abstraction turns out to need fundamental redesign), the agent surfaces immediately via `/ask` rather than ballooning the PR.

The user gets to choose: split the milestone, defer, or accept the larger scope.

---

## 10. Follow-up issues to file

These items are known follow-ups, deliberately NOT included in any C.0.x milestone:

1. **Ctrl-R inverse history search** -- C.0.1 ships up/down only; Ctrl-R is a deferred polish item
2. **Fuzzy completion matching** -- C.0.2 ships prefix matching only
3. **Documentation preview pane in completion** -- C.0.2 doesn't show docs alongside candidates
4. **Verb signature caching for completion** -- C.0.2 recomputes every Tab; could cache
5. **Palette search/recents** -- C.0.3 ships parity with legacy only
6. **Async drain thread** -- C.0.4 uses synchronous polling; could move to separate thread if needed
7. **Multi-line history entries** -- C.0.5 / C.0.1 don't compose specially
8. **Delete linenoise REPL entirely** -- C.6 deprecates but doesn't delete; cleanup PR after a release cycle

Each follow-up is at most P2 and won't block the main path. File as part of C.0.6 documentation pass.

---

## 11. Approval

JES approved Path A direction in REPL_SCHISM_RESOLUTION.md (2026-06-10).

This execution plan is the operational form of that approval. Each milestone proceeds via /doit or /auto as scoped above. The user retains the final-merge decision authority on C.6 specifically.

Open this document and the proposal side-by-side when dispatching agents for any milestone.
