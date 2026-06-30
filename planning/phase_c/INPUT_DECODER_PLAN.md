# Option C Input Decoder -- Implementation Plan

**Date**: 2026-06-29
**Status**: ACTIVE -- feeds /fleet execution
**Author**: JES + Claude (system-architect)
**Scope**: Custom terminal input decoder for the boxen REPL; replaces termbox2's
  input layer while keeping termbox2's rendering layer intact.
**Context**: PR #808 (reverted at commit 799583b21) failed because termbox2's
  internal escape-sequence parser silently dropped modifier bits on arrow keys
  (issue #805: Option-Left/Right, mousewheel) and broke terminal selection
  (no bracketed paste). Option C builds a purpose-built decoder that owns the
  TTY read loop end-to-end.

---

## 1. Goals and Non-Goals

### 1.1 What the decoder must solve

The root-cause table from the pre-plan investigation:

| Symptom | Root cause in termbox2 |
|---------|----------------------|
| Option-Left / Option-Right not recognized | termbox2 decodes `ESC b` / `ESC f` as TB_MOD_ALT + arrow but modifier-param form `\e[1;3D` is lost: tb_mod table does not propagate modifier param 3 (Alt) to TB_MOD_ALT on CSI sequences on macOS Terminal.app or iTerm2 | 
| Mousewheel scroll emits no event | TB_KEY_MOUSE_WHEEL_UP / DOWN not reached because SGR mouse report `\e[<64;x;yM` is parsed by termbox2 but the button mapping to TB_KEY_MOUSE_* never fires the boxen path on the sequences Terminal.app actually sends |
| Terminal text selection broken after enabling mouse | termbox2 enables RXVT-style mouse (mode 1000) but does not enable bracketed paste (mode 2004); copy-paste broke because the terminal cannot distinguish paste from typed input |
| Modifier keys on F-keys lost | termbox2's SS3 parser does not propagate modifier params for `\e[1;2P` (Shift-F1) and friends |
| Partial sequences on read() boundary fall through | termbox2's parse loop calls tb_parse_seq then falls through to printable if the buffer is exhausted mid-escape; next chunk sees a truncated sequence |

All five must be fixed. No other input behaviors may regress.

### 1.2 What stays untouched

- termbox2 rendering path: `tb_set_cell`, `tb_present`, `tb_clear`, `tb_width`,
  `tb_height`, `tb_hide_cursor`, `tb_set_cursor`, `tb_set_output_mode`.
- The `boxen_backend_t` vtable shape (`boxen.h:208-221`); zero fields added or removed.
- `boxen_event_t` struct layout (`boxen.h:146-165`); zero fields added or removed.
- All consumers above the vtable: `boxen_repl.c`, `debugger_tui.c`,
  `boxen_outline.c`, `boxen_palette_backend.c`, test harnesses. They call
  `boxen_poll_event()` and receive `boxen_event_t`; they will not change.
- The mock backend (`backend_mock.c` / `backend_mock.h`); all unit tests continue
  to use it without modification.

### 1.3 Explicit non-goals

- Not building a full terminal emulator (no xterm state machine, no screen
  scraping, no sixel graphics).
- Not replacing termbox2's init/shutdown for alternate-screen, raw-mode entry,
  or SIGWINCH handling. Those are correct; only the read side is broken.
- Not supporting every terminal that has ever existed. Target: Terminal.app,
  iTerm2, Alacritty, tmux (pass-through), Ghostty, kitty. That covers >98% of
  the macOS developer audience.
- Not supporting the Kitty keyboard protocol in M5 fully -- only `\e[?u` enable
  and the simple `CSI u` form. Full progressive enhancement is a post-launch item.
- Not exposing a public API for raw byte injection outside the boxen subsystem.

---

## 2. Architecture

### 2.1 Source tree layout

```
frontier-cli/boxen/
  input_decoder.h        -- public API (visible only inside frontier-cli/boxen/)
  input_decoder.c        -- state machine implementation (~600 LOC)
  pty_replay_tests.c     -- unit tests driven by raw byte fixtures (M1 harness)
                            compiled with the test suite; not linked into the binary
  backend_tb2.c          -- MODIFIED in M6: tb2_poll_event replaced by a thin
                            wrapper that calls input_decoder_poll() then
                            translates one decoded event to boxen_event_t
```

`input_decoder.h` is NOT added to the list of public boxen headers (only `boxen.h`
is public per `boxen.h:5`). The header is private, like `boxen_internal.h`.

### 2.2 Public API (`input_decoder.h`)

```c
/*
 * input_decoder.h -- private terminal input decoder for the boxen tb2 backend.
 *
 * Owns: TTY fd read loop, byte buffering, escape-sequence state machine,
 * Unicode (UTF-8) decoding, mouse-mode enable/disable.
 *
 * Does NOT own: rendering, alternate screen, SIGWINCH, cell buffer,
 * color, cursor positioning. termbox2 retains all of those.
 *
 * Threading: same contract as the rest of boxen -- single-threaded, GIL-held.
 * The caller (tb2_poll_event) releases the GIL before entering the read loop
 * and reacquires before calling back into Frontier.
 */

#ifndef INPUT_DECODER_H
#define INPUT_DECODER_H

#include "boxen.h"   /* for boxen_event_t */
#include <stdbool.h>

/* Opaque decoder state. Allocated by input_decoder_create(). */
typedef struct input_decoder input_decoder_t;

/* Create a decoder attached to the given file descriptor (usually the TTY
 * fd obtained from tb_get_fds() or /dev/tty).  Returns NULL on ENOMEM.
 * Mouse reporting starts DISABLED; call input_decoder_set_mouse(true) to
 * enable SGR + bracketed paste. */
input_decoder_t *input_decoder_create(int tty_fd);

/* Free all resources. Does NOT close tty_fd. */
void             input_decoder_destroy(input_decoder_t *dec);

/* Enable or disable SGR mouse reporting (mode 1006) and bracketed paste
 * (mode 2004).  When mouse is disabled the terminal selection is active.
 * When mouse is enabled, selections require Shift-click (Terminal.app) or
 * the configured pass-through modifier (iTerm2/Alacritty). */
void             input_decoder_set_mouse(input_decoder_t *dec, bool enable);

/* Query current mouse-enable state. */
bool             input_decoder_mouse_enabled(const input_decoder_t *dec);

/* Block until one event is decoded or timeout_ms elapses.
 * On success writes *out and returns BOXEN_OK.
 * On timeout returns BOXEN_ERR_TIMEOUT (*out is zeroed).
 * On read error returns BOXEN_ERR_IO. */
int input_decoder_poll(input_decoder_t *dec, boxen_event_t *out, int timeout_ms);

/* Enable the Kitty keyboard protocol (if the terminal supports it).
 * Sends the enable sequence; responses arrive via normal input_decoder_poll.
 * Idempotent. No-op if already enabled or terminal does not respond to probe. */
void input_decoder_kitty_enable(input_decoder_t *dec);

/* Inject raw bytes into the decoder's buffer (test seam only; compile-guarded
 * by INPUT_DECODER_TEST_SEAM).  Normal production code never calls this. */
#ifdef INPUT_DECODER_TEST_SEAM
void input_decoder_inject_bytes(input_decoder_t *dec,
                                const uint8_t *bytes, size_t len);
#endif

#endif /* INPUT_DECODER_H */
```

### 2.3 Integration with `backend_tb2.c`

The only change to `backend_tb2.c` is in `tb2_poll_event()` (currently
`backend_tb2.c:316-356`). Today it calls `tb_peek_event()`; after M6 it calls
`input_decoder_poll()` instead.

The decoder is created in `tb2_init()` (currently `backend_tb2.c:249-253`) and
stored in a static:

```c
static input_decoder_t *g_decoder = NULL;
```

`tb2_shutdown()` (`backend_tb2.c:263-270`) calls `input_decoder_destroy(g_decoder)`.

termbox2's `tb_init()` continues to run in `tb2_init()` before decoder creation.
It sets up the alternate screen, raw mode, and SIGWINCH handler. The decoder
opens the same `/dev/tty` fd obtained via `tb_get_fds()` (termbox2 v2.5+ exports
this). If `tb_get_fds()` is not available, the decoder opens `/dev/tty` directly.

### 2.4 What the decoder owns vs. what termbox2 owns

| Concern | Owner |
|---------|-------|
| Alternate screen enter/leave | termbox2 |
| Raw mode (`cfmakeraw`) | termbox2 |
| SIGWINCH handling | termbox2 |
| TTY fd read loop | **decoder** |
| Byte ring buffer | **decoder** |
| Escape-sequence state machine | **decoder** |
| UTF-8 multi-byte assembly | **decoder** |
| Modifier param parsing | **decoder** |
| SGR mouse enable/disable (write) | **decoder** |
| Bracketed paste enable/disable (write) | **decoder** |
| Kitty protocol enable (write) | **decoder** |
| Cell buffer, color, set_cell | termbox2 |
| tb_present (render flush) | termbox2 |
| Cursor show/hide | termbox2 |
| tb_width / tb_height | termbox2 |

---

## 3. Protocol Coverage Matrix

Column definitions:
- **Sequence**: raw bytes (printable where possible; `\xNN` for non-printable)
- **Terminals**: confirmed senders (T=Terminal.app, i=iTerm2, A=Alacritty,
  G=Ghostty, k=kitty, *=all modern)
- **Expected `boxen_event_t`**: struct fields that must be non-zero/non-default

### 3.1 Cursor keys -- plain

| Sequence | Terminals | Expected event |
|----------|-----------|---------------|
| `\e[A` | * | KEY UP, mod=NONE |
| `\e[B` | * | KEY DOWN, mod=NONE |
| `\e[C` | * | KEY RIGHT, mod=NONE |
| `\e[D` | * | KEY LEFT, mod=NONE |
| `\eOA` | T,i | KEY UP, mod=NONE (SS3 form) |
| `\eOB` | T,i | KEY DOWN, mod=NONE |
| `\eOC` | T,i | KEY RIGHT, mod=NONE |
| `\eOD` | T,i | KEY LEFT, mod=NONE |

### 3.2 Cursor keys -- modifier params (CSI 1;N form)

Modifier param N maps to: 1=none, 2=Shift, 3=Alt, 4=Shift+Alt, 5=Ctrl,
6=Shift+Ctrl, 7=Alt+Ctrl, 8=Shift+Alt+Ctrl, 9=Meta, 10=Meta+Shift,
11=Meta+Alt, 12=Meta+Shift+Alt, 13=Meta+Ctrl, 14=Meta+Shift+Ctrl,
15=Meta+Alt+Ctrl, 16=Meta+Shift+Alt+Ctrl.

| Sequence example | N | Expected mod bits |
|-----------------|---|-------------------|
| `\e[1;2A` | 2 | SHIFT |
| `\e[1;3A` | 3 | ALT |
| `\e[1;4A` | 4 | ALT\|SHIFT |
| `\e[1;5A` | 5 | CTRL |
| `\e[1;6A` | 6 | CTRL\|SHIFT |
| `\e[1;7A` | 7 | ALT\|CTRL |
| `\e[1;8A` | 8 | ALT\|CTRL\|SHIFT |
| `\e[1;9A` | 9 | META |
| `\e[1;10A` | 10 | META\|SHIFT |
| `\e[1;11A` | 11 | META\|ALT |
| `\e[1;12A` | 12 | META\|ALT\|SHIFT |
| `\e[1;13A` | 13 | META\|CTRL |
| `\e[1;14A` | 14 | META\|CTRL\|SHIFT |
| `\e[1;15A` | 15 | META\|ALT\|CTRL |
| `\e[1;16A` | 16 | META\|ALT\|CTRL\|SHIFT |

Same table applies for B (DOWN), C (RIGHT), D (LEFT), H (Home), F (End).

**The #805 root cause**: Terminal.app sends `\e[1;3D` for Option-Left and
`\e[1;3C` for Option-Right (param N=3, Alt). iTerm2 optionally also sends
`\eb` / `\ef` (ESC-prefix meta form, see 3.5). termbox2's parser maps modifier
params only to TB_MOD_SHIFT (N=2) and TB_MOD_CTRL (N=5); it ignores N=3 (Alt).

### 3.3 Nav keys -- plain and with modifiers

| Sequence | Key | Notes |
|----------|-----|-------|
| `\e[H` | HOME | |
| `\e[F` | END | |
| `\e[2~` | INSERT | |
| `\e[3~` | DELETE | |
| `\e[5~` | PGUP | |
| `\e[6~` | PGDN | |
| `\e[1;N H/F/2~/3~/5~/6~` | with modifier | same N table as 3.2 |
| `\eOH` | HOME | SS3 form (some terminals) |
| `\eOF` | END | SS3 form |

### 3.4 Function keys

**SS3 form** (VT100 app-keypad):

| Sequence | Key |
|----------|-----|
| `\eOP` | F1 |
| `\eOQ` | F2 |
| `\eOR` | F3 |
| `\eOS` | F4 |

**CSI form** (xterm/linux):

| Sequence | Key |
|----------|-----|
| `\e[11~` | F1 (legacy) |
| `\e[12~` | F2 |
| `\e[13~` | F3 |
| `\e[14~` | F4 |
| `\e[15~` | F5 |
| `\e[17~` | F6 |
| `\e[18~` | F7 |
| `\e[19~` | F8 |
| `\e[20~` | F9 |
| `\e[21~` | F10 |
| `\e[23~` | F11 |
| `\e[24~` | F12 |

**With modifiers** (xterm): `\e[1;NP` (F1 Shift), `\e[11;N~` (legacy F1 Shift).
Modifier N maps same as 3.2.

### 3.5 ESC-prefix Meta keys

Terminals configured for Meta-sends-ESC (or when Option key is mapped to ESC+):

| Sequence | Expected event |
|----------|---------------|
| `\ea` | ch='a', mod=ALT |
| `\eb` | KEY LEFT (word-left in many conventions) OR ch='b', mod=ALT -- see policy below |
| `\ef` | KEY RIGHT (word-right) OR ch='f', mod=ALT |
| `\e[` (followed by printable within 50ms) | treat as Meta+printable |
| `\e` alone (no further byte within disambiguation window) | ESCAPE key |

**Policy for `\eb` / `\ef`**: Emit them as `ch='b', mod=ALT` and `ch='f', mod=ALT`
rather than as KEY LEFT/RIGHT. The REPL can bind word-motion to Alt-Left/Right
(the standard `\e[1;3D/C` sequences) and Alt-b/Alt-f separately in its keymap.
Conflating them in the decoder would prevent a future keymap from distinguishing
the two. Terminal.app sends `\e[1;3D` for Option-Left; `\eb` comes from bash/readline
emacs mode via a different binding path. Both arrive; both should be distinct.

### 3.6 SGR mouse protocol (`\e[<...m/M`)

Mouse mode 1006 (SGR): `\e[<button;col;rowM` (press) / `\e[<button;col;rowm` (release).

Button encoding:
| Button value | Meaning |
|-------------|---------|
| 0 | Left press |
| 1 | Middle press |
| 2 | Right press |
| 4 | Shift modifier |
| 8 | Meta modifier |
| 16 | Ctrl modifier |
| 32 | Motion (added to any button for drag/motion) |
| 64 | Wheel up |
| 65 | Wheel down |

Expected translations:

| SGR bytes | Event |
|-----------|-------|
| `\e[<0;10;5M` | MOUSE, button=1, pressed=true, x=9, y=4 |
| `\e[<0;10;5m` | MOUSE, button=1, pressed=false, x=9, y=4 |
| `\e[<64;10;5M` | MOUSE, button=4 (wheel-up), pressed=true |
| `\e[<65;10;5M` | MOUSE, button=5 (wheel-down), pressed=true |
| `\e[<32;10;5M` | MOUSE, button=0 (motion), pressed=false |

Note: x and y arrive as 1-based; decoder subtracts 1 to produce 0-based coords
consistent with `boxen_event_t.mouse.{x,y}`.

Modifier bits 4/8/16 on the button value map to BOXEN_MOD_SHIFT, BOXEN_MOD_META,
BOXEN_MOD_CTRL respectively and are ORed into `ev.mouse.mod`.

**The #805 mousewheel root cause**: termbox2's `translate_mouse()` in `backend_tb2.c`
at line 202 dispatches on `te.key` using TB_KEY_MOUSE_* constants. Whether
termbox2's parser correctly maps SGR button=64/65 to TB_KEY_MOUSE_WHEEL_UP/DOWN
depends on its internal SGR parsing path. Empirically this path fails on some
macOS terminal versions (the same sequences work in the custom decoder because we
parse the button value directly without going through termbox2's internal enum).

### 3.7 X10 legacy mouse (`\e[Mbxy`)

Mouse mode 1000 (X10): three-byte sequence `\e[M` + button-byte + x-byte + y-byte.
Bytes are offset by 32 (`button_raw - 32`, `x_raw - 32 - 1` for 0-based).

| X10 bytes | Expected event |
|-----------|---------------|
| `\e[M\x20\x2a\x19` | Left press at x=9, y=0 |
| `\e[M\x60\x2a\x19` | Wheel-up at x=9, y=0 (0x60=96=32+64) |
| `\e[M\x61\x2a\x19` | Wheel-down at x=9, y=0 |

X10 is the fallback when the terminal does not acknowledge SGR mode. The decoder
probes for SGR first (by enabling mode 1006 and checking if subsequent reports
use `<` prefix). If yes, X10 parsing is disabled. If no SGR report arrives within
5 seconds of mouse enable, the decoder falls back to X10 parsing.

In practice: Terminal.app and iTerm2 both support SGR. X10 fallback is for
unusual configurations.

### 3.8 Bracketed paste

Mode 2004: enabled alongside mouse mode. Sequence: `\e[200~` ... paste content ...
`\e[201~`.

| Sequence | Expected event |
|----------|---------------|
| `\e[200~hello world\e[201~` | BOXEN_EV_PASTE (new event type; see below) |

The decoder must buffer all bytes between the two markers before emitting a single
event. Maximum paste buffer: 256 KB (configurable via compile-time constant). A
paste that exceeds the buffer is truncated and a BOXEN_LOG_W is emitted.

This requires adding `BOXEN_EV_PASTE` to `boxen_event_type_t` and a new
`paste` union arm to `boxen_event_t`:

```c
/* In boxen_event_type_t (boxen.h): */
BOXEN_EV_PASTE,   /* bracketed paste; ev.paste.data + ev.paste.len */

/* In boxen_event union (boxen.h): */
struct {
    char   *data;   /* heap-allocated UTF-8; caller must free() */
    size_t  len;    /* byte length, not including NUL */
} paste;
```

`BOXEN_EV_PASTE` is a new event type. Consumers that do not handle it must
treat it as BOXEN_EV_NONE (existing switch default). The REPL's `boxen_repl_run_one_tick()`
handles it by inserting the paste data at the cursor position (M4).

**Why bracketed paste solves the #805 copy/paste pain**: Without mode 2004,
pasting into the REPL sends each character individually. With mouse mode enabled
(which PR #808 did), some terminals also send mouse-release events interleaved
with paste characters, corrupting the input. Bracketed paste puts a clean
`\e[200~` ... `\e[201~` wrapper around the paste so the decoder knows exactly
where it starts and ends.

### 3.9 Kitty keyboard protocol

Probe: send `\e[?u` and check if terminal responds with `\e[=FLAGS u`. If yes,
enable progressive enhancement flags (1 = disambiguate escape codes, 2 = report
event types, 4 = report alternate keys).

After enable, the terminal sends:
- `\e[KEYCODE u` for simple keys (e.g. `\e[65u` for 'A')
- `\e[KEYCODE ; MODIFIERS u` for modified keys
- `\e[KEYCODE ; MODIFIERS ; EVENT u` for press/repeat/release

For M5 scope: decode the `\e[N;Nu` form with modifier to boxen_event_t. Full
three-part form (with event type and alternate key) is post-launch.

### 3.10 UTF-8 multi-byte codepoints

Multi-byte UTF-8: leading byte determines how many continuation bytes follow.
- 0xxxxxxx: 1 byte (ASCII)
- 110xxxxx 10xxxxxx: 2 bytes (U+0080..U+07FF)
- 1110xxxx 10xxxxxx 10xxxxxx: 3 bytes (U+0800..U+FFFF)
- 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx: 4 bytes (U+10000..U+10FFFF)

The decoder accumulates continuation bytes. If a non-continuation byte arrives
before the sequence is complete (a read() boundary), it stays in the ring buffer
for the next poll cycle. The assembled codepoint goes into `ev.key.ch`.

### 3.11 Raw control characters

| Byte | Expected event |
|------|---------------|
| 0x01 | CTRL_A |
| 0x02..0x1A | CTRL_B..CTRL_Z |
| 0x08 | CTRL_H / BACKSPACE |
| 0x09 | TAB |
| 0x0D | ENTER |
| 0x1B (alone) | ESCAPE (after disambiguation window) |
| 0x1C | CTRL_BACKSLASH |
| 0x7F | BACKSPACE (DEL) |
| 0x00 | CTRL_SPACE |

### 3.12 Burst-read robustness

The decoder MUST NOT fall back to treating a partial escape as a printable.
The contract: if the buffer ends mid-sequence, the decoder returns BOXEN_ERR_TIMEOUT
(nothing to emit yet) and retains the partial bytes for the next `read()`.

This is the exact failure mode in termbox2 that the prior investigation identified.
The test harness validates it by injecting partial sequences split across
`input_decoder_inject_bytes()` calls (the test seam).

---

## 4. State Machine

### 4.1 States

```
GROUND          -- initial / after a complete event
ESC_RECEIVED    -- saw 0x1B; waiting to disambiguate
CSI_COLLECTING  -- saw 0x1B 0x5B ([); reading params + final byte
SS3_RECEIVED    -- saw 0x1B 0x4F (O); reading one byte for SS3 key
OSC_COLLECTING  -- saw 0x1B 0x5D (]); reading until ST (0x07 or 0x1B 0x5C)
PASTE_ACTIVE    -- inside bracketed paste (after \e[200~); buffering until \e[201~
UTF8_CONT       -- inside a multi-byte UTF-8 sequence; waiting for continuation bytes
KITTY_COLLECTING -- inside a CSI u sequence (Kitty protocol)
```

### 4.2 Transition pseudocode

```
state = GROUND

on byte B:
  if state == PASTE_ACTIVE:
    append B to paste_buf
    if paste_buf ends with "\e[201~":
      trim marker; emit BOXEN_EV_PASTE; state = GROUND
    return

  if state == UTF8_CONT:
    if B is a continuation byte (10xxxxxx):
      append to utf8_buf; utf8_remaining--
      if utf8_remaining == 0:
        emit KEY ch=assemble_codepoint(utf8_buf), mod=utf8_pending_mod
        state = GROUND
      return
    else:
      // premature non-continuation: discard partial, reprocess B in GROUND
      state = GROUND
      // fall through to GROUND handling of B

  if state == GROUND:
    if B == 0x1B:
      state = ESC_RECEIVED
      esc_timer_start(50ms)  // disambiguation window
      return
    if is_utf8_lead(B):
      utf8_buf = [B]; utf8_remaining = utf8_continuation_count(B); state = UTF8_CONT
      return
    emit_control_or_printable(B)
    return

  if state == ESC_RECEIVED:
    if no more bytes available AND esc_timer_expired:
      emit KEY ESCAPE; state = GROUND; return
    if B == '[' (0x5B):
      state = CSI_COLLECTING; csi_buf = ""; return
    if B == 'O' (0x4F):
      state = SS3_RECEIVED; return
    if B == ']' (0x5D):
      state = OSC_COLLECTING; osc_buf = ""; return
    if B == 0x1B:
      // double ESC: emit ESCAPE, stay in ESC_RECEIVED for new ESC
      emit KEY ESCAPE
      esc_timer_start(50ms)
      return
    // ESC + printable: Meta-key
    emit KEY ch=B, mod=ALT
    state = GROUND
    return

  if state == SS3_RECEIVED:
    if B in {P,Q,R,S}: emit F1-F4; state = GROUND; return
    if B in {A,B,C,D}: emit UP/DOWN/RIGHT/LEFT; state = GROUND; return
    if B in {H,F}: emit HOME/END; state = GROUND; return
    // unrecognized SS3: discard; state = GROUND; return

  if state == CSI_COLLECTING:
    if B is a parameter byte (0x30..0x3F) or intermediate (0x20..0x2F):
      append B to csi_buf; return
    // Final byte: 0x40..0x7E
    decode_csi(csi_buf, B)  // maps to boxen_event_t
    state = GROUND
    // Special: if final=u and format matches Kitty -> KITTY_COLLECTING or decode inline
    return
```

### 4.3 CSI decode (`decode_csi`)

```
decode_csi(params_str, final_byte):
  parse params_str as semicolon-delimited integers p[0], p[1], p[2]

  if final_byte == '<':   // SGR mouse prefix (\e[<...)
    // params_str is incomplete -- '<' is an intermediate, not final
    // actual parsing happens when 'M' or 'm' arrives
    // (handled by appending to csi_buf as intermediate byte)
    // This note clarifies the encoding: '<' is classified as intermediate 0x3C

  switch final_byte:
    case 'A': emit KEY UP,    mod=modifier_from_param(p[1])
    case 'B': emit KEY DOWN,  mod=modifier_from_param(p[1])
    case 'C': emit KEY RIGHT, mod=modifier_from_param(p[1])
    case 'D': emit KEY LEFT,  mod=modifier_from_param(p[1])
    case 'H': emit KEY HOME,  mod=modifier_from_param(p[1])
    case 'F': emit KEY END,   mod=modifier_from_param(p[1])
    case '~':
      switch p[0]:
        2: emit INSERT,  mod=modifier_from_param(p[1])
        3: emit DELETE,  mod=modifier_from_param(p[1])
        5: emit PGUP,    mod=modifier_from_param(p[1])
        6: emit PGDN,    mod=modifier_from_param(p[1])
        11: emit F1 (legacy); 12: F2; 13: F3; 14: F4
        15: emit F5
        17: emit F6; 18: F7; 19: F8; 20: F9; 21: F10
        23: emit F11; 24: emit F12
        200: state = PASTE_ACTIVE; paste_buf = ""; return
        201: // spurious close; ignore
    case 'P': emit F1, mod=modifier_from_param(p[1])
    case 'Q': emit F2, mod=modifier_from_param(p[1])
    case 'R': emit F3, mod=modifier_from_param(p[1])
    case 'S': emit F4, mod=modifier_from_param(p[1])
    case 'M':  // SGR mouse press  (csi_buf started with '<')
      if csi_buf starts with '<': decode_sgr_mouse(csi_buf, pressed=true)
      else:                         decode_x10_mouse_from_csi(csi_buf)
    case 'm':  // SGR mouse release
      if csi_buf starts with '<': decode_sgr_mouse(csi_buf, pressed=false)
    case 'u':  // Kitty or CSI-u
      decode_kitty_or_csiu(p[0], p[1], p[2])
    // unknown final: discard silently (never emit as printable)

modifier_from_param(N):
  if N <= 1 or N is absent: return BOXEN_MOD_NONE
  N -= 1   // modifier params are 1-based (1=no mod, 2=Shift, ...)
  bits = 0
  if N & 1: bits |= BOXEN_MOD_SHIFT
  if N & 2: bits |= BOXEN_MOD_ALT
  if N & 4: bits |= BOXEN_MOD_CTRL
  if N & 8: bits |= BOXEN_MOD_META
  return bits
```

### 4.4 ESC disambiguation strategy

**Strategy**: emit ESC immediately when the next `read()` call returns no bytes
within a non-blocking check. Do NOT use `select()` with a timeout on the
disambiguation path.

**Justification**: The only case where `ESC` arrives without a follow-up byte is
when the user actually pressed Escape. In practice, terminal escape sequences
arrive as a burst from the OS pipe buffer -- the entire `\e[1;3D` sequence
arrives in one or two `read()` calls. A brief `select()` timeout (50-100ms) would
add latency to every Escape keypress for a rare edge case. The modern approach
(used by vim, neovim, fish) is to read as many bytes as available with a non-
blocking read after the initial blocking read, then decide based on what arrived.

**Concrete algorithm**:

1. `read()` with the full `timeout_ms` timeout (blocking, via `select()`).
2. If a byte arrives and it is `0x1B`, immediately do a non-blocking `read()` for
   the rest of the available bytes (drain the pipe buffer).
3. If the non-blocking read returns at least one byte: the ESC is the start of a
   sequence; process the accumulated bytes through the state machine.
4. If the non-blocking read returns zero bytes: emit ESCAPE and return.

This gives sub-millisecond ESC response on fast pipes and correct sequence
parsing on all observed terminals. It matches neovim's `timeoutlen=0` behavior.

---

## 5. Mouse-Mode Policy

### 5.1 Default state

Mouse mode is **disabled on startup**. The terminal's native selection mechanism
works without restriction. The user can copy text from the REPL by highlighting
with the mouse normally.

### 5.2 When mouse mode activates

Mouse mode enables on demand when the REPL actively needs mouse input:
- When a boxen window registers a mouse handler (any `input_fn` that is set on a
  window with `movable=true`, `resizable=true`, or that explicitly calls
  `boxen_set_global_key_handler` with a non-NULL filter).
- The current REPL does not need mouse at startup. Mouse becomes useful when the
  palette is open (click to select), when a completion popup is shown, or when a
  future window manager UI needs drag-to-resize.

This deferred enable means that on a fresh REPL session, selection works normally.
Mouse mode enables only when a feature that needs it is active.

### 5.3 Bracketed paste as the copy/paste bridge

When mouse mode IS enabled, the terminal's native click-to-select stops working
unless the user holds Shift (Terminal.app) or the configured bypass modifier
(iTerm2, Alacritty: Shift by default; Ghostty and kitty: configurable). This is
a fundamental protocol constraint, not a bug.

Bracketed paste (mode 2004) is enabled simultaneously with mouse mode and solves
the paste-into-REPL problem: when the user pastes via Cmd-V (macOS), the terminal
wraps the paste in `\e[200~` ... `\e[201~`. The decoder emits a single
`BOXEN_EV_PASTE` event rather than a torrent of individual character events.
The REPL inserts the paste string at the cursor. This is the standard modern
answer (all major terminals support mode 2004).

### 5.4 The `/mouse` toggle

A `/mouse on` / `/mouse off` slash command allows the user to explicitly control
mouse mode:
- `/mouse off`: disables SGR mouse + mode 2004; native selection resumes.
  Mousewheel and palette click no longer work.
- `/mouse on`: enables SGR mouse + mode 2004; paste via Cmd-V uses bracketed paste.
  Native selection requires Shift+drag.
- Preference persists across sessions in the history file's config section
  (or a separate dotfile; exact mechanism is M7 scope).

The default is `off` (per 5.1). The footer hint bar should show the current state
and remind the user of the toggle.

### 5.5 Why PR #808 broke selection

PR #808 called `tb_set_input_mode(TB_INPUT_MOUSE)` unconditionally in `tb2_init`.
This enabled mouse mode permanently, including on startup when the user had not
opted in. The terminal stopped processing native selection. The fix is:
1. Do NOT call any mouse-enable API in `tb2_init`.
2. The decoder calls the enable/disable sequences on demand via
   `input_decoder_set_mouse(true/false)`.
3. Mouse mode starts disabled; the REPL opts in when it opens the palette or
   a mouse-interactive surface.

---

## 6. Testing Strategy

### 6.1 PTY-replay test harness (M1)

**Location**: `tests/input_decoder_tests.c`

**Compile flag**: `-DINPUT_DECODER_TEST_SEAM` exposes
`input_decoder_inject_bytes()`. Test builds add this flag; the production build
does not.

**Shape**:
```c
/* Each test calls:
 *   1. input_decoder_create(FAKE_FD)  -- FAKE_FD = -1; not opened in tests
 *   2. input_decoder_inject_bytes(dec, bytes, len)  -- bypass OS read()
 *   3. input_decoder_poll(dec, &ev, 0)  -- non-blocking; drain injected buffer
 *   4. assert on ev fields
 *   5. input_decoder_destroy(dec)
 */

static void test_option_left_csi_1_3_D(void) {
    input_decoder_t *dec = input_decoder_create(-1);
    assert(dec != NULL);
    static const uint8_t seq[] = {0x1b, '[', '1', ';', '3', 'D'};
    input_decoder_inject_bytes(dec, seq, sizeof(seq));
    boxen_event_t ev = {0};
    int r = input_decoder_poll(dec, &ev, 0);
    assert(r == BOXEN_OK);
    assert(ev.type == BOXEN_EV_KEY);
    assert(ev.key.key == BOXEN_KEY_LEFT);
    assert(ev.key.mod == BOXEN_MOD_ALT);
    input_decoder_destroy(dec);
}
```

**Fixture recording**: byte sequences in the test file are recorded by running
`xxd -i` on a file captured with:
```sh
# In a real terminal session:
stty raw -echo; cat -v > /tmp/input_capture.bin
# Press the key combination to capture; then Ctrl-C
stty sane
```
or using a Python PTY wrapper (same pattern as `tests/debugger_tui_pty_test.py`).
Fixtures are stored as C `static const uint8_t[]` arrays inline in the test file.
No external fixture files; the test is self-contained and runs via the Makefile
without network or filesystem dependencies.

**Terminal coverage per fixture**: each test documents which terminals were
recorded from (T=Terminal.app, i=iTerm2, A=Alacritty, G=Ghostty, k=kitty).
Sequences that vary by terminal get multiple test functions, one per variant.

**Partial-sequence split test**:
```c
static void test_partial_sequence_across_read_boundary(void) {
    input_decoder_t *dec = input_decoder_create(-1);
    // Inject first half of \e[1;3D: \e[1;
    static const uint8_t half1[] = {0x1b, '[', '1', ';'};
    input_decoder_inject_bytes(dec, half1, sizeof(half1));
    boxen_event_t ev = {0};
    // Poll with 0ms: must NOT emit anything (partial sequence)
    int r = input_decoder_poll(dec, &ev, 0);
    assert(r == BOXEN_ERR_TIMEOUT);
    assert(ev.type == BOXEN_EV_NONE);
    // Inject second half: 3D
    static const uint8_t half2[] = {'3', 'D'};
    input_decoder_inject_bytes(dec, half2, sizeof(half2));
    // Poll again: now complete, must emit LEFT+ALT
    r = input_decoder_poll(dec, &ev, 0);
    assert(r == BOXEN_OK);
    assert(ev.type == BOXEN_EV_KEY);
    assert(ev.key.key == BOXEN_KEY_LEFT);
    assert(ev.key.mod == BOXEN_MOD_ALT);
    input_decoder_destroy(dec);
}
```

**Coverage target for M1**: harness infrastructure only (no decoder yet). The
tests compile but SKIP all protocol tests (return early) until M2 decoder
code lands. The harness is the gate for M1.

### 6.2 Protocol coverage targets per milestone

| Milestone | Tests required before code ships |
|-----------|----------------------------------|
| M1 | Harness: inject_bytes + poll + create/destroy compile and link. All protocol tests marked SKIP (stubs). |
| M2 | CSI arrow keys plain + all modifier params (2-16) + SS3 form + partial-sequence split. ESC-alone disambiguation. |
| M3 | SGR mouse (wheel-up, wheel-down, left/right/middle press/release, motion). X10 fallback. Burst-read (multiple events in one inject). |
| M4 | Bracketed paste start+end. Paste truncation at 256KB. Paste with embedded newlines. |
| M5 | Kitty CSI-u basic form (letter + modifier). Kitty enable probe. |
| M6 | Integration: `backend_tb2.c` cutover. The PTY-replay tests continue to pass because inject_bytes bypasses the real fd. |
| M7 | Mouse toggle: `/mouse on/off` acceptance test via TUI harness (tmux). |

### 6.3 TUI harness role (tmux, `tools/tui-tests/`)

The existing tmux harness (`tools/tui-tests/tui_harness.py`) is **regression-only**
for input decoder work. It cannot record raw byte sequences (tmux translates keys
before sending to the pane). Its role after Option C lands:

- Verify that arrow keys in the REPL still work end-to-end (regression).
- Verify that Ctrl-A / Ctrl-E (existing tests in `test_05_cursor_editing.py`)
  still pass.
- Verify the `/mouse` toggle slash command (M7 acceptance test).

Do NOT add new protocol-coverage tests to the tmux harness. The PTY-replay C
tests own protocol correctness; the tmux harness owns end-to-end integration.

### 6.4 Manual smoke checklist (release gate)

Before declaring any milestone done, a human must verify:

**For M2 (cursor keys)**:
- [ ] Left / Right arrows move cursor in REPL input bar
- [ ] Option-Left / Option-Right recognized (show in REPL footer as "Alt+Left/Right")
- [ ] Up / Down navigate history
- [ ] Ctrl-A, Ctrl-E jump to start/end

**For M3 (mouse)**:
- [ ] Mousewheel scrolls the output pane
- [ ] Left-click on a palette entry selects it
- [ ] Native selection works when mouse mode is disabled (default)

**For M4 (paste)**:
- [ ] Cmd-V pastes text into input bar correctly (no garbled characters)
- [ ] Multi-line paste inserts entire content at cursor

**For M5 (Kitty)**:
- [ ] On kitty terminal: Option-arrows still recognized (verify via footer debug display)
- [ ] On Terminal.app: Kitty probe returns no response; decoder falls back to CSI form transparently

**For M6 (cutover)**:
- [ ] All existing TUI tests pass (`make -C tools/tui-tests` or `cd tools/tui-tests && python3 run.py`)
- [ ] Integration test suite still green (`cd tests && make test-integration`)

**For M7 (mouse toggle)**:
- [ ] `/mouse on` footer shows mouse-on indicator
- [ ] `/mouse off` restores native selection
- [ ] Preference survives REPL restart (if persistence is in scope)

### 6.5 TDD discipline

Every entry in section 3 (Protocol Coverage Matrix) that is in-scope for a
milestone MUST have a PTY-replay test that fails BEFORE the decoder code is
written. The sub-agent dispatched for that milestone writes the failing tests,
runs `make -C tests input_decoder_tests` to confirm they fail for the right
reason (not a link error), then writes the decoder code to make them pass.
Then full suite (`./tools/run_headless_tests.sh`). Then integration.

No exceptions. The test file is the deliverable, not the decoder.

---

## 7. Milestone Decomposition

### M1 -- PTY-replay test harness

**What**: Add the test harness infrastructure. No production code changes.

**Deliverables**:
- `frontier-cli/boxen/input_decoder.h` -- stub (typedef + function declarations;
  bodies are `{ return NULL; }` / `{ return BOXEN_ERR_IO; }` etc.)
- `frontier-cli/boxen/input_decoder.c` -- minimal skeleton: `input_decoder_t`
  struct with ring buffer fields; `create` allocates it; `destroy` frees it;
  `inject_bytes` copies to ring buffer; `poll` returns BOXEN_ERR_TIMEOUT always
- `tests/input_decoder_tests.c` -- harness: create/destroy smoke test; all
  protocol tests SKIP (stub bodies that assert(false && "not yet implemented") or
  call `TR_SKIP`)
- `tests/Makefile` updated to build `input_decoder_tests`

**Done criterion**: `make -C tests input_decoder_tests && ./tests/input_decoder_tests`
compiles and passes (all tests either pass smoke or show SKIP). Full unit suite
still green.

**LOC estimate**: ~150 LOC (decoder stub) + ~200 LOC (test harness).

**Dependencies**: none.

---

### M2 -- Cursor keys, modifier params, ESC disambiguation

**What**: Implement the CSI cursor-key and SS3 paths. This is the #805 fix.

**Deliverables**:
- `frontier-cli/boxen/input_decoder.c` -- full state machine through GROUND,
  ESC_RECEIVED, CSI_COLLECTING, SS3_RECEIVED states. `modifier_from_param()`
  implemented. No mouse, no paste, no Kitty yet.
- `tests/input_decoder_tests.c` -- un-SKIP and pass:
  - Plain CSI arrows (`\e[A/B/C/D`)
  - SS3 arrows (`\eOA/B/C/D`)
  - Modifier params 2-16 for all four arrow directions
  - Home/End CSI and SS3
  - Insert/Delete/PgUp/PgDn with modifiers
  - F1-F12 in both SS3 and CSI forms with modifiers
  - ESC-alone (confirm emits ESCAPE, not silenced)
  - ESC-prefix Meta (Alt-a, Alt-b, Alt-f etc.)
  - Partial-sequence split (M1 stub un-skipped)
  - Control characters 0x01-0x1A

**Done criterion**: all M2 tests green; PTY-replay tests pass; `/gate` clean.
Manual: Option-Left and Option-Right recognized in a real REPL session on
Terminal.app and iTerm2.

**LOC estimate**: ~300 LOC decoder + ~200 LOC tests.

**Dependencies**: M1.

---

### M3 -- SGR mouse + X10 fallback + burst-read

**What**: Implement the mouse protocol path and the burst-read robustness contract.

**Deliverables**:
- `frontier-cli/boxen/input_decoder.c` -- SGR mouse parsing (`\e[<...M/m`),
  X10 fallback (`\e[Mbxy`), button/modifier mapping to `boxen_event_t.mouse`,
  double-click synthesis (port from `backend_tb2.c:54-83`), burst-read: a single
  `input_decoder_poll` call drains all available bytes into the ring buffer
  before returning the first event, leaving the rest for subsequent calls.
- `input_decoder.h` -- no changes needed (mouse enable/disable was in M1 stub)
- `tests/input_decoder_tests.c` -- un-SKIP and pass:
  - SGR wheel-up / wheel-down
  - SGR left/middle/right press + release
  - SGR motion events
  - X10 left press
  - X10 wheel-up / wheel-down
  - Mouse modifier bits (Shift, Meta, Ctrl on button byte)
  - Double-click synthesis (two presses within window)
  - Burst: inject 3 complete events in one call; verify all three dequeue

**Done criterion**: mousewheel works in a live REPL session. PTY-replay tests
pass. No regression on cursor-key tests from M2.

**LOC estimate**: ~200 LOC decoder + ~150 LOC tests.

**Dependencies**: M2.

---

### M4 -- Bracketed paste

**What**: Implement paste buffer, `BOXEN_EV_PASTE`, and REPL paste handler.

**Deliverables**:
- `frontier-cli/boxen/boxen.h` -- add `BOXEN_EV_PASTE` to `boxen_event_type_t`;
  add `paste` union arm to `boxen_event_t`
- `frontier-cli/boxen/input_decoder.c` -- `PASTE_ACTIVE` state; paste buffer
  (heap-allocated, grown as needed up to 256 KB cap); emit `BOXEN_EV_PASTE` with
  heap-allocated `data` field (caller `free()`s)
- `frontier-cli/boxen_repl.c` -- handle `BOXEN_EV_PASTE` in
  `boxen_repl_run_one_tick()`: insert `ev.paste.data` at cursor; call `free(ev.paste.data)`
- `tests/input_decoder_tests.c` -- un-SKIP and pass:
  - Simple paste (`\e[200~hello\e[201~`)
  - Paste with newlines (CR/LF normalization)
  - Paste at size limit (256 KB): verify truncation + BOXEN_LOG_W
  - Paste with embedded ESC sequences (must be treated as literal text, not parsed)
- `tests/boxen_repl_tests.c` -- add test for `BOXEN_EV_PASTE` dispatch path
  (inject via mock; verify input bar updated)

**Done criterion**: Cmd-V paste works in a live REPL session with mouse mode
enabled. The paste content appears at the cursor without garbling.
`boxen_event_t` ABI change is contained to this milestone; document in PR body.

**LOC estimate**: ~100 LOC decoder + ~60 LOC REPL handler + ~100 LOC tests.

**Dependencies**: M3 (decoder infrastructure); does NOT require M6 cutover.

---

### M5 -- Kitty keyboard protocol

**What**: Detect kitty support, enable progressive enhancement, decode CSI-u form.

**Deliverables**:
- `frontier-cli/boxen/input_decoder.c` -- `input_decoder_kitty_enable()`: write
  `\e[=1u` (disambiguate-escape flag) to TTY fd; record that Kitty mode was
  requested. In the CSI decode path, add `case 'u':` to `decode_csi()` to handle
  `\e[KEYCODE;MODIFIER u` form; map to `boxen_event_t` using the Kitty modifier
  encoding (which matches the CSI modifier table).
- `tests/input_decoder_tests.c` -- un-SKIP and pass:
  - `\e[65u` (A without modifier) -> ch='A'
  - `\e[65;3u` (A with Alt) -> ch='A', mod=ALT
  - `\e[13;5u` (Enter with Ctrl) -> KEY_ENTER, mod=CTRL
  - `\e[27;3u` (Escape with Alt) -> KEY_ESCAPE, mod=ALT
  - Kitty enable followed by CSI-u keys: full decode round-trip

**Notes**: Kitty protocol response detection (does the terminal actually send `\e[=0u`
back?) is handled in `tb2_init` via a probe-and-read with a 200ms timeout. If no
response, the decoder treats `input_decoder_kitty_enable()` as a no-op and
continues with standard CSI parsing (which already handles most cases correctly).

**Done criterion**: On a kitty terminal, Option-arrows and modified function keys
produce correct events. On Terminal.app and iTerm2, the kitty probe times out
cleanly and the decoder falls back to standard CSI -- no regression.

**LOC estimate**: ~80 LOC decoder + ~80 LOC tests.

**Dependencies**: M2 (CSI decoder base).

---

### M6 -- Cutover: replace `tb2_poll_event` with the custom decoder

**What**: Wire the decoder into `backend_tb2.c` as the poll implementation.

This is the milestone that actually fixes issue #805 in production. All prior
milestones build and test the decoder in isolation; M6 makes it the live path.

**Deliverables**:
- `frontier-cli/boxen/backend_tb2.c`:
  - Add `#include "input_decoder.h"`
  - Add `static input_decoder_t *g_decoder = NULL;`
  - `tb2_init()`: after `tb_init()`, obtain the TTY fd via `tb_get_fds()` (or
    open `/dev/tty` directly); call `input_decoder_create(tty_fd)`.
  - `tb2_shutdown()`: call `input_decoder_destroy(g_decoder); g_decoder = NULL;`
    BEFORE `tb_shutdown()`.
  - `tb2_poll_event()`: replace the `tb_peek_event()` call with
    `input_decoder_poll(g_decoder, out_raw, timeout_ms)` where `out_raw` is a
    local `boxen_event_t` already filled by the decoder (no translation needed:
    the decoder emits `boxen_event_t` directly).
  - Remove `translate_key()`, `translate_mod()`, `translate_mouse()` (the decoder
    subsumes all of this). Keep `translate_attr()` and `translate_color()`
    (rendering-side; not affected).
  - The `g_tb2_last_press` double-click struct is also removed; the decoder owns
    it internally now.
- Confirm `tb_set_input_mode(TB_INPUT_MOUSE)` is NOT called anywhere in the init
  path (mouse starts disabled per section 5.1).
- All existing tests must remain green (PTY-replay tests pass because
  `input_decoder_inject_bytes()` bypasses the real TTY fd).

**Done criterion**: Full test suite green (unit + integration). Manual smoke
checklist passes. The reverted PR #808 symptoms are gone: Option-arrows work,
mousewheel works, native selection works by default.

**LOC estimate**: ~60 LOC changed in `backend_tb2.c` (net deletion). Translator
functions removed (~60 LOC deleted). Net negative lines.

**Dependencies**: M2 (cursor keys) + M3 (mouse) + M4 (paste) -- all decoder
features should land before cutover so the cutover is atomic.

---

### M7 -- Mouse-mode policy + `/mouse` toggle

**What**: Implement the on-demand mouse enable/disable logic and the `/mouse`
slash command.

**Deliverables**:
- `frontier-cli/boxen_repl.c` -- on `/mouse on`: call
  `input_decoder_set_mouse(true)` via the tb2 backend shim; update footer hint.
  On `/mouse off`: call `input_decoder_set_mouse(false)`.
  Auto-enable when palette opens; auto-restore when palette closes (if the user
  has not explicitly set `/mouse on`).
- `frontier-cli/repl.c` -- register `/mouse` as a slash command (alongside
  existing `/jump`, `/list`, etc.).
- `frontier-cli/boxen/backend_tb2.c` -- expose a passthrough:
  `void boxen_tb2_set_mouse(bool enable)` that calls
  `input_decoder_set_mouse(g_decoder, enable)`. Called from boxen layer above.
  Alternatively: add `set_mouse` to the `boxen_backend_t` vtable (preferred for
  testability). The vtable already has 11 slots; a 12th is a public API change.
  Decision deferred to implementation -- if vtable extension is not warranted,
  use the backend-specific accessor pattern.
- `tests/boxen_repl_tests.c` -- test `/mouse on` / `/mouse off` dispatch.
- `tools/tui-tests/tests/test_XX_mouse_toggle.py` -- TUI harness test: type
  `/mouse on`; verify footer updates; type `/mouse off`; verify footer reverts.

**Done criterion**: `/mouse on` enables mouse mode (palette clickable, mousewheel
scrolls); `/mouse off` restores native selection. Footer reflects state. TUI test
green.

**LOC estimate**: ~100 LOC REPL + ~50 LOC backend + ~80 LOC tests.

**Dependencies**: M6 (decoder must be the live path before toggle is meaningful).

---

## 8. Risk Register

### R1 -- termbox2's `tb_get_fds()` may not exist in the vendored version

**Risk**: The decoder needs the TTY fd that termbox2 obtained. `tb_get_fds()` was
added in termbox2 v2.3+. The vendored copy at
`frontier-cli/third_party/termbox2/termbox2.h` may be older.

**Mitigation**: M6 agent checks for `tb_get_fds()` existence. If absent, the
decoder opens `/dev/tty` directly (same as what termbox2 does internally). This
is a well-documented fallback pattern in the termbox2 community. Risk level: LOW.

**Rollback**: If both approaches fail, M6 is blocked until the vendored termbox2
is updated. The prior milestones (M1-M5) are unaffected; decoder development
continues against the inject-bytes seam.

### R2 -- `BOXEN_EV_PASTE` is a public ABI change

**Risk**: Adding `BOXEN_EV_PASTE` to `boxen_event_type_t` and a new union arm
to `boxen_event_t` is a struct layout change. Any code that uses `sizeof(boxen_event_t)`
or initializes a `boxen_event_t` with a designated initializer for the union will
need review.

**Mitigation**: `boxen_event_t` is already a tagged union. Adding a new tag and
union arm does not change the size of the union (the new `paste` arm is two
pointer-size fields, likely smaller than the existing `resize` arm: {int,int}).
Existing code that does `switch (ev->type)` and has a `default:` case will
silently ignore PASTE events -- correct behavior. M4 agent audits all
`switch (ev->type)` sites before landing.

**Rollback**: If the ABI change causes unexpected downstream issues, M4 can be
reverted independently. M1-M3 and M5-M6 do not depend on BOXEN_EV_PASTE.

### R3 -- Mouse enable/disable timing: screen corruption window

**Risk**: The moment between sending `\e[?1006h` (enable SGR mouse) and the
terminal acknowledging it, a resize event or spurious byte could be misinterpreted.

**Mitigation**: The decoder sends the enable sequence as a synchronous write, then
immediately begins parsing. Any bytes in flight before the terminal processed the
enable are in the old (X10) format; the decoder's X10 fallback handles them. In
practice the window is sub-millisecond and has no user-visible effect.

**Rollback**: If systematic corruption is observed, the mouse-enable handshake can
add a brief `select()` drain (10ms) between the write and the start of parsing.
This is well-understood tuning, not a design change.

### R4 -- PR #808 regression repeat: modifier keys silently broken

**Risk**: M6 cutover could introduce a different silent failure mode where some key
combination emits the wrong event. PR #808 was reverted precisely because the
breakage (Option-arrows, mousewheel) was not caught by existing tests.

**Mitigation**: The PTY-replay test harness (M1) exists specifically to prevent
this. Every entry in the protocol coverage matrix (section 3) has a test case
that must pass before M6 merges. The /gate bar-raiser and security reviewers
also run on M6.

**Gate**: M6 PR description must include a link to the PTY-replay test run output
showing all tests green. A /gate failure on M6 blocks the merge -- no exceptions.

### R5 -- Bracketed paste buffer overflow

**Risk**: A malicious or accidental very large paste could exhaust memory.

**Mitigation**: The decoder hard-caps the paste buffer at 256 KB and emits a
`BOXEN_LOG_W` on truncation. The cap is a compile-time constant so it can be
adjusted. The heap allocation uses the boxen allocator vtable (or system malloc
in the backend context) and checks for NULL. M4 includes a test for the truncation
path.

### R6 -- Kitty probe hangs on non-responsive terminal (M5)

**Risk**: `input_decoder_kitty_enable()` sends `\e[?u` and waits for a response.
If the terminal does not respond, the poll with timeout could block the REPL
startup for 200ms.

**Mitigation**: The probe timeout is 200ms and uses `select()` -- it will
not hang. 200ms is imperceptible to the user. The probe only runs once at init.
If the terminal never responds, the decoder operates in non-Kitty mode for the
session.

### R7 -- `BOXEN_EV_PASTE` memory leak if caller forgets to `free()`

**Risk**: The paste data is heap-allocated by the decoder and passed to the caller
via `ev.paste.data`. If any caller's `switch (ev->type)` does not handle
`BOXEN_EV_PASTE` it leaks the allocation.

**Mitigation**: `boxen_dispatch_event()` (`boxen.c`) is the universal dispatcher.
It will be audited in M4 to free `ev.paste.data` after dispatching (or before
returning if the event is discarded because no window's `input_fn` handled it).
The `input_fn` callbacks do NOT need to free -- only `boxen_dispatch_event` does.
This is documented in the `boxen.h` comment for `BOXEN_EV_PASTE`.

### 8.1 Rollback policy

If any milestone lands and produces a regression that slips through /gate:
1. File a P0 issue immediately.
2. If the regression is in user-facing input behavior (wrong key emitted, REPL
   hangs, native selection broken), revert the milestone PR via `git revert`.
3. The PTY-replay test harness must have a failing test added BEFORE the revert
   is re-landed. This is the lesson from PR #808.

---

## 9. /fleet Brief

```
FLEET BRIEF -- Option C input decoder
Issue: #805 (Option-arrows + mousewheel + copy-paste regression from #808)
Plan: planning/phase_c/INPUT_DECODER_PLAN.md
Base branch: develop
Merge strategy: --squash (one squash-commit per milestone)

Milestones (sequential -- each depends on prior):
  M1: PTY-replay harness + decoder stub         -- Closes: TBD (file as part of M1 PR)
  M2: CSI cursor keys + modifier params + ESC   -- Closes: #805 (partially; full fix in M6)
  M3: SGR mouse + X10 fallback + burst-read     -- Closes: TBD
  M4: Bracketed paste + BOXEN_EV_PASTE          -- Closes: TBD
  M5: Kitty keyboard protocol enable + decode   -- Closes: TBD
  M6: Cutover (replace tb2_poll_event)          -- Closes: #805 (fully)
  M7: Mouse-mode policy + /mouse toggle         -- Closes: TBD

Per-milestone constraints (apply to every dispatch):

  WORKING DIRECTORY: /Users/jake/dev/jsavin/Frontier
  BASE BRANCH: develop

  TDD order (mandatory):
    1. Write PTY-replay tests for the milestone's protocol entries (section 3)
    2. Run tests -- confirm they FAIL for the right reason (link error is wrong reason)
    3. Write decoder / integration code
    4. Run tests -- confirm they PASS
    5. Run full unit suite: ./tools/run_headless_tests.sh
    6. Run integration suite: cd tests && make test-integration
    7. Manual smoke checklist from section 6.4 for this milestone

  Protocol scope: reference INPUT_DECODER_PLAN.md section 3 for the sequence
    table and section 7 for per-milestone coverage requirements.

  Do NOT touch rendering path:
    tb_set_cell, tb_present, tb_clear, tb_width, tb_height are off-limits.
    Do NOT call tb_set_input_mode(TB_INPUT_MOUSE) -- mouse enable is owned by
    the decoder (input_decoder_set_mouse).

  Coding standards:
    - ASCII-only in commit messages, code comments, identifiers
    - Tabs (not spaces) for C indentation
    - K&R brace style
    - 100-column soft limit
    - Date-prefixed comment headers: /* YYYY-MM-DD JES #805 MN: ... */
    - snake_case for C identifiers

  /gate expectations per milestone:
    - bar-raiser: always
    - security: always
    - concurrency: auto (triggers on GIL, pthread, signal patterns)
    - swiftui: excluded

  Rollback gate (M6 specific):
    The M6 PR description MUST include the PTY-replay test run output showing
    all tests green. /gate failure on M6 blocks merge -- no exceptions.
    If M6 regresses any input behavior, revert immediately and add a failing
    PTY-replay test before re-attempting.
```

---

*End of plan.*
