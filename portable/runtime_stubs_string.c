/*
 * runtime_stubs_string.c - Minimal string helpers for portable Frontier builds
 * 2025-10-27 Codex: Provide lightweight stubs that are safe to link alongside
 * runtime_stubs_system.c, plus a headless-friendly stringtoostype helper.
 */

#include "runtime_stubs.h"
#include "standard_portable.h"

#include <string.h>

void copystring(const char *src, char *dst) {
    if (!src || !dst)
        return;
    strcpy(dst, src);
}

void copyctopstring(const char *src, char *dst) {
    if (!src || !dst)
        return;
    size_t len = strlen(src);
    if (len > lenbigstring)
        len = lenbigstring;
    dst[0] = (unsigned char) len;
    memcpy(dst + 1, src, len);
}

void copyheapstring(const char *src, char *dst) {
    copystring(src, dst);
}

void deletestring(void *str) {
    (void)str;
}

boolean stringtoostype(bigstring bs, OSType *type) {
    if (bs == NULL || type == NULL)
        return false;

    unsigned char len = bs[0];
    uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
        unsigned char ch = (i < len) ? bs[i + 1] : ' ';
        value = (value << 8) | ch;
    }
    *type = (OSType) value;
    return len > 0;
}
