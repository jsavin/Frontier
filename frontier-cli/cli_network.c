/*
 * Frontier CLI - Command Line Interface for UserTalk Script Execution
 * CLI Network Implementation
 * 
 * Copyright (C) 1992-2004 UserLand Software, Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "cli_network.h"
#include "cli_utils.h"
#include "cli_executor.h"
#include "cli_database.h"

// Global network error buffer
static char g_network_error_buffer[1024] = {0};

// Global server state
static network_server_t* g_current_server = NULL;
static boolean g_server_running = false;

// Set network error
void cli_set_network_error(const char* error) {
    if (error == NULL) {
        g_network_error_buffer[0] = '\0';
    } else {
        strncpy(g_network_error_buffer, error, sizeof(g_network_error_buffer) - 1);
        g_network_error_buffer[sizeof(g_network_error_buffer) - 1] = '\0';
    }
}

// Get network error
const char* cli_get_network_error(void) {
    return g_network_error_buffer;
}

// Clear network error
void cli_clear_network_error(void) {
    g_network_error_buffer[0] = '\0';
}

// Validate port number
boolean cli_validate_port(int port) {
    return (port >= 1 && port <= 65535);
}

// Create network server
network_server_t* cli_create_network_server(int port) {
    if (!cli_validate_port(port)) {
        cli_set_network_error("Invalid port number");
        return NULL;
    }
    
    network_server_t* server = cli_calloc(1, sizeof(network_server_t));
    if (server == NULL) {
        cli_set_network_error("Failed to allocate network server");
        return NULL;
    }
    
    server->port = port;
    server->flhttp = false;
    server->flwebsocket = false;
    server->http_server = NULL;
    server->websocket_server = NULL;
    server->flrunning = false;
    
    cli_log_debug("Created network server for port %d", port);
    return server;
}

// Free network server
void cli_free_network_server(network_server_t* server) {
    if (server == NULL) {
        return;
    }
    
    // Stop server if running
    if (server->flrunning) {
        cli_stop_server(server);
    }
    
    cli_free(server);
    cli_log_debug("Freed network server");
}

// Start server
boolean cli_start_server(network_server_t* server) {
    if (server == NULL) {
        cli_set_network_error("Invalid server context");
        return false;
    }
    
    if (server->flrunning) {
        cli_set_network_error("Server is already running");
        return false;
    }
    
    cli_log_info("Starting network server on port %d", server->port);
    
    // For now, we'll implement a simple placeholder
    // In a real implementation, this would start the actual server
    server->flrunning = true;
    g_current_server = server;
    g_server_running = true;
    
    cli_log_info("Network server started on port %d", server->port);
    return true;
}

// Stop server
boolean cli_stop_server(network_server_t* server) {
    if (server == NULL) {
        return false;
    }
    
    if (!server->flrunning) {
        return true; // Already stopped
    }
    
    cli_log_info("Stopping network server on port %d", server->port);
    
    server->flrunning = false;
    if (g_current_server == server) {
        g_current_server = NULL;
        g_server_running = false;
    }
    
    cli_log_info("Network server stopped");
    return true;
}

// Start HTTP server
boolean cli_start_http_server(int port) {
    if (!cli_validate_port(port)) {
        cli_set_network_error("Invalid port number for HTTP server");
        return false;
    }
    
    cli_log_info("Starting HTTP server on port %d", port);
    
    // Create server context
    network_server_t* server = cli_create_network_server(port);
    if (server == NULL) {
        return false;
    }
    
    server->flhttp = true;
    
    // Start the server
    boolean success = cli_start_server(server);
    if (!success) {
        cli_free_network_server(server);
        return false;
    }
    
    cli_log_info("HTTP server started on port %d", port);
    return true;
}

// Stop HTTP server
boolean cli_stop_http_server(void) {
    if (g_current_server == NULL || !g_current_server->flhttp) {
        cli_set_network_error("No HTTP server running");
        return false;
    }
    
    return cli_stop_server(g_current_server);
}

// Check if HTTP server is running
boolean cli_http_server_is_running(void) {
    return (g_current_server != NULL && g_current_server->flhttp && g_current_server->flrunning);
}

// Start WebSocket server
boolean cli_start_websocket_server(int port) {
    if (!cli_validate_port(port)) {
        cli_set_network_error("Invalid port number for WebSocket server");
        return false;
    }
    
    cli_log_info("Starting WebSocket server on port %d", port);
    
    // Create server context
    network_server_t* server = cli_create_network_server(port);
    if (server == NULL) {
        return false;
    }
    
    server->flwebsocket = true;
    
    // Start the server
    boolean success = cli_start_server(server);
    if (!success) {
        cli_free_network_server(server);
        return false;
    }
    
    cli_log_info("WebSocket server started on port %d", port);
    return true;
}

// Stop WebSocket server
boolean cli_stop_websocket_server(void) {
    if (g_current_server == NULL || !g_current_server->flwebsocket) {
        cli_set_network_error("No WebSocket server running");
        return false;
    }
    
    return cli_stop_server(g_current_server);
}

// Check if WebSocket server is running
boolean cli_websocket_server_is_running(void) {
    return (g_current_server != NULL && g_current_server->flwebsocket && g_current_server->flrunning);
}

// Get server status
char* cli_get_server_status(void) {
    if (g_current_server == NULL) {
        return cli_strdup("No server running");
    }
    
    const char* type = g_current_server->flhttp ? "HTTP" : 
                      g_current_server->flwebsocket ? "WebSocket" : "Unknown";
    
    return cli_format_string("Server: %s\nPort: %d\nStatus: %s",
                           type, g_current_server->port,
                           g_current_server->flrunning ? "Running" : "Stopped");
}

// Print server information
void cli_print_server_info(const network_server_t* server) {
    if (server == NULL) {
        printf("Server: NULL\n");
        return;
    }
    
    printf("Server Information:\n");
    printf("  Port: %d\n", server->port);
    printf("  HTTP: %s\n", server->flhttp ? "enabled" : "disabled");
    printf("  WebSocket: %s\n", server->flwebsocket ? "enabled" : "disabled");
    printf("  Status: %s\n", server->flrunning ? "running" : "stopped");
}

// Handle HTTP request
char* cli_handle_http_request(const char* method, const char* path, const char* body) {
    if (method == NULL || path == NULL) {
        return cli_create_json_error("Invalid request parameters");
    }
    
    cli_log_debug("HTTP request: %s %s", method, path);
    
    // Handle different HTTP methods
    if (strcmp(method, "GET") == 0) {
        return cli_handle_http_get(path);
    } else if (strcmp(method, "POST") == 0) {
        return cli_handle_http_post(path, body);
    } else if (strcmp(method, "PUT") == 0) {
        return cli_handle_http_put(path, body);
    } else if (strcmp(method, "DELETE") == 0) {
        return cli_handle_http_delete(path);
    } else {
        return cli_create_json_error("Unsupported HTTP method");
    }
}

// Handle HTTP GET request
static char* cli_handle_http_get(const char* path) {
    if (strcmp(path, "/") == 0 || strcmp(path, "/status") == 0) {
        // Return server status
        char* status = cli_get_server_status();
        char* response = cli_create_json_success(status);
        cli_free(status);
        return response;
    } else if (strncmp(path, "/execute", 8) == 0) {
        // Execute UserTalk script
        const char* script = path + 8;
        if (*script == '?') script++;
        
        if (strlen(script) == 0) {
            return cli_create_json_error("No script provided");
        }
        
        // Execute the script
        usertalk_execution_t* execution = cli_create_execution_context();
        if (execution == NULL) {
            return cli_create_json_error("Failed to create execution context");
        }
        
        boolean compiled = cli_compile_script(script, execution);
        boolean executed = false;
        
        if (compiled) {
            executed = cli_execute_compiled_script(execution);
        }
        
        char* result = NULL;
        if (executed && execution->flsuccess) {
            result = cli_get_execution_result_string(execution);
        }
        
        char* response = NULL;
        if (result != NULL) {
            response = cli_create_json_success(result);
            cli_free(result);
        } else {
            response = cli_create_json_error(cli_get_execution_error(execution));
        }
        
        cli_free_execution_context(execution);
        return response;
    } else if (strncmp(path, "/database", 9) == 0) {
        // Database operations
        const char* operation = path + 9;
        if (*operation == '?') operation++;
        
        // Parse database operation
        // For now, return a placeholder
        return cli_create_json_success("Database operations not yet implemented");
    } else {
        return cli_create_json_error("Unknown endpoint");
    }
}

// Handle HTTP POST request
static char* cli_handle_http_post(const char* path, const char* body) {
    if (strcmp(path, "/execute") == 0) {
        // Execute UserTalk script from body
        if (body == NULL || strlen(body) == 0) {
            return cli_create_json_error("No script provided in body");
        }
        
        // Execute the script
        usertalk_execution_t* execution = cli_create_execution_context();
        if (execution == NULL) {
            return cli_create_json_error("Failed to create execution context");
        }
        
        boolean compiled = cli_compile_script(body, execution);
        boolean executed = false;
        
        if (compiled) {
            executed = cli_execute_compiled_script(execution);
        }
        
        char* result = NULL;
        if (executed && execution->flsuccess) {
            result = cli_get_execution_result_string(execution);
        }
        
        char* response = NULL;
        if (result != NULL) {
            response = cli_create_json_success(result);
            cli_free(result);
        } else {
            response = cli_create_json_error(cli_get_execution_error(execution));
        }
        
        cli_free_execution_context(execution);
        return response;
    } else {
        return cli_create_json_error("Unknown POST endpoint");
    }
}

// Handle HTTP PUT request
static char* cli_handle_http_put(const char* path, const char* body) {
    return cli_create_json_error("PUT method not yet implemented");
}

// Handle HTTP DELETE request
static char* cli_handle_http_delete(const char* path) {
    return cli_create_json_error("DELETE method not yet implemented");
}

// Handle WebSocket message
char* cli_handle_websocket_message(const char* message) {
    if (message == NULL) {
        return cli_create_json_error("No message provided");
    }
    
    cli_log_debug("WebSocket message: %s", message);
    
    // For now, treat WebSocket messages as UserTalk scripts
    usertalk_execution_t* execution = cli_create_execution_context();
    if (execution == NULL) {
        return cli_create_json_error("Failed to create execution context");
    }
    
    boolean compiled = cli_compile_script(message, execution);
    boolean executed = false;
    
    if (compiled) {
        executed = cli_execute_compiled_script(execution);
    }
    
    char* result = NULL;
    if (executed && execution->flsuccess) {
        result = cli_get_execution_result_string(execution);
    }
    
    char* response = NULL;
    if (result != NULL) {
        response = cli_create_json_success(result);
        cli_free(result);
    } else {
        response = cli_create_json_error(cli_get_execution_error(execution));
    }
    
    cli_free_execution_context(execution);
    return response;
}

// Create JSON response
char* cli_create_json_response(boolean success, const char* data, const char* error) {
    if (success) {
        return cli_format_string("{\"success\":true,\"data\":\"%s\"}", data ? data : "");
    } else {
        return cli_format_string("{\"success\":false,\"error\":\"%s\"}", error ? error : "Unknown error");
    }
}

// Create JSON error response
char* cli_create_json_error(const char* error) {
    return cli_create_json_response(false, NULL, error);
}

// Create JSON success response
char* cli_create_json_success(const char* data) {
    return cli_create_json_response(true, data, NULL);
}
