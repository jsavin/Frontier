/*
 * Frontier CLI - Operation Handler
 *
 * op_handler.c - Transport-agnostic JSON operation dispatch
 *
 * Contains the op_dispatch() entry point that both the stdio NDJSON protocol
 * handler and the WebSocket server call into. Operation handlers (script/eval,
 * script/clearContext, odb/get etc., shutdown) live here so both transports share one
 * implementation.
 *
 * JSON parsing uses the vendored cJSON library for both envelope fields
 * (op, id) and ODB operation parameters.
 *
 * Copyright (C) 1992-2004 UserLand Software, Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "op_handler.h"
#include "odb_ops.h"
#include "cli_json_output.h"
#include "repl_variables.h"
#include "repl.h"

#include "../Common/headers/frontier.h"
#include "../Common/headers/lang.h"
#include "../Common/headers/langexternal.h"
#include "../Common/headers/memory.h"
#include "../Common/headers/logging.h"
#include "../Common/headers/langinternal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "../third_party/cJSON/cJSON.h"

/* Maximum number of items in a single ODB batch request */
#define OP_MAX_BATCH_SIZE 1000

/* ========================================================================
 * Response helpers — build JSON response string, send via transport
 * ======================================================================== */

/*
 * Send a pre-formatted JSON string via the transport.
 */
static void transport_send(transport_t *transport, const char *json) {
    transport->write_line(transport->ctx, json, strlen(json));
}

/*
 * Format and send a response. Uses a static buffer for small responses
 * and falls back to malloc for larger ones.
 *
 * Note: Three response-building mechanisms coexist in this file:
 * - send_response: vsnprintf for simple pre-escaped JSON (e.g. odb results)
 * - send_error / send_eval_success: open_memstream for runtime JSON escaping
 * - ODB handlers: cJSON_PrintUnformatted for structured results
 * Consolidating to a single mechanism is a follow-up task.
 */
static void send_response(transport_t *transport, long id, const char *fmt, ...) {
    char buf[4096];
    va_list args;

    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (n >= 0 && (size_t)n < sizeof(buf)) {
        transport->write_line(transport->ctx, buf, (size_t)n);
    } else if (n >= 0) {
        size_t needed = (size_t)n + 1;
        char *heap = malloc(needed);
        if (heap != NULL) {
            va_start(args, fmt);
            vsnprintf(heap, needed, fmt, args);
            va_end(args);
            transport->write_line(transport->ctx, heap, (size_t)n);
            free(heap);
        } else {
            log_error(LOG_COMP_GENERAL, "op_handler: failed to allocate response buffer (%zu bytes)", needed);
            /* Send minimal error so client doesn't hang */
            char fallback[128];
            int fb = snprintf(fallback, sizeof(fallback),
                              "{\"id\":%ld,\"success\":false,\"error\":{\"message\":\"Server out of memory\"}}", id);
            if (fb > 0 && (size_t)fb < sizeof(fallback)) {
                transport->write_line(transport->ctx, fallback, (size_t)fb);
            }
        }
    }
}

/*
 * Send a simple success ack.
 */
static void send_ack(long id, transport_t *transport) {
    send_response(transport, id, "{\"id\":%ld,\"success\":true}", id);
}

/*
 * Send an error response.
 */
static void send_error(long id, const char *message, transport_t *transport) {
    /* Build the response with properly escaped error message.
     * We use a FILE* buffer via open_memstream for JSON escaping.
     * Note: open_memstream requires POSIX.1-2008 / macOS 10.13+. */
    char *buf = NULL;
    size_t buf_len = 0;
    FILE *f = open_memstream(&buf, &buf_len);
    if (f == NULL) {
        /* Fallback: send minimal error so client doesn't hang */
        char fallback[256];
        int fb = snprintf(fallback, sizeof(fallback),
                          "{\"id\":%ld,\"success\":false,\"error\":{\"message\":\"Internal error\"}}", id);
        if (fb > 0 && (size_t)fb < sizeof(fallback)) {
            transport->write_line(transport->ctx, fallback, (size_t)fb);
        }
        return;
    }

    fprintf(f, "{\"id\":%ld,\"error\":{\"message\":", id);
    cli_json_write_escaped_string(f, message);
    fprintf(f, "},\"success\":false}");
    fclose(f);

    if (buf != NULL) {
        transport->write_line(transport->ctx, buf, buf_len);
        free(buf);
    }
}

/*
 * Send a success response with a script eval result value.
 */
static void send_eval_success(long id, tyvaluerecord *val, transport_t *transport) {
    const char *type_name = type_name_str(val->valuetype);

    tyvaluerecord coerced = *val;

    if (coerced.valuetype == novaluetype) {
        send_response(transport, id,
            "{\"id\":%ld,\"result\":{\"value\":null,\"type\":\"none\"},\"success\":true}",
            id);
        return;
    }

    if (coerced.valuetype == externalvaluetype) {
        hdlexternalvariable hv = (hdlexternalvariable)coerced.data.externalvalue;
        if (hv != nil && (**hv).id == idtableprocessor) {
            send_response(transport, id,
                "{\"id\":%ld,\"result\":{\"value\":\"[table]\",\"type\":\"table\"},\"success\":true}",
                id);
            return;
        }
    }

    /* Track whether coercion allocated a new handle (non-string types).
     * If the value is already a string, coercetostring is a no-op and
     * coerced shares the same handle — disposing it would double-free. */
    boolean coerced_allocated = (coerced.valuetype != stringvaluetype);

    if (!coercetostring(&coerced)) {
        send_response(transport, id,
            "{\"id\":%ld,\"result\":{\"value\":null,\"type\":\"%s\"},\"success\":true}",
            id, type_name);
        return;
    }

    Handle hstring = coerced.data.stringvalue;
    long slen = gethandlesize(hstring);

    /* Build response with properly escaped value string */
    char *buf = NULL;
    size_t buf_len = 0;
    FILE *f = open_memstream(&buf, &buf_len);
    if (f == NULL) {
        if (coerced_allocated) disposevaluerecord(coerced, false);
        return;
    }

    fprintf(f, "{\"id\":%ld,\"result\":{\"value\":", id);
    cli_json_write_escaped_buffer(f, (const char *)*hstring, slen);
    fprintf(f, ",\"type\":\"%s\"},\"success\":true}", type_name);
    fclose(f);

    if (buf != NULL) {
        transport->write_line(transport->ctx, buf, buf_len);
        free(buf);
    }

    /* Dispose the coerced value only if coercion allocated a new handle.
     * String values pass through coercetostring() as a no-op, sharing the
     * same handle as the original — the caller disposes that one. */
    if (coerced_allocated) disposevaluerecord(coerced, false);
}

/* ========================================================================
 * Operation handlers
 * ======================================================================== */

static void handle_script_eval(long id, const char *json_line, transport_t *transport) {
    /* Use cJSON for expression extraction — strstr-based op_json_extract_string
     * could match "expression" inside a string value from untrusted WebSocket input. */
    cJSON *root = cJSON_Parse(json_line);
    if (root == NULL) {
        send_error(id, "Invalid JSON", transport);
        return;
    }
    cJSON *params = cJSON_GetObjectItemCaseSensitive(root, "params");
    cJSON *expr_json = params ? cJSON_GetObjectItemCaseSensitive(params, "expression") : NULL;
    if (!cJSON_IsString(expr_json) || expr_json->valuestring == NULL) {
        send_error(id, "Missing 'expression' in params", transport);
        cJSON_Delete(root);
        return;
    }
    char *expression = strdup(expr_json->valuestring);
    cJSON_Delete(root);
    if (expression == NULL) {
        send_error(id, "Memory allocation failed", transport);
        return;
    }

    tyvaluerecord result;
    bigstring error_msg;

    initvalue(&result, novaluetype);
    setemptystring(error_msg);

    boolean ok = repl_eval_with_variables_value(expression, &result, error_msg);

    if (ok) {
        send_eval_success(id, &result, transport);
    } else {
        char c_error[256];
        long errlen = stringlength(error_msg);
        if (errlen > 0 && errlen < (long)sizeof(c_error)) {
            memcpy(c_error, stringbaseaddress(error_msg), errlen);
            c_error[errlen] = '\0';
        } else if (errlen == 0) {
            snprintf(c_error, sizeof(c_error), "Script evaluation failed");
        } else {
            snprintf(c_error, sizeof(c_error), "Script error (message too long)");
        }
        send_error(id, c_error, transport);
    }

    disposevaluerecord(result, false);
    free(expression);
}

static void handle_clear_context(long id, transport_t *transport) {
    hdlhashtable vars = repl_get_variables_table();
    if (vars != nil) {
        emptyhashtable(vars, true);
    }

    repl_jump_path("");

    /* Reset error state to a known baseline. These are intentional
     * protocol-level resets (not mid-operation mutations), so no context
     * guard is needed — we're establishing a clean state, not restoring one.
     *
     * Note: This resets global error state (langerrordisable, langerrorlogdisable,
     * fllangerror) for the entire process. Over the WebSocket transport, multiple
     * long-lived clients share a single process lifetime, so one client's
     * clearContext affects all subsequent operations. The GIL serializes access
     * so there's no race, but the blast radius is process-wide.
     * Acceptable for a localhost-only tool; would need per-session state for
     * multi-user use. */
    langerrordisable = 0;
    langerrorlogdisable = 0;
    fllangerror = false;

    send_ack(id, transport);
}

/* ========================================================================
 * ODB operation handlers
 * ======================================================================== */

/*
 * Add an error field as {"message":"..."} object to a cJSON item.
 * Ensures per-item errors match the top-level send_error() format.
 */
static void cjson_add_error_object(cJSON *item, const char *message) {
    cJSON *error_obj = cJSON_CreateObject();
    if (error_obj == NULL) return;  /* OOM — item will lack "error" field */
    cJSON_AddStringToObject(error_obj, "message", message);
    cJSON_AddItemToObject(item, "error", error_obj);
}

static void handle_odb_get(long id, const char *json_line, transport_t *transport) {
    cJSON *root = cJSON_Parse(json_line);
    if (root == NULL) {
        send_error(id, "Invalid JSON", transport);
        return;
    }

    cJSON *params = cJSON_GetObjectItemCaseSensitive(root, "params");
    cJSON *items = params ? cJSON_GetObjectItemCaseSensitive(params, "items") : NULL;

    if (!cJSON_IsArray(items)) {
        send_error(id, "Missing or invalid 'items' array in params", transport);
        cJSON_Delete(root);
        return;
    }

    int count = cJSON_GetArraySize(items);
    if (count > OP_MAX_BATCH_SIZE) {
        send_error(id, "Batch too large (max 1000 items)", transport);
        cJSON_Delete(root);
        return;
    }

    cJSON *results = cJSON_CreateArray();

    for (int i = 0; i < count; i++) {
        cJSON *item = cJSON_GetArrayItem(items, i);
        cJSON *path_json = cJSON_GetObjectItemCaseSensitive(item, "path");

        if (!cJSON_IsString(path_json)) {
            cJSON *err = cJSON_CreateObject();
            cjson_add_error_object(err, "Missing 'path'");
            cJSON_AddBoolToObject(err, "success", 0);
            cJSON_AddItemToArray(results, err);
            continue;
        }

        const char *path = path_json->valuestring;
        cJSON *result_item = odb_get_value(path);
        if (result_item == NULL) {
            result_item = cJSON_CreateObject();
            cJSON_AddStringToObject(result_item, "path", path);
            cjson_add_error_object(result_item, "Internal error");
            cJSON_AddBoolToObject(result_item, "success", 0);
        }
        cJSON_AddItemToArray(results, result_item);
    }

    cJSON *response = cJSON_CreateObject();
    cJSON_AddNumberToObject(response, "id", id);
    cJSON_AddBoolToObject(response, "success", 1);
    cJSON_AddItemToObject(response, "results", results);

    char *json_out = cJSON_PrintUnformatted(response);
    if (json_out != NULL) {
        transport_send(transport, json_out);
        cJSON_free(json_out);
    }

    cJSON_Delete(response);
    cJSON_Delete(root);
}

static void handle_odb_set(long id, const char *json_line, transport_t *transport) {
    cJSON *root = cJSON_Parse(json_line);
    if (root == NULL) {
        send_error(id, "Invalid JSON", transport);
        return;
    }

    cJSON *params = cJSON_GetObjectItemCaseSensitive(root, "params");
    cJSON *items = params ? cJSON_GetObjectItemCaseSensitive(params, "items") : NULL;

    if (!cJSON_IsArray(items)) {
        send_error(id, "Missing or invalid 'items' array in params", transport);
        cJSON_Delete(root);
        return;
    }

    int count = cJSON_GetArraySize(items);
    if (count > OP_MAX_BATCH_SIZE) {
        send_error(id, "Batch too large (max 1000 items)", transport);
        cJSON_Delete(root);
        return;
    }

    cJSON *results = cJSON_CreateArray();

    for (int i = 0; i < count; i++) {
        cJSON *item = cJSON_GetArrayItem(items, i);
        cJSON *path_json = cJSON_GetObjectItemCaseSensitive(item, "path");
        cJSON *type_json = cJSON_GetObjectItemCaseSensitive(item, "type");
        cJSON *value_json = cJSON_GetObjectItemCaseSensitive(item, "value");

        if (!cJSON_IsString(path_json)) {
            cJSON *err = cJSON_CreateObject();
            cjson_add_error_object(err, "Missing 'path'");
            cJSON_AddBoolToObject(err, "success", 0);
            cJSON_AddItemToArray(results, err);
            continue;
        }

        const char *path = path_json->valuestring;
        const char *type_str = cJSON_IsString(type_json) ? type_json->valuestring : NULL;

        cJSON *result_item = odb_set_value(path, type_str, value_json);
        if (result_item == NULL) {
            result_item = cJSON_CreateObject();
            cJSON_AddStringToObject(result_item, "path", path);
            cjson_add_error_object(result_item, "Internal error");
            cJSON_AddBoolToObject(result_item, "success", 0);
        }
        cJSON_AddItemToArray(results, result_item);
    }

    cJSON *response = cJSON_CreateObject();
    cJSON_AddNumberToObject(response, "id", id);
    cJSON_AddBoolToObject(response, "success", 1);
    cJSON_AddItemToObject(response, "results", results);

    char *json_out = cJSON_PrintUnformatted(response);
    if (json_out != NULL) {
        transport_send(transport, json_out);
        cJSON_free(json_out);
    }

    cJSON_Delete(response);
    cJSON_Delete(root);
}

static void handle_odb_list(long id, const char *json_line, transport_t *transport) {
    cJSON *root = cJSON_Parse(json_line);
    if (root == NULL) {
        send_error(id, "Invalid JSON", transport);
        return;
    }

    cJSON *params = cJSON_GetObjectItemCaseSensitive(root, "params");
    cJSON *items = params ? cJSON_GetObjectItemCaseSensitive(params, "items") : NULL;

    if (!cJSON_IsArray(items)) {
        send_error(id, "Missing or invalid 'items' array in params", transport);
        cJSON_Delete(root);
        return;
    }

    int count = cJSON_GetArraySize(items);
    if (count > OP_MAX_BATCH_SIZE) {
        send_error(id, "Batch too large (max 1000 items)", transport);
        cJSON_Delete(root);
        return;
    }

    cJSON *results = cJSON_CreateArray();

    for (int i = 0; i < count; i++) {
        cJSON *item = cJSON_GetArrayItem(items, i);
        cJSON *path_json = cJSON_GetObjectItemCaseSensitive(item, "path");
        cJSON *depth_json = cJSON_GetObjectItemCaseSensitive(item, "depth");
        cJSON *max_json = cJSON_GetObjectItemCaseSensitive(item, "maxResults");

        if (!cJSON_IsString(path_json)) {
            cJSON *err = cJSON_CreateObject();
            cjson_add_error_object(err, "Missing 'path'");
            cJSON_AddBoolToObject(err, "success", 0);
            cJSON_AddItemToArray(results, err);
            continue;
        }

        const char *path = path_json->valuestring;
        int depth = cJSON_IsNumber(depth_json) ? (int)depth_json->valuedouble : 1;
        /* Default max_results of 10,000: high enough for power-user localhost
         * exploration, bounded enough to prevent accidental OOM on huge databases.
         * Combined with MAX_RECURSION_DEPTH (32) in list_table_entries(). */
        int max_results = cJSON_IsNumber(max_json) ? (int)max_json->valuedouble : 10000;

        cJSON *result_item = odb_list_children(path, depth, max_results);
        if (result_item == NULL) {
            result_item = cJSON_CreateObject();
            cJSON_AddStringToObject(result_item, "path", path);
            cjson_add_error_object(result_item, "Internal error");
            cJSON_AddBoolToObject(result_item, "success", 0);
        }
        cJSON_AddItemToArray(results, result_item);
    }

    cJSON *response = cJSON_CreateObject();
    cJSON_AddNumberToObject(response, "id", id);
    cJSON_AddBoolToObject(response, "success", 1);
    cJSON_AddItemToObject(response, "results", results);

    char *json_out = cJSON_PrintUnformatted(response);
    if (json_out != NULL) {
        transport_send(transport, json_out);
        cJSON_free(json_out);
    }

    cJSON_Delete(response);
    cJSON_Delete(root);
}

static void handle_odb_delete(long id, const char *json_line, transport_t *transport) {
    cJSON *root = cJSON_Parse(json_line);
    if (root == NULL) {
        send_error(id, "Invalid JSON", transport);
        return;
    }

    cJSON *params = cJSON_GetObjectItemCaseSensitive(root, "params");
    cJSON *items = params ? cJSON_GetObjectItemCaseSensitive(params, "items") : NULL;

    if (!cJSON_IsArray(items)) {
        send_error(id, "Missing or invalid 'items' array in params", transport);
        cJSON_Delete(root);
        return;
    }

    int count = cJSON_GetArraySize(items);
    if (count > OP_MAX_BATCH_SIZE) {
        send_error(id, "Batch too large (max 1000 items)", transport);
        cJSON_Delete(root);
        return;
    }

    cJSON *results = cJSON_CreateArray();

    for (int i = 0; i < count; i++) {
        cJSON *item = cJSON_GetArrayItem(items, i);
        cJSON *path_json = cJSON_GetObjectItemCaseSensitive(item, "path");

        if (!cJSON_IsString(path_json)) {
            cJSON *err = cJSON_CreateObject();
            cjson_add_error_object(err, "Missing 'path'");
            cJSON_AddBoolToObject(err, "success", 0);
            cJSON_AddItemToArray(results, err);
            continue;
        }

        const char *path = path_json->valuestring;
        cJSON *result_item = odb_delete_value(path);
        if (result_item == NULL) {
            result_item = cJSON_CreateObject();
            cJSON_AddStringToObject(result_item, "path", path);
            cjson_add_error_object(result_item, "Internal error");
            cJSON_AddBoolToObject(result_item, "success", 0);
        }
        cJSON_AddItemToArray(results, result_item);
    }

    cJSON *response = cJSON_CreateObject();
    cJSON_AddNumberToObject(response, "id", id);
    cJSON_AddBoolToObject(response, "success", 1);
    cJSON_AddItemToObject(response, "results", results);

    char *json_out = cJSON_PrintUnformatted(response);
    if (json_out != NULL) {
        transport_send(transport, json_out);
        cJSON_free(json_out);
    }

    cJSON_Delete(response);
    cJSON_Delete(root);
}

/* ========================================================================
 * Main dispatch
 * ======================================================================== */

int op_dispatch(const char *json_line, size_t len, transport_t *transport) {
    (void)len;

    /* GIL INVARIANT: This function must be called with the GIL held.
     * All ODB and script operations access Frontier runtime globals
     * (hash tables, lang APIs) that require serialized access (ADR-014).
     * No runtime assertion is available since the GIL is a simple mutex
     * without an "is-held-by-current-thread" query API. */

    /* Rate limiting: Not implemented in this PR. Per-connection request
     * counting or wall-clock timeouts for expensive operations (odb/list
     * depth:-1, script/eval) are tracked as a future enhancement.
     * The server currently relies on localhost-only binding and the
     * WS_MAX_CLIENTS cap (8) to limit exposure. */

    /* Parse envelope fields (op, id) using cJSON for safety.
     * WebSocket clients can send crafted JSON where strstr-based
     * extraction would match keys inside string values. */
    cJSON *envelope = cJSON_Parse(json_line);
    if (envelope == NULL) {
        return 0;
    }

    cJSON *op_json = cJSON_GetObjectItemCaseSensitive(envelope, "op");
    cJSON *id_json = cJSON_GetObjectItemCaseSensitive(envelope, "id");

    char *op = NULL;
    if (cJSON_IsString(op_json) && op_json->valuestring != NULL) {
        op = strdup(op_json->valuestring);
    }
    bool id_present = cJSON_IsNumber(id_json);
    long id = id_present ? (long)id_json->valuedouble : 0;

    cJSON_Delete(envelope);

    /* Memory ownership: `op` is strdup'd and freed at the end of this function.
     * The shutdown handler frees op before its early return. All other paths
     * fall through to the free(op) at function end. No leak on any path. */

    if (op == NULL) {
        send_error(id, "Missing 'op' field", transport);
        return 0;
    }

    if (strcmp(op, "script/eval") == 0) {
        handle_script_eval(id, json_line, transport);
    } else if (strcmp(op, "script/clearContext") == 0) {
        handle_clear_context(id, transport);
    } else if (strcmp(op, "odb/get") == 0) {
        handle_odb_get(id, json_line, transport);
    } else if (strcmp(op, "odb/set") == 0) {
        handle_odb_set(id, json_line, transport);
    } else if (strcmp(op, "odb/list") == 0) {
        handle_odb_list(id, json_line, transport);
    } else if (strcmp(op, "odb/delete") == 0) {
        handle_odb_delete(id, json_line, transport);
    } else if (strcmp(op, "shutdown") == 0) {
        send_ack(id, transport);
        free(op);
        return 1;  /* signal shutdown */
    } else {
        char err[256];
        snprintf(err, sizeof(err), "Unknown operation: %s", op);
        send_error(id, err, transport);
    }

    free(op);
    return 0;
}
