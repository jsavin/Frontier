/* platform_null.c - Null adapter for tests */

#include "platform_adapter.h"
#include "logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

bool fp_read_all(const char* path, void** out_data, size_t* out_size) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    void* buf = malloc((size_t)sz);
    if (!buf) { fclose(f); return false; }
    if (sz > 0 && fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); fclose(f); return false; }
    fclose(f);
    *out_data = buf;
    *out_size = (size_t)sz;
    return true;
}

bool fp_write_all(const char* path, const void* data, size_t size) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    bool ok = (size == 0) || (fwrite(data, 1, size, f) == size);
    fclose(f);
    return ok;
}

bool fp_exists(const char* path) {
    struct stat st; return stat(path, &st) == 0;
}

frontier_timeval fp_now(void) {
    struct timespec ts; frontier_timeval tv = {0};
    if (timespec_get(&ts, TIME_UTC) == TIME_UTC) {
        tv.millis = (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    }
    return tv;
}

void fp_log(const char* message) { if (message) log_debug(LOG_COMP_GENERAL, "%s", message); }


