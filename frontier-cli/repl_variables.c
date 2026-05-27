/*
    repl_variables.c - REPL persistent variable management

    Manages system.temp.FrontierREPL for persistent REPL state:
    - variables: Variables that persist across REPL evaluations
    - target: Current target address (for target.set()/clear())
    - focus: Mirrors /jump location, exposed to UserTalk
    - commands: Custom slash command scripts

    Implementation uses a callback hook in langpushlocalchain:
    When the `with` statement's local table is about to be disposed,
    we sync any new/modified variables to the persistent table.

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

#include <stdio.h>
#include <string.h>

#include "repl_variables.h"
#include "../Common/headers/lang.h"
#include "../Common/headers/langinternal.h"
#include "../Common/headers/strings.h"
#include "../Common/headers/memory.h"
#include "../Common/headers/logging.h"
#include "../Common/headers/tablestructure.h"
#include "../Common/headers/langexternal.h"

/* Name constants for our REPL tables (Pascal strings: length byte + chars) */
static byte namerepltable[] = "\x0c" "FrontierREPL";	 /* "FrontierREPL" (12 chars) */
static byte namevariables[] = "\x09" "variables";		  /* "variables" (9 chars) */
static byte namecommands[] = "\x08" "commands";			  /* "commands" (8 chars) */
static byte nametemptable[] = "\x04" "temp";			  /* "temp" (4 chars) */
static byte nametarget[] = "\x06" "target";				  /* "target" (6 chars) */
static byte namefocus[] = "\x05" "focus";				  /* "focus" (5 chars) */

/* Cached handle to our tables for quick access */
static hdlhashtable g_repl_variables_table = nil;
static hdlhashtable g_repl_table = nil;

/* Flag to track when we're in a REPL eval - only sync during REPL evals */
static boolean g_repl_eval_active = false;

/* Forward declaration for table lookup */
extern boolean findnamedtable(hdlhashtable htable, bigstring bs, hdlhashtable *hnamedtable);

/*
 * Create a subtable if it doesn't exist.
 * Returns true if table exists or was created.
 */
static boolean ensure_subtable(hdlhashtable parent, byte *name, hdlhashtable *result) {
	if (findnamedtable(parent, name, result)) {
		return true;  /* Already exists */
	}

	/* Create it */
	if (!tablenewsubtable(parent, name, result)) {
		log_error(LOG_COMP_GENERAL, "Failed to create REPL subtable: %.*s",
				  (int)name[0], name + 1);
		return false;
	}

	log_debug(LOG_COMP_GENERAL, "Created REPL subtable: %.*s",
			  (int)name[0], name + 1);
	return true;
}

/*
 * Callback to sync variables from the with-block's local table
 * to the persistent variables table before disposal.
 *
 * FLOW DIAGRAM:
 *	 1. User enters code in REPL (e.g., "x = 42")
 *	 2. repl_eval_with_variables() wraps it: "with system.temp.FrontierREPL.variables { x = 42 }"
 *	 3. Sets g_repl_eval_active = true
 *	 4. langrunhandletraperror() evaluates the wrapped script
 *	 5. The `with` statement creates a local table with references to variables table
 *	 6. User's code executes, creating/modifying variables in the local table
 *	 7. When `with` block ends, langpoplocalchain() is called
 *	 8. langpoplocalchain() invokes this callback with the local table
 *	 9. We copy new/modified variables to system.temp.FrontierREPL.variables
 *	10. Local table is disposed (but variables now persisted)
 *	11. g_repl_eval_active = false
 *
 * This is called by langpoplocalchain just before disposing the local table.
 * We only sync when g_repl_eval_active is true (we're in a REPL eval).
 */
static void repl_sync_variables_callback(hdlhashtable hlocals) {
	/* Only sync during REPL evals, not for every script execution */
	if (!g_repl_eval_active) {
		return;
	}

	log_trace(LOG_COMP_GENERAL, "repl_sync_variables_callback called with hlocals=%p, eval_active=%d",
			  (void*)hlocals, g_repl_eval_active);

	if (hlocals == nil) {
		log_trace(LOG_COMP_GENERAL, "  hlocals is nil, returning");
		return;
	}

	if (g_repl_variables_table == nil) {
		log_trace(LOG_COMP_GENERAL, "  g_repl_variables_table is nil, returning");
		return;
	}

	/* Check if this is a local table (we only want to sync from local tables) */
	if (!(**hlocals).fllocaltable) {
		log_trace(LOG_COMP_GENERAL, "  not a local table, returning");
		return;
	}

	log_debug(LOG_COMP_GENERAL, "Syncing variables from hlocals=%p to persistent=%p",
			  (void*)hlocals, (void*)g_repl_variables_table);

	/* Walk all entries in the local table and copy to persistent table.
	 * Skip the "with" reference entries (address values pointing to tables). */
	hdlhashnode h;
	int count = 0;

	for (h = (**hlocals).hfirstsort; h != nil; h = (**h).sortedlink) {
		bigstring bsname;
		tyvaluerecord val;

		gethashkey(h, bsname);
		val = (**h).val;

		/* Skip the "with" reference entries - they're address values
		 * with names like "with1", "with2" etc. that point to tables */
		if (bsname[0] >= 4 &&
			bsname[1] == 'w' && bsname[2] == 'i' &&
			bsname[3] == 't' && bsname[4] == 'h') {
			log_trace(LOG_COMP_GENERAL, "Skipping with-reference: %.*s",
					  (int)bsname[0], bsname + 1);
			continue;
		}

		/* Skip code values (functions/scripts) - they require special handling
		 * to copy correctly and trigger assertion failures in langunpacktree.
		 * TODO: Support function persistence in a future update. */
		if (val.valuetype == codevaluetype) {
			log_trace(LOG_COMP_GENERAL, "Skipping code value: %.*s (function persistence not yet supported)",
					  (int)bsname[0], bsname + 1);
			continue;
		}

		/* Copy value to persistent table */
		tyvaluerecord valcopy;
		if (!copyvaluerecord(val, &valcopy)) {
			log_warn(LOG_COMP_GENERAL, "Failed to copy value for variable %.*s",
					 (int)bsname[0], bsname + 1);
			continue;
		}

		/* Assign to persistent table */
		pushhashtable(g_repl_variables_table);
		boolean fl = hashassign(bsname, valcopy);
		pophashtable();

		if (fl) {
			/* Remove from tmpstack since it's now owned by the hash table */
			exemptfromtmpstack(&valcopy);
			count++;
			log_debug(LOG_COMP_GENERAL, "Synced variable %.*s to persistent storage",
					  (int)bsname[0], bsname + 1);
		} else {
			/* Assignment failed - need to dispose the copy */
			disposevaluerecord(valcopy, false);
			log_warn(LOG_COMP_GENERAL, "Failed to assign variable %.*s",
					 (int)bsname[0], bsname + 1);
		}
	}

	if (count > 0) {
		log_info(LOG_COMP_GENERAL, "Synced %d variable(s) to persistent storage", count);
	}
}

boolean repl_variables_init(void) {
	hdlhashtable htemp = nil;
	hdlhashtable hrepl = nil;
	hdlhashtable hvars = nil;
	hdlhashtable hcommands = nil;

	log_debug(LOG_COMP_GENERAL, "Initializing REPL variables subsystem");

	/* Find system.temp - required for persistent variables */
	if (systemtable == nil) {
		log_warn(LOG_COMP_GENERAL, "systemtable is nil, REPL variables will not persist");
		return false;
	}

	if (!findnamedtable(systemtable, nametemptable, &htemp)) {
		log_warn(LOG_COMP_GENERAL, "system.temp not found, REPL variables will not persist");
		return false;
	}

	log_debug(LOG_COMP_GENERAL, "Found system.temp at %p", (void*)htemp);

	/* Create system.temp.FrontierREPL if it doesn't exist */
	if (!ensure_subtable(htemp, namerepltable, &hrepl)) {
		return false;
	}
	g_repl_table = hrepl;

	/* Create the subtables - only variables and commands are tables */
	if (!ensure_subtable(hrepl, namevariables, &hvars)) {
		return false;
	}
	g_repl_variables_table = hvars;

	if (!ensure_subtable(hrepl, namecommands, &hcommands)) {
		return false;
	}

	/* Initialize target to nil (no target set) */
	{
		tyvaluerecord nilval;
		initvalue(&nilval, novaluetype);
		pushhashtable(hrepl);
		if (!hashassign(nametarget, nilval)) {
			pophashtable();
			log_warn(LOG_COMP_GENERAL, "Failed to initialize REPL target");
		} else {
			pophashtable();
		}
	}

	/* Initialize focus to @root (start at root table) */
	{
		tyvaluerecord focusval;
		if (setaddressvalue(roottable, zerostring, &focusval)) {
			pushhashtable(hrepl);
			if (!hashassign(namefocus, focusval)) {
				pophashtable();
				disposevaluerecord(focusval, false);
				log_warn(LOG_COMP_GENERAL, "Failed to initialize REPL focus");
			} else {
				pophashtable();
				exemptfromtmpstack(&focusval);
			}
		}
	}

	log_info(LOG_COMP_GENERAL, "REPL variables initialized: variables=%p, commands=%p",
			 (void*)hvars, (void*)hcommands);

	/* Install the callback for syncing variables */
	langmagictabledisposecallback = repl_sync_variables_callback;

	log_debug(LOG_COMP_GENERAL, "Installed REPL callback: %p", (void*)langmagictabledisposecallback);

	return true;
}

void repl_variables_cleanup(void) {
	/* Remove the callback */
	langmagictabledisposecallback = nil;

	log_debug(LOG_COMP_GENERAL, "REPL variables cleanup");
	g_repl_variables_table = nil;
	g_repl_table = nil;
}

hdlhashtable repl_get_variables_table(void) {
	return g_repl_variables_table;
}

/*
 * Skip past inter-token noise — whitespace and comments — and return a
 * pointer to the next real source byte (or to the '\0' terminator if
 * nothing real follows).
 *
 * Noise includes:
 *   - ASCII whitespace: space, tab, CR, LF
 *   - C++-style line comments: '// ... \r|\n'
 *   - UserTalk curly comments: '«...»' encoded as UTF-8 (0xC2 0xAB ... 0xC2 0xBB)
 *
 * UTF-8 note: '«' is 0xC2 0xAB and '»' is 0xC2 0xBB. Both bytes are
 * well-formed UTF-8, and any non-comment 0xC2 byte (continuation of a
 * different multi-byte character) is followed by a non-0xAB byte, so the
 * leading-byte check is unambiguous.
 *
 * Used by the newline-normalizer's lookahead to decide whether a real
 * statement follows a bare \r/\n. Factored out so the same skip rules
 * apply to both the '}' guard (#635) and the EOF guard (#620 trailing
 * newline cluster).
 */
static const char *peek_next_real_byte(const char *p) {
	for (;;) {
		while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
			p++;
		if (p[0] == '/' && p[1] == '/') {
			while (*p != '\0' && *p != '\r' && *p != '\n')
				p++;
			/* Stop AT the \r/\n. The outer loop's whitespace pass
			 * consumes it on the next iteration. */
			continue;
		}
		if ((unsigned char)p[0] == 0xC2 && (unsigned char)p[1] == 0xAB) {
			p += 2;
			/* Walk to the closing '»' (0xC2 0xBB). End-of-string
			 * exit is safe — a malformed unterminated comment will
			 * fail to compile after normalization; we just stop
			 * scanning here. */
			while (*p != '\0' &&
			       !((unsigned char)p[0] == 0xC2 && (unsigned char)p[1] == 0xBB))
				p++;
			if (*p != '\0')
				p += 2;	/* skip the closing '»' */
			continue;
		}
		return p;
	}
}

/* Token-kind classification for the newline-normalizer transition table.
 *
 * prev_kind tracks the last non-whitespace byte (or token) written to the
 * output. The normalizer distinguishes four anchor bytes ('\0' = start of
 * script, ';', '{', '}'), a "binary-operator-at-EOL" class (the most recent
 * non-whitespace bytes form a binary operator that demands a continuation),
 * and "anything else". The BINOP class is the #620-burndown fix for line
 * continuations like 'x and\n y' / 'x ==\n y' / 'f (\n x, y)' — without it,
 * the (PREV_OTHER, NEXT_OTHER) cell would emit ';' and break the expression. */
typedef enum {
	PREV_START = 0,	/* '\0' — nothing written yet */
	PREV_SEMI,	/* ';' — already separated */
	PREV_LBRACE,	/* '{' — block-open, next byte is first statement */
	PREV_RBRACE,	/* '}' — block-close, may attach else/};/}} */
	PREV_BINOP,	/* trailing token is a binary operator — continuation expected */
	PREV_OTHER,	/* identifier / digit / paren / operator / etc. */
	PREV_KIND_COUNT
} prev_kind_t;

/* next_kind is the kind of the next real source byte after peeking past
 * whitespace and comments. The normalizer cares about EOF, the 'else'
 * keyword, ';', '}', and "anything else" — those are the only cells that
 * can flip a suppression decision. */
typedef enum {
	NEXT_EOF = 0,	/* '\0' — nothing real follows */
	NEXT_ELSE,	/* 'else' keyword followed by a delimiter */
	NEXT_SEMI,	/* ';' */
	NEXT_RBRACE,	/* '}' */
	NEXT_OTHER,	/* any other source byte (identifier / number / paren / ...) */
	NEXT_KIND_COUNT
} next_kind_t;

/* Returns 1 if byte c can be part of a UserTalk identifier (ASCII letter,
 * digit, or underscore). Used to enforce a word boundary before identifier-
 * shaped operators 'and' / 'or' — without this, the variable name 'band'
 * would falsely match as an 'and' continuation token. */
static int is_ident_byte(unsigned char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
	       (c >= '0' && c <= '9') || c == '_';
}

/* Walks backward from the current dst position over output bytes that act
 * as inter-token whitespace (space, tab, \r, \n) and returns a pointer
 * (one past) the last non-whitespace byte written to dst. Returns 'out' if
 * the buffer is empty or contains only whitespace. The byte immediately
 * before the returned pointer is the trailing token's last byte. */
static const char *rtrim_dst(const char *out, const char *dst) {
	while (dst > out) {
		unsigned char c = (unsigned char)dst[-1];
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
			dst--;
		else
			break;
	}
	return dst;
}

/* Returns 1 if the bytes immediately preceding p (exclusive) exactly match
 * the keyword kw of length kwlen AND that match is preceded by either the
 * start of the output buffer or a non-identifier byte. Used by prev_is_binop
 * to detect trailing UserTalk word-form infix operators while preventing
 * false matches against identifiers ending in the same suffix (e.g. 'band'
 * vs 'and', 'door' vs 'or', 'thatContains' vs 'contains'). */
static int prev_matches_keyword(const char *out, const char *p,
                                const char *kw, size_t kwlen) {
	if ((size_t)(p - out) < kwlen)
		return 0;
	if (memcmp(p - kwlen, kw, kwlen) != 0)
		return 0;
	if ((size_t)(p - out) > kwlen &&
	    is_ident_byte((unsigned char)*(p - kwlen - 1)))
		return 0;
	return 1;
}

/* Detects whether the most recent token in the output buffer is a binary
 * operator that demands a continuation on the next line. Operators covered:
 *
 *   symbolic boolean:     '&&', '||'
 *   keyword boolean:      'and', 'or' (word-bounded)
 *   symbolic relational:  '==', '>=', '<=', '!='
 *   keyword relational:   'equals', 'notEquals', 'lessThan', 'greaterThan'
 *                         (the UserTalk parser does not accept word forms
 *                         of '<=' / '>=', so none are listed here)
 *   keyword string/list:  'contains', 'beginsWith', 'endsWith'
 *   single-char arith:    '+', '-', '*', '/', '%'
 *   list separator:       ','
 *   single-char relational: '>', '<'
 *   open paren:           '('
 *
 * Excluded by design:
 *   ')'        — close paren is end-of-expression, not continuation
 *   '='        — assignment; a continuation case ends in '==', not '='
 *   '}', '{', ';' — handled by their own prev-kind classes
 *
 * Unary '-' at EOL (e.g. 'x = -\n 1') is rare and statically indistinguishable
 * from binary '-' without a tokenizer; we over-suppress here. The worst case
 * is a parser error on a script that was already broken — surfacing the bug
 * is preferable to silently mis-normalizing the common binary-'-' case.
 *
 * The byte-before-operator check for '=' disambiguates '==' from assignment.
 * The word-boundary check inside prev_matches_keyword prevents false matches
 * against identifiers ending in keyword substrings ('band', 'door', etc).
 */
static int prev_is_binop(const char *out, const char *dst) {
	const char *p = rtrim_dst(out, dst);
	if (p == out)
		return 0;
	unsigned char last = (unsigned char)p[-1];
	switch (last) {
		case '+': case '-': case '*': case '/': case '%': case ',':
		case '(': case '>': case '<':
			return 1;
		case '=':
			/* '==' or '>=' or '<=' or '!=' are BINOP; bare '=' is assignment. */
			if (p - out >= 2) {
				unsigned char prev = (unsigned char)p[-2];
				if (prev == '=' || prev == '>' || prev == '<' || prev == '!')
					return 1;
			}
			return 0;
		case '&':
			/* '&&' is the symbolic boolean AND; bare '&' is not a UserTalk operator. */
			if (p - out >= 2 && p[-2] == '&')
				return 1;
			return 0;
		case '|':
			/* '||' is the symbolic boolean OR; bare '|' is not a UserTalk operator. */
			if (p - out >= 2 && p[-2] == '|')
				return 1;
			return 0;
		default:
			/* Word-form infix operators. Each match requires a word boundary
			 * (start-of-buffer or non-identifier byte) before the keyword. */
			if (prev_matches_keyword(out, p, "and", 3)) return 1;
			if (prev_matches_keyword(out, p, "or", 2)) return 1;
			if (prev_matches_keyword(out, p, "equals", 6)) return 1;
			if (prev_matches_keyword(out, p, "notEquals", 9)) return 1;
			if (prev_matches_keyword(out, p, "lessThan", 8)) return 1;
			if (prev_matches_keyword(out, p, "greaterThan", 11)) return 1;
			if (prev_matches_keyword(out, p, "contains", 8)) return 1;
			if (prev_matches_keyword(out, p, "beginsWith", 10)) return 1;
			if (prev_matches_keyword(out, p, "endsWith", 8)) return 1;
			return 0;
	}
}

/* Map the last byte written (prev_nonws) to its prev-kind. For anchor bytes
 * the byte alone is sufficient; for the BINOP class, the caller must consult
 * prev_is_binop() because the operator may span multiple bytes ('and', '==').
 * The single-byte form is retained for callers that have no buffer pointer. */
static prev_kind_t classify_prev(unsigned char prev_nonws) {
	switch (prev_nonws) {
		case '\0': return PREV_START;
		case ';':  return PREV_SEMI;
		case '{':  return PREV_LBRACE;
		case '}':  return PREV_RBRACE;
		default:   return PREV_OTHER;
	}
}

/* Buffer-aware classifier used by the normalizer loop. Falls back to the
 * single-byte form, then upgrades PREV_OTHER → PREV_BINOP when the trailing
 * token in the output buffer is a binary operator demanding continuation. */
static prev_kind_t classify_prev_full(unsigned char prev_nonws,
                                      const char *out, const char *dst) {
	prev_kind_t k = classify_prev(prev_nonws);
	if (k == PREV_OTHER && prev_is_binop(out, dst))
		return PREV_BINOP;
	return k;
}

/* Classify the byte at *peek (a pointer returned by peek_next_real_byte).
 *
 * The 'else' check requires that 'else' be a standalone keyword, not the
 * prefix of a longer identifier — so the byte after the 'e' must be a
 * delimiter ('\0', whitespace, or '{'). This is the same check the
 * original guard ran inline. */
static next_kind_t classify_next(const char *peek) {
	if (*peek == '\0')
		return NEXT_EOF;
	if (*peek == ';')
		return NEXT_SEMI;
	if (*peek == '}')
		return NEXT_RBRACE;
	if (peek[0] == 'e' && peek[1] == 'l' && peek[2] == 's' && peek[3] == 'e' &&
	    (peek[4] == '\0' || peek[4] == ' ' || peek[4] == '\t' ||
	     peek[4] == '\r' || peek[4] == '\n' || peek[4] == '{')) {
		return NEXT_ELSE;
	}
	return NEXT_OTHER;
}

/* Transition table: emit (1) a synthetic ';' before the bare \r/\n, or
 * suppress (0). Rows indexed by prev_kind, columns by next_kind.
 *
 * The cells are the specification — the loop below is just a lookup.
 *
 *                 |  EOF  | ELSE  | SEMI  | RBRACE| OTHER |
 *   --------------+-------+-------+-------+-------+-------+
 *   START         |   -   |   -   |   -   |   -   |   -   |   start-of-script: nothing precedes
 *   SEMI          |   -   |   -   |   -   |   -   |   -   |   already separated by ';'
 *   LBRACE        |   -   |   -   |   -   |   -   |   -   |   inside block: '{' acts as separator
 *   RBRACE        |   -   |   -   |   -   |   -   |   E   |   '}' attaches else/};/}}; only "other" needs ';'
 *   BINOP         |   -   |   -   |   -   |   -   |   -   |   continuation expected; next line is same expression
 *   OTHER         |   -   |   E   |   E   |   E   |   E   |   real expression: ';' needed for any real follower
 *
 *   E = emit ';'; '-' = suppress.
 *
 * Notes on individual cells:
 *   - OTHER x EOF (#620 trailing-newline): a purely trailing '\r'/'\n' must
 *     not inject a ';' — the with-wrapper's '\n}' suffix would then enclose
 *     an empty statement whose evaltree default 'true' would mask the
 *     user's last-expression value.
 *   - RBRACE x ELSE/SEMI/RBRACE (#635 '}' guard): tokens that attach to or
 *     close the '}' must not be split from it. Inserting ';' before 'else'
 *     would orphan it from its if/try/case clause; before another '}' or
 *     ';' would inject an empty statement.
 *   - LBRACE row mirrors SEMI: the language treats '{' as a statement
 *     separator at block boundaries, so the first body byte never needs ';'.
 *   - START row exists to match real source: leading bare newlines (common
 *     in YAML `|` block scalars) and whitespace-only scripts must not gain
 *     a leading ';'.
 *   - BINOP row (#620 5th-hole, this commit): when a line ends with a
 *     binary-operator token ('and', 'or', '==', '>=', '<=', '!=', '>', '<',
 *     '+', '-', '*', '/', ',', '('), the next line is a continuation of the
 *     same expression. Inserting ';' produces invalid source like 'x and; y'
 *     which the parser rejects. Suppress for every next-kind. The classifier
 *     (prev_is_binop) requires a word boundary before identifier-shaped
 *     operators so 'band' / 'door' aren't misread.
 *
 * Moot-by-construction cells (the input that would exercise them is itself
 * a parse error before normalization matters, so flipping these values has
 * no observable effect): START x ELSE, START x RBRACE, SEMI x ELSE,
 * LBRACE x ELSE, LBRACE x EOF, BINOP x ELSE/SEMI/RBRACE/EOF (a script that
 * ends with a binop is itself malformed; those cells stay 0 for consistency).
 * The reachable BINOP cell is BINOP x OTHER. The other non-moot cells are
 * covered by integration tests in
 * tests/integration/test_cases/protocol_eval_compile_errors.yaml.
 *
 * If a future grammar change needs a new suppression, flip exactly one cell
 * here and the loop logic stays unchanged.
 */
static const unsigned char kEmitSep[PREV_KIND_COUNT][NEXT_KIND_COUNT] = {
	/*               EOF  ELSE  SEMI  RBRACE  OTHER */
	/* START   */ {  0,    0,    0,    0,      0    },
	/* SEMI    */ {  0,    0,    0,    0,      0    },
	/* LBRACE  */ {  0,    0,    0,    0,      0    },
	/* RBRACE  */ {  0,    0,    0,    0,      1    },
	/* BINOP   */ {  0,    0,    0,    0,      0    },
	/* OTHER   */ {  0,    1,    1,    1,      1    },
};

/* Strip trailing ';' and trailing ASCII whitespace from the normalized
 * output, advancing *dst_io backward and rewriting the terminator.
 *
 * Used as a post-pass on the normalized buffer (#620 trailing-';' case).
 * A user-typed trailing ';' — possibly followed by whitespace — would
 * otherwise parse as an empty trailing statement and produce the parser's
 * default truthy value instead of the preceding expression's value. The
 * EOF guard in the main loop handles the no-explicit-';' case; this strip
 * handles the explicit-';' case.
 *
 * Never strips into the prefix: the loop stops at dst == out, which is
 * the initial value before any byte was emitted. Empty input (and fully
 * strippable input like "   \n\n;;;") safely yields "". */
static void strip_trailing_whitespace_and_semicolons(char *out, char **dst_io) {
	char *dst = *dst_io;
	while (dst > out) {
		unsigned char last = (unsigned char)dst[-1];
		if (last == ' ' || last == '\t' || last == '\r' || last == '\n' || last == ';') {
			dst--;
			*dst = '\0';
		} else {
			break;
		}
	}
	*dst_io = dst;
}

/* Normalize bare \r/\n in a UserTalk script into statement separators so
 * multi-line protocol script/eval sources parse correctly.
 *
 * UserTalk's grammar requires ';' between statements at top level; bare
 * \r/\n are whitespace to the scanner. Without normalization, YACC error
 * recovery drops every statement after the first and the with-wrapper's
 * evaltree default 'true' replaces the user's last-expression value.
 *
 * The kEmitSep transition table above is the specification — for every
 * (prev_kind, next_kind) pair, the cell value decides emit (';' inserted)
 * or suppress. Post-pass strips trailing whitespace and ';' so explicitly-
 * terminated scripts return their last-expression value, matching REPL
 * convention.
 *
 * Caller owns the returned heap buffer. Returns NULL on alloc failure or
 * NULL input. Issues #624 (initial), #635 ('}' guard), #620 (trailing
 * newline + trailing-';' strip).
 */
static char *normalize_newlines_to_semicolons(const char *script) {
	if (script == NULL)
		return NULL;

	size_t src_len = strlen(script);
	/* Worst case: every byte is a \r or \n, each gains a ';' prefix. */
	char *out = malloc(src_len * 2 + 1);
	if (out == NULL)
		return NULL;

	const char *src = script;
	char *dst = out;
	unsigned char prev_nonws = '\0';	/* last non-whitespace byte written */

	while (*src != '\0') {
		unsigned char c = (unsigned char)*src;

		if (c == '\r' || c == '\n') {
			/* UserTalk grammar requires ';' between statements at top level.
			 * Bare \r/\n are whitespace to the scanner, so multi-line scripts
			 * without explicit ';' fail to parse. The transition table above
			 * maps every (prev_kind, next_kind) pair to emit-or-suppress.
			 * classify_prev_full inspects the dst buffer to detect multi-byte
			 * binary-operator tokens (and / or / == / >= / <= / !=) so the
			 * BINOP row suppresses ';' for line continuations. */
			prev_kind_t pk = classify_prev_full(prev_nonws, out, dst);
			next_kind_t nk = classify_next(peek_next_real_byte(src));
			if (kEmitSep[pk][nk])
				*dst++ = ';';
			/* PR1 of REPL error context chain (2026-05-26 JES): emit CR
			 * for both CR and LF newlines. The UserTalk scanner only
			 * increments ctscanlines on chreturn (CR); bare LFs are
			 * silently consumed without advancing the line counter (see
			 * langscan.c parsepopchar). Normalizing both line endings to
			 * CR here makes error.location.line report the correct user-
			 * relative line for protocol clients without changing the
			 * statement-separator semantics callers already rely on. */
			*dst++ = '\r';
			src++;
			/* CRLF: silently consume the \n after an emitted CR so we
			 * don't produce two visible separators. Matches the pre-#628
			 * contract. */
			if (c == '\r' && *src == '\n')
				src++;
			/* prev_nonws is now ';' regardless of whether we emitted one.
			 * The newline acts as a statement-boundary signal even when the
			 * table suppressed the explicit ';' — subsequent guards key off
			 * "we just finished a statement", which is the truth either way.
			 * Don't try to mirror the emitted-byte history here. */
			prev_nonws = ';';
		} else {
			*dst++ = (char)c;
			src++;
			if (c != ' ' && c != '\t')
				prev_nonws = c;
		}
	}
	*dst = '\0';

	strip_trailing_whitespace_and_semicolons(out, &dst);
	return out;
}

/*
 * Build a wrapped script that brings variables into scope.
 *
 * Transforms:
 *	 user code
 * Into:
 *	 with system.temp.FrontierREPL.variables { user code }
 *
 * NOTE: We previously included target.set(@system.temp.FrontierREPL.variables)
 * but calling target.set() multiple times crashes due to a bug in target verb
 * implementation. See issue for target.set() double-call crash.
 */
static boolean build_wrapped_script(const char *script, Handle *hresult) {
	/* PR1 of REPL error context chain (2026-05-26 JES): use CR instead of
	 * LF as the line break in the wrapper. Bare LFs do not advance
	 * langscan.c ctscanlines, so a LF-prefixed wrapper would leave the
	 * user's first line of code on raw scan-line 1, defeating the input
	 * offset machinery that handle_script_eval installs. CRs increment
	 * the counter correctly. */
	static const char prefix[] =
		"with system.temp.FrontierREPL.variables {\r";
	static const char suffix[] = "\r}";

	/* Issue #624: normalize bare \r/\n to ;\r/;\n so they act as
	 * statement separators (UserTalk grammar uses ';' only). */
	char *normalized = normalize_newlines_to_semicolons(script);
	if (normalized == NULL)
		return false;

	size_t script_len = strlen(normalized);
	size_t prefix_len = sizeof(prefix) - 1;
	size_t suffix_len = sizeof(suffix) - 1;
	size_t total_len = prefix_len + script_len + suffix_len;

	Handle htext = nil;

	if (!newemptyhandle(&htext)) {
		free(normalized);
		return false;
	}

	if (!sethandlesize(htext, (long)total_len)) {
		disposehandle(htext);
		free(normalized);
		return false;
	}

	HLock(htext);
	char *p = (char *) *htext;
	memcpy(p, prefix, prefix_len);
	p += prefix_len;
	memcpy(p, normalized, script_len);
	p += script_len;
	memcpy(p, suffix, suffix_len);
	HUnlock(htext);
	free(normalized);

	*hresult = htext;
	return true;
}

boolean repl_eval_with_variables(
	const char *script,
	bigstring result,
	bigstring error_msg
) {
	Handle htext = nil;

	if (script == NULL || result == NULL || error_msg == NULL) {
		if (error_msg != NULL) {
			copyctopstring("Invalid parameters", error_msg);
		}
		return false;
	}

	/* Initialize result and error */
	setemptystring(result);
	setemptystring(error_msg);

	/* If variables table not initialized, fall back to regular eval */
	log_trace(LOG_COMP_GENERAL, "repl_eval_with_variables called, g_repl_variables_table=%p", (void*)g_repl_variables_table);
	if (g_repl_variables_table == nil) {
		log_warn(LOG_COMP_GENERAL, "REPL variables not initialized, using direct eval");

		/* Issue #624: normalize bare \r/\n to ;\r/;\n. */
		char *normalized = normalize_newlines_to_semicolons(script);
		if (normalized == NULL) {
			copyctopstring("Out of memory normalizing script", error_msg);
			return false;
		}

		size_t script_len = strlen(normalized);

		if (!newemptyhandle(&htext)) {
			free(normalized);
			copyctopstring("Out of memory allocating script handle", error_msg);
			return false;
		}

		if (!sethandlesize(htext, (long)script_len)) {
			disposehandle(htext);
			free(normalized);
			copyctopstring("Out of memory resizing script handle", error_msg);
			return false;
		}

		HLock(htext);
		memcpy(*htext, normalized, script_len);
		HUnlock(htext);
		free(normalized);

		return langrunhandletraperror(htext, result, error_msg);
	}

	/* Build wrapped script: with system.temp.FrontierREPL.variables { ... } */
	if (!build_wrapped_script(script, &htext)) {
		copyctopstring("Failed to wrap script", error_msg);
		return false;
	}

	/* Log the wrapped script for debugging */
	HLock(htext);
	log_trace(LOG_COMP_GENERAL, "Wrapped script (%ld bytes): %.*s",
			  GetHandleSize(htext), (int)GetHandleSize(htext), *htext);
	HUnlock(htext);

	log_debug(LOG_COMP_GENERAL, "Evaluating script with persistent variables");

	/*
	 * Execute the wrapped script.
	 *
	 * Flow:
	 * 1. `with` statement evaluates, creating a local table
	 * 2. Code runs, creating variables in the local table
	 * 3. `with` block ends, langpoplocalchain is called
	 * 4. Our callback syncs variables from the local table to persistent
	 * 5. Local table is disposed
	 *
	 * IMPORTANT: langrunhandletraperror CONSUMES htext.
	 */
	g_repl_eval_active = true;	/* Enable callback syncing */

	boolean ok = langrunhandletraperror(htext, result, error_msg);

	g_repl_eval_active = false;	 /* Disable callback syncing */

	log_trace(LOG_COMP_GENERAL, "Script evaluation %s", ok ? "succeeded" : "failed");

	return ok;
}

boolean repl_eval_with_variables_value(
	const char *script,
	tyvaluerecord *vreturned,
	bigstring error_msg
) {
	Handle htext = nil;

	if (script == NULL || vreturned == NULL || error_msg == NULL) {
		if (error_msg != NULL) {
			copyctopstring("Invalid parameters", error_msg);
		}
		return false;
	}

	/* Initialize result and error */
	initvalue(vreturned, novaluetype);
	setemptystring(error_msg);

	/* If variables table not initialized, fall back to regular eval */
	if (g_repl_variables_table == nil) {
		log_warn(LOG_COMP_GENERAL, "REPL variables not initialized, using direct eval");

		/* Issue #624: normalize bare \r/\n to ;\r/;\n. */
		char *normalized = normalize_newlines_to_semicolons(script);
		if (normalized == NULL) {
			copyctopstring("Out of memory normalizing script", error_msg);
			return false;
		}

		size_t script_len = strlen(normalized);

		if (!newemptyhandle(&htext)) {
			free(normalized);
			copyctopstring("Out of memory allocating script handle", error_msg);
			return false;
		}

		if (!sethandlesize(htext, (long)script_len)) {
			disposehandle(htext);
			free(normalized);
			copyctopstring("Out of memory resizing script handle", error_msg);
			return false;
		}

		HLock(htext);
		memcpy(*htext, normalized, script_len);
		HUnlock(htext);
		free(normalized);

		return langrunhandle_value(htext, vreturned);
	}

	/* Build wrapped script: with system.temp.FrontierREPL.variables { ... } */
	if (!build_wrapped_script(script, &htext)) {
		copyctopstring("Failed to wrap script", error_msg);
		return false;
	}

	/* Use langruntraperror which returns value and traps errors */
	g_repl_eval_active = true;

	boolean ok = langruntraperror(htext, vreturned, error_msg);

	g_repl_eval_active = false;

	/* Trap-and-return is a REPL UX feature: a typo at the REPL prompt
	 * shouldn't crash the loop. But for protocol script/eval callers (and
	 * yaml integration tests that go through this path), a compile error
	 * inside the `with system.temp.FrontierREPL.variables { ... }` wrapper
	 * must NOT be reported as success — the user's code never ran, and the
	 * wrapper block defaulting to a truthy value would mask the failure.
	 * If langtraperror populated error_msg, the script failed even if the
	 * wrapper itself evaluated. Surface that as overall failure.
	 *
	 * Issue #618 surfaced ~85 yaml tests that pass on develop only because
	 * this trap returns true. With this guard, those tests will fail and
	 * can be fixed against real behavior. */
	if (ok && !isemptystring(error_msg))
		ok = false;

	return ok;
}

void repl_set_focus(hdlhashtable htable) {
	/*
	 * Update system.temp.FrontierREPL.focus to point to the given table.
	 *
	 * We create an address value pointing to the table itself (with empty name)
	 * rather than trying to reconstruct the path. This avoids calling
	 * langexpandtodotparams which can corrupt interpreter state.
	 *
	 * We use setexemptaddressvalue instead of setaddressvalue to avoid
	 * interacting with the tmpstack, which can cause issues when called
	 * from REPL command handlers.
	 *
	 * CONTEXT GUARD NOTE (per docs/ARCHITECTURAL_ANTIPATTERNS.md):
	 * We do NOT need context guards here because:
	 * 1. We're not navigating through addresses (no langgetdotparams/langsymbolreference)
	 * 2. We're directly assigning a table handle we already have
	 * 3. setexemptaddressvalue() creates a simple address value without evaluation
	 * 4. hashassign() just stores the value - no interpreter state changes
	 * This function is called from REPL command handlers (/jump) outside of
	 * script evaluation context, so there's no mode stack to corrupt.
	 */
	log_debug(LOG_COMP_GENERAL, "repl_set_focus: htable=%p", (void*)htable);

	if (g_repl_table == nil) {
		log_warn(LOG_COMP_GENERAL, "repl_set_focus: REPL table not initialized");
		return;
	}

	/* Use roottable if htable is nil */
	if (htable == nil) {
		htable = roottable;
	}

	/* Create address value pointing to this table.
	 * Use setexemptaddressvalue to avoid tmpstack interaction.
	 * This creates a direct reference without evaluating paths. */
	tyvaluerecord focusval;
	if (!setexemptaddressvalue(htable, zerostring, &focusval)) {
		log_warn(LOG_COMP_GENERAL, "repl_set_focus: failed to create address");
		return;
	}

	/* Store in system.temp.FrontierREPL.focus */
	pushhashtable(g_repl_table);
	boolean ok = hashassign(namefocus, focusval);
	pophashtable();

	if (!ok) {
		disposevaluerecord(focusval, false);
		log_warn(LOG_COMP_GENERAL, "repl_set_focus: failed to assign focus");
	} else {
		log_debug(LOG_COMP_GENERAL, "repl_set_focus: focus set to table %p", (void*)htable);
	}
}
