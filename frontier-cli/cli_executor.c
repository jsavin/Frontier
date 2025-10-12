/*
 * Frontier CLI - Command Line Interface for UserTalk Script Execution
 * CLI Executor Implementation
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

#include "cli_executor.h"
#include "cli_utils.h"

// Include Frontier language headers
#include "../Common/headers/lang.h"
#include "../Common/headers/langevaluate.h"
#include "../Common/headers/langvalue.h"

// Create a new execution context
usertalk_execution_t* cli_create_execution_context(void) {
    usertalk_execution_t* execution = cli_calloc(1, sizeof(usertalk_execution_t));
    if (execution == NULL) {
        cli_log_error("Failed to allocate execution context");
        return NULL;
    }
    
    // Initialize the execution context
    execution->hcode = nil;
    execution->hcontext = nil;
    execution->flsuccess = false;
    execution->error_message = NULL;
    execution->flcompiled = false;
    
    // Initialize the result value
    setbooleanvalue(false, &execution->vresult);
    
    cli_log_debug("Created UserTalk execution context");
    return execution;
}

// Free an execution context
void cli_free_execution_context(usertalk_execution_t* execution) {
    if (execution == NULL) {
        return;
    }
    
    // Cleanup compiled code tree
    if (execution->hcode != nil) {
        langdisposetree(execution->hcode);
        execution->hcode = nil;
    }
    
    // Free error message
    if (execution->error_message != NULL) {
        cli_free(execution->error_message);
        execution->error_message = NULL;
    }
    
    // Free the execution context itself
    cli_free(execution);
    
    cli_log_debug("Freed UserTalk execution context");
}

// Set execution error
void cli_set_execution_error(usertalk_execution_t* execution, const char* error) {
    if (execution == NULL) {
        return;
    }
    
    if (execution->error_message != NULL) {
        cli_free(execution->error_message);
    }
    
    execution->error_message = cli_strdup(error);
    execution->flsuccess = false;
    
    cli_log_error("UserTalk execution error: %s", error);
}

// Get execution error
const char* cli_get_execution_error(const usertalk_execution_t* execution) {
    if (execution == NULL) {
        return "Invalid execution context";
    }
    
    return execution->error_message ? execution->error_message : "No error";
}

// Check if execution has error
boolean cli_has_execution_error(const usertalk_execution_t* execution) {
    if (execution == NULL) {
        return true;
    }
    
    return (execution->error_message != NULL);
}

// Validate script syntax
boolean cli_validate_script_syntax(const char* script_code) {
    if (script_code == NULL) {
        return false;
    }
    
    // Create a temporary execution context for validation
    usertalk_execution_t* temp_execution = cli_create_execution_context();
    if (temp_execution == NULL) {
        return false;
    }
    
    // Try to compile the script
    boolean valid = cli_compile_script(script_code, temp_execution);
    
    // Cleanup
    cli_free_execution_context(temp_execution);
    
    return valid;
}

// Compile UserTalk script
boolean cli_compile_script(const char* script_code, usertalk_execution_t* execution) {
    if (script_code == NULL || execution == NULL) {
        cli_set_execution_error(execution, "Invalid parameters for script compilation");
        return false;
    }
    
    cli_log_debug("Compiling UserTalk script (%zu characters)", strlen(script_code));
    
    // Create a text handle from the script code
    Handle htext = nil;
    if (!newtexthandle(script_code, &htext)) {
        cli_set_execution_error(execution, "Failed to create text handle for script");
        return false;
    }
    
    // Build the code tree
    boolean compiled = langbuildtree(htext, true, &execution->hcode);
    
    if (!compiled) {
        cli_set_execution_error(execution, "Failed to compile UserTalk script");
        disposehandle(htext);
        return false;
    }
    
    execution->flcompiled = true;
    cli_log_debug("Successfully compiled UserTalk script");
    
    // Cleanup text handle
    disposehandle(htext);
    
    return true;
}

// Execute compiled script
boolean cli_execute_compiled_script(usertalk_execution_t* execution) {
    if (execution == NULL) {
        return false;
    }
    
    if (!execution->flcompiled || execution->hcode == nil) {
        cli_set_execution_error(execution, "No compiled script to execute");
        return false;
    }
    
    cli_log_debug("Executing compiled UserTalk script");
    
    // Execute the script using Frontier's runtime
    boolean success = langrun(execution->hcode, &execution->vresult);
    
    if (!success) {
        cli_set_execution_error(execution, "Failed to execute UserTalk script");
        return false;
    }
    
    execution->flsuccess = true;
    cli_log_debug("Successfully executed UserTalk script");
    
    return true;
}

// Execute script from file
boolean cli_execute_script_file(const char* script_path) {
    if (script_path == NULL) {
        cli_log_error("No script file specified");
        return false;
    }
    
    cli_log_info("Executing script file: %s", script_path);
    
    // Check if file exists
    if (!cli_file_exists(script_path)) {
        cli_log_error("Script file does not exist: %s", script_path);
        return false;
    }
    
    // Read the script file
    long file_size;
    char* script_content = cli_read_file(script_path, &file_size);
    if (script_content == NULL) {
        cli_log_error("Failed to read script file: %s", script_path);
        return false;
    }
    
    // Execute the script
    boolean success = cli_execute_inline_script(script_content);
    
    // Cleanup
    cli_free(script_content);
    
    return success;
}

// Execute inline script
boolean cli_execute_inline_script(const char* script_code) {
    if (script_code == NULL) {
        cli_log_error("No script code specified");
        return false;
    }
    
    cli_log_info("Executing inline script (%zu characters)", strlen(script_code));
    
    // Create execution context
    usertalk_execution_t* execution = cli_create_execution_context();
    if (execution == NULL) {
        cli_log_error("Failed to create execution context");
        return false;
    }
    
    // Compile the script
    boolean compiled = cli_compile_script(script_code, execution);
    if (!compiled) {
        cli_log_error("Failed to compile script: %s", cli_get_execution_error(execution));
        cli_free_execution_context(execution);
        return false;
    }
    
    // Execute the script
    boolean executed = cli_execute_compiled_script(execution);
    if (!executed) {
        cli_log_error("Failed to execute script: %s", cli_get_execution_error(execution));
        cli_free_execution_context(execution);
        return false;
    }
    
    // Print the result
    cli_print_execution_result(execution);
    
    // Cleanup
    cli_free_execution_context(execution);
    
    return true;
}

// Get execution result as string
char* cli_get_execution_result_string(const usertalk_execution_t* execution) {
    if (execution == NULL || !execution->flsuccess) {
        return cli_strdup("");
    }
    
    // Convert result to string
    bigstring bsresult;
    setemptystring(bsresult);
    
    if (coercetostring(&execution->vresult)) {
        texthandletostring(execution->vresult.data.stringvalue, bsresult);
        return cli_strdup(bsresult);
    }
    
    return cli_strdup("");
}

// Get execution result as number
long cli_get_execution_result_number(const usertalk_execution_t* execution) {
    if (execution == NULL || !execution->flsuccess) {
        return 0;
    }
    
    // Convert result to number
    if (execution->vresult.valuetype == numbervaluetype) {
        return execution->vresult.data.numbervalue;
    }
    
    // Try to coerce to number
    tyvaluerecord coerced = execution->vresult;
    if (coercetonumber(&coerced)) {
        return coerced.data.numbervalue;
    }
    
    return 0;
}

// Get execution result as boolean
boolean cli_get_execution_result_boolean(const usertalk_execution_t* execution) {
    if (execution == NULL || !execution->flsuccess) {
        return false;
    }
    
    // Convert result to boolean
    if (execution->vresult.valuetype == booleanvaluetype) {
        return execution->vresult.data.booleanvalue;
    }
    
    // Try to coerce to boolean
    tyvaluerecord coerced = execution->vresult;
    if (coercetoboolean(&coerced)) {
        return coerced.data.booleanvalue;
    }
    
    return false;
}

// Print execution result
void cli_print_execution_result(const usertalk_execution_t* execution) {
    if (execution == NULL) {
        printf("Error: Invalid execution context\n");
        return;
    }
    
    if (!execution->flsuccess) {
        printf("Error: %s\n", cli_get_execution_error(execution));
        return;
    }
    
    // Format and print the result
    char* formatted_result = cli_format_script_result(execution);
    if (formatted_result != NULL) {
        printf("%s\n", formatted_result);
        cli_free(formatted_result);
    } else {
        printf("(no result)\n");
    }
}

// Format script result for output
char* cli_format_script_result(const usertalk_execution_t* execution) {
    if (execution == NULL || !execution->flsuccess) {
        return cli_strdup("");
    }
    
    // Handle different value types
    switch (execution->vresult.valuetype) {
        case stringvaluetype:
            {
                bigstring bsresult;
                setemptystring(bsresult);
                texthandletostring(execution->vresult.data.stringvalue, bsresult);
                return cli_strdup(bsresult);
            }
            
        case numbervaluetype:
            return cli_format_string("%ld", execution->vresult.data.numbervalue);
            
        case booleanvaluetype:
            return cli_strdup(execution->vresult.data.booleanvalue ? "true" : "false");
            
        case addressvaluetype:
            return cli_format_string("@%p", execution->vresult.data.addressvalue);
            
        case codevaluetype:
            return cli_strdup("(compiled code)");
            
        case listvaluetype:
            return cli_strdup("(list)");
            
        case recordvaluetype:
            return cli_strdup("(record)");
            
        case tablevaluetype:
            return cli_strdup("(table)");
            
        default:
            return cli_strdup("(unknown type)");
    }
}

// Cleanup execution resources
void cli_cleanup_execution(usertalk_execution_t* execution) {
    if (execution == NULL) {
        return;
    }
    
    cli_log_debug("Cleaning up execution resources");
    
    // Cleanup compiled code tree
    if (execution->hcode != nil) {
        langdisposetree(execution->hcode);
        execution->hcode = nil;
    }
    
    // Clear result value
    cleartmpstack();
    
    execution->flcompiled = false;
    execution->flsuccess = false;
}
