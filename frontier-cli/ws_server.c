/*
 * Frontier CLI - WebSocket Server
 *
 * ws_server.c - Minimal WebSocket server for ODB access
 *
 * Single-threaded poll()-based server. Integrates with the REPL event loop
 * or can be used standalone with its own poll loop.
 *
 * Copyright (C) 1992-2004 UserLand Software, Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "ws_server.h"
#include "ws_frame.h"
#include "op_handler.h"

#include "../Common/headers/logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Global server pointer — set by main.c, used by repl.c event loop */
ws_server_t *g_ws_server = NULL;

/* ========================================================================
 * WebSocket transport — write_line callback for transport_t
 *
 * Sends a JSON response as a WebSocket text frame.
 * ======================================================================== */

typedef struct {
    int fd;
} ws_transport_ctx_t;

static void ws_write_line(void *ctx, const char *json, size_t len) {
    ws_transport_ctx_t *wctx = (ws_transport_ctx_t *)ctx;

    size_t frame_len;
    uint8_t *frame = ws_frame_encode(WS_OPCODE_TEXT, (const uint8_t *)json, len, &frame_len);
    if (frame == NULL) {
        return;
    }

    /* Best-effort write — non-blocking socket may not accept all bytes.
     * For this initial implementation we don't buffer partial writes.
     * A production server would need a send queue. */
    ssize_t n = write(wctx->fd, frame, frame_len);
    if (n < 0) {
        log_debug(LOG_COMP_GENERAL, "ws: write failed: %s", strerror(errno));
    }

    free(frame);
}

/* ========================================================================
 * Connection management
 * ======================================================================== */

static void close_client(ws_conn_t *conn) {
    if (conn->fd >= 0) {
        close(conn->fd);
        conn->fd = -1;
    }
    if (conn->recv_buf != NULL) {
        free(conn->recv_buf);
        conn->recv_buf = NULL;
    }
    conn->recv_len = 0;
    conn->state = WS_STATE_EMPTY;
}

static ws_conn_t *find_free_slot(ws_server_t *server) {
    for (int i = 0; i < WS_MAX_CLIENTS; i++) {
        if (server->clients[i].state == WS_STATE_EMPTY) {
            return &server->clients[i];
        }
    }
    return NULL;
}

static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        log_error(LOG_COMP_GENERAL, "ws: set_nonblocking failed (fd=%d): %s",
                  fd, strerror(errno));
    }
}

/* ========================================================================
 * Handle incoming data on a connection
 * ======================================================================== */

static void handle_handshake(ws_conn_t *conn) {
    char response[512];
    size_t request_len;

    int n = ws_handshake(conn->recv_buf, conn->recv_len,
                         response, sizeof(response), &request_len);

    if (n == 0) {
        /* Incomplete headers, wait for more data */
        return;
    }

    if (n < 0) {
        log_debug(LOG_COMP_GENERAL, "ws: handshake failed, closing connection");
        close_client(conn);
        return;
    }

    /* Send the upgrade response */
    ssize_t sent = write(conn->fd, response, (size_t)n);
    if (sent < 0) {
        log_debug(LOG_COMP_GENERAL, "ws: failed to send handshake response");
        close_client(conn);
        return;
    }

    /* Remove consumed request bytes */
    if (request_len < conn->recv_len) {
        memmove(conn->recv_buf, conn->recv_buf + request_len,
                conn->recv_len - request_len);
    }
    conn->recv_len -= request_len;

    conn->state = WS_STATE_OPEN;
    log_info(LOG_COMP_GENERAL, "ws: client connected (fd=%d)", conn->fd);
}

static void handle_frame(ws_conn_t *conn) {

    while (conn->recv_len > 0) {
        ws_frame_t frame;
        ws_frame_status_t status = ws_frame_decode(conn->recv_buf, conn->recv_len, &frame);

        if (status == WS_FRAME_INCOMPLETE) {
            break;
        }

        if (status == WS_FRAME_ERROR) {
            log_debug(LOG_COMP_GENERAL, "ws: frame error, closing connection (fd=%d)", conn->fd);
            close_client(conn);
            return;
        }

        switch (frame.opcode) {
            case WS_OPCODE_TEXT: {
                /* Null-terminate the payload for JSON parsing */
                char *json = malloc(frame.payload_len + 1);
                if (json == NULL) break;
                memcpy(json, frame.payload, frame.payload_len);
                json[frame.payload_len] = '\0';

                /* Dispatch via shared operation handler */
                ws_transport_ctx_t wctx = { .fd = conn->fd };
                transport_t transport = {
                    .ctx = &wctx,
                    .write_line = ws_write_line,
                };

                /* GIL invariant: op_dispatch() accesses Frontier runtime globals (hash tables,
                 * lang APIs, etc.) which require the GIL to be held (see ADR-014). This is
                 * satisfied because ws_server_handle_events() is only called from the REPL
                 * event loop and protocol_main() poll loop, both of which run on the main
                 * thread with the GIL held. If this code is ever called from a non-GIL
                 * context, runtime state corruption will occur. */
                int shutdown = op_dispatch(json, frame.payload_len, &transport);
                free(json);

                if (shutdown) {
                    /* Send close frame */
                    size_t close_len;
                    uint8_t *close_frame = ws_frame_encode(WS_OPCODE_CLOSE, NULL, 0, &close_len);
                    if (close_frame != NULL) {
                        write(conn->fd, close_frame, close_len);
                        free(close_frame);
                    }
                    close_client(conn);
                    return;
                }
                break;
            }

            case WS_OPCODE_PING: {
                /* Respond with pong */
                size_t pong_len;
                uint8_t *pong = ws_frame_encode(WS_OPCODE_PONG,
                                                 frame.payload, frame.payload_len, &pong_len);
                if (pong != NULL) {
                    write(conn->fd, pong, pong_len);
                    free(pong);
                }
                break;
            }

            case WS_OPCODE_CLOSE: {
                /* Send close back and disconnect */
                size_t close_len;
                uint8_t *close_frame = ws_frame_encode(WS_OPCODE_CLOSE,
                                                        frame.payload, frame.payload_len, &close_len);
                if (close_frame != NULL) {
                    write(conn->fd, close_frame, close_len);
                    free(close_frame);
                }
                log_info(LOG_COMP_GENERAL, "ws: client disconnected (fd=%d)", conn->fd);
                close_client(conn);
                return;
            }

            case WS_OPCODE_PONG:
                /* Ignore unsolicited pongs */
                break;

            default:
                break;
        }

        /* Remove consumed bytes */
        if (frame.frame_len < conn->recv_len) {
            memmove(conn->recv_buf, conn->recv_buf + frame.frame_len,
                    conn->recv_len - frame.frame_len);
        }
        conn->recv_len -= frame.frame_len;
    }
}

/* ========================================================================
 * Public API
 * ======================================================================== */

int ws_server_init(ws_server_t *server, int port) {

    memset(server, 0, sizeof(*server));
    server->listen_fd = -1;
    server->port = port;

    /* Initialize client slots */
    for (int i = 0; i < WS_MAX_CLIENTS; i++) {
        server->clients[i].fd = -1;
        server->clients[i].state = WS_STATE_EMPTY;
    }

    /* Create listen socket */
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        log_error(LOG_COMP_GENERAL, "ws: socket() failed: %s", strerror(errno));
        return -1;
    }

    /* Allow port reuse */
    int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  /* localhost only */
    addr.sin_port = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        log_error(LOG_COMP_GENERAL, "ws: bind() failed on port %d: %s", port, strerror(errno));
        close(fd);
        return -1;
    }

    if (listen(fd, 4) < 0) {
        log_error(LOG_COMP_GENERAL, "ws: listen() failed: %s", strerror(errno));
        close(fd);
        return -1;
    }

    set_nonblocking(fd);
    server->listen_fd = fd;

    log_info(LOG_COMP_GENERAL, "WebSocket server listening on ws://127.0.0.1:%d/", port);
    return 0;
}

int ws_server_pollfds(ws_server_t *server, struct pollfd *fds, int start_index) {

    int idx = start_index;

    /* Listen socket */
    if (server->listen_fd >= 0) {
        fds[idx].fd = server->listen_fd;
        fds[idx].events = POLLIN;
        fds[idx].revents = 0;
        idx++;
    }

    /* Client sockets */
    for (int i = 0; i < WS_MAX_CLIENTS; i++) {
        if (server->clients[i].state != WS_STATE_EMPTY) {
            fds[idx].fd = server->clients[i].fd;
            fds[idx].events = POLLIN;
            fds[idx].revents = 0;
            idx++;
        }
    }

    return idx - start_index;
}

void ws_server_handle_events(ws_server_t *server, struct pollfd *fds, int start_index) {

    int idx = start_index;

    /* Check listen socket for new connections */
    if (server->listen_fd >= 0 && (fds[idx].revents & POLLIN)) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(server->listen_fd, (struct sockaddr *)&client_addr, &addr_len);

        if (client_fd >= 0) {
            ws_conn_t *slot = find_free_slot(server);
            if (slot == NULL) {
                log_debug(LOG_COMP_GENERAL, "ws: max clients reached, rejecting connection");
                close(client_fd);
            } else {
                set_nonblocking(client_fd);
                slot->fd = client_fd;
                slot->state = WS_STATE_HANDSHAKE;
                slot->recv_buf = malloc(WS_CLIENT_BUF_SIZE);
                slot->recv_len = 0;
                if (slot->recv_buf == NULL) {
                    close(client_fd);
                    slot->fd = -1;
                    slot->state = WS_STATE_EMPTY;
                } else {
                    log_debug(LOG_COMP_GENERAL, "ws: new connection (fd=%d)", client_fd);
                }
            }
        }
    }
    if (server->listen_fd >= 0) {
        idx++;
    }

    /* pollfd layout is deterministic: fds[start_index] = listen socket,
     * fds[start_index + 1 + i] = client slot i. With WS_MAX_CLIENTS = 8
     * the linear scan is harmless, but the mapping is direct if needed. */

    /* Check client sockets */
    for (int i = 0; i < WS_MAX_CLIENTS; i++) {
        if (server->clients[i].state == WS_STATE_EMPTY) {
            continue;
        }

        ws_conn_t *conn = &server->clients[i];

        /* Find this fd in the pollfd array */
        int pfd_idx = -1;
        for (int j = idx; j < idx + WS_MAX_CLIENTS; j++) {
            if (fds[j].fd == conn->fd) {
                pfd_idx = j;
                break;
            }
        }

        if (pfd_idx < 0) {
            continue;
        }

        if (!(fds[pfd_idx].revents & POLLIN)) {
            continue;
        }

        /* Read available data */
        size_t space = WS_CLIENT_BUF_SIZE - conn->recv_len;
        if (space == 0) {
            log_debug(LOG_COMP_GENERAL, "ws: client buffer full (fd=%d)", conn->fd);
            close_client(conn);
            continue;
        }

        ssize_t n = read(conn->fd, conn->recv_buf + conn->recv_len, space);
        if (n <= 0) {
            if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
                log_debug(LOG_COMP_GENERAL, "ws: client disconnected (fd=%d)", conn->fd);
                close_client(conn);
            }
            continue;
        }

        conn->recv_len += (size_t)n;

        /* Process based on state */
        switch (conn->state) {
            case WS_STATE_HANDSHAKE:
                handle_handshake(conn);
                /* If handshake completed and there's leftover data, process frames */
                if (conn->state == WS_STATE_OPEN && conn->recv_len > 0) {
                    handle_frame(conn);
                }
                break;

            case WS_STATE_OPEN:
                handle_frame(conn);
                break;

            default:
                break;
        }
    }
}

void ws_server_shutdown(ws_server_t *server) {

    for (int i = 0; i < WS_MAX_CLIENTS; i++) {
        if (server->clients[i].state != WS_STATE_EMPTY) {
            /* Try to send close frame */
            if (server->clients[i].state == WS_STATE_OPEN) {
                size_t close_len;
                uint8_t *close_frame = ws_frame_encode(WS_OPCODE_CLOSE, NULL, 0, &close_len);
                if (close_frame != NULL) {
                    write(server->clients[i].fd, close_frame, close_len);
                    free(close_frame);
                }
            }
            close_client(&server->clients[i]);
        }
    }

    if (server->listen_fd >= 0) {
        close(server->listen_fd);
        server->listen_fd = -1;
    }

    log_info(LOG_COMP_GENERAL, "WebSocket server shut down");
}
