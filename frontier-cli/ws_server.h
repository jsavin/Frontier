/*
 * Frontier CLI - WebSocket Server
 *
 * ws_server.h - Minimal WebSocket server for ODB access
 *
 * Single-threaded server designed to integrate with the REPL's poll() event
 * loop. Call ws_server_init() to bind a listen socket, then add the returned
 * fds to your poll array and call ws_server_handle_events() when they're ready.
 *
 * Copyright (C) 1992-2004 UserLand Software, Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef WS_SERVER_H
#define WS_SERVER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <poll.h>

/* Maximum concurrent WebSocket clients */
#define WS_MAX_CLIENTS 8

/* Maximum receive buffer per client */
#define WS_CLIENT_BUF_SIZE (256 * 1024)

/* Connection states */
typedef enum {
    WS_STATE_EMPTY = 0,     /* Slot is free */
    WS_STATE_HANDSHAKE,     /* Waiting for HTTP upgrade */
    WS_STATE_OPEN,          /* WebSocket connection established */
} ws_conn_state_t;

/* Per-connection state */
typedef struct {
    int fd;
    ws_conn_state_t state;
    uint8_t *recv_buf;
    size_t recv_len;
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
 * Shut down the server — close all connections and the listen socket.
 */
void ws_server_shutdown(ws_server_t *server);

#endif /* WS_SERVER_H */
