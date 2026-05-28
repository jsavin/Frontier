/*
    Frontier CLI - Operation Handler

    op_handler.c - Transport-agnostic JSON operation dispatch

    Contains the op_dispatch() entry point that both the stdio NDJSON protocol
    handler and the WebSocket server call into. Operation handlers (script/eval,
    script/clearContext, odb/get etc., shutdown) live here so both transports share one
    implementation.

    JSON parsing uses the vendored cJSON library for both envelope fields
    (op, id) and ODB operation parameters.

    SPDX-License-Identifier: MIT

    Copyright (c) 1992-2004 UserLand Software, Inc.
    Copyright (c) 2025-2026 Frontier contributors

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#include "op_handler.h"
#include "odb_ops.h"
#include "debug_handler.h"
#include "repl_variables.h"
#include "repl.h"

#include "../Common/headers/frontier.h"
#include "../Common/headers/lang.h"
#include "../Common/headers/langexternal.h"
#include "../Common/headers/memory.h"
#include "../Common/headers/langinternal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "../third_party/cJSON/cJSON.h"

/* Maximum number of items in a single ODB batch request */
#define OP_MAX_BATCH_SIZE 1000
#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)
#define OP_BATCH_ERR "Batch too large (max " STRINGIFY(OP_MAX_BATCH_SIZE) " items)"

/* ========================================================================
 * Response helpers — build JSON via cJSON, send via transport
 * ======================================================================== */

/* Static fallback for OOM conditions where cJSON_CreateObject() returns NULL.
 * Ensures the client always gets a response and doesn't hang. */
static const char *OOM_FALLBACK = "{\"id\":0,\"success\":false,\"error\":{\"message\":\"Server out of memory\"}}";

/*
 * Send a pre-formatted JSON string via the transport.
 */
static void transport_send(transport_t *transport, const char *json) {
	transport->write_line(transport->ctx, json, strlen(json));
}

/*
 * Send a cJSON object as a response and clean up.
 * Prints the object as unformatted JSON, sends it via transport,
 * then frees both the printed string and the cJSON object.
 * On print failure, sends the OOM fallback.
 */
static void send_cjson_response(transport_t *transport, cJSON *response) {
	char *json_out = cJSON_PrintUnformatted(response);
	if (json_out != NULL) {
		transport_send(transport, json_out);
		cJSON_free(json_out);
	} else {
		transport_send(transport, OOM_FALLBACK);
	}
	cJSON_Delete(response);
}

/*
 * Send a simple success ack.
 */
static void send_ack(long id, transport_t *transport) {
	cJSON *response = cJSON_CreateObject();
	if (response == NULL) {
		transport_send(transport, OOM_FALLBACK);
		return;
	}
	cJSON_AddNumberToObject(response, "id", id);
	cJSON_AddBoolToObject(response, "success", 1);
	send_cjson_response(transport, response);
}

/*
 * Send an error response. cJSON_AddStringToObject handles JSON escaping
 * of the message string automatically.
 *
 * PR1 of REPL error context chain (2026-05-26 JES): if location and/or
 * stack are non-NULL, they are attached under error.location / error.stack.
 * Ownership transfers to the response on attach; the helper calls
 * cJSON_Delete on these arguments if it can't construct the wrapper, so
 * callers must not delete them on the success path.
 *
 * PR3 of REPL error context chain (2026-05-27 JES): added caused_by
 * parameter. When non-NULL it is attached under error.causedBy with the
 * same ownership semantics as location/stack. Used when an else-block
 * re-failure is the primary error and the originating try-block failure
 * provides additional context.
 */
static void send_error_with_metadata(long id, const char *message,
                                     cJSON *location, cJSON *stack,
                                     cJSON *caused_by,
                                     transport_t *transport) {
	cJSON *response = cJSON_CreateObject();
	if (response == NULL) {
		if (location != NULL) cJSON_Delete(location);
		if (stack != NULL) cJSON_Delete(stack);
		if (caused_by != NULL) cJSON_Delete(caused_by);
		transport_send(transport, OOM_FALLBACK);
		return;
	}
	cJSON_AddNumberToObject(response, "id", id);

	cJSON *error_obj = cJSON_CreateObject();
	if (error_obj == NULL) {
		/* Degrade gracefully: flat error string instead of nested object.
		 * Drop any location/stack/causedBy — they can't be attached without
		 * the containing error object. */
		if (location != NULL) cJSON_Delete(location);
		if (stack != NULL) cJSON_Delete(stack);
		if (caused_by != NULL) cJSON_Delete(caused_by);
		cJSON_AddStringToObject(response, "error", message);
	} else {
		cJSON_AddStringToObject(error_obj, "message", message);
		if (location != NULL)
			cJSON_AddItemToObject(error_obj, "location", location);
		if (stack != NULL)
			cJSON_AddItemToObject(error_obj, "stack", stack);
		if (caused_by != NULL)
			cJSON_AddItemToObject(error_obj, "causedBy", caused_by);
		cJSON_AddItemToObject(response, "error", error_obj);
	}

	cJSON_AddBoolToObject(response, "success", 0);
	send_cjson_response(transport, response);
}

static void send_error(long id, const char *message, transport_t *transport) {
	send_error_with_metadata(id, message, NULL, NULL, NULL, transport);
}

/*
 * Send a success response with a script eval result value.
 * Uses cJSON for all JSON construction including string escaping.
 */
static void send_eval_success(long id, tyvaluerecord *val, transport_t *transport) {
	const char *type_name = odb_type_name_str(val->valuetype);

	tyvaluerecord coerced = *val;

	if (coerced.valuetype == novaluetype) {
		cJSON *response = cJSON_CreateObject();
		if (response == NULL) {
			transport_send(transport, OOM_FALLBACK);
			return;
		}
		cJSON_AddNumberToObject(response, "id", id);
		cJSON *result = cJSON_CreateObject();
		if (result != NULL) {
			cJSON_AddNullToObject(result, "value");
			cJSON_AddStringToObject(result, "type", "none");
			cJSON_AddItemToObject(response, "result", result);
		}
		cJSON_AddBoolToObject(response, "success", 1);
		send_cjson_response(transport, response);
		return;
	}

	if (coerced.valuetype == externalvaluetype) {
		hdlexternalvariable hv = (hdlexternalvariable)coerced.data.externalvalue;
		if (hv != nil && (**hv).id == idtableprocessor) {
			cJSON *response = cJSON_CreateObject();
			if (response == NULL) {
				transport_send(transport, OOM_FALLBACK);
				return;
			}
			cJSON_AddNumberToObject(response, "id", id);
			cJSON *result = cJSON_CreateObject();
			if (result != NULL) {
				cJSON_AddStringToObject(result, "value", "[table]");
				cJSON_AddStringToObject(result, "type", "table");
				cJSON_AddItemToObject(response, "result", result);
			}
			cJSON_AddBoolToObject(response, "success", 1);
			send_cjson_response(transport, response);
			return;
		}
	}

	/* Track whether coercion allocated a new handle (non-string types).
	 * If the value is already a string, coercetostring is a no-op and
	 * coerced shares the same handle — disposing it would double-free. */
	boolean coerced_allocated = (coerced.valuetype != stringvaluetype);

	if (!coercetostring(&coerced)) {
		cJSON *response = cJSON_CreateObject();
		if (response == NULL) {
			transport_send(transport, OOM_FALLBACK);
			return;
		}
		cJSON_AddNumberToObject(response, "id", id);
		cJSON *result = cJSON_CreateObject();
		if (result != NULL) {
			cJSON_AddNullToObject(result, "value");
			cJSON_AddStringToObject(result, "type", type_name);
			cJSON_AddItemToObject(response, "result", result);
		}
		cJSON_AddBoolToObject(response, "success", 1);
		send_cjson_response(transport, response);
		return;
	}

	Handle hstring = coerced.data.stringvalue;
	long slen = gethandlesize(hstring);

	/* Build response with cJSON — it handles all JSON string escaping.
	 * The handle data may not be null-terminated, so we create a
	 * temporary null-terminated copy for cJSON_AddStringToObject. */
	cJSON *response = cJSON_CreateObject();
	if (response == NULL) {
		if (coerced_allocated) disposevaluerecord(coerced, false);
		transport_send(transport, OOM_FALLBACK);
		return;
	}
	cJSON_AddNumberToObject(response, "id", id);

	cJSON *result_obj = cJSON_CreateObject();
	if (result_obj != NULL) {
		/* Create null-terminated string from handle data for cJSON */
		char *str_value = malloc((size_t)slen + 1);
		if (str_value != NULL) {
			memcpy(str_value, *hstring, (size_t)slen);
			str_value[slen] = '\0';
			cJSON_AddStringToObject(result_obj, "value", str_value);
			free(str_value);
		} else {
			cJSON_AddNullToObject(result_obj, "value");
		}
		cJSON_AddStringToObject(result_obj, "type", type_name);
		cJSON_AddItemToObject(response, "result", result_obj);
	}
	cJSON_AddBoolToObject(response, "success", 1);
	send_cjson_response(transport, response);

	/* Dispose the coerced value only if coercion allocated a new handle.
	 * String values pass through coercetostring() as a no-op, sharing the
	 * same handle as the original — the caller disposes that one. */
	if (coerced_allocated) disposevaluerecord(coerced, false);
}

/* ========================================================================
 * Operation handlers
 * ======================================================================== */

/*
 * PR1 of REPL error context chain (2026-05-26 JES): derive a human-readable
 * script name from an error-frame errorrefcon. Thin wrapper around the
 * shared kernel helper langscriptnamefromrefcon -- copies the resulting
 * Pascal bigstring into the caller's C buffer with size-aware clamping.
 *
 * Refcon semantics (see lang.c::langscriptnamefromrefcon for details):
 *   0L  -- "<eval>"     (outermost top-level / REPL input / script.eval)
 *   -1L -- "<eval-inner>" (inline nested call)
 *   else -- (hdlhashnode)refcon's hashkey, or "<unknown>" if nil/HNoNode
 */
static void script_name_from_refcon(long refcon, char *out, size_t outlen) {
	bigstring bsname;

	if (outlen == 0) return;

	langscriptnamefromrefcon(refcon, bsname);

	/* Copy bigstring (length byte + payload) to C string with clamp. */
	size_t n = (size_t) bsname[0];
	if (n >= outlen) n = outlen - 1;
	memcpy(out, (const char *)(bsname + 1), n);
	out[n] = '\0';
}

/*
 * PR1 of REPL error context chain: build the error.location cJSON object
 * from the snapshot captured by langseterrorcallbackline. Returns NULL if
 * no snapshot is available or on OOM -- caller proceeds without a location
 * field (backwards-compatible response).
 */
static cJSON *build_error_location(void) {
	tyerrorrecord rec;
	if (!langgetlasterror(&rec))
		return NULL;

	cJSON *loc = cJSON_CreateObject();
	if (loc == NULL)
		return NULL;

	char namebuf[256];
	script_name_from_refcon(rec.errorrefcon, namebuf, sizeof(namebuf));

	cJSON_AddStringToObject(loc, "script", namebuf);
	cJSON_AddNumberToObject(loc, "line", (double)rec.errorline);
	cJSON_AddNumberToObject(loc, "column", (double)rec.errorchar);
	cJSON_AddNumberToObject(loc, "tokenStart", (double)rec.tokenstart);
	cJSON_AddNumberToObject(loc, "tokenEnd", (double)rec.tokenend);
	return loc;
}

/*
 * PR1 of REPL error context chain: build the error.stack cJSON array.
 * Index 0 is the failure site, growing outward to the outermost caller.
 * Returns NULL when no snapshot is available; returns an empty array if
 * the snapshot exists but has zero frames (so clients can rely on the
 * field's array type).
 */
static cJSON *build_error_stack(void) {
	short depth = langgetstackdepth();
	if (depth <= 0)
		return NULL;

	cJSON *stack = cJSON_CreateArray();
	if (stack == NULL)
		return NULL;

	for (short ix = 0; ix < depth; ++ix) {
		tyerrorrecord rec;
		long refcon = 0;
		if (!langgetstackframe(ix, &rec, &refcon))
			break;

		cJSON *frame = cJSON_CreateObject();
		if (frame == NULL)
			break;

		char namebuf[256];
		script_name_from_refcon(refcon, namebuf, sizeof(namebuf));

		cJSON_AddStringToObject(frame, "script", namebuf);
		cJSON_AddNumberToObject(frame, "line", (double)rec.errorline);
		cJSON_AddNumberToObject(frame, "column", (double)rec.errorchar);
		cJSON_AddItemToArray(stack, frame);
	}

	return stack;
}

/*
 * PR3 of REPL error context chain (2026-05-27 JES): build the
 * error.causedBy cJSON object from the causedby snapshot
 * (langgetcausedbyerror / langgetcausedbystackframe / langgetcausedbystackdepth).
 *
 * Structure mirrors the top-level error response:
 *   { message: "<originating try-block error message or empty>",
 *     location: { script, line, column, tokenStart, tokenEnd },
 *     stack:    [ { script, line, column }, ... ] }
 *
 * The message field is best-effort: we read it from the tryError global
 * when accessible. If unavailable (e.g. the runtime has already cleared
 * the handle by the time we build the response), we emit an empty
 * string so the field is type-stable for clients. The location and
 * stack are always populated from the snapshot.
 *
 * Returns NULL if no causedby snapshot is available -- caller proceeds
 * without an error.causedBy field (backwards-compatible response).
 */
static cJSON *build_error_causedby(void) {
	tyerrorrecord rec;
	if (!langgetcausedbyerror(&rec))
		return NULL;

	cJSON *cb = cJSON_CreateObject();
	if (cb == NULL)
		return NULL;

	/* PR3 of REPL error context chain: read the originating error message
	 * captured by langerrormessage when the try-block error fired (via
	 * langtrysetcausedbymessage in lang.c). Falls back to an empty string
	 * if unavailable (e.g., the message was longer than the bigstring
	 * buffer and got truncated to empty -- shouldn't happen, but be
	 * defensive). */
	const char *cb_msg = langgetcausedbymessage();
	cJSON_AddStringToObject(cb, "message", (cb_msg != NULL) ? cb_msg : "");

	cJSON *loc = cJSON_CreateObject();
	if (loc != NULL) {
		char namebuf[256];
		script_name_from_refcon(rec.errorrefcon, namebuf, sizeof(namebuf));

		cJSON_AddStringToObject(loc, "script", namebuf);
		cJSON_AddNumberToObject(loc, "line", (double)rec.errorline);
		cJSON_AddNumberToObject(loc, "column", (double)rec.errorchar);
		cJSON_AddNumberToObject(loc, "tokenStart", (double)rec.tokenstart);
		cJSON_AddNumberToObject(loc, "tokenEnd", (double)rec.tokenend);
		cJSON_AddItemToObject(cb, "location", loc);
	}

	short depth = langgetcausedbystackdepth();
	if (depth > 0) {
		cJSON *stack = cJSON_CreateArray();
		if (stack != NULL) {
			for (short ix = 0; ix < depth; ++ix) {
				tyerrorrecord frec;
				long refcon = 0;
				if (!langgetcausedbystackframe(ix, &frec, &refcon))
					break;

				cJSON *frame = cJSON_CreateObject();
				if (frame == NULL)
					break;

				char namebuf[256];
				script_name_from_refcon(refcon, namebuf, sizeof(namebuf));

				cJSON_AddStringToObject(frame, "script", namebuf);
				cJSON_AddNumberToObject(frame, "line", (double)frec.errorline);
				cJSON_AddNumberToObject(frame, "column", (double)frec.errorchar);
				cJSON_AddItemToArray(stack, frame);
			}
			cJSON_AddItemToObject(cb, "stack", stack);
		}
	}

	return cb;
}

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

	/*
	 * PR1 of REPL error context chain (2026-05-26 JES):
	 * repl_eval_with_variables_value wraps user input as
	 *     with system.temp.FrontierREPL.variables {\r <user code> \r}
	 * adding exactly one line before user input -- BUT only if the
	 * variables table is initialized (interactive REPL); the protocol
	 * mode falls through to a non-wrapped langrunhandle_value path,
	 * which means raw and user-relative line numbers are the same and
	 * no offset should be subtracted. Gate the offset install on the
	 * table actually existing so the fallback path reports raw lines
	 * verbatim.
	 */
	hdlhashtable repl_vars = repl_get_variables_table();
	if (repl_vars != nil)
		langsetevalinputoffset(1);
	else
		langclearevalinputoffset();

	boolean ok = repl_eval_with_variables_value(expression, &result, error_msg);

	if (ok) {
		langclearevalinputoffset();
		send_eval_success(id, &result, transport);
	} else {
		char c_error[256];
		long errlen = stringlength(error_msg);
		boolean error_empty = false;
		if (errlen > 0 && errlen < (long)sizeof(c_error)) {
			memcpy(c_error, stringbaseaddress(error_msg), errlen);
			c_error[errlen] = '\0';
		} else if (errlen == 0) {
			snprintf(c_error, sizeof(c_error), "Script evaluation failed");
			error_empty = true;
		} else {
			snprintf(c_error, sizeof(c_error), "Script error (message too long)");
		}

		/* Build location / stack / causedBy BEFORE clearing the offset --
		 * the builders apply the offset internally to errorline.
		 *
		 * PR3 of REPL error context chain: build_error_causedby returns
		 * NULL when no causedby snapshot exists (the common case: error
		 * fired outside any try/else chain), in which case the response
		 * has no error.causedBy field -- backwards compatible. */
		cJSON *location = build_error_location();
		cJSON *stack = build_error_stack();
		cJSON *caused_by = build_error_causedby();

		/* P1-2 (bar-raiser, 2026-05-27 JES): when bserror is empty (common
		 * for scriptError() fired from inside an else block — the
		 * langtraperror path leaves bserror empty for eval-wrapped scripts)
		 * AND a causedby snapshot is available, promote the causedby message
		 * to the primary error.message slot and drop the causedBy field to
		 * avoid duplicate text. This makes the user-facing message
		 * meaningful instead of the "Script evaluation failed" boilerplate.
		 *
		 * The causedby snapshot's location/stack remain available via the
		 * top-level error.location / error.stack (already built from the
		 * live error state, which carries the originating frame in this
		 * empty-bserror case). */
		const char *cb_msg = langgetcausedbymessage();
		short cb_depth = langgetcausedbystackdepth();
		if (error_empty && caused_by != NULL && cb_msg != NULL
		    && cb_msg[0] != '\0' && cb_depth > 0) {
			size_t cb_len = strlen(cb_msg);
			if (cb_len >= sizeof(c_error))
				cb_len = sizeof(c_error) - 1;
			memcpy(c_error, cb_msg, cb_len);
			c_error[cb_len] = '\0';
			cJSON_Delete(caused_by);
			caused_by = NULL;
		}

		langclearevalinputoffset();
		send_error_with_metadata(id, c_error, location, stack, caused_by, transport);
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
	if (error_obj == NULL) {
		/* OOM fallback: try adding error as a flat string instead of an object.
		 * The response will have "error":"message" rather than
		 * "error":{"message":"..."}, but at least the client gets something. */
		cJSON_AddStringToObject(item, "error", message);
		return;
	}
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
		send_error(id, OP_BATCH_ERR, transport);
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
		send_error(id, OP_BATCH_ERR, transport);
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
		send_error(id, OP_BATCH_ERR, transport);
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
		send_error(id, OP_BATCH_ERR, transport);
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

	/* Require id field so clients can correlate responses.
	 * Send "id":null (not "id":0) for missing ids per JSON-RPC convention. */
	if (!id_present) {
		const char *err_resp = "{\"id\":null,\"error\":{\"message\":\"Missing 'id' field\"},\"success\":false}";
		transport_send(transport, err_resp);
		free(op);
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
	} else if (strcmp(op, "debug/run") == 0) {
		handle_debug_run(id, json_line, transport);
	} else if (strcmp(op, "debug/step") == 0) {
		handle_debug_step(id, json_line, transport);
	} else if (strcmp(op, "debug/continue") == 0) {
		handle_debug_continue(id, json_line, transport);
	} else if (strcmp(op, "debug/kill") == 0) {
		handle_debug_kill(id, json_line, transport);
	} else if (strcmp(op, "debug/pause") == 0) {
		handle_debug_pause(id, json_line, transport);
	} else if (strcmp(op, "debug/setBreakpoint") == 0) {
		handle_debug_setbreakpoint(id, json_line, transport);
	} else if (strcmp(op, "debug/listBreakpoints") == 0) {
		handle_debug_listbreakpoints(id, json_line, transport);
	} else if (strcmp(op, "debug/clearBreakpoints") == 0) {
		handle_debug_clearbreakpoints(id, json_line, transport);
	} else if (strcmp(op, "debug/getLocals") == 0) {
		handle_debug_getlocals(id, json_line, transport);
	} else if (strcmp(op, "debug/getSource") == 0) {
		handle_debug_getsource(id, json_line, transport);
	} else if (strcmp(op, "debug/getStack") == 0) {
		handle_debug_getstack(id, json_line, transport);
	} else if (strcmp(op, "debug/listThreads") == 0) {
		handle_debug_listthreads(id, json_line, transport);
	} else if (strcmp(op, "debug/setWatchpoint") == 0) {
		handle_debug_setwatchpoint(id, json_line, transport);
	} else if (strcmp(op, "debug/listWatchpoints") == 0) {
		handle_debug_listwatchpoints(id, json_line, transport);
	} else if (strcmp(op, "debug/clearWatchpoints") == 0) {
		handle_debug_clearwatchpoints(id, json_line, transport);
	} else if (strcmp(op, "shutdown") == 0) {
		send_ack(id, transport);
		free(op);
		return 1;  /* signal shutdown */
	} else {
		/* Build error message. cJSON (via send_error) handles escaping
		 * the op string, so no injection risk from crafted op names. */
		char err[256];
		snprintf(err, sizeof(err), "Unknown operation: %s", op);
		send_error(id, err, transport);
	}

	free(op);
	return 0;
}
