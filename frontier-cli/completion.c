/*
 * completion.c - REPL Tab Completion Engine Implementation
 *
 * Implements intelligent tab completion for the Frontier CLI REPL:
 * - Phase 1: UserTalk language keywords
 * - Phase 2: Database-defined names (via hashtablevisit)
 * - Phase 3: Dotted path navigation (system.verbs.string)
 * - Phase 4: Context-aware completion (file. shows file.* verbs)
 *
 * Copyright (C) 1992-2004 UserLand Software, Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "completion.h"
#include "../Common/headers/logging.h"
#include "../Common/headers/strings.h"
#include "../Common/headers/tablestructure.h"
#include "../Common/headers/langexternal.h"
#include "../Common/headers/langinternal.h"  /* hashresolvevalue */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Terminal width detection - POSIX only */
#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#include <sys/ioctl.h>
#define HAVE_IOCTL 1
#endif

/* ============================================================================
 * Phase 1: UserTalk Keywords
 * ============================================================================ */

/*
 * Static list of UserTalk language keywords for completion.
 * Sorted alphabetically for binary search (future optimization).
 */
static const char *usertalk_keywords[] = {
    /* Control flow */
    "break",
    "bundle",
    "case",
    "continue",
    "else",
    "for",
    "if",
    "loop",
    "return",
    "while",

    /* Declarations */
    "kernel",
    "local",
    "on",
    "syscall",
    "thread",

    /* Boolean/nil */
    "and",
    "false",
    "nil",
    "not",
    "or",
    "true",

    /* Type names (for typeof comparisons) */
    "addressType",
    "aliasType",
    "binaryType",
    "booleanType",
    "charType",
    "codeType",
    "dateType",
    "directionType",
    "doubleType",
    "externalvaluetype",
    "filespecType",
    "fixedType",
    "intType",
    "listType",
    "longType",
    "menuidType",
    "novaluetype",
    "objspecType",
    "oldstringType",
    "ostypeType",
    "outlineType",
    "patternType",
    "pictType",
    "pointType",
    "recordType",
    "rectType",
    "rgbType",
    "scriptType",
    "singleType",
    "stringType",
    "string4Type",
    "tableType",
    "tokenType",
    "unknownType",
    "wordType",
    "wptextType",

    NULL  /* Sentinel */
};

/* ============================================================================
 * Match Collection Management
 * ============================================================================ */

/* Initializes a match collection for use. */
void completion_matches_init(completion_matches_t *matches) {
    matches->count = 0;
    matches->common_prefix[0] = '\0';
}

/* Adds a completion match to the collection; returns false if collection is full. */
bool completion_matches_add(completion_matches_t *matches,
                            const char *name,
                            tyvaluetype type,
                            bool is_table) {
    if (matches->count >= COMPLETION_MAX_MATCHES) {
        return false;
    }

    completion_match_t *m = &matches->items[matches->count];
    strncpy(m->name, name, COMPLETION_MAX_NAME_LEN - 1);
    m->name[COMPLETION_MAX_NAME_LEN - 1] = '\0';
    m->type = type;
    m->is_table = is_table;
    matches->count++;

    return true;
}

/* Computes the longest common prefix shared by all matches for auto-expansion. */
void completion_compute_common_prefix(completion_matches_t *matches) {
    if (matches->count == 0) {
        matches->common_prefix[0] = '\0';
        return;
    }

    if (matches->count == 1) {
        strncpy(matches->common_prefix, matches->items[0].name, COMPLETION_MAX_NAME_LEN - 1);
        matches->common_prefix[COMPLETION_MAX_NAME_LEN - 1] = '\0';
        return;
    }

    /* Start with first match as prefix */
    strncpy(matches->common_prefix, matches->items[0].name, COMPLETION_MAX_NAME_LEN - 1);
    matches->common_prefix[COMPLETION_MAX_NAME_LEN - 1] = '\0';

    /* Shorten prefix until it matches all items */
    for (size_t i = 1; i < matches->count; i++) {
        size_t j = 0;
        while (matches->common_prefix[j] != '\0' &&
               tolower((unsigned char)matches->common_prefix[j]) ==
               tolower((unsigned char)matches->items[i].name[j])) {
            j++;
        }
        matches->common_prefix[j] = '\0';
    }
}

/* ============================================================================
 * Context Parsing
 * ============================================================================ */

/*
 * Check if character is valid in a UserTalk identifier.
 */
static bool is_ident_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

/*
 * Check if character is valid in a dotted path.
 */
static bool is_path_char(char c) {
    return is_ident_char(c) || c == '.';
}

/* Parses the input line at cursor position to extract completion context. */
void completion_parse_context(const char *line, int cursor_pos, completion_context_t *ctx) {
    ctx->line = line;
    ctx->cursor_pos = cursor_pos;
    ctx->token[0] = '\0';
    ctx->table_path[0] = '\0';
    ctx->leaf_prefix[0] = '\0';
    ctx->has_dot = false;
    ctx->ctx_type = COMPLETION_CTX_GENERAL;

    if (cursor_pos <= 0 || line == NULL) {
        return;
    }

    /* Find token start (scan backwards) */
    int token_start = cursor_pos;
    bool found_at = false;

    while (token_start > 0) {
        char c = line[token_start - 1];
        if (c == '@') {
            token_start--;
            found_at = true;
            break;
        }
        if (!is_path_char(c)) {
            break;
        }
        token_start--;
    }

    /* Extract token */
    int token_len = cursor_pos - token_start;
    if (token_len >= COMPLETION_MAX_NAME_LEN) {
        token_len = COMPLETION_MAX_NAME_LEN - 1;
    }
    memcpy(ctx->token, line + token_start, token_len);
    ctx->token[token_len] = '\0';

    /* Check for @ prefix */
    if (found_at || (token_len > 0 && ctx->token[0] == '@')) {
        ctx->ctx_type = COMPLETION_CTX_ADDRESS;
        /* Strip @ from token for matching */
        if (ctx->token[0] == '@') {
            memmove(ctx->token, ctx->token + 1, strlen(ctx->token) + 1);  /* +1 for null terminator */
        }
    }

    /* Check for dot (dotted path) */
    char *last_dot = strrchr(ctx->token, '.');
    if (last_dot != NULL) {
        ctx->has_dot = true;

        /* Split into table_path and leaf_prefix */
        size_t path_len = last_dot - ctx->token;
        if (path_len >= COMPLETION_MAX_NAME_LEN) {
            path_len = COMPLETION_MAX_NAME_LEN - 1;
        }
        memcpy(ctx->table_path, ctx->token, path_len);
        ctx->table_path[path_len] = '\0';

        strncpy(ctx->leaf_prefix, last_dot + 1, COMPLETION_MAX_NAME_LEN - 1);
        ctx->leaf_prefix[COMPLETION_MAX_NAME_LEN - 1] = '\0';
    } else {
        strncpy(ctx->leaf_prefix, ctx->token, COMPLETION_MAX_NAME_LEN - 1);
        ctx->leaf_prefix[COMPLETION_MAX_NAME_LEN - 1] = '\0';
    }

    /* Detect context type if not already address */
    if (ctx->ctx_type != COMPLETION_CTX_ADDRESS) {
        ctx->ctx_type = completion_detect_context(line, cursor_pos);
    }
}

/* ============================================================================
 * Phase 1: Keyword Completion
 * ============================================================================ */

/* Adds matching UserTalk keywords to the completion results. */
void completion_add_keywords(completion_matches_t *matches, const char *prefix) {
    size_t prefix_len = strlen(prefix);

    for (int i = 0; usertalk_keywords[i] != NULL; i++) {
        if (matches->count >= COMPLETION_MAX_MATCHES) {
            break;
        }

        const char *kw = usertalk_keywords[i];
        if (prefix_len == 0 || strncasecmp(kw, prefix, prefix_len) == 0) {
            completion_matches_add(matches, kw, novaluetype, false);
        }
    }
}

/* ============================================================================
 * Phase 2: Database Name Completion
 * ============================================================================ */

/* Adds matching entries from a hash table to the completion results.
 * Iterates in sorted (alphabetical) order using the table's sorted linked list. */
void completion_add_table_entries(completion_matches_t *matches,
                                  hdlhashtable table,
                                  const char *prefix) {
    if (table == nil) {
        return;
    }

    size_t prefix_len = strlen(prefix);
    hdlhashnode node = (**table).hfirstsort;

    while (node != nil && matches->count < COMPLETION_MAX_MATCHES) {
        /* Get the key name */
        bigstring bs;
        gethashkey(node, bs);

        /* Convert to C string */
        char name[COMPLETION_MAX_NAME_LEN];
        size_t len = stringlength(bs);
        if (len >= COMPLETION_MAX_NAME_LEN) {
            len = COMPLETION_MAX_NAME_LEN - 1;
        }
        memcpy(name, stringbaseaddress(bs), len);
        name[len] = '\0';

        /* Check prefix match (case-insensitive) */
        if (prefix_len == 0 || strncasecmp(name, prefix, prefix_len) == 0) {
            tyvaluetype type = (**node).val.valuetype;
            bool is_table = (type == externalvaluetype);

            /* For external values, check if it's a table */
            if (is_table) {
                tyexternalid exttype = langexternalgettype((**node).val);
                is_table = (exttype == idtableprocessor);
            }

            completion_matches_add(matches, name, type, is_table);
        }

        node = (**node).sortedlink;
    }
}

/* ============================================================================
 * Phase 2.5: system.paths Support
 * ============================================================================
 * system.paths contains address values pointing to tables that are in global
 * scope. This allows verbs like fileMenu, new(), defined() to work without
 * fully-qualified paths. We iterate through pathstable to:
 * 1. Find names for tab completion (completion_add_path_entries)
 * 2. Resolve single-component paths via path search (completion_search_paths)
 */

/* Searches system.paths for a name and returns the table, optionally with full path.
 * Used as a fallback when direct roottable lookup fails.
 *
 * For example, searching for "fileMenu":
 * - Iterates through pathstable entries
 * - For each entry (e.g., @system.verbs.builtins), resolves to table
 * - Looks up "fileMenu" in that table
 * - If found and it's a table, returns it
 * - If resolved_path is provided, fills it with "system.verbs.builtins.fileMenu"
 *
 * Parameters:
 *   name - The name to search for (e.g., "fileMenu")
 *   resolved_path - If non-NULL, filled with the full path (e.g., "system.verbs.builtins.fileMenu")
 *   path_bufsize - Size of the resolved_path buffer
 *
 * Returns the target table, or nil if not found.
 */
hdlhashtable completion_search_paths_ex(const char *name, char *resolved_path, size_t path_bufsize) {
    if (name == NULL || name[0] == '\0' || pathstable == nil) {
        return nil;
    }

    bigstring bsname;
    copyctopstring(name, bsname);

    hdlhashnode nomad = (**pathstable).hfirstsort;

    while (nomad != nil) {
        /* Skip non-address entries */
        if ((**nomad).val.valuetype != addressvaluetype) {
            nomad = (**nomad).sortedlink;
            continue;
        }

        /* Resolve unresolved addresses */
        if ((**nomad).flunresolvedaddress) {
            if (!hashresolvevalue(pathstable, nomad)) {
                nomad = (**nomad).sortedlink;
                continue;
            }
        }

        /* Get the table that this path entry points to */
        bigstring bs_local;
        hdlhashtable hparent;
        if (!getaddressvalue((**nomad).val, &hparent, bs_local)) {
            nomad = (**nomad).sortedlink;
            continue;
        }

        if (hparent == nil) {
            nomad = (**nomad).sortedlink;
            continue;
        }

        /* Resolve bs_local in hparent to get the actual target table */
        tyvaluerecord pathval;
        hdlhashnode pathnode;

        if (!hashtablelookup(hparent, bs_local, &pathval, &pathnode)) {
            nomad = (**nomad).sortedlink;
            continue;
        }

        hdlhashtable hsearch;
        if (!langexternalvaltotable(pathval, &hsearch, pathnode) || hsearch == nil) {
            nomad = (**nomad).sortedlink;
            continue;
        }

        /* Now look up the requested name in this path table */
        tyvaluerecord val;
        hdlhashnode node;
        if (hashtablelookup(hsearch, bsname, &val, &node)) {
            /* Found it - check if it's a navigable table */
            hdlhashtable result;
            if (langexternalvaltotable(val, &result, node) && result != nil) {
                /* Build the full resolved path if requested */
                if (resolved_path != NULL && path_bufsize > 0) {
                    /* Get the path from the address value (e.g., "system.verbs.builtins") */
                    bigstring bspath;
                    if (getaddresspath((**nomad).val, bspath)) {
                        /* Skip leading @ if present */
                        const char *pathstart = (const char *)stringbaseaddress(bspath);
                        size_t pathlen = stringlength(bspath);
                        if (pathlen > 0 && pathstart[0] == '@') {
                            pathstart++;
                            pathlen--;
                        }
                        /* Build "path.name" */
                        size_t namelen = strlen(name);
                        if (pathlen + 1 + namelen < path_bufsize) {
                            memcpy(resolved_path, pathstart, pathlen);
                            resolved_path[pathlen] = '.';
                            memcpy(resolved_path + pathlen + 1, name, namelen);
                            resolved_path[pathlen + 1 + namelen] = '\0';
                        } else {
                            /* Buffer too small - just use the name */
                            strncpy(resolved_path, name, path_bufsize - 1);
                            resolved_path[path_bufsize - 1] = '\0';
                        }
                    } else {
                        /* Couldn't get path - just use the name */
                        strncpy(resolved_path, name, path_bufsize - 1);
                        resolved_path[path_bufsize - 1] = '\0';
                    }
                }
                return result;  /* Return the target table */
            }
        }

        nomad = (**nomad).sortedlink;
    }

    /* Search filewindowtable (guest databases) - matches langsearchpathvisit() */
    if (filewindowtable != nil) {
        hdlhashnode fwnomad;
        for (fwnomad = (**filewindowtable).hfirstsort; fwnomad != nil; fwnomad = (**fwnomad).sortedlink) {
            hdlhashtable hsearch;
            if (langexternalvaltotable((**fwnomad).val, &hsearch, fwnomad)) {
                hdlhashnode hnode;
                if (hashtablelookupnode(hsearch, bsname, &hnode)) {
                    /* Found in guest database - check if it's a navigable table */
                    hdlhashtable result;
                    if (langexternalvaltotable((**hnode).val, &result, hnode) && result != nil) {
                        if (resolved_path != NULL && path_bufsize > 0) {
                            /* For guest databases, use the guest db name + entry name */
                            bigstring bsdbname;
                            gethashkey(fwnomad, bsdbname);
                            char dbname[COMPLETION_MAX_NAME_LEN];
                            size_t dbnamelen = stringlength(bsdbname);
                            if (dbnamelen >= COMPLETION_MAX_NAME_LEN)
                                dbnamelen = COMPLETION_MAX_NAME_LEN - 1;
                            memcpy(dbname, stringbaseaddress(bsdbname), dbnamelen);
                            dbname[dbnamelen] = '\0';

                            size_t namelen = strlen(name);
                            if (dbnamelen + 1 + namelen < path_bufsize) {
                                memcpy(resolved_path, dbname, dbnamelen);
                                resolved_path[dbnamelen] = '.';
                                memcpy(resolved_path + dbnamelen + 1, name, namelen);
                                resolved_path[dbnamelen + 1 + namelen] = '\0';
                            } else {
                                strncpy(resolved_path, name, path_bufsize - 1);
                                resolved_path[path_bufsize - 1] = '\0';
                            }
                        }
                        return result;
                    }
                }
            }
        }
    }

    return nil;  /* Not found in any path table */
}

/* Simple wrapper for completion_search_paths_ex without path output */
hdlhashtable completion_search_paths(const char *name) {
    return completion_search_paths_ex(name, NULL, 0);
}

/* Adds matching entries from all system.paths tables to completion results.
 * This makes path-accessible names (like fileMenu, new, etc.) available
 * for top-level tab completion without requiring the full path.
 */
void completion_add_path_entries(completion_matches_t *matches, const char *prefix) {
    if (pathstable == nil) {
        return;
    }

    size_t prefix_len = strlen(prefix);
    hdlhashnode nomad = (**pathstable).hfirstsort;

    while (nomad != nil && matches->count < COMPLETION_MAX_MATCHES) {
        /* Skip non-address entries */
        if ((**nomad).val.valuetype != addressvaluetype) {
            nomad = (**nomad).sortedlink;
            continue;
        }

        /* Resolve unresolved addresses */
        if ((**nomad).flunresolvedaddress) {
            if (!hashresolvevalue(pathstable, nomad)) {
                nomad = (**nomad).sortedlink;
                continue;
            }
        }

        /* Get the table that this path entry points to */
        bigstring bs_local;
        hdlhashtable hparent;
        if (!getaddressvalue((**nomad).val, &hparent, bs_local)) {
            nomad = (**nomad).sortedlink;
            continue;
        }

        if (hparent == nil) {
            nomad = (**nomad).sortedlink;
            continue;
        }

        /* Resolve bs_local in hparent to get the actual target table */
        tyvaluerecord pathval;
        hdlhashnode pathnode;

        if (!hashtablelookup(hparent, bs_local, &pathval, &pathnode)) {
            nomad = (**nomad).sortedlink;
            continue;
        }

        hdlhashtable hsearch;
        if (!langexternalvaltotable(pathval, &hsearch, pathnode) || hsearch == nil) {
            nomad = (**nomad).sortedlink;
            continue;
        }

        /* Add matching entries from this path table */
        hdlhashnode entry = (**hsearch).hfirstsort;

        while (entry != nil && matches->count < COMPLETION_MAX_MATCHES) {
            bigstring bs;
            gethashkey(entry, bs);

            /* Convert to C string */
            char name[COMPLETION_MAX_NAME_LEN];
            size_t len = stringlength(bs);
            if (len >= COMPLETION_MAX_NAME_LEN) {
                len = COMPLETION_MAX_NAME_LEN - 1;
            }
            memcpy(name, stringbaseaddress(bs), len);
            name[len] = '\0';

            /* Check prefix match (case-insensitive) */
            if (prefix_len == 0 || strncasecmp(name, prefix, prefix_len) == 0) {
                tyvaluetype type = (**entry).val.valuetype;
                bool is_table = (type == externalvaluetype);

                if (is_table) {
                    tyexternalid exttype = langexternalgettype((**entry).val);
                    is_table = (exttype == idtableprocessor);
                }

                completion_matches_add(matches, name, type, is_table);
            }

            entry = (**entry).sortedlink;
        }

        nomad = (**nomad).sortedlink;
    }

    /* Add entries from filewindowtable (guest databases) - matches langsearchpathvisit() */
    if (filewindowtable != nil) {
        hdlhashnode fwnomad;
        for (fwnomad = (**filewindowtable).hfirstsort; fwnomad != nil && matches->count < COMPLETION_MAX_MATCHES; fwnomad = (**fwnomad).sortedlink) {
            hdlhashtable hsearch;
            if (langexternalvaltotable((**fwnomad).val, &hsearch, fwnomad)) {
                completion_add_table_entries(matches, hsearch, prefix);
            }
        }
    }
}

/* ============================================================================
 * Phase 3: Dotted Path Navigation
 * ============================================================================ */

/* Navigates a dotted path (e.g., "system.verbs") and returns the target table.
 * For single-component paths, also searches system.paths as a fallback.
 * This allows /jump fileMenu to navigate to system.verbs.builtins.fileMenu.
 */
hdlhashtable completion_navigate_path(const char *path) {
    if (path == NULL || path[0] == '\0') {
        return roottable;
    }

    /* Make a mutable copy for tokenization */
    char path_copy[COMPLETION_MAX_NAME_LEN];
    strncpy(path_copy, path, COMPLETION_MAX_NAME_LEN - 1);
    path_copy[COMPLETION_MAX_NAME_LEN - 1] = '\0';

    hdlhashtable current = roottable;
    int depth = 0;
    boolean first_component = true;

    char *saveptr = NULL;
    char *component = strtok_r(path_copy, ".", &saveptr);

    while (component != NULL && current != nil && depth < COMPLETION_MAX_PATH_DEPTH) {
        /* Convert to bigstring */
        bigstring bs;
        copyctopstring(component, bs);

        /* Look up in current table */
        tyvaluerecord val;
        hdlhashnode node;
        if (!hashtablelookup(current, bs, &val, &node)) {
            /* Not found in current table.
             * For the first component, try system.paths as fallback.
             * This allows single names like "fileMenu" to resolve via paths.
             */
            if (first_component) {
                hdlhashtable path_result = completion_search_paths(component);
                if (path_result != nil) {
                    current = path_result;
                    component = strtok_r(NULL, ".", &saveptr);
                    depth++;
                    first_component = false;
                    continue;
                }
            }
            return nil;  /* Path component not found */
        }

        /* Check if it's a table */
        if (val.valuetype == externalvaluetype) {
            hdlexternalvariable hv = (hdlexternalvariable)val.data.externalvalue;
            if (hv == nil) {
                return nil;
            }

            /* Get the table from external value */
            if (!langexternalvaltotable(val, &current, node)) {
                return nil;  /* Not a table */
            }
        } else {
            return nil;  /* Not navigable */
        }

        component = strtok_r(NULL, ".", &saveptr);
        depth++;
        first_component = false;
    }

    return current;
}

/* ============================================================================
 * Phase 4: Context-Aware Completion
 * ============================================================================ */

/*
 * Known verb processors for context detection.
 */
static const char *verb_processors[] = {
    "base64", "bit", "clock", "crypt", "date", "db", "dialog",
    "file", "frontier", "html", "kb", "lang", "launch", "mainwindow",
    "math", "menu", "mouse", "op", "pict", "point", "re", "rectangle",
    "rgb", "script", "search", "semaphore", "speaker", "string",
    "sys", "table", "target", "tcp", "thread", "webserver",
    "window", "wp", "xml",
    NULL
};

/* Determines completion context type (address, verb call, string, or general). */
completion_context_type_t completion_detect_context(const char *line, int cursor_pos) {
    if (line == NULL || cursor_pos <= 0) {
        return COMPLETION_CTX_GENERAL;
    }

    /* Check if inside string literal */
    int quote_count = 0;
    for (int i = 0; i < cursor_pos; i++) {
        if (line[i] == '"' && (i == 0 || line[i-1] != '\\')) {
            quote_count++;
        }
    }
    if (quote_count % 2 == 1) {
        return COMPLETION_CTX_STRING;  /* Inside string - no completion */
    }

    /* Find token start */
    int token_start = cursor_pos;
    while (token_start > 0 && is_path_char(line[token_start - 1])) {
        token_start--;
    }

    /* Check for @ prefix */
    if (token_start > 0 && line[token_start - 1] == '@') {
        return COMPLETION_CTX_ADDRESS;
    }

    /* Check if token starts with known verb processor */
    for (int i = 0; verb_processors[i] != NULL; i++) {
        const char *proc = verb_processors[i];
        size_t proc_len = strlen(proc);

        /* Check if line at token_start starts with "processor." */
        if (cursor_pos - token_start > (int)proc_len &&
            strncasecmp(line + token_start, proc, proc_len) == 0 &&
            line[token_start + proc_len] == '.') {
            return COMPLETION_CTX_VERB_CALL;
        }
    }

    return COMPLETION_CTX_GENERAL;
}

/* ============================================================================
 * Display Matches
 * ============================================================================ */

/* Prints completion matches in a multi-column format to the terminal. */
void completion_display_matches(const completion_matches_t *matches) {
    if (matches->count == 0) {
        return;
    }

    printf("\n");

    /* Calculate columns for display */
    int max_len = 0;
    for (size_t i = 0; i < matches->count; i++) {
        int len = (int)strlen(matches->items[i].name);
        if (len > max_len) {
            max_len = len;
        }
    }

    /* Add padding and type indicator space */
    int col_width = max_len + 4;

    /* Get actual terminal width, fall back to 80 columns */
    int term_width = 80;
#ifdef HAVE_IOCTL
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        term_width = ws.ws_col;
    }
#endif

    int cols = term_width / col_width;
    if (cols < 1) cols = 1;

    /* Print matches in columns */
    for (size_t i = 0; i < matches->count; i++) {
        const completion_match_t *m = &matches->items[i];

        /* Type indicator */
        char indicator = ' ';
        if (m->is_table) {
            indicator = '/';  /* Directory-like indicator for tables */
        }

        printf("%-*s%c", max_len, m->name, indicator);

        if ((i + 1) % cols == 0 || i == matches->count - 1) {
            printf("\n");
        } else {
            printf("  ");
        }
    }
}

/* ============================================================================
 * Main Completion Callback
 * ============================================================================ */

/* Initializes the completion engine (placeholder for future state). */
bool completion_init(void) {
    /* Nothing to initialize for now */
    return true;
}

/* Cleans up completion engine resources (placeholder for future state). */
void completion_cleanup(void) {
    /* Nothing to cleanup for now */
}
