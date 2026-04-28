/*
    Frontier CLI - ODB Operations

    odb_ops.h - ODB path resolution, value serialization, and CRUD operations

    Provides the implementation for odb/get, odb/set, odb/list, odb/delete.
    Each function takes path/value parameters and returns a cJSON object
    representing the per-item result (suitable for embedding in a batch
    response array).

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

#ifndef ODB_OPS_H
#define ODB_OPS_H

#include "../third_party/cJSON/cJSON.h"
#include "../Common/headers/lang.h"

/*
 * Get the simple type name string for a value type.
 * Shared with op_handler.c to avoid duplicate implementations.
 */
const char *odb_type_name_str(tyvaluetype t);

/*
 * Get a value from the ODB at the given dotted path.
 * Returns a cJSON object with: path, name, type, value, success.
 */
cJSON *odb_get_value(const char *path);

/*
 * Set (create or overwrite) a value in the ODB at the given dotted path.
 * type_str is the value type name ("string", "long", "boolean", etc.).
 * value_json is the cJSON node containing the value.
 * Returns a cJSON object with: path, success, [error].
 */
cJSON *odb_set_value(const char *path, const char *type_str, const cJSON *value_json);

/*
 * List children of a table at the given dotted path.
 * depth: 0 = just confirm exists, 1 = direct children, 2+ = recurse, -1 = full recursive
 * max_results: cap on total entries returned.
 * Returns a cJSON object with: path, entries[], success.
 */
cJSON *odb_list_children(const char *path, int depth, int max_results);

/*
 * Delete a value from the ODB at the given dotted path.
 * Returns a cJSON object with: path, success, [error].
 */
cJSON *odb_delete_value(const char *path);

#endif /* ODB_OPS_H */
