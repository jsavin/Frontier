/*
 * Test: file verb parameter state isolation
 *
 * Tests that flnextparamislast global state is properly reset between verb calls.
 *
 * Bug Context:
 * - flnextparamislast is a global variable used during parameter extraction
 * - If a verb sets it to true but doesn't consume all parameters (e.g., file.open with 1 param),
 *   it remains true for the next verb call
 * - This causes 2-parameter verbs to fail with "too many parameters" error
 *
 * Fix:
 * - Reset flnextparamislast = false at the start of portable_filefunctionvalue()
 *
 * This test ensures the fix prevents regression.
 */

#include "../frontier-cli/frontier-cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_OUTPUT_DIR "tests/tmp/unit/"

static int test_writeline_after_open(void) {
	const char *script =
		"local(f); "
		"f = file.open(\"" TEST_OUTPUT_DIR "test_state_writeline.txt\"); "
		"file.writeline(f, \"test line\"); "
		"file.close(f); "
		"return true";

	tyvaluerecord result;
	if (!execute_script(script, &result)) {
		fprintf(stderr, "FAIL: file.writeline after file.open failed\n");
		return 0;
	}

	if (result.valuetype != booleanvaluetype || !result.data.flvalue) {
		fprintf(stderr, "FAIL: Expected true, got different result\n");
		return 0;
	}

	printf("PASS: file.writeline after file.open\n");
	return 1;
}

static int test_write_after_open(void) {
	const char *script =
		"local(f); "
		"f = file.open(\"" TEST_OUTPUT_DIR "test_state_write.bin\"); "
		"file.write(f, \"binary\"); "
		"file.close(f); "
		"return true";

	tyvaluerecord result;
	if (!execute_script(script, &result)) {
		fprintf(stderr, "FAIL: file.write after file.open failed\n");
		return 0;
	}

	if (result.valuetype != booleanvaluetype || !result.data.flvalue) {
		fprintf(stderr, "FAIL: Expected true, got different result\n");
		return 0;
	}

	printf("PASS: file.write after file.open\n");
	return 1;
}

static int test_setposition_after_open(void) {
	const char *script =
		"local(f); "
		"f = file.open(\"" TEST_OUTPUT_DIR "test_state_setpos.txt\"); "
		"file.setposition(f, 0); "
		"file.close(f); "
		"return true";

	tyvaluerecord result;
	if (!execute_script(script, &result)) {
		fprintf(stderr, "FAIL: file.setposition after file.open failed\n");
		return 0;
	}

	if (result.valuetype != booleanvaluetype || !result.data.flvalue) {
		fprintf(stderr, "FAIL: Expected true, got different result\n");
		return 0;
	}

	printf("PASS: file.setposition after file.open\n");
	return 1;
}

static int test_compare_after_open(void) {
	const char *script =
		"file.open(\"" TEST_OUTPUT_DIR "a.txt\"); "
		"return file.compare(\"" TEST_OUTPUT_DIR "test_state_writeline.txt\", \"" TEST_OUTPUT_DIR "test_state_write.bin\")";

	tyvaluerecord result;
	if (!execute_script(script, &result)) {
		fprintf(stderr, "FAIL: file.compare after file.open failed\n");
		return 0;
	}

	/* compare returns boolean - we don't care about the value, just that it succeeded */
	if (result.valuetype != booleanvaluetype) {
		fprintf(stderr, "FAIL: Expected boolean result\n");
		return 0;
	}

	printf("PASS: file.compare after file.open\n");
	return 1;
}

int main(void) {
	int passed = 0;
	int total = 4;

	/* Create output directory */
	system("mkdir -p " TEST_OUTPUT_DIR);

	/* Initialize Frontier runtime */
	if (!initialize_frontier_cli(NULL)) {
		fprintf(stderr, "FATAL: Failed to initialize Frontier runtime\n");
		return 1;
	}

	printf("Testing file verb parameter state isolation...\n\n");

	passed += test_writeline_after_open();
	passed += test_write_after_open();
	passed += test_setposition_after_open();
	passed += test_compare_after_open();

	printf("\n========================================\n");
	printf("Results: %d/%d tests passed\n", passed, total);

	if (passed == total) {
		printf("✓ All parameter state isolation tests passed!\n");
		return 0;
	} else {
		printf("✗ Some tests failed\n");
		return 1;
	}
}
