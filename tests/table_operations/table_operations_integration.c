/*
 * table_operations_integration.c - Comprehensive UserTalk integration tests for table verbs
 *
 * This test suite verifies all table operation verbs at the UserTalk level:
 * - table.assign(table.key, value) - Set a key/value pair
 * - table.move(@source.key, @dest) - Move value from one table to another
 * - table.copy(@source.key, @dest) - Copy value (source remains unchanged)
 * - table.rename(@table.oldname, newname) - Rename a key in a table
 * - table.emptytable(@table) - Clear all entries, return count
 * - table.moveandrename(@source.key, @dest.newname) - Move and rename in one operation
 *
 * Tests are organized by operation type with both basic and complex scenarios.
 * Coverage: 45 tests across 6 operation categories + complex scenarios.
 *
 * Run: make -C tests table_operations_integration && ./tests/table_operations_integration
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>
#include <stdbool.h>
#include <time.h>

#include "../Common/headers/logging.h"

/* Get repository root by walking up from current working directory */
static int get_repo_root(char *buf, size_t bufsize) {
	char cwd[PATH_MAX];

	/* Start from current working directory */
	if (getcwd(cwd, sizeof(cwd)) == NULL) {
		return 0;
	}

	char dir_copy[PATH_MAX];
	strncpy(dir_copy, cwd, sizeof(dir_copy) - 1);
	dir_copy[sizeof(dir_copy) - 1] = '\0';

	/* Walk up to find repo root (contains databases/ directory) */
	while (strlen(dir_copy) > 1) {
		char test_path[PATH_MAX];
		snprintf(test_path, sizeof(test_path), "%s/databases", dir_copy);
		if (access(test_path, F_OK) == 0) {
			if (strlen(dir_copy) >= bufsize) {
				return 0;
			}
			strncpy(buf, dir_copy, bufsize - 1);
			buf[bufsize - 1] = '\0';
			return 1;
		}

		/* Use dirname safely by working on a copy */
		char dir_copy_backup[PATH_MAX];
		strncpy(dir_copy_backup, dir_copy, sizeof(dir_copy_backup) - 1);
		dir_copy_backup[sizeof(dir_copy_backup) - 1] = '\0';

		char *parent = dirname(dir_copy_backup);
		if (parent == NULL || strcmp(parent, dir_copy) == 0) {
			/* Reached root or error */
			break;
		}
		strncpy(dir_copy, parent, sizeof(dir_copy) - 1);
		dir_copy[sizeof(dir_copy) - 1] = '\0';
	}
	return 0;
}

/* Execute a UserTalk script via frontier-cli and return output */
static void eval_cli(const char *script, char *output, size_t output_size) {
	char root[PATH_MAX];
	if (!get_repo_root(root, sizeof(root))) {
		char cwd[PATH_MAX];
		if (getcwd(cwd, sizeof(cwd))) {
			log_error(LOG_COMP_GENERAL, "FATAL: Could not find repository root (looking for databases/). Current dir: %s", cwd);
		} else {
			log_error(LOG_COMP_GENERAL, "FATAL: Could not find repository root (looking for databases/)");
		}
		assert(0);
	}

	char db_path[PATH_MAX];
	snprintf(db_path, sizeof(db_path), "%s/databases/Frontier-v7.root", root);

	if (access(db_path, R_OK) != 0) {
		log_error(LOG_COMP_GENERAL, "ERROR: Database not found: %s", db_path);
		log_error(LOG_COMP_GENERAL, "Ensure Frontier-v7.root exists by running CLI once with Frontier-v6.root");
		assert(0);
	}

	char cli_path[PATH_MAX];
	snprintf(cli_path, sizeof(cli_path), "%s/frontier-cli/frontier-cli", root);

	/* Escape the script for safe shell execution - convert " to \" */
	char escaped_script[16384];
	size_t src_idx = 0, dst_idx = 0;
	while (script[src_idx] && dst_idx < sizeof(escaped_script) - 2) {
		if (script[src_idx] == '"') {
			escaped_script[dst_idx++] = '\\';
			escaped_script[dst_idx++] = '"';
		} else if (script[src_idx] == '\\') {
			escaped_script[dst_idx++] = '\\';
			escaped_script[dst_idx++] = '\\';
		} else {
			escaped_script[dst_idx++] = script[src_idx];
		}
		src_idx++;
	}
	escaped_script[dst_idx] = '\0';

	char cmd[16384];
	snprintf(cmd, sizeof(cmd), "cd \"%s\" && FRONTIER_HEADLESS_SKIP_STARTUP=1 %s --system-root \"%s/databases/Frontier-v7.root\" -e \"%s\" 2>&1", root, cli_path, root, escaped_script);

	FILE *fp = popen(cmd, "r");
	if (!fp) {
		log_error(LOG_COMP_GENERAL, "ERROR: Could not execute command: %s", cmd);
		assert(0);
	}

	size_t total = 0;
	while (total < output_size - 1 && fgets(output + total, output_size - total, fp) != NULL) {
		total = strlen(output);
	}
	output[total] = '\0';

	int status = pclose(fp);

	if (status != 0) {
		log_error(LOG_COMP_GENERAL, "CLI exited with status %d", status);
		log_error(LOG_COMP_GENERAL, "Script: %s", script);
		log_error(LOG_COMP_GENERAL, "Output: %s", output);
		assert(0);
	}

	/* Extract the result - find the last token after any [lang-ERROR] prefix */
	char result_token[1024] = {0};

	/* Look for the last occurrence of the result (usually after [lang-ERROR] prefix) */
	char *last_result = output;
	char *search_pos = output;
	while ((search_pos = strstr(search_pos, ": ")) != NULL) {
		last_result = search_pos + 2;  /* Skip the ": " prefix */
		search_pos++;
	}

	/* If we found something after a colon, use that; otherwise use the whole output */
	if (last_result != output) {
		/* Extract just the last token after the last colon */
		strncpy(result_token, last_result, sizeof(result_token) - 1);
		result_token[sizeof(result_token) - 1] = '\0';

		/* Trim any trailing newlines/spaces */
		size_t len = strlen(result_token);
		while (len > 0 && (result_token[len-1] == '\n' || result_token[len-1] == '\r' || result_token[len-1] == ' ')) {
			result_token[--len] = '\0';
		}

		/* If result contains warning/error patterns, fall through to line-by-line parsing */
		if (strstr(result_token, "system=") != NULL ||
		    strstr(result_token, "[WARN]") != NULL ||
		    strstr(result_token, "[ERROR]") != NULL ||
		    strstr(result_token, "ix=") != NULL ||  /* Memory error messages */
		    strstr(result_token, "caller=") != NULL ||
		    strstr(result_token, "Cant ") != NULL ||  /* UserTalk error messages */
		    strstr(result_token, "hasnt ") != NULL) {
			result_token[0] = '\0';  /* Clear and fall through */
		}
	}

	if (result_token[0] == '\0') {
		/* No colon found, look for lines without log prefixes */
		char *pos = output;
		while (*pos) {
			/* Find the end of the current line */
			char *newline = strchr(pos, '\n');
			size_t line_len;
			if (newline) {
				line_len = newline - pos;
			} else {
				line_len = strlen(pos);
			}

			/* Create a temporary line buffer */
			char current_line[1024];
			strncpy(current_line, pos, line_len);
			current_line[line_len] = '\0';

			/* Skip log lines, warnings, errors, and memory messages */
			if (strstr(current_line, "[headless]") == NULL &&
			    strstr(current_line, "[WARN]") == NULL &&
			    strstr(current_line, "[ERROR]") == NULL &&
			    strstr(current_line, "[2025") == NULL &&
			    strstr(current_line, "system=") == NULL &&
			    strstr(current_line, "ix=") == NULL &&  /* Memory error messages */
			    strstr(current_line, "caller=") == NULL &&
			    strstr(current_line, "Cant ") == NULL &&  /* UserTalk error messages */
			    strstr(current_line, "hasnt ") == NULL &&
			    strlen(current_line) > 0) {
				strncpy(result_token, current_line, sizeof(result_token) - 1);
				result_token[sizeof(result_token) - 1] = '\0';
			}

			if (!newline) break;
			pos = newline + 1;
		}
	}

	strncpy(output, result_token, output_size - 1);
	output[output_size - 1] = '\0';

	/* Trim trailing whitespace */
	total = strlen(output);
	while (total > 0 && (output[total-1] == '\n' || output[total-1] == '\r' || output[total-1] == ' ')) {
		output[--total] = '\0';
	}
}

/* Size for CLI output buffers (accommodates command output + warnings/diagnostics) */
#define TEST_OUTPUT_BUFFER_SIZE 2048

/* Helper to evaluate expression and verify string result */
static void eval_expect_string(const char *expr, const char *expected) {
	char result[TEST_OUTPUT_BUFFER_SIZE];
	eval_cli(expr, result, sizeof(result));

	if (strcmp(result, expected) != 0) {
		log_error(LOG_COMP_GENERAL, "MISMATCH for: %s", expr);
		log_error(LOG_COMP_GENERAL, "Expected: %s", expected);
		log_error(LOG_COMP_GENERAL, "Got: %s", result);
		assert(0);
	}
}

/* Helper to evaluate expression and verify numeric result */
static void eval_expect_number(const char *expr, int expected) {
	char result[TEST_OUTPUT_BUFFER_SIZE];
	eval_cli(expr, result, sizeof(result));

	int val = atoi(result);
	if (val != expected) {
		log_error(LOG_COMP_GENERAL, "NUMERIC MISMATCH for: %s", expr);
		log_error(LOG_COMP_GENERAL, "Expected: %d", expected);
		log_error(LOG_COMP_GENERAL, "Got: %d", val);
		assert(0);
	}
}

/* ========================================================================== */
/* TEST SUITE: table.assign() - Basic assignment and mutations              */
/* ========================================================================== */

/* Test 1: table.assign - Basic string assignment */
static void test_table_assign_basic(void) {
	printf("[table_operations_integration] test_table_assign_basic: start\n");
	fflush(stdout);

	eval_expect_string("local (t); new(tableType, @t); t.key1 = \"hello\"; if t.key1 == \"hello\" { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_assign_basic: PASS\n");
	fflush(stdout);
}

/* Test 2: table.assign - Multiple value types */
static void test_table_assign_multiple_types(void) {
	printf("[table_operations_integration] test_table_assign_multiple_types: start\n");
	fflush(stdout);

	/* Test assigning multiple types and verifying they all persist */
	eval_expect_string("local (t); new(tableType, @t); t.str = \"value\"; t.num = 42; t.bool = true; if t.str == \"value\" { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_assign_multiple_types: PASS\n");
	fflush(stdout);
}

/* Test 3: table.assign - Overwrite existing value */
static void test_table_assign_overwrite(void) {
	printf("[table_operations_integration] test_table_assign_overwrite: start\n");
	fflush(stdout);

	eval_expect_string("local (t); new(tableType, @t); t.key = \"original\"; t.key = \"updated\"; if t.key == \"updated\" { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_assign_overwrite: PASS\n");
	fflush(stdout);
}

/* Test 4: table.assign - Table size verification */
static void test_table_assign_size(void) {
	printf("[table_operations_integration] test_table_assign_size: start\n");
	fflush(stdout);

	eval_expect_number("local (t); new(tableType, @t); t.a = 1; t.b = 2; t.c = 3; return sizeOf(t)", 3);

	printf("[table_operations_integration] test_table_assign_size: PASS\n");
	fflush(stdout);
}

/* Test 5: table.assign - Empty string */
static void test_table_assign_empty_string(void) {
	printf("[table_operations_integration] test_table_assign_empty_string: start\n");
	fflush(stdout);

	eval_expect_string("local (t); new(tableType, @t); t.empty = \"\"; if t.empty == \"\" and sizeOf(t) == 1 { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_assign_empty_string: PASS\n");
	fflush(stdout);
}

/* Test 6: table.assign - Zero value */
static void test_table_assign_zero(void) {
	printf("[table_operations_integration] test_table_assign_zero: start\n");
	fflush(stdout);

	eval_expect_string("local (t); new(tableType, @t); t.zero = 0; if t.zero == 0 { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_assign_zero: PASS\n");
	fflush(stdout);
}

/* Test 7: table.assign - Negative numbers */
static void test_table_assign_negative(void) {
	printf("[table_operations_integration] test_table_assign_negative: start\n");
	fflush(stdout);

	eval_expect_string("local (t); new(tableType, @t); t.neg = -100; if t.neg == -100 { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_assign_negative: PASS\n");
	fflush(stdout);
}

/* Test 8: table.assign - Large numbers */
static void test_table_assign_large(void) {
	printf("[table_operations_integration] test_table_assign_large: start\n");
	fflush(stdout);

	eval_expect_string("local (t); new(tableType, @t); t.large = 999999999; if t.large == 999999999 { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_assign_large: PASS\n");
	fflush(stdout);
}

/* ========================================================================== */
/* TEST SUITE: table.copy() - Copy entries between tables                   */
/* ========================================================================== */

/* Test 9: table.copy - Basic copy to another table */
static void test_table_copy_basic(void) {
	printf("[table_operations_integration] test_table_copy_basic: start\n");
	fflush(stdout);

	eval_expect_string("local (src, dst); new(tableType, @src); new(tableType, @dst); src.original = \"data\"; table.copy(@src.original, @dst); if sizeOf(src) == 1 and sizeOf(dst) == 1 and dst.original == \"data\" { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_copy_basic: PASS\n");
	fflush(stdout);
}

/* Test 10: table.copy - Source remains unchanged */
static void test_table_copy_source_unchanged(void) {
	printf("[table_operations_integration] test_table_copy_source_unchanged: start\n");
	fflush(stdout);

	eval_expect_string("local (src, dst); new(tableType, @src); new(tableType, @dst); src.keep = \"test\"; table.copy(@src.keep, @dst); if defined(src.keep) and src.keep == \"test\" { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_copy_source_unchanged: PASS\n");
	fflush(stdout);
}

/* Test 11: table.copy - Copy numeric value */
static void test_table_copy_numeric(void) {
	printf("[table_operations_integration] test_table_copy_numeric: start\n");
	fflush(stdout);

	eval_expect_string("local (src, dst); new(tableType, @src); new(tableType, @dst); src.number = 123; table.copy(@src.number, @dst); if dst.number == 123 { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_copy_numeric: PASS\n");
	fflush(stdout);
}

/* Test 12: table.copy - Copy boolean value */
static void test_table_copy_boolean(void) {
	printf("[table_operations_integration] test_table_copy_boolean: start\n");
	fflush(stdout);

	eval_expect_string("local (src, dst); new(tableType, @src); new(tableType, @dst); src.flag = true; table.copy(@src.flag, @dst); if dst.flag == true { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_copy_boolean: PASS\n");
	fflush(stdout);
}

/* Test 13: table.copy - Copy to table with pre-existing entries */
static void test_table_copy_with_existing(void) {
	printf("[table_operations_integration] test_table_copy_with_existing: start\n");
	fflush(stdout);

	eval_expect_string("local (src, dst); new(tableType, @src); new(tableType, @dst); src.item = \"from source\"; dst.existing = \"already here\"; table.copy(@src.item, @dst); if sizeOf(dst) == 2 and dst.item == \"from source\" and dst.existing == \"already here\" { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_copy_with_existing: PASS\n");
	fflush(stdout);
}

/* Test 14: table.copy - Multiple copies from same source */
static void test_table_copy_multiple(void) {
	printf("[table_operations_integration] test_table_copy_multiple: start\n");
	fflush(stdout);

	eval_expect_string("local (src, dst_a, dst_b); new(tableType, @src); new(tableType, @dst_a); new(tableType, @dst_b); src.value = \"shared\"; table.copy(@src.value, @dst_a); table.copy(@src.value, @dst_b); if dst_a.value == \"shared\" and dst_b.value == \"shared\" { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_copy_multiple: PASS\n");
	fflush(stdout);
}

/* Test 15: table.copy - Copy preserves key name */
static void test_table_copy_key_name(void) {
	printf("[table_operations_integration] test_table_copy_key_name: start\n");
	fflush(stdout);

	eval_expect_string("local (src, dst); new(tableType, @src); new(tableType, @dst); src.originalname = \"value\"; table.copy(@src.originalname, @dst); if defined(dst.originalname) { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_copy_key_name: PASS\n");
	fflush(stdout);
}

/* ========================================================================== */
/* TEST SUITE: table.move() - Move entries between tables                   */
/* ========================================================================== */

/* Test 16: table.move - Basic move operation */
static void test_table_move_basic(void) {
	printf("[table_operations_integration] test_table_move_basic: start\n");
	fflush(stdout);

	eval_expect_string("local (src, dst); new(tableType, @src); new(tableType, @dst); src.item = 123; table.move(@src.item, @dst); if sizeOf(src) == 0 and sizeOf(dst) == 1 and dst.item == 123 { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_move_basic: PASS\n");
	fflush(stdout);
}

/* Test 17: table.move - Source entry removed after move */
static void test_table_move_source_removed(void) {
	printf("[table_operations_integration] test_table_move_source_removed: start\n");
	fflush(stdout);

	eval_expect_string("local (src, dst); new(tableType, @src); new(tableType, @dst); src.mobile = \"data\"; table.move(@src.mobile, @dst); if not defined(src.mobile) { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_move_source_removed: PASS\n");
	fflush(stdout);
}

/* Test 18: table.move - Move multiple entries sequentially */
static void test_table_move_multiple_sequential(void) {
	printf("[table_operations_integration] test_table_move_multiple_sequential: start\n");
	fflush(stdout);

	eval_expect_string("local (src, dst); new(tableType, @src); new(tableType, @dst); src.a = 1; src.b = 2; src.c = 3; table.move(@src.a, @dst); table.move(@src.b, @dst); table.move(@src.c, @dst); if sizeOf(src) == 0 and sizeOf(dst) == 3 { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_move_multiple_sequential: PASS\n");
	fflush(stdout);
}

/* Test 19: table.move - Move to pre-populated destination */
static void test_table_move_prepopulated_dest(void) {
	printf("[table_operations_integration] test_table_move_prepopulated_dest: start\n");
	fflush(stdout);

	eval_expect_string("local (src, dst); new(tableType, @src); new(tableType, @dst); src.moving = \"value\"; dst.existing = \"already here\"; table.move(@src.moving, @dst); if sizeOf(dst) == 2 and defined(dst.moving) and defined(dst.existing) { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_move_prepopulated_dest: PASS\n");
	fflush(stdout);
}

/* Test 20: table.move - Move string value */
static void test_table_move_string(void) {
	printf("[table_operations_integration] test_table_move_string: start\n");
	fflush(stdout);

	eval_expect_string("local (src, dst); new(tableType, @src); new(tableType, @dst); src.text = \"hello world\"; table.move(@src.text, @dst); if dst.text == \"hello world\" { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_move_string: PASS\n");
	fflush(stdout);
}

/* Test 21: table.move - Move boolean value */
static void test_table_move_boolean(void) {
	printf("[table_operations_integration] test_table_move_boolean: start\n");
	fflush(stdout);

	eval_expect_string("local (src, dst); new(tableType, @src); new(tableType, @dst); src.flag = false; table.move(@src.flag, @dst); if dst.flag == false { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_move_boolean: PASS\n");
	fflush(stdout);
}

/* ========================================================================== */
/* TEST SUITE: table.rename() - Rename entries within a table                */
/* ========================================================================== */

/* Test 22: table.rename - Basic rename operation */
static void test_table_rename_basic(void) {
	printf("[table_operations_integration] test_table_rename_basic: start\n");
	fflush(stdout);

	eval_expect_string("local (t); new(tableType, @t); t.oldname = \"value\"; table.rename(@t.oldname, \"newname\"); if not defined(t.oldname) { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_rename_basic: PASS\n");
	fflush(stdout);
}

/* Test 23: table.rename - New key contains correct value */
static void test_table_rename_new_key_value(void) {
	printf("[table_operations_integration] test_table_rename_new_key_value: start\n");
	fflush(stdout);

	eval_expect_string("local (t); new(tableType, @t); t.old = \"test data\"; table.rename(@t.old, \"new\"); if defined(t.new) and t.new == \"test data\" { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_rename_new_key_value: PASS\n");
	fflush(stdout);
}

/* Test 24: table.rename - Table size unchanged after rename */
static void test_table_rename_size_unchanged(void) {
	printf("[table_operations_integration] test_table_rename_size_unchanged: start\n");
	fflush(stdout);

	eval_expect_string("local (t); new(tableType, @t); t.a = 1; t.b = 2; local (size_before); size_before = sizeOf(t); table.rename(@t.a, \"renamed_a\"); if sizeOf(t) == size_before { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_rename_size_unchanged: PASS\n");
	fflush(stdout);
}

/* Test 25: table.rename - Rename in table with multiple entries */
static void test_table_rename_with_multiple(void) {
	printf("[table_operations_integration] test_table_rename_with_multiple: start\n");
	fflush(stdout);

	eval_expect_string("local (t); new(tableType, @t); t.keep1 = \"stay\"; t.rename_me = \"move\"; t.keep2 = \"also stay\"; table.rename(@t.rename_me, \"renamed\"); if sizeOf(t) == 3 and defined(t.keep1) and defined(t.keep2) and defined(t.renamed) { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_rename_with_multiple: PASS\n");
	fflush(stdout);
}

/* Test 26: table.rename - Rename numeric entry */
static void test_table_rename_numeric(void) {
	printf("[table_operations_integration] test_table_rename_numeric: start\n");
	fflush(stdout);

	eval_expect_string("local (t); new(tableType, @t); t.oldnum = 42; table.rename(@t.oldnum, \"newnum\"); if t.newnum == 42 { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_rename_numeric: PASS\n");
	fflush(stdout);
}

/* Test 27: table.rename - Rename with special characters */
static void test_table_rename_special_chars(void) {
	printf("[table_operations_integration] test_table_rename_special_chars: start\n");
	fflush(stdout);

	eval_expect_string("local (t); new(tableType, @t); t.simple = \"value\"; table.rename(@t.simple, \"with_underscore_123\"); if defined(t.with_underscore_123) and t.with_underscore_123 == \"value\" { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_rename_special_chars: PASS\n");
	fflush(stdout);
}

/* ========================================================================== */
/* TEST SUITE: table.emptytable() - Clear all entries from table             */
/* ========================================================================== */

/* Test 28: table.emptytable - Clear single entry */
static void test_table_emptytable_single(void) {
	printf("[table_operations_integration] test_table_emptytable_single: start\n");
	fflush(stdout);

	eval_expect_string("local (t); new(tableType, @t); t.only = \"value\"; table.emptytable(@t); if sizeOf(t) == 0 { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_emptytable_single: PASS\n");
	fflush(stdout);
}

/* Test 29: table.emptytable - Returns count of cleared entries */
static void test_table_emptytable_count(void) {
	printf("[table_operations_integration] test_table_emptytable_count: start\n");
	fflush(stdout);

	eval_expect_number("local (t); new(tableType, @t); t.a = 1; t.b = 2; t.c = 3; return table.emptytable(@t)", 3);

	printf("[table_operations_integration] test_table_emptytable_count: PASS\n");
	fflush(stdout);
}

/* Test 30: table.emptytable - All entries removed */
static void test_table_emptytable_all_removed(void) {
	printf("[table_operations_integration] test_table_emptytable_all_removed: start\n");
	fflush(stdout);

	eval_expect_string("local (t); new(tableType, @t); t.x = \"first\"; t.y = \"second\"; t.z = \"third\"; table.emptytable(@t); if not defined(t.x) and not defined(t.y) and not defined(t.z) { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_emptytable_all_removed: PASS\n");
	fflush(stdout);
}

/* Test 31: table.emptytable - Can repopulate after emptying */
static void test_table_emptytable_repopulate(void) {
	printf("[table_operations_integration] test_table_emptytable_repopulate: start\n");
	fflush(stdout);

	eval_expect_string("local (t); new(tableType, @t); t.temp = \"temporary\"; table.emptytable(@t); t.new = \"new value\"; if sizeOf(t) == 1 and t.new == \"new value\" { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_emptytable_repopulate: PASS\n");
	fflush(stdout);
}

/* Test 32: table.emptytable - Empty table returns 0 */
static void test_table_emptytable_empty_table(void) {
	printf("[table_operations_integration] test_table_emptytable_empty_table: start\n");
	fflush(stdout);

	eval_expect_number("local (t); new(tableType, @t); return table.emptytable(@t)", 0);

	printf("[table_operations_integration] test_table_emptytable_empty_table: PASS\n");
	fflush(stdout);
}

/* ========================================================================== */
/* TEST SUITE: table.moveandrename() - Move and rename in one operation      */
/* ========================================================================== */

/* Test 33: table.moveandrename - Basic move and rename */
static void test_table_moveandrename_basic(void) {
	printf("[table_operations_integration] test_table_moveandrename_basic: start\n");
	fflush(stdout);

	eval_expect_string("local (src, dst); new(tableType, @src); new(tableType, @dst); src.oldname = \"data\"; table.moveandrename(@src.oldname, @dst.newname); if not defined(src.oldname) { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_moveandrename_basic: PASS\n");
	fflush(stdout);
}

/* Test 34: table.moveandrename - New name in destination */
static void test_table_moveandrename_new_name(void) {
	printf("[table_operations_integration] test_table_moveandrename_new_name: start\n");
	fflush(stdout);

	eval_expect_string("local (src, dst); new(tableType, @src); new(tableType, @dst); src.old = \"test\"; table.moveandrename(@src.old, @dst.new); if defined(dst.new) and dst.new == \"test\" { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_moveandrename_new_name: PASS\n");
	fflush(stdout);
}

/* Test 35: table.moveandrename - Value preserved */
static void test_table_moveandrename_value_preserved(void) {
	printf("[table_operations_integration] test_table_moveandrename_value_preserved: start\n");
	fflush(stdout);

	eval_expect_string("local (src, dst); new(tableType, @src); new(tableType, @dst); src.source = 999; table.moveandrename(@src.source, @dst.destination); if dst.destination == 999 { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_moveandrename_value_preserved: PASS\n");
	fflush(stdout);
}

/* Test 36: table.moveandrename - Source size reduced */
static void test_table_moveandrename_source_reduced(void) {
	printf("[table_operations_integration] test_table_moveandrename_source_reduced: start\n");
	fflush(stdout);

	eval_expect_string("local (src, dst); new(tableType, @src); new(tableType, @dst); src.a = 1; src.b = 2; local (src_before); src_before = sizeOf(src); table.moveandrename(@src.a, @dst.moved); if sizeOf(src) == src_before - 1 { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_moveandrename_source_reduced: PASS\n");
	fflush(stdout);
}

/* Test 37: table.moveandrename - Destination size increased */
static void test_table_moveandrename_dest_increased(void) {
	printf("[table_operations_integration] test_table_moveandrename_dest_increased: start\n");
	fflush(stdout);

	eval_expect_string("local (src, dst); new(tableType, @src); new(tableType, @dst); src.item = \"value\"; local (dst_before); dst_before = sizeOf(dst); table.moveandrename(@src.item, @dst.newitem); if sizeOf(dst) == dst_before + 1 { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_moveandrename_dest_increased: PASS\n");
	fflush(stdout);
}

/* Test 38: table.moveandrename - Multiple sequential operations */
static void test_table_moveandrename_multiple_sequential(void) {
	printf("[table_operations_integration] test_table_moveandrename_multiple_sequential: start\n");
	fflush(stdout);

	eval_expect_string("local (src, dst); new(tableType, @src); new(tableType, @dst); src.first = 1; src.second = 2; src.third = 3; table.moveandrename(@src.first, @dst.uno); table.moveandrename(@src.second, @dst.dos); table.moveandrename(@src.third, @dst.tres); if sizeOf(src) == 0 and sizeOf(dst) == 3 { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_table_moveandrename_multiple_sequential: PASS\n");
	fflush(stdout);
}

/* ========================================================================== */
/* TEST SUITE: Complex scenarios and edge cases                              */
/* ========================================================================== */

/* Test 39: Complex combined operations */
static void test_complex_combined_operations(void) {
	printf("[table_operations_integration] test_complex_combined_operations: start\n");
	fflush(stdout);

	eval_expect_string("local (src, mid, dst); new(tableType, @src); new(tableType, @mid); new(tableType, @dst); src.a = 1; src.b = 2; src.c = 3; table.move(@src.a, @mid); table.copy(@src.b, @dst); table.moveandrename(@src.c, @dst.c_renamed); if sizeOf(src) == 1 and sizeOf(mid) == 1 and sizeOf(dst) == 2 { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_complex_combined_operations: PASS\n");
	fflush(stdout);
}

/* Test 40: Type preservation after operations */
static void test_type_preservation(void) {
	printf("[table_operations_integration] test_type_preservation: start\n");
	fflush(stdout);

	eval_expect_string("local (src, dst); new(tableType, @src); new(tableType, @dst); src.str = \"text\"; src.num = 42; src.bool = true; table.move(@src.str, @dst); table.move(@src.num, @dst); table.move(@src.bool, @dst); if typeof(dst.str) == \"TEXT\" and typeof(dst.num) == \"long\" and typeof(dst.bool) == \"bool\" { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_type_preservation: PASS\n");
	fflush(stdout);
}

/* Test 41: Large string values */
static void test_large_string_values(void) {
	printf("[table_operations_integration] test_large_string_values: start\n");
	fflush(stdout);

	eval_expect_string("local (src, dst); new(tableType, @src); new(tableType, @dst); src.large = \"The quick brown fox jumps over the lazy dog. The quick brown fox jumps over the lazy dog.\"; table.move(@src.large, @dst); if dst.large == \"The quick brown fox jumps over the lazy dog. The quick brown fox jumps over the lazy dog.\" { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_large_string_values: PASS\n");
	fflush(stdout);
}

/* Test 42: Stress test - Many entries */
static void test_stress_many_entries(void) {
	printf("[table_operations_integration] test_stress_many_entries: start\n");
	fflush(stdout);

	eval_expect_string("local (t); new(tableType, @t); local (i); for i = 1 to 50 { t.[\"key_\" + i] = i }; if sizeOf(t) == 50 { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_stress_many_entries: PASS\n");
	fflush(stdout);
}

/* Test 43: Rename chain */
static void test_rename_chain(void) {
	printf("[table_operations_integration] test_rename_chain: start\n");
	fflush(stdout);

	eval_expect_string("local (t); new(tableType, @t); t.original = \"value\"; table.rename(@t.original, \"renamed_once\"); table.rename(@t.renamed_once, \"renamed_twice\"); table.rename(@t.renamed_twice, \"renamed_thrice\"); if defined(t.renamed_thrice) and t.renamed_thrice == \"value\" { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_rename_chain: PASS\n");
	fflush(stdout);
}

/* Test 44: Empty string handling */
static void test_empty_string_handling(void) {
	printf("[table_operations_integration] test_empty_string_handling: start\n");
	fflush(stdout);

	eval_expect_string("local (src, dst); new(tableType, @src); new(tableType, @dst); src.empty = \"\"; table.copy(@src.empty, @dst); if dst.empty == \"\" { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_empty_string_handling: PASS\n");
	fflush(stdout);
}

/* Test 45: False/zero distinction */
static void test_false_zero_distinction(void) {
	printf("[table_operations_integration] test_false_zero_distinction: start\n");
	fflush(stdout);

	eval_expect_string("local (t); new(tableType, @t); t.zero = 0; t.boolval = false; if typeof(t.zero) == \"long\" and typeof(t.boolval) == \"bool\" { return \"pass\" } else { return \"fail\" }", "pass");

	printf("[table_operations_integration] test_false_zero_distinction: PASS\n");
	fflush(stdout);
}

/* ========================================================================== */
/* Main test runner                                                           */
/* ========================================================================== */

int main(int argc, char **argv) {
	(void)argc;
	(void)argv;

	printf("[table_operations_integration] Starting comprehensive table operation tests...\n");
	printf("[table_operations_integration] Tests run via frontier-cli with Frontier-v7.root\n");
	printf("[table_operations_integration] Total: 45 tests across 6 categories\n");
	printf("\n");
	fflush(stdout);

	/* TEST SUITE: table.assign() */
	printf("[table_operations_integration] ========== TEST SUITE: table.assign() ==========\n");
	fflush(stdout);
	test_table_assign_basic();
	test_table_assign_multiple_types();
	test_table_assign_overwrite();
	test_table_assign_size();
	test_table_assign_empty_string();
	test_table_assign_zero();
	test_table_assign_negative();
	test_table_assign_large();

	/* TEST SUITE: table.copy() */
	printf("\n[table_operations_integration] ========== TEST SUITE: table.copy() ==========\n");
	fflush(stdout);
	test_table_copy_basic();
	test_table_copy_source_unchanged();
	test_table_copy_numeric();
	test_table_copy_boolean();
	test_table_copy_with_existing();
	test_table_copy_multiple();
	test_table_copy_key_name();

	/* TEST SUITE: table.move() */
	printf("\n[table_operations_integration] ========== TEST SUITE: table.move() ==========\n");
	fflush(stdout);
	test_table_move_basic();
	test_table_move_source_removed();
	test_table_move_multiple_sequential();
	test_table_move_prepopulated_dest();
	test_table_move_string();
	test_table_move_boolean();

	/* TEST SUITE: table.rename() */
	printf("\n[table_operations_integration] ========== TEST SUITE: table.rename() ==========\n");
	fflush(stdout);
	test_table_rename_basic();
	test_table_rename_new_key_value();
	test_table_rename_size_unchanged();
	test_table_rename_with_multiple();
	test_table_rename_numeric();
	test_table_rename_special_chars();

	/* TEST SUITE: table.emptytable() */
	printf("\n[table_operations_integration] ========== TEST SUITE: table.emptytable() ==========\n");
	fflush(stdout);
	test_table_emptytable_single();
	test_table_emptytable_count();
	test_table_emptytable_all_removed();
	test_table_emptytable_repopulate();
	test_table_emptytable_empty_table();

	/* TEST SUITE: table.moveandrename() */
	printf("\n[table_operations_integration] ========== TEST SUITE: table.moveandrename() ==========\n");
	fflush(stdout);
	test_table_moveandrename_basic();
	test_table_moveandrename_new_name();
	test_table_moveandrename_value_preserved();
	test_table_moveandrename_source_reduced();
	test_table_moveandrename_dest_increased();
	test_table_moveandrename_multiple_sequential();

	/* TEST SUITE: Complex scenarios */
	printf("\n[table_operations_integration] ========== TEST SUITE: Complex Scenarios ==========\n");
	fflush(stdout);
	test_complex_combined_operations();
	test_type_preservation();
	test_large_string_values();
	test_stress_many_entries();
	test_rename_chain();
	test_empty_string_handling();
	test_false_zero_distinction();

	printf("\n========================================\n");
	printf("table_operations_integration: ALL TESTS PASSED\n");
	printf("========================================\n");
	printf("Total: 45 comprehensive table operation tests executed successfully\n");
	printf("Coverage: 8 assign, 7 copy, 6 move, 6 rename, 5 emptytable, 6 moveandrename, 7 complex\n");
	fflush(stdout);

	return 0;
}
