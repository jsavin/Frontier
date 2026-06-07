# boxen — Phase A Milestone Breakdown

Phase A from `EXECUTION_PLAN.md` is "the substrate library, ready for the debugger TUI." That's 4 weeks of work. This document breaks it into ~7 PR-sized milestones that can each be shipped through `/auto` independently.

**Status**: 2026-06-06. Phase A not yet started. Author: Claude (session `f1-boxen-tui-lib`).

---

## Why split

The Phase A acceptance criteria in `EXECUTION_PLAN.md` Section 16 are binary and demanding:

- 5 test binaries
- ≥ 30 test functions, all green
- `split_demo` working on macOS and Linux
- CI green on macOS, Linux, **and** Windows
- `grep -r 'tb_' frontier-cli/boxen/ | grep -v backend_tb2.c` returns empty
- `boxen.h` is the only public header

Hitting all of that in one PR is the wrong shape. Reviewers can't usefully review a 1,400-LOC substrate drop. Splitting into milestones with clear dependencies lets each PR land with focused review, focused tests, and a small blast radius if any milestone needs rework.

Each milestone below is shaped to be **one `/auto` PR**: scope bounded, dependencies explicit, acceptance criteria binary, fits the "small enough to review" bar.

---

## Dependency graph

```
A.0 (termbox2 vendor + CI)
 ├─> A.1 (backend interface + tb2 backend stub + mock backend)
 │     ├─> A.2 (window primitive + clipping + first tests)
 │     │     ├─> A.3 (z-order + redraw cycle + focus)
 │     │     │     ├─> A.4 (resize + move state machine)
 │     │     │     ├─> A.5 (scrolling content model)
 │     │     │     └─> A.6 (chrome + layout helpers + split_demo)
```

A.4, A.5, A.6 can theoretically run in parallel after A.3. In practice, /auto chain runs them sequentially because each builds on the others' API exposure.

---

## Milestone A.0 — Vendor termbox2 + CI infrastructure

**One sentence**: Drop termbox2 into the tree, wire up the build system to compile it, set up CI on macOS — no boxen code yet.

**Scope**:
- Create `frontier-cli/third_party/termbox2/` and vendor `termbox2.h` from upstream (v2.5.0, MIT license).
- NOTE: termbox2 v2.5.0 is a **single-header library** — there is no separate `termbox2.c` upstream. The implementation is activated by `#define TB_IMPL` before including the header. `termbox2_impl.c` is the Frontier-authored TU that owns this define.
- Pin the upstream commit SHA in `frontier-cli/third_party/termbox2/VERSION` so future updates are explicit.
- Update `frontier-cli/Makefile` to compile `termbox2_impl.c` into the existing frontier-cli build, and add a `tb2-smoke` target (see below).
- Add a placeholder `frontier-cli/boxen/` directory with an empty `boxen.h` (just a header guard) and an empty `README.md`.
- Create `.github/workflows/boxen-ci.yml` (macOS-only — see scope note below).

**Intentional scope reduction: macOS-only CI**

The original A.0 acceptance criteria called for CI green on macOS, Linux, and Windows. This is amended:

- **A.0 CI is macOS-only.** frontier-cli uses `-fpascal-strings` and Mach-O linker flags (`-Wl,-undefined,dynamic_lookup`) that are clang/macOS-specific. We do not have a Linux/Windows test environment. Building frontier-cli on Linux/Windows is a multi-PR effort deferred to a later phase.
- This is not a deferral of the termbox2 substrate question. The `tb2-smoke` target (see below) proves termbox2 itself compiles cleanly. When a Linux/Windows env is available, adding those CI jobs is a CI-config change — no code changes needed.
- The `tb2-smoke` Makefile target exists and is verified on macOS in A.0's CI. It compiles `termbox2_impl.c` + `smoke_main.c` as a standalone binary (no frontier-cli dependencies). This is the forward-compat hook for Linux/Windows CI.

**The tb2-smoke target**

`make -C frontier-cli tb2-smoke` compiles termbox2 in isolation (no Frontier headers, no pascal-strings, no Mach-O flags). It does NOT call `tb_init()` (which requires a TTY). It exercises link-time symbol resolution only and exits 0. The target exists at A.0 so adding Linux/Windows CI jobs at A.N is purely a workflow YAML change.

**Out of scope** (deferred to later milestones):
- The boxen backend implementation (A.1)
- Any boxen-specific tests (A.2+)
- Linux/Windows CI (deferred — no test env available)
- termbox2 integration tests (proven indirectly by linking)

**Acceptance criteria** (all binary, updated from original):

| Criterion | Status | Notes |
|-----------|--------|-------|
| `frontier-cli/third_party/termbox2/termbox2.h` exists with `VERSION` file | ☐ | Single-header library; no separate `.c` upstream |
| `make -C frontier-cli` succeeds on macOS, no new warnings | ☐ | termbox2_impl.c produces zero warnings |
| `make -C frontier-cli tb2-smoke && ./frontier-cli/tb2-smoke` succeeds | ☐ | Forward-compat smoke for Linux/Windows CI |
| `./tools/run_headless_tests.sh` passes (unit baseline unchanged) | ☐ | |
| `frontier-cli/boxen/boxen.h` exists with header guard only | ☐ | |
| `.github/workflows/boxen-ci.yml` exists, triggers on PR, CI green on macOS | ☐ | macOS-only per scope reduction above |
| Frontier integration suite baseline preserved (21 pre-existing failures, no new ones) | ☐ | |

**Rough LOC**: ~3500 lines vendored (termbox2.h is 3519 lines) + ~100 lines of build/CI config.

**Risk callouts**:
- termbox2 may have warnings under `-Wall -Wextra` that don't show up in its own build but do show up in Frontier's. Mitigation: if warnings fire, add targeted `#pragma clang diagnostic` suppressions in `termbox2_impl.c` only. (For v2.5.0 on macOS/clang with `-std=c17 -Wall -Wextra`: zero warnings observed.)

---

## Milestone A.1 — Backend interface + termbox2 backend + mock backend

**One sentence**: Define the 11-function backend vtable, implement it for termbox2, implement it for the mock backend, no window manager logic yet.

**Scope**:
- Define `boxen_backend_t` in `frontier-cli/boxen/boxen.h` exactly as specified in `EXECUTION_PLAN.md` §13 (11 functions, with `set_cell` taking `attr`, `color_depth`, `set_cursor_visible`).
- Define `boxen_event_t` (the union over key/mouse/resize sub-structs, with `mod` on key+mouse and `flags` on mouse per recent corrections).
- Define `boxen_key_t` and `boxen_mod_t` enums in the public header per `EXECUTION_PLAN.md` §6.
- Implement `frontier-cli/boxen/backend_tb2.c`: a thin shim that translates `TB_KEY_*` → `BOXEN_KEY_*` and forwards everything else to termbox2. Synthesize double-click in the shim per `EXECUTION_PLAN.md` §13 correction 5 (tracked timestamp + position state).
- Implement `frontier-cli/boxen/backend_mock.c`: an in-memory cell grid with `boxen_mock_*` test helpers (`cell_at`, `has_text`, `push_event`, `push_double_click`, `width_set`, `height_set`).
- Implement `frontier-cli/boxen/boxen_wcwidth.c`: vendored Kuhn 2007 `mk_wcwidth`, renamed `boxen_wcwidth` per `EXECUTION_PLAN.md` §4.
- Implement `frontier-cli/boxen/boxen_log.c`: the no-op-default logging hook vtable per `EXECUTION_PLAN.md` §9.
- One test binary: `tests/boxen_backend_tests.c` — verifies the mock backend's behavior (cell writes, event injection, double-click synthesis timing), confirms the termbox2 backend at least initializes without error.

**Out of scope**:
- Any window management (windows, z-order, etc.) — A.2 onward
- `backend_tb2.c` actually being used by anything — the test binary uses the mock

**Acceptance criteria**:
- ☐ `boxen.h` declares `boxen_backend_t` (11 functions), `boxen_event_t`, `boxen_key_t`, `boxen_mod_t`, `boxen_color_t`, `boxen_attr_t`, `BOXEN_MOUSE_*` flags
- ☐ `boxen_internal.h` exists with private definitions and is NOT included by any test or example outside `frontier-cli/boxen/`
- ☐ `backend_tb2.c` and `backend_mock.c` both build clean
- ☐ `tests/boxen_backend_tests.c` has ≥ 6 test functions, all green
- ☐ `grep -r 'tb_' frontier-cli/boxen/ | grep -v backend_tb2.c` returns empty
- ☐ CI green on all 3 platforms
- ☐ Frontier baselines preserved

**Rough LOC**: ~600 (backend_tb2 ~200, backend_mock ~150, wcwidth ~150, log ~50, headers ~50)

**Risk callouts**:
- Mock backend's double-click synthesis needs a deterministic clock for tests (can't depend on wall-clock). Pattern: take a `boxen_mock_now_ms_fn` hook in the mock backend so tests can inject time. Document as part of the mock API.

---

## Milestone A.2 — Window primitive + clipping + lifecycle

**One sentence**: Add `boxen_window_t`, open/close/destroy, content-coordinate translation, window-bounded clipping for cell writes — the foundation everything else stacks on.

**Scope**:
- Implement `boxen_window_t` (opaque, defined in `boxen_internal.h`).
- Implement `boxen_init` / `boxen_shutdown` (the library lifecycle).
- Implement `boxen_window_open` / `boxen_window_close`.
- Implement `boxen_window_set_rect` / `boxen_window_set_title` / `boxen_window_set_user_data`.
- Implement `boxen_set_cell` and `boxen_draw_text` and `boxen_fill_rect` — all of which take a `boxen_window_t *` and translate window-local coordinates to terminal coordinates, clipping at the window border.
- Implement `boxen_window_content_width` / `boxen_window_content_height` (w/h minus chrome — chrome itself comes in A.6, so for A.2 the content rect equals the window rect).
- Implement `boxen_window_at(sx, sy, *cx, *cy)` — the screen-to-window-local coordinate translator (no chrome yet, so output is straightforward).
- Test binary: `tests/boxen_core_tests.c` — covers window lifecycle, clipping at all 4 borders, content/screen coordinate translation, multiple windows on the grid.

**Out of scope**:
- z-order (A.3)
- input dispatch (A.3)
- chrome / borders / titles drawn on the screen (A.6)
- modal flag (A.3)

**Acceptance criteria**:
- ☐ `boxen_core_tests.c` has ≥ 8 test functions, all green
- ☐ At least one test verifies that a `boxen_set_cell(x, y, ...)` call with x or y outside the window's content area is a no-op (clipping works in all four directions)
- ☐ At least one test verifies `boxen_window_at(sx, sy, &cx, &cy)` returns the correct window and `(cx, cy)` for a point inside a window, and `NULL` for a point outside any window
- ☐ CI green on all 3 platforms
- ☐ Frontier baselines preserved

**Rough LOC**: ~250

---

## Milestone A.3 — Z-order + redraw cycle + focus + input dispatch + modal

**One sentence**: Add the redraw loop, focus model, input dispatch, modal flag — the parts that make a window manager an actual window manager.

**Scope**:
- Implement the back-to-front sorted-list redraw cycle: `boxen_present()` walks windows from back to front, calls each window's draw callback, then calls `backend->present()`.
- Implement `boxen_window_raise` / `boxen_window_lower`.
- Implement `boxen_window_focus` (sets the focused window and raises it).
- Implement `boxen_window_set_modal` — when a modal window exists, input events go to the modal window first; non-modal windows do not receive input until the modal is closed.
- Implement `boxen_poll_event(boxen_event_t *out, int timeout_ms)` — the primitive that Frontier surface code wraps with GIL release/reacquire.
- Implement `boxen_window_set_draw` / `boxen_window_set_input` (the callback setters).
- Implement `boxen_window_invalidate` — request redraw (currently always-redraw, but the API is the same when dirty-rect tracking is added later).
- Implement `boxen_run` + `boxen_quit` — the convenience main loop for standalone programs (calls poll + dispatch + present in a loop with a quit flag).
- Test binary: `tests/boxen_input_tests.c` — covers focus, z-order, modal input blocking, event dispatch, the redraw cycle's back-to-front ordering.

**Acceptance criteria**:
- ☐ `boxen_input_tests.c` has ≥ 8 test functions, all green
- ☐ At least one test injects a key event via `boxen_mock_push_event` and verifies the focused window's input callback is invoked, not any other window's
- ☐ At least one test verifies modal input blocking: with a modal window present, key events do NOT reach non-modal windows
- ☐ At least one test verifies z-order: raising a covered window causes it to appear on top of the cells it shares with the previously-raised window after the next `boxen_present()` cycle
- ☐ CI green on all 3 platforms
- ☐ Frontier baselines preserved

**Rough LOC**: ~250

---

## Milestone A.4 — Resize + move state machine

**One sentence**: Mouse-drag and keyboard-driven move/resize, terminal-resize event handling, minimum-size enforcement.

**Scope**:
- Implement the resize/move state machine: NORMAL → MOVING/RESIZING (mouse) and MOVING_KB/RESIZING_KB (keyboard with Ctrl+M / Ctrl+R + arrows).
- Implement hit-testing: given mouse `(x, y)`, find the topmost window under it and whether the click is on title bar, body, or a resize-corner.
- Implement drag tracking (anchor + start-rect → current-rect on mouse-move).
- Implement `boxen_window_set_resizable(win, bool)` / `boxen_window_set_movable(win, bool)`.
- Implement `boxen_window_set_min_size(win, w, h)` — minimum dimensions the state machine enforces.
- Implement terminal-resize handling: when `BOXEN_EV_RESIZE` arrives, clamp every window's rect to fit the new terminal, possibly shrinking them.
- Test binary: `tests/boxen_resize_tests.c` — covers the state machine, hit-testing, minimum-size enforcement, terminal-resize clamping.

**Acceptance criteria**:
- ☐ `boxen_resize_tests.c` has ≥ 6 test functions, all green
- ☐ At least one test simulates a sequence: mouse-press on title bar → mouse-move (drag) → mouse-release, and verifies the window's position changed accordingly
- ☐ At least one test verifies that a `BOXEN_EV_RESIZE` event with smaller dimensions causes all windows to clamp to the new bounds and never go below `min_size`
- ☐ At least one test verifies hit-testing: a click in the title-bar region returns "title", a click in a resize corner returns "corner-N" for the right N
- ☐ CI green on all 3 platforms
- ☐ Frontier baselines preserved

**Rough LOC**: ~280

---

## Milestone A.5 — Scrolling content model

**One sentence**: Each window owns `scroll_x` / `scroll_y` and a content size; surfaces draw via "draw row N here" callbacks; ensure-visible helpers handle the math.

**Scope**:
- Implement `boxen_window_set_content_size(win, w, h)` — surfaces declare logical content dimensions.
- Implement `boxen_window_set_scroll(win, x, y)` / `boxen_window_get_scroll(win, *x, *y)` / `boxen_window_scroll_by(win, dx, dy)`.
- Implement `boxen_window_ensure_visible(win, x, y)` — adjust scroll so `(x, y)` is in the visible content area.
- Update `boxen_set_cell` and `boxen_window_at` to account for scroll offset when translating between content and screen coordinates.
- Implement `boxen_window_set_row_highlight(win, row, attr, fg, bg)` — the row-highlight decorator for the debugger's current-line marker.
- Test binary: `tests/boxen_scroll_tests.c` — covers content larger than rect, scroll-by, ensure-visible, row-highlight rendering, and content↔screen coordinate translation under scroll.

**Acceptance criteria**:
- ☐ `boxen_scroll_tests.c` has ≥ 6 test functions, all green
- ☐ At least one test creates a window with content size > rect, calls `ensure_visible` for a row outside the current view, then verifies via `get_scroll` that the scroll changed by the expected amount
- ☐ At least one test verifies that `boxen_window_at(sx, sy, &cx, &cy)` returns content coordinates that correctly account for `scroll_y` after the window has been scrolled
- ☐ At least one test sets a row highlight and verifies the cells in that row carry the highlight attr after the next `present()`
- ☐ CI green on all 3 platforms
- ☐ Frontier baselines preserved

**Rough LOC**: ~180

---

## Milestone A.6 — Chrome + layout + split_demo

**One sentence**: Borders, titles, scrollbars, the two split-layout helpers, and the `split_demo` example program that proves the substrate works end-to-end.

**Scope**:
- Implement border drawing (configurable line style, focused vs unfocused).
- Implement title rendering with truncation for narrow windows.
- Implement scrollbar rendering on the right edge of scrollable windows.
- Implement `boxen_layout_split_h(rect, ratio, left_title, *left, right_title, *right)` and `boxen_layout_split_v(...)` per `EXECUTION_PLAN.md` §11 correction.
- Implement `boxen_window_set_pinned(win, bool)` — pinned windows (the keybind footer) don't move during resize/layout-recalc.
- Update `boxen_window_content_width` / `boxen_window_content_height` to subtract chrome.
- Build `frontier-cli/boxen/examples/split_demo.c` — opens two side-by-side scrollable windows with a third pinned footer window showing keybinds. Demonstrates the substrate end-to-end interactively.
- Test binary: `tests/boxen_chrome_tests.c` — covers border rendering, title truncation, scrollbar visibility, layout helpers' output dimensions, pinned-window behavior under resize.

**Acceptance criteria** — this is the Phase A final acceptance gate:
- ☐ `boxen_chrome_tests.c` has ≥ 6 test functions, all green
- ☐ Total test count across all 5 test binaries is **≥ 30** (Phase A acceptance criterion)
- ☐ `split_demo` builds on macOS and Linux and is interactively usable (manual smoke test acceptable; documented in PR description)
- ☐ `grep -r 'tb_' frontier-cli/boxen/ | grep -v backend_tb2.c` returns empty
- ☐ `boxen.h` is the only public header (`boxen_internal.h` exists but is not included from any test or example outside `frontier-cli/boxen/`)
- ☐ CI green on macOS, Linux, **and** Windows
- ☐ Frontier baselines preserved

**Rough LOC**: ~280 (chrome ~80, layout ~80, split_demo ~80, pinned ~40)

**On hitting A.6 green**: Phase A is complete. Update task #135 to "Phase A done; Phase B ready for parallel session." Notify the parallel session via task #145.

---

## What this breakdown costs

| Milestone | LOC | Tests added | Cumulative tests | Cumulative LOC |
|-----------|-----|-------------|------------------|----------------|
| A.0 | ~3000 (mostly vendored) | 0 | 0 | 3000 |
| A.1 | ~600 | ~6 | ~6 | 3600 |
| A.2 | ~250 | ~8 | ~14 | 3850 |
| A.3 | ~250 | ~8 | ~22 | 4100 |
| A.4 | ~280 | ~6 | ~28 | 4380 |
| A.5 | ~180 | ~6 | ~34 | 4560 |
| A.6 | ~280 | ~6 | ~40 | 4840 |

Total: ~4840 LOC, of which ~3000 is vendored termbox2 and the rest is boxen substrate + tests. Substrate-only is ~1840 LOC, which exceeds the OVERVIEW estimate of ~1300-1400. The delta is test code (every milestone adds tests; OVERVIEW didn't count test LOC) and the vendored wcwidth + log + backend shim already accounted for in the corrected estimate.

Tests-only total: ~40 across 5 binaries, comfortably above the ≥ 30 floor.

---

## Risk callouts for the chain

- **A.0 is the load-bearing milestone for CI**. If Windows CI fails at A.0, the substrate decision (termbox2 over notcurses, partly because of Windows support) may need revisiting. **This is the most likely `/ask` moment in the chain.**
- **A.1's double-click synthesis is the most likely source of subtle bugs**. Time-dependent state in a backend shim is hard to test deterministically. The deterministic-clock hook is the mitigation.
- **A.4's state machine touches mouse + keyboard + resize event handling**. Most state-machine bugs in a window manager are found here. Test coverage matters.
- **A.6's `split_demo` is the only Phase A artifact that requires interactive use to verify**. Acceptance allows manual smoke test. If we can't run a TTY in CI, that's fine; the unit-level coverage of layout helpers in `boxen_chrome_tests.c` is the deterministic check.

---

## Chain execution discipline

When `/auto` runs each milestone:

1. Each `/auto` invocation is a single PR (worktree → plan → implement → gate → merge → cleanup).
2. The chain proceeds in the order A.0 → A.6. No skipping; A.N depends on A.0..A.N-1 being merged.
3. Between milestones, the chain pulls develop fresh and verifies tests still pass before starting the next milestone (per `/auto` chain mode discipline).
4. If a milestone fails review (P0/P1 not resolvable in 10 rounds), the chain pauses and surfaces.
5. If a milestone introduces a new architectural question that affects later milestones (e.g., backend interface needs a function that was deferred to A.3 but A.1 needs it), the chain pauses with `/ask`.
6. The Phase B parallel session does NOT start until A.6 ships green. Task #145 is the reminder.

---

## When this document is wrong

If implementing A.N reveals that the dependency order is wrong, or the scope of A.M needs to be split further, or a milestone genuinely belongs in Phase B, update this document AS PART OF the milestone's PR. Don't let the planning artifact diverge from reality.
