# boxen — Execution Plan

Detailed sequencing for the boxen window manager library. This document resolves the open questions and deferred decisions from `OVERVIEW.md` and provides the concrete implementation guide that OVERVIEW deliberately leaves for the system-architect.

**Status**: Phase A not yet started. OVERVIEW approved 2026-06-06.

---

## 0. Relationship to OVERVIEW.md

This document refines but does not contradict OVERVIEW.md except in one case noted explicitly in Section 12. Read OVERVIEW first for motivation, surface inventory, and high-level architecture. This document takes those as given and fills in every concrete decision OVERVIEW defers.

---

## 1. File Layout (OVERVIEW Section 6, Question 1)

**Decision: `frontier-cli/boxen/` confirmed, with the directory tree below.**

The OVERVIEW suggestion is correct. `frontier-cli/boxen/` fits Frontier's existing conventions: `frontier-cli/` is where all interactive/TUI code lives (`pane.c`, `terminal_control.c`, `scrollback_pane.c`). Boxen is TUI substrate; it belongs here.

An alternative was `src/boxen/` at the repo root, separate from `frontier-cli/`, to signal early its extraction destiny. Rejected because: (1) Frontier has no `src/` top-level — adding one for one subsystem is inconsistent; (2) `frontier-cli/` already has mature conventions (Makefile integration, test patterns, header layout) that boxen should inherit during the in-tree phase; (3) extraction to a standalone repo is a file-copy operation at Phase E, so location during Phases A-D is not load-bearing.

### Directory tree

```
frontier-cli/boxen/
    boxen.h                 -- the ONLY public header; all consumers include this
    boxen_internal.h        -- private definitions; NOT installed, NOT part of API
    boxen.c                 -- window manager core (z-order, redraw, clipping, input dispatch)
    boxen_layout.c          -- layout preset helpers (split_h, split_v, stack)
    boxen_chrome.c          -- border/title/scrollbar decoration helpers
    boxen_wcwidth.c         -- Unicode width implementation (see Section 4)
    backend_tb2.c           -- termbox2 backend (links against termbox2; only file allowed to use tb_*)
    backend_mock.c          -- in-memory mock backend for unit tests
    boxen_log.c             -- logging hook shim (see Section 9)

frontier-cli/third_party/termbox2/
    termbox2.h              -- single-header (the canonical termbox2 distribution)
    termbox2.c              -- single-file .c (include guard pattern; one TU owns the definition)

tests/
    boxen_core_tests.c      -- window lifecycle, z-order, clipping, modal
    boxen_layout_tests.c    -- split_h, split_v, resize callbacks
    boxen_scroll_tests.c    -- scroll_by, ensure_visible, content_size tracking
    boxen_input_tests.c     -- event dispatch, focus, modal input blocking
    boxen_chrome_tests.c    -- border rendering, title truncation, scrollbar rendering
```

The `tests/` directory is the existing `tests/` at the repo root, matching the established convention where every `frontier-cli/*.c` module has its test in `tests/*_tests.c`. The scrollback_pane_tests.c precedent is directly applicable.

**No `include/` or `examples/` subdirectory during Phases A-D.** Those are extraction artifacts for Phase E. During in-tree development, `boxen.h` lives directly in `frontier-cli/boxen/` and is referenced by a relative include path. At Phase E, the files reorganize into the standalone-repo layout: `include/boxen.h`, `src/boxen.c`, `examples/`.

**At Phase E, the standalone repo layout:**

```
boxen/
    include/boxen.h
    src/
        boxen.c
        boxen_layout.c
        boxen_chrome.c
        boxen_wcwidth.c
        backend_tb2.c
        backend_mock.c
        boxen_log.c
    third_party/termbox2/
        termbox2.h
        termbox2.c
    tests/
        (mirror of the above test files)
    examples/
        split_panes.c
        resizable_window.c
        scrolling_text.c
        modal_overlay.c
        input_echo.c
    CMakeLists.txt
    boxen.pc.in         -- pkg-config template
    CHANGELOG.md
    LICENSE
    README.md
```

---

## 2. License (OVERVIEW Section 6, Question 2)

**Decision: MIT. No alternatives.**

The reasoning in OVERVIEW is correct and complete. termbox2 is MIT. Frontier's existing third-party dependencies are MIT, BSD-2, and ISC — all permissive, compatible. MIT is the weakest-friction choice for the intended use case (open-source library consumed by anyone). Apache-2.0 adds patent clauses that are unnecessary overhead for a terminal library with no IP surface. BSD-2 would also be fine but MIT is already the de facto standard for small C utility libraries with termbox heritage.

**The `LICENSE` file must be committed in the same PR that introduces `boxen.h`.** Do not let the first public artifact ship without it.

---

## 3. Windows CI (OVERVIEW Section 6, Question 3)

**Decision: MSYS2/MinGW-w64 via GitHub Actions `windows-latest` runner, GCC toolchain.**

### Why not native MSVC

MSVC's C17 support is incomplete in meaningful ways for this codebase: VLAs, `_Generic`, and `<stdbool.h>` prior to C23 mode all have edge cases. Frontier already uses GCC/clang conventions and the codebase is written with POSIX-adjacent idioms. MSVC requires `.vcxproj` or CMake MSVC generator integration — a separate build system from the Makefile used on macOS/Linux. MSYS2+MinGW gives a GCC toolchain running on Windows, producing native Windows binaries, without a second build system.

### Build matrix commitment

```yaml
# .github/workflows/boxen.yml (sketch — final CI is out of scope for this plan)
strategy:
  matrix:
    include:
      - os: macos-14        # arm64; primary development platform
        cc: clang
      - os: ubuntu-24.04    # x86_64 Linux
        cc: gcc
      - os: windows-latest  # x86_64 Windows
        cc: gcc
        shell: msys2 {0}
        msys2-packages: mingw-w64-x86_64-gcc make
```

### termbox2 Windows

termbox2 has a Windows port that works with standard Windows Console API (no ConPTY required). The backend shim (`backend_tb2.c`) must be written so it compiles on Windows without POSIX-specific includes. No `<unistd.h>` or `<termios.h>` in `backend_tb2.c` — those are handled inside termbox2 itself.

### CI trigger scope during Phase A

Windows CI runs on every PR that touches `frontier-cli/boxen/`, `frontier-cli/third_party/termbox2/`, or `tests/boxen_*.c`. A Windows failure is a **P1 blocker** — do not merge. This discipline is what prevents "Windows port" from accumulating as a future project.

---

## 4. Unicode Width Function (OVERVIEW Section 6, Question 4)

**Decision: Vendor Markus Kuhn's `mk_wcwidth` from the canonical 2007 revision, with two adjustments.**

### Source

- Author: Markus Kuhn, University of Cambridge
- Canonical URL: https://www.cl.cam.ac.uk/~mgk25/ucs/wcwidth.c
- Published: 2007-05-26 (last public update from Kuhn's site)
- License: Public domain (explicit in file header: "Permission to use, copy, modify, and distribute this software for any purpose without fee is hereby granted.")

### Why this over alternatives

- **ICU `u_getIntPropertyValue(cp, UCHAR_EAST_ASIAN_WIDTH)`**: 30MB dependency. Absurd for a terminal library.
- **notcurses internal wcwidth**: Entangled with notcurses data structures; not extractable.
- **libunistring `uc_width()`**: A runtime dependency that breaks the zero-dependency goal.
- **hand-rolled from Unicode tables**: Maintenance nightmare when tables update.
- **Kuhn's mk_wcwidth**: Single .c file, public domain, well-known, used by virtually every terminal emulator that handles CJK. The Unicode tables it's based on are from Unicode 5.1; they are slightly stale (current is 15.1) but acceptable for v1.0. The known gap is some emoji added post-5.1 where width is 1 instead of 2; this is a documented limitation, not a correctness problem for the ASCII-heavy Frontier use case.

### Integration adjustments

1. **Rename**: Rename from `mk_wcwidth.c` to `boxen_wcwidth.c`. Export only `boxen_wcwidth(uint32_t ucs)` and `boxen_wcswidth(const uint32_t *pwcs, size_t n)`. Do not expose `mk_wcwidth` in the public API — the name is opaque. If we swap the implementation later, callers see no change.

2. **NULL guard**: The original returns `-1` for C0/C1 controls. Wrap it: `boxen_wcwidth` returns `1` for any codepoint where the underlying call returns `-1` and the codepoint is a printable ASCII control (0x20-0x7E), and `0` for non-printable controls. The backend renders non-printable controls as a space with an error attribute rather than emitting them raw.

---

## 5. Coordinate Convention (OVERVIEW Section 6, Question 5)

**Decision: 0-based throughout the public API.**

### Reasoning

The OVERVIEW correctly identifies the tension: termbox2 is 0-based, UserTalk ODB conventions are 1-based. The resolution is straightforward because **coordinate convention belongs to the API surface, not the data model**. Boxen coordinates are screen positions, not ODB table indices. Conflating these two coordinate spaces would be a confusion, not a consistency.

Concrete reasons for 0-based:

1. **termbox2 compatibility**: Every backend call maps directly with no offset arithmetic. Offset arithmetic in the hot path (per-cell rendering) is a correctness risk. Even a `+1/-1` mismatch in the clipping code produces off-by-one rendering bugs that are painful to debug.

2. **C array indexing**: `buf[y * w + x]` with y and x 0-based is the natural C model. All the existing pane code in Frontier (`pane.c`, `scrollback_pane.c`) uses 0-based coordinates.

3. **rect semantics**: `{ x=0, y=0, w=80, h=24 }` means "top-left is (0,0), spans 80 columns and 24 rows." With 1-based, the same rect would need to be `{ x=1, y=1, w=80, h=24 }` which is just confusing.

4. **Consumer code**: Surface code will do arithmetic on coordinates: `int mid = rect.x + rect.w / 2`. With 0-based this is obvious. With 1-based it requires thinking about what the base is.

**UserTalk surfaces that need to expose line numbers or row indices** (e.g., the debugger's "jump to line N" operation) will translate at the boundary: line number 1 in the script → scroll_y = 0 in the boxen window. This translation lives in the surface, not in boxen.

---

## 6. Key/Modifier Representation (OVERVIEW Section 6, Question 6)

**Decision: Define our own portable enum. Do not pass through termbox2 representations.**

### The wrong answer and why it's wrong

Pass-through is tempting: less code, works immediately. The problem is that `TB_KEY_*` constants and `TB_MOD_*` constants are termbox2's internal numbering, which is in turn derived from historical VT100 escape sequences. If we ever swap the backend (Phase D's notcurses stub exists precisely to validate this), every consumer must be updated. The API stability commitment is broken on day one.

More concretely: termbox2's `TB_KEY_CTRL_A` through `TB_KEY_CTRL_Z` are numbered 1-26, matching ASCII control codes. notcurses uses a different numbering for the same keys. A consumer that writes `if (ev.key.key == TB_KEY_CTRL_C)` has violated the backend abstraction rule from OVERVIEW Section 3.2.

### Design

```c
/* boxen.h — key definitions (abbreviated sketch) */

typedef enum {
    /* Printable range: not enumerated; use ev.key.ch (Unicode codepoint) */
    BOXEN_KEY_NONE = 0,

    /* Function keys */
    BOXEN_KEY_F1 = 0x10000, BOXEN_KEY_F2, /* ... */ BOXEN_KEY_F12,

    /* Navigation */
    BOXEN_KEY_UP, BOXEN_KEY_DOWN, BOXEN_KEY_LEFT, BOXEN_KEY_RIGHT,
    BOXEN_KEY_HOME, BOXEN_KEY_END, BOXEN_KEY_PGUP, BOXEN_KEY_PGDN,
    BOXEN_KEY_INSERT, BOXEN_KEY_DELETE,

    /* Editing */
    BOXEN_KEY_BACKSPACE, BOXEN_KEY_TAB, BOXEN_KEY_ENTER, BOXEN_KEY_ESCAPE,

    /* Control characters (named, not using ASCII values) */
    BOXEN_KEY_CTRL_A, BOXEN_KEY_CTRL_B, /* ... */ BOXEN_KEY_CTRL_Z,
    BOXEN_KEY_CTRL_BACKSLASH, BOXEN_KEY_CTRL_BRACKET,
    BOXEN_KEY_CTRL_SPACE,

} boxen_key_t;

typedef enum {
    BOXEN_MOD_NONE  = 0,
    BOXEN_MOD_ALT   = 1 << 0,
    BOXEN_MOD_CTRL  = 1 << 1,   /* separate from BOXEN_KEY_CTRL_* for cases
                                    where backend reports Ctrl separately */
    BOXEN_MOD_SHIFT = 1 << 2,
    BOXEN_MOD_META  = 1 << 3,   /* macOS Cmd / Windows Meta */
} boxen_mod_t;
```

The `key` field in `boxen_event_t` carries a `boxen_key_t` for special keys, zero for printable characters. The `ch` field carries the Unicode codepoint for printable characters (and is zero when `key` is a special key). The `mod` field is a bitmask of `boxen_mod_t`.

The `backend_tb2.c` shim translates `TB_KEY_*` → `BOXEN_KEY_*` and `TB_MOD_*` → `BOXEN_MOD_*`. This is ~50 lines of translation table. The cost is small; the benefit is real.

### Function keys with modifiers come as separate fields

Function-key + modifier combinations (Shift-F11, Alt-F5, etc.) are **always** delivered as `ev.key.key = BOXEN_KEY_Fn` with `ev.key.mod` carrying the modifier bitmask. Boxen does NOT define synthetic combined constants like `BOXEN_KEY_SHIFT_F11`. Consumers detect Shift-F11 by checking:

```c
if (ev.type == BOXEN_EV_KEY
    && ev.key.key == BOXEN_KEY_F11
    && (ev.key.mod & BOXEN_MOD_SHIFT)) {
    /* Shift-F11 — debugger TUI's step-out */
}
```

This matches what termbox2 delivers natively, avoids combinatorial enum explosion (n keys × m modifier combinations), and keeps the modifier semantics consistent across keyboard and mouse events. The debugger TUI's standard keybinds (F5 continue, F10 step-over, F11 step-into, Shift-F11 step-out, F9 toggle-breakpoint) all use this pattern.

### The cmd-2-click case

The debugger TUI needs "cmd-click" for identifier resolution. `BOXEN_MOD_META` is how this is expressed:

```c
if (ev.type == BOXEN_EV_MOUSE
    && ev.mouse.button == BOXEN_MOUSE_LEFT
    && (ev.mouse.mod & BOXEN_MOD_META)
    && ev.mouse.pressed) {
    /* cmd-click: resolve identifier at (ev.mouse.x, ev.mouse.y) */
}
```

Note: `boxen_event_t` for mouse events must also carry `mod`. OVERVIEW's draft has `uint8_t button; bool pressed` but no mod on mouse events. This is a gap in OVERVIEW — addressed in Section 11.

---

## 7. Threading Model (OVERVIEW Section 6, Question 7)

**Decision: Single-threaded. Boxen is not thread-safe. Contract documented explicitly.**

### The contract

```c
/*
 * Threading contract (boxen.h top-of-file comment)
 *
 * Boxen is single-threaded. All public API calls must be made from a
 * single thread. In Frontier, that thread is the GIL holder (the main
 * REPL thread or any UserTalk thread that holds the GIL).
 *
 * Boxen has no internal locks. Concurrent access from multiple threads
 * produces undefined behavior.
 *
 * Background threads that need to update display content (e.g., async
 * debug event arrival while the UserTalk evaluator is running) must
 * marshal their updates to the boxen thread using Frontier's existing
 * async output queue pattern (see repl_output_async.c) and call
 * boxen_window_invalidate() from the marshal target.
 *
 * The boxen_poll_event() call may block. The GIL must be released before
 * calling boxen_poll_event() with a non-zero timeout. Standard pattern:
 *
 *   lang_yield_gil();
 *   boxen_result_t r = boxen_poll_event(&ev, timeout_ms);
 *   lang_reacquire_gil();
 *   if (r == BOXEN_OK) { ... }
 */
```

### Why not thread-safe

Making boxen thread-safe would require locking around every window-list access, every cell write, and every event dispatch. This is a significant complexity increase in a library whose entire value is simplicity. The existing `pane.c` code in Frontier is explicitly documented as single-threaded for the same reasons. Frontier's threading model (cooperative GIL, yield points at `langbackgroundtask()`) means the natural usage pattern for a TUI surface is: hold GIL, do UI work, yield GIL while blocking on input. This is a clean contract.

If a future use case requires multi-threaded event delivery, the right pattern is a message queue at the application level, not locks inside boxen. The marshal pattern already exists in `repl_output_async.c` — boxen surfaces should use the same pattern.

### The boxen_run() main loop and GIL

The `boxen_run()` main loop must release the GIL around `boxen_poll_event()` to prevent the Frontier runtime from starving. This is not boxen's problem to solve — `boxen_run()` accepts a callback-per-event model, and the Frontier-specific wrapper that calls `boxen_run()` is responsible for GIL discipline. Boxen itself is GIL-unaware; it is pure terminal I/O.

**Beyond GIL discipline:** the consumer's wrapper around `boxen_poll_event()` is also responsible for any *additional* state-snapshot work the Frontier runtime requires across a GIL yield (e.g., snapshotting `hglobals` before unlock and restoring after lock — see `frontier-cli/protocol_handler.c` for the canonical pattern from PR #722). Boxen has no opinion on this; it neither knows about nor manages Frontier runtime state. The boundary is clean: boxen owns the terminal, the consumer owns the runtime contract.

---

## 8. Build-System Integration

### During Phases A-D (in Frontier's Makefile)

Boxen compiles as a set of translation units linked directly into `frontier-cli`. There is no separate static library during in-tree development — this avoids archive/link order issues and keeps the build simple.

**Makefile additions in `frontier-cli/Makefile`:**

```makefile
# Boxen source files (add to existing SRCS list)
BOXEN_DIR = boxen
BOXEN_SRCS = \
    $(BOXEN_DIR)/boxen.c \
    $(BOXEN_DIR)/boxen_layout.c \
    $(BOXEN_DIR)/boxen_chrome.c \
    $(BOXEN_DIR)/boxen_wcwidth.c \
    $(BOXEN_DIR)/backend_tb2.c \
    $(BOXEN_DIR)/boxen_log.c

# termbox2 vendored source
TB2_DIR = ../third_party/termbox2
BOXEN_SRCS += $(TB2_DIR)/termbox2.c

# Include paths
INCLUDES += -I$(BOXEN_DIR) -I$(TB2_DIR)

# Boxen-specific flag: tell termbox2.c to define implementations
# (termbox2 uses a define-once single-header pattern)
$(TB2_DIR)/termbox2.c: CFLAGS += -DTB_IMPL
```

**Test Makefile additions in `tests/Makefile`:**

Each boxen test binary is a standalone executable, matching the existing pattern (scrollback_pane_tests, pane_compositor_tests, etc.). Each links `boxen.c`, `backend_mock.c`, and the module under test, but NOT `backend_tb2.c` — tests never need a real terminal.

```makefile
BOXEN_TEST_SRCS_COMMON = \
    ../frontier-cli/boxen/boxen.c \
    ../frontier-cli/boxen/boxen_layout.c \
    ../frontier-cli/boxen/boxen_chrome.c \
    ../frontier-cli/boxen/boxen_wcwidth.c \
    ../frontier-cli/boxen/backend_mock.c \
    ../frontier-cli/boxen/boxen_log.c

boxen_core_tests: boxen_core_tests.c $(BOXEN_TEST_SRCS_COMMON)
    $(CC) $(CFLAGS) -I../frontier-cli/boxen -o $@ $^

# ... repeat for other boxen_*_tests targets
```

The root `Makefile` unit target runs `./tools/run_headless_tests.sh`, which discovers and runs all `*_tests` executables in `tests/`. No special registration needed — new boxen test binaries are picked up automatically.

### At Phase E (standalone CMakeLists)

```cmake
cmake_minimum_required(VERSION 3.16)
project(boxen VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED YES)

# Core library
add_library(boxen STATIC
    src/boxen.c
    src/boxen_layout.c
    src/boxen_chrome.c
    src/boxen_wcwidth.c
    src/boxen_log.c
)
target_include_directories(boxen PUBLIC include)
target_include_directories(boxen PRIVATE src third_party/termbox2)

# termbox2 backend (optional — link when building against a real terminal)
add_library(boxen_tb2 STATIC
    src/backend_tb2.c
    third_party/termbox2/termbox2.c
)
target_compile_definitions(boxen_tb2 PRIVATE TB_IMPL)
target_include_directories(boxen_tb2 PRIVATE third_party/termbox2)
target_link_libraries(boxen_tb2 PUBLIC boxen)

# Mock backend for test consumers
add_library(boxen_mock STATIC src/backend_mock.c)
target_link_libraries(boxen_mock PUBLIC boxen)

# Tests
enable_testing()
add_subdirectory(tests)

# Examples
add_subdirectory(examples)

# pkg-config
configure_file(boxen.pc.in boxen.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/boxen.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
```

Note: `boxen` core and `boxen_tb2` are separated into distinct targets. A consumer that wants to use only the mock backend (for testing) links `boxen` + `boxen_mock` without pulling in termbox2.

---

## 9. Logging Integration

Frontier has `log_error()`, `log_warn()`, etc. from `Common/headers/logging.h` with a component tagging system. Boxen must:

1. Be **silent by default** when used as a standalone library (no dependency on Frontier logging).
2. Route through Frontier's logging when embedded in Frontier.
3. Not pollute Frontier's log stream with per-cell noise — only warn-level and above during normal operation.

### The hook design

```c
/* boxen.h — logging hook */

typedef enum {
    BOXEN_LOG_ERROR = 0,  /* Always show; library is broken */
    BOXEN_LOG_WARN  = 1,  /* Unexpected but recoverable */
    BOXEN_LOG_DEBUG = 2,  /* Development diagnostics */
    BOXEN_LOG_TRACE = 3,  /* Per-frame / per-cell verbosity */
} boxen_log_level_t;

typedef void (*boxen_log_fn)(
    boxen_log_level_t level,
    const char *file,
    int line,
    const char *msg,
    void *user_data
);

/* Install a log hook. Pass NULL to suppress all output (the default).
 * The hook is called synchronously; do not call boxen APIs from inside it. */
void boxen_set_log_hook(boxen_log_fn fn, void *user_data);
```

### Frontier integration (in Frontier-specific glue, NOT in boxen itself)

```c
/* frontier-cli/boxen_frontier_log.c — the Frontier bridge, outside boxen/ */

#include "logging.h"
#include "boxen/boxen.h"

static void frontier_boxen_log(boxen_log_level_t level,
                                const char *file, int line,
                                const char *msg, void *user_data) {
    (void)user_data;
    switch (level) {
        case BOXEN_LOG_ERROR:
            log_error(LOG_COMP_GENERAL, "boxen: %s (%s:%d)", msg, file, line);
            break;
        case BOXEN_LOG_WARN:
            log_warn(LOG_COMP_GENERAL, "boxen: %s (%s:%d)", msg, file, line);
            break;
        case BOXEN_LOG_DEBUG:
            log_debug(LOG_COMP_GENERAL, "boxen: %s (%s:%d)", msg, file, line);
            break;
        case BOXEN_LOG_TRACE:
            log_trace(LOG_COMP_GENERAL, "boxen: %s (%s:%d)", msg, file, line);
            break;
    }
}

void boxen_install_frontier_log(void) {
    boxen_set_log_hook(frontier_boxen_log, NULL);
}
```

A future enhancement would add `LOG_COMP_BOXEN` to Frontier's component enum, allowing `FRONTIER_LOG=boxen:trace` to control boxen verbosity independently. That's a Phase B or C concern.

### Inside boxen itself

All internal logging uses a thin macro layer defined in `boxen_internal.h`:

```c
/* boxen_internal.h */
void boxen__log(boxen_log_level_t level,
                const char *file, int line,
                const char *fmt, ...);

#define BOXEN_LOG_ERROR(fmt, ...) \
    boxen__log(BOXEN_LOG_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define BOXEN_LOG_WARN(fmt, ...) \
    boxen__log(BOXEN_LOG_WARN,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define BOXEN_LOG_DEBUG(fmt, ...) \
    boxen__log(BOXEN_LOG_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define BOXEN_LOG_TRACE(fmt, ...) \
    boxen__log(BOXEN_LOG_TRACE, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
```

`boxen_log.c` implements `boxen__log()`, which checks the installed hook and calls it if non-NULL. If NULL (the default), the function is a no-op. No `fprintf(stderr, ...)` anywhere in boxen.

---

## 10. Memory Allocator Policy

**Decision: Direct malloc/free with a configurable allocator vtable, defaulting to system malloc.**

### The options evaluated

| Option | Pros | Cons |
|--------|------|------|
| Direct malloc/free, no vtable | Simplest code | Can't arena-allocate; can't intercept in tests |
| Allocator vtable in boxen_init | Clean extraction seam; testable | 8 more lines in the init call |
| Frontier NewPtr-style | Consistent with legacy Frontier code | Ties boxen to Frontier; unextractable |
| Global replacement (function pointers) | No init parameter change | Thread-unsafe by nature; one configuration global |

The vtable-in-init approach is the right shape for a library that will be extracted. The vtable is simple:

```c
/* boxen.h */
typedef struct boxen_allocator {
    void *(*malloc)(size_t size, void *user_data);
    void  (*free)(void *ptr, void *user_data);
    void *user_data;
} boxen_allocator_t;

/* Pass NULL for system malloc/free defaults */
boxen_result_t boxen_init(const boxen_backend_t *backend,
                          void *backend_config,
                          const boxen_allocator_t *allocator);  /* NULL = default */
```

**Default**: when `allocator` is NULL, boxen uses `malloc`/`free` from `<stdlib.h>`.

**In Frontier**: pass NULL — Frontier does not have a custom allocator that would benefit boxen. The vtable exists for extraction-time consumers who might want it (e.g., a game engine that arena-allocates everything).

**Allocation scope**: boxen allocates window structs and cell buffers. A 200x50 cell buffer is 200 * 50 * sizeof(cell_t) = 200 * 50 * 8 = ~80KB. Allocation frequency is low (window open/close) except during terminal resize (every window reallocates its cell buffer). Resize allocation is acceptable malloc-frequency.

---

## 11. Error Reporting Conventions

**Decision: errno-style negative integer return codes from every fallible function, plus `boxen_last_error_str()` for human-readable context.**

### The options evaluated

| Option | Pros | Cons |
|--------|------|------|
| Negative integer codes only | C precedent (POSIX, SQLite); simple | Error context is a code number, not a message |
| Thread-local error string only | Rich messages | Thread-local storage on Windows (MSVC vs MinGW) is a hazard |
| Callback | Maximum flexibility | Async-safe; but consumers rarely need async error delivery |
| Integer + optional string accessor | Best of both | Slightly more API surface |

The integer + string accessor approach matches SQLite's model (`sqlite3_errmsg()`) and is appropriate here.

### Error code set

```c
/* boxen.h */
typedef int boxen_result_t;

#define BOXEN_OK              0
#define BOXEN_ERR_INVALID    -1   /* NULL or bad argument */
#define BOXEN_ERR_INIT       -2   /* backend init failed */
#define BOXEN_ERR_ALREADY    -3   /* already initialized */
#define BOXEN_ERR_NOMEM      -4   /* allocation failed */
#define BOXEN_ERR_TIMEOUT    -5   /* poll_event timed out (not an error per se) */
#define BOXEN_ERR_IO         -6   /* terminal I/O error */
#define BOXEN_ERR_OVERFLOW   -7   /* window count or buffer limit exceeded */
```

```c
/* Returns a human-readable description of the last error on this thread.
 * The string is owned by boxen; valid until the next boxen API call on
 * this thread. Never returns NULL (returns "" for BOXEN_OK). */
const char *boxen_last_error_str(void);
```

**Thread-local storage**: `boxen_last_error_str()` uses `_Thread_local` (C11 / MSYS2 GCC supports this on Windows). This is safe because boxen is single-threaded from the consumer's perspective; the "last error on this thread" is always the error from the one thread allowed to call boxen.

**Void-returning functions**: Functions that cannot fail (e.g., `boxen_window_set_title`, `boxen_window_raise`) return void. Silently clip or ignore invalid arguments with a debug log; do not crash or return an error from non-fallible operations. If a caller passes a NULL window, log it and return — do not undefined-behavior.

---

## 12. Public API — Pressure Test and Refinements

OVERVIEW's ~35 function sketch is approximately right. After pressure-testing against the debugger TUI and the two things OVERVIEW's draft is missing, the recommended API is the OVERVIEW draft plus the following changes:

### Additions required for debugger TUI (day one)

**1. Screen-to-window coordinate mapping**

The cmd-2-click identifier resolution path requires mapping a raw terminal mouse coordinate to a window and then to a content position within that window.

```c
/* Given a screen position (sx, sy), find which window is topmost at that
 * point and return the window-local content coordinate in (*cx, *cy),
 * accounting for the window's position, border chrome, and scroll offset.
 * Returns NULL if no window is at (sx, sy). */
boxen_window_t *boxen_window_at(int sx, int sy, int *cx, int *cy);
```

**2. Line highlight / decoration**

The debugger script pane needs to highlight the currently-executing line. This is distinct from drawing cell by cell — it's a "mark this row" operation.

```c
/* Set a highlight on a content row. highlight_attr is an OR of
 * BOXEN_ATTR_REVERSE, BOXEN_ATTR_BOLD, etc. Pass 0 to clear.
 * Affects all cells in that content row during the next redraw. */
void boxen_window_set_row_highlight(boxen_window_t *win, int content_row,
                                    uint8_t highlight_attr,
                                    boxen_color_t fg, boxen_color_t bg);
```

**3. Query scroll state**

The debugger TUI's "ensure the current line is visible" logic needs to read back scroll state, not just set it.

```c
void boxen_window_get_scroll(const boxen_window_t *win,
                             int *scroll_x, int *scroll_y);
```

**4. Mouse mod field**

OVERVIEW's `boxen_event` mouse struct lacks a modifier field, breaking the cmd-2-click use case. The corrected struct:

```c
struct {
    int x, y;
    uint8_t button;
    bool pressed;
    uint16_t mod;     /* BOXEN_MOD_* bitmask — ADDED */
    uint16_t flags;   /* BOXEN_MOUSE_* bitmask — see below */
} mouse;
```

**5. Synthesized double-click events**

Termbox2 delivers raw mouse press/release events; there is no native double-click. Every consumer would otherwise reinvent the same state machine (track last-click position + timestamp, fire double-click if next click is within N ms and within K cells of the previous one). Boxen synthesizes double-click instead, surfacing it via a `flags` bitmask on the existing `BOXEN_EV_MOUSE` event:

```c
#define BOXEN_MOUSE_DOUBLE_CLICK   0x01   /* second press in a rapid sequence */
#define BOXEN_MOUSE_TRIPLE_CLICK   0x02   /* reserved for future surfaces */
```

A double-click is detected when a second `pressed=true` arrives within 500 ms of the previous `pressed=true` AND within a 1-cell radius of its position. The double-click event is delivered IN ADDITION TO the second raw press — consumers that don't care about double-click ignore the flag; consumers that do (the debugger's cmd-2-click identifier resolution) check `ev.mouse.flags & BOXEN_MOUSE_DOUBLE_CLICK`.

Tuning constants (`BOXEN_DOUBLE_CLICK_MS`, `BOXEN_DOUBLE_CLICK_RADIUS_CELLS`) live in `boxen_internal.h` and are not part of the public API in v1.0. If a future consumer needs to tune them, that's a v1.1 conversation.

Mock backend exposes a convenience: `boxen_mock_push_double_click(int x, int y, uint16_t mod)` synthesizes the press/release/press/release sequence with the correct timing so tests don't have to thread `clock_gettime` mocks through.

### Removals or scope reductions

**`boxen_layout_split_h/v` return value**

OVERVIEW's layout functions return a "parent" window. The split helpers are convenience functions, not window factories — they should set up two new windows sized to the split ratio and return `void`, with the two window pointers returned via out-parameters. The parent geometry is computed from the terminal dimensions at the time of the call; there is no persistent "parent window" object in the general case.

```c
/* Revised signature: no return value; creates two new windows */
void boxen_layout_split_h(boxen_rect_t total, float ratio,
                          const char *left_title,  boxen_window_t **left,
                          const char *right_title, boxen_window_t **right);
void boxen_layout_split_v(boxen_rect_t total, float ratio,
                          const char *top_title,    boxen_window_t **top,
                          const char *bottom_title, boxen_window_t **bottom);
```

**`boxen_window_set_resizable` / `boxen_window_set_movable`**

Useful for Phase C (outline editor may want a floating, movable window). Not needed for Phase A. Include them in the public header (they're simple state setters), but mark them `/* Phase C */` in a comment so they're not implemented until needed.

### Additions for debugger TUI's footer

The keybind footer is a fixed single-row surface at the bottom of the screen, outside the z-order of normal windows. It needs a dedicated surface type or a convention for "pinned to bottom." The simplest approach is a conventional window with a flag:

```c
void boxen_window_set_pinned(boxen_window_t *win, boxen_pin_edge_t edge);

typedef enum {
    BOXEN_PIN_NONE   = 0,
    BOXEN_PIN_TOP    = 1,
    BOXEN_PIN_BOTTOM = 2,
} boxen_pin_edge_t;
```

A pinned window is excluded from z-order management and always rendered at the specified edge, shrinking the available space for other windows by its height.

### Attribute flags

OVERVIEW omits cell attribute flags (bold, underline, reverse). The backend already sketches `set_cell` with `fg`/`bg`; we need a third `attr` field. termbox2 supports `TB_BOLD`, `TB_UNDERLINE`, `TB_REVERSE`, `TB_ITALIC` as attribute flags on cells. The public API should define its own:

```c
typedef enum {
    BOXEN_ATTR_NONE      = 0,
    BOXEN_ATTR_BOLD      = 1 << 0,
    BOXEN_ATTR_UNDERLINE = 1 << 1,
    BOXEN_ATTR_REVERSE   = 1 << 2,
    BOXEN_ATTR_ITALIC    = 1 << 3,  /* not all terminals */
    BOXEN_ATTR_DIM       = 1 << 4,
} boxen_attr_t;
```

`boxen_set_cell` gains an `attr` parameter; `boxen_draw_text` gets `attr` too. The backend vtable's `set_cell` gets an `attr` field.

### Revised function count

After the above: ~42 public functions. The 35 in OVERVIEW was a floor, not a ceiling. 42 is still a tight, coherent API for a v1.0 terminal window manager. No function in the list does anything surprising.

---

## 13. Backend Interface Refinements

OVERVIEW's 9-function vtable is close to right. Changes recommended:

### `set_cell` signature

```c
/* OVERVIEW draft */
void (*set_cell)(int x, int y, uint32_t ch, uint16_t fg, uint16_t bg);

/* Recommended */
void (*set_cell)(int x, int y, uint32_t ch, uint16_t fg, uint16_t bg, uint16_t attr);
```

Add `attr` as above. Using `uint16_t` for attr matches termbox2's internal representation and avoids packing issues.

### Color capability query

```c
/* Add to boxen_backend_t */
int (*color_depth)(void);   /* Returns 1 (monochrome), 16, 256, or 16777216 (true-color) */
```

This is needed so boxen's palette can adapt: on a 16-color terminal, use the 16-color palette; on a 256-color terminal, use xterm-256 extended colors; on a true-color terminal, enable full RGB. Without this query, boxen either always limits to 16 colors (safe but ugly on modern terminals) or always tries true-color (crashes on 16-color terminals). The termbox2 backend exposes this via `tb_has_truecolor()` / `tb_has_256color()`. The mock backend returns 256 by default (covers all test cases without requiring a real terminal).

### `peek_event` vs `poll_event` only

OVERVIEW asks whether we need `peek_event` alongside `poll_event`. Decision: no, not in v1.0. The usage pattern for a blocking TUI is always: block until event, dispatch, redraw. The only use case for a non-blocking peek is checking for pending input before doing an expensive operation — but in a GIL-cooperative model, the right pattern is yield the GIL, not spin on peek. A `timeout_ms = 0` on `poll_event` provides non-blocking behavior when needed.

### Cursor visibility

```c
/* Add to boxen_backend_t */
void (*set_cursor_visible)(bool visible);
```

The debugger TUI's command input line needs a visible cursor; the script/stack panes do not. This is a simple backend call — termbox2 exposes `tb_set_cursor()` with a `TB_HIDE_CURSOR` constant.

### Does the backend need `flush()` distinct from `present()`?

Decision: no. In termbox2, `present()` writes the diff to the terminal and there is no separate flush. A separate `flush()` would be meaningful for a backend that writes to a buffer and flushes separately (like ncurses' `wrefresh` + `doupdate`), but this adds complexity for little gain in v1.0. If a future backend needs explicit flush, the vtable can be extended at that point.

### Final backend vtable (11 functions)

```c
typedef struct boxen_backend {
    int  (*init)(void *config);
    void (*shutdown)(void);
    int  (*width)(void);
    int  (*height)(void);
    int  (*color_depth)(void);                    /* NEW */
    void (*set_cell)(int x, int y, uint32_t ch,
                     uint16_t fg, uint16_t bg, uint16_t attr); /* attr added */
    void (*set_cursor)(int x, int y);
    void (*set_cursor_visible)(bool visible);     /* NEW */
    void (*present)(void);
    int  (*poll_event)(boxen_event_t *out, int timeout_ms);
    void (*clear)(void);
} boxen_backend_t;
```

11 functions. The OVERVIEW estimate of 9 was slightly low, but the additions are non-controversial.

---

## 14. Redraw Model

**Decision: Full back-to-front redraw per frame, no dirty-rect tracking in v1.0.**

### Reasoning

The OVERVIEW rightly implies back-to-front per frame. The question is whether dirty-rect tracking is worth adding. The analysis:

**Frame cost without dirty-rect tracking**: For a 200x50 terminal (10,000 cells), a full redraw at ~100 frames/second is 1,000,000 cell writes per second into an in-memory buffer. The buffer write itself is trivially fast (<1ms). The expensive part is the diff against the previous frame and the escape-sequence emission to the terminal. This is exactly what termbox2's `present()` optimizes: it diffs the current cell buffer against the previous frame and emits only changed cells. So the actual terminal I/O is already dirty-rect-equivalent.

**Where dirty-rect tracking would help**: If the `draw_fn` callbacks themselves were expensive (complex layout computation, string formatting for large data sets), dirty-rect tracking would let us skip calling those callbacks for windows whose content hasn't changed. For the debugger use case (script pane + stack pane), the callbacks are cheap — they read pre-computed line data. For the future table editor with 10,000 rows, the callbacks need to be designed with virtualization (only draw visible rows), and that virtualization is the surface's job, not boxen's.

**Conclusion**: The termbox2 `present()` call already does the expensive part (diff + emit). Adding dirty-rect tracking to skip `draw_fn` invocations is premature optimization for v1.0. If profiling during Phase B or C shows draw callbacks are a bottleneck, add `boxen_window_set_dirty(bool)` as a performance hint at that point — it's additive and non-breaking.

**Redraw sequence** (fully specified):

1. `boxen_run()` calls `boxen_poll_event()` to block until an event.
2. On event receipt, dispatch to the focused window's `input_fn`.
3. After input dispatch, call each window's `draw_fn` in z-order (bottom to top).
4. Each `draw_fn` writes into the window's internal cell buffer using `boxen_set_cell` et al.
5. After all `draw_fn` calls, boxen composites the window cell buffers into a screen-sized buffer (handling clipping and z-order occlusion).
6. Call `backend->present()` to diff against the previous frame and emit changes.
7. Loop.

The `boxen_window_invalidate()` call in the API is a no-op in this model (redraw always happens after each event). It exists in the API for the future case where a surface wants to trigger a redraw from an async notification that arrives outside the event loop — it schedules a redraw on the next loop iteration.

---

## 15. Test Discipline

### Assertion library

**Decision: `assert.h` with Frontier's existing `test_report.h` harness (TR_RUN/TR_SUMMARY/TR_EXIT_CODE).**

No new test framework. The existing pattern is: `assert()` for all behavioral checks, `test_report.h` for JSON result collection, `TR_RUN(test_function)` for each test function. This matches every other unit test in Frontier. No reason to deviate.

### How cells are inspected after a draw

The mock backend exposes a flat 2D cell array and an inspection API:

```c
/* backend_mock.h (internal, used only by tests) */

/* Reset the mock to a blank WxH screen. Must be called at the start of each test. */
void boxen_mock_reset(int width, int height);

/* Read a cell from the mock's "screen" (the composited output after present()). */
const boxen_mock_cell_t *boxen_mock_cell_at(int x, int y);

typedef struct boxen_mock_cell {
    uint32_t ch;
    uint16_t fg;
    uint16_t bg;
    uint16_t attr;
} boxen_mock_cell_t;

/* Count of cells matching a predicate (for "was this row drawn with X color"). */
int boxen_mock_count_cells(boxen_color_t fg, boxen_color_t bg);

/* Check if a string appears consecutively starting at (x, y). */
bool boxen_mock_has_text_at(int x, int y, const char *s);

/* Check if a string appears anywhere on the screen (any row, any column). */
bool boxen_mock_has_text(const char *s);
```

This mirrors the pattern in `scrollback_pane_tests.c` which uses `compositor_test_fb_at()` for the same purpose. The boxen mock is architecturally cleaner because it's an explicit test backend, not an internal inspection hook, but the conceptual pattern is the same.

### How input is injected

The mock backend's `poll_event` can be fed a queue of synthetic events:

```c
/* Inject an event to be returned by the next boxen_poll_event() call. */
void boxen_mock_push_event(const boxen_event_t *ev);

/* Inject a key event (convenience). */
void boxen_mock_push_key(boxen_key_t key, uint16_t mod, uint32_t ch);

/* Inject a mouse event (convenience). */
void boxen_mock_push_mouse(int x, int y, uint8_t button, bool pressed, uint16_t mod);

/* Returns BOXEN_ERR_TIMEOUT if queue is empty (no blocking in mock). */
```

A test sets up its scenario by calling `boxen_mock_push_event()` before calling `boxen_run()` with a bounded loop that exits after N events.

### Test shapes (4 examples)

**Test shape 1: window with content larger than its rect scrolls correctly**

```
Setup:
  - boxen_mock_reset(80, 24)
  - Open a window at {0, 0, 80, 10} with title "Script"
  - draw_fn writes 50 "lines" of text at content rows 0-49 using boxen_draw_text
  - boxen_window_set_content_size(win, 80, 50)

Action:
  - boxen_window_set_scroll(win, 0, 0)
  - Force a redraw (push a no-op event, run one iteration)

Assertion:
  - boxen_mock_has_text_at(0, 1, "line 0")  -- first visible line at row 1 (row 0 is title border)
  - boxen_mock_has_text_at(0, 8, "line 7")  -- last visible line (8 content rows: rows 1-8; row 9 is border)
  - !boxen_mock_has_text("line 8")          -- line 8 is below the window, should not appear

Action:
  - boxen_window_set_scroll(win, 0, 5)
  - Force redraw

Assertion:
  - boxen_mock_has_text_at(0, 1, "line 5")  -- scroll_y=5, first visible is line 5
  - !boxen_mock_has_text("line 4")          -- line 4 is above the scroll offset
```

**Test shape 2: modal window blocks input to underlying windows**

```
Setup:
  - boxen_mock_reset(80, 24)
  - Open background window "bg" with an input_fn that sets g_bg_saw_input = true
  - Open modal window "modal" in front; boxen_window_set_modal(modal, true)
  - boxen_window_focus(modal)

Action:
  - Push a key event (BOXEN_KEY_F1)
  - Run one event iteration

Assertion:
  - g_bg_saw_input == false    -- modal blocked the event
  - g_modal_saw_input == true  -- modal received it
```

**Test shape 3: terminal resize shrinks windows that won't fit**

```
Setup:
  - boxen_mock_reset(80, 24)
  - Open window A at {0, 0, 40, 24}   -- fills left half
  - Open window B at {40, 0, 40, 24}  -- fills right half

Action:
  - boxen_mock_reset(60, 24)  -- shrink to 60 columns
  - Push a BOXEN_EV_RESIZE event {w=60, h=24}
  - Run one event iteration

Assertion:
  - boxen_window_get_rect(A) returns rect with w <= 30  -- resized
  - boxen_window_get_rect(B) returns rect with x+w <= 60  -- doesn't overflow terminal
  - No window extends beyond (59, 23)  -- no OOB rendering
```

**Test shape 4: z-order changes when a covered window is raised**

```
Setup:
  - boxen_mock_reset(80, 24)
  - Open window A at {0, 0, 40, 10}; draw_fn writes 'A' to every cell
  - Open window B at {0, 0, 40, 10}; draw_fn writes 'B' to every cell (overlaps A)
  - B is created after A, so B is on top

Assertion after initial render:
  - boxen_mock_cell_at(1, 1)->ch == 'B'   -- B is on top

Action:
  - boxen_window_raise(A)  -- bring A to top

Assertion after raise + redraw:
  - boxen_mock_cell_at(1, 1)->ch == 'A'   -- A is now on top
```

These test shapes are complete enough that an implementer can write the actual assertion code directly from them. The behavioral property being tested is clear in each case. No source-code inspection, no regex over strings — cell codepoint comparisons only.

---

## 16. Phase A Acceptance Criteria

"4 weeks" is the OVERVIEW estimate. Here is the specific deliverable that says Phase A is done:

### Required artifacts

1. **`frontier-cli/boxen/boxen.h`** — complete public header matching the API in Section 12 of this document (minus Phase C items). All types defined; all function prototypes present; no placeholder `// TODO` in the public header.

2. **`frontier-cli/boxen/*.c`** — all implementation files (boxen.c, boxen_layout.c, boxen_chrome.c, boxen_wcwidth.c, backend_tb2.c, backend_mock.c, boxen_log.c). All compile without warnings under `-Wall -Wextra`.

3. **`frontier-cli/third_party/termbox2/`** — vendored termbox2. Entry in `third_party/README.md`.

4. **Test suite passing**: all five test binaries (boxen_core_tests, boxen_layout_tests, boxen_scroll_tests, boxen_input_tests, boxen_chrome_tests) pass under `./tools/run_headless_tests.sh`. At minimum 30 test functions total across the suite.

5. **Demo program**: `frontier-cli/boxen/examples/` (even during the in-tree phase) contains `split_demo.c` — a standalone program that opens two side-by-side scrollable windows, populates them with numbered lines, and supports:
   - Up/Down arrows to scroll the focused window
   - Tab to switch focus between windows
   - 'q' to quit
   This program compiles and runs on macOS and Linux. It is the "smoke test" that proves boxen actually works with a real terminal, not just the mock.

6. **Discipline check passes**: `grep -r 'tb_' frontier-cli/boxen/ | grep -v backend_tb2.c` returns no matches.

7. **CI green**: The GitHub Actions matrix (macOS, Linux, Windows) all pass the test suite. Windows runner builds and passes, even if the demo program is manual-only on Windows.

### What Phase A explicitly does NOT require

- The debugger TUI (that's Phase B).
- A `boxen_run()` main loop with event dispatch (the demo can use a manual event loop for Phase A).
- Terminal resize handling (can be stubbed to a no-op that logs a warning).
- Mouse input (key events only for Phase A is sufficient for the demo).

The intent: Phase A is the substrate proof-of-concept. If the test suite passes and the demo works, boxen's core model is validated. Phase B begins with confidence.

---

## 17. Documentation

**Decision: Hand-written doc comments in `boxen.h`, with Doxygen extraction as a secondary artifact.**

### Reasoning

The public header is the primary API documentation. For a ~42-function API in a single header, a well-commented header is self-contained. Forcing Doxygen-style `@param` / `@return` on every function adds overhead; the value of machine-extracted HTML docs is low when the header itself is readable.

What the header comments must contain per function:

1. One sentence of purpose.
2. Any non-obvious precondition (e.g., "must be called inside a draw callback").
3. Any ownership semantics (who frees what).
4. What error codes can be returned.

At Phase E, generate Doxygen HTML as a CI artifact. The Doxygen config is a 20-line `Doxyfile` that points at `include/boxen.h`. Publish to GitHub Pages on tag. This is setup-once-and-forget.

**No man pages, no Markdown-mirrored API docs** — the header is the truth, the Doxygen output is a rendering convenience.

---

## 18. OVERVIEW Corrections Needed

The following items in OVERVIEW.md are wrong or misleading; Jake should decide whether to update them:

1. **OVERVIEW Section 3.2, backend `set_cell` signature**: Missing `attr` parameter. The existing `pane.c` cell type has `uint8_t attr`; termbox2 supports attributes. The oversight is minor but should be corrected before `API_DRAFT.h` is written.

2. **OVERVIEW Section 4, `boxen_event` mouse struct**: Missing `mod` field on the mouse sub-struct. Cmd-2-click is called out as a key debugger TUI feature in Section 1 and Section 2; the event struct doesn't support it. This is a concrete gap.

3. **OVERVIEW Section 5, Phase A**: "Write a unit test suite using the mock backend" is listed without any specifics. Given that TDD is mandatory in this repo, the acceptance criteria should be concrete (test count, what behaviors are covered). Section 16 of this document provides those specifics.

4. **OVERVIEW Section 3.1, LOC estimates**: The "total substrate ~940 LOC" estimate does not include the backend interface shim (`backend_tb2.c`, ~200 LOC), `boxen_wcwidth.c` (~150 LOC), or `boxen_log.c` (~50 LOC). The realistic total for all files under `frontier-cli/boxen/` is ~1300-1400 LOC, not ~1000. This is still well within the "small relative to surface code" claim; the estimate just needs updating.

5. **OVERVIEW Section 4, `boxen_run()`**: The OVERVIEW shows `boxen_run()` as a blocking main loop. For Frontier's use case, the surface code needs to interleave boxen events with Frontier runtime events (async debug notifications, UserTalk thread completions). Whether `boxen_run()` is the right entry point or whether `boxen_poll()` + `boxen_present()` is the right primitive for surface code to call directly is worth resolving explicitly in `API_DRAFT.h`. The execution plan recommendation is to provide both: `boxen_run()` for standalone programs, `boxen_poll()` + `boxen_present()` as the lower-level primitive for Frontier surface code.

---

## 19. Summary Table: All Decisions

| Decision | Resolution | Section |
|----------|------------|---------|
| File layout during Phases A-D | `frontier-cli/boxen/`, test files in `tests/` | 1 |
| Phase E standalone layout | `include/`, `src/`, `tests/`, `examples/`, `CMakeLists.txt` | 1, 8 |
| License | MIT, committed with first artifact | 2 |
| Windows CI | MSYS2/MinGW-w64, `windows-latest` GHA runner | 3 |
| Unicode width | Kuhn mk_wcwidth 2007, renamed `boxen_wcwidth.c`, wrapped | 4 |
| Coordinate convention | 0-based throughout | 5 |
| Key/modifier representation | Own `boxen_key_t` / `boxen_mod_t` enum; translate in backend shim | 6 |
| Threading model | Single-threaded; GIL release before blocking poll | 7 |
| Build integration Phases A-D | Source files compiled directly into frontier-cli; no lib archive | 8 |
| Build integration Phase E | CMake with separate `boxen` core and `boxen_tb2` targets | 8 |
| Logging integration | `boxen_set_log_hook()` vtable; Frontier bridge in separate glue file | 9 |
| Memory allocator | `boxen_allocator_t` vtable in `boxen_init`; NULL = system malloc | 10 |
| Error reporting | Negative integer codes + `boxen_last_error_str()` | 11 |
| Public API size | ~42 functions (OVERVIEW estimated 35) | 12 |
| Mouse event mod field | Added `uint16_t mod` to mouse sub-struct | 12 |
| Row highlight API | `boxen_window_set_row_highlight()` added for debugger | 12 |
| Pinned windows | `boxen_window_set_pinned()` for keybind footer | 12 |
| Cell attribute flags | `boxen_attr_t` enum; `attr` parameter on `set_cell` and `draw_text` | 12 |
| Backend vtable size | 11 functions (OVERVIEW estimated 9) | 13 |
| `set_cell` signature | Takes `attr` parameter | 13 |
| Color capability query | `color_depth()` added to vtable | 13 |
| Cursor visibility | `set_cursor_visible()` added to vtable | 13 |
| `peek_event` | Not added; `poll_event(timeout=0)` suffices | 13 |
| Separate `flush()` | Not added; `present()` handles it | 13 |
| Redraw model | Full back-to-front per frame; no dirty-rect in v1.0 | 14 |
| Test assertion library | `assert.h` + existing `test_report.h` TR_RUN harness | 15 |
| Mock backend inspection | `boxen_mock_cell_at()`, `boxen_mock_has_text()` functions | 15 |
| Input injection in tests | `boxen_mock_push_event()` queue | 15 |
| Phase A acceptance | 5 test binaries + 30 tests + split_demo + CI green | 16 |
| Documentation | Hand-written header comments; Doxygen at Phase E | 17 |
