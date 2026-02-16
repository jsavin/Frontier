/*
 * Frontier CLI - Shared JSON Output Utilities
 *
 * cli_json_output.h - JSON string escaping and output helpers
 *
 * Copyright (C) 1992-2004 UserLand Software, Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef CLI_JSON_OUTPUT_H
#define CLI_JSON_OUTPUT_H

#include <stdio.h>

/*
 * Write a JSON-escaped string (with surrounding quotes) to the given stream.
 *
 * Handles: \", \\, \b, \f, \n, \r, \t, and \uXXXX for control chars < 32.
 * Passes through printable ASCII and valid UTF-8 multi-byte sequences.
 *
 * If str is NULL, writes the JSON literal "null" (without quotes).
 */
void cli_json_write_escaped_string(FILE *stream, const char *str);

/*
 * Write a JSON-escaped string from a buffer with explicit length.
 * Same escaping rules as cli_json_write_escaped_string but does not
 * rely on NUL termination — uses len bytes starting at buf.
 */
void cli_json_write_escaped_buffer(FILE *stream, const char *buf, long len);

#endif /* CLI_JSON_OUTPUT_H */
