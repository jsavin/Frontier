#include "cli_executor.h"
#include "cli_executor.h"

/* 2025-12-08 Codex: Route long inline CLI scripts through langrunhandle so compiled evals return results without bogus empty verb names. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations for static helper functions
static void cli_print_json_escaped_string(const char* str);
static void cli_print_execution_result_json(const usertalk_execution_t* execution, boolean success);

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

#if defined(FRONTIER_HEADLESS)
    extern hdlhashtable currenthashtable;
    cli_log_debug("before execute currenthashtable=%p", (void *)currenthashtable);
#endif

    /* For short scripts, prefer langrunstring to avoid compiler overhead */
    if (len <= lenbigstring) {
        bigstring program;
        bigstring result;
        copyctopstring(execution->script_source, program);
        extern hdlhashtable currenthashtable;
        extern hdlhashtable roottable;
        extern boolean pushhashtable(hdlhashtable);
        extern boolean pophashtable(void);
        extern boolean langrunstring(const bigstring, bigstring);
        hdlhashtable saved_current = currenthashtable;
        hdlhashtable target_table = (roottable != NULL) ? roottable : saved_current;
        currenthashtable = target_table;
        boolean pushed = (target_table != NULL) ? pushhashtable(target_table) : false;
        boolean ok = langrunstring(program, result);
        if (pushed)
            pophashtable();
        currenthashtable = saved_current;
        if (!ok) {
            cli_set_execution_error_internal(execution, "Script execution failed");
            return false;
        }

#if defined(FRONTIER_HEADLESS)
        cli_log_debug("langrunstring ok; result length=%ld", (long) stringlength(result));
#endif

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

    bigstring bsresult;
    setemptystring(bsresult);
    if (!langrunhandle(htext, bsresult)) {
        cli_set_execution_error_internal(execution, "Failed to execute script");
        return false;
    }

#if defined(FRONTIER_HEADLESS)
    cli_log_debug("langrunhandle ok; result length=%ld", (long) stringlength(bsresult));
#endif

    execution->result = cli_malloc((size_t)stringlength(bsresult) + 1);
    if (execution->result == NULL) {
        cli_set_execution_error_internal(execution, "Out of memory while copying result");
        return false;
    }
    memcpy(execution->result, stringbaseaddress(bsresult), stringlength(bsresult));
    execution->result[stringlength(bsresult)] = '\0';
#if defined(FRONTIER_HEADLESS)
    cli_log_debug("after execute currenthashtable=%p", (void *)currenthashtable);
#endif
    return true;
}

boolean cli_execute_script_file(const char* script_path, boolean output_json) {
    if (!cli_file_exists(script_path)) {
        cli_log_error("Script file does not exist: %s", script_path);
        return false;
    }

    long size = 0;
    char* contents = cli_read_file(script_path, &size);
    if (contents == NULL) {
        return false;
    }

    boolean success = cli_execute_inline_script(contents, output_json);
    cli_free(contents);
    return success;
}

boolean cli_execute_inline_script(const char* script_code, boolean output_json) {
    usertalk_execution_t* exec = cli_create_execution_context();
    if (exec == NULL) {
        cli_log_error("Failed to create execution context");
        return false;
    }

    boolean ok = cli_compile_script(script_code, exec) && cli_execute_compiled_script(exec);

    if (output_json) {
        // Print JSON output to stdout
        cli_print_execution_result_json(exec, ok);

        // Also log errors to stderr for debugging (doesn't interfere with JSON on stdout)
        if (!ok) {
            const char* error = cli_get_execution_error(exec);
            if (error != NULL) {
                cli_log_error("Execution error: %s", error);
            } else {
                cli_log_error("Execution failed");
            }
        }
    } else {
        // Traditional text output
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

// Helper function to escape JSON strings
static void cli_print_json_escaped_string(const char* str) {
    if (str == NULL) {
        printf("null");
        return;
    }

    printf("\"");
    for (const char* p = str; *p != '\0'; p++) {
        switch (*p) {
            case '"':  printf("\\\""); break;
            case '\\': printf("\\\\"); break;
            case '\b': printf("\\b"); break;
            case '\f': printf("\\f"); break;
            case '\n': printf("\\n"); break;
            case '\r': printf("\\r"); break;
            case '\t': printf("\\t"); break;
            default:
                if ((unsigned char)*p < 32) {
                    printf("\\u%04x", (unsigned char)*p);
                } else {
                    putchar(*p);
                }
                break;
        }
    }
    printf("\"");
}

// Print execution result as JSON
static void cli_print_execution_result_json(const usertalk_execution_t* execution, boolean success) {
    printf("{\n");
    printf("  \"success\": %s,\n", success ? "true" : "false");

    if (success && execution != NULL && execution->result != NULL) {
        printf("  \"result\": ");
        cli_print_json_escaped_string(execution->result);
        printf(",\n");
        printf("  \"result_type\": \"string\",\n");
    } else {
        printf("  \"result\": null,\n");
        printf("  \"result_type\": null,\n");
    }

    if (!success && execution != NULL && execution->error_message != NULL) {
        printf("  \"error\": ");
        cli_print_json_escaped_string(execution->error_message);
        printf(",\n");
        printf("  \"error_type\": \"script_error\"\n");
    } else {
        printf("  \"error\": null,\n");
        printf("  \"error_type\": null\n");
    }

    printf("}\n");
}
