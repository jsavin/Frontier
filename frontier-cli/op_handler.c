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
 * JSON parsing for the envelope (op, id) uses the simple string-scanning
 * helpers already proven in protocol_handler.c. ODB operations that need
 * array parsing use the vendored cJSON library.
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

#define OP_MAX_STRING 8192

/* ========================================================================
 * Minimal JSON field extraction (same as protocol_handler.c originals)
 *
 * These helpers extract string and integer values from a known-schema JSON
 * line. They are NOT general-purpose JSON parsers — they rely on the input
 * being machine-generated with predictable formatting.
 * ======================================================================== */

/*
 * Extract a JSON string value for a given key from a JSON line.
 * Handles JSON escape sequences in the value (\n, \t, \\, \", \uXXXX).
 * Returns a malloc'd C string (caller must free), or NULL if key not found.
 */
char *op_json_extract_string(const char *json, const char *key) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *pos = strstr(json, pattern);
    if (pos == NULL) {
        return NULL;
    }

    pos += strlen(pattern);

    while (*pos == ' ' || *pos == ':') {
        pos++;
    }

    if (*pos != '"') {
        return NULL;
    }
    pos++;

    size_t capacity = 256;
    size_t len = 0;
    char *result = malloc(capacity);
    if (result == NULL) {
        return NULL;
    }

    while (*pos != '\0' && *pos != '"') {
        char c;

        if (*pos == '\\') {
            pos++;
            switch (*pos) {
                case '"':  c = '"';  break;
                case '\\': c = '\\'; break;
                case '/':  c = '/';  break;
                case 'b':  c = '\b'; break;
                case 'f':  c = '\f'; break;
                case 'n':  c = '\n'; break;
                case 'r':  c = '\r'; break;
                case 't':  c = '\t'; break;
                case 'u': {
                    if (pos[1] && pos[2] && pos[3] && pos[4]) {
                        char hex[5] = { pos[1], pos[2], pos[3], pos[4], '\0' };
                        unsigned int codepoint = (unsigned int)strtoul(hex, NULL, 16);
                        pos += 4;

                        if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                            if (pos[1] == '\\' && pos[2] == 'u') {
                                char hex2[5] = { pos[3], pos[4], pos[5], pos[6], '\0' };
                                unsigned int low = (unsigned int)strtoul(hex2, NULL, 16);
                                if (low >= 0xDC00 && low <= 0xDFFF) {
                                    codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
                                    pos += 6;
                                }
                            }
                        }

                        size_t need = (codepoint < 0x80) ? 1 : (codepoint < 0x800) ? 2 : (codepoint < 0x10000) ? 3 : 4;
                        if (len + need >= capacity) {
                            capacity = (capacity + need) * 2;
                            char *tmp = realloc(result, capacity);
                            if (tmp == NULL) { free(result); return NULL; }
                            result = tmp;
                        }
                        if (codepoint < 0x80) {
                            c = (char)codepoint;
                        } else if (codepoint < 0x800) {
                            if (len + 2 >= OP_MAX_STRING) { free(result); return NULL; }
                            result[len++] = (char)(0xC0 | (codepoint >> 6));
                            result[len++] = (char)(0x80 | (codepoint & 0x3F));
                            pos++;
                            continue;
                        } else if (codepoint < 0x10000) {
                            if (len + 3 >= OP_MAX_STRING) { free(result); return NULL; }
                            result[len++] = (char)(0xE0 | (codepoint >> 12));
                            result[len++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
                            result[len++] = (char)(0x80 | (codepoint & 0x3F));
                            pos++;
                            continue;
                        } else {
                            if (len + 4 >= OP_MAX_STRING) { free(result); return NULL; }
                            result[len++] = (char)(0xF0 | (codepoint >> 18));
                            result[len++] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
                            result[len++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
                            result[len++] = (char)(0x80 | (codepoint & 0x3F));
                            pos++;
                            continue;
                        }
                    } else {
                        c = '?';
                    }
                    break;
                }
                default:
                    c = *pos;
                    break;
            }
        } else {
            c = *pos;
        }

        if (len + 1 >= OP_MAX_STRING) {
            free(result);
            return NULL;
        }
        if (len + 1 >= capacity) {
            capacity *= 2;
            char *tmp = realloc(result, capacity);
            if (tmp == NULL) { free(result); return NULL; }
            result = tmp;
        }
        result[len++] = c;
        pos++;
    }

    result[len] = '\0';
    return result;
}

/*
 * Extract a JSON integer value for a given key.
 * Returns the integer value, or -1 if not found.
 */
long op_json_extract_int(const char *json, const char *key) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *pos = strstr(json, pattern);
    if (pos == NULL) {
        return -1;
    }

    pos += strlen(pattern);

    while (*pos == ' ' || *pos == ':') {
        pos++;
    }

    char *end;
    long val = strtol(pos, &end, 10);
    if (end == pos) {
        return -1;
    }

    return val;
}

/* ========================================================================
 * Value type name for JSON response
 * ======================================================================== */

static const char *valuetype_name(tyvaluetype t) {
    switch (t) {
        case novaluetype:       return "none";
        case charvaluetype:     return "char";
        case intvaluetype:      return "int";
        case longvaluetype:     return "long";
        case booleanvaluetype:  return "boolean";
        case stringvaluetype:   return "string";
        case doublevaluetype:   return "double";
        case datevaluetype:     return "date";
        case addressvaluetype:  return "address";
        case directionvaluetype:return "direction";
        case externalvaluetype: return "external";
        case listvaluetype:     return "list";
        case recordvaluetype:   return "record";
        case binaryvaluetype:   return "binary";
        default:                return "unknown";
    }
}

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
     * We use a FILE* buffer via open_memstream for JSON escaping. */
    char *buf = NULL;
    size_t buf_len = 0;
    FILE *f = open_memstream(&buf, &buf_len);
    if (f == NULL) {
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
    const char *type_name = valuetype_name(val->valuetype);

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
}

/* ========================================================================
 * Operation handlers
 * ======================================================================== */

static void handle_script_eval(long id, const char *json_line, transport_t *transport) {
    char *expression = op_json_extract_string(json_line, "expression");
    if (expression == NULL) {
        send_error(id, "Missing 'expression' in params", transport);
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

    langerrordisable = 0;
    langerrorlogdisable = 0;
    fllangerror = false;

    send_ack(id, transport);
}

/* ========================================================================
 * ODB operation handlers
 * ======================================================================== */

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

    cJSON *results = cJSON_CreateArray();
    int count = cJSON_GetArraySize(items);

    for (int i = 0; i < count; i++) {
        cJSON *item = cJSON_GetArrayItem(items, i);
        cJSON *path_json = cJSON_GetObjectItemCaseSensitive(item, "path");

        if (!cJSON_IsString(path_json)) {
            cJSON *err = cJSON_CreateObject();
            cJSON_AddStringToObject(err, "error", "Missing 'path'");
            cJSON_AddBoolToObject(err, "success", 0);
            cJSON_AddItemToArray(results, err);
            continue;
        }

        const char *path = path_json->valuestring;
        cJSON *result_item = odb_get_value(path);
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

    cJSON *results = cJSON_CreateArray();
    int count = cJSON_GetArraySize(items);

    for (int i = 0; i < count; i++) {
        cJSON *item = cJSON_GetArrayItem(items, i);
        cJSON *path_json = cJSON_GetObjectItemCaseSensitive(item, "path");
        cJSON *type_json = cJSON_GetObjectItemCaseSensitive(item, "type");
        cJSON *value_json = cJSON_GetObjectItemCaseSensitive(item, "value");

        if (!cJSON_IsString(path_json)) {
            cJSON *err = cJSON_CreateObject();
            cJSON_AddStringToObject(err, "error", "Missing 'path'");
            cJSON_AddBoolToObject(err, "success", 0);
            cJSON_AddItemToArray(results, err);
            continue;
        }

        const char *path = path_json->valuestring;
        const char *type_str = cJSON_IsString(type_json) ? type_json->valuestring : NULL;

        cJSON *result_item = odb_set_value(path, type_str, value_json);
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

    cJSON *results = cJSON_CreateArray();
    int count = cJSON_GetArraySize(items);

    for (int i = 0; i < count; i++) {
        cJSON *item = cJSON_GetArrayItem(items, i);
        cJSON *path_json = cJSON_GetObjectItemCaseSensitive(item, "path");
        cJSON *depth_json = cJSON_GetObjectItemCaseSensitive(item, "depth");
        cJSON *max_json = cJSON_GetObjectItemCaseSensitive(item, "maxResults");

        if (!cJSON_IsString(path_json)) {
            cJSON *err = cJSON_CreateObject();
            cJSON_AddStringToObject(err, "error", "Missing 'path'");
            cJSON_AddBoolToObject(err, "success", 0);
            cJSON_AddItemToArray(results, err);
            continue;
        }

        const char *path = path_json->valuestring;
        int depth = cJSON_IsNumber(depth_json) ? (int)depth_json->valuedouble : 1;
        int max_results = cJSON_IsNumber(max_json) ? (int)max_json->valuedouble : 10000;

        cJSON *result_item = odb_list_children(path, depth, max_results);
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

    cJSON *results = cJSON_CreateArray();
    int count = cJSON_GetArraySize(items);

    for (int i = 0; i < count; i++) {
        cJSON *item = cJSON_GetArrayItem(items, i);
        cJSON *path_json = cJSON_GetObjectItemCaseSensitive(item, "path");

        if (!cJSON_IsString(path_json)) {
            cJSON *err = cJSON_CreateObject();
            cJSON_AddStringToObject(err, "error", "Missing 'path'");
            cJSON_AddBoolToObject(err, "success", 0);
            cJSON_AddItemToArray(results, err);
            continue;
        }

        const char *path = path_json->valuestring;
        cJSON *result_item = odb_delete_value(path);
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

    char *op = op_json_extract_string(json_line, "op");
    long id = op_json_extract_int(json_line, "id");

    if (op == NULL) {
        if (id >= 0) {
            send_error(id, "Missing 'op' field", transport);
        }
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
        if (id >= 0) {
            char err[256];
            snprintf(err, sizeof(err), "Unknown operation: %s", op);
            send_error(id, err, transport);
        }
    }

    free(op);
    return 0;
}
