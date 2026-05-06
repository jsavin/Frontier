/*
 * mouse_parser_tests.c - Unit tests for SGR 1006 mouse-event parser.
 *
 * Verifies mouse_parse() against the xterm SGR 1006 protocol:
 *   ESC [ < Cb ; Cx ; Cy ('M' = press, 'm' = release)
 * Cb low 2 bits = button (0=left, 1=middle, 2=right, 3=release-of-any),
 * bit 6 (value 64) = wheel events (64=up, 65=down). Cx/Cy are 1-based
 * terminal coordinates.
 *
 * Tests cover the full button matrix plus malformed-input rejection. The
 * parser MUST be safe against truncated / non-null-terminated buffers —
 * the slice provided to mouse_parse() may not be NUL-bounded.
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "pane.h"
#include "test_report.h"

static bool parse_str(const char *s, mouse_event_t *out) {
	return mouse_parse(s, strlen(s), out);
}

static void test_valid_press_left(void) {
	mouse_event_t ev;
	memset(&ev, 0, sizeof(ev));
	bool ok = parse_str("\x1b[<0;10;5M", &ev);
	assert(ok);
	assert(ev.btn == MOUSE_LEFT);
	assert(ev.x == 10);
	assert(ev.y == 5);
	assert(ev.press == true);
}

static void test_valid_release_left(void) {
	mouse_event_t ev;
	memset(&ev, 0, sizeof(ev));
	bool ok = parse_str("\x1b[<0;10;5m", &ev);
	assert(ok);
	assert(ev.btn == MOUSE_LEFT);
	assert(ev.x == 10);
	assert(ev.y == 5);
	assert(ev.press == false);
}

static void test_wheel_up(void) {
	mouse_event_t ev;
	memset(&ev, 0, sizeof(ev));
	bool ok = parse_str("\x1b[<64;10;5M", &ev);
	assert(ok);
	assert(ev.btn == MOUSE_WHEEL_UP);
	assert(ev.x == 10);
	assert(ev.y == 5);
	assert(ev.press == true);
}

static void test_wheel_down(void) {
	mouse_event_t ev;
	memset(&ev, 0, sizeof(ev));
	bool ok = parse_str("\x1b[<65;10;5M", &ev);
	assert(ok);
	assert(ev.btn == MOUSE_WHEEL_DOWN);
}

static void test_middle_and_right(void) {
	mouse_event_t ev;
	memset(&ev, 0, sizeof(ev));
	bool ok = parse_str("\x1b[<1;3;4M", &ev);
	assert(ok);
	assert(ev.btn == MOUSE_MIDDLE);
	assert(ev.x == 3);
	assert(ev.y == 4);
	assert(ev.press == true);

	memset(&ev, 0, sizeof(ev));
	ok = parse_str("\x1b[<2;7;8m", &ev);
	assert(ok);
	assert(ev.btn == MOUSE_RIGHT);
	assert(ev.x == 7);
	assert(ev.y == 8);
	assert(ev.press == false);
}

static void test_large_coordinates(void) {
	mouse_event_t ev;
	memset(&ev, 0, sizeof(ev));
	bool ok = parse_str("\x1b[<0;200;120M", &ev);
	assert(ok);
	assert(ev.x == 200);
	assert(ev.y == 120);
}

static void test_malformed_inputs_rejected(void) {
	mouse_event_t ev;
	/* Missing terminator. */
	assert(!parse_str("\x1b[<0;10;5", &ev));
	/* Missing '<'. */
	assert(!parse_str("\x1b[0;10;5M", &ev));
	/* Missing leading ESC. */
	assert(!parse_str("[<0;10;5M", &ev));
	/* Missing semicolon. */
	assert(!parse_str("\x1b[<010;5M", &ev));
	/* Non-digit content. */
	assert(!parse_str("\x1b[<0;1a;5M", &ev));
	/* Too short. */
	assert(!mouse_parse("\x1b[<", 3, &ev));
	/* NULL inputs. */
	assert(!mouse_parse(NULL, 5, &ev));
	assert(!mouse_parse("\x1b[<0;1;1M", 9, NULL));
	/* Empty. */
	assert(!mouse_parse("", 0, &ev));
}

static void test_buffer_bounded_no_overrun(void) {
	mouse_event_t ev;
	/* len shorter than the apparent string — parser must respect len,
	 * not run off into unspecified memory after a missing terminator. */
	const char *seq = "\x1b[<0;10;5";  /* 9 bytes, no terminator */
	assert(!mouse_parse(seq, 9, &ev));

	/* Fully terminated within len. */
	const char *seq2 = "\x1b[<0;10;5M";
	assert(mouse_parse(seq2, 10, &ev));
	assert(ev.btn == MOUSE_LEFT);
}

int main(void) {
	TR_INIT("mouse_parser_tests");
	TR_RUN(test_valid_press_left);
	TR_RUN(test_valid_release_left);
	TR_RUN(test_wheel_up);
	TR_RUN(test_wheel_down);
	TR_RUN(test_middle_and_right);
	TR_RUN(test_large_coordinates);
	TR_RUN(test_malformed_inputs_rejected);
	TR_RUN(test_buffer_bounded_no_overrun);
	TR_SUMMARY();
	return TR_EXIT_CODE();
}
