/*
 * repl_slash_dispatch.h -- exposes dispatch_slash_command from repl.c.
 *
 * Prior to Phase C.0, dispatch_slash_command was a static function in repl.c.
 * The boxen REPL (boxen_repl.c) needs to call it directly to route "/" prefixed
 * input lines through the same menubar-based dispatch path the linenoise REPL uses.
 *
 * This header declares the de-static'd function.  Only boxen_repl.c and
 * repl.c should include this header (repl.c implicitly via the definition).
 *
 * The function is NOT part of the public repl.h API (that header is large
 * enough already and this function is an implementation detail).
 *
 * 2026-06-08 JES Phase C.0 #691
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Frontier contributors
 */

#ifndef REPL_SLASH_DISPATCH_H
#define REPL_SLASH_DISPATCH_H

#include "../Common/headers/frontier.h"  /* boolean */

/*
 * dispatch_slash_command -- process a slash-prefixed REPL input line.
 *
 * Routes the line through the REPL menubar dispatch infrastructure.
 * Handles argument injection for accepts_args=true leaves.
 *
 * Parameters:
 *   line    -- NUL-terminated input line; caller has already verified line[0] == '/'
 *   running -- OUT: set to false if the REPL should exit (e.g. /exit command)
 *
 * Returns true if the REPL should keep running; sets *running = false on /exit.
 *
 * GIL: must be called while holding the GIL.
 * Thread safety: touches file-level statics in repl.c (g_repl_dispatching_from_slash,
 *   g_repl_exit_requested); single-threaded with GIL held.
 *
 * Side effect: if the /exit command is processed, the replverbhost_exit kernel-verb
 *   host adapter sets g_repl_exit_requested = 1 (a file-static in repl.c).
 *   Callers that drive the REPL event loop via a separate quit flag (e.g.,
 *   boxen_repl_main's s->should_quit) should drive exit from this function's
 *   return value and *running, not by polling g_repl_exit_requested directly.
 *   Use repl_reset_exit_flag() (repl.h) to clear stale state at session start.
 *
 * 2026-06-08 JES #691 Phase C.0 round 2 P2-18
 */
boolean dispatch_slash_command(const char *line, boolean *running);

#endif /* REPL_SLASH_DISPATCH_H */
