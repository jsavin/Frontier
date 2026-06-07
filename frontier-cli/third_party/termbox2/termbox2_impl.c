/*
 * termbox2_impl.c -- single translation unit that compiles the termbox2
 * implementation.
 *
 * termbox2 v2.5.0 is a single-header library. Defining TB_IMPL before the
 * include activates the implementation code. This file is that one TU;
 * all other files in the project include termbox2.h WITHOUT TB_IMPL to get
 * declarations only.
 *
 * TB_IMPL is defined here (not via Makefile flag) because the frontier-cli
 * main build compiles all .c files in a single clang invocation, making
 * per-file Makefile CFLAGS overrides ineffective for this purpose.
 *
 * Upstream: https://github.com/termbox/termbox2 (tag v2.5.0, SHA 9b5a5da)
 * See VERSION in this directory for pin details.
 */

/* Upstream warning suppressions (per-file, do NOT leak to project):
 * None needed for v2.5.0 under clang -std=c17 -Wall -Wextra on macOS.
 * Add targeted #pragma clang diagnostic push/pop here if a future
 * upstream upgrade introduces warnings. */

#define TB_IMPL
#include "termbox2.h"
