/*	$Id$    */

/*
    SPDX-License-Identifier: MIT

    Copyright (c) 2026 Frontier contributors

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

/*
 * repl_verbs.c - Five REPL kernel verbs surfaced into UserTalk.
 *
 * Verbs:
 *   repl.exit()                    -> bool   (always returns true)
 *   repl.clearVariables()          -> bool   (always returns true)
 *   repl.jumpPath(path: string)    -> bool   (host's success flag)
 *   repl.printKeyCodes()           -> bool   (always returns true)
 *   repl.list([path: string])      -> bool   (always returns true)
 *
 * All five dispatch through the host adapter installed via
 * repl_verbs_set_host. The host pointer is module-local; if no host is
 * installed (NULL), every verb returns false at the script level — that's
 * a recoverable runtime error in UserTalk, not a crash.
 *
 * Why a host adapter?
 *   - Tests can run without booting the full REPL stack.
 *   - The verb implementation file does not need to reach into repl.c
 *     symbols (linenoise, linenoiseEditFeed, repl_get_variables_table,
 *     etc.), avoiding a cyclic frontier-cli/headless-runtime dep.
 *   - Future hosts (a future GUI host) can install a different adapter.
 *
 * Pattern
 * -------
 * The verb-registration shape (newfunctionprocessor + pushhashtable +
 * langaddkeyword + pophashtable) mirrors threadinitverbs() in
 * frontier-cli/headless_thread_verbs.c. Param parsing (string params,
 * optional params) mirrors menu_valueproc in tests/headless_menu_verbs.c.
 */

#include "frontier.h"
#include "standard.h"
#include "shelltypes.h"
#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

#include "repl_verbs.h"

#include <string.h>


/* ------------------------------------------------------------ */
/*  host adapter slot                                            */
/* ------------------------------------------------------------ */

static repl_verbs_host_t g_host;
static boolean g_host_installed = false;


void repl_verbs_set_host(const repl_verbs_host_t *host) {
	if (host == NULL) {
		memset(&g_host, 0, sizeof(g_host));
		g_host_installed = false;
		return;
	}
	g_host = *host;
	g_host_installed = true;
}


/* ------------------------------------------------------------ */
/*  verb token enum                                              */
/* ------------------------------------------------------------ */

enum {
	rplv_exit = 0,
	rplv_clearvariables = 1,
	rplv_jumppath = 2,
	rplv_printkeycodes = 3,
	rplv_list = 4
};


/* ------------------------------------------------------------ */
/*  helpers                                                      */
/* ------------------------------------------------------------ */

/*
 * Pull a Pascal-string param into a NUL-terminated C string in outbuf.
 * Returns true on success.
 */
static boolean copy_string_param(bigstring bs, char *outbuf, size_t cap) {
	size_t n;
	if (cap == 0)
		return false;
	n = (size_t) stringlength(bs);
	if (n >= cap)
		n = cap - 1;
	memcpy(outbuf, &bs[1], n);
	outbuf[n] = '\0';
	return true;
}


/* ------------------------------------------------------------ */
/*  verb dispatcher                                              */
/* ------------------------------------------------------------ */

static boolean repl_valueproc(short token, hdltreenode hparam1,
                              tyvaluerecord *vreturned, bigstring bserror) {
	(void) bserror;

	switch (token) {

	case rplv_exit:
		(void) hparam1;
		if (!g_host_installed || g_host.exit == NULL)
			return setbooleanvalue(false, vreturned);
		g_host.exit();
		return setbooleanvalue(true, vreturned);

	case rplv_clearvariables:
		(void) hparam1;
		if (!g_host_installed || g_host.clear_variables == NULL)
			return setbooleanvalue(false, vreturned);
		g_host.clear_variables();
		return setbooleanvalue(true, vreturned);

	case rplv_jumppath: {
		bigstring bspath;
		char cpath[1024];
		boolean ok;

		flnextparamislast = true;

		if (!getstringvalue(hparam1, 1, bspath))
			return false;

		if (!copy_string_param(bspath, cpath, sizeof(cpath)))
			return false;

		if (!g_host_installed || g_host.jump_path == NULL)
			return setbooleanvalue(false, vreturned);

		ok = g_host.jump_path(cpath);
		return setbooleanvalue(ok, vreturned);
	}

	case rplv_printkeycodes: {
		boolean fl;
		(void) hparam1;
		if (!g_host_installed || g_host.print_key_codes == NULL)
			return setbooleanvalue(false, vreturned);
		fl = g_host.print_key_codes();
		return setbooleanvalue(fl, vreturned);
	}

	case rplv_list: {
		short ctconsumed = 0, ctpositional = 0;
		tyvaluerecord val;
		bigstring bspath;
		char cpath[1024];
		boolean param_ok;

		initvalue(&val, stringvaluetype);
		flnextparamislast = true;

		/* getoptionalparamvalue returns false on parse error (NOT on
		 * "param absent") — a missing optional param leaves val at
		 * the initvalue default and still returns true. So this flag
		 * tracks param parsing success, not presence. */
		param_ok = getoptionalparamvalue(hparam1, &ctconsumed, &ctpositional,
		                                 BIGSTRING("\x04" "path"), &val);
		if (!param_ok)
			return false;

		if (!g_host_installed || g_host.list == NULL) {
			disposevaluerecord(val, false);
			return setbooleanvalue(false, vreturned);
		}

		if (val.data.stringvalue != nil) {
			texthandletostring((Handle) val.data.stringvalue, bspath);
			disposevaluerecord(val, false);
			if (!isemptystring(bspath)) {
				if (!copy_string_param(bspath, cpath, sizeof(cpath)))
					return false;
				g_host.list(cpath);
			} else {
				g_host.list(NULL);
			}
		} else {
			g_host.list(NULL);
		}
		return setbooleanvalue(true, vreturned);
	}

	default:
		return false;
	}
}


/* ------------------------------------------------------------ */
/*  verb registration                                            */
/* ------------------------------------------------------------ */

boolean replinitverbs(void) {
	hdlhashtable htable = nil;
	bigstring bsname;

	copystring(PSTRING("\004", "repl"), bsname);

	if (!newfunctionprocessor(bsname, &repl_valueproc, false, &htable))
		return false;

	pushhashtable(htable);

	#define ADD_VERB(name, tok) do { \
		bigstring bs; \
		copystring(name, bs); \
		if (!langaddkeyword(bs, tok)) { \
			pophashtable(); \
			return false; \
		} \
	} while (0)

	ADD_VERB(PSTRING("\004", "exit"), rplv_exit);
	ADD_VERB(PSTRING("\016", "clearvariables"), rplv_clearvariables);
	ADD_VERB(PSTRING("\010", "jumppath"), rplv_jumppath);
	ADD_VERB(PSTRING("\015", "printkeycodes"), rplv_printkeycodes);
	ADD_VERB(PSTRING("\004", "list"), rplv_list);

	#undef ADD_VERB

	pophashtable();
	return true;
}
