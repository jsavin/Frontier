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
#include <ctype.h>

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

    long pathlen = (long)strlen(path);
    if (pathlen > 255) {
        return false;
    }

    /* Validate path characters — only allow alphanumeric, dots, and underscores.
     * langexpandtodotparams() compiles and evaluates the path as UserTalk, so
     * unsanitized input could execute arbitrary expressions. */
    for (long i = 0; i < pathlen; i++) {
        char ch = path[i];
        if (!isalnum((unsigned char)ch) && ch != '.' && ch != '_') {
            return false;
        }
    }

    setstringlength(bspath, (short)pathlen);
    memcpy(stringbaseaddress(bspath), path, pathlen);

    boolean ok = langexpandtodotparams(bspath, htable, bsname);

    /* For bare names like "workspace" (no dot), langexpandtodotparams
     * returns htable == nil. In this case, search the root table. */
    if (ok && *htable == nil) {
        extern hdlhashtable roottable;
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
static const char *type_name_str(tyvaluetype t) {

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
            /* TODO: base64 encode for binary data */
            return cJSON_CreateString("[binary data]");

        case listvaluetype:
        case recordvaluetype:
            *out_type = type_name_str(val->valuetype);
            {
                tyvaluerecord coerced = *val;
                if (coercetostring(&coerced)) {
                    Handle h = coerced.data.stringvalue;
                    long len = gethandlesize(h);
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

    if (path != NULL) {
        cJSON_AddStringToObject(obj, "path", path);
    }
    cJSON_AddStringToObject(obj, "error", error_msg);
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
                              cJSON *entries) {

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

        /* Build full path */
        char child_path[1024];
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
                                   max_results, count_ptr, entries);
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
    cJSON_AddStringToObject(result, "path", path);
    cJSON_AddStringToObject(result, "name", name);
    cJSON_AddStringToObject(result, "type", type);
    cJSON_AddItemToObject(result, "value", json_val);
    cJSON_AddBoolToObject(result, "success", 1);

    return result;
}

cJSON *odb_set_value(const char *path, const char *type_str, const cJSON *value_json) {

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
                return make_error_result(path, "Memory allocation failed for double");
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
                /* Infer type from JSON value */
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
                            return make_error_result(path, "Memory allocation failed for double");
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

    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "path", path);
    cJSON_AddBoolToObject(result, "success", 1);
    return result;
}

cJSON *odb_list_children(const char *path, int depth, int max_results) {

    /* depth 0 = just confirm it exists */
    if (depth == 0) {
        hdlhashtable htable;
        bigstring bsname;
        if (!resolve_path(path, &htable, bsname)) {
            return make_error_result(path, "Path not found");
        }
        cJSON *result = cJSON_CreateObject();
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
    list_table_entries(target_table, path, depth, max_results, &count, entries);

    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "path", path);
    cJSON_AddItemToObject(result, "entries", entries);
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
    cJSON_AddStringToObject(result, "path", path);
    cJSON_AddBoolToObject(result, "success", 1);
    return result;
}
