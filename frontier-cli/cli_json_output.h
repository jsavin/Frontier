/*
    Frontier CLI - Shared JSON Output Utilities

    cli_json_output.h - JSON string escaping and output helpers

    SPDX-License-Identifier: MIT

    Copyright (c) 1992-2004 UserLand Software, Inc.
    Copyright (c) 2025-present Frontier contributors

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
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
