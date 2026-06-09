/*
 * boxen_outline.h -- public API for the boxen outline editor window.
 *
 * C.1: read-only outline editor opened via /edit [path] REPL slash command.
 *
 * Entry points:
 *   boxen_outline_open(path) -- open an editor for the given ODB path.
 *   boxen_outline_close_all() -- close all editor windows (called on REPL exit).
 *
 * Multiple editor windows are supported up to BOXEN_OUTLINE_MAX_EDITORS.
 * Opening the same path twice raises the existing window instead of duplicating.
 *
 * Threading: all public functions must be called while holding the GIL.
 *
 * 2026-06-09 JES Phase C.1 #691
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Frontier contributors
 */

#ifndef BOXEN_OUTLINE_H
#define BOXEN_OUTLINE_H

#include <stdbool.h>
#include <stddef.h>

/* Maximum number of simultaneously open editor windows.
 * Chosen conservatively for C.1; expand in a later milestone if needed. */
#define BOXEN_OUTLINE_MAX_EDITORS 8

/*
 * boxen_outline_open -- open an outline editor for the given ODB path.
 *
 * path -- dotted ODB path; "@" prefix is optional and stripped.
 *         May be relative (resolved against REPL's current table) or
 *         absolute (starts with a known root name).
 *
 * Returns true on success (editor opened or raised).
 * Returns false if the path is invalid, the ODB object is not an outline,
 * or the open-editors registry is full.
 *
 * The editor is read-only for C.1.  F9/Cmd-/ mutations are committed
 * immediately to the outline record under GIL (no buffering).
 */
bool boxen_outline_open(const char *path);

/*
 * boxen_outline_close_all -- close all open editor windows.
 *
 * Called from boxen_repl_main teardown before state is freed.
 * Safe to call when no editors are open (no-op).
 */
void boxen_outline_close_all(void);

#endif /* BOXEN_OUTLINE_H */
