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
#include "repl.h"       /* For repl_get_current_table() */
#include "linenoise.h"  /* For linenoiseHide/Show */
#include "../Common/headers/lang.h"
#include "../Common/headers/langexternal.h"
#include "../Common/headers/strings.h"
#include "../Common/headers/tablestructure.h"  /* For roottable */
#include "../Common/headers/logging.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

/* Global linenoise state for async output (set by event loop) */
static struct linenoiseState *g_linenoisestate = NULL;

/* Helper: Output string to FILE, converting CR (Mac) to LF (Unix).
 * Frontier internally uses CR for line endings (Classic Mac convention).
 * This converts CR→LF for proper Unix terminal display.
 * Note: Frontier never produces CRLF (Windows) - only CR (Mac) or LF (Unix).
 * If CRLF were present, this would convert to LFLF (double newlines). */
static void fputs_cr_to_lf(const char *str, size_t len, FILE *stream) {
	for (size_t i = 0; i < len; i++) {
		if (str[i] == '\r') {
			putc('\n', stream);
		} else {
			putc(str[i], stream);
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
	fputs("\n", stdout);
	fputs("/list - List contents of a table:\n", stdout);
	fputs("  /list                     List current table\n", stdout);
	fputs("  /list system.verbs        List specific table by path\n", stdout);
	fputs("  /list fileMenu            List via system.paths\n", stdout);
	fputs("  /list parentOf(fileMenu)  Evaluate expression for table\n", stdout);
	fputs("\n", stdout);
	fputs("QuickScript Model - Variable Persistence:\n", stdout);
	fputs("  Local variables (x = 5) don't persist between evaluations\n", stdout);
	fputs("  For persistence, use explicit database paths:\n", stdout);
	fputs("    system.temp.x = 42       (session-scoped)\n", stdout);
	fputs("    workspace.x = 42         (saved to database)\n", stdout);
	fputs("\n", stdout);
	fputs("Multi-line input: Coming in Phase 2\n", stdout);
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
			char value_buf[512];
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

/* --- Event Loop Support (Phase 4) --- */

/* Set the active linenoise state for async output */
void repl_set_active_linenoisestate(struct linenoiseState *ls) {
	g_linenoisestate = ls;
}

/* Display async output while user is typing at prompt.
 * Uses linenoiseHide/Show to preserve the user's current input.
 */
void repl_async_output(const char *message) {
	if (message == NULL) {
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
