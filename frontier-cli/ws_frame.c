/*
 * Frontier CLI - WebSocket Frame Codec
 *
 * ws_frame.c - RFC 6455 WebSocket frame encoding/decoding and HTTP upgrade handshake
 *
 * Implements the minimal subset of RFC 6455 needed for a WebSocket server:
 * - Frame decode with client masking
 * - Frame encode (server, unmasked)
 * - HTTP upgrade handshake with SHA-1 + base64
 *
 * Uses the existing SHA1 implementation from the Frontier runtime (sha1dgst.c).
 * Base64 encoding for the handshake is done inline (just 28 chars of output).
 *
 * Copyright (C) 1992-2004 UserLand Software, Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "ws_frame.h"
#include "../Common/headers/sha.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* RFC 6455 magic GUID for Sec-WebSocket-Accept */
static const char *WS_MAGIC_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

/* ========================================================================
 * Minimal base64 encoder for handshake (20-byte SHA1 → 28-char base64)
 * ======================================================================== */

static const char b64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void base64_encode_raw(const uint8_t *in, size_t in_len,
                              char *out, size_t out_size) {
    size_t i = 0, j = 0;

    while (i < in_len && j + 4 < out_size) {
        uint32_t a = in[i++];
        /* Track how many input bytes contributed to this triple */
        int bytes_in_triple = 1;
        uint32_t b = 0, c = 0;
        if (i < in_len) { b = in[i++]; bytes_in_triple++; }
        if (i < in_len) { c = in[i++]; bytes_in_triple++; }
        uint32_t triple = (a << 16) | (b << 8) | c;

        out[j++] = b64_table[(triple >> 18) & 0x3F];
        out[j++] = b64_table[(triple >> 12) & 0x3F];
        out[j++] = (bytes_in_triple < 2) ? '=' : b64_table[(triple >> 6) & 0x3F];
        out[j++] = (bytes_in_triple < 3) ? '=' : b64_table[triple & 0x3F];
    }
    out[j] = '\0';
}

/* ========================================================================
 * Frame decode
 * ======================================================================== */

ws_frame_status_t ws_frame_decode(uint8_t *buf, size_t len, ws_frame_t *frame) {

    if (len < 2) {
        return WS_FRAME_INCOMPLETE;
    }

    memset(frame, 0, sizeof(*frame));

    frame->fin = (buf[0] & 0x80) != 0;
    frame->opcode = buf[0] & 0x0F;
    frame->masked = (buf[1] & 0x80) != 0;

    uint64_t payload_len = buf[1] & 0x7F;
    size_t header_len = 2;

    if (payload_len == 126) {
        if (len < 4) return WS_FRAME_INCOMPLETE;
        payload_len = ((uint64_t)buf[2] << 8) | buf[3];
        header_len = 4;
    } else if (payload_len == 127) {
        if (len < 10) return WS_FRAME_INCOMPLETE;
        payload_len = 0;
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | buf[2 + i];
        }
        header_len = 10;
    }

    /* Reject frames that exceed the receive buffer (WS_CLIENT_BUF_SIZE = 256KB).
     * Without this check, a frame between 256KB and 16MB would fill the buffer
     * and return INCOMPLETE forever, hanging the connection. */
    if (payload_len > 256 * 1024) {
        return WS_FRAME_ERROR;
    }

    if (frame->masked) {
        if (len < header_len + 4) return WS_FRAME_INCOMPLETE;
        memcpy(frame->mask_key, buf + header_len, 4);
        header_len += 4;
    }

    if (len < header_len + payload_len) {
        return WS_FRAME_INCOMPLETE;
    }

    /* Unmask payload in place */
    frame->payload = buf + header_len;
    frame->payload_len = (size_t)payload_len;
    frame->frame_len = header_len + (size_t)payload_len;

    if (frame->masked) {
        for (size_t i = 0; i < frame->payload_len; i++) {
            frame->payload[i] ^= frame->mask_key[i & 3];
        }
    }

    return WS_FRAME_OK;
}

/* ========================================================================
 * Frame encode
 * ======================================================================== */

uint8_t *ws_frame_encode(uint8_t opcode, const uint8_t *payload, size_t payload_len,
                         size_t *out_len) {

    size_t header_len;

    if (payload_len < 126) {
        header_len = 2;
    } else if (payload_len <= 0xFFFF) {
        header_len = 4;
    } else {
        header_len = 10;
    }

    *out_len = header_len + payload_len;
    uint8_t *buf = malloc(*out_len);
    if (buf == NULL) {
        return NULL;
    }

    buf[0] = 0x80 | (opcode & 0x0F);  /* FIN + opcode */

    if (payload_len < 126) {
        buf[1] = (uint8_t)payload_len;
    } else if (payload_len <= 0xFFFF) {
        buf[1] = 126;
        buf[2] = (uint8_t)(payload_len >> 8);
        buf[3] = (uint8_t)(payload_len & 0xFF);
    } else {
        buf[1] = 127;
        for (int i = 0; i < 8; i++) {
            buf[2 + i] = (uint8_t)(payload_len >> (56 - 8 * i));
        }
    }

    if (payload_len > 0 && payload != NULL) {
        memcpy(buf + header_len, payload, payload_len);
    }

    return buf;
}

/* ========================================================================
 * HTTP Upgrade Handshake
 * ======================================================================== */

/*
 * Case-insensitive header search.
 */
static const char *find_header(const char *headers, const char *name) {
    const char *p = headers;
    size_t name_len = strlen(name);

    while (*p) {
        if (strncasecmp(p, name, name_len) == 0) {
            p += name_len;
            /* Skip whitespace after colon */
            while (*p == ' ' || *p == '\t') p++;
            /* Return pointer to value start */
            return p;
        }
        /* Skip to next line */
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }
    return NULL;
}

int ws_handshake(const uint8_t *buf, size_t len, char *response, size_t response_size,
                 size_t *request_len) {

    /* Find end of HTTP headers (\r\n\r\n) */
    const char *str = (const char *)buf;
    const char *end = NULL;

    for (size_t i = 0; i + 3 < len; i++) {
        if (str[i] == '\r' && str[i+1] == '\n' && str[i+2] == '\r' && str[i+3] == '\n') {
            end = str + i + 4;
            break;
        }
    }

    if (end == NULL) {
        /* Headers not complete yet */
        return 0;
    }

    *request_len = (size_t)(end - str);

    /* Verify it's a GET request */
    if (strncmp(str, "GET ", 4) != 0) {
        return -1;
    }

    /* Find Sec-WebSocket-Key header */
    const char *key_start = find_header(str, "Sec-WebSocket-Key:");
    if (key_start == NULL) {
        return -1;
    }

    /* Extract the key value (up to CRLF) */
    char ws_key[64];
    int ki = 0;
    while (*key_start && *key_start != '\r' && *key_start != '\n' && ki < 63) {
        ws_key[ki++] = *key_start++;
    }
    ws_key[ki] = '\0';

    /* Trim trailing whitespace */
    while (ki > 0 && (ws_key[ki-1] == ' ' || ws_key[ki-1] == '\t')) {
        ws_key[--ki] = '\0';
    }

    /* Compute accept key: SHA1(key + GUID), then base64 */
    char concat[128];
    snprintf(concat, sizeof(concat), "%s%s", ws_key, WS_MAGIC_GUID);

    SHA_CTX sha;
    uint8_t digest[20];
    SHA1_Init(&sha);
    SHA1_Update(&sha, (unsigned char *)concat, (unsigned long)strlen(concat));
    SHA1_Final(digest, &sha);

    char accept_key[64];
    base64_encode_raw(digest, 20, accept_key, sizeof(accept_key));

    /* Build response */
    int n = snprintf(response, response_size,
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n",
        accept_key);

    return n;
}
