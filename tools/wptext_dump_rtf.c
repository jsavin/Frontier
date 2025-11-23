#include "../portable/paige_text_extractor.h"
#include "../Common/headers/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

static uint8_t *read_file(const char *path, long *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);
    uint8_t *buf = (uint8_t *)malloc(len);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    if (len > 0 && fread(buf, 1, len, f) != (size_t)len) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *out_len = len;
    return buf;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <paige_blob.bin> <out.rtf>\n", argv[0]);
        return 1;
    }

    long blob_len = 0;
    uint8_t *blob = read_file(argv[1], &blob_len);
    if (!blob) {
        fprintf(stderr, "failed to read %s\n", argv[1]);
        return 2;
    }

    Handle hrtf = nil;
    long char_count = 0;
    paige_extract_stats stats = {0};
    char errbuf[256] = {0};
    if (!wptext_emit_rtf_from_paige_blob(blob, blob_len, &hrtf, &char_count, &stats, errbuf, sizeof(errbuf))) {
        fprintf(stderr, "emit failed: %s\n", errbuf[0] ? errbuf : "unknown error");
        free(blob);
        return 3;
    }

    FILE *out = fopen(argv[2], "wb");
    if (!out) {
        fprintf(stderr, "failed to open %s\n", argv[2]);
        disposehandle(hrtf);
        free(blob);
        return 4;
    }

    long len = gethandlesize(hrtf);
    if (len > 0)
        fwrite(*hrtf, 1, (size_t)len, out);
    fclose(out);
    disposehandle(hrtf);
    free(blob);
    return 0;
}
