/*
    Frontier CLI - WebSocket Server

    ws_server.h - Minimal WebSocket server for ODB access

    Single-threaded server designed to integrate with the REPL's poll() event
    loop. Call ws_server_init() to bind a listen socket, then add the returned
    fds to your poll array and call ws_server_handle_events() when they're ready.

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

#ifndef WS_SERVER_H
#define WS_SERVER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <poll.h>
#include <time.h>

/* Maximum concurrent WebSocket clients */
#define WS_MAX_CLIENTS 32

/* Total pollfd slots needed: 1 listen socket + WS_MAX_CLIENTS client slots */
#define WS_POLL_FDS_COUNT (1 + WS_MAX_CLIENTS)

/* Maximum receive buffer per client */
#define WS_CLIENT_BUF_SIZE (256 * 1024)

/* Connection states */
typedef enum {
	WS_STATE_EMPTY = 0,		/* Slot is free */
	WS_STATE_HANDSHAKE,		/* Waiting for HTTP upgrade */
	WS_STATE_OPEN,			/* WebSocket connection established */
} ws_conn_state_t;

/* Maximum seconds a connection may remain in HANDSHAKE state before
 * being closed. Prevents slowloris-style resource exhaustion. */
#define WS_HANDSHAKE_TIMEOUT_SECS 5

/* Per-connection state */
typedef struct {
	int fd;
	ws_conn_state_t state;
	uint8_t *recv_buf;
	size_t recv_len;
	time_t handshake_start;	 /* time(NULL) when connection entered HANDSHAKE state */
} ws_conn_t;

/* Server state */
typedef struct {
	int listen_fd;
	ws_conn_t clients[WS_MAX_CLIENTS];
	int port;
} ws_server_t;

/*
 * Initialize the WebSocket server — creates and binds the listen socket.
 * Returns 0 on success, -1 on error.
 */
int ws_server_init(ws_server_t *server, int port);

/*
 * Populate pollfd entries for the server's sockets.
 * fds must have space for at least WS_MAX_CLIENTS + 1 entries.
 * Returns the number of entries filled.
 */
int ws_server_pollfds(ws_server_t *server, struct pollfd *fds, int start_index);

/*
 * Process events after poll() returns. Accepts new connections, reads
 * frames, dispatches operations via op_dispatch(), and sends responses.
 *
 * fds/nfds should include the entries populated by ws_server_pollfds().
 * start_index is the offset into fds[] where the server entries begin.
 */
void ws_server_handle_events(ws_server_t *server, struct pollfd *fds, int start_index);

/*
 * Return the number of currently connected clients (handshaking or open).
 */
int ws_server_client_count(ws_server_t *server);

/*
 * Shut down the server — close all connections and the listen socket.
 */
void ws_server_shutdown(ws_server_t *server);

#endif /* WS_SERVER_H */
