# boxen — Overview

A portable window manager library for terminal applications. Built first as the substrate for Frontier's text-mode UI; designed for extraction as a standalone open-source library once the API has earned a v1.0 stamp.

Backed by [termbox2](https://github.com/termbox/termbox2). The backend layer is small, well-defined, and swappable: boxen's public API is the stability commitment, the backend is implementation detail.

---

## 1. Motivation

Frontier needs a text-mode UI layer. The first concrete need is an interactive UserTalk debugger TUI (issue #691) with a classic-Frontier flavor: script pane on the left, stack-and-locals pane on the right, keybind footer at the bottom, cmd-2-click identifier-to-frame-value as the killer feature. Beyond the debugger, the planned surface set includes table editor, outline editor, WPText editor (markdown round-trip, RTF-canonical storage), XML editor over compiled XML tables, menu editor with attached script editors, and a status-bar surface for `msg()` allegorical to legacy Frontier's "About" window.

Six-plus distinct TUI surfaces in the medium-term horizon. Each needs windows, z-order, movable/resizable panes, scrolling content, and a coherent input model. termbox2 gives us a cell grid and input events; everything above is substrate we have to provide.

The alternative substrates were considered:

- **notcurses**: technically the strongest option for macOS+Linux, but Windows support is experimental and the runtime dependency chain (libunistring, optionally libdeflate, FFmpeg, etc.) is significant even for a surgical build. Frontier has classic-Mac heritage where Windows was a real platform; preserving Windows as a target tilts the substrate decision.
- **Hand-rolled ANSI**: viable for a single surface, painful for six. Reinventing the same five primitives per surface is the worst case.
- **termbox2 + boxen**: termbox2 has a maintained Windows port (`termbox2-WIN`) that works in standard Windows shells without depending on ConPTY. The substrate we'd build on top is bounded (~1000 LOC, see Section 3) and small relative to the surface code that will sit on it (estimated 5,000-10,000 LOC across six surfaces). The substrate becomes ~10-15% of total UI code, and the most reusable part.

**Decision (2026-06-06)**: Build boxen on termbox2. Design the backend interface to be swappable so a future move to notcurses (or anything else) does not break consumers.

---

## 2. Frontier Surface Inventory

What boxen has to serve. Each surface drives a different mix of primitives; the union defines the substrate's feature set.

| Surface | Status | What it needs from boxen |
|---------|--------|--------------------------|
| **Debugger TUI** (#691) | first consumer | Two panes (script + stack/locals), keybind footer, scrollable script content with highlighted current-line, cmd-2-click identifier resolution (hits the runtime, returns a frame address, jumps the stack pane to that frame and highlights the value), modal overlay for breakpoint/watchpoint config |
| **Outline editor** | second consumer (validates extraction) | Scrollable nested content with collapse/expand, in-place text editing, focus model that handles "edit the current node's text" vs "navigate the structure" |
| **Table editor** | future | Grid view with row/column scrolling, in-cell editing, column resizing, large-data virtualization (don't draw rows you can't see) |
| **WPText editor** | future | Multi-line text edit with soft-wrap reflow, paragraph styles surfaced in markdown view, status bar showing canonical-RTF / markdown view toggle |
| **XML editor** | future | Tree view over compiled XML tables; similar to outline editor but with attribute editing semantics |
| **Menu editor** | future | Nested editors (a menu opens a script editor on top), modal-within-modal focus stack |
| **Status bar / `msg()`** | future | Floating non-modal surface that doesn't fight the main pane for input; classic Frontier's "About" allegory |

The surfaces share more than they differ. The recurring primitives are: window with border + title, scrollable content region, focus model, keybind footer, modal overlay, and a discipline for "this surface owns these key bindings while focused." Every surface above is implementable on the boxen primitives listed in Section 3.

---

## 3. Architecture

Three layers, clearly separated.

```
+-----------------------------------+
|     Frontier surfaces             |   (consumer code: debugger, outline, etc.)
|     - debugger_tui.c              |
|     - outline_tui.c               |
|     - ...                         |
+-----------------------------------+
|     boxen public API              |   (stable; v1.0 commitment)
|     boxen_window_t,               |
|     boxen_open, boxen_close,      |
|     boxen_resize, boxen_scroll,   |
|     boxen_poll, etc.              |
+-----------------------------------+
|     boxen core                    |   (window manager logic; backend-agnostic)
|     - window list, z-order        |
|     - redraw cycle                |
|     - clipping                    |
|     - input dispatch              |
+-----------------------------------+
|     boxen backend interface       |   (small vtable; swappable)
|     boxen_backend_t               |
+-----------------------------------+
|     concrete backends             |
|     - boxen-tb2  (termbox2)       |   <-- first and only initial backend
|     - (future) boxen-nc            |   <-- notcurses shim
|     - boxen-mock (for tests)      |
+-----------------------------------+
```

### 3.1 The four primitive layers

What we build on top of termbox2's cell grid:

| Layer | Estimated LOC | What it provides |
|-------|---------------|------------------|
| Window primitive + region clipping | ~200 | `boxen_window_t`, `boxen_set_cell` with window-local coordinates and automatic clipping at window borders |
| Z-order and redraw cycle | ~100 | Sorted window list, back-to-front redraw, `boxen_raise` / `boxen_lower` / `boxen_focus` |
| Resize and move (state machine) | ~250 | Mouse-drag and keyboard-driven move/resize, hit-testing for title bar / corners, minimum-size enforcement, terminal-resize handling |
| Scrolling content (logical model) | ~150 | Window stores `scroll_y` / `scroll_x`; surfaces provide a "draw line N here" callback; ensure-visible helpers, scrollbar rendering |
| Borders / titles / focus chrome | ~80 | Decoration helpers shared by all windows |
| Modal handling | ~30 | Modal flag traps input until close |
| Layout presets | ~80 | `split_horizontal`, `split_vertical`, `stack` for surfaces that want common layouts |
| Window-list helpers | ~50 | find-by-id, find-topmost-at-point, iteration |
| termbox2 backend shim (`backend_tb2.c`) | ~200 | Implement the 11 backend functions on top of termbox2; only file allowed to call `tb_*` |
| Unicode width (`boxen_wcwidth.c`) | ~150 | Vendored public-domain wcwidth (see execution plan); needed for CJK/emoji-aware cell layout |
| Logging hook shim (`boxen_log.c`) | ~50 | No-op-default log hook with vtable for embedders |

Total under `frontier-cli/boxen/`: roughly 1300-1400 LOC of focused C. Substrate logic alone is ~940 LOC; the backend/wcwidth/log files bring the total above the original ~1000 estimate. Still small relative to the projected surface-code total (5,000-10,000 LOC across six surfaces).

### 3.2 Backend interface

The backend interface is the load-bearing abstraction that makes boxen's API outlast any specific terminal library. It is intentionally small.

```c
typedef struct boxen_backend {
    int  (*init)(void *config);
    void (*shutdown)(void);
    int  (*width)(void);
    int  (*height)(void);
    int  (*color_depth)(void);                   // 1, 4, 8, 24 — terminal color capability
    void (*set_cell)(int x, int y, uint32_t ch,
                     uint16_t fg, uint16_t bg, uint16_t attr);
    void (*set_cursor)(int x, int y);
    void (*set_cursor_visible)(bool visible);
    void (*present)(void);
    int  (*poll_event)(boxen_event_t *out, int timeout_ms);
    void (*clear)(void);
} boxen_backend_t;
```

Eleven functions. The `attr` field carries cell-level style flags (bold, underline, reverse, italic) — termbox2 supports all four, and the debugger's current-line highlight and stack-frame focus markers will need at least reverse. `color_depth()` lets boxen adapt its palette to terminal capability (the debugger color scheme degrades gracefully on a 16-color terminal). `set_cursor_visible()` is needed for the debugger's command-input focus management.

termbox2 implements every backend function in fewer than 250 LOC of shim code. notcurses can implement them as a thin wrapper around its planes API if we ever go that direction. A mock backend for unit tests captures cell writes to an in-memory 2D array — no terminal needed in CI.

**Discipline**: nothing in boxen core may call `tb_*` directly. All terminal access goes through the backend interface. This discipline is what makes the swap viable.

### 3.3 What boxen does **not** provide

Honest scope discipline. Things consumers will sometimes want, that boxen won't offer (at least in v1.0):

- **Text widgets** (multi-line edit, single-line edit, password prompt). Each Frontier surface has unusual data models (ODB tables, outline nodes, WPText with markup); a generic text widget would either be too general or too specific to anyone. Surfaces implement their own text editing on top of boxen's scrolling + input dispatch primitives.
- **Themes / styling systems**. v1.0 ships with a small fixed palette and a discipline that surfaces pass colors explicitly. Theming can be added later without breaking the API.
- **Built-in scrollback** for content larger than memory. Surfaces that need this (e.g., a log viewer) implement their own paging.
- **High-quality CJK / emoji width**. v1.0 uses a public-domain wcwidth implementation. Edge cases will be wrong; this is a known trade-off vs notcurses.
- **Sub-pixel graphics, images, video**. Irrelevant.

---

## 4. API Shape (First Draft)

A first cut at the public surface. The system-architect will refine this in the execution plan; what's below is the shape to react to, not the final spec.

### Types

```c
typedef struct boxen_window  boxen_window_t;  // opaque
typedef struct boxen_event   boxen_event_t;
typedef struct boxen_rect    { int x, y, w, h; } boxen_rect_t;
typedef enum   boxen_color   { ... }            boxen_color_t;
typedef int    boxen_result_t;                  // 0 = ok, negative = error

typedef void (*boxen_draw_fn)(boxen_window_t *win, void *user_data);
typedef int  (*boxen_input_fn)(boxen_window_t *win, const boxen_event_t *ev, void *user_data);
```

### Lifecycle

```c
boxen_result_t  boxen_init(const boxen_backend_t *backend, void *backend_config);
void            boxen_shutdown(void);

// Lower-level primitives — Frontier surfaces use these to interleave boxen
// with Frontier-runtime events (async debug notifications, UserTalk callbacks).
boxen_result_t  boxen_poll(boxen_event_t *out, int timeout_ms);
void            boxen_present(void);             // redraw all visible windows

// Convenience main loop for standalone programs (examples/, eventual non-Frontier
// consumers). Wraps poll + dispatch + present + the quit flag. Frontier surface
// code does NOT call this — they call poll/present directly.
boxen_result_t  boxen_run(void);
void            boxen_quit(void);                // request main loop exit
```

The split between `boxen_run()` (convenience) and `boxen_poll()` + `boxen_present()` (primitives) is deliberate. Frontier surface code needs the primitives so it can interleave terminal events with Frontier-runtime callbacks under the GIL discipline. Standalone consumers (the `examples/` programs, any future non-Frontier consumer) use `boxen_run()` for the typical case.

### Window management

```c
boxen_window_t *boxen_window_open(const char *title, boxen_rect_t rect, void *user_data);
void            boxen_window_close(boxen_window_t *win);

void            boxen_window_set_draw(boxen_window_t *win, boxen_draw_fn fn);
void            boxen_window_set_input(boxen_window_t *win, boxen_input_fn fn);

void            boxen_window_set_title(boxen_window_t *win, const char *title);
void            boxen_window_set_rect(boxen_window_t *win, boxen_rect_t rect);
void            boxen_window_set_resizable(boxen_window_t *win, bool resizable);
void            boxen_window_set_movable(boxen_window_t *win, bool movable);
void            boxen_window_set_modal(boxen_window_t *win, bool modal);

void            boxen_window_raise(boxen_window_t *win);
void            boxen_window_lower(boxen_window_t *win);
void            boxen_window_focus(boxen_window_t *win);

void            boxen_window_invalidate(boxen_window_t *win);  // request redraw
```

### Drawing (inside a draw callback)

```c
void  boxen_set_cell(boxen_window_t *win, int x, int y,
                     uint32_t ch, boxen_color_t fg, boxen_color_t bg,
                     boxen_attr_t attr);
void  boxen_draw_text(boxen_window_t *win, int x, int y, const char *utf8,
                      boxen_color_t fg, boxen_color_t bg, boxen_attr_t attr);
void  boxen_fill_rect(boxen_window_t *win, boxen_rect_t rect,
                      uint32_t ch, boxen_color_t fg, boxen_color_t bg,
                      boxen_attr_t attr);
int   boxen_window_content_width(const boxen_window_t *win);   // w minus chrome
int   boxen_window_content_height(const boxen_window_t *win);
```

`boxen_attr_t` is a bitmask of style flags: `BOXEN_ATTR_BOLD`, `BOXEN_ATTR_UNDERLINE`, `BOXEN_ATTR_REVERSE`, `BOXEN_ATTR_ITALIC`. The debugger's current-line highlight and stack-frame focus markers will use these.

### Scrolling

```c
void  boxen_window_set_scroll(boxen_window_t *win, int scroll_x, int scroll_y);
void  boxen_window_scroll_by(boxen_window_t *win, int dx, int dy);
void  boxen_window_ensure_visible(boxen_window_t *win, int x, int y);
void  boxen_window_set_content_size(boxen_window_t *win, int w, int h);
```

### Input

```c
typedef enum {
    BOXEN_EV_KEY,
    BOXEN_EV_MOUSE,
    BOXEN_EV_RESIZE,
} boxen_event_type_t;

struct boxen_event {
    boxen_event_type_t type;
    union {
        struct { uint32_t key; uint32_t ch; uint16_t mod;             } key;
        struct { int x, y; uint8_t button; bool pressed; uint16_t mod; } mouse;
        struct { int w, h;                                             } resize;
    };
};
```

The `mod` field on mouse events is load-bearing: the debugger's cmd-2-click identifier resolution (Section 2) must detect Meta+double-click on a script-pane cell. Boxen defines its own portable `boxen_key_t` / `boxen_mod_t` enums; the backend shim translates from the underlying terminal library's representation. This is one of the cases where backend swappability requires a small up-front investment.

### Layout helpers

```c
boxen_window_t *boxen_layout_split_h(boxen_window_t *parent, float ratio,
                                     boxen_window_t **left, boxen_window_t **right);
boxen_window_t *boxen_layout_split_v(boxen_window_t *parent, float ratio,
                                     boxen_window_t **top, boxen_window_t **bottom);
```

That's ~35 functions, which feels right for v1.0. The system-architect should pressure-test this against the debugger TUI's specific needs in the execution plan.

---

## 5. Extraction Roadmap

Milestones, in order. Each is gated on the previous.

### Phase A — In-tree, single consumer (weeks 1-4)

- Vendor termbox2 (single header + single .c, MIT) into `frontier-cli/third_party/termbox2/`.
- Implement boxen core in `frontier-cli/boxen/` with `boxen.h` as the only public header.
- Implement the termbox2 backend as `boxen/backend_tb2.c`.
- Implement the mock backend as `boxen/backend_mock.c` for testing.
- Write behavioral unit tests using the mock backend.
- Write a `split_demo` example program that opens two side-by-side scrollable windows.
- Set up CI on macOS, Linux, and Windows from the first PR.

**Phase A is done when ALL of these are true** (binary, not fuzzy):

- ✅ Five test binaries exist: `boxen_core_tests`, `boxen_layout_tests`, `boxen_scroll_tests`, `boxen_input_tests`, `boxen_chrome_tests`
- ✅ At least 30 test functions across them, all green
- ✅ `split_demo` runs interactively on macOS and Linux (manual smoke test acceptable for Windows in Phase A)
- ✅ CI green on macOS, Linux, **and** Windows
- ✅ `grep -r 'tb_' frontier-cli/boxen/ | grep -v backend_tb2.c` returns empty (backend-abstraction discipline holds)
- ✅ `boxen.h` is the only public header (`boxen_internal.h` is in `frontier-cli/boxen/` but not `#include`-able from outside)

Until every box is checked, Phase A is not done. TDD is mandatory (per Frontier project policy); the test count is the floor, not the ceiling.

### Phase B — Debugger TUI as first real consumer (weeks 4-8)

- Build the debugger TUI on boxen. Two panes (script left, stack right), keybind footer, cmd-2-click identifier resolution, breakpoint toggle.
- Iterate on the boxen API in response to what the debugger actually needs. **Expect API churn here** — the first consumer always reveals API mistakes.
- At the end of this phase, the boxen API should be honest about what's stable and what's still in flux.

### Phase C — Outline editor as second consumer (weeks 8-14)

- Build the outline editor on boxen. This is the critical phase for substrate validation: a second surface with different needs (scrollable nested content, in-place editing) will surface API issues a single consumer doesn't reveal.
- API changes from this phase are the last expected churn before v1.0.
- **Quality bar**: at the end of this phase, both surfaces should run on the same boxen with no surface-specific hacks in boxen core.

### Phase D — Backend interface freeze (week 14)

- Lock the `boxen_backend_t` vtable. No changes after this point without a major version bump.
- Verify: write a stub notcurses backend (no need to fully implement) that compiles against the vtable. If it can't even compile, the vtable is wrong.

### Phase E — Extraction to standalone repo (week 15)

- Move `frontier-cli/boxen/` to a new repository: `boxen` (license: MIT, matching termbox2).
- Public artifacts: `boxen.h`, `boxen.c` (or a small set of `.c` files), `backend_tb2.c`, `backend_mock.c`, `examples/` (3-5 small programs: split panes, resizable window, scrolling text), `CHANGELOG.md`, README.
- CI on macOS, Linux, Windows.
- Frontier vendors boxen back as a git submodule, deletes `frontier-cli/boxen/` from its own tree.
- Tag boxen v1.0.0.

### Phase F — Ongoing (post-v1.0)

- Frontier consumes future boxen surfaces (table editor, WPText, XML, menus) on a stable API.
- Other consumers (if any appear) get the same stability commitment.
- Future minor versions add features without API breakage; major versions for breaking changes only.

---

## 6. Open Questions

To resolve during the execution plan or early Phase A:

1. **Final location of boxen during Phases A-D**: `frontier-cli/boxen/` is the working name. The execution plan should confirm or correct.
2. **License**: MIT is the default (matching termbox2). Confirm before first public artifact.
3. **Windows CI**: we want Windows in CI from day one to prevent regressions. What's the cheapest CI setup for Windows native? (GitHub Actions has Windows runners.)
4. **Unicode width**: vendor a small `wcwidth.c` from a known-good public-domain implementation. Which one? (`mk_wcwidth` is the obvious candidate.)
5. **Coordinate convention**: 0-based or 1-based for the public API? Termbox2 is 0-based; matching is simplest.
6. **Naming convention for keys / modifiers**: define a portable enum, or pass through the backend's representation? Defining our own enum is more work but supports backend swap; pass-through saves code but couples the API to termbox2.
7. **Threading**: is boxen single-threaded only, or thread-safe? Frontier's GIL model means the answer is probably "single-threaded; consumer holds the GIL when calling boxen." Document explicitly.

---

## 7. Risks and Mitigations

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| termbox2 maintainer disappears | Low | Medium | Vendored copy + backend abstraction means we can swap without API change. |
| The 1000-LOC substrate estimate doubles in practice | Medium | Low-medium | If it doubles, we still come out ahead vs hand-rolled ANSI per surface. Budget tracking during Phase A surfaces this early. |
| API churn during Phase B is more disruptive than expected | Medium | Medium | Debugger TUI is the first consumer specifically *because* we expect churn there. Phase C is when we'd worry. |
| Windows behavior diverges from macOS / Linux | Medium | Medium | Windows in CI from day one; treat any divergence as a P1. |
| Backend abstraction leaks (consumers accidentally depend on termbox2 details) | Medium | High (long-term) | Lint discipline: grep boxen public API for any termbox-specific concepts. Document forbidden patterns. |
| Extraction reveals the API isn't actually clean | Low (if Phases B + C went well) | High | Don't extract until two real consumers run on the same API without hacks. |

---

## 8. What This Document Doesn't Cover

Deliberately deferred to the execution plan:

- Concrete file layout (`boxen/include/`, `boxen/src/`, etc.).
- Build-system integration (Frontier's Makefile, future standalone CMake).
- Test harness specifics (what assertions, how cells are inspected, how input is injected).
- Logging integration (Frontier has its own logging system; boxen needs to be quiet by default and offer a hook).
- Memory allocator policy (does boxen call malloc directly, take an allocator vtable, use a Frontier-provided allocator?).
- Error reporting conventions (errno-style integers? a thread-local error string? a callback?).
- Documentation generation (Doxygen? hand-written? both?).

These belong in `EXECUTION_PLAN.md` (system-architect to write next).
