#ifndef LANGWARNINGS_H
#define LANGWARNINGS_H

#include <stdbool.h>

/*
 * langwarnings.h - shared warning/deprecation logging helpers.
 *
 * The caller passes a category tag and human-readable message. In GUI builds
 * we optionally escalate to a script error; in headless builds we funnel the
 * warning to stderr. All configurations append the warning to a local log file
 * when possible.
 */

void langwarning_emit(const char *category, const char *message, bool raise_script_error_in_ui);

#endif /* LANGWARNINGS_H */
