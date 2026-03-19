/*
 * Frontier CLI - Operation Handler
 *
 * op_handler.h - Transport-agnostic JSON operation dispatch
 *
 * Provides the transport_t abstraction and op_dispatch() function shared by
 * both the stdio NDJSON protocol handler and the WebSocket server.
 *
 * Copyright (C) 1992-2004 UserLand Software, Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
