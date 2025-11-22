#include "../Common/headers/frontier.h"
#include "wptext_portable.h"

/* 2025-11-19 Codex: Headless builds no longer bootstrap Paige; these helpers are compatibility stubs. */

static Boolean g_wp_initialized = false;

Boolean wp_portable_init(void) {
    g_wp_initialized = true;
    return true;
}

void wp_portable_shutdown(void) {
    g_wp_initialized = false;
}

int wp_portable_get_last_error_line(void) {
    return 0;
}

int wp_portable_get_last_errno(void) {
    return 0;
}
