/*
 * repl_output.c - Formats and displays REPL output (values, errors, prompts, help)
 *
 * Output routing: prompts go to stderr (keeps stdout clean for piping),
 * values and help go to stdout, errors go to stderr with logging.
 *
 * String Conversion: Uses coercetostring() for value display. Tables get
 * special handling (summary instead of full dump). See texthandletostring()
 * for heap-to-Pascal string conversion.
 *
 * Phase 4: Added async output support using linenoiseHide/Show for event loop.
 */

#include "repl_output.h"
#include "repl.h"                /* For repl_get_current_table(), REPL_PATH_MAX_LEN */
#include "repl_output_async.h"   /* For palette-aware async routing (#593) */
#include "linenoise.h"           /* For linenoiseHide/Show */
#include "../Common/headers/lang.h"
#include "../Common/headers/langexternal.h"
#include "../Common/headers/strings.h"
#include "../Common/headers/tablestructure.h"  /* For roottable */
#include "../Common/headers/logging.h"
#include "../Common/headers/op.h"               /* For hdloutlinerecord (PR2 source fetch) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

/* Forward declaration for ODB outline-text retrieval -- mirrors
 * debug_handler.c. opgetlangtext exists in Common/source but has no
 * public header. Used by the PR2 structured error renderer to fetch
 * source for named-script stack frames. */
extern boolean opgetlangtext(hdloutlinerecord, boolean, Handle *);	/* oplangtext.c */

/* Global linenoise state for async output (set by event loop) */
static struct linenoiseState *g_linenoisestate = NULL;

/* Mac Roman to Unicode mapping for bytes 0x80-0xFF.
 * Source: portable/paige_text_extractor.c kMacRomanToUnicode table.
 * Bytes 0x00-0x7F are identical to ASCII/UTF-8 and need no conversion. */
const uint16_t kMacRomanHighToUnicode[128] = {
	0x00C4,0x00C5,0x00C7,0x00C9,0x00D1,0x00D6,0x00DC,0x00E1, /* 0x80 */
	0x00E0,0x00E2,0x00E4,0x00E3,0x00E5,0x00E7,0x00E9,0x00E8,
	0x00EA,0x00EB,0x00ED,0x00EC,0x00EE,0x00EF,0x00F1,0x00F3, /* 0x90 */
	0x00F2,0x00F4,0x00F6,0x00F5,0x00FA,0x00F9,0x00FB,0x00FC,
	0x2020,0x00B0,0x00A2,0x00A3,0x00A7,0x2022,0x00B6,0x00DF, /* 0xA0 */
	0x00AE,0x00A9,0x2122,0x00B4,0x00A8,0x2260,0x00C6,0x00D8,
	0x221E,0x00B1,0x2264,0x2265,0x00A5,0x00B5,0x2202,0x2211, /* 0xB0 */
	0x220F,0x03C0,0x222B,0x00AA,0x00BA,0x03A9,0x00E6,0x00F8,
	0x00BF,0x00A1,0x00AC,0x221A,0x0192,0x2248,0x2206,0x00AB, /* 0xC0 */
	0x00BB,0x2026,0x00A0,0x00C0,0x00C3,0x00D5,0x0152,0x0153,
	0x2013,0x2014,0x201C,0x201D,0x2018,0x2019,0x00F7,0x25CA, /* 0xD0 */
	0x00FF,0x0178,0x2044,0x20AC,0x2039,0x203A,0xFB01,0xFB02,
	0x2021,0x00B7,0x201A,0x201E,0x2030,0x00C2,0x00CA,0x00C1, /* 0xE0 */
	0x00CB,0x00C8,0x00CD,0x00CE,0x00CF,0x00CC,0x00D3,0x00D4,
	0xF8FF,0x00D2,0x00DA,0x00DB,0x00D9,0x0131,0x02C6,0x02DC, /* 0xF0 */
	0x00AF,0x02D8,0x02D9,0x02DA,0x00B8,0x02DD,0x02DB,0x02C7
};

/* Write a Unicode code point as UTF-8 to a stream. */
void putc_utf8(uint16_t cp, FILE *stream) {
	if (cp < 0x80) {
		putc(cp, stream);
	} else if (cp < 0x800) {
		putc(0xC0 | (cp >> 6), stream);
		putc(0x80 | (cp & 0x3F), stream);
	} else {
		putc(0xE0 | (cp >> 12), stream);
		putc(0x80 | ((cp >> 6) & 0x3F), stream);
		putc(0x80 | (cp & 0x3F), stream);
	}
}

/* Helper: Output string to FILE, converting CR (Mac) to LF (Unix)
 * and Mac Roman high bytes (0x80-0xFF) to UTF-8.
 *
 * Frontier internally uses CR for line endings and Mac Roman encoding.
 * Modern terminals expect LF line endings and UTF-8 encoding. */
static void fputs_cr_to_lf(const char *str, size_t len, FILE *stream) {
	for (size_t i = 0; i < len; i++) {
		unsigned char ch = (unsigned char)str[i];
		if (ch == '\r') {
			putc('\n', stream);
		} else if (ch < 0x80) {
			putc(ch, stream);
		} else {
			putc_utf8(kMacRomanHighToUnicode[ch - 0x80], stream);
		}
	}
}

/* Get terminal width, defaulting to 80 columns if unavailable. */
static int get_terminal_width(void) {
	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
		return ws.ws_col;
	}
	return 80;
}

/* Returns true if the value type is a scalar that can be displayed inline. */
static boolean is_scalar_valuetype(tyvaluetype vtype) {
	switch (vtype) {
		case charvaluetype:
		case intvaluetype:
		case longvaluetype:
		case booleanvaluetype:
		case stringvaluetype:
		case addressvaluetype:
		case doublevaluetype:
		case singlevaluetype:
		case fixedvaluetype:
		case datevaluetype:
		case ostypevaluetype:
		case directionvaluetype:
		case pointvaluetype:
		case rectvaluetype:
		case rgbvaluetype:
			return true;
		default:
			return false;
	}
}

/* Display welcome message at REPL startup */
void repl_output_welcome(void) {
	fputs("Frontier REPL - Interactive UserTalk Environment\n", stdout);
	fputs("Type /help for commands, /exit to quit\n\n", stdout);
	fflush(stdout);
}

/* Display goodbye message at REPL exit */
void repl_output_goodbye(void) {
	fputs("\nGoodbye!\n", stdout);
	fflush(stdout);
}

/* Display prompt (e.g., "[root]> ")
 * system_root_name: name of loaded system root, or NULL if none
 */
void repl_output_prompt(const char *system_root_name) {
	/* Use stderr for prompts to keep stdout clean for piping */
	if (system_root_name != NULL && strlen(system_root_name) > 0) {
		fprintf(stderr, "[%s]> ", system_root_name);
	} else {
		fputs("[root]> ", stderr);
	}
	fflush(stderr);
}

/* Counts items in a hash table by traversing the sorted link list. */
static long count_hashtable_items(hdlhashtable htable) {
	long count = 0;
	hdlhashnode nomad;

	if (htable == nil) {
		return 0;
	}

	nomad = (**htable).hfirstsort;

	while (nomad != nil) {
		count++;
		nomad = (**nomad).sortedlink;
	}

	return count;
}

/* Displays a tyvaluerecord, with special formatting for tables and strings.
 * Note: This function does NOT dispose the value - caller retains ownership.
 * However, calling coercetostring mutates the value to string type. */
void repl_output_value(tyvaluerecord *val) {
	Handle hstring;
	long len;
	boolean was_string;

	if (val == nil) {
		return;
	}

	/* Special handling for nil - don't print anything (like Python REPL) */
	if (val->valuetype == novaluetype) {
		return;
	}

	/* Special handling for tables - show summary instead of full dump */
	if (val->valuetype == externalvaluetype) {
		hdlexternalvariable hv = (hdlexternalvariable)val->data.externalvalue;
		if (hv != nil) {
			/* Check if this is a table type */
			if ((**hv).id == idtableprocessor) {
				hdlhashtable htable = (hdlhashtable)(**hv).variabledata;
				long count = count_hashtable_items(htable);
				printf("[table with %ld item%s]\n", count, (count == 1) ? "" : "s");
				fflush(stdout);
				return;
			}
		}
	}

	/* Remember if this was originally a string (for quoting) */
	was_string = (val->valuetype == stringvaluetype);

	/* Coerce to string for display - mutates the value */
	if (!coercetostring(val)) {
		printf("[unable to display value of type %d]\n", val->valuetype);
		fflush(stdout);
		return;
	}

	/* Get string handle directly - don't copy to bigstring which truncates at 255 */
	hstring = val->data.stringvalue;
	len = gethandlesize(hstring);

	/* Print value - add quotes for original string types, convert CR to LF for terminal */
	if (was_string) {
		putchar('"');
		fputs_cr_to_lf((const char *)*hstring, len, stdout);
		putchar('"');
	} else {
		fputs_cr_to_lf((const char *)*hstring, len, stdout);
	}
	putchar('\n');
	fflush(stdout);
}

/* Displays evaluation result as a string (empty results are suppressed). */
void repl_output_result(bigstring result) {
	if (result == nil) {
		return;
	}

	/* Don't display empty results (like Python REPL for None) */
	size_t len = stringlength(result);
	if (len == 0) {
		return;
	}

	/* Print result, converting CR to LF for terminal display */
	fputs_cr_to_lf((const char *)stringbaseaddress(result), len, stdout);
	putchar('\n');
	fflush(stdout);
}

/* Displays error message to stderr and logs it (terminal UI output). */
void repl_output_error(const char *error_msg) {
	if (error_msg == NULL) {
		log_error(LOG_COMP_GENERAL, "Unknown error");
		fprintf(stderr, "Error: (unknown error)\n");
	} else {
		size_t len = strlen(error_msg);
		log_error(LOG_COMP_GENERAL, "%s", error_msg);
		fputs("Error: ", stderr);
		fputs_cr_to_lf(error_msg, len, stderr);
		putc('\n', stderr);
	}
	fflush(stderr);
}

/* Displays the /help command output with available commands and persistence info. */
void repl_output_help(void) {
	fputs("Available commands:\n", stdout);
	fputs("  /clear             Clear variables and reset focus to root\n", stdout);
	fputs("  /exit              Exit the REPL\n", stdout);
	fputs("  /help              Show this help message\n", stdout);
	fputs("  /jump [path]       Navigate to a table (like cd)\n", stdout);
	fputs("  /keycodes          Debug terminal key sequences\n", stdout);
	fputs("  /list [path]       List contents of a table\n", stdout);
	fputs("\n", stdout);
	fputs("/jump - Navigate to a table (like cd in a shell):\n", stdout);
	fputs("  /jump                     Return to root\n", stdout);
	fputs("  /jump system              Navigate to system table\n", stdout);
	fputs("  /jump user.inetd          Navigate to nested table\n", stdout);
	fputs("  /jump ..                  Go to parent table\n", stdout);
	fputs("  /jump fileMenu            Navigate via system.paths\n", stdout);
	fputs("  /jump parentOf(@user)     Evaluate expression for address\n", stdout);
	fputs("  /jump system.verbs[1]     Navigate to 1st item (1-based index)\n", stdout);
	fputs("\n", stdout);
	fputs("/list - List contents of a table:\n", stdout);
	fputs("  /list                     List current table\n", stdout);
	fputs("  /list system.verbs        List specific table by path\n", stdout);
	fputs("  /list fileMenu            List via system.paths\n", stdout);
	fputs("  /list parentOf(fileMenu)  Evaluate expression for table\n", stdout);
	fputs("  /list system.verbs[1]     List/show 1st item (1-based index)\n", stdout);
	fputs("  /list files               Relative path (after /jump)\n", stdout);
	fputs("\n", stdout);
	fputs("QuickScript Model - Variable Persistence:\n", stdout);
	fputs("  Local variables (x = 5) don't persist between evaluations\n", stdout);
	fputs("  For persistence, use explicit database paths:\n", stdout);
	fputs("    system.temp.x = 42       (session-scoped)\n", stdout);
	fputs("    workspace.x = 42         (saved to database)\n", stdout);
	fputs("\n", stdout);
	fputs("Multi-line input: Coming in Phase 2\n", stdout);
	fputs("\n", stdout);
	fputs("Kernel verbs (call from any UserTalk expression):\n", stdout);
	fputs("  repl.syncScan()    scan the ut-sync dir and create ODB nodes for any orphan .ut files\n", stdout);
	fflush(stdout);
}

/* Formats a value as a compact string for /vars output (e.g., "42", "[table with 3 items]"). */
static void format_value_summary(tyvaluerecord *val, char *buffer, size_t bufsize) {
	bigstring bs;
	tyvaluerecord val_copy;

	if (val == nil || buffer == nil || bufsize == 0) {
		return;
	}

	switch (val->valuetype) {
		case novaluetype:
			snprintf(buffer, bufsize, "nil");
			break;

		case booleanvaluetype:
			snprintf(buffer, bufsize, "%s", val->data.flvalue ? "true" : "false");
			break;

		case longvaluetype:
			snprintf(buffer, bufsize, "%lld", (long long)val->data.longvalue);
			break;

		case stringvaluetype:
			texthandletostring(val->data.stringvalue, bs);
			snprintf(buffer, bufsize, "\"%.*s\"",
				(int)stringlength(bs), stringbaseaddress(bs));
			break;

		case externalvaluetype: {
			hdlexternalvariable hv = (hdlexternalvariable)val->data.externalvalue;
			if (hv != nil && (**hv).id == idtableprocessor) {
				hdlhashtable htable = (hdlhashtable)(**hv).variabledata;
				long count = count_hashtable_items(htable);
				snprintf(buffer, bufsize, "[table with %ld item%s]",
					count, (count == 1) ? "" : "s");
			} else {
				snprintf(buffer, bufsize, "[external]");
			}
			break;
		}

		default:
			/* Use coercetostring for other types */
			val_copy = *val;
			if (coercetostring(&val_copy)) {
				texthandletostring(val_copy.data.stringvalue, bs);
				snprintf(buffer, bufsize, "%.*s",
					(int)stringlength(bs), stringbaseaddress(bs));
				disposevaluerecord(val_copy, false);
			} else {
				snprintf(buffer, bufsize, "[type %d]", val->valuetype);
			}
			break;
	}
}

/* Displays all variables in a workspace table with their values. */
void repl_output_vars(hdlhashtable workspace) {
	hdlhashnode nomad;
	long count;
	bigstring name;
	tyvaluerecord val;
	char value_buf[256];

	if (workspace == nil) {
		fputs("(empty)\n", stdout);
		fflush(stdout);
		return;
	}

	count = count_hashtable_items(workspace);

	if (count == 0) {
		fputs("(empty)\n", stdout);
		fflush(stdout);
		return;
	}

	printf("Workspace variables (%ld):\n", count);

	/* Iterate workspace hash table (sorted order) */
	nomad = (**workspace).hfirstsort;

	while (nomad != nil) {
		gethashkey(nomad, name);
		val = (**nomad).val;

		/* Format value summary */
		format_value_summary(&val, value_buf, sizeof(value_buf));

		/* Print name = value */
		printf("  %.*s = %s\n",
			(int)stringlength(name), stringbaseaddress(name),
			value_buf);

		nomad = (**nomad).sortedlink;
	}

	fflush(stdout);
}

/* --- /list Command Support --- */

/* Display contents of a table (for /list command).
 * If htable is nil, displays the current REPL table.
 * path_label is displayed as a header (e.g., "user.prefs:"). Can be NULL.
 */
void repl_output_list(hdlhashtable htable, const char *path_label) {
	if (htable == nil) {
		htable = repl_get_current_table();
	}
	hdlhashnode nomad;
	long count;

	/* Check if we have a current table */
	if (htable == nil) {
		fputs("(no current table)\n", stdout);
		fflush(stdout);
		return;
	}

	/* Display path header */
	if (path_label != NULL && path_label[0] != '\0') {
		printf("%s:\n", path_label);
	} else {
		/* Use current path if no label provided */
		const char *current_path = repl_get_current_path();
		if (current_path != NULL && current_path[0] != '\0') {
			printf("%s:\n", current_path);
		} else {
			printf("root:\n");
		}
	}

	count = count_hashtable_items(htable);

	if (count == 0) {
		fputs("(empty table)\n", stdout);
		fflush(stdout);
		return;
	}

	/* First pass: calculate column widths for alignment */
	size_t max_name_len = 0;
	size_t max_type_len = 0;

	nomad = (**htable).hfirstsort;

	while (nomad != nil) {
		bigstring name;
		gethashkey(nomad, name);
		size_t name_len = stringlength(name);
		if (name_len > max_name_len) {
			max_name_len = name_len;
		}

		/* Get type string */
		bigstring type_str;
		tyvaluerecord val = (**nomad).val;

		if (val.valuetype == externalvaluetype) {
			langexternaltypestring((hdlexternalvariable)val.data.externalvalue, type_str);
		} else {
			langgettypestring(val.valuetype, type_str);
		}

		size_t type_len = stringlength(type_str);
		if (type_len > max_type_len) {
			max_type_len = type_len;
		}

		nomad = (**nomad).sortedlink;
	}

	/* Get terminal width for value truncation */
	int term_width = get_terminal_width();

	/* Second pass: print entries with alignment */
	nomad = (**htable).hfirstsort;

	while (nomad != nil) {
		bigstring name;
		gethashkey(nomad, name);

		/* Get type string */
		bigstring type_str;
		tyvaluerecord val = (**nomad).val;

		if (val.valuetype == externalvaluetype) {
			langexternaltypestring((hdlexternalvariable)val.data.externalvalue, type_str);
		} else {
			langgettypestring(val.valuetype, type_str);
		}

		/* Print: name : type */
		int prefix_len = printf("  %-*.*s : %-*.*s",
			(int)max_name_len,
			(int)stringlength(name), stringbaseaddress(name),
			(int)max_type_len,
			(int)stringlength(type_str), stringbaseaddress(type_str));

		/* Get display string - either external info or scalar value */
		if (val.valuetype == externalvaluetype) {
			/* External types: show "N items" or "on disk" */
			bigstring display_str;
			setemptystring(display_str);
			langexternalgetdisplaystring((hdlexternalvariable)val.data.externalvalue, display_str);

			if (stringlength(display_str) > 0) {
				printf(" : %.*s", (int)stringlength(display_str), stringbaseaddress(display_str));
			}
		} else if (is_scalar_valuetype(val.valuetype)) {
			/* Scalar types: show value inline, truncated to fit terminal */
			char value_buf[REPL_PATH_MAX_LEN];
			format_value_summary(&val, value_buf, sizeof(value_buf));

			/* Calculate available space: terminal - prefix - " : " - "..." margin */
			int available = term_width - prefix_len - 3 - 3;
			if (available < 10) available = 10;  /* Minimum display width */

			size_t value_len = strlen(value_buf);
			if ((int)value_len <= available) {
				printf(" : %s", value_buf);
			} else {
				/* Truncate with ellipsis */
				printf(" : %.*s...", available, value_buf);
			}
		}

		printf("\n");

		nomad = (**nomad).sortedlink;
	}

	fflush(stdout);
}

/* Display a single scalar value (for /list with index syntax).
 * Used when /list resolves to a non-table value via [n] indexing.
 * Output format: path = value (type)
 */
void repl_output_single_value(const char *path_label, tyvaluerecord *val) {
	if (val == NULL) return;

	/* Get type string (safe to read directly - type info is inline, not a handle) */
	bigstring type_str;
	if (val->valuetype == externalvaluetype) {
		langexternaltypestring((hdlexternalvariable)val->data.externalvalue, type_str);
	} else {
		langgettypestring(val->valuetype, type_str);
	}

	/* Deep-copy the value so we own all handle-based data (strings, etc.).
	 * The input val is a borrowed reference from the ODB; using a deep copy
	 * avoids any risk of use-after-free if the ODB mutates during display. */
	char value_buf[REPL_PATH_MAX_LEN];
	tyvaluerecord val_copy;
	boolean owns_copy = copyvaluerecord(*val, &val_copy);

	if (!owns_copy) {
		/* copyvaluerecord failed (memory allocation) - fall back to shallow read.
		 * This is safe in practice because no ODB mutations occur between
		 * resolve and display, but prefer the deep copy when possible. */
		val_copy = *val;
	}

	if (val_copy.valuetype == externalvaluetype) {
		bigstring display_str;
		setemptystring(display_str);
		langexternalgetdisplaystring((hdlexternalvariable)val_copy.data.externalvalue, display_str);
		if (stringlength(display_str) > 0) {
			snprintf(value_buf, sizeof(value_buf), "%.*s",
				(int)stringlength(display_str), stringbaseaddress(display_str));
		} else {
			snprintf(value_buf, sizeof(value_buf), "[external]");
		}
	} else if (is_scalar_valuetype(val_copy.valuetype)) {
		format_value_summary(&val_copy, value_buf, sizeof(value_buf));
	} else {
		snprintf(value_buf, sizeof(value_buf), "[type %d]", val_copy.valuetype);
	}

	/* Print: path = value (type) */
	if (path_label != NULL && path_label[0] != '\0') {
		printf("%s = %s (%.*s)\n", path_label, value_buf,
			(int)stringlength(type_str), stringbaseaddress(type_str));
	} else {
		printf("%s (%.*s)\n", value_buf,
			(int)stringlength(type_str), stringbaseaddress(type_str));
	}
	fflush(stdout);

	/* Dispose the deep copy to avoid leaking handle-based data */
	if (owns_copy) {
		disposevaluerecord(val_copy, false);
	}
}

/* --- Event Loop Support (Phase 4) --- */

/* Set the active linenoise state for async output */
void repl_set_active_linenoisestate(struct linenoiseState *ls) {
	g_linenoisestate = ls;
}

/* Display async output while user is typing at prompt.
 *
 * Routing rules (issue #593):
 *   - Palette modal active: route through repl_async_output_emit, which
 *     appends to the registered scrollback pane. Bytes do NOT touch
 *     stdout — that would corrupt the compositor's framebuffer.
 *   - Linenoise editing active and palette inactive: hide prompt, write
 *     CR-to-LF and Mac-Roman-to-UTF-8 converted bytes to stdout, restore
 *     prompt. (Legacy event-loop behaviour.)
 *   - Neither active: write directly to stdout.
 *
 * The palette-active branch bypasses the linenoise hide/show dance
 * entirely — linenoise is suspended for the duration of the modal
 * (see run_palette_modal: linenoiseEditStop on entry,
 * linenoiseEditStart on exit), so g_linenoisestate is NULL anyway.
 */
void repl_async_output(const char *message) {
	if (message == NULL) {
		return;
	}

	/* Palette-active path: delegate to the router so the bytes land in
	 * the scrollback ring, not on stdout. The router's append walks the
	 * ring under its own mutex; no further coordination needed here. */
	if (repl_async_output_palette_active()) {
		size_t len = strlen(message);
		repl_async_output_emit(message, len);
		/* Append a trailing newline so the message renders as a
		 * standalone scrollback line — matches the legacy behaviour
		 * (stdout path appended '\n' below). */
		repl_async_output_emit("\n", 1);
		return;
	}

	size_t len = strlen(message);

	if (g_linenoisestate != NULL) {
		/* In event loop mode - hide prompt, print, restore */
		linenoiseHide(g_linenoisestate);
		fputs_cr_to_lf(message, len, stdout);
		putchar('\n');
		fflush(stdout);
		linenoiseShow(g_linenoisestate);
	} else {
		/* Not in event loop mode - just print directly */
		fputs_cr_to_lf(message, len, stdout);
		putchar('\n');
		fflush(stdout);
	}
}

/* ========================================================================
 * Structured error renderer (PR2 of REPL error context chain)
 *
 * Reads the snapshot PR1 captures via langseterrorcallbackline (exposed
 * through langgetlasterror / langgetstackdepth / langgetstackframe) and
 * builds a rich error display with per-frame source-context windows.
 *
 * ANSI styling: bold + underline of the failing token in TTY mode, with
 * a ">>>" line prefix + "^^^" caret line in plain-text mode. TTY check
 * is isatty(STDERR_FILENO) with FRONTIER_FORCE_COLOR=1 as override.
 *
 * Source fetching: the outermost <eval> frame uses the caller-supplied
 * eval_source_text (the line the user just typed); named-script frames
 * resolve via the refcon-as-hdlhashnode (the encoding op_handler.c
 * uses) and opgetlangtext, mirroring debug_handler.c::handle_debug_getsource.
 * ======================================================================== */

#define ERR_CONTEXT_BEFORE 7
#define ERR_CONTEXT_AFTER  7

/* ANSI escape sequences -- kept minimal. Bold marks the failing line in
 * the source window; underline brackets the failing token; reset closes
 * each marker. Codes are stored as string constants so the compiler can
 * coalesce them. */
#define ANSI_BOLD       "\x1b[1m"
#define ANSI_UNDERLINE  "\x1b[4m"
#define ANSI_RESET      "\x1b[0m"

/*
 * TTY-vs-plain decision. FRONTIER_FORCE_COLOR=1 forces ANSI rendering
 * even when stderr is not a TTY; useful when piping the REPL into a
 * pager that interprets ANSI, and lets the integration test runner
 * exercise the TTY branch from a pipe-driven REPL spawn.
 *
 * Any other value (including "0") falls through to the isatty check, so
 * unset and "0" are equivalent (no force; isatty wins).
 */
static boolean stderr_supports_ansi(void) {
	const char *force = getenv("FRONTIER_FORCE_COLOR");
	if (force != NULL && force[0] == '1' && force[1] == '\0')
		return true;
	return isatty(STDERR_FILENO) ? true : false;
}

/*
 * Derive a printable script name from an error-frame errorrefcon. Thin
 * wrapper around the shared kernel helper langscriptnamefromrefcon --
 * copies the resulting Pascal bigstring into the caller's C buffer with
 * size-aware clamping. See lang.c::langscriptnamefromrefcon for the
 * refcon semantics shared with op_handler.c and langevaluate.c.
 */
static void script_name_for_frame(long refcon, char *out, size_t outlen) {
	bigstring bsname;

	if (outlen == 0) return;

	langscriptnamefromrefcon(refcon, bsname);

	size_t n = (size_t) bsname[0];
	if (n >= outlen) n = outlen - 1;
	memcpy(out, (const char *)(bsname + 1), n);
	out[n] = '\0';
}

/*
 * Resolve a named-script frame's hdlhashnode (the errorrefcon a
 * non-<eval> frame carries) to its source text. The node already points
 * at the script's value record, so we skip the path-string round-trip
 * the previous implementation needed and go straight to the outline.
 * This mirrors debug_handler.c::handle_debug_getsource's outline-fetch
 * tail (after it has resolved the path) but starts from the node, not
 * a path string.
 *
 * NOTE: requires the caller to hold the GIL (we touch the ODB). All
 * current invocation paths (process_line in repl.c) already do.
 *
 * Returns a malloc'd null-terminated string (caller frees) with CR
 * separators normalized to LF, or NULL on any failure. We choose to
 * normalize here so the line splitter has a single separator to chase.
 *
 * Only handles in-memory outlines -- the headless REPL keeps scripts
 * hot once loaded, and we don't want to take a load-from-DB dependency
 * in the error path. If the outline isn't in memory the renderer skips
 * the source window for that frame.
 */
static char *fetch_script_source_from_node(hdlhashnode hnode) {
	if (hnode == nil)
		return NULL;

	tyvaluerecord val = (**hnode).val;
	if (val.valuetype != externalvaluetype)
		return NULL;

	hdlexternalvariable hv = (hdlexternalvariable)val.data.externalvalue;
	if (hv == nil)
		return NULL;

	if (!(**hv).flinmemory)
		return NULL;

	hdloutlinerecord houtline = (hdloutlinerecord)(**hv).variabledata;
	if (houtline == nil)
		return NULL;

	Handle htext = nil;
	opgetlangtext(houtline, false, &htext);
	if (htext == nil)
		return NULL;

	long textlen = gethandlesize(htext);
	char *out = (char *) malloc((size_t)textlen + 1);
	if (out == NULL) {
		disposehandle(htext);
		return NULL;
	}
	memcpy(out, *htext, (size_t)textlen);
	out[textlen] = '\0';
	disposehandle(htext);

	/* Normalize CR to LF so the line splitter has a single separator.
	 * opgetlangtext returns CR-separated text (Frontier outline
	 * convention); the splitter below treats LF as the boundary. */
	for (long i = 0; i < textlen; i++)
		if (out[i] == '\r')
			out[i] = '\n';

	return out;
}

/*
 * Split a NUL-terminated source string into an array of NUL-terminated
 * lines. Caller must free *out_lines AND free each entry it owns -- we
 * allocate the outer array and a single backing buffer; entries point
 * into the backing buffer (so free(backing); free(out_lines)). To keep
 * the caller simple we return both via out params: out_buf is the
 * backing string copy, out_lines is the array of char* pointers into it.
 *
 * Returns true on success, false on OOM or empty input. Empty input
 * yields a single empty line so the renderer always has something to
 * point at.
 */
static boolean split_into_lines(const char *src,
                                 char **out_buf,
                                 char ***out_lines,
                                 int *out_count) {
	*out_buf = NULL;
	*out_lines = NULL;
	*out_count = 0;

	if (src == NULL)
		src = "";

	size_t srclen = strlen(src);
	char *buf = (char *) malloc(srclen + 1);
	if (buf == NULL)
		return false;
	memcpy(buf, src, srclen);
	buf[srclen] = '\0';

	/* First pass: count lines (always at least 1). */
	int count = 1;
	for (size_t i = 0; i < srclen; i++)
		if (buf[i] == '\n')
			count++;

	char **lines = (char **) malloc((size_t)count * sizeof(char *));
	if (lines == NULL) {
		free(buf);
		return false;
	}

	int ix = 0;
	lines[ix++] = buf;
	for (size_t i = 0; i < srclen; i++) {
		if (buf[i] == '\n') {
			buf[i] = '\0';
			if (ix < count)
				lines[ix++] = buf + i + 1;
		}
	}

	/* If the input ends with a trailing newline, the last "line" is
	 * empty; that's intentional -- we keep it so the splitter is
	 * faithful to the input. */

	*out_buf = buf;
	*out_lines = lines;
	*out_count = ix;
	return true;
}

/* Visual column width for tab expansion. Tabs advance to the next
 * multiple of TAB_WIDTH in visual column space. Eight matches the
 * canonical Frontier outline indent and the most common terminal
 * default. */
#define TAB_WIDTH 8

/*
 * Returns true when byte b is safe to emit to a terminal as-is. Control
 * bytes (< 0x20, except tab which write_source_line expands) and DEL
 * are unsafe — they can fire ANSI / OSC sequences (title spoof, OSC 52
 * clipboard write, OSC 8 hyperlink hijack, cursor reposition). Bytes
 * 0x80-0x9F are also rejected before MacRoman translation since several
 * map to format-control code points. The renderer substitutes '?' for
 * any byte that fails this check; '?' preserves single-byte alignment
 * (same width as the rejected byte in a fixed-width font) and matches
 * the debugger-style sanitization convention.
 *
 * Tab (0x09) is allowed here because write_source_line expands tabs to
 * spaces before emitting them; the visual-column math depends on tab
 * being preserved through this filter.
 */
static boolean is_safe_display_byte(unsigned char b) {
	if (b == '\t')
		return true;
	if (b < 0x20)
		return false;	/* C0 controls including ESC, BEL, BS */
	if (b == 0x7F)
		return false;	/* DEL */
	if (b >= 0x80 && b <= 0x9F)
		return false;	/* C1 controls (pre-MacRoman) */
	return true;
}

/*
 * Compute the visual column that a given byte offset reaches when the
 * line is rendered with tabs expanded to TAB_WIDTH-stops. start_col is
 * the visual column at which byte 0 of the line is emitted (0 for the
 * caret-line math; the gutter prefix is added separately). Bytes that
 * fail is_safe_display_byte() contribute one column (the '?' substitute).
 *
 * Note: this is a byte-to-column map, not a grapheme-to-column map. UTF-8
 * multi-byte sequences (from MacRoman 0x80-0xFF) render as one display
 * column per source byte in our renderer, which matches what most
 * monospace terminals do for the BMP characters in the MacRoman table.
 * Combining marks would break this assumption, but MacRoman has none.
 */
static int visual_column_for_byte(const char *line, size_t byte_offset,
                                  int start_col) {
	int col = start_col;
	for (size_t i = 0; i < byte_offset; i++) {
		unsigned char ch = (unsigned char)line[i];
		if (ch == '\t') {
			col += TAB_WIDTH - (col % TAB_WIDTH);
		} else {
			col++;
		}
	}
	return col;
}

/* Write a single source line to stderr with CR->LF and Mac Roman->UTF-8
 * conversion (same translation fputs_cr_to_lf does). The input slice is
 * a single line already, so we just translate per-byte.
 *
 * Tabs are expanded to spaces using TAB_WIDTH-column tab stops, relative
 * to the supplied start_col (the visual column at which the first byte
 * lands). This keeps caret alignment consistent with what the user sees
 * on screen.
 *
 * Control bytes that could fire terminal sequences (ESC, BEL, OSC, etc.)
 * are replaced with '?' via is_safe_display_byte(). The bold/underline
 * envelope around the failing token therefore can't be hijacked by
 * source bytes coming in from a hostile script.
 *
 * Returns the visual column reached after the last byte is emitted.
 */
static int write_source_line(const char *line, size_t len, int start_col) {
	int col = start_col;
	for (size_t i = 0; i < len; i++) {
		unsigned char ch = (unsigned char)line[i];
		if (ch == '\r' || ch == '\n')
			continue;	/* shouldn't occur; line is pre-split */
		if (ch == '\t') {
			int spaces = TAB_WIDTH - (col % TAB_WIDTH);
			for (int s = 0; s < spaces; s++)
				putc(' ', stderr);
			col += spaces;
			continue;
		}
		if (!is_safe_display_byte(ch)) {
			putc('?', stderr);
			col++;
			continue;
		}
		if (ch < 0x80) {
			putc(ch, stderr);
		} else {
			putc_utf8(kMacRomanHighToUnicode[ch - 0x80], stderr);
		}
		col++;
	}
	return col;
}

/* Width (in decimal digits) of a 1-origin line number. */
static int digit_width(int n) {
	if (n < 10)   return 1;
	if (n < 100)  return 2;
	if (n < 1000) return 3;
	int w = 0;
	while (n > 0) { w++; n /= 10; }
	return w;
}

/*
 * Render a source-context window for one stack frame.
 *
 *   lines / count          - all source lines for this frame (1-origin
 *                            indexing via lines[error_line-1])
 *   error_line             - 1-origin line number where the error fired
 *   token_start, token_end - 0-origin column range of the failing token
 *                            on error_line. If both are 0 (no token
 *                            available) we fall back to underlining /
 *                            caret-marking the whole line.
 *   use_ansi               - true for TTY rendering (bold/underline);
 *                            false for the plain-text fallback (">>>"
 *                            prefix + "^^^" caret line)
 */
static void render_source_window(char **lines, int count,
                                  int error_line,
                                  int token_start, int token_end,
                                  boolean use_ansi,
                                  int *out_actual_line) {
	if (count <= 0) {
		if (out_actual_line != NULL)
			*out_actual_line = error_line;
		return;
	}

	/* Clamp error_line into the source range. The REPL wrapper adds a
	 * synthetic closing brace line; an EOF-terminated parse error
	 * reports that synthetic line (raw line = wrapper_prefix +
	 * user_lines + 1), and after subtracting the eval input offset
	 * the result can sit one past the last user line. Clamp to the
	 * last actual line so the user sees their own source rather than
	 * an empty window for these EOF-shaped failures.
	 *
	 * The clamped value is returned via out_actual_line so the caller
	 * can use it in the frame header instead of the raw runtime value;
	 * otherwise the header reads "line 2" while the window highlights
	 * line 1. */
	if (error_line < 1)
		error_line = 1;
	if (error_line > count)
		error_line = count;
	if (out_actual_line != NULL)
		*out_actual_line = error_line;

	int first = error_line - ERR_CONTEXT_BEFORE;
	if (first < 1) first = 1;
	int last = error_line + ERR_CONTEXT_AFTER;
	if (last > count) last = count;

	int gutter_w = digit_width(last);

	for (int ln = first; ln <= last; ln++) {
		const char *src = lines[ln - 1];
		size_t srclen = strlen(src);
		boolean is_error_line = (ln == error_line);

		/* Marker column (plain-text mode): ">>>" for the error line,
		 * "   " otherwise. TTY mode uses bold on the error line and
		 * blanks the marker column to keep the gutter alignment. */
		if (!use_ansi) {
			fputs(is_error_line ? ">>> " : "    ", stderr);
		} else {
			fputs("    ", stderr);
		}

		fprintf(stderr, "%*d | ", gutter_w, ln);

		if (is_error_line) {
			if (use_ansi) {
				/* Bold the entire line for visibility, with underline
				 * over the token range. Token range is clamped to the
				 * line length so we never run off the end.
				 *
				 * Order matters: clamp ts up to srclen BEFORE clamping
				 * te, otherwise a tokenstart past srclen falls through
				 * to write_source_line(src, ts) and reads past the end
				 * of this line into the next packed line in the
				 * backing buffer. */
				int ts = token_start;
				int te = token_end;
				if (ts < 0) ts = 0;
				if (ts > (int)srclen) ts = (int)srclen;
				if (te > (int)srclen) te = (int)srclen;
				if (te < ts) te = ts;

				/* If we have no actual token range (PR1 sets both to 0
				 * when nothing useful is known), underline the whole
				 * line as a conservative fallback. */
				if (ts == 0 && te == 0) {
					ts = 0;
					te = (int)srclen;
				}

				fputs(ANSI_BOLD, stderr);
				int col = 0;
				if (ts > 0)
					col = write_source_line(src, (size_t)ts, col);
				fputs(ANSI_UNDERLINE, stderr);
				col = write_source_line(src + ts, (size_t)(te - ts), col);
				fputs(ANSI_RESET, stderr);
				fputs(ANSI_BOLD, stderr);
				if ((size_t)te < srclen)
					(void) write_source_line(src + te, srclen - (size_t)te, col);
				fputs(ANSI_RESET, stderr);
				putc('\n', stderr);
			} else {
				(void) write_source_line(src, srclen, 0);
				putc('\n', stderr);

				/* Caret line: leading spaces sized to match the gutter
				 * ("    NNN | ") plus the visual column of the token
				 * start, then "^^^" spanning the visual width of the
				 * token range. We use visual columns (not byte offsets)
				 * so tabs in the source line don't desync the caret. */
				int prefix_spaces = 4 + gutter_w + 3;	/* marker + gutter + " | " */
				int ts = token_start;
				int te = token_end;
				if (ts < 0) ts = 0;
				if (ts > (int)srclen) ts = (int)srclen;
				if (te > (int)srclen) te = (int)srclen;
				if (te < ts) te = ts;

				int ts_col = visual_column_for_byte(src, (size_t)ts, 0);
				int te_col;
				int caret_count;
				if (ts == 0 && te == 0) {
					/* No token info -- caret the whole line. */
					te_col = visual_column_for_byte(src, srclen, 0);
					caret_count = te_col;
					if (caret_count < 3) caret_count = 3;
				} else {
					te_col = visual_column_for_byte(src, (size_t)te, 0);
					caret_count = te_col - ts_col;
					if (caret_count < 3) caret_count = 3;
				}

				for (int i = 0; i < prefix_spaces + ts_col; i++)
					putc(' ', stderr);
				for (int i = 0; i < caret_count; i++)
					putc('^', stderr);
				putc('\n', stderr);
			}
		} else {
			(void) write_source_line(src, srclen, 0);
			putc('\n', stderr);
		}
	}
}

/*
 * PR3 of REPL error context chain (2026-05-27 JES): walk an error
 * stack snapshot and render one "at <script> line N" header plus a
 * source-context window per frame. Extracted from
 * repl_output_structured_error so the same machinery can render
 * either the primary error stack (use_causedby=false, reads
 * langgetstackframe) or the causedby stack (use_causedby=true,
 * reads langgetcausedbystackframe).
 *
 * Parameters:
 *   depth             - frame count returned by the matching
 *                       langget(causedby)stackdepth call
 *   use_causedby      - true to read frames from the causedby
 *                       snapshot, false for the primary snapshot
 *   eval_source_text  - source text for <eval> / <eval-inner> frames
 *                       (the user's REPL input); NULL omits the
 *                       source window for those frames
 *   use_ansi          - true to emit ANSI styling in render_source_window
 */
static void render_error_stack(short depth, boolean use_causedby,
                               const char *eval_source_text,
                               boolean use_ansi) {
	for (short ix = 0; ix < depth; ix++) {
		tyerrorrecord frame;
		long refcon = 0;
		boolean ok = use_causedby
		             ? langgetcausedbystackframe(ix, &frame, &refcon)
		             : langgetstackframe(ix, &frame, &refcon);
		if (!ok)
			break;

		char namebuf[256];
		script_name_for_frame(refcon, namebuf, sizeof(namebuf));

		/* Choose source text for this frame:
		 *   - <eval> / <eval-inner>: the caller-supplied input buffer
		 *   - named script: fetch via opgetlangtext using the
		 *     refcon-as-hdlhashnode (the encoding op_handler.c uses)
		 */
		char *source_text_owned = NULL;	/* malloc'd, must free */
		const char *source_text = NULL;	/* borrowed view */

		if (refcon == 0L || refcon == -1L) {
			source_text = eval_source_text;
		} else {
			source_text_owned =
				fetch_script_source_from_node((hdlhashnode)refcon);
			source_text = source_text_owned;
		}

		/* Render the source window first so we can clamp the line
		 * number against the actual line count; then print the frame
		 * header with the clamped value. Without the clamp, an EOF
		 * shaped parse error would say "line N+1" while the window
		 * highlights line N. */
		int header_line = (int)frame.errorline;
		if (source_text != NULL) {
			char *buf = NULL;
			char **lines = NULL;
			int line_count = 0;
			if (split_into_lines(source_text, &buf, &lines, &line_count)) {
				int actual_line = header_line;
				if (line_count > 0) {
					if (actual_line < 1) actual_line = 1;
					if (actual_line > line_count) actual_line = line_count;
				}
				header_line = actual_line;
				fprintf(stderr, "  at %s line %d\n", namebuf, header_line);
				render_source_window(lines, line_count,
				                      (int)frame.errorline,
				                      (int)frame.tokenstart,
				                      (int)frame.tokenend,
				                      use_ansi,
				                      &actual_line);
				(void) actual_line;
				free(lines);
				free(buf);
			} else {
				fprintf(stderr, "  at %s line %d\n", namebuf, header_line);
			}
		} else {
			fprintf(stderr, "  at %s line %d\n", namebuf, header_line);
		}

		free(source_text_owned);
	}
}

void repl_output_structured_error(const char *error_msg,
                                  const char *eval_source_text) {
	/* Fall back to the legacy single-line renderer if there's no
	 * structured snapshot to draw from. This keeps the entry point
	 * safe to call unconditionally on the error path. */
	short depth = langgetstackdepth();
	tyerrorrecord topframe;
	boolean have_snapshot = (depth > 0) && langgetlasterror(&topframe);

	if (!have_snapshot) {
		repl_output_error(error_msg);
		return;
	}

	boolean use_ansi = stderr_supports_ansi();

	/* Header. Logged at error severity to match the legacy renderer's
	 * logging behavior so existing log consumers don't regress. */
	const char *msg = (error_msg != NULL && error_msg[0] != '\0')
	                  ? error_msg
	                  : "(no message)";
	log_error(LOG_COMP_GENERAL, "%s", msg);

	if (use_ansi)
		fprintf(stderr, ANSI_BOLD "Error: %s" ANSI_RESET "\n", msg);
	else
		fprintf(stderr, "Error: %s\n", msg);

	render_error_stack(depth, /*use_causedby=*/false, eval_source_text, use_ansi);

	/*
	 * PR3 of REPL error context chain (2026-05-27 JES): if the error
	 * came from an else block whose try body originally failed, surface
	 * the originating failure under a "Caused by:" header followed by
	 * its own context-window stack. This makes the chain explicit in
	 * the REPL output without forcing the user to reconstruct it from
	 * the tryError / tryErrorLine / tryErrorScript locals.
	 *
	 * Detection is langgetcausedbyerror() returning true -- populated
	 * by langseterrorcallbackline when an error fires inside a try
	 * body. Cleared on the next eval (langprescript) and at the next
	 * try block's entry (evaluatetry).
	 */
	short cb_depth = langgetcausedbystackdepth();
	tyerrorrecord cb_top;
	if (cb_depth > 0 && langgetcausedbyerror(&cb_top)) {
		const char *cb_msg = langgetcausedbymessage();
		if (cb_msg == NULL) cb_msg = "(no message)";

		fputc('\n', stderr);

		/* P1-3 (bar-raiser, 2026-05-27 JES): include the originating
		 * frame's line in the "Caused by" header when available. The
		 * errorline on cb_top has already had langeval_inputoffset applied
		 * by langgetcausedbyerror, so we print it verbatim and don't need
		 * to adjust here. */
		if (cb_top.errorline > 0) {
			if (use_ansi)
				fprintf(stderr,
				        ANSI_BOLD "Caused by (line %lu): %s" ANSI_RESET "\n",
				        cb_top.errorline, cb_msg);
			else
				fprintf(stderr, "Caused by (line %lu): %s\n",
				        cb_top.errorline, cb_msg);
		} else {
			if (use_ansi)
				fprintf(stderr, ANSI_BOLD "Caused by: %s" ANSI_RESET "\n",
				        cb_msg);
			else
				fprintf(stderr, "Caused by: %s\n", cb_msg);
		}

		render_error_stack(cb_depth, /*use_causedby=*/true,
		                    eval_source_text, use_ansi);
	}

	fflush(stderr);
}
