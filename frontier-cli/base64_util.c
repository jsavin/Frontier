/*
 * Frontier CLI - Base64 Utility
 *
 * base64_util.c - Shared base64 encoding
 *
 * Extracted from ws_frame.c so that non-WebSocket code (e.g. ODB binary
 * serialization) can use base64 without pulling in WebSocket headers.
 *
 * Copyright (C) 1992-2004 UserLand Software, Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "base64_util.h"

static const char b64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* Callers must provide an output buffer of at least (in_len + 2) / 3 * 4 + 1
 * bytes. Returns the number of base64 characters written (excluding NUL).
 * If the buffer is too small, output is truncated and the return value
 * will be less than the expected ((in_len+2)/3)*4. */
size_t base64_encode_raw(const uint8_t *in, size_t in_len,
                              char *out, size_t out_size) {
    size_t i = 0, j = 0;

    /* Guard: need room for 4 output bytes (j..j+3) plus the NUL terminator
     * that is written at out[j] after the loop. So j+4 < out_size ensures
     * both the 4-byte write and the subsequent NUL are in bounds. */
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
    return j;
}
