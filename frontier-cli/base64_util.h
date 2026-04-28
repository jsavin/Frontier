/*
    Frontier CLI - Base64 Utility

    base64_util.h - Shared base64 encoding for WebSocket handshake and ODB binary values

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

#ifndef BASE64_UTIL_H
#define BASE64_UTIL_H

#include <stddef.h>
#include <stdint.h>

/*
 * Base64-encode raw bytes into a caller-supplied output buffer.
 * Output is NUL-terminated. out_size must be at least ((in_len+2)/3)*4 + 1.
 * Returns the number of base64 characters written (excluding NUL).
 * If the buffer is too small, output is truncated and the return value
 * will be less than the expected ((in_len+2)/3)*4.
 */
size_t base64_encode_raw(const uint8_t *in, size_t in_len, char *out, size_t out_size);

#endif /* BASE64_UTIL_H */
