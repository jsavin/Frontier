#include "frontier.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "strings.h"
#include "langwarnings.h"
#include "langinternal.h"

static void format_timestamp(char *out, size_t outlen) {
    time_t now = time(NULL);
    struct tm tmnow;
#if defined(_WIN32)
    localtime_s(&tmnow, &now);
#else
    localtime_r(&now, &tmnow);
#endif
    strftime(out, outlen, "%Y-%m-%d %H:%M:%S", &tmnow);
}

#if !defined(FRONTIER_HEADLESS)
static void log_to_file(const char *line) {
    FILE *fp = fopen("FrontierWarnings.log", "a");
    if (fp != NULL) {
        fputs(line, fp);
        fputc('\n', fp);
        fclose(fp);
    }
}
#endif

void langwarning_emit(const char *category, const char *message, bool raise_script_error_in_ui) {
    if (category == NULL)
        category = "runtime";
    if (message == NULL)
        message = "(no message)";

    char timestamp[32];
    format_timestamp(timestamp, sizeof timestamp);

    char line[512];
    snprintf(line, sizeof line, "[%s] [%s] %s", timestamp, category, message);

#if defined(FRONTIER_HEADLESS)
    (void) raise_script_error_in_ui;
    fprintf(stderr, "%s\n", line);
#else
    log_to_file(line);
    if (raise_script_error_in_ui) {
        char ui_line[512];
        snprintf(ui_line, sizeof ui_line, "%s", message);
        bigstring bs;
        copyctopstring(ui_line, bs);
        langerrormessage(bs);
    }
#endif
}
