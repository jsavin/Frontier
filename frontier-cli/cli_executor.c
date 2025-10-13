#include "cli_executor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* cli_dup_string(const char* source) {
    if (source == NULL) {
        return NULL;
    }
    size_t len = strlen(source);
    char* copy = cli_malloc(len + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, source, len + 1);
    return copy;
}

usertalk_execution_t* cli_create_execution_context(void) {
    usertalk_execution_t* exec = cli_calloc(1, sizeof(*exec));
    return exec;
}

void cli_free_execution_context(usertalk_execution_t* execution) {
    if (execution == NULL) {
        return;
    }
    cli_free(execution->script_source);
    cli_free(execution->result);
    cli_free(execution->error_message);
    cli_free(execution);
}

static void cli_clear_execution_error(usertalk_execution_t* execution) {
    if (execution == NULL) {
        return;
    }
    cli_free(execution->error_message);
    execution->error_message = NULL;
}

static void cli_set_execution_error_internal(usertalk_execution_t* execution, const char* message) {
    if (execution == NULL) {
        return;
    }
    cli_free(execution->error_message);
    execution->error_message = cli_dup_string(message);
    if (execution->error_message == NULL) {
        execution->error_message = cli_strdup("Unknown error");
    }
}

boolean cli_compile_script(const char* script_code, usertalk_execution_t* execution) {
    if (execution == NULL) {
        return false;
    }

    cli_clear_execution_error(execution);

    if (script_code == NULL || strlen(script_code) == 0) {
        cli_set_execution_error_internal(execution, "Script is empty");
        return false;
    }

    cli_free(execution->script_source);
    execution->script_source = cli_dup_string(script_code);
    if (execution->script_source == NULL) {
        cli_set_execution_error_internal(execution, "Out of memory while copying script");
        return false;
    }

    return true;
}

boolean cli_execute_compiled_script(usertalk_execution_t* execution) {
    if (execution == NULL || execution->script_source == NULL) {
        return false;
    }

    cli_clear_execution_error(execution);
    cli_free(execution->result);
    execution->result = NULL;

    size_t len = strlen(execution->script_source);

    if (len <= lenbigstring) {
        bigstring program;
        bigstring result;
        copyctopstring(execution->script_source, program);
        if (!langrunstringnoerror(program, result)) {
            cli_set_execution_error_internal(execution, "Script execution failed");
            return false;
        }

        execution->result = cli_malloc((size_t)stringlength(result) + 1);
        if (execution->result == NULL) {
            cli_set_execution_error_internal(execution, "Out of memory while copying result");
            return false;
        }
        memcpy(execution->result, stringbaseaddress(result), stringlength(result));
        execution->result[stringlength(result)] = '\0';
        return true;
    }

    tyvaluerecord value; setnilvalue(&value);
    Handle htext = NULL;
    if (!newemptyhandle(&htext)) {
        cli_set_execution_error_internal(execution, "Unable to allocate script handle");
        return false;
    }

    if (!sethandlesize(htext, (long)len)) {
        disposehandle(htext);
        cli_set_execution_error_internal(execution, "Unable to resize script handle");
        return false;
    }

    HLock(htext);
    memcpy(*htext, execution->script_source, len);
    HUnlock(htext);

    hdltreenode hcode = NULL;
    if (!langcompiletext(htext, false, &hcode)) {
        disposehandle(htext);
        cli_set_execution_error_internal(execution, "Failed to compile script");
        return false;
    }

    tyvaluerecord params; setnilvalue(&params);
    tyvaluerecord resultValue; setnilvalue(&resultValue);
    bigstring empty; setstringlength(empty, 0);
    extern boolean langrunscriptcode(hdlhashtable, bigstring, hdltreenode, tyvaluerecord*, hdlhashtable, tyvaluerecord*);
    boolean ok = langrunscriptcode(NULL, empty, hcode, &params, NULL, &resultValue);
    disposehandle(htext);

    if (!ok) {
        cli_set_execution_error_internal(execution, "Failed to execute script");
        return false;
    }

    if (!coercetostring(&resultValue)) {
        cli_set_execution_error_internal(execution, "Unable to coerce result to string");
        return false;
    }

    bigstring bsresult;
    copyheapstring(resultValue.data.stringvalue, bsresult);
    execution->result = cli_malloc((size_t)stringlength(bsresult) + 1);
    if (execution->result == NULL) {
        cli_set_execution_error_internal(execution, "Out of memory while copying result");
        return false;
    }
    memcpy(execution->result, stringbaseaddress(bsresult), stringlength(bsresult));
    execution->result[stringlength(bsresult)] = '\0';
    return true;
}

boolean cli_execute_script_file(const char* script_path) {
    if (!cli_file_exists(script_path)) {
        cli_log_error("Script file does not exist: %s", script_path);
        return false;
    }

    long size = 0;
    char* contents = cli_read_file(script_path, &size);
    if (contents == NULL) {
        return false;
    }

    boolean success = cli_execute_inline_script(contents);
    cli_free(contents);
    return success;
}

boolean cli_execute_inline_script(const char* script_code) {
    usertalk_execution_t* exec = cli_create_execution_context();
    if (exec == NULL) {
        cli_log_error("Failed to create execution context");
        return false;
    }

    boolean ok = cli_compile_script(script_code, exec) && cli_execute_compiled_script(exec);
    if (!ok) {
        const char* error = cli_get_execution_error(exec);
        if (error != NULL) {
            cli_log_error("Execution error: %s", error);
        } else {
            cli_log_error("Execution failed");
        }
    } else {
        cli_print_execution_result(exec);
    }

    cli_free_execution_context(exec);
    return ok;
}

char* cli_get_execution_result_string(const usertalk_execution_t* execution) {
    if (execution == NULL || execution->result == NULL) {
        return NULL;
    }
    return cli_strdup(execution->result);
}

const char* cli_get_execution_error(const usertalk_execution_t* execution) {
    if (execution == NULL || execution->error_message == NULL) {
        return NULL;
    }
    return execution->error_message;
}

boolean cli_has_execution_error(const usertalk_execution_t* execution) {
    return execution != NULL && execution->error_message != NULL;
}

void cli_print_execution_result(const usertalk_execution_t* execution) {
    if (execution == NULL || execution->result == NULL) {
        printf("(no result)\n");
        return;
    }
    printf("%s\n", execution->result);
}
