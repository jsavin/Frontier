/*
 * Frontier CLI - Command Line Interface for UserTalk Script Execution
 * CLI Network Header
 * 
 * Copyright (C) 1992-2004 UserLand Software, Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef CLI_NETWORK_H
#define CLI_NETWORK_H

#include "../Common/SystemHeaders/standard.h"
#include "../Common/headers/db.h"
#include "cli_executor.h"
#include "cli_database.h"

// Network server structure
typedef struct {
    int port;                   // Server port
    boolean flhttp_enabled;     // HTTP server enabled
    boolean flwebsocket_enabled; // WebSocket server enabled
    void* http_server;          // HTTP server handle
    void* websocket_server;     // WebSocket server handle
    boolean flrunning;          // Server running status
} network_server_t;

// Server management functions
network_server_t* cli_create_network_server(int port);
boolean cli_start_server(network_server_t* server);
boolean cli_stop_server(network_server_t* server);
void cli_free_network_server(network_server_t* server);

// HTTP server functions
boolean cli_start_http_server(network_server_t* server);
boolean cli_stop_http_server(network_server_t* server);
boolean cli_handle_http_request(const char* method, const char* path, const char* body, char** response);

// WebSocket server functions
boolean cli_start_websocket_server(network_server_t* server);
boolean cli_stop_websocket_server(network_server_t* server);
boolean cli_handle_websocket_message(const char* message, char** response);

// Response formatting functions
char* cli_create_json_response(boolean success, const char* data, const char* error);
char* cli_format_execution_result(const usertalk_execution_t* execution);
char* cli_format_database_result(const cli_database_t* db_context, const char* operation);

// Utility functions
boolean cli_validate_port(int port);
char* cli_get_server_status(const network_server_t* server);
boolean cli_is_server_running(const network_server_t* server);

#endif // CLI_NETWORK_H
