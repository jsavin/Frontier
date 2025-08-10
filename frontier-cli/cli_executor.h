/*
 * Frontier CLI - Command Line Interface for UserTalk Script Execution
 * CLI Executor Header
 * 
 * Copyright (C) 1992-2004 UserLand Software, Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef CLI_EXECUTOR_H
#define CLI_EXECUTOR_H

#include "../Common/SystemHeaders/standard.h"
#include "../Common/headers/lang.h"
#include "../Common/headers/db.h"

// UserTalk execution context
typedef struct {
    hdltreenode hcode;          // Compiled script tree
    hdlhashtable hcontext;      // Execution context
    tyvaluerecord vresult;      // Execution result
    boolean flsuccess;          // Success flag
    char* error_message;        // Error message
    boolean flcompiled;         // Whether script was compiled
} usertalk_execution_t;

// Script execution functions
boolean cli_execute_script_file(const char* script_path);
boolean cli_execute_inline_script(const char* script_code);
boolean cli_compile_script(const char* script_code, usertalk_execution_t* execution);
boolean cli_execute_compiled_script(usertalk_execution_t* execution);
void cli_cleanup_execution(usertalk_execution_t* execution);

// Result handling functions
char* cli_get_execution_result_string(const usertalk_execution_t* execution);
long cli_get_execution_result_number(const usertalk_execution_t* execution);
boolean cli_get_execution_result_boolean(const usertalk_execution_t* execution);
void cli_print_execution_result(const usertalk_execution_t* execution);

// Error handling functions
const char* cli_get_execution_error(const usertalk_execution_t* execution);
boolean cli_has_execution_error(const usertalk_execution_t* execution);
void cli_set_execution_error(usertalk_execution_t* execution, const char* error);

// Memory management functions
usertalk_execution_t* cli_create_execution_context(void);
void cli_free_execution_context(usertalk_execution_t* execution);

// Utility functions
boolean cli_validate_script_syntax(const char* script_code);
char* cli_format_script_result(const usertalk_execution_t* execution);

#endif // CLI_EXECUTOR_H
