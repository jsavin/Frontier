/*
    Frontier CLI - Operation Handler

    op_handler.h - Transport-agnostic JSON operation dispatch

    Provides the transport_t abstraction and op_dispatch() function shared by
    both the stdio NDJSON protocol handler and the WebSocket server.

    SPDX-License-Identifier: MIT

    Copyright (c) 1992-2004 UserLand Software, Inc.
    Copyright (c) 2025-2026 Frontier contributors

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
 * Stable machine-readable error codes for protocol error responses.
 * Every top-level error object carries error.code from this set so
 * clients can branch without string-matching messages. Documented in
 * planning/gui/STDIO_PROTOCOL.md (keep both in sync).
 */
#define OP_ERRCODE_PARSE          "parse_error"      /* request line is not valid JSON */
#define OP_ERRCODE_MISSING_ID     "missing_id"       /* no numeric id field */
#define OP_ERRCODE_MISSING_OP     "missing_op"       /* no string op field */
#define OP_ERRCODE_UNKNOWN_OP     "unknown_op"       /* op not in dispatch table */
#define OP_ERRCODE_BAD_PARAMS     "bad_params"       /* missing or wrong-typed parameter */
#define OP_ERRCODE_BATCH_TOO_LARGE "batch_too_large" /* params.items exceeds batch limit */
#define OP_ERRCODE_LIMIT_EXCEEDED "limit_exceeded"   /* breakpoint/watchpoint slots full */
#define OP_ERRCODE_NOT_FOUND      "not_found"        /* thread/table/script not found */
#define OP_ERRCODE_BAD_STATE      "bad_state"        /* op invalid for target's current state */
#define OP_ERRCODE_SCRIPT_ERROR   "script_error"     /* compile or runtime script failure */
#define OP_ERRCODE_INTERNAL       "internal_error"   /* allocation/spawn/load failure */
#define OP_ERRCODE_LINE_TOO_LONG  "line_too_long"    /* request line exceeds PROTOCOL_LINE_MAX */

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
