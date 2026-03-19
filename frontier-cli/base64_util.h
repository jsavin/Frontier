/*
 * Frontier CLI - Base64 Utility
 *
 * base64_util.h - Shared base64 encoding for WebSocket handshake and ODB binary values
 *
 * Copyright (C) 1992-2004 UserLand Software, Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef BASE64_UTIL_H
#define BASE64_UTIL_H

#include <stddef.h>
#include <stdint.h>

/*
 * Base64-encode raw bytes into a caller-supplied output buffer.
 * Output is NUL-terminated. out_size must be at least ((in_len+2)/3)*4 + 1.
 * Returns the number of base64 characters written (excluding NUL).
 * If the buffer is too small, output is truncated and the return value
 * will be less than the expected ((in_len+2)/3)*4.
 */
size_t base64_encode_raw(const uint8_t *in, size_t in_len, char *out, size_t out_size);

#endif /* BASE64_UTIL_H */
