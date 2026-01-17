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
 * Phase 2: Hash Table Visitor for Database Names
 * ============================================================================ */

/*
 * Context passed to hash table visitor callback.
 */
typedef struct {
    completion_matches_t *matches;
    const char *prefix;
    size_t prefix_len;
} completion_visitor_ctx_t;

/*
 * Hash table visitor callback.
 * Called for each entry in a table.
 */
static boolean completion_visitor_callback(hdlhashnode node, ptrvoid refcon) {
    completion_visitor_ctx_t *ctx = (completion_visitor_ctx_t *)refcon;

    /* Check if we've hit the match limit */
    if (ctx->matches->count >= COMPLETION_MAX_MATCHES) {
        return false;  /* Stop iteration */
    }

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
    if (ctx->prefix_len == 0 || strncasecmp(name, ctx->prefix, ctx->prefix_len) == 0) {
        tyvaluetype type = (**node).val.valuetype;
        bool is_table = (type == externalvaluetype);

        /* For external values, check if it's a table */
        if (is_table) {
            /* Check external type - tablevaluetype means navigable */
            tyexternalid exttype = langexternalgettype((**node).val);
            is_table = (exttype == idtableprocessor);
        }

        completion_matches_add(ctx->matches, name, type, is_table);
    }

    return true;  /* Continue iteration */
}

/* ============================================================================
 * Match Collection Management
 * ============================================================================ */

void completion_matches_init(completion_matches_t *matches) {
    matches->count = 0;
    matches->common_prefix[0] = '\0';
}

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

void completion_add_table_entries(completion_matches_t *matches,
                                  hdlhashtable table,
                                  const char *prefix) {
    if (table == nil) {
        return;
    }

    completion_visitor_ctx_t ctx = {
        .matches = matches,
        .prefix = prefix,
        .prefix_len = strlen(prefix)
    };

    hashtablevisit(table, completion_visitor_callback, &ctx);
}

/* ============================================================================
 * Phase 3: Dotted Path Navigation
 * ============================================================================ */

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

bool completion_init(void) {
    /* Nothing to initialize for now */
    return true;
}

void completion_cleanup(void) {
    /* Nothing to cleanup for now */
}
