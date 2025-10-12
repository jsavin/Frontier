/* portable/cli_executor.h - Test-only portable stub for CLI executor */

#ifndef PORTABLE_CLI_EXECUTOR_H
#define PORTABLE_CLI_EXECUTOR_H

#include <stdbool.h>

typedef struct usertalk_execution_t {
    char* script_source;
    char* result;
} usertalk_execution_t;

usertalk_execution_t* cli_create_execution_context(void);
bool cli_compile_script(const char* script, usertalk_execution_t* exec);
bool cli_execute_compiled_script(usertalk_execution_t* exec);
char* cli_get_execution_result_string(usertalk_execution_t* exec);
void cli_free_execution_context(usertalk_execution_t* exec);
void cli_free(void* p);

#endif /* PORTABLE_CLI_EXECUTOR_H */


