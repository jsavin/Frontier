
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

#define xxxFRONTIER_PYTHON 1

#ifdef FRONTIER_PYTHON

#include "C:\\Python16\\include\\Python.h"

#endif

#include "error.h"
#include "file.h"
#include "memory.h"
#include "ops.h"
#include "resources.h"
#include "strings.h"
#include "lang.h"
#include "langipc.h"
#include "langinternal.h"
#include "langexternal.h"
#include "langsystem7.h"
#include "langhtml.h"
#include "langwinipc.h"
#include "process.h"
#include "tableinternal.h"
#include "tablestructure.h"
#include "tableverbs.h"
#include "op.h"
#include "opinternal.h"
#include "oplist.h"
#include "opverbs.h"
#include "kernelverbs.h"
#include "kernelverbdefs.h"
#include "shell.rsrc.h"
#include "timedate.h"
#include "osacomponent.h"
#include "langpython.h"

#ifdef FRONTIER_PYTHON

static boolean flpythoninitialized = false;

static void initpython (void) {

	if (!flpythoninitialized) {

		flpythoninitialized = true;

		Py_Initialize ();
		}

	return;
	}/*initpython*/


boolean langrunpythonscript (hdltreenode hp1, tyvaluerecord *v) {

	Handle h;
	boolean fl = false;
	char nilchar = '\0';

	flnextparamislast = true;

	if (!getexempttextvalue (hp1, 1, &h))
		return (false);

	if (!enlargehandle (h, 1, &nilchar)) {

		disposehandle (h);

		return (false);
		}

	initpython ();

	lockhandle (h);

	//PyObject* PyRun_String (char *str, int start, PyObject *globals, PyObject *locals);

	if (PyRun_SimpleString (*h) == 0)
		fl = setbooleanvalue (true, v);
	
	unlockhandle (h);

	disposehandle (h);

	return (fl);
	}/*langrunpythonscript*/

#else

boolean langrunpythonscript (hdltreenode hp1, tyvaluerecord *v) {
#pragma unused(hp1, v)

	langerror (unimplementedverberror);

	return (false);
	}

#endif
