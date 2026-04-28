
/*	$Id$    */

/*
    SPDX-License-Identifier: MIT

    Copyright (c) 1992-2004 UserLand Software, Inc.
    Copyright (c) 2025-present Frontier contributors

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

#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "quickdraw.h"
#include "strings.h"
#include "ops.h"
#include "resources.h"
#include "shell.rsrc.h"
#include "langinternal.h"



#define dberrorlist 256

static byte * dberrorstrings [] = {
		/* [1] */
		STR_File_was_created_by_an_incompatible_version_of_this_program,
		/* [2] */
		STR_Internal_error_attempted_to_read_a_free_block
	};


static byte * langmiscstrings [] = {
		/* [1] */
		STR_unknown,
		/* [2] */
		STR_error
	};


static byte * stacknames [] = {
		/* [1] */
		STR_hash_table
	};


static byte * langerrorstrings [] = {
		/* [1] */
		STR_Cant_delete_XXX_because_it_hasnt_been_defined,
		/* [2] */
		STR_Stack_overflow_XXX_stack,
		/* [3] */
		STR_The_name_XXX_hasnt_been_defined,
		/* [4] */
		STR_Address_value_doesnt_refer_to_a_valid_table
	};










