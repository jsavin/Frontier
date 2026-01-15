/*
 * repl_output.c - REPL output formatter implementation
 *
 * Displays UserTalk values, errors, prompts, and help text for REPL mode.
 *
 * Implementation notes:
 * - Uses coercetostring() for value-to-string conversion
 * - Special handling for tables (show summary instead of full dump)
 * - Prompts go to stderr (keeps stdout clean for piping)
 * - Values and help text go to stdout
 * - Errors go to stderr
 *
 * String Conversion Strategy:
 *
 * Frontier stores strings in two formats:
 * 1. Heap strings (hdlstring): Raw character data in handle, NO length byte
 * 2. Pascal strings (bigstring): Length byte at [0], data starts at [1]
 *
 * For value display, we use texthandletostring() to convert heap strings
 * to Pascal strings. This properly handles the format difference.
 *
 * DO NOT use copyheapstring() - it expects Pascal format in the handle.
 */

#include "repl_output.h"
#include "../Common/headers/lang.h"
#include "../Common/headers/langexternal.h"
#include "../Common/headers/strings.h"
#include "../Common/headers/logging.h"
#include <stdio.h>
#include <string.h>

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

/* Get count of items in hash table
 * (hashtablecount is not exported, so we count manually)
 */
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

/* Display value result from evaluation */
void repl_output_value(tyvaluerecord *val) {
	bigstring bs;
	tyvaluerecord val_copy;

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

	/* Use coercetostring() for all other types */
	val_copy = *val;  /* Don't mutate original */

	if (!coercetostring(&val_copy)) {
		printf("[unable to display value of type %d]\n", val->valuetype);
		fflush(stdout);
		return;
	}

	/* Extract string from coerced value */
	copyheapstring(val_copy.data.stringvalue, bs);

	/* Print value - add quotes for string types */
	if (val->valuetype == stringvaluetype) {
		printf("\"%.*s\"\n", (int)stringlength(bs), stringbaseaddress(bs));
	} else {
		printf("%.*s\n", (int)stringlength(bs), stringbaseaddress(bs));
	}
	fflush(stdout);

	/* Clean up coerced value */
	disposevaluerecord(val_copy, false);
}

/* Display result from evaluation (as bigstring) */
void repl_output_result(bigstring result) {
	if (result == nil) {
		return;
	}

	/* Don't display empty results (like Python REPL for None) */
	if (stringlength(result) == 0) {
		return;
	}

	/* Print result */
	printf("%.*s\n", (int)stringlength(result), stringbaseaddress(result));
	fflush(stdout);
}

/* Display error message
 *
 * User-facing REPL error output (exception to logging standards).
 * This is terminal UI output, not diagnostic logging.
 */
void repl_output_error(const char *error_msg) {
	if (error_msg == NULL) {
		log_error(LOG_COMP_GENERAL, "Unknown error");
		fprintf(stderr, "Error: (unknown error)\n");
	} else {
		log_error(LOG_COMP_GENERAL, "%s", error_msg);
		fprintf(stderr, "Error: %s\n", error_msg);
	}
	fflush(stderr);
}

/* Display help text (for /help command) */
void repl_output_help(void) {
	fputs("Available commands:\n", stdout);
	fputs("  /exit          Exit the REPL\n", stdout);
	fputs("  /help          Show this help message\n", stdout);
	fputs("  /clear         Clear workspace variables\n", stdout);
	fputs("  /vars          Show workspace variables\n", stdout);
	fputs("\n", stdout);
	fputs("Examples:\n", stdout);
	fputs("  workspace.x = 42\n", stdout);
	fputs("  workspace.x * 2\n", stdout);
	fputs("  workspace.sum = workspace.x + 10\n", stdout);
	fputs("\n", stdout);
	fputs("Multi-line input: Coming in Phase 2\n", stdout);
	fflush(stdout);
}

/* Helper: format a value for compact display in /vars output */
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

/* Display workspace variables (for /vars command) */
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
