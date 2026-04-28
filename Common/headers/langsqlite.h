
/*	$Id: langsqlite.h,v 1.1.2.1 2006/03/24 01:31:35 davidgewirtz Exp $    */

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


// prototypes

extern boolean sqliteopenverb (hdltreenode, tyvaluerecord *, bigstring); // 2006-03-15 gewirtz

extern boolean sqlitecloseverb (hdltreenode, tyvaluerecord *, bigstring); // 2006-03-17 gewirtz

extern boolean sqlitecompilequeryverb (hdltreenode, tyvaluerecord *, bigstring); // 2006-04-18 gewirtz

extern boolean sqliteclearqueryverb (hdltreenode, tyvaluerecord *, bigstring); // 2006-04-18 gewirtz

extern boolean sqliteresetqueryverb (hdltreenode, tyvaluerecord *, bigstring); // 2006-08-31 gewirtz

extern boolean sqlitestepqueryverb (hdltreenode, tyvaluerecord *, bigstring); // 2006-08-31 gewirtz

extern boolean sqlitegetcolumncountverb (hdltreenode, tyvaluerecord *, bigstring); // 2006-04-18 gewirtz

extern boolean sqlitegetcolumntypeverb (hdltreenode, tyvaluerecord *, bigstring); // 2006-04-18 gewirtz

extern boolean sqlitegetcolumnintverb (hdltreenode, tyvaluerecord *, bigstring); // 2006-04-18 gewirtz

extern boolean sqlitegetcolumndoubleverb (hdltreenode, tyvaluerecord *, bigstring); // 2006-04-18 gewirtz

extern boolean sqlitegetcolumntextverb (hdltreenode, tyvaluerecord *, bigstring); // 2006-04-18 gewirtz

extern boolean sqlitegetcolumnnameverb (hdltreenode, tyvaluerecord *, bigstring); // 2006-04-18 gewirtz

extern boolean sqlitegetcolumnverb (hdltreenode, tyvaluerecord *, bigstring); // 2006-04-20 gewirtz

extern boolean sqlitegetrowverb (hdltreenode, tyvaluerecord *, bigstring); // 2006-08-31 gewirtz

extern boolean sqlitegeterrormessageverb (hdltreenode, tyvaluerecord *, bigstring); // 2006-04-20 gewirtz

extern boolean sqliteinitverbs (void); // 2006-03-15 gewirtz

extern boolean sqlitesetcolumnblobverb ( hdltreenode, tyvaluerecord *, bigstring ); // 2007-08-25 creedon

extern boolean getlastinsertrowidverb ( hdltreenode, tyvaluerecord *, bigstring );  // 2007-08-28 creedon


// SQLITE MESSAGES

#define SQLITE_COLUMN_ERROR_0 "\xe9""SQLite column error. Frontier uses a base-1 index. Columns must be specified with 1 indicating the first column. This is different from the SQLite API, which requires that columns must be specified with 0 indicating the first column."
#define SQLITE_COLUMN_ERROR_MAX "\x50""SQLite column error. An attempt was made to access a column that does not exist."
#define SQLITE_COLUMN_ERROR_UNDEFINED "\x29""SQLite returned an undefined column type."
#define SQLITE_COLUMN_ERROR_ROW_OR_COLUMN "\x31" "SQLite invalid row or column number out of range." // 2007-08-25 creedon
#define SQLITE_PARAMETER_ERROR_COUNT "\x1b" "SQLite parameter error. An attempt was made to access a parameter that does not exist." // 2007-08-26 creedon

