# REPL slash-menu palette — test harness design

**Date**: 2026-05-06
**Author**: Jake Savin (drafted with research support)
**Companion to**: [repl-slash-menu-implementation-plan.md](repl-slash-menu-implementation-plan.md), [ADR-016](../architectural_decision_records/ADR-016-headless-menu-system-projection.md).
**Status**: Design — to ship as Plan 1, before PR 4 (compositor) starts.
**Audience**: internal technical (Jake + Frontier kernel agents).

---

## Why a dedicated test harness, before PR 4

The slash-menu palette is the first piece of Frontier code where the *output is a screen*, not a value or a side-effect on the ODB. Every UI bug we don't catch in CI either:

1. lands in front of the user as a regression (terminal stuck in mouse-tracking mode, garbled cascade rendering, ESC ambiguity hanging the input loop), or
2. forces a manual smoke step that `/auto` cannot honestly converge on.

The slash-menu plan acknowledges this by listing four acceptance-suite layers in §3 (kernel-side, palette UX, palette mouse, compositor unit, mouse-parser unit). This document picks those apart into a coherent four-layer harness with concrete decisions about formats, drivers, and where the seams are.

The non-negotiables driving the design:

- **Behavioral, not source-inspection.** A palette test that greps `palette.c` for "ESC" is worthless. The tests must exercise the data path from input byte → palette state → cell buffer → terminal output (or stop one frame short of the terminal, in the snapshot tests).
- **Deterministic.** No real terminals, no real mouse, no sleep-based timing. All inputs synthetic; all clocks injectable.
- **Fast.** A unit test that runs in 5 ms is the difference between TDD-natural and TDD-aspirational. Aim: <1 s for the unit layers, <30 s for the integration layer end-to-end.
- **Isolated.** Layer 1 (snapshot) must not depend on linenoise, the ODB, or the full REPL boot. Layer 4 (integration) is the only layer that touches a real `frontier-cli` process.

---

## The four-layer harness, at a glance

| Layer | What it tests | Runner | Speed | Format |
|---|---|---|---|---|
| L1 — Snapshot | `palette_state_t` → cell buffer ASCII frame | C unit test (`tests/unit/palette_render_test.c`) | <50 ms | Golden text fixtures in `tests/fixtures/palette/*.txt` |
| L2 — Input parser | SGR 1006 mouse seq parser, ESC vs ESC-bracket disambiguation | C unit test (`tests/unit/palette_input_test.c`) | <10 ms | Inline expected structs |
| L3 — Compositor | register/move/resize panes, z-order, hit-test, diff-render | C unit test (`tests/unit/pane_compositor_test.c`) | <100 ms | Inline + ASCII golden frames |
| L4 — Integration | real `frontier-cli` REPL via pexpect; keystrokes/mouse → observable behavior | Python via `tests/integration/runner.py` | 5–30 s | YAML test cases (`tests/integration/test_cases/repl_palette*.yaml`) |

Layers 1–3 link against a tiny harness-library subset of the compositor + palette TUs, with no linenoise, no ODB, no kernel boot. Layer 4 spawns a real CLI in a PTY; it is the only place where flakiness can creep in, and it stays small (cascade open/close, hotkey jump, ESC, click routing).

The kernel-side acceptance suite (the 16 unskipped `menu_data_verbs.yaml` tests) is **out of scope** for this harness — it's pure ODB/verb behavior, validated by the existing integration runner. This harness is about the UI substrate.

---

## Layer 1: Pure-function snapshot tests

### What we're asserting

Given a fully constructed `palette_state_t` (menubar layout, cascade stack, focused pane, selected row, hotkey underlines, scroll offset), the rendering function produces exactly the expected cell buffer. Frame-perfect.

This is the layer that catches:

- "the underline moved from F to i when File was selected"
- "the cascade child opened at the wrong y-coordinate"
- "the selected row's inverse-video extends one cell past the border"
- "the right-overflow rule didn't kick in when there are 4 cells of slack"
- "the parent menu got dimmed when the child opened (it shouldn't)"

These are the bugs that pure rendering tests catch *cheaply*, and that integration tests catch *expensively and flakily* (or not at all, when the bug is "wrong glyph at column 12 row 3" buried inside a 24×80 PTY scrape).

### The render entry point

The compositor exposes a pure function:

```c
// palette_render.h
//
// Pure render: given palette state + viewport size, emit cells into a
// caller-provided cell-buffer. No terminal I/O. No globals read or written.
// Uses only the inputs supplied. Safe to call from a unit test thread.

typedef struct palette_cell {
    uint32_t codepoint;     // UCS-4; 0 = blank
    uint8_t  fg;            // ANSI 0-7 + bright bit; 0xFF = default
    uint8_t  bg;
    uint8_t  attrs;         // bit 0 underline, bit 1 reverse, bit 2 dim, bit 3 bold
    uint8_t  _pad;
} palette_cell_t;

typedef struct palette_frame {
    int rows, cols;
    palette_cell_t *cells;  // cells[row * cols + col]; caller-owned
} palette_frame_t;

// Render. Never fails. Returns the dirty bounding rect or whole-frame on first call.
struct palette_dirty_rect palette_render(const palette_state_t *state,
                                         palette_frame_t *out);
```

The same function feeds the production renderer (which then calls `compositor_diff_emit` to produce CSI). In the unit test it stops at the cell buffer.

### Snapshot fixture format

ASCII frames in `tests/fixtures/palette/*.txt`. One fixture per scenario. Two flavors of golden:

**Frame-only (terse, default).** Plain text, one line per terminal row. Cells with attributes other than default get a sidecar attribute map (next paragraph). Trailing whitespace is significant — strip-on-write would corrupt blank-cell positions.

```
+--Frontier--+
| About       |
| Documentat. |
| Key Codes   |
| Clear Vars  |
| Quit        |
+-------------+
```

**Frame + attrs (verbose).** When the test cares about underline / inverse / colors, the fixture pairs the text frame with an attribute frame using single-char codes:

```
# frame.txt
+--Frontier--+
| About       |
| Documentat. |
...

# attrs.txt   (same dimensions; '.' = default)
.............
.U...........
.............
.....U.......
.............
.............
```

Where `U` = underline, `R` = reverse, `D` = dim, `B` = bold. Combinations get distinct codes (`X` = U+R, etc.; full table in the harness header). Color is rare in v1 (the plan is monochrome); add `1`–`8`/`9`–`F` cells when ANSI 16-color lands in PR 8.

**Why ASCII, not binary or JSON.** Goldens get reviewed by humans in PRs. A diff that says "row 3 col 5: 'F' → 'f'" is instantly legible; a diff in a JSON-encoded cell array is not. The verbose form is opt-in, only used when attrs matter.

### Test driver

A C harness in `tests/unit/palette_render_test.c`:

```c
// Skeleton. Each test builds a palette_state_t, renders, compares to fixture.

static void test_layer1_frontier_menu_open(void) {
    palette_state_t s;
    palette_state_init_for_test(&s);
    palette_open_menubar(&s, "Frontier");  // opens Layer 1's Frontier menu

    palette_cell_t cells[24 * 80];
    palette_frame_t frame = { .rows = 24, .cols = 80, .cells = cells };
    palette_render(&s, &frame);

    assert_frame_matches_fixture(&frame, "fixtures/palette/frontier_menu_open.txt");
}
```

`assert_frame_matches_fixture` does:

1. Read fixture file (and sidecar `.attrs` if present).
2. Compare cell-by-cell.
3. On mismatch: print a unified diff highlighting changed cells (row, col, expected, actual). Fail the test.
4. On `FRONTIER_UPDATE_GOLDENS=1`: rewrite the fixture instead of failing. (Used once, reviewed in the PR.)

The golden-update environment variable is a project convention copied from snapshot-test frameworks. Behavior must be opt-in and impossible to trigger accidentally.

### Scenarios to cover (initial set; add as bugs surface)

- `frontier_menu_closed.txt` — just the menubar
- `frontier_menu_open.txt` — Frontier menu cascaded
- `frontier_file_open.txt` — File menu cascaded (different x-position)
- `cascade_two_levels.txt` — top + submenu visible (verbose, asserting parent stays bright)
- `cascade_right_overflow.txt` — submenu would overflow → opens left
- `cascade_bottom_overflow.txt` — submenu would overflow → opens up
- `hotkey_underlines.txt` (verbose) — underlines on the right characters per per-layer-scope rule
- `selected_row_inverse.txt` (verbose)
- `disabled_item_dim.txt` (verbose) — items with `.enabled=false`
- `scrollback_pane_during_render.txt` — async log line landed in scrollback while menu was open

Each fixture has a comment header explaining the scenario:

```
# Scenario: Frontier menu opened from menubar. Selection on first item (About).
# Generated by test_layer1_frontier_menu_open.
# Renders expected: 24 rows x 80 cols. Underlines: F (Frontier), A (About).
```

---

## Layer 2: Mouse / key parser unit tests

### What we're asserting

Two parsers, one file each:

- **SGR 1006 mouse decoder.** Bytes in, `mouse_event_t {x, y, button, action, modifiers}` out. Round-trip every legal sequence; reject every illegal one with a defined error code.
- **ESC disambiguator.** The hard problem. A bare ESC means "close the deepest pane" (palette nav). An ESC followed (within ~10 ms) by `[` starts a CSI/SS3 sequence. A bare ESC must not be lost; a CSI must not be mis-processed as ESC + `[`.

### SGR 1006 decoder tests

Single-table-driven test:

```c
static const struct {
    const char *bytes;
    int expected_x, expected_y;
    uint8_t expected_button;
    enum mouse_action expected_action;
    int expected_consumed;  // bytes consumed; 0 = parse failed
} sgr1006_cases[] = {
    { "\x1b[<0;10;5M",   10, 5, 0, MOUSE_PRESS,   strlen("\x1b[<0;10;5M")   },
    { "\x1b[<0;10;5m",   10, 5, 0, MOUSE_RELEASE, strlen("\x1b[<0;10;5m")   },
    { "\x1b[<64;1;1M",    1, 1, 64, MOUSE_PRESS,  strlen("\x1b[<64;1;1M")   },  // wheel up
    { "\x1b[<65;1;1M",    1, 1, 65, MOUSE_PRESS,  strlen("\x1b[<65;1;1M")   },  // wheel down
    { "\x1b[<0;-1;-1M",   0, 0, 0, 0,             0                          }, // reject negative
    { "\x1b[<0;10;5",     0, 0, 0, 0,             -1                         }, // need-more
    { "\x1b[<garbage",    0, 0, 0, 0,             0                          }, // reject
    /* ...row per case... */
};
```

The parser is incremental — bytes arrive one at a time from `read()`. The `-1` "need-more" return path must be tested explicitly. A common bug class is "consumed N bytes on the first call but failed to remember partial state on the next call." The harness drives the parser one byte at a time and asserts the cumulative result.

### ESC disambiguation tests

The disambiguator is a small state machine:

- IDLE → got 0x1B → ESC_PENDING (start timer)
- ESC_PENDING + got `[` → CSI_BUILDING
- ESC_PENDING + got non-`[` → emit `KEY_ESC` + reprocess byte
- ESC_PENDING + 10 ms elapsed → emit `KEY_ESC` (timeout fires bare-ESC)
- CSI_BUILDING + final byte (e.g. `M` / `m` / `~`) → emit decoded key/mouse

The harness injects a fake clock:

```c
// palette_input.h
typedef int64_t (*palette_clock_fn)(void *userdata);  // monotonic ms

void palette_input_set_clock_for_test(palette_clock_fn fn, void *ud);
```

Tests advance the clock explicitly. No `usleep`, no real timers.

```c
static void test_bare_esc_emits_after_timeout(void) {
    fake_clock_ms = 0;
    palette_input_set_clock_for_test(get_fake_clock, NULL);

    palette_input_feed_byte(&parser, 0x1B);
    assert(palette_input_pending(&parser));            // not yet emitted

    fake_clock_ms = 9;
    palette_input_tick(&parser);
    assert(palette_input_pending(&parser));            // still pending at 9 ms

    fake_clock_ms = 11;
    palette_input_tick(&parser);
    assert(palette_input_event_ready(&parser));        // bare-ESC emitted at 11 ms
    assert(palette_input_pop(&parser).type == KEY_ESC);
}

static void test_esc_bracket_does_not_emit_bare_esc(void) {
    fake_clock_ms = 0;
    palette_input_feed_byte(&parser, 0x1B);
    fake_clock_ms = 1;
    palette_input_feed_byte(&parser, '[');
    fake_clock_ms = 2;
    palette_input_feed_byte(&parser, 'A');             // up arrow

    palette_input_tick(&parser);
    assert(palette_input_event_ready(&parser));
    assert(palette_input_pop(&parser).type == KEY_UP);

    fake_clock_ms = 100;                               // timeout long passed
    palette_input_tick(&parser);
    assert(!palette_input_event_ready(&parser));       // no spurious bare-ESC
}
```

This is the kind of timing test that cannot honestly be written against a real clock. Injecting the clock is non-negotiable.

### Why this is its own layer (not folded into L3)

The compositor doesn't care how the mouse event arrived; it cares where the click was. Putting parser tests in the compositor harness would couple two unrelated change-axes. SGR can evolve (we may want SGR 1015 or pixel-resolution mode later) without touching pane code; the pane API can evolve without touching parser code.

---

## Layer 3: Compositor unit tests

### What we're asserting

The pane compositor is the substrate from §1.3 of the implementation plan: `~600-800 LOC` managing a z-ordered list of rectangular panes with per-pane cell buffers. Tests exercise:

- `pane_register` / `pane_unregister` / `pane_move` / `pane_resize` — list invariants
- z-order: pane added with higher z renders on top; ties broken by insertion order
- diff-render: registering a pane and rendering twice with no changes emits zero CSI bytes
- `compositor_pane_at(x, y)` hit-test: returns topmost pane at the given cell
- async-output safety: a write to a registered "scrollback" pane during render lands in the right cells, never to stdout

### API sketch

```c
// pane_compositor.h

typedef struct pane pane_t;

typedef struct compositor compositor_t;

compositor_t *compositor_new(int rows, int cols);
void          compositor_free(compositor_t *c);

pane_t *compositor_register_pane(compositor_t *c, int x, int y, int w, int h, int z);
void    compositor_unregister_pane(compositor_t *c, pane_t *p);
void    compositor_move_pane(compositor_t *c, pane_t *p, int x, int y);
void    compositor_resize_pane(compositor_t *c, pane_t *p, int w, int h);
void    compositor_set_z(compositor_t *c, pane_t *p, int z);

// Write into a pane's cell buffer (caller computes coordinates relative to pane).
void    pane_set_cell(pane_t *p, int row, int col, palette_cell_t cell);

// Render: walk z-sorted, copy buffers into framebuffer, return dirty rect.
// Test-only render: emits into caller-supplied frame instead of stdout.
struct compositor_dirty compositor_render_to_frame(compositor_t *c, palette_frame_t *out);

// Production render: emits CSI to a writer callback. Returns bytes written.
typedef int (*compositor_writer_fn)(const char *bytes, size_t n, void *ud);
int     compositor_render(compositor_t *c, compositor_writer_fn w, void *ud);

// Hit-test.
pane_t *compositor_pane_at(compositor_t *c, int x, int y);

// Test-only inspection. Compiled in only when FRONTIER_PALETTE_TEST_HOOKS is defined.
#ifdef FRONTIER_PALETTE_TEST_HOOKS
size_t  compositor_pane_count(const compositor_t *c);
const palette_cell_t *compositor_peek_framebuffer(const compositor_t *c);
size_t  compositor_last_render_bytes(const compositor_t *c);  // CSI bytes emitted last call
#endif
```

### The test-hook decision

`FRONTIER_PALETTE_TEST_HOOKS` is a compile-time flag (added to `tests/Makefile` for the unit-test target only; never set in production builds). It turns on inspection accessors that would otherwise leak internal state to callers.

Three reasons over the alternative ("rewrite the compositor with a separately compiled mock"):

1. **One implementation, one source of truth.** The thing being tested is the same code that ships.
2. **Inspection is read-only.** No test-only mutation paths to forget about.
3. **Compiled out in production.** Zero footprint in `frontier-cli` builds.

The alternative — exposing the framebuffer always — was rejected because it would let user code reach into internals.

### Diff-render assertion

The diff-renderer is the trickiest part of the compositor. The test:

```c
static void test_unchanged_render_emits_zero_bytes(void) {
    compositor_t *c = compositor_new(24, 80);
    pane_t *p = compositor_register_pane(c, 5, 5, 10, 5, 0);
    pane_fill_with_test_pattern(p);

    int n1 = compositor_render(c, count_writer, NULL);
    assert(n1 > 0);

    int n2 = compositor_render(c, count_writer, NULL);
    assert(n2 == 0);                                   // truly idempotent

    compositor_free(c);
}
```

If `n2 != 0` the diff-renderer is dirtying cells that haven't changed — a real performance and (worse) flicker bug.

### Hit-test edge cases

```c
static void test_hit_test_picks_topmost(void) {
    compositor_t *c = compositor_new(24, 80);
    pane_t *bottom = compositor_register_pane(c, 0, 0, 20, 20, 0);
    pane_t *top    = compositor_register_pane(c, 5, 5, 10, 10, 1);

    assert(compositor_pane_at(c, 1, 1)  == bottom);   // outside top
    assert(compositor_pane_at(c, 6, 6)  == top);      // inside both, top wins
    assert(compositor_pane_at(c, 25, 25) == NULL);    // outside both
}
```

### Async-output-during-render

This is the load-bearing test for the §1.6 design. Background threads must NEVER write to stdout; they enqueue log lines, and the render thread drains the queue at frame boundary into a scrollback pane.

```c
static void test_async_log_lands_in_scrollback_not_stdout(void) {
    compositor_t *c = compositor_new(24, 80);
    pane_t *scroll = compositor_register_pane(c, 0, 0, 80, 20, 0);
    pane_t *menu   = compositor_register_pane(c, 10, 0, 20, 5, 10);  // higher z
    compositor_attach_log_sink(c, scroll);

    // Simulate a log line while the menu is open.
    compositor_enqueue_log_line(c, "background log: hello");

    char captured_stdout[4096] = {0};
    capture_stdout_for_test(captured_stdout, sizeof(captured_stdout));
    compositor_render(c, write_to_captured_stdout, NULL);

    assert(strstr(captured_stdout, "background log") == NULL);  // not on stdout
    // The string should land in the scrollback pane's cell buffer:
    assert(pane_contains_text(scroll, "background log: hello"));
}
```

The `capture_stdout_for_test` helper is harness-local; it routes `compositor_writer_fn` through a buffer rather than the real `write(STDOUT_FILENO, ...)`. (Production hooks the writer callback to `write` directly.)

---

## Layer 4: Integration tests via pexpect

### What we're asserting

Behavior visible to a real user driving a real `frontier-cli` REPL through a real PTY. This is where we catch:

- linenoise + palette interaction (does pressing `/` at column 1 actually engage the palette? Does it disengage cleanly?)
- the SIGWINCH path (does resizing the terminal mid-cascade redraw correctly?)
- the mouse mode toggle (does the terminal exit mouse-tracking mode after the palette closes? after the REPL exits? after Ctrl-C?)
- the `installHostMenubar.ut` boot path (Layer 1 is populated when the user first presses `/`)
- the legacy `/exit` etc. fallback (still works when the user types it as a command)

### Driver: extend `runner.py`, don't fork

`tests/integration/runner.py` already has `execute_interactive` — pexpect-based, used today for dialog prompts. It hits the right shape: spawn a CLI in a PTY, send/expect pairs, collect stdout.

**Decision: extend, don't fork.** Add three things:

1. **Optional `palette_mode: true`** at the YAML level — sets `TERM=xterm-256color` (so linenoise stays in raw mode and the palette can render), and sets `FRONTIER_PALETTE_FAST_TIMERS=1` (a CLI-only env var that compresses the 10ms ESC timeout to 1ms for test determinism).
2. **A `send_mouse:` step type** alongside `send:`, taking `{x, y, button, action}` and emitting the SGR 1006 byte sequence directly to the PTY.
3. **A `screenshot_match:` assertion** that captures the current PTY buffer state and compares to a golden frame. Uses an existing terminal-emulation library (vt100-ish) on the test side to interpret the CSI stream the CLI emits, then runs the same fixture comparison as Layer 1.

```yaml
# tests/integration/test_cases/repl_palette.yaml
tests:
  - name: "palette opens on / at column 1 of empty input"
    palette_mode: true
    interactive_steps:
      - expect: "Frontier> "
        send: "/"
      - expect: ""                       # no prompt; palette is rendering
        screenshot_match: "fixtures/palette/menubar_just_opened.txt"
        timeout: 1
      - send: "\u001b"                   # ESC: close palette
      - expect: "Frontier> "
        send: "/exit"

  - name: "ESC at submenu pops one level, not the whole palette"
    palette_mode: true
    interactive_steps:
      - expect: "Frontier> "
        send: "/"
      - send: "F"                        # File hotkey
      - send: "O"                        # File > Open Database
      - send: "\u001b"                   # ESC: pop submenu
        screenshot_match: "fixtures/palette/file_menu_open.txt"
      - send: "\u001b"                   # ESC again: close palette
      - expect: "Frontier> "
        send: "/exit"

  - name: "mouse click on menubar opens that menu"
    palette_mode: true
    interactive_steps:
      - expect: "Frontier> "
        send: "/"
      - send_mouse: { x: 12, y: 1, button: 0, action: press }
      - send_mouse: { x: 12, y: 1, button: 0, action: release }
        screenshot_match: "fixtures/palette/file_menu_via_click.txt"
      - send: "\u001b"
      - expect: "Frontier> "
        send: "/exit"

  - name: "mouse mode disabled cleanly on palette close"
    palette_mode: true
    interactive_steps:
      - expect: "Frontier> "
        send: "/"
      - send: "\u001b"
      - expect: "Frontier> "
        # The teardown CSI \e[?1006l\e[?1000l must have been emitted before the prompt returned.
        # We capture-then-grep because the bytes flow through the PTY interleaved with the prompt.
        emitted_csi_contains: "\u001b[?1000l"
        send: "/exit"
```

Three new YAML keys: `palette_mode`, `send_mouse`, `screenshot_match`, `emitted_csi_contains`. Each is small and explicit; together they cover what the integration layer needs without inventing a DSL.

### `FRONTIER_PALETTE_FAST_TIMERS=1`

The 10 ms ESC disambiguation window is the only piece of palette behavior with real wall-clock semantics. In production it's 10 ms; in tests, even with a deterministic fake clock at L2, the integration layer is at the mercy of the OS scheduler. Compressing to 1 ms in tests cuts integration runtime sharply without changing observable behavior — every test that drives ESC follows it with `tick`-equivalent waiting that's >1 ms in practice anyway. This env var only takes effect when `FRONTIER_PALETTE_TEST_HOOKS` is compiled in, so production builds can't accidentally enable it.

### Terminal emulation on the test side

`screenshot_match` requires interpreting the CSI stream the CLI emits. Two options:

1. **Vendor `pyte`** (~3k LOC pure Python) — interprets ANSI escapes into a 2D character grid. Used by other test harnesses for the same purpose. Already on PyPI; could vendor or list as optional.
2. **Hand-roll a minimal interpreter** (~300 LOC). Risk: every CSI we emit (cursor positioning, SGR, mouse mode, scroll region) needs handling. Reinventing a wheel.

**Recommend pyte** (vendored under `tests/vendor/pyte/`, MIT-licensed). The integration runner already vendors pexpect; same pattern. Worth ~50 lines of Python glue to convert pyte's screen state into the same `palette_frame_t` ASCII format Layer 1 uses, so fixtures are interchangeable across L1 and L4.

### When to use each layer

A given bug can usually be reproduced at multiple layers. The rule of thumb:

| Class of bug | Layer | Why |
|---|---|---|
| Wrong character / wrong attribute on a cell | L1 | Cheap, exact, no PTY noise |
| Wrong layout when overflowing the right edge | L1 | State-only; no terminal needed |
| Mouse coordinate off-by-one | L2 + L3 | Parser → hit-test |
| Diff-render emits CSI when nothing changed | L3 | Compositor-internal |
| ESC ambiguity in real terminal | L4 | Only PTY exposes the timing surface |
| Palette doesn't open on `/` at column 1 | L4 | Linenoise integration only happens in real REPL |
| Mouse mode leak on Ctrl-C | L4 | Only signals work in a real process |

Push every test as far down the layer stack as the bug permits.

---

## Where each piece lives

```
tests/
  unit/                                       # existing dir; new C tests join
    palette_render_test.c                     # L1 (new)
    palette_input_test.c                      # L2 (new)
    pane_compositor_test.c                    # L3 (new)
  fixtures/
    palette/                                  # NEW
      frontier_menu_open.txt
      frontier_menu_open.attrs                # sidecar; only when attrs matter
      cascade_two_levels.txt
      menubar_just_opened.txt
      file_menu_open.txt
      file_menu_via_click.txt
      ...
  integration/
    runner.py                                 # extend with send_mouse / screenshot_match
    test_cases/
      repl_palette.yaml                       # L4 (new)
      repl_palette_mouse.yaml                 # L4 (new)
    vendor/
      pyte/                                   # NEW; vendored
  Makefile                                    # add new unit-test targets
```

The new harness ships behind the same `make test-integration` and `./tools/run_headless_tests.sh` entry points — no new top-level commands. The /auto Test Manifest in `CLAUDE.md` doesn't change.

---

## Build wiring

**Unit tests (L1, L2, L3).** Each is a small C executable linked against:

- `frontier-cli/palette.c` (new in PR 5)
- `frontier-cli/pane_compositor.c` (new in PR 4)
- `frontier-cli/palette_input.c` (new in PR 4)
- `tests/test_report.h` (existing, lightweight assertion macros)
- `Common/source/memory.c` and a handful of portable helpers for any types the compositor uses

The unit-test executables are compiled with `-DFRONTIER_PALETTE_TEST_HOOKS`, which the production CLI build doesn't set. Hooks compile out cleanly via `#ifdef`.

A new line in `tests/Makefile`:

```makefile
PALETTE_UNIT_TESTS = palette_render_test palette_input_test pane_compositor_test

palette_render_test: palette_render_test.c \
                     ../frontier-cli/palette.c ../frontier-cli/pane_compositor.c
	$(CC) $(CFLAGS) -DFRONTIER_PALETTE_TEST_HOOKS \
	    -o $@ $^ $(LINK_LIBS)

# (analogous for the other two)

test-palette-unit: $(PALETTE_UNIT_TESTS)
	@for t in $(PALETTE_UNIT_TESTS); do ./$$t || exit $$?; done
```

`./tools/run_headless_tests.sh` already iterates the unit-test targets in `tests/Makefile`; adding the three new binaries to its list is a one-line change.

**Integration tests (L4).** No build change. The runner discovers `tests/integration/test_cases/repl_palette*.yaml` automatically.

---

## Risk register

| Risk | Mitigation |
|---|---|
| Goldens grow stale and `FRONTIER_UPDATE_GOLDENS=1` becomes the only way anyone runs the tests | Required PR-template line: "Did you regenerate any palette goldens? If yes, link the visual diff." Reviewer enforces. |
| pyte rendering diverges from real xterm in some edge case (selective backspace etc.) we depend on | Keep L4 small (cascade open/close, hotkey, ESC, click). Don't try to assert subtle terminal behavior in pyte. |
| Diff-render byte-count assertion (`n2 == 0`) is too strict — small no-op writes (cursor reposition) might leak through | Allow `n2 < 8` if leak is provably-CSI-cursor-only. Ratchet down once compositor stabilizes. |
| Test-hook accessors leak into production through a typo | CI build of `frontier-cli` runs `nm` and asserts no `compositor_peek_*` symbols are exported. Cheap. |
| Layer 4 is flaky on slow CI | Use `FRONTIER_PALETTE_FAST_TIMERS=1`. If still flaky, increase pexpect timeouts but never increase wall-clock waits. |

---

## Open questions

These are real architecture decisions where I see the tradeoffs but want a discussion before committing.

1. **Sidecar `.attrs` vs unified rich-fixture format.** The proposal is plain text + optional `.attrs`. An alternative is a single file per fixture with framing characters that encode attrs (e.g. lowercase letters for underlined chars on otherwise-uppercase regions, or a Unicode combining underline on each underlined char). Sidecar is simpler to diff but doubles the file count. **Default**: sidecar `.attrs`, only when needed. Single-file is tempting for terseness but encodes attribute information into the same character stream that's already encoding glyphs, which makes goldens harder to read.

2. **Pyte vs hand-rolled terminal emulator for L4.** Pyte is ~3k LOC of Python that we'd vendor; hand-rolled is ~300 LOC that only handles the CSI subset we emit. Pyte is correct on every edge case we'll never hit; hand-rolled is correct on exactly the cases we hit, with the risk that an unexpected CSI from linenoise is silently misinterpreted. **Default**: pyte. The simplicity argument loses to "matches real terminal behavior," and `tests/vendor/` precedent is established.

3. **Whether `screenshot_match` belongs at L4 at all.** If L1 is comprehensive, L4 may not need to assert frames — only behavior (cascades opened, the right CSI emitted, mouse mode toggled). Frame-comparison at L4 risks duplicating L1 coverage. **Default**: ship `screenshot_match` for the boot scenario only (one fixture: `menubar_just_opened.txt`), to prove that the whole pipeline from real boot → real linenoise → real palette renders right. Other L4 tests assert behaviorally (`expect`, `emitted_csi_contains`).

4. **Fake clock injection for L2 vs accepting that L2 takes 10ms per ESC test.** Injection is cleaner; the alternative is "accept 10ms × ~6 ESC tests = 60ms total, no clock plumbing." 60ms is acceptable but builds discipline matters: a clock-dependency in palette code is exactly the thing TDD-time-injection patterns exist to handle. **Default**: inject the clock. Sets a precedent for any future palette timing (animation, double-click) so we don't end up with a real-clock test suite later.

5. **Does the compositor expose the framebuffer or only a render-into-frame call.** Two flavors of test hook: (a) `compositor_peek_framebuffer` returns a pointer to internal state — fast, but exposes internals; (b) `compositor_render_to_frame` re-renders into a caller-supplied buffer — slower, but no internal exposure. **Default**: (b). The compositor still has its own internal framebuffer for diff-render purposes; tests just ask it to also render into a test-supplied frame, which is the same operation it does in production minus the CSI emit.

6. **Where does the "host menubar built by `installHostMenubar.ut`" get tested?** That's a UserTalk script + ODB state, not pure rendering. Two options: (a) integration test only — boots a real CLI, types `/`, asserts the menubar shape — flaky-prone but real; (b) unit test that fakes the ODB to look like post-installHostMenubar state, then drives palette open against synthetic data. **Default**: both. (b) at L1 (deterministic, fast — covers the rendering-against-Layer-1-data path) and (a) at L4 as a single end-to-end smoke (proves the script + palette actually compose). PR 6 owns the integration test; PR 5 owns the unit test using a synthetic Layer-1 outline.

7. **How to test mouse-mode-leak-on-Ctrl-C without actually sending SIGINT to the test process.** Sending SIGINT to a pexpect child is fine, but asserting "the terminal disable sequence was emitted" is awkward when SIGINT also kills the child mid-emit. Two paths: (a) wrap the disable sequence in an `atexit` AND a SIGINT handler — test with both `child.sendintr()` and `child.terminate()` and assert the trailing PTY bytes contain `\e[?1000l`; (b) skip — accept that this is a manual smoke test. **Default**: (a). Mouse-mode leak is the most-visible-when-it-fails palette bug ("my terminal is now mysteriously broken"), and it deserves to fail loud in CI.

8. **Whether to test against the production palette code or a simplified harness shim.** The temptation when the test is hard is to test a stub. The correct answer is to test the production code with synthetic inputs. **Default**: no shims. Every test in this design drives the real functions. If a function is hard to drive, that's a refactor signal.

9. **Whether L1 fixtures should pin the terminal size.** A 24×80 fixture is meaningless if the production palette decides at runtime to render at 30×100 (different overflow behavior, different cascade positions). **Default**: every L1 fixture explicitly states its `(rows, cols)` at the top, and the test passes that size to `palette_render`. L4 sets the PTY size explicitly via `child.setwinsize`. No reliance on inherited environment.

---

## Implementation order

The harness ships as one PR ("Plan 1") between PR 3 (terminal_control extensions) and PR 4 (compositor). Order within Plan 1:

1. Add `tests/fixtures/palette/` and `tests/vendor/pyte/`. Empty fixtures; pyte vendored as-is.
2. Add `tests/unit/palette_render_test.c` with one passing trivial test (`palette_render` of empty state produces all-blank frame). Establishes the harness wiring; wires `make test-palette-unit`.
3. Add `tests/unit/palette_input_test.c` with the SGR table-driven test. Establishes fake clock pattern.
4. Add `tests/unit/pane_compositor_test.c` with `register / unregister / pane_count` tests. Establishes the test-hook compile flag.
5. Extend `runner.py` with `palette_mode`, `send_mouse`, `screenshot_match`, `emitted_csi_contains`. Add one passing trivial L4 test (REPL boots, `/` opens *something*).
6. PR 4 (compositor) lands its real implementation; the L3 tests written in step 4 turn green.
7. PR 5 (palette) lands; L1 + L2 + L4 tests written in steps 2/3/5 turn green; new fixtures land alongside palette features.

Plan 1 is **structural**, not behavioral. It ships a harness that can fail meaningfully but doesn't ship comprehensive coverage — that arrives PR-by-PR alongside the features being tested. The success criterion for Plan 1 is: PR 4 and PR 5 cannot land without their layer's tests passing.

---

## Acceptance criteria for Plan 1 itself

- `tests/unit/palette_render_test.c`, `tests/unit/palette_input_test.c`, `tests/unit/pane_compositor_test.c` exist, build, and pass.
- `tests/fixtures/palette/` exists with a README explaining the format (frame + optional sidecar attrs).
- `tests/vendor/pyte/` exists and is importable from `runner.py`.
- `runner.py` understands `palette_mode`, `send_mouse`, `screenshot_match`, `emitted_csi_contains`. Each has a unit test in `runner.py`'s own self-test (yes, the runner has tests).
- `./tools/run_headless_tests.sh` runs the three new unit-test binaries.
- `cd tests && make test-integration` runs at least one new YAML test that exercises the end-to-end pipeline (boot → `/` → fixture-match → ESC → `/exit`).
- A PR template line exists ("regenerated palette goldens? link visual diff").
- A documentation note in `docs/TESTING_GUIDE.md` (or a new `docs/PALETTE_TEST_HARNESS.md`) explaining the four layers and when to add tests at each.

When all of the above are true, Plan 1 is mergeable. PR 4 then opens against this foundation.
