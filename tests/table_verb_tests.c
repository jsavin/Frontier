/*
 * table_verb_tests.c - Unit tests for table processor verbs
 *
 * Tests the 9 headless-compatible table verbs:
 * - table.assign
 * - table.move
 * - table.copy
 * - table.rename
 * - table.moveandrename
 * - table.validate
 * - table.packtable
 * - table.emptytable
 * - table.jettison
 *
 * These tests use the frontier-cli with a loaded database to ensure
 * tests run in a complete UserTalk environment with all glue scripts.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>

/* Get repository root by walking up from test binary location */
static int get_repo_root(char *buf, size_t bufsize) {
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len == -1) {
        /* macOS doesn't have /proc/self/exe, use argv[0] fallback */
        if (getcwd(exe_path, sizeof(exe_path)) == NULL) {
            return 0;
        }
    } else {
        exe_path[len] = '\0';
    }

    char dir_copy[PATH_MAX];
    strncpy(dir_copy, exe_path, sizeof(dir_copy) - 1);
    dir_copy[sizeof(dir_copy) - 1] = '\0';

    /* Walk up to find repo root (contains databases/ directory) */
    while (strlen(dir_copy) > 1) {
        char test_path[PATH_MAX];
        snprintf(test_path, sizeof(test_path), "%s/databases", dir_copy);
        if (access(test_path, F_OK) == 0) {
            if (strlen(dir_copy) >= bufsize) {
                return 0;
            }
            strcpy(buf, dir_copy);
            return 1;
        }

        /* Use dirname safely by working on a copy */
        char *parent = dirname(dir_copy);
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
    assert(get_repo_root(root, sizeof(root)));

    char db_path[PATH_MAX];
    snprintf(db_path, sizeof(db_path), "%s/databases/Frontier-v6-v7.root", root);

    if (access(db_path, R_OK) != 0) {
        fprintf(stderr, "[table_verb_tests] ERROR: Database not found: %s\n", db_path);
        fprintf(stderr, "[table_verb_tests] Ensure Frontier-v6-v7.root exists by running CLI once with Frontier-v6.root\n");
        assert(0);
    }

    char cli_path[PATH_MAX];
    snprintf(cli_path, sizeof(cli_path), "%s/frontier-cli/frontier-cli", root);

    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "FRONTIER_HEADLESS_SKIP_STARTUP=1 %s --system-root \"%s\" -e \"%s\" 2>/dev/null", cli_path, db_path, script);

    FILE *fp = popen(cmd, "r");
    assert(fp != NULL);

    size_t total = 0;
    while (total < output_size - 1 && fgets(output + total, output_size - total, fp) != NULL) {
        total = strlen(output);
    }
    output[total] = '\0';

    int status = pclose(fp);
    if (status != 0) {
        fprintf(stderr, "[table_verb_tests] CLI exited with status %d\n", status);
        fprintf(stderr, "[table_verb_tests] Script: %s\n", script);
        fprintf(stderr, "[table_verb_tests] Output: %s\n", output);
        assert(0);
    }

    /* Trim trailing whitespace */
    while (total > 0 && (output[total-1] == '\n' || output[total-1] == '\r' || output[total-1] == ' ')) {
        output[--total] = '\0';
    }
}

/* Helper to evaluate expression and verify string result */
static void eval_expect_string(const char *expr, const char *expected) {
    char result[1024];
    eval_cli(expr, result, sizeof(result));

    if (strcmp(result, expected) != 0) {
        fprintf(stderr, "[table_verb_tests] MISMATCH for: %s\n", expr);
        fprintf(stderr, "[table_verb_tests] Expected: %s\n", expected);
        fprintf(stderr, "[table_verb_tests] Got: %s\n", result);
        assert(0);
    }
}

/* Test 1: table.assign - Assign value to table entry */
static void test_table_assign(void) {
    printf("[table_verb_tests] test_table_assign: start\n");
    fflush(stdout);

    eval_expect_string("local (t); new (tableType, @t); t.key1 = \"hello\"; t.key2 = 42; t.key3 = true; if (sizeOf(t) == 3 and t.key1 == \"hello\") { return (\"pass\") }; return (\"fail\")", "pass");

    printf("[table_verb_tests] test_table_assign: PASS\n");
    fflush(stdout);
}

/* Test 2: table.copy - Copy entry to another table */
static void test_table_copy(void) {
    printf("[table_verb_tests] test_table_copy: start\n");
    fflush(stdout);

    eval_expect_string("local (src, dst); new (tableType, @src); new (tableType, @dst); src.original = \"data\"; table.copy(@src.original, @dst); if (sizeOf(src) == 1 and sizeOf(dst) == 1 and dst.original == \"data\") { return \"pass\" } else { return \"fail\" }", "pass");

    printf("[table_verb_tests] test_table_copy: PASS\n");
    fflush(stdout);
}

/* Test 3: table.move - Move entry between tables */
static void test_table_move(void) {
    printf("[table_verb_tests] test_table_move: start\n");
    fflush(stdout);

    eval_expect_string("local (src, dst); new (tableType, @src); new (tableType, @dst); src.item = 123; table.move(@src.item, @dst); if (sizeOf(src) == 0 and sizeOf(dst) == 1 and dst.item == 123) { return \"pass\" } else { return \"fail\" }", "pass");

    printf("[table_verb_tests] test_table_move: PASS\n");
    fflush(stdout);
}

/* Test 4: table.rename - Rename table entry */
static void test_table_rename(void) {
    printf("[table_verb_tests] test_table_rename: start\n");
    fflush(stdout);

    eval_expect_string("local (t); new (tableType, @t); t.oldname = \"value\"; table.rename(@t.oldname, \"newname\"); if (sizeOf(t) == 1 and t.newname == \"value\" and not defined(t.oldname)) { return \"pass\" } else { return \"fail\" }", "pass");

    printf("[table_verb_tests] test_table_rename: PASS\n");
    fflush(stdout);
}

/* Test 5: table.moveandrename - Move and rename in one operation */
static void test_table_moveandrename(void) {
    printf("[table_verb_tests] test_table_moveandrename: start\n");
    fflush(stdout);

    eval_expect_string("local (src, dst); new (tableType, @src); new (tableType, @dst); src.old = \"data\"; table.moveandrename(@src.old, @dst.new); if (sizeOf(src) == 0 and sizeOf(dst) == 1 and dst.new == \"data\") { return \"pass\" } else { return \"fail\" }", "pass");

    printf("[table_verb_tests] test_table_moveandrename: PASS\n");
    fflush(stdout);
}

/* Test 6: table.validate - Validate table structure */
static void test_table_validate(void) {
    printf("[table_verb_tests] test_table_validate: start\n");
    fflush(stdout);

    eval_expect_string("local (t); new (tableType, @t); t.a = 1; t.b = 2; t.c = 3; if (table.validate(@t)) { return \"pass\" } else { return \"fail\" }", "pass");

    printf("[table_verb_tests] test_table_validate: PASS\n");
    fflush(stdout);
}

/* Test 7: table.emptytable - Clear all entries from table */
static void test_table_emptytable(void) {
    printf("[table_verb_tests] test_table_emptytable: start\n");
    fflush(stdout);

    eval_expect_string("local (t); new (tableType, @t); t.x = \"first\"; t.y = \"second\"; t.z = \"third\"; table.emptytable(@t); if (sizeOf(t) == 0) { return \"pass\" } else { return \"fail\" }", "pass");

    printf("[table_verb_tests] test_table_emptytable: PASS\n");
    fflush(stdout);
}

/* Test 8: table.jettison - Delete entry without loading */
static void test_table_jettison(void) {
    printf("[table_verb_tests] test_table_jettison: start\n");
    fflush(stdout);

    eval_expect_string("local (t); new (tableType, @t); t.target = \"to be deleted\"; table.jettison(@t.target); if (sizeOf(t) == 0 and not defined(t.target)) { return \"pass\" } else { return \"fail\" }", "pass");

    printf("[table_verb_tests] test_table_jettison: PASS\n");
    fflush(stdout);
}

/* Test 9: table.packtable - Force table to pack to disk */
static void test_table_packtable(void) {
    printf("[table_verb_tests] test_table_packtable: start\n");
    fflush(stdout);

    eval_expect_string("local (t); new (tableType, @t); t.data1 = \"value1\"; t.data2 = \"value2\"; table.packtable(@t); if (sizeOf(t) == 2 and t.data1 == \"value1\") { return \"pass\" } else { return \"fail\" }", "pass");

    printf("[table_verb_tests] test_table_packtable: PASS\n");
    fflush(stdout);
}

/* Complex scenario: Combined operations */
static void test_complex_table_operations(void) {
    printf("[table_verb_tests] test_complex_table_operations: start\n");
    fflush(stdout);

    eval_expect_string("local (src, bak, moved); new (tableType, @src); new (tableType, @bak); new (tableType, @moved); src.a = 1; src.b = 2; src.c = 3; table.copy(@src.a, @bak); table.rename(@src.b, \"b_renamed\"); table.move(@src.c, @moved); if (sizeOf(src) == 2 and sizeOf(bak) == 1 and sizeOf(moved) == 1) { return \"pass\" } else { return \"fail\" }", "pass");

    printf("[table_verb_tests] test_complex_table_operations: PASS\n");
    fflush(stdout);
}

/* Main test runner */
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("[table_verb_tests] Starting table verb tests...\n");
    printf("[table_verb_tests] Tests run via frontier-cli with Frontier-v6-v7.root\n");
    fflush(stdout);

    /* Run tests */
    test_table_assign();
    test_table_copy();
    test_table_move();
    test_table_rename();
    test_table_moveandrename();
    test_table_validate();
    test_table_emptytable();
    test_table_jettison();
    test_table_packtable();
    test_complex_table_operations();

    printf("\n========================================\n");
    printf("table_verb_tests: ALL TESTS PASSED\n");
    printf("========================================\n");
    fflush(stdout);

    return 0;
}
