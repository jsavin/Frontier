/*
 * completion.h - REPL Tab Completion Engine
 *
 * Provides intelligent tab completion for the Frontier CLI REPL:
 * - Phase 1: UserTalk language keywords
 * - Phase 2: Database-defined names
 * - Phase 3: Dotted path navigation (system.verbs.string)
 * - Phase 4: Context-aware completion (file. shows file.* verbs)
 *
 * Copyright (C) 1992-2004 UserLand Software, Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef COMPLETION_H
#define COMPLETION_H

#include <stdbool.h>
#include <stddef.h>
#include "../Common/headers/frontier.h"
#include "../Common/headers/lang.h"

/*
 * Maximum number of completion matches to collect.
 * Limits memory usage and display clutter.
 */
#define COMPLETION_MAX_MATCHES 100

/*
 * Maximum length of a completion match name.
 */
#define COMPLETION_MAX_NAME_LEN 256

/*
 * Maximum depth for dotted path navigation.
 * Prevents infinite recursion on circular references.
 */
#define COMPLETION_MAX_PATH_DEPTH 10

/*
 * Completion match structure.
 * Represents a single completion candidate.
 */
typedef struct completion_match {
    char name[COMPLETION_MAX_NAME_LEN];  /* Matched name */
    tyvaluetype type;                     /* Value type (for display hints) */
    bool is_table;                        /* True if navigable (table/external) */
} completion_match_t;

/*
 * Completion matches collection.
 * Holds all matching candidates for current completion.
 */
typedef struct completion_matches {
    completion_match_t items[COMPLETION_MAX_MATCHES];
    size_t count;
    char common_prefix[COMPLETION_MAX_NAME_LEN];  /* Longest common prefix */
} completion_matches_t;

/*
 * Completion context type.
 * Determines what kind of completion to perform.
 */
typedef enum {
    COMPLETION_CTX_GENERAL,       /* Default: keywords + database names */
    COMPLETION_CTX_VERB_CALL,     /* After verb processor (file., db., etc.) */
    COMPLETION_CTX_ADDRESS,       /* After @ symbol */
    COMPLETION_CTX_STRING,        /* Inside string literal (no completion) */
} completion_context_type_t;

/*
 * Completion context structure.
 * Parsed from the current line being edited.
 */
typedef struct completion_context {
    const char *line;             /* Full line being edited */
    int cursor_pos;               /* Cursor position in line */
    char token[COMPLETION_MAX_NAME_LEN];      /* Token being completed */
    char table_path[COMPLETION_MAX_NAME_LEN]; /* Table path prefix for dotted paths */
    char leaf_prefix[COMPLETION_MAX_NAME_LEN]; /* Leaf name prefix for dotted paths */
    completion_context_type_t ctx_type;        /* Context type */
    bool has_dot;                 /* True if token contains a dot */
} completion_context_t;

/*
 * Initialize the completion engine.
 * Call once at REPL startup.
 * Returns true on success.
 */
bool completion_init(void);

/*
 * Cleanup the completion engine.
 * Call at REPL shutdown.
 */
void completion_cleanup(void);

/*
 * Initialize a matches collection.
 */
void completion_matches_init(completion_matches_t *matches);

/*
 * Add a match to the collection.
 * Returns false if collection is full.
 */
bool completion_matches_add(completion_matches_t *matches,
                            const char *name,
                            tyvaluetype type,
                            bool is_table);

/*
 * Compute the common prefix of all matches.
 * Stored in matches->common_prefix.
 */
void completion_compute_common_prefix(completion_matches_t *matches);

/*
 * Parse completion context from line buffer.
 */
void completion_parse_context(const char *line, int cursor_pos, completion_context_t *ctx);

/*
 * Phase 1: Add keyword matches.
 */
void completion_add_keywords(completion_matches_t *matches, const char *prefix);

/*
 * Phase 2: Add database name matches from a hash table.
 */
void completion_add_table_entries(completion_matches_t *matches,
                                  hdlhashtable table,
                                  const char *prefix);

/*
 * Phase 2.5: Search system.paths for a name.
 * Returns the table if found via system.paths, nil otherwise.
 * Used as fallback when direct roottable lookup fails.
 */
hdlhashtable completion_search_paths(const char *name);

/*
 * Phase 2.5: Search system.paths for a name, returning full resolved path.
 * Like completion_search_paths but also fills resolved_path with the full
 * path (e.g., "system.verbs.builtins.fileMenu" for name "fileMenu").
 *
 * Optional guest database output parameters (pass NULL to ignore):
 *   out_guest_db_root - If match came from filewindowtable, set to the guest DB's root table
 *   out_guest_db_name - If match came from filewindowtable, filled with the DB name (e.g., "mainResponder.root")
 *   db_name_bufsize   - Size of the out_guest_db_name buffer
 *
 * When a match comes from a guest database:
 *   - resolved_path is set to just the entry name (not the full db.entry path)
 *   - out_guest_db_root is set to the guest DB's root table
 *   - out_guest_db_name is filled with the filewindowtable key
 */
hdlhashtable completion_search_paths_ex(const char *name, char *resolved_path, size_t path_bufsize,
                                         hdlhashtable *out_guest_db_root, char *out_guest_db_name, size_t db_name_bufsize);

/*
 * Phase 2.5: Add matching entries from all system.paths tables.
 * Makes path-accessible names available for top-level completion.
 */
void completion_add_path_entries(completion_matches_t *matches, const char *prefix);

/*
 * Phase 3: Navigate to a table by dotted path starting from an arbitrary table.
 * Unlike completion_navigate_path(), does NOT search system.paths as fallback.
 * Returns nil if path is invalid or not a table.
 */
hdlhashtable completion_navigate_dotpath_from(hdlhashtable start_table, const char *path);

/*
 * Phase 3: Navigate to a table by dotted path.
 * Returns nil if path is invalid or not a table.
 * For single-component paths, also searches system.paths as fallback.
 */
hdlhashtable completion_navigate_path(const char *path);

/*
 * Phase 4: Detect completion context from line.
 */
completion_context_type_t completion_detect_context(const char *line, int cursor_pos);

/*
 * Display completion matches to terminal.
 */
void completion_display_matches(const completion_matches_t *matches);

#endif /* COMPLETION_H */
