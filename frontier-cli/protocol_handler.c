/*
    Frontier CLI - NDJSON Protocol Handler

    protocol_handler.c - Stdio NDJSON transport for the operation handler

    Reads one JSON object per line from stdin, dispatches via op_dispatch(),
    writes one JSON response line to stdout. The actual operation logic lives
    in op_handler.c; this file only handles the stdio framing.

    When a WebSocket server is active (ws_server != NULL), uses poll() to
    multiplex between stdin and WebSocket connections. Otherwise uses blocking
    fgets() for backwards compatibility.

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

#include "protocol_handler.h"
#include "op_handler.h"
#include "ws_server.h"
#include "repl.h"

#include "../Common/headers/logging.h"
#include "headless_threading.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
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

/* Saved original stdout fd for restoration during teardown.
 * Set by setup_protocol_output(), used by teardown_protocol_output(). */
static int g_saved_stdout_fd = -1;

static int setup_protocol_output(void) {
	int saved_stdout_fd = dup(STDOUT_FILENO);
	if (saved_stdout_fd < 0) {
		fprintf(stderr, "protocol: failed to dup stdout\n");
		return -1;
	}
	g_saved_stdout_fd = saved_stdout_fd;
	g_protocol_out = fdopen(saved_stdout_fd, "w");
	if (g_protocol_out == NULL) {
		fprintf(stderr, "protocol: failed to fdopen saved stdout\n");
		close(saved_stdout_fd);
		g_saved_stdout_fd = -1;
		return -1;
	}
	setvbuf(g_protocol_out, NULL, _IOLBF, 0);
	dup2(STDERR_FILENO, STDOUT_FILENO);
	return 0;
}

static void teardown_protocol_output(void) {
	if (g_protocol_out != NULL) {
		fflush(g_protocol_out);
		/* Restore the original stdout fd so downstream code (e.g. atexit
		 * handlers) sees a normal stdout. We must dup the saved fd before
		 * fclose, because fclose closes the underlying fd (g_saved_stdout_fd). */
		if (g_saved_stdout_fd >= 0) {
			dup2(g_saved_stdout_fd, STDOUT_FILENO);
		}
		fclose(g_protocol_out);	 /* closes g_saved_stdout_fd */
		g_protocol_out = NULL;
		g_saved_stdout_fd = -1;
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

int protocol_main(cli_options_t *options, ws_server_t *ws_server) {
	(void)options;

	int stdin_flags = -1;  /* saved stdin fcntl flags; restored before return */

	/* Install the repl.* verb host adapter so verbs like repl.syncScan()
	 * work in --protocol mode, just as they do in --repl mode. */
	repl_install_verb_host();

	char *line_buf = malloc(PROTOCOL_LINE_MAX);
	if (line_buf == NULL) {
		fprintf(stderr, "protocol: failed to allocate line buffer\n");
		repl_uninstall_verb_host();
		return 1;
	}

	if (setup_protocol_output() < 0) {
		free(line_buf);
		repl_uninstall_verb_host();
		return 1;
	}

	transport_t transport = {
		.ctx = NULL,
		.write_line = stdio_write_line,
	};

	log_info(LOG_COMP_GENERAL, "Protocol mode: ready for NDJSON on stdin");

	if (ws_server != NULL) {
		/* poll()-based event loop: multiplex stdin + WebSocket */
		/* Make stdin non-blocking for poll integration */
		stdin_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
		fcntl(STDIN_FILENO, F_SETFL, stdin_flags | O_NONBLOCK);

		size_t line_pos = 0;
		int running = 1;
		int stdin_eof = 0;
		bool draining = false;	/* true while discarding an oversized line */

		while (running) {
			struct pollfd pfds[WS_POLL_FDS_COUNT + 1];
			memset(pfds, 0, sizeof(pfds));
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
			nfds += ws_server_pollfds(ws_server, pfds, ws_start);

			/* Yield GIL during poll wait so debug/spawned threads can run.
			 * Save/restore globals because spawned threads overwrite hthreadglobals. */
			hdlthreadglobals saved_globals = hthreadglobals;
			headless_save_threadglobals(saved_globals);
			pthread_mutex_unlock(&frontier_gil);
			int ready = poll(pfds, (nfds_t)nfds, PROTOCOL_POLL_TIMEOUT_MS);
			pthread_mutex_lock(&frontier_gil);
			headless_restore_threadglobals(saved_globals);

			if (ready < 0) {
				if (errno == EINTR) continue;
				log_error(LOG_COMP_GENERAL, "protocol: poll() failed: %s", strerror(errno));
				break;
			}

			/* Handle WebSocket events (GIL held — required by op_dispatch) */
			ws_server_handle_events(ws_server, pfds, ws_start);

			/* Handle stdin data */
			if (!stdin_eof && (pfds[0].revents & POLLIN)) {
				size_t remaining_space = PROTOCOL_LINE_MAX - 1 - line_pos;
				if (remaining_space == 0) {
					/* Buffer is full without a newline — enter drain mode to
					 * skip the rest of this oversized line until we see a newline. */
					log_error(LOG_COMP_GENERAL,
							  "protocol: line exceeds %d bytes, discarding",
							  PROTOCOL_LINE_MAX);
					line_pos = 0;
					draining = true;
					continue;
				}
				ssize_t n = read(STDIN_FILENO, line_buf + line_pos,
								 remaining_space);
				if (n > 0) {
					/* Drain mode: discard an oversized line until we find its newline.
					 *
					 * Buffer state transitions:
					 *	 1. Enter drain mode: line_pos is reset to 0 (above, when buffer fills
					 *		without a newline).
					 *	 2. Each read() fills line_buf starting at line_pos (which is 0 during
					 *		drain), so the fresh data always lands at the start of the buffer.
					 *	 3. memchr() searches the freshly-read n bytes for a newline.
					 *	 4. No newline found: keep line_pos = 0 and loop, effectively discarding
					 *		the data we just read by overwriting it on the next read().
					 *	 5. Newline found: memmove() shifts post-newline data to the start of
					 *		line_buf, set line_pos to the remaining byte count, clear draining
					 *		flag, and fall through to normal line processing. */
					if (draining) {
						const char *nl = memchr(line_buf + line_pos, '\n', (size_t)n);
						if (nl == NULL) {
							/* Still in oversized line, discard everything */
							continue;
						}
						/* Found newline — keep data after it, resume normal processing */
						size_t skip = (size_t)(nl - (line_buf + line_pos)) + 1;
						size_t remaining = (size_t)n - skip;
						if (remaining > 0) {
							memmove(line_buf, line_buf + line_pos + skip, remaining);
						}
						line_pos = remaining;
						draining = false;
						/* Fall through to process any complete lines in the kept data */
					} else {
						line_pos += (size_t)n;
					}
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
					log_info(LOG_COMP_GENERAL, "protocol: stdin closed, continuing to serve WebSocket clients");
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

			/* Exit when stdin is closed and no WebSocket clients remain */
			if (stdin_eof && ws_server_client_count(ws_server) == 0) {
				log_info(LOG_COMP_GENERAL, "protocol: stdin closed and no WebSocket clients, exiting");
				running = 0;
				break;
			}

			/* If stdin is closed but WS server still has clients, keep running */
			if (stdin_eof && !running) {
				break;
			}
		}

	} else {
		/* Simple blocking fgets loop (no WS server).
		 * Yield GIL during fgets so debug/spawned threads can run.
		 * Save/restore globals because spawned threads overwrite hthreadglobals. */
		for (;;) {
			hdlthreadglobals main_globals = hthreadglobals;
			headless_save_threadglobals(main_globals);
			pthread_mutex_unlock(&frontier_gil);
			char *result = fgets(line_buf, PROTOCOL_LINE_MAX, stdin);
			pthread_mutex_lock(&frontier_gil);
			headless_restore_threadglobals(main_globals);

			if (result == NULL)
				break;

			size_t len = strlen(line_buf);
			if (process_line(line_buf, len, &transport)) {
				break;
			}
		}
	}

	/* Restore stdin to blocking mode on all exit paths. When the WebSocket
	 * server is active we set stdin non-blocking for poll(); restore the
	 * original flags so downstream code (e.g. atexit handlers) isn't
	 * surprised by non-blocking stdin. */
	if (stdin_flags >= 0) {
		fcntl(STDIN_FILENO, F_SETFL, stdin_flags);
	}

	free(line_buf);
	teardown_protocol_output();
	repl_uninstall_verb_host();

	log_info(LOG_COMP_GENERAL, "Protocol mode: shutting down");
	return 0;
}
