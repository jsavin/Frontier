/*
 * cli_executor.h - Declarations for UserTalk script execution
 *
 * Defines the execution context structure and functions for compiling,
 * running, and retrieving results from UserTalk scripts.
 */

#ifndef CLI_EXECUTOR_H
#define CLI_EXECUTOR_H

#include "cli_utils.h"
#include "../Common/headers/lang.h"
#include "../Common/headers/strings.h"
#include "../Common/headers/memory.h"

typedef struct {
	char* script_source;
	char* result;
	char* error_message;
} usertalk_execution_t;

usertalk_execution_t* cli_create_execution_context(void);
void cli_free_execution_context(usertalk_execution_t* execution);

boolean cli_compile_script(const char* script_code, usertalk_execution_t* execution);
boolean cli_execute_compiled_script(usertalk_execution_t* execution);

boolean cli_execute_script_file(const char* script_path, boolean output_json);
boolean cli_execute_inline_script(const char* script_code, boolean output_json);

char* cli_get_execution_result_string(const usertalk_execution_t* execution);
const char* cli_get_execution_error(const usertalk_execution_t* execution);
boolean cli_has_execution_error(const usertalk_execution_t* execution);
void cli_print_execution_result(const usertalk_execution_t* execution);

#endif /* CLI_EXECUTOR_H */
