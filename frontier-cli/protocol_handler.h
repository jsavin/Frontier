/*
    Frontier CLI - NDJSON Protocol Handler

    protocol_handler.h - Structured JSON protocol over stdin/stdout for batch test execution

    Activated by --protocol flag. Each message is one JSON object per line (NDJSON).
    Message shapes align with planning/gui/PROTOCOL.md for future GUI convergence.

    Supported operations:
      script/eval        - Evaluate a UserTalk expression, return result
      script/clearContext - Reset REPL variables and focus to root
      odb/get            - Get values from the ODB by dotted path
      odb/set            - Set (create or overwrite) values in the ODB
      odb/list           - List children of a table with optional depth
      odb/delete         - Delete values from the ODB
      shutdown           - Clean exit

    When --ws-port is specified, also serves the same operations over WebSocket.

    SPDX-License-Identifier: MIT

    Copyright (c) 1992-2004 UserLand Software, Inc.
    Copyright (c) 2025-2026 Frontier contributors

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
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
