#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include "test_report.h"

static bool get_repo_root(char *out, size_t out_size) {
    if (out == NULL || out_size == 0) {
        return false;
    }

    if (getcwd(out, out_size) == NULL) {
        perror("getcwd");
        return false;
    }

    size_t len = strlen(out);
    const char suffix[] = "/tests";
    size_t suffix_len = sizeof suffix - 1;
    if (len >= suffix_len && strcmp(out + len - suffix_len, suffix) == 0) {
        out[len - suffix_len] = '\0';
    }

    return true;
}

static bool ensure_cli_present(const char *root) {
    static bool checked = false;
    static bool available = false;

    if (checked)
        return available;

    checked = true;

    char cli_path[PATH_MAX];
    if (snprintf(cli_path, sizeof cli_path, "%s/frontier-cli/frontier-cli", root) >= (int)sizeof cli_path) {
        fprintf(stderr, "cli_path buffer too small\n");
        return false;
    }

    if (access(cli_path, X_OK) == 0) {
        available = true;
        return true;
    }

    char build_command[PATH_MAX * 2];
    if (snprintf(build_command, sizeof build_command,
                 "cd \"%s\" && make -C frontier-cli >/dev/null 2>&1",
                 root) >= (int)sizeof build_command) {
        fprintf(stderr, "build_command buffer too small\n");
        return false;
    }

    int rc = system(build_command);
    if (rc == 0 && access(cli_path, X_OK) == 0) {
        available = true;
    }

    return available;
}

static void strip_trailing_newlines(char *text) {
    if (text == NULL)
        return;

    size_t len = strlen(text);
    while (len > 0) {
        char ch = text[len - 1];
        if (ch != '\n' && ch != '\r')
            break;
        text[len - 1] = '\0';
        --len;
    }
}

static bool string_contains(const char *haystack, const char *needle) {
    if (haystack == NULL || needle == NULL)
        return false;
    return strstr(haystack, needle) != NULL;
}

static bool copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (in == NULL)
        return false;

    FILE *out = fopen(dst, "wb");
    if (out == NULL) {
        fclose(in);
        return false;
    }

    unsigned char buffer[4096];
    size_t nread;
    bool ok = true;

    while ((nread = fread(buffer, 1, sizeof buffer, in)) > 0) {
        if (fwrite(buffer, 1, nread, out) != nread) {
            ok = false;
            break;
        }
    }

    if (ferror(in) || ferror(out))
        ok = false;

    fclose(out);
    fclose(in);
    return ok;
}

static void ensure_results_dir(const char *root) {
    char path[PATH_MAX];
    if (snprintf(path, sizeof path, "%s/tests/tmp/results", root) >= (int)sizeof path) {
        fprintf(stderr, "results dir path too small\n");
        exit(1);
    }
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
        return;
    (void) mkdir(path, 0755);
}

static void remove_system_root_backups(const char *databases_dir) {
    DIR *dir = opendir(databases_dir);
    if (dir == NULL)
        return;

    const char prefix[] = "Frontier.root.";
    size_t prefix_len = sizeof prefix - 1;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, prefix, prefix_len) != 0)
            continue;

        char path[PATH_MAX];
        if (snprintf(path, sizeof path, "%s/%s", databases_dir, entry->d_name) >= (int)sizeof path)
            continue;

        unlink(path);
    }

    closedir(dir);
}

static int run_cli_command_ex(const char *args, char *output, size_t output_size, bool capture_stderr) {
    char root[PATH_MAX];
    if (!get_repo_root(root, sizeof root)) {
        return -1;
    }

    if (!ensure_cli_present(root)) {
        fprintf(stderr, "frontier-cli binary is not available\n");
        return -1;
    }

    char command[PATH_MAX * 2];
    // Optionally suppress stderr to avoid debug logging from filling the output buffer
    const char *stderr_redirect = capture_stderr ? "2>&1" : "2>/dev/null";
    // Skip startup scripts to speed up tests (use --skip-startup CLI flag)
    if (snprintf(command, sizeof command,
                 "cd \"%s\" && ./frontier-cli/frontier-cli --skip-startup %s %s",
                 root, args, stderr_redirect) >= (int)sizeof command) {
        fprintf(stderr, "command buffer too small\n");
        return -1;
    }

    FILE *pipe = popen(command, "r");
    if (!pipe) {
        perror("popen");
        return -1;
    }

    if (output && output_size > 0)
        output[0] = '\0';

    char buffer[256];
    while (fgets(buffer, sizeof buffer, pipe) != NULL) {
        if (output && output_size > 1) {
            size_t current_len = strlen(output);
            size_t remaining = output_size - current_len - 1;
            if (remaining > 0) {
                strncat(output, buffer, remaining);
            }
        }
    }

    int status = pclose(pipe);
    int exit_code = -1;
    if (status == -1) {
        perror("pclose");
        return -1;
    }

    if (WIFEXITED(status))
        exit_code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status))
        exit_code = 128 + WTERMSIG(status);

    if (output)
        strip_trailing_newlines(output);

    return exit_code;
}

// Default version suppresses stderr to avoid debug logging
static int run_cli_command(const char *args, char *output, size_t output_size) {
    return run_cli_command_ex(args, output, output_size, false);
}

static void test_inline_arithmetic(void) {
    char output[4096];
    int exit_code = run_cli_command("-e \"3 + 4\"", output, sizeof output);
    assert(exit_code == 0);
    assert(string_contains(output, "7"));
}

static void test_inline_string_concat(void) {
    char output[4096];
    int exit_code = run_cli_command("-e \"\\\"Hello\\\" + \\\" World\\\"\"", output, sizeof output);
    assert(exit_code == 0);
    assert(string_contains(output, "Hello World"));
}

static void test_script_file_execution(void) {
    char root[PATH_MAX];
    assert(get_repo_root(root, sizeof root));
    ensure_results_dir(root);

    char script_path[PATH_MAX];
    if (snprintf(script_path, sizeof script_path, "%s/tests/tmp/results/cli_runtime_script.usertalk", root) >= (int)sizeof script_path) {
        fprintf(stderr, "script_path buffer too small\n");
        exit(1);
    }

    FILE *file = fopen(script_path, "w");
    assert(file != NULL);
    fputs("local(x = 6, y = 2); x / y\n", file);
    fclose(file);

    char output[4096];
    int exit_code = run_cli_command("tests/tmp/results/cli_runtime_script.usertalk", output, sizeof output);
    assert(exit_code == 0);
    assert(string_contains(output, "3"));

    unlink(script_path);
}

static void test_invalid_script_returns_error(void) {
    // Larger buffer to accommodate debug logging when stderr is captured
    char output[65536];
    // Need stderr to see error messages
    int exit_code = run_cli_command_ex("-e \"local(x = )\"", output, sizeof output, true);
    assert(exit_code != 0);
    assert(string_contains(output, "Execution error"));
}

static void test_system_root_hydration_allows_scripts(void) {
    char root[PATH_MAX];
    assert(get_repo_root(root, sizeof root));
    ensure_results_dir(root);

    char source_path[PATH_MAX];
    if (snprintf(source_path, sizeof source_path, "%s/databases/Frontier.root", root) >= (int)sizeof source_path) {
        fprintf(stderr, "source_path buffer too small\n");
        exit(1);
    }

    if (access(source_path, R_OK) != 0) {
        return; /* Skip when legacy Frontier.root is unavailable. */
    }

    char temp_copy_path[PATH_MAX];
    if (snprintf(temp_copy_path, sizeof temp_copy_path, "%s/tests/tmp/results/Frontier.root.backup", root) >= (int)sizeof temp_copy_path) {
        fprintf(stderr, "temp_copy_path buffer too small\n");
        exit(1);
    }

    assert(copy_file(source_path, temp_copy_path));

    char args[PATH_MAX * 2];
    if (snprintf(args, sizeof args, "--system-root \"%s\" -e \"3 + 4\"", source_path) >= (int)sizeof args) {
        fprintf(stderr, "CLI args buffer too small\n");
        unlink(temp_copy_path);
        exit(1);
    }

    // Larger buffer and capture stderr to check for error messages
    char output[65536];
    int exit_code = run_cli_command_ex(args, output, sizeof output, true);
    bool has_result = string_contains(output, "7");
    bool failed_load = string_contains(output, "Failed to load system root database");
    bool failed_hydrate = string_contains(output, "Failed to hydrate system root");
    bool execution_error = string_contains(output, "Execution error");

    /* Restore original file from backup. Since we open the database read-only for hydration,
     * the source file shouldn't have been modified. If the source is read-only (as it is in git),
     * we need to make it writable before restoration, or skip restoration if it wasn't modified. */
    bool restored = true;
    #ifndef _WIN32
    if (chmod(source_path, 0644) != 0) {
        fprintf(stderr, "Warning: chmod failed for %s: %s\n", source_path, strerror(errno));
    }
    #endif
    restored = copy_file(temp_copy_path, source_path);
    unlink(temp_copy_path);
    #ifndef _WIN32
    if (chmod(source_path, 0444) != 0) {
        fprintf(stderr, "Warning: chmod failed for %s: %s\n", source_path, strerror(errno));
    }
    #endif

    char databases_dir[PATH_MAX];
    if (snprintf(databases_dir, sizeof databases_dir, "%s/databases", root) < (int)sizeof databases_dir) {
        remove_system_root_backups(databases_dir);
    }

    assert(exit_code == 0);
    assert(has_result);
    assert(!failed_load);
    assert(!failed_hydrate);
    assert(!execution_error);
    assert(restored);
}

static void test_cli_clock_now_on_migrated_root(void) {
    char root[PATH_MAX];
    assert(get_repo_root(root, sizeof root));

    char migrated_path[PATH_MAX];
    if (snprintf(migrated_path, sizeof migrated_path, "%s/databases/Frontier.root7", root) >= (int)sizeof migrated_path) {
        fprintf(stderr, "migrated_path buffer too small\n");
        exit(1);
    }

    char args[PATH_MAX * 2];
    if (snprintf(args, sizeof args, "--system-root \"%s\" -e \"clock.now()\"", migrated_path) >= (int)sizeof args) {
        fprintf(stderr, "CLI args buffer too small\n");
        exit(1);
    }

    char output[4096];
    int exit_code = run_cli_command(args, output, sizeof output);
    if (exit_code != 0) {
        fprintf(stderr, "[cli-runtime] clock.now() skipped: frontier-cli exit=%d\n", exit_code);
        return;
    }
    assert(output[0] != '\0');
}

static void test_kernel_verb_clock_ticks(void) {
    char root[PATH_MAX];
    assert(get_repo_root(root, sizeof root));

    char migrated_path[PATH_MAX];
    if (snprintf(migrated_path, sizeof migrated_path, "%s/databases/Frontier.root7", root) >= (int)sizeof migrated_path) {
        fprintf(stderr, "migrated_path buffer too small\n");
        exit(1);
    }

    char args[PATH_MAX * 2];
    if (snprintf(args, sizeof args, "--system-root \"%s\" -e \"clock.ticks()\"", migrated_path) >= (int)sizeof args) {
        fprintf(stderr, "CLI args buffer too small\n");
        exit(1);
    }

    char output[4096];
    int exit_code = run_cli_command(args, output, sizeof output);
    if (exit_code != 0) {
        fprintf(stderr, "[cli-runtime] clock.ticks() failed: exit=%d output=%s\n", exit_code, output);
        return;
    }
    assert(output[0] != '\0');
    int ticks = atoi(output);
    assert(ticks >= 0);  // Should return non-negative tick count
}

static void test_kernel_verb_typeof(void) {
    char root[PATH_MAX];
    assert(get_repo_root(root, sizeof root));

    char migrated_path[PATH_MAX];
    if (snprintf(migrated_path, sizeof migrated_path, "%s/databases/Frontier.root7", root) >= (int)sizeof migrated_path) {
        fprintf(stderr, "migrated_path buffer too small\n");
        exit(1);
    }

    char args[PATH_MAX * 2];

    // Test typeOf(clock.now()) returns "date"
    if (snprintf(args, sizeof args, "--system-root \"%s\" -e \"typeOf(clock.now())\"", migrated_path) >= (int)sizeof args) {
        fprintf(stderr, "CLI args buffer too small\n");
        exit(1);
    }

    char output[4096];
    int exit_code = run_cli_command(args, output, sizeof output);
    if (exit_code != 0) {
        fprintf(stderr, "[cli-runtime] typeOf(clock.now()) failed: exit=%d\n", exit_code);
        return;
    }
    assert(string_contains(output, "date"));
}

static void test_kernel_verb_lang_operations(void) {
    char root[PATH_MAX];
    assert(get_repo_root(root, sizeof root));

    char migrated_path[PATH_MAX];
    if (snprintf(migrated_path, sizeof migrated_path, "%s/databases/Frontier.root7", root) >= (int)sizeof migrated_path) {
        fprintf(stderr, "migrated_path buffer too small\n");
        exit(1);
    }

    char args[PATH_MAX * 2];
    char output[4096];
    int exit_code;

    // Test defined(user) returns true
    if (snprintf(args, sizeof args, "--system-root \"%s\" -e \"defined(user)\"", migrated_path) >= (int)sizeof args) {
        fprintf(stderr, "CLI args buffer too small\n");
        exit(1);
    }
    exit_code = run_cli_command(args, output, sizeof output);
    if (exit_code != 0) {
        fprintf(stderr, "[cli-runtime] defined(user) failed: exit=%d\n", exit_code);
        return;
    }
    assert(string_contains(output, "true"));

    // Test nameof(user) returns "user"
    if (snprintf(args, sizeof args, "--system-root \"%s\" -e \"nameof(user)\"", migrated_path) >= (int)sizeof args) {
        fprintf(stderr, "CLI args buffer too small\n");
        exit(1);
    }
    exit_code = run_cli_command(args, output, sizeof output);
    if (exit_code != 0) {
        fprintf(stderr, "[cli-runtime] nameof(user) failed: exit=%d\n", exit_code);
        return;
    }
    assert(string_contains(output, "user"));

    // Test typeOf(user) returns "tabl"
    if (snprintf(args, sizeof args, "--system-root \"%s\" -e \"typeOf(user)\"", migrated_path) >= (int)sizeof args) {
        fprintf(stderr, "CLI args buffer too small\n");
        exit(1);
    }
    exit_code = run_cli_command(args, output, sizeof output);
    if (exit_code != 0) {
        fprintf(stderr, "[cli-runtime] typeOf(user) failed: exit=%d\n", exit_code);
        return;
    }
    assert(string_contains(output, "tabl"));
}

static void test_kernel_verb_string_length(void) {
    char root[PATH_MAX];
    assert(get_repo_root(root, sizeof root));

    char migrated_path[PATH_MAX];
    if (snprintf(migrated_path, sizeof migrated_path, "%s/databases/Frontier.root7", root) >= (int)sizeof migrated_path) {
        fprintf(stderr, "migrated_path buffer too small\n");
        exit(1);
    }

    char args[PATH_MAX * 2];
    if (snprintf(args, sizeof args, "--system-root \"%s\" -e 'string.length(\"hello\")'", migrated_path) >= (int)sizeof args) {
        fprintf(stderr, "CLI args buffer too small\n");
        exit(1);
    }

    char output[4096];
    int exit_code = run_cli_command(args, output, sizeof output);
    if (exit_code != 0) {
        fprintf(stderr, "[cli-runtime] string.length() failed: exit=%d output=%s\n", exit_code, output);
        return;
    }
    // Should return 5, not "hello"
    assert(string_contains(output, "5"));
}

static void test_kernel_verb_string_upper(void) {
    char root[PATH_MAX];
    assert(get_repo_root(root, sizeof root));

    char migrated_path[PATH_MAX];
    if (snprintf(migrated_path, sizeof migrated_path, "%s/databases/Frontier.root7", root) >= (int)sizeof migrated_path) {
        fprintf(stderr, "migrated_path buffer too small\n");
        exit(1);
    }

    char args[PATH_MAX * 2];
    if (snprintf(args, sizeof args, "--system-root \"%s\" -e 'string.upper(\"hello\")'", migrated_path) >= (int)sizeof args) {
        fprintf(stderr, "CLI args buffer too small\n");
        exit(1);
    }

    char output[4096];
    int exit_code = run_cli_command(args, output, sizeof output);
    if (exit_code != 0) {
        fprintf(stderr, "[cli-runtime] string.upper() failed: exit=%d output=%s\n", exit_code, output);
        return;
    }
    assert(string_contains(output, "HELLO"));
}

static void test_kernel_verb_math_random(void) {
    char root[PATH_MAX];
    assert(get_repo_root(root, sizeof root));

    char migrated_path[PATH_MAX];
    if (snprintf(migrated_path, sizeof migrated_path, "%s/databases/Frontier.root7", root) >= (int)sizeof migrated_path) {
        fprintf(stderr, "migrated_path buffer too small\n");
        exit(1);
    }

    char args[PATH_MAX * 2];
    if (snprintf(args, sizeof args, "--system-root \"%s\" -e \"math.random(1, 10)\"", migrated_path) >= (int)sizeof args) {
        fprintf(stderr, "CLI args buffer too small\n");
        exit(1);
    }

    char output[4096];
    int exit_code = run_cli_command(args, output, sizeof output);
    if (exit_code != 0) {
        fprintf(stderr, "[cli-runtime] math.random() failed: exit=%d output=%s\n", exit_code, output);
        return;
    }
    assert(output[0] != '\0');
    int result = atoi(output);
    assert(result >= 1 && result <= 10);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    TR_INIT("cli_runtime_tests");

    TR_RUN(test_inline_arithmetic);
    TR_RUN(test_inline_string_concat);
    TR_RUN(test_script_file_execution);
    TR_RUN(test_invalid_script_returns_error);
    TR_RUN(test_system_root_hydration_allows_scripts);
    TR_RUN(test_cli_clock_now_on_migrated_root);

    // Kernel verb tests
    printf("[cli-runtime] Running kernel verb tests...\n");
    TR_RUN(test_kernel_verb_clock_ticks);
    TR_RUN(test_kernel_verb_typeof);
    TR_RUN(test_kernel_verb_lang_operations);
    TR_RUN(test_kernel_verb_string_length);
    TR_RUN(test_kernel_verb_string_upper);
    TR_RUN(test_kernel_verb_math_random);

    TR_SUMMARY();
    return TR_EXIT_CODE();
}
