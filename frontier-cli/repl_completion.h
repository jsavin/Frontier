/* repl_completion.h -- 2026-06-09 JES #691 Phase C.0.2: REPL-agnostic completion API.
 * Shared by the legacy linenoise REPL (repl.c) and the boxen REPL
 * (boxen_repl.c) via callback adapters.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Frontier contributors
 */

#ifndef REPL_COMPLETION_H
#define REPL_COMPLETION_H

#include <stddef.h>

/* Generic add-completion callback.  ctx is opaque (caller-supplied). */
typedef void (*repl_completion_add_fn)(void *ctx, const char *completion);

/*
 * repl_complete_slash_command_path -- complete a path argument for slash
 * commands like /jump and /list.
 *
 * buf        -- current input buffer (NUL-terminated)
 * buf_len    -- length of buf (bytes, not including NUL)
 * path_offset -- byte offset in buf where the path argument starts
 * add        -- callback invoked for each candidate completion string
 * ctx        -- opaque context passed verbatim to add()
 *
 * Calls add(ctx, candidate) for every ODB path that matches the prefix
 * buf[path_offset..].  Does nothing if there are no matches.
 */
void repl_complete_slash_command_path(const char *buf, size_t buf_len,
                                      size_t path_offset,
                                      repl_completion_add_fn add, void *add_ctx);

/* NULL-terminated list of slash command names (without the leading '/').
 * Exposed for the boxen REPL completion engine. */
extern const char *const repl_slash_commands_list[];

#endif /* REPL_COMPLETION_H */
