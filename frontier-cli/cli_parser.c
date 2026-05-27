/*
    cli_parser.c - Command-Line Argument Parsing for frontier-cli

    Parses and validates command-line options using getopt_long.
    Supports script execution (-e), database loading (--system-root),
    output modes (--output-json), and maintenance operations (--hydrate, --upgrade).

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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#include "cli_parser.h"
#include "cli_utils.h"
#include "standard.h"			/* env_truthy() */
#include "../Common/headers/logging.h"
#include "../Common/headers/shell_api.h"	/* shell_api_set_lock_opened_roots() */

/*
 * Convert a kebab-case flag name to camelCase.
 * Strips leading dashes, then splits on '-' and capitalizes each word after the first.
 * Caller must free the returned string.
 */
static char *kebab_to_camel(const char *flag) {
	const char *p = flag;

	/* Strip leading dashes */
	while (*p == '-')
		p++;

	size_t len = strlen(p);
	char *result = malloc(len + 1);
	if (result == NULL)
		return NULL;

	size_t j = 0;
	boolean capitalize_next = false;

	for (size_t i = 0; i < len; i++) {
		if (p[i] == '-') {
			capitalize_next = true;
		} else {
			if (capitalize_next && p[i] >= 'a' && p[i] <= 'z') {
				result[j++] = (char)(p[i] - 'a' + 'A');
			} else {
				result[j++] = p[i];
			}
			capitalize_next = false;
		}
	}

	result[j] = '\0';
	return result;
}

/*
 * Append an extra argument to the linked list.
 * key is the camelCase name, value is the string value (or NULL for boolean).
 * Both are strdup'd internally.
 */
static boolean cli_add_extra_arg(cli_options_t *options, const char *key, const char *value) {
	cli_extra_arg_t *node = calloc(1, sizeof(cli_extra_arg_t));
	if (node == NULL)
		return false;

	node->key = strdup(key);
	if (node->key == NULL) {
		free(node);
		return false;
	}

	if (value != NULL) {
		node->value = strdup(value);
		if (node->value == NULL) {
			free(node->key);
			free(node);
			return false;
		}
	}

	/* Append to end of list to preserve command-line order */
	if (options->extra_args == NULL) {
		options->extra_args = node;
	} else {
		cli_extra_arg_t *tail = options->extra_args;
		while (tail->next != NULL)
			tail = tail->next;
		tail->next = node;
	}

	return true;
}

/* Returns true if flag is a known long option name (without --).
 * Derives the known set from long_options[] to avoid duplication. */
static boolean is_known_long_option(const char *name, const struct option *long_options) {
	for (int k = 0; long_options[k].name != NULL; k++) {
		if (strcmp(long_options[k].name, name) == 0)
			return true;
	}
	return false;
}

/* Checks if a path ends with .root or .root7 (case-insensitive). */
static boolean cli_is_root_file(const char* path) {
	if (path == NULL) {
		return false;
	}

	size_t len = strlen(path);

	// Check for .root (5 chars minimum)
	if (len >= 5) {
		const char* ext = path + len - 5;
		if (strcasecmp(ext, ".root") == 0) {
			return true;
		}
	}

	// Check for .root7 (6 chars minimum)
	if (len >= 6) {
		const char* ext = path + len - 6;
		if (strcasecmp(ext, ".root7") == 0) {
			return true;
		}
	}

	return false;
}

/* Initializes all options to zero/NULL defaults. */
static void cli_init_options(cli_options_t* options) {
	memset(options, 0, sizeof(cli_options_t));
}

/* Validates parsed options for consistency and required dependencies. */
boolean cli_validate_options(const cli_options_t* options) {
	if (options->show_help || options->show_version) {
		return true;
	}

	boolean hydration_mode = options->hydrate_system_root;
	boolean migrate_mode = (options->migrate_database != NULL);

	/* --migrate mode: migrate a database to v7 and exit */
	if (migrate_mode) {
		if (!cli_file_exists(options->migrate_database)) {
			log_error(LOG_COMP_GENERAL, "Error: Database does not exist: %s", options->migrate_database);
			return false;
		}
		if (!cli_file_readable(options->migrate_database)) {
			log_error(LOG_COMP_GENERAL, "Error: Database is not readable: %s", options->migrate_database);
			return false;
		}
		/* --output without --migrate is an error */
		/* --force without --migrate is harmless but meaningless */
		if (hydration_mode) {
			log_error(LOG_COMP_GENERAL, "Error: --migrate cannot be combined with --hydrate-system-root");
			return false;
		}
		if (options->system_root != NULL || options->script_file != NULL || options->inline_script != NULL) {
			log_error(LOG_COMP_GENERAL, "Error: --migrate runs standalone; do not combine with --system-root, scripts, or REPL mode");
			return false;
		}
		return true;
	}

	/* --output requires --migrate */
	if (options->output_path != NULL) {
		log_error(LOG_COMP_GENERAL, "Error: --output requires --migrate");
		return false;
	}

	if (hydration_mode) {
		if (options->system_root == NULL) {
			log_error(LOG_COMP_GENERAL, "Error: --hydrate-system-root requires --system-root PATH");
			return false;
		}
	}

	/* --protocol mode: incompatible with script/inline execution */
	if (options->protocol_mode) {
		if (options->script_file != NULL || options->inline_script != NULL) {
			log_error(LOG_COMP_GENERAL, "Error: --protocol cannot be combined with script execution");
			return false;
		}
	}

	// Note: Conflict validation for positional .root argument is handled in cli_parse_arguments()
	// when we detect a .root file and system_root is already set.

	// Note: No script_file and no inline_script means REPL mode (interactive)
	// This is now a valid execution mode, so we don't error out here

	if (options->system_root != NULL) {
		if (!cli_file_exists(options->system_root)) {
			log_error(LOG_COMP_GENERAL, "Error: System root does not exist: %s", options->system_root);
			return false;
		}
		if (!cli_file_readable(options->system_root)) {
			log_error(LOG_COMP_GENERAL, "Error: System root is not readable: %s", options->system_root);
			return false;
		}
	}

	return true;
}

/* Parses command-line arguments and populates the options structure.
 * On failure, the caller must still call cli_free_options() to release
 * any partially-allocated fields (extra_args, positional_args, etc.). */
boolean cli_parse_arguments(int argc, char* argv[], cli_options_t* options) {
	int opt;
	int option_index = 0;

	// Initialize options with defaults
	cli_init_options(options);

	/* Long-only option codes (no short alias). 256+ keeps them out of the
	 * single-character optstring while remaining valid getopt return values. */
	enum {
		OPT_LOCK_OPENED_ROOTS = 256,
	};

	// Define long options
	static struct option long_options[] = {
		{"execute", required_argument, 0, 'e'},
		{"system-root", required_argument, 0, 'R'},
		{"migrate", required_argument, 0, 'm'},
		{"output", required_argument, 0, 'o'},
		{"force", no_argument, 0, 'f'},
		{"batch", no_argument, 0, 'b'},
		{"non-interactive", no_argument, 0, 'b'},  /* Alias for --batch */
		{"hydrate-system-root", no_argument, 0, 'H'},
		{"output-json", no_argument, 0, 'J'},
		{"verbose", no_argument, 0, 'v'},
		{"debug", no_argument, 0, 'D'},
		{"log", required_argument, 0, 'L'},
		{"skip-startup", no_argument, 0, 'S'},
		{"protocol", no_argument, 0, 'P'},
		{"ws-port", optional_argument, 0, 'W'},
		{"lock-opened-roots", no_argument, 0, OPT_LOCK_OPENED_ROOTS},
		{"help", no_argument, 0, 'h'},
		{"version", no_argument, 0, 'V'},
		{0, 0, 0, 0}
	};

	/*
	 * Two-pass parsing:
	 *	 Pass 1 — Extract unknown --flags (and their values) from argv,
	 *			  building a filtered argv for getopt_long.
	 *	 Pass 2 — Run getopt_long on the filtered argv (known flags only).
	 *
	 * This avoids getopt_long's inability to skip unknown long options
	 * that take values (it would misinterpret the value as a positional).
	 */

	/* Pass 1: Build filtered argv, extract unknown --flags into extra_args */

	char **filtered_argv = malloc((size_t)(argc + 1) * sizeof(char *));
	if (filtered_argv == NULL)
		return false;

	int filtered_argc = 0;
	boolean first_positional_seen = false;

	filtered_argv[filtered_argc++] = argv[0]; /* program name */

	for (int i = 1; i < argc; i++) {
		const char *arg = argv[i];

		if (arg[0] == '-' && arg[1] == '-' && arg[2] != '\0') {
			/* Long flag: check if known */
			const char *flag_name = arg + 2;

			/* For known-flag check, strip =value if present
			 * (getopt_long handles --flag=value natively for known flags).
			 * If the flag name exceeds 255 chars, known_check_name keeps the
			 * full flag=value string, which won't match any known option —
			 * so it falls through to the unknown-flag branch correctly. */
			const char *known_eq = strchr(flag_name, '=');
			char known_name_buf[256];
			const char *known_check_name = flag_name;
			if (known_eq != NULL) {
				size_t klen = (size_t)(known_eq - flag_name);
				if (klen < sizeof(known_name_buf)) {
					memcpy(known_name_buf, flag_name, klen);
					known_name_buf[klen] = '\0';
					known_check_name = known_name_buf;
				}
			}

			if (is_known_long_option(known_check_name, long_options)) {
				/* Known flag — pass through to getopt (handles =value internally) */
				filtered_argv[filtered_argc++] = argv[i];

				/* If it takes a separate argument (no = present), pass that through too */
				if (known_eq == NULL) {
					for (int k = 0; long_options[k].name != NULL; k++) {
						if (strcmp(long_options[k].name, known_check_name) == 0) {
							if (long_options[k].has_arg == required_argument && i + 1 < argc) {
								i++;
								filtered_argv[filtered_argc++] = argv[i];
							}
							break;
						}
					}
				}
			}
			else {
				/* Unknown flag — extract into extra_args.
				 * Handle --flag=value syntax: split on first '=' if present. */
				const char *eq = strchr(flag_name, '=');
				const char *key_part = flag_name;
				const char *inline_value = NULL;
				char *key_buf = NULL;

				if (eq != NULL) {
					/* --flag=value: key is everything before '=', value after */
					size_t key_len = (size_t)(eq - flag_name);
					key_buf = malloc(key_len + 1);
					if (key_buf == NULL) {
						log_warn(LOG_COMP_GENERAL, "Memory allocation failed, skipping flag: %s", arg);
						continue;
					}
					memcpy(key_buf, flag_name, key_len);
					key_buf[key_len] = '\0';
					key_part = key_buf;
					inline_value = eq + 1;
				}

				char *camel = kebab_to_camel(key_part);
				free(key_buf); /* safe if NULL; key_part is now dangling if eq != NULL */
				key_part = NULL; /* prevent accidental use-after-free */
				key_buf = NULL;
				if (camel == NULL)
					continue;

				/* Reject keys longer than 255 chars (bigstring limit) */
				if (strlen(camel) > 255) {
					log_warn(LOG_COMP_GENERAL, "Custom flag name too long, skipping: %s", camel);
					free(camel);
					continue;
				}

				/* Determine the value: inline (--flag=val) takes priority,
				 * otherwise next arg if it doesn't start with '-'.
				 * Note: --threshold -5 treats -5 as a flag, not a negative
				 * number value. This is a known limitation. */
				const char *value = inline_value;
				if (value == NULL && i + 1 < argc && argv[i + 1][0] != '-') {
					value = argv[i + 1];
					i++; /* consume the value */
				}

				if (!cli_add_extra_arg(options, camel, value)) {
					log_error(LOG_COMP_GENERAL, "Memory allocation failed for custom flag: %s", camel);
					free(camel);
					free(filtered_argv);
					return false;
				}
				free(camel);
			}
		}
		else {
			/* Short flag or positional — pass through.
			 * Note: first_positional_seen tracks whether we've passed the
			 * script file / .root positional to filtered_argv. Short flag
			 * values (e.g. the 'expr' in -e expr) are consumed by the
			 * short-flag branch above and won't trigger this flag. */
			if (arg[0] != '-' && !first_positional_seen) {
				first_positional_seen = true;
				filtered_argv[filtered_argc++] = argv[i];
			}
			else if (arg[0] == '-' && arg[1] != '\0') {
				/* Short flag — pass through */
				filtered_argv[filtered_argc++] = argv[i];

				/* Consume required argument for known short flags (separate arg only).
				 * Only when flag is exactly "-X" (not "-Xvalue" inline form).
				 *
				 * SYNC WARNING: This list must match the required_argument short flags
				 * from optstring "e:R:m:o:fbHJvDPW::ShV". A colon after a letter
				 * means required_argument. W:: is optional_argument — intentionally
				 * excluded because optional args must use --ws-port=VALUE syntax
				 * (getopt does not consume a separate next-arg for optional). */
				static const char short_with_arg[] = "eRmo";
				char flag_char = arg[1];
				if (arg[2] == '\0' && strchr(short_with_arg, flag_char) != NULL && i + 1 < argc) {
					i++;
					filtered_argv[filtered_argc++] = argv[i];
				}
			}
			else {
				/* Extra positional arg */
				int n = options->positional_count;
				char **new_arr = realloc(options->positional_args, (size_t)(n + 1) * sizeof(char *));
				if (new_arr == NULL) {
					log_error(LOG_COMP_GENERAL, "Error: Memory allocation failed for positional arg");
					free(filtered_argv);
					return false;
				}
				new_arr[n] = strdup(arg);
				if (new_arr[n] == NULL) {
					log_error(LOG_COMP_GENERAL, "Error: Memory allocation failed for positional arg");
					options->positional_args = new_arr;
					free(filtered_argv);
					return false;
				}
				options->positional_args = new_arr;
				options->positional_count = n + 1;
			}
		}
	}

	filtered_argv[filtered_argc] = NULL;

	/* Pass 2: Run getopt_long on filtered argv (known flags only) */

	optind = 1; /* reset getopt state */
	opterr = 1; /* re-enable error messages for truly bad syntax */

	while ((opt = getopt_long(filtered_argc, filtered_argv, "e:R:m:o:fbHJvDPW::ShV", long_options, &option_index)) != -1) {
		switch (opt) {
			case 'e':
				if (options->inline_script != NULL) {
					log_error(LOG_COMP_GENERAL, "Error: Multiple --execute options not allowed");
					goto parse_error;
				}
				if (strlen(optarg) > CLI_MAX_SCRIPT_LENGTH) {
					log_error(LOG_COMP_GENERAL, "Error: Inline script too long (max %d characters)", CLI_MAX_SCRIPT_LENGTH);
					goto parse_error;
				}
				options->inline_script = strdup(optarg);
				break;

			case 'R':
				if (options->system_root != NULL) {
					log_error(LOG_COMP_GENERAL, "Error: Multiple --system-root options not allowed");
					goto parse_error;
				}
				if (strlen(optarg) > CLI_MAX_PATH_LENGTH) {
					log_error(LOG_COMP_GENERAL, "Error: System root path too long (max %d characters)", CLI_MAX_PATH_LENGTH);
					goto parse_error;
				}
				options->system_root = strdup(optarg);
				break;

			case 'm':
				if (options->migrate_database != NULL) {
					log_error(LOG_COMP_GENERAL, "Error: Multiple --migrate options not allowed");
					goto parse_error;
				}
				if (strlen(optarg) > CLI_MAX_PATH_LENGTH) {
					log_error(LOG_COMP_GENERAL, "Error: Migrate path too long (max %d characters)", CLI_MAX_PATH_LENGTH);
					goto parse_error;
				}
				options->migrate_database = strdup(optarg);
				break;

			case 'o':
				if (options->output_path != NULL) {
					log_error(LOG_COMP_GENERAL, "Error: Multiple --output options not allowed");
					goto parse_error;
				}
				if (strlen(optarg) > CLI_MAX_PATH_LENGTH) {
					log_error(LOG_COMP_GENERAL, "Error: Output path too long (max %d characters)", CLI_MAX_PATH_LENGTH);
					goto parse_error;
				}
				options->output_path = strdup(optarg);
				break;

			case 'f':  options->force_overwrite = true; break;
			case 'b':  options->batch_mode = true; break;
			case 'H':  options->hydrate_system_root = true; break;
			case 'J':  options->output_json = true; break;
			case 'v':  options->verbose = true; break;
			case 'D':  options->debug = true; break;
			case 'S':  options->skip_startup = true; break;
			case 'P':  options->protocol_mode = true; break;
			case OPT_LOCK_OPENED_ROOTS: options->lock_opened_roots = true; break;
			case 'h':  options->show_help = true; break;
			case 'V':  options->show_version = true; break;

			case 'L':
				// Log spec (--log comp:level,...)
				if (options->log_spec != NULL) {
					size_t old_len = strlen(options->log_spec);
					size_t new_len = strlen(optarg);
					char *combined = malloc(old_len + 1 + new_len + 1);
					if (combined == NULL) {
						log_error(LOG_COMP_GENERAL, "Error: Memory allocation failed for --log");
						goto parse_error;
					}
					memcpy(combined, options->log_spec, old_len);
					combined[old_len] = ',';
					memcpy(combined + old_len + 1, optarg, new_len + 1);
					free(options->log_spec);
					options->log_spec = combined;
				} else {
					options->log_spec = strdup(optarg);
					if (options->log_spec == NULL) {
						log_error(LOG_COMP_GENERAL, "Error: Memory allocation failed for --log");
						goto parse_error;
					}
				}
				break;

			case 'W':
				/* --ws-port uses optional_argument: value must be inline (--ws-port=5337)
				 * or omitted (uses default). Pass 1 intentionally does NOT consume a
				 * separate next-arg for 'W' since optional_argument requires = syntax. */
				if (optarg != NULL && optarg[0] != '\0') {
					char *endptr;
					long port = strtol(optarg, &endptr, 10);
					if (*endptr != '\0' || port < 1 || port > 65535) {
						log_error(LOG_COMP_GENERAL, "Error: Invalid WebSocket port: %s", optarg);
						goto parse_error;
					}
					options->ws_port = (int)port;
				} else {
					options->ws_port = CLI_DEFAULT_WS_PORT;
				}
				break;

			case '?':
				goto parse_error;

			default:
				break;
		}
	}

	// Handle the first non-option argument from filtered argv
	if (optind < filtered_argc) {
		const char* arg = filtered_argv[optind];

		if (strlen(arg) > CLI_MAX_PATH_LENGTH) {
			log_error(LOG_COMP_GENERAL, "Error: Path too long (max %d characters)", CLI_MAX_PATH_LENGTH);
			free(filtered_argv);
			return false;
		}

		if (cli_is_root_file(arg)) {
			if (options->system_root != NULL) {
				log_error(LOG_COMP_GENERAL, "Error: System root already specified via --system-root");
				free(filtered_argv);
				return false;
			}
			options->system_root = strdup(arg);
			if (options->system_root == NULL) {
				log_error(LOG_COMP_GENERAL, "Error: Memory allocation failed");
				free(filtered_argv);
				return false;
			}
		} else {
			if (options->script_file != NULL) {
				log_error(LOG_COMP_GENERAL, "Error: Multiple script files not allowed");
				free(filtered_argv);
				return false;
			}
			options->script_file = strdup(arg);
			if (options->script_file == NULL) {
				log_error(LOG_COMP_GENERAL, "Error: Memory allocation failed");
				free(filtered_argv);
				return false;
			}
		}
	}

	free(filtered_argv);

	/* Environment <-> CLI flag synchronization for --lock-opened-roots (issue #127).
	 *
	 * The env var FRONTIER_LOCK_OPENED_ROOTS lets the integration test runner
	 * and other ephemeral consumers opt every loaded-from-disk DB into
	 * read-only-on-save without threading a CLI flag through every spawn
	 * site.
	 *
	 * Sync rules so non-CLI consumers (e.g., dbopenverb in
	 * Common/source/dbverbs.c, which has no link to the CLI parser state)
	 * can read a single source of truth via getenv():
	 *
	 *   - If env is set truthy (non-empty and not "0"), it enables the flag.
	 *   - If --lock-opened-roots was passed on the CLI, the flag is
	 *     authoritative: setenv() unconditionally so any descendant code
	 *     path that consults FRONTIER_LOCK_OPENED_ROOTS sees "1", even if
	 *     the inherited env had FRONTIER_LOCK_OPENED_ROOTS=0. Without this
	 *     overwrite, the CLI flag would lock the system root while
	 *     dbopenverb's env check would still permit guest-DB writes --
	 *     inconsistent state. The explicit CLI flag always wins.
	 *
	 * Validation in cli_validate_options() runs after. */
	{
		/* env_truthy() lives in Common/SystemHeaders/standard.h; reusing it
		 * here keeps the FRONTIER_LOCK_OPENED_ROOTS truthiness contract in
		 * lockstep with dbverbs.c's enforcement check. */
		if (env_truthy("FRONTIER_LOCK_OPENED_ROOTS"))
			options->lock_opened_roots = true;

		if (options->lock_opened_roots)
			setenv("FRONTIER_LOCK_OPENED_ROOTS", "1", 1);

		/* Issue #649: publish the converged decision to shell_api so
		 * non-CLI consumers (e.g., dbopenverb) can read a single
		 * in-process source of truth without re-parsing the env var. */
		shell_api_set_lock_opened_roots(options->lock_opened_roots);
	}

	// Validate the parsed options
	return cli_validate_options(options);

parse_error:
	free(filtered_argv);
	return false;
}

/* Frees dynamically allocated strings in the options structure. */
void cli_free_options(cli_options_t* options) {
	if (options == NULL) {
		return;
	}

	// Free allocated strings
	if (options->script_file != NULL) {
		free(options->script_file);
		options->script_file = NULL;
	}

	if (options->inline_script != NULL) {
		free(options->inline_script);
		options->inline_script = NULL;
	}

	if (options->system_root != NULL) {
		free(options->system_root);
		options->system_root = NULL;
	}

	if (options->migrate_database != NULL) {
		free(options->migrate_database);
		options->migrate_database = NULL;
	}

	if (options->output_path != NULL) {
		free(options->output_path);
		options->output_path = NULL;
	}

	if (options->log_spec != NULL) {
		free(options->log_spec);
		options->log_spec = NULL;
	}

	/* Free extra args linked list */
	{
		cli_extra_arg_t *node = options->extra_args;
		while (node != NULL) {
			cli_extra_arg_t *next = node->next;
			free(node->key);
			free(node->value);
			free(node);
			node = next;
		}
		options->extra_args = NULL;
	}

	/* Free positional args array */
	for (int i = 0; i < options->positional_count; i++) {
		free(options->positional_args[i]);
	}
	free(options->positional_args);
	options->positional_args = NULL;
	options->positional_count = 0;
}

/* Prints parsed options for debugging purposes. */
void cli_print_options(const cli_options_t* options) {
	if (options == NULL) {
		printf("CLI Options: NULL\n");
		return;
	}

	printf("CLI Options:\n");
	printf("  Script File: %s\n", options->script_file ? options->script_file : "(none)");
	printf("  Inline Script: %s\n", options->inline_script ? options->inline_script : "(none)");
	printf("  System Root: %s\n", options->system_root ? options->system_root : "(none)");
	printf("  Migrate Database: %s\n", options->migrate_database ? options->migrate_database : "(none)");
	printf("  Output Path: %s\n", options->output_path ? options->output_path : "(none)");
	printf("  Force Overwrite: %s\n", options->force_overwrite ? "yes" : "no");
	printf("  Skip Startup: %s\n", options->skip_startup ? "yes" : "no");
	printf("  Protocol Mode: %s\n", options->protocol_mode ? "yes" : "no");
	printf("  Lock Opened Roots: %s\n", options->lock_opened_roots ? "yes" : "no");
	printf("  WebSocket Port: %d\n", options->ws_port);
	printf("  Verbose: %s\n", options->verbose ? "yes" : "no");
	printf("  Debug: %s\n", options->debug ? "yes" : "no");
	printf("  Output JSON: %s\n", options->output_json ? "yes" : "no");
	printf("  Hydrate System Root: %s\n", options->hydrate_system_root ? "yes" : "no");
	printf("  Show Help: %s\n", options->show_help ? "yes" : "no");
	printf("  Show Version: %s\n", options->show_version ? "yes" : "no");
}
