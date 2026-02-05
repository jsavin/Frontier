/*
 * cli_executor.c - UserTalk script compilation and execution engine
 *
 * Provides the execution context for running UserTalk scripts, including
 * compilation, execution, result capture, and JSON output formatting.
 * Uses langrunstring for short scripts and langrunhandle for longer ones.
 */

#include "cli_executor.h"

/* 2025-12-08 Codex: Route long inline CLI scripts through langrunhandle so compiled evals return results without bogus empty verb names. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations for static helper functions
static void cli_print_json_escaped_string(const char* str);
static void cli_print_execution_result_json(const usertalk_execution_t* execution, boolean success);

/* Duplicates a C string using CLI memory allocation. */
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

/* Allocates a new execution context for running UserTalk scripts. */
usertalk_execution_t* cli_create_execution_context(void) {
    usertalk_execution_t* exec = cli_calloc(1, sizeof(*exec));
    return exec;
}

/* Frees an execution context and all associated memory. */
void cli_free_execution_context(usertalk_execution_t* execution) {
    if (execution == NULL) {
        return;
    }
    cli_free(execution->script_source);
    cli_free(execution->result);
    cli_free(execution->error_message);
    cli_free(execution);
}

/* Clears any existing error message from the execution context. */
static void cli_clear_execution_error(usertalk_execution_t* execution) {
    if (execution == NULL) {
        return;
    }
    cli_free(execution->error_message);
    execution->error_message = NULL;
}

/* Sets the error message in the execution context. */
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

/* Stores the script source in the execution context for later execution. */
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

/* Runs the compiled script and captures the result or error. */
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

    /*
     * NOTE: We used to have a short-script optimization that used langrunstring()
     * for scripts <= 255 bytes. This was removed because langrunstring() returns
     * results in a bigstring (max 255 chars), truncating longer results.
     * Now all scripts use langrunhandle_value() which supports arbitrary length results.
     */
    tyvaluerecord value;
    Handle htext = NULL;

    initvalue(&value, novaluetype);

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

    /* Use langrunhandle_value to avoid 255-byte bigstring truncation */
    if (!langrunhandle_value(htext, &value)) {
        cli_set_execution_error_internal(execution, "Failed to execute script");
        return false;
    }

    /* Coerce result to string for output */
    if (value.valuetype == novaluetype) {
        /* No result - use empty string */
        execution->result = cli_strdup("");
    } else if (!coercetostring(&value)) {
        disposevaluerecord(value, false);
        cli_set_execution_error_internal(execution, "Failed to coerce result to string");
        return false;
    } else {
        /* Copy string handle contents to execution result */
        Handle hstring = value.data.stringvalue;
        long slen = gethandlesize(hstring);

#if defined(FRONTIER_HEADLESS)
        cli_log_debug("langrunhandle_value ok; result length=%ld", slen);
#endif

        execution->result = cli_malloc((size_t)slen + 1);
        if (execution->result == NULL) {
            disposevaluerecord(value, false);
            cli_set_execution_error_internal(execution, "Out of memory while copying result");
            return false;
        }
        memcpy(execution->result, *hstring, slen);
        execution->result[slen] = '\0';

        disposevaluerecord(value, false);
    }

#if defined(FRONTIER_HEADLESS)
    cli_log_debug("after execute currenthashtable=%p", (void *)currenthashtable);
#endif
    return true;
}

/* Reads a script file from disk and executes it. */
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

/* Executes an inline script string and outputs the result. */
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

/* Returns a copy of the execution result string (caller must free). */
char* cli_get_execution_result_string(const usertalk_execution_t* execution) {
    if (execution == NULL || execution->result == NULL) {
        return NULL;
    }
    return cli_strdup(execution->result);
}

/* Returns the error message from the execution context (do not free). */
const char* cli_get_execution_error(const usertalk_execution_t* execution) {
    if (execution == NULL || execution->error_message == NULL) {
        return NULL;
    }
    return execution->error_message;
}

/* Checks if the execution context contains an error. */
boolean cli_has_execution_error(const usertalk_execution_t* execution) {
    return execution != NULL && execution->error_message != NULL;
}

/* Prints the execution result to stdout as plain text. */
void cli_print_execution_result(const usertalk_execution_t* execution) {
    if (execution == NULL || execution->result == NULL) {
        printf("(no result)\n");
        return;
    }
    printf("%s\n", execution->result);
}

// Helper function to escape JSON strings
//
// NOTE: This function expects UTF-8 encoded strings and passes multi-byte
// UTF-8 sequences through directly (valid per JSON RFC 8259). Control characters
// (< 32) are escaped as \uXXXX. Invalid UTF-8 sequences may produce malformed JSON.
//
// Current Frontier string encoding: Pascal strings (bigstring/pstring) are
// length-prefixed byte arrays, typically ASCII or MacRoman. Future roadmap
// includes UTF-8 migration for runtime strings.
//
// For now, this handles ASCII and valid UTF-8 correctly. Edge cases with invalid
// multi-byte sequences will be addressed when runtime string encoding is unified.
static void cli_print_json_escaped_string(const char* str) {
    if (str == NULL) {
        printf( "null");
        return;
    }

    printf( "\"");
    for (const char* p = str; *p != '\0'; p++) {
        switch (*p) {
            case '"':  printf( "\\\""); break;
            case '\\': printf( "\\\\"); break;
            case '\b': printf( "\\b"); break;
            case '\f': printf( "\\f"); break;
            case '\n': printf( "\\n"); break;
            case '\r': printf( "\\r"); break;
            case '\t': printf( "\\t"); break;
            default:
                if ((unsigned char)*p < 32) {
                    // Escape control characters
                    printf( "\\u%04x", (unsigned char)*p);
                } else {
                    // Pass through printable ASCII and UTF-8 multi-byte sequences
                    // JSON spec (RFC 8259) allows unescaped UTF-8
                    putchar(*p);
                }
                break;
        }
    }
    printf( "\"");
}

// Print execution result as JSON to stdout for clean separation from prompts/logs
static void cli_print_execution_result_json(const usertalk_execution_t* execution, boolean success) {
    printf( "{\n");
    printf( "  \"success\": %s,\n", success ? "true" : "false");

    if (success && execution != NULL && execution->result != NULL) {
        printf( "  \"result\": ");
        cli_print_json_escaped_string(execution->result);
        printf( ",\n");
        printf( "  \"result_type\": \"string\",\n");
    } else {
        printf( "  \"result\": null,\n");
        printf( "  \"result_type\": null,\n");
    }

    if (!success && execution != NULL && execution->error_message != NULL) {
        printf( "  \"error\": ");
        cli_print_json_escaped_string(execution->error_message);
        printf( ",\n");
        printf( "  \"error_type\": \"script_error\"\n");
    } else {
        printf( "  \"error\": null,\n");
        printf( "  \"error_type\": null\n");
    }

    printf( "}\n");
}
