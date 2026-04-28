/*
    Frontier CLI - Base64 Utility

    base64_util.c - Shared base64 encoding

    Extracted from ws_frame.c so that non-WebSocket code (e.g. ODB binary
    serialization) can use base64 without pulling in WebSocket headers.

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

#include "base64_util.h"

static const char b64_table[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* Callers must provide an output buffer of at least (in_len + 2) / 3 * 4 + 1
 * bytes. Returns the number of base64 characters written (excluding NUL).
 * If the buffer is too small, output is truncated and the return value
 * will be less than the expected ((in_len+2)/3)*4. */
size_t base64_encode_raw(const uint8_t *in, size_t in_len,
							  char *out, size_t out_size) {
	size_t i = 0, j = 0;

	/* Guard: need room for 4 output bytes (j..j+3) plus the NUL terminator
	 * that is written at out[j] after the loop. So j+4 < out_size ensures
	 * both the 4-byte write and the subsequent NUL are in bounds. */
	while (i < in_len && j + 4 < out_size) {
		uint32_t a = in[i++];
		/* Track how many input bytes contributed to this triple */
		int bytes_in_triple = 1;
		uint32_t b = 0, c = 0;
		if (i < in_len) { b = in[i++]; bytes_in_triple++; }
		if (i < in_len) { c = in[i++]; bytes_in_triple++; }
		uint32_t triple = (a << 16) | (b << 8) | c;

		out[j++] = b64_table[(triple >> 18) & 0x3F];
		out[j++] = b64_table[(triple >> 12) & 0x3F];
		out[j++] = (bytes_in_triple < 2) ? '=' : b64_table[(triple >> 6) & 0x3F];
		out[j++] = (bytes_in_triple < 3) ? '=' : b64_table[triple & 0x3F];
	}
	out[j] = '\0';
	return j;
}
