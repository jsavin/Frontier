/*
    Frontier CLI - Operation Handler

    op_handler.h - Transport-agnostic JSON operation dispatch

    Provides the transport_t abstraction and op_dispatch() function shared by
    both the stdio NDJSON protocol handler and the WebSocket server.

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

#ifndef OP_HANDLER_H
#define OP_HANDLER_H

#include <stddef.h>

/*
 * Transport abstraction — callers provide a write callback so operation
 * handlers can respond without knowing whether the transport is stdio or
 * a WebSocket connection.
 */
typedef struct {
	void *ctx;
	void (*write_line)(void *ctx, const char *json, size_t len);
} transport_t;

/*
 * Dispatch a single JSON operation message.
 *
 * Parses the "op" and "id" fields, delegates to the appropriate handler,
 * and writes the response via transport->write_line().
 *
 * Returns 0 normally, 1 if the client requested shutdown.
 */
int op_dispatch(const char *json_line, size_t len, transport_t *transport);

#endif /* OP_HANDLER_H */
