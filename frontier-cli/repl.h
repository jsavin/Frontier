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

/* Result from index-aware path navigation (repl_navigate_path_ex).
 * Can represent either a table or a scalar value at the end of a path. */
typedef struct {
    hdlhashtable htable;    /* non-nil if result is a table */
    tyvaluerecord val;      /* the value (valid for both table and scalar results) */
    hdlhashnode hnode;      /* the node containing the value */
    boolean is_table;       /* true if result is a navigable table */
} typathlookupresult;

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

/* Resolve a path to a table without changing current table.
 * Accepts dot-paths, addresses, system.paths names, or script expressions.
 * Supports [n] index syntax (1-based) and relative paths.
 * If resolved_path is non-NULL, fills it with the actual resolved path
 * (e.g., "system.verbs.builtins" for "parentOf(fileMenu)").
 * Returns the resolved table, or nil if path is invalid.
 */
hdlhashtable repl_resolve_path(const char *path, char *resolved_path, size_t path_bufsize);

/* Extended path resolution that returns both tables and scalar values.
 * Supports [n] index syntax (1-based) and relative paths.
 * If result->is_table is true, the path resolved to a table (in result->htable).
 * If result->is_table is false, the path resolved to a scalar (in result->val).
 * If resolved_path is non-NULL, fills it with the resolved path string.
 * Returns true if path resolved successfully, false on error.
 * On error, sets error_msg (if non-NULL) to describe the problem.
 */
boolean repl_resolve_path_ex(const char *path, typathlookupresult *result,
                              char *resolved_path, size_t path_bufsize,
                              char *error_msg, size_t error_bufsize);

/* Check if REPL mode is currently active.
 * Used by msg() to add "msg: " prefix in interactive mode.
 */
boolean repl_is_active(void);

#endif // REPL_H
