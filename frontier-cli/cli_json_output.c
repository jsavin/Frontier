/*
 * Frontier CLI - Shared JSON Output Utilities
 *
 * cli_json_output.c - JSON string escaping and output helpers
 *
 * Extracted from cli_executor.c to be shared with protocol_handler.c.
 *
 * Copyright (C) 1992-2004 UserLand Software, Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "cli_json_output.h"

/* Internal: escape one character to the stream. */
static void write_escaped_char(FILE *stream, char c) {
    switch (c) {
        case '"':  fputs("\\\"", stream); break;
        case '\\': fputs("\\\\", stream); break;
        case '\b': fputs("\\b", stream);  break;
        case '\f': fputs("\\f", stream);  break;
        case '\n': fputs("\\n", stream);  break;
        case '\r': fputs("\\r", stream);  break;
        case '\t': fputs("\\t", stream);  break;
        default:
            if ((unsigned char)c < 32) {
                fprintf(stream, "\\u%04x", (unsigned char)c);
            } else {
                fputc(c, stream);
            }
            break;
    }
}

void cli_json_write_escaped_string(FILE *stream, const char *str) {
    if (str == NULL) {
        fputs("null", stream);
        return;
    }

    fputc('"', stream);
    for (const char *p = str; *p != '\0'; p++) {
        write_escaped_char(stream, *p);
    }
    fputc('"', stream);
}

void cli_json_write_escaped_buffer(FILE *stream, const char *buf, long len) {
    if (buf == NULL) {
        fputs("null", stream);
        return;
    }

    fputc('"', stream);
    for (long i = 0; i < len; i++) {
        write_escaped_char(stream, buf[i]);
    }
    fputc('"', stream);
}
