/*
 * Frontier CLI - NDJSON Protocol Handler
 *
 * protocol_handler.c - Stdio NDJSON transport for the operation handler
 *
 * Reads one JSON object per line from stdin, dispatches via op_dispatch(),
 * writes one JSON response line to stdout. The actual operation logic lives
 * in op_handler.c; this file only handles the stdio framing.
 *
 * When a WebSocket server is active (g_ws_server != NULL), uses poll() to
 * multiplex between stdin and WebSocket connections. Otherwise uses blocking
 * fgets() for backwards compatibility.
 *
 * Copyright (C) 1992-2004 UserLand Software, Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "protocol_handler.h"
#include "op_handler.h"
#include "ws_server.h"

#include "../Common/headers/logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <fcntl.h>

#define PROTOCOL_LINE_MAX 65536
#define PROTOCOL_POLL_TIMEOUT_MS 10

/* Protocol output stream. In protocol mode, we redirect the C library's stdout
 * to stderr (so stray printf output from verb implementations appears in
 * diagnostic output) and write protocol messages to this saved copy of the
 * original stdout fd. */
static FILE *g_protocol_out = NULL;

/* ========================================================================
 * Stdio transport — write_line callback for transport_t
 * ======================================================================== */

static void stdio_write_line(void *ctx, const char *json, size_t len) {
    (void)ctx;

    if (g_protocol_out == NULL) {
        return;
    }

    fwrite(json, 1, len, g_protocol_out);
    fputc('\n', g_protocol_out);
    fflush(g_protocol_out);
}

/* ========================================================================
 * Setup and teardown of the protocol output channel
 * ======================================================================== */

static int setup_protocol_output(void) {
    int saved_stdout_fd = dup(STDOUT_FILENO);
    if (saved_stdout_fd < 0) {
        fprintf(stderr, "protocol: failed to dup stdout\n");
        return -1;
    }
    g_protocol_out = fdopen(saved_stdout_fd, "w");
    if (g_protocol_out == NULL) {
        fprintf(stderr, "protocol: failed to fdopen saved stdout\n");
        close(saved_stdout_fd);
        return -1;
    }
    setvbuf(g_protocol_out, NULL, _IOLBF, 0);
    dup2(STDERR_FILENO, STDOUT_FILENO);
    return 0;
}

static void teardown_protocol_output(void) {
    if (g_protocol_out != NULL) {
        fclose(g_protocol_out);
        g_protocol_out = NULL;
    }
}

/* ========================================================================
 * Process a complete line from the line buffer
 * ======================================================================== */

static int process_line(char *line, size_t len, transport_t *transport) {
    /* Strip trailing newline */
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[--len] = '\0';
    }

    if (len == 0) {
        return 0;
    }

    return op_dispatch(line, len, transport);
}

/* ========================================================================
 * Main protocol loop
 * ======================================================================== */

int protocol_main(cli_options_t *options) {
    (void)options;

    char *line_buf = malloc(PROTOCOL_LINE_MAX);
    if (line_buf == NULL) {
        fprintf(stderr, "protocol: failed to allocate line buffer\n");
        return 1;
    }

    if (setup_protocol_output() < 0) {
        free(line_buf);
        return 1;
    }

    transport_t transport = {
        .ctx = NULL,
        .write_line = stdio_write_line,
    };

    log_info(LOG_COMP_GENERAL, "Protocol mode: ready for NDJSON on stdin");

    if (g_ws_server != NULL) {
        /* poll()-based event loop: multiplex stdin + WebSocket */
        /* Make stdin non-blocking for poll integration */
        int stdin_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, stdin_flags | O_NONBLOCK);

        size_t line_pos = 0;
        int running = 1;
        int stdin_eof = 0;

        while (running) {
            struct pollfd pfds[1 + WS_MAX_CLIENTS + 1];
            int nfds = 0;

            if (!stdin_eof) {
                pfds[nfds].fd = STDIN_FILENO;
                pfds[nfds].events = POLLIN;
                pfds[nfds].revents = 0;
            } else {
                pfds[nfds].fd = -1;
                pfds[nfds].events = 0;
                pfds[nfds].revents = 0;
            }
            nfds = 1;

            int ws_start = nfds;
            nfds += ws_server_pollfds(g_ws_server, pfds, ws_start);

            int ready = poll(pfds, (nfds_t)nfds, PROTOCOL_POLL_TIMEOUT_MS);

            if (ready < 0) {
                if (errno == EINTR) continue;
                log_error(LOG_COMP_GENERAL, "protocol: poll() failed: %s", strerror(errno));
                break;
            }

            /* Handle WebSocket events */
            ws_server_handle_events(g_ws_server, pfds, ws_start);

            /* Handle stdin data */
            if (!stdin_eof && (pfds[0].revents & POLLIN)) {
                ssize_t n = read(STDIN_FILENO, line_buf + line_pos,
                                 PROTOCOL_LINE_MAX - 1 - line_pos);
                if (n > 0) {
                    line_pos += (size_t)n;
                    line_buf[line_pos] = '\0';

                    /* Process complete lines */
                    char *start = line_buf;
                    char *nl;
                    while ((nl = strchr(start, '\n')) != NULL) {
                        *nl = '\0';
                        size_t llen = (size_t)(nl - start);
                        if (process_line(start, llen, &transport)) {
                            running = 0;
                            break;
                        }
                        start = nl + 1;
                    }

                    /* Shift remaining data to start of buffer */
                    if (start > line_buf && running) {
                        size_t remaining = line_pos - (size_t)(start - line_buf);
                        memmove(line_buf, start, remaining);
                        line_pos = remaining;
                    } else if (!running) {
                        break;
                    }
                } else if (n == 0) {
                    stdin_eof = 1;
                    /* Process any remaining data in buffer */
                    if (line_pos > 0) {
                        line_buf[line_pos] = '\0';
                        process_line(line_buf, line_pos, &transport);
                        line_pos = 0;
                    }
                }
            } else if (!stdin_eof && (pfds[0].revents & (POLLHUP | POLLERR))) {
                stdin_eof = 1;
            }

            /* If stdin is closed but WS server still has clients, keep running */
            if (stdin_eof && !running) {
                break;
            }
        }

        /* Restore stdin blocking mode */
        fcntl(STDIN_FILENO, F_SETFL, stdin_flags);
    } else {
        /* Simple blocking fgets loop (no WS server) */
        while (fgets(line_buf, PROTOCOL_LINE_MAX, stdin) != NULL) {
            size_t len = strlen(line_buf);
            if (process_line(line_buf, len, &transport)) {
                break;
            }
        }
    }

    free(line_buf);
    teardown_protocol_output();

    log_info(LOG_COMP_GENERAL, "Protocol mode: shutting down");
    return 0;
}
