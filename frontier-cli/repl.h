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

// Main REPL entry point
// Returns: exit code (0 for success, 1 for error)
int repl_main(cli_options_t *options);

#endif // REPL_H
