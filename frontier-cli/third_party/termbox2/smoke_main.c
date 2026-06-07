/*
 * smoke_main.c -- termbox2 portability smoke test entry point.
 *
 * Does NOT call tb_init() (which requires a TTY). Exercises link-time
 * symbol resolution only. A successful compile + exit(0) proves the
 * library links and the TB_IMPL translation unit is correct.
 *
 * Used by the tb2-smoke Makefile target. See frontier-cli/Makefile.
 */

#include <stdio.h>
#include <stdint.h>

/* Include declarations only (no TB_IMPL here; termbox2_impl.c owns it). */
#include "termbox2.h"

int main(void) {
	/* tb_utf8_char_length: no TTY required, exercises link-time resolution. */
	int len = tb_utf8_char_length('A');
	printf("termbox2 vendored OK (tb_utf8_char_length('A') = %d)\n", len);
	return 0;
}
