/*
 * debugger_tui.h -- public API for the boxen-based UserTalk debugger TUI.
 *
 * Milestone B.0: tracer-bullet skeleton (planning/phase_b/EXECUTION_PLAN.md).
 * This is the consumer-side entry point; boxen.h is the substrate.
 *
 * Threading contract: debugger_tui_main() is called from the GIL holder
 * (main.c dispatch block). It releases and reacquires the GIL around
 * boxen_poll_event() calls, following the same pattern as protocol_main().
 *
 * 2026-06-06 JES Phase B.0 #691
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Frontier contributors
 */

#ifndef DEBUGGER_TUI_H
#define DEBUGGER_TUI_H

#include "cli_parser.h"

/*
 * Entry point for TUI debug mode. Matches the shape of protocol_main().
 *
 * Initializes boxen, creates a placeholder two-pane layout (script pane +
 * stack/locals pane) plus a pinned keybind footer, enters the event loop
 * with GIL release/reacquire around each boxen_poll_event(), exits on 'q',
 * Escape, or Ctrl-C, then cleans up transport and boxen.
 *
 * Returns 0 on clean exit, non-zero on initialization failure.
 */
int debugger_tui_main(const cli_options_t *opts);

#endif /* DEBUGGER_TUI_H */
