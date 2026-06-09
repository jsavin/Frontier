/*
 * boxen_repl.h -- public API for the boxen-native Frontier REPL.
 *
 * C.0 tracer-bullet: minimum-viable boxen REPL with scrollback output pane
 * and a single-line input bar at the bottom.
 *
 * Consumer-side entry point.  Threading contract mirrors debugger_tui.h:
 * boxen_repl_main() is called from the GIL holder (main.c dispatch block).
 * It releases and reacquires the GIL around boxen_poll_event() calls,
 * following the same pattern as debugger_tui_main().
 *
 * 2026-06-08 JES Phase C.0 #691
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Frontier contributors
 */

#ifndef BOXEN_REPL_H
#define BOXEN_REPL_H

#include "cli_parser.h"

/*
 * Entry point for the boxen-native Frontier REPL.
 *
 * Initializes boxen, creates a two-region layout (scrollback pane + input
 * line), enters the event loop with GIL release/reacquire around each
 * boxen_poll_event(), exits on Ctrl-C, then cleans up and returns.
 *
 * If opts->script_file or opts->inline_script is set, the REPL dispatches
 * "/debug <path>" automatically after startup to preserve the PR #756
 * launch-entry-point contract.
 *
 * Returns 0 on clean exit, non-zero on initialization failure.
 */
int boxen_repl_main(const cli_options_t *opts);

#endif /* BOXEN_REPL_H */
