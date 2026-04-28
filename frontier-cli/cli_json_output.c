/*
    Frontier CLI - Shared JSON Output Utilities

    cli_json_output.c - JSON string escaping and output helpers

    Extracted from cli_executor.c to be shared with protocol_handler.c.

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
