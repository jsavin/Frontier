/*
 * Frontier CLI - NDJSON Protocol Handler
 *
 * protocol_handler.h - Structured JSON protocol over stdin/stdout for batch test execution
 *
 * Activated by --protocol flag. Each message is one JSON object per line (NDJSON).
 * Message shapes align with planning/gui/PROTOCOL.md for future GUI convergence.
 *
 * Supported operations:
 *   script/eval        - Evaluate a UserTalk expression, return result
 *   script/clearContext - Reset REPL variables and focus to root
 *   odb/get            - Get values from the ODB by dotted path
 *   odb/set            - Set (create or overwrite) values in the ODB
 *   odb/list           - List children of a table with optional depth
 *   odb/delete         - Delete values from the ODB
 *   shutdown           - Clean exit
 *
 * When --ws-port is specified, also serves the same operations over WebSocket.
 *
 * Copyright (C) 1992-2004 UserLand Software, Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef PROTOCOL_HANDLER_H
#define PROTOCOL_HANDLER_H

#include "cli_parser.h"
#include "ws_server.h"

/*
 * Main protocol loop. Reads NDJSON from stdin, dispatches operations,
 * writes NDJSON responses to stdout. Returns exit code (0 = clean shutdown).
 */
int protocol_main(cli_options_t *options, ws_server_t *ws_server);

#endif /* PROTOCOL_HANDLER_H */
