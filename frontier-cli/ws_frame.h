/*
 * Frontier CLI - WebSocket Frame Codec
 *
 * ws_frame.h - RFC 6455 WebSocket frame encoding/decoding and HTTP upgrade handshake
 *
 * Copyright (C) 1992-2004 UserLand Software, Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef WS_FRAME_H
#define WS_FRAME_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Maximum allowed frame payload size — matches the receive buffer size
 * in ws_server.h (WS_CLIENT_BUF_SIZE). Frames larger than this are
 * rejected to prevent buffer-fill hangs. */
#define WS_MAX_FRAME_PAYLOAD (256 * 1024)

/* WebSocket opcodes (RFC 6455 section 5.2)
 * Note: Continuation frames (FIN=0) are not supported. All messages must
 * fit in a single frame. This is intentional — our JSON protocol messages
 * are well within the 256KB frame limit and no client sends fragmented. */
#define WS_OPCODE_CONTINUATION 0x0
#define WS_OPCODE_TEXT         0x1
#define WS_OPCODE_BINARY       0x2
#define WS_OPCODE_CLOSE        0x8
#define WS_OPCODE_PING         0x9
#define WS_OPCODE_PONG         0xA

/* Frame parsing result */
typedef enum {
    WS_FRAME_OK = 0,       /* Complete frame decoded */
    WS_FRAME_INCOMPLETE,   /* Need more data */
    WS_FRAME_ERROR,        /* Protocol error */
} ws_frame_status_t;

/* Decoded frame */
typedef struct {
    uint8_t  opcode;
    bool     fin;
    bool     masked;
    uint8_t  mask_key[4];
    uint8_t *payload;       /* Points into caller's buffer (after unmasking) */
    size_t   payload_len;
    size_t   frame_len;     /* Total bytes consumed from input */
} ws_frame_t;

/*
 * Parse a WebSocket frame from a buffer.
 * On WS_FRAME_OK, frame is populated and frame.frame_len bytes have been consumed.
 * payload is unmasked in-place in buf.
 */
ws_frame_status_t ws_frame_decode(uint8_t *buf, size_t len, ws_frame_t *frame);

/*
 * Encode a WebSocket frame into a newly-allocated buffer.
 * Server frames are NOT masked per RFC 6455.
 * Returns the buffer (caller must free) and sets *out_len.
 * Returns NULL on allocation failure.
 */
uint8_t *ws_frame_encode(uint8_t opcode, const uint8_t *payload, size_t payload_len,
                         size_t *out_len);

/*
 * Perform the HTTP upgrade handshake.
 * Reads the HTTP request from `buf` (len bytes), validates it,
 * and writes the HTTP 101 response into `response` (max response_size bytes).
 * Returns the number of bytes written to response, or -1 on error.
 * Sets *request_len to the number of bytes consumed from buf (the full HTTP request).
 */
int ws_handshake(uint8_t *buf, size_t len, char *response, size_t response_size,
                 size_t *request_len);

#endif /* WS_FRAME_H */
