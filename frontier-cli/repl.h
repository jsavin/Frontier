/*
 * Frontier CLI - REPL Interface
 * Phase 1: Basic REPL Foundation
 *
 * repl.h - Main entry point for the Read-Eval-Print Loop interactive mode
 *
 * Copyright (C) 1992-2004 UserLand Software, Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef REPL_H
#define REPL_H

#include "cli_parser.h"
#include "../Common/headers/frontier.h"
#include "../Common/headers/lang.h"  /* For hdlhashtable */

// Main REPL entry point
// Returns: exit code (0 for success, 1 for error)
int repl_main(cli_options_t *options);

/* REPL Navigation - similar to CWD in a shell */

/* Get the current REPL table (like CWD). Returns roottable if at root. */
hdlhashtable repl_get_current_table(void);

/* Get the current REPL path as a string. Empty string means root. */
const char *repl_get_current_path(void);

/* Navigate to a path. Accepts dot-paths with or without leading @.
 * Returns true on success, false if path doesn't exist or isn't a table.
 */
boolean repl_jump_path(const char *path);

#endif // REPL_H
