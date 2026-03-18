/*
 * Frontier CLI - ODB Operations
 *
 * odb_ops.c - ODB path resolution, value serialization, and CRUD operations
 *
 * Implements odb/get, odb/set, odb/list, odb/delete operations using the
 * Frontier runtime's hash table and lang APIs.
 *
 * Path resolution uses langexpandtodotparams() which compiles and evaluates
 * a dotted path string to locate the parent table and leaf name. This is
 * the same mechanism UserTalk uses for address resolution.
 *
 * Copyright (C) 1992-2004 UserLand Software, Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "odb_ops.h"

#include "../Common/headers/frontier.h"
#include "../Common/headers/lang.h"
#include "../Common/headers/langexternal.h"
#include "../Common/headers/langinternal.h"
#include "../Common/headers/memory.h"
#include "../Common/headers/logging.h"
#include "../Common/headers/strings.h"
#include "../Common/headers/tablestructure.h"
#include "../Common/headers/op.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#define MAX_RECURSION_DEPTH 32

extern hdlhashtable roottable;

/* ========================================================================
 * Minimal base64 encoder for binary value serialization
 * ======================================================================== */

static const char odb_b64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void odb_base64_encode(const uint8_t *in, size_t in_len,
                               char *out, size_t out_size) {
    size_t i = 0, j = 0;

    while (i < in_len && j + 4 < out_size) {
        uint32_t a = in[i++];
        int bytes_in_triple = 1;
        uint32_t b = 0, c = 0;
        if (i < in_len) { b = in[i++]; bytes_in_triple++; }
        if (i < in_len) { c = in[i++]; bytes_in_triple++; }
        uint32_t triple = (a << 16) | (b << 8) | c;

        out[j++] = odb_b64_table[(triple >> 18) & 0x3F];
        out[j++] = odb_b64_table[(triple >> 12) & 0x3F];
        out[j++] = (bytes_in_triple < 2) ? '=' : odb_b64_table[(triple >> 6) & 0x3F];
        out[j++] = (bytes_in_triple < 3) ? '=' : odb_b64_table[triple & 0x3F];
    }
    out[j] = '\0';
}

/* ========================================================================
 * Internal helpers
 * ======================================================================== */

/*
 * Resolve a dotted path (e.g. "workspace.scratchpad.greeting") to
 * the parent hashtable and leaf name.
 *
 * Returns true if resolution succeeded. For bare names (no dot),
 * searches the root table as parent.
 */
static boolean resolve_path(const char *path, hdlhashtable *htable, bigstring bsname) {

    bigstring bspath;

    size_t pathlen = strlen(path);
    if (pathlen > 255) {
        return false;
    }

    /* Validate path characters — only allow alphanumeric, dots, and underscores.
     * langexpandtodotparams() compiles and evaluates the path as UserTalk, so
     * unsanitized input could execute arbitrary expressions. */
    for (size_t i = 0; i < pathlen; i++) {
        char ch = path[i];
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) && ch != '.' && ch != '_') {
            return false;
        }
    }

    /* Reject paths with consecutive dots, leading dot, or trailing dot */
    if (pathlen > 0 && (path[0] == '.' || path[pathlen - 1] == '.')) {
        return false;
    }
    for (size_t i = 0; i + 1 < pathlen; i++) {
        if (path[i] == '.' && path[i + 1] == '.') {
            return false;
        }
    }

    setstringlength(bspath, (short)pathlen);
    memcpy(stringbaseaddress(bspath), path, pathlen);

    boolean ok = langexpandtodotparams(bspath, htable, bsname);

    /* For bare names like "workspace" (no dot), langexpandtodotparams
     * returns htable == nil. In this case, search the root table. */
    if (ok && *htable == nil) {
        hdlhashtable hspecial;
        if (langgetspecialtable(bsname, &hspecial)) {
            *htable = hspecial;
        } else {
            *htable = roottable;
        }
    }

    return ok;
}

/*
 * Get the friendly type name for an external value (table, outline, script, etc.)
 */
static const char *external_type_name(tyvaluerecord *val) {

    if (val->valuetype != externalvaluetype) {
        return "unknown";
    }

    hdlexternalvariable hv = (hdlexternalvariable)val->data.externalvalue;

    if (hv == nil) {
        return "external";
    }

    switch ((**hv).id) {
        case idtableprocessor:    return "table";
        case idoutlineprocessor:  return "outline";
        case idscriptprocessor:   return "script";
        case idwordprocessor:     return "wptext";
        case idmenuprocessor:     return "menu";
        case idpictprocessor:     return "picture";
        default:                  return "external";
    }
}

/*
 * Get the simple type name string for a value type.
 */
const char *type_name_str(tyvaluetype t) {

    switch (t) {
        case novaluetype:        return "none";
        case charvaluetype:      return "char";
        case intvaluetype:       return "int";
        case longvaluetype:      return "long";
        case booleanvaluetype:   return "boolean";
        case stringvaluetype:    return "string";
        case doublevaluetype:    return "double";
        case datevaluetype:      return "date";
        case addressvaluetype:   return "address";
        case directionvaluetype: return "direction";
        case externalvaluetype:  return "external";  /* overridden by external_type_name */
        case listvaluetype:      return "list";
        case recordvaluetype:    return "record";
        case binaryvaluetype:    return "binary";
        default:                 return "unknown";
    }
}

/*
 * Convert a Frontier tyvaluerecord to a cJSON value for get responses.
 * Returns the type name via out_type.
 */
static cJSON *value_to_json(tyvaluerecord *val, const char **out_type) {

    switch (val->valuetype) {
        case novaluetype:
            *out_type = "none";
            return cJSON_CreateNull();

        case booleanvaluetype:
            *out_type = "boolean";
            return cJSON_CreateBool(val->data.flvalue ? 1 : 0);

        case charvaluetype:
            *out_type = "char";
            {
                char buf[2] = { (char)val->data.chvalue, '\0' };
                return cJSON_CreateString(buf);
            }

        case intvaluetype:
            *out_type = "int";
            return cJSON_CreateNumber(val->data.intvalue);

        case longvaluetype:
            *out_type = "long";
            return cJSON_CreateNumber(val->data.longvalue);

        case doublevaluetype:
            *out_type = "double";
            {
                double d = (val->data.doublevalue != nil) ? **(val->data.doublevalue) : 0.0;
                return cJSON_CreateNumber(d);
            }

        case datevaluetype:
            *out_type = "date";
            return cJSON_CreateNumber(val->data.longvalue);

        case directionvaluetype:
            *out_type = "direction";
            return cJSON_CreateNumber((int)val->data.dirvalue);

        case stringvaluetype:
            *out_type = "string";
            {
                Handle h = val->data.stringvalue;
                if (h == nil) {
                    return cJSON_CreateString("");
                }
                long len = gethandlesize(h);
                if (len < 0) return cJSON_CreateString("");
                /* Create null-terminated copy for cJSON */
                char *buf = malloc(len + 1);
                if (buf == NULL) {
                    return cJSON_CreateString("");
                }
                memcpy(buf, *h, len);
                buf[len] = '\0';
                cJSON *s = cJSON_CreateString(buf);
                free(buf);
                return s;
            }

        case addressvaluetype:
            *out_type = "address";
            {
                /* Coerce to string representation */
                tyvaluerecord coerced = *val;
                if (coercetostring(&coerced)) {
                    Handle h = coerced.data.stringvalue;
                    long len = gethandlesize(h);
                    if (len < 0) { disposevaluerecord(coerced, false); return cJSON_CreateString(""); }
                    char *buf = malloc(len + 1);
                    if (buf != NULL) {
                        memcpy(buf, *h, len);
                        buf[len] = '\0';
                        cJSON *s = cJSON_CreateString(buf);
                        free(buf);
                        disposevaluerecord(coerced, false);
                        return s;
                    }
                    disposevaluerecord(coerced, false);
                }
                return cJSON_CreateString("");
            }

        case externalvaluetype:
            {
                const char *ext_type = external_type_name(val);
                *out_type = ext_type;

                hdlexternalvariable hv = (hdlexternalvariable)val->data.externalvalue;
                if (hv == nil) {
                    return cJSON_CreateNull();
                }

                if ((**hv).id == idtableprocessor) {
                    /* For tables, return child count */
                    hdlhashtable htable;
                    if (langexternalvaltotable(*val, &htable, nil)) {
                        long count = 0;
                        hashcountitems(htable, &count);
                        cJSON *obj = cJSON_CreateObject();
                        cJSON_AddNumberToObject(obj, "childCount", count);
                        return obj;
                    }
                    return cJSON_CreateNull();
                }

                if ((**hv).id == idscriptprocessor) {
                    /* For scripts, coerce to string to get source */
                    tyvaluerecord coerced = *val;
                    if (coercetostring(&coerced)) {
                        Handle h = coerced.data.stringvalue;
                        long len = gethandlesize(h);
                        if (len < 0) { disposevaluerecord(coerced, false); return cJSON_CreateNull(); }
                        char *buf = malloc(len + 1);
                        if (buf != NULL) {
                            memcpy(buf, *h, len);
                            buf[len] = '\0';
                            cJSON *obj = cJSON_CreateObject();
                            cJSON_AddStringToObject(obj, "source", buf);
                            free(buf);
                            disposevaluerecord(coerced, false);
                            return obj;
                        }
                        disposevaluerecord(coerced, false);
                    }
                    return cJSON_CreateNull();
                }

                /* Other external types: coerce to string */
                tyvaluerecord coerced = *val;
                if (coercetostring(&coerced)) {
                    Handle h = coerced.data.stringvalue;
                    long len = gethandlesize(h);
                    if (len < 0) { disposevaluerecord(coerced, false); return cJSON_CreateNull(); }
                    char *buf = malloc(len + 1);
                    if (buf != NULL) {
                        memcpy(buf, *h, len);
                        buf[len] = '\0';
                        cJSON *s = cJSON_CreateString(buf);
                        free(buf);
                        disposevaluerecord(coerced, false);
                        return s;
                    }
                    disposevaluerecord(coerced, false);
                }
                return cJSON_CreateNull();
            }

        case binaryvaluetype:
            *out_type = "binary";
            {
                Handle h = val->data.binaryvalue;
                if (h == nil || gethandlesize(h) <= 0) {
                    return cJSON_CreateString("");
                }
                long bin_len = gethandlesize(h);
                /* base64 output: 4 chars per 3 input bytes, rounded up, plus NUL */
                size_t b64_len = (((size_t)bin_len + 2) / 3) * 4 + 1;
                char *b64_buf = malloc(b64_len);
                if (b64_buf == NULL) {
                    return cJSON_CreateString("");
                }
                odb_base64_encode((const uint8_t *)*h, (size_t)bin_len, b64_buf, b64_len);
                cJSON *s = cJSON_CreateString(b64_buf);
                free(b64_buf);
                return s;
            }

        case listvaluetype:
        case recordvaluetype:
            *out_type = type_name_str(val->valuetype);
            {
                tyvaluerecord coerced = *val;
                if (coercetostring(&coerced)) {
                    Handle h = coerced.data.stringvalue;
                    long len = gethandlesize(h);
                    if (len < 0) { disposevaluerecord(coerced, false); return cJSON_CreateNull(); }
                    char *buf = malloc(len + 1);
                    if (buf != NULL) {
                        memcpy(buf, *h, len);
                        buf[len] = '\0';
                        cJSON *s = cJSON_CreateString(buf);
                        free(buf);
                        disposevaluerecord(coerced, false);
                        return s;
                    }
                    disposevaluerecord(coerced, false);
                }
                return cJSON_CreateNull();
            }

        default:
            *out_type = "unknown";
            return cJSON_CreateNull();
    }
}

/*
 * Create a per-item error result.
 */
static cJSON *make_error_result(const char *path, const char *error_msg) {

    cJSON *obj = cJSON_CreateObject();
    if (obj == NULL) return NULL;

    if (path != NULL) {
        cJSON_AddStringToObject(obj, "path", path);
    }
    cJSON *error_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(error_obj, "message", error_msg);
    cJSON_AddItemToObject(obj, "error", error_obj);
    cJSON_AddBoolToObject(obj, "success", 0);
    return obj;
}

/*
 * Parse a value type name to a tyvaluetype enum.
 */
static tyvaluetype parse_value_type(const char *type_str) {

    if (type_str == NULL) return novaluetype;
    if (strcmp(type_str, "string") == 0)    return stringvaluetype;
    if (strcmp(type_str, "long") == 0)      return longvaluetype;
    if (strcmp(type_str, "int") == 0)       return intvaluetype;
    if (strcmp(type_str, "boolean") == 0)   return booleanvaluetype;
    if (strcmp(type_str, "bool") == 0)      return booleanvaluetype;
    if (strcmp(type_str, "double") == 0)    return doublevaluetype;
    if (strcmp(type_str, "float") == 0)     return doublevaluetype;
    if (strcmp(type_str, "char") == 0)      return charvaluetype;
    if (strcmp(type_str, "date") == 0)      return datevaluetype;
    if (strcmp(type_str, "direction") == 0) return directionvaluetype;
    if (strcmp(type_str, "address") == 0)   return addressvaluetype;
    if (strcmp(type_str, "binary") == 0)    return binaryvaluetype;
    return novaluetype;
}

/*
 * List entries of a hash table, adding them to the entries array.
 * path_prefix is the dotted path prefix for child paths.
 * Returns the count of entries added.
 */
static int list_table_entries(hdlhashtable htable, const char *path_prefix,
                              int depth, int max_results, int *count_ptr,
                              cJSON *entries, int recursion_level,
                              bool *truncated) {

    if (recursion_level > MAX_RECURSION_DEPTH) {
        return *count_ptr;  /* Stop recursing to prevent stack overflow */
    }

    hdlhashnode hnode = (**htable).hfirstsort;

    while (hnode != nil && *count_ptr < max_results) {
        bigstring bsname;
        gethashkey(hnode, bsname);

        /* Convert pascal string to C string */
        char name[256];
        long namelen = stringlength(bsname);
        if (namelen > 255) namelen = 255;
        memcpy(name, stringbaseaddress(bsname), namelen);
        name[namelen] = '\0';

        /* Build full path — skip entry if path would be truncated */
        char child_path[1024];
        size_t needed = strlen(path_prefix) + 1 + strlen(name) + 1;
        if (needed > sizeof(child_path)) {
            if (truncated != NULL) *truncated = true;
            hnode = (**hnode).sortedlink;
            continue;  /* Skip entry rather than produce a truncated path */
        }
        snprintf(child_path, sizeof(child_path), "%s.%s", path_prefix, name);

        /* Get type info */
        tyvaluerecord val = (**hnode).val;
        const char *type;
        if (val.valuetype == externalvaluetype) {
            type = external_type_name(&val);
        } else {
            type = type_name_str(val.valuetype);
        }

        cJSON *entry = cJSON_CreateObject();
        cJSON_AddStringToObject(entry, "name", name);
        cJSON_AddStringToObject(entry, "path", child_path);
        cJSON_AddStringToObject(entry, "type", type);
        cJSON_AddItemToArray(entries, entry);
        (*count_ptr)++;

        /* Recurse into tables if depth allows */
        if ((depth > 1 || depth == -1) && val.valuetype == externalvaluetype) {
            hdlhashtable hsubtable;
            if (langexternalvaltotable(val, &hsubtable, nil)) {
                int sub_depth = (depth == -1) ? -1 : depth - 1;
                list_table_entries(hsubtable, child_path, sub_depth,
                                   max_results, count_ptr, entries,
                                   recursion_level + 1, truncated);
            }
        }

        hnode = (**hnode).sortedlink;
    }

    return *count_ptr;
}

/* ========================================================================
 * Public API
 * ======================================================================== */

cJSON *odb_get_value(const char *path) {

    hdlhashtable htable;
    bigstring bsname;

    if (!resolve_path(path, &htable, bsname)) {
        return make_error_result(path, "Path not found");
    }

    if (htable == nil) {
        return make_error_result(path, "Could not resolve parent table");
    }

    tyvaluerecord val;
    hdlhashnode hnode;

    if (!hashtablelookup(htable, bsname, &val, &hnode)) {
        return make_error_result(path, "Value not found");
    }

    const char *type;
    cJSON *json_val = value_to_json(&val, &type);

    /* Build the leaf name as a C string */
    char name[256];
    long namelen = stringlength(bsname);
    if (namelen > 255) namelen = 255;
    memcpy(name, stringbaseaddress(bsname), namelen);
    name[namelen] = '\0';

    cJSON *result = cJSON_CreateObject();
    if (result == NULL) return NULL;
    cJSON_AddStringToObject(result, "path", path);
    cJSON_AddStringToObject(result, "name", name);
    cJSON_AddStringToObject(result, "type", type);
    cJSON_AddItemToObject(result, "value", json_val);
    cJSON_AddBoolToObject(result, "success", 1);

    return result;
}

cJSON *odb_set_value(const char *path, const char *type_str, const cJSON *value_json) {

    /* When value_json is NULL (value field omitted from request):
     * - string: defaults to empty string ""
     * - long/int: defaults to 0
     * - boolean: defaults to false
     * - double: defaults to 0.0
     * This matches C zero-initialization semantics for tyvaluerecord. */

    hdlhashtable htable;
    bigstring bsname;

    if (!resolve_path(path, &htable, bsname)) {
        return make_error_result(path, "Could not resolve path (parent table must exist)");
    }

    if (htable == nil) {
        return make_error_result(path, "Could not resolve parent table");
    }

    /* Determine type */
    tyvaluetype vtype = parse_value_type(type_str);
    if (vtype == novaluetype && type_str != NULL) {
        /* Check for table type — can't create tables via odb/set */
        if (strcmp(type_str, "table") == 0) {
            return make_error_result(path, "Cannot create tables via odb/set (use script/eval with new(tabletype, @path))");
        }
        char err[128];
        snprintf(err, sizeof(err), "Unknown type: %s", type_str);
        return make_error_result(path, err);
    }

    /* Build the value */
    tyvaluerecord val;
    initvalue(&val, novaluetype);

    switch (vtype) {
        case stringvaluetype: {
            const char *s = cJSON_IsString(value_json) ? value_json->valuestring : "";
            Handle h;
            if (!newfilledhandle((void *)s, strlen(s), &h)) {
                return make_error_result(path, "Memory allocation failed");
            }
            val.valuetype = stringvaluetype;
            val.data.stringvalue = h;
            break;
        }

        case longvaluetype:
            val.valuetype = longvaluetype;
            val.data.longvalue = cJSON_IsNumber(value_json) ? (long)value_json->valuedouble : 0;
            break;

        case intvaluetype:
            val.valuetype = intvaluetype;
            val.data.intvalue = cJSON_IsNumber(value_json) ? (short)value_json->valuedouble : 0;
            break;

        case booleanvaluetype:
            val.valuetype = booleanvaluetype;
            if (cJSON_IsBool(value_json)) {
                val.data.flvalue = cJSON_IsTrue(value_json) ? true : false;
            } else if (cJSON_IsNumber(value_json)) {
                val.data.flvalue = (value_json->valuedouble != 0.0) ? true : false;
            } else {
                val.data.flvalue = false;
            }
            break;

        case doublevaluetype: {
            double d = cJSON_IsNumber(value_json) ? value_json->valuedouble : 0.0;
            if (!setdoublevalue(d, &val)) {
                return make_error_result(path, "Memory allocation failed");
            }
            break;
        }

        case charvaluetype:
            val.valuetype = charvaluetype;
            val.data.chvalue = (cJSON_IsString(value_json) && value_json->valuestring[0]) ?
                               value_json->valuestring[0] : '\0';
            break;

        case datevaluetype:
            val.valuetype = datevaluetype;
            val.data.longvalue = cJSON_IsNumber(value_json) ? (long)value_json->valuedouble : 0;
            break;

        case addressvaluetype: {
            const char *s = cJSON_IsString(value_json) ? value_json->valuestring : "";
            Handle h;
            if (!newfilledhandle((void *)s, strlen(s), &h)) {
                return make_error_result(path, "Memory allocation failed");
            }
            val.valuetype = addressvaluetype;
            val.data.stringvalue = h;
            break;
        }

        default:
            if (type_str == NULL) {
                /* Infer type from JSON value.
                 * Each branch creates at most one heap value in `val`.
                 * exemptfromtmpstack() after hashassign() covers it. */
                if (cJSON_IsString(value_json)) {
                    const char *s = value_json->valuestring;
                    Handle h;
                    if (!newfilledhandle((void *)s, strlen(s), &h)) {
                        return make_error_result(path, "Memory allocation failed");
                    }
                    val.valuetype = stringvaluetype;
                    val.data.stringvalue = h;
                } else if (cJSON_IsNumber(value_json)) {
                    double d = value_json->valuedouble;
                    if (d == floor(d) && d >= -2147483648.0 && d <= 2147483647.0) {
                        val.valuetype = longvaluetype;
                        val.data.longvalue = (long)d;
                    } else {
                        if (!setdoublevalue(d, &val)) {
                            return make_error_result(path, "Memory allocation failed");
                        }
                    }
                } else if (cJSON_IsBool(value_json)) {
                    val.valuetype = booleanvaluetype;
                    val.data.flvalue = cJSON_IsTrue(value_json) ? true : false;
                } else if (cJSON_IsNull(value_json)) {
                    return make_error_result(path, "Cannot set null value; specify a type");
                } else {
                    return make_error_result(path, "Cannot infer type from value; specify 'type'");
                }
            } else {
                return make_error_result(path, "Unsupported type for odb/set");
            }
            break;
    }

    /* Assign the value using pushhashtable/hashassign */
    pushhashtable(htable);
    boolean ok = hashassign(bsname, val);
    pophashtable();

    if (!ok) {
        return make_error_result(path, "Failed to assign value");
    }

    /* Exempt heap-allocated values from the tmp stack so the GC doesn't
     * collect the handle while it's stored in the persistent hash table.
     * See docs/ARCHITECTURAL_ANTIPATTERNS.md (Tmp Stack Ownership). */
    exemptfromtmpstack(&val);

    cJSON *result = cJSON_CreateObject();
    if (result == NULL) return NULL;
    cJSON_AddStringToObject(result, "path", path);
    cJSON_AddBoolToObject(result, "success", 1);
    return result;
}

cJSON *odb_list_children(const char *path, int depth, int max_results) {

    /* Validate parameters — reject negative max_results and out-of-range depth.
     * depth == -1 is valid (full recursive), but other negatives are not. */
    if (max_results < 0) {
        max_results = 10000;  /* default */
    }
    if (depth < -1) {
        return make_error_result(path, "Invalid depth (use -1 for recursive, 0+ for limited)");
    }

    /* depth 0 = just confirm it exists */
    if (depth == 0) {
        hdlhashtable htable;
        bigstring bsname;
        if (!resolve_path(path, &htable, bsname)) {
            return make_error_result(path, "Path not found");
        }

        /* Verify the named entry actually exists in the parent table.
         * resolve_path() only confirms the parent table exists; without this
         * check, a path like "workspace.does_not_exist" would return success. */
        hdlhashnode hnode;
        if (!hashtablelookupnode(htable, bsname, &hnode)) {
            return make_error_result(path, "Path not found");
        }

        cJSON *result = cJSON_CreateObject();
        if (result == NULL) return NULL;
        cJSON_AddStringToObject(result, "path", path);
        cJSON_AddBoolToObject(result, "success", 1);
        return result;
    }

    /* Resolve the path to a table */
    hdlhashtable htable;
    bigstring bsname;

    if (!resolve_path(path, &htable, bsname)) {
        return make_error_result(path, "Path not found");
    }

    if (htable == nil) {
        return make_error_result(path, "Could not resolve parent table");
    }

    /* Look up the node and check if it's a table */
    tyvaluerecord val;
    hdlhashnode hnode;

    if (!hashtablelookup(htable, bsname, &val, &hnode)) {
        return make_error_result(path, "Value not found");
    }

    if (val.valuetype != externalvaluetype) {
        return make_error_result(path, "Not a table");
    }

    hdlhashtable target_table;
    if (!langexternalvaltotable(val, &target_table, nil)) {
        return make_error_result(path, "Could not resolve as table");
    }

    /* List children */
    cJSON *entries = cJSON_CreateArray();
    int count = 0;
    bool truncated = false;
    list_table_entries(target_table, path, depth, max_results, &count, entries, 0, &truncated);

    cJSON *result = cJSON_CreateObject();
    if (result == NULL) return NULL;
    cJSON_AddStringToObject(result, "path", path);
    cJSON_AddItemToObject(result, "entries", entries);
    if (truncated) {
        cJSON_AddBoolToObject(result, "truncated", 1);
    }
    cJSON_AddBoolToObject(result, "success", 1);
    return result;
}

cJSON *odb_delete_value(const char *path) {

    hdlhashtable htable;
    bigstring bsname;

    if (!resolve_path(path, &htable, bsname)) {
        return make_error_result(path, "Path not found");
    }

    if (htable == nil) {
        return make_error_result(path, "Could not resolve parent table");
    }

    /* Verify the entry exists first */
    hdlhashnode hnode;
    if (!hashtablelookupnode(htable, bsname, &hnode)) {
        return make_error_result(path, "Value not found");
    }

    if (!hashtabledelete(htable, bsname)) {
        return make_error_result(path, "Failed to delete value");
    }

    cJSON *result = cJSON_CreateObject();
    if (result == NULL) return NULL;
    cJSON_AddStringToObject(result, "path", path);
    cJSON_AddBoolToObject(result, "success", 1);
    return result;
}
