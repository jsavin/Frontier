#!/bin/bash
# Tests for the C-indent check in tools/hooks/pre-commit-integration-tests.
#
# Each test creates a fresh scratch git repo in a temp dir, stages a fixture
# C/H file, runs the hook, and asserts the exit code (and for failures, that
# expected source line numbers appear in stderr).
#
# Usage: ./test_c_indent_hook.sh
# Exit:  0 if all tests pass, 1 otherwise.
#
# Requires: bash, git, awk. No project-specific tooling — the hook is invoked
# in isolation from a temp repo, so OPML/doc/verb branches in the hook are
# inert (their file-type guards do not match our *.c/*.h fixtures).

# Intentionally no `set -e` — hook invocations are expected to exit non-zero
# on FAIL fixtures and that is captured in HOOK_EXIT. `set -e` would abort
# the suite on the first failing hook call. `set -u` is fine and catches
# typo bugs in the harness without affecting hook exit handling.
set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HOOK="$SCRIPT_DIR/../pre-commit-integration-tests"

if [ ! -f "$HOOK" ]; then
    echo "ERROR: hook not found at $HOOK" >&2
    exit 1
fi

PASS_COUNT=0
FAIL_COUNT=0
FAILED_TESTS=()
REPO=""
STDERR_FILE=""
# Sentinel — surfaces an obvious failure mode if expect_pass/expect_fail is
# ever called without a prior run_hook (set -u would otherwise abort with a
# less-helpful "unbound variable" error).
HOOK_EXIT=-1

# ──────────────────────────────────────────────────────────────
# Cleanup on any exit (success, failure, or Ctrl-C) so we don't
# leak scratch repos or stderr capture files into $TMPDIR.
# ──────────────────────────────────────────────────────────────
on_exit() {
    if [ -n "$REPO" ] && [ -d "$REPO" ]; then
        rm -rf "$REPO"
    fi
    if [ -n "$STDERR_FILE" ] && [ -f "$STDERR_FILE" ]; then
        rm -f "$STDERR_FILE"
    fi
}
trap on_exit EXIT INT TERM

# ──────────────────────────────────────────────────────────────
# Harness
# ──────────────────────────────────────────────────────────────

# make_repo — create a fresh scratch git repo, cd into it, install the hook.
# Sets REPO global to the path so cleanup_repo / on_exit can remove it.
make_repo() {
    # Guard mktemp explicitly: if it fails (e.g. low disk), REPO would be
    # empty, "cd ''" would silently land in $HOME, and the subsequent
    # `git init` would corrupt the user's home directory.
    REPO=$(mktemp -d -t frontier-hook-test.XXXXXX) \
        || { echo "ERROR: mktemp -d failed" >&2; exit 1; }
    cd "$REPO" || { echo "ERROR: cannot cd to $REPO" >&2; exit 1; }
    git init -q -b main
    git config user.email "test@example.com"
    git config user.name "Hook Test"
    # Initial commit so we have a HEAD to diff against for "modify" cases.
    echo "# scratch" > README.md
    git add README.md
    git commit -q -m "init"
    # Install the hook by copying (not symlinking — the hook locates GIT_ROOT
    # via `git rev-parse --show-toplevel` from $PWD, so a copy in the scratch
    # repo's hooks dir works correctly).
    cp "$HOOK" .git/hooks/pre-commit
    chmod +x .git/hooks/pre-commit
}

# cleanup_repo — remove the current scratch repo and its stderr capture file
# between tests. on_exit handles the same cleanup if the script aborts.
#
# Restores cwd to SCRIPT_DIR before removing the scratch repo so that any
# code running between cleanup_repo and the next make_repo (e.g. a future
# helper that does fs work) does not run with $PWD pointing at a deleted
# directory.
cleanup_repo() {
    cd "$SCRIPT_DIR" || true
    if [ -n "$REPO" ] && [ -d "$REPO" ]; then
        rm -rf "$REPO"
        REPO=""
    fi
    if [ -n "$STDERR_FILE" ] && [ -f "$STDERR_FILE" ]; then
        rm -f "$STDERR_FILE"
        STDERR_FILE=""
    fi
}

# run_hook — invoke the installed hook directly (not via `git commit`, to
# avoid coupling test outcomes to commit-time messages or editors). Sets
# HOOK_EXIT to the hook exit code; captures stderr to $STDERR_FILE.
#
# Cleans up any prior STDERR_FILE before allocating a new one so that a
# future fixture which calls run_hook twice does not leak the first temp
# file until on_exit.
run_hook() {
    if [ -n "$STDERR_FILE" ] && [ -f "$STDERR_FILE" ]; then
        rm -f "$STDERR_FILE"
    fi
    STDERR_FILE=$(mktemp) \
        || { echo "ERROR: mktemp for STDERR_FILE failed" >&2; exit 1; }
    .git/hooks/pre-commit 2>"$STDERR_FILE"
    HOOK_EXIT=$?
}

# expect_pass NAME — assert the hook just exited 0.
expect_pass() {
    local name="$1"
    if [ "$HOOK_EXIT" -eq 0 ]; then
        PASS_COUNT=$((PASS_COUNT + 1))
        echo "  PASS: $name"
    else
        FAIL_COUNT=$((FAIL_COUNT + 1))
        FAILED_TESTS+=("$name")
        echo "  FAIL: $name (expected exit 0, got $HOOK_EXIT)"
        echo "    stderr:"
        sed 's/^/      /' "$STDERR_FILE"
    fi
}

# expect_fail NAME [LINE...] — assert the hook just exited non-zero AND that
# each LINE number appears in stderr formatted as "N:" (the hook's violation
# output format).
expect_fail() {
    local name="$1"
    shift
    expect_fail_with "$name" "" "$@"
}

# expect_fail_with NAME EXPECTED_PATH [LINE...] — like expect_fail but ALSO
# asserts that EXPECTED_PATH appears literally in stderr. Pass "" to skip
# the path assertion. Used by fixtures that want to verify the hook reports
# the correct filename (e.g. paths containing spaces).
expect_fail_with() {
    local name="$1"
    local expected_path="$2"
    shift 2
    local expected_lines=("$@")

    if [ "$HOOK_EXIT" -eq 0 ]; then
        FAIL_COUNT=$((FAIL_COUNT + 1))
        FAILED_TESTS+=("$name")
        echo "  FAIL: $name (expected non-zero exit, got 0)"
        return
    fi

    if [ -n "$expected_path" ] && ! grep -qF "$expected_path" "$STDERR_FILE"; then
        FAIL_COUNT=$((FAIL_COUNT + 1))
        FAILED_TESTS+=("$name")
        echo "  FAIL: $name (path '$expected_path' not in stderr)"
        echo "    stderr:"
        sed 's/^/      /' "$STDERR_FILE"
        return
    fi

    local missing=()
    for ln in "${expected_lines[@]}"; do
        if ! grep -qE "^[[:space:]]+${ln}:" "$STDERR_FILE"; then
            missing+=("$ln")
        fi
    done

    if [ "${#missing[@]}" -eq 0 ]; then
        PASS_COUNT=$((PASS_COUNT + 1))
        echo "  PASS: $name"
    else
        FAIL_COUNT=$((FAIL_COUNT + 1))
        FAILED_TESTS+=("$name")
        echo "  FAIL: $name (missing expected line numbers: ${missing[*]})"
        echo "    stderr:"
        sed 's/^/      /' "$STDERR_FILE"
    fi
}

# ──────────────────────────────────────────────────────────────
# Test 1: Pure-tab indent (new file) → PASS
# ──────────────────────────────────────────────────────────────
test_pure_tab_indent_passes() {
    echo "Test: pure_tab_indent_passes"
    make_repo
    printf 'int main(void) {\n\treturn 0;\n}\n' > ok.c
    git add ok.c
    run_hook
    expect_pass "pure_tab_indent_passes"
    cleanup_repo
}

# ──────────────────────────────────────────────────────────────
# Test 2: Pure-space indent (new file) → FAIL on lines 2,3
# ──────────────────────────────────────────────────────────────
test_pure_space_indent_fails() {
    echo "Test: pure_space_indent_fails"
    make_repo
    printf 'int main(void) {\n    int x = 0;\n    return x;\n}\n' > bad.c
    git add bad.c
    run_hook
    expect_fail "pure_space_indent_fails" 2 3
    cleanup_repo
}

# ──────────────────────────────────────────────────────────────
# Test 3: Mixed tab+space indent (\t followed by spaces) → FAIL
# Catches the round-7 regression: pure-tab-first lines previously slipped
# through because the check only looked at the first indent character.
# ──────────────────────────────────────────────────────────────
test_mixed_tab_space_indent_fails() {
    echo "Test: mixed_tab_space_indent_fails"
    make_repo
    printf 'int main(void) {\n\t    int z = 3;\n\treturn z;\n}\n' > mixed.c
    git add mixed.c
    run_hook
    expect_fail "mixed_tab_space_indent_fails" 2
    cleanup_repo
}

# ──────────────────────────────────────────────────────────────
# Test 4: Tab-indented block-comment continuation → PASS
# Exercises the round-2/4 carve-out. The leading-whitespace run of a
# "\t * continuation" line is "\t " (tab + space-before-*) per the
# greedy [[:blank:]]* match in the hook's awk regex, so the line WOULD
# fail the indent check without the carve-out for "* "/"*/"/"/*"/"*"
# patterns. Verified by patching the hook to disable the carve-out:
# this fixture flips to FAIL, confirming it exercises the carve-out.
# Test 15 covers the same path with a more visually obvious
# space-then-* continuation.
# ──────────────────────────────────────────────────────────────
test_tab_indented_block_comment_passes() {
    echo "Test: tab_indented_block_comment_passes"
    make_repo
    printf 'int main(void) {\n\t/* opening line\n\t * continuation with content\n\t *\n\t */\n\treturn 0;\n}\n' > comments.c
    git add comments.c
    run_hook
    expect_pass "tab_indented_block_comment_passes"
    cleanup_repo
}

# ──────────────────────────────────────────────────────────────
# Test 5: Space-indented pointer deref → FAIL
# Round-4 regression guard. Previously the broad "starts with *" carve-out
# falsely excluded "    *ptr = x;".
# ──────────────────────────────────────────────────────────────
test_space_indented_pointer_deref_fails() {
    echo "Test: space_indented_pointer_deref_fails"
    make_repo
    printf 'void f(int *ptr) {\n    *ptr = 42;\n}\n' > ptr.c
    git add ptr.c
    run_hook
    expect_fail "space_indented_pointer_deref_fails" 2
    cleanup_repo
}

# ──────────────────────────────────────────────────────────────
# Test 6: Rename + modify with space-indent addition → FAIL
# Round-7: --diff-filter=ACMR (added R) so renames-with-changes are scanned.
# ──────────────────────────────────────────────────────────────
test_rename_with_space_indent_fails() {
    echo "Test: rename_with_space_indent_fails"
    make_repo
    # Commit an initial tab-indented file so the rename has a source.
    printf 'int main(void) {\n\treturn 0;\n}\n' > old.c
    git add old.c
    git commit -q -m "add old.c"
    # Rename and add a space-indented line in the same staged change.
    git mv old.c new.c
    printf 'int main(void) {\n\treturn 0;\n    int extra = 1;\n}\n' > new.c
    git add new.c
    run_hook
    # Line 3 is the inserted space-indented "    int extra = 1;"
    expect_fail "rename_with_space_indent_fails" 3
    cleanup_repo
}

# ──────────────────────────────────────────────────────────────
# Test 7: Delete-only changes → PASS
# Removing lines from a file produces "-" diff lines only (no "+"), so
# the hook has nothing to flag even when the underlying file is space
# indented. The hook's awk block intentionally ignores deletion lines.
# ──────────────────────────────────────────────────────────────
test_delete_only_changes_pass() {
    echo "Test: delete_only_changes_pass"
    make_repo
    # Seed a tab-indented file with an extra tab-indented block we will
    # delete. We use tab indent (not space) so the file as a whole would
    # pass the hook normally — we are isolating the delete-only behavior,
    # not testing whether existing space-indent triggers on delete.
    printf 'int main(void) {\n\tint a = 1;\n\tint to_delete_1 = 10;\n\tint to_delete_2 = 20;\n\treturn a;\n}\n' > legacy.c
    # core.hooksPath=/dev/null disables the hook for this seed step (we are
    # creating the baseline, not testing it). --no-verify is omitted because
    # the hooks-path override already covers all hook types.
    git -c core.hooksPath=/dev/null add legacy.c
    git -c core.hooksPath=/dev/null commit -q -m "seed"
    # Remove the two contiguous lines. The minimal -U0 diff for a pure
    # contiguous deletion is "-" lines only.
    printf 'int main(void) {\n\tint a = 1;\n\treturn a;\n}\n' > legacy.c
    git add legacy.c
    # Sanity-check the diff really is delete-only (no "+" content lines)
    # before running the hook — guards against future git versions emitting
    # the diff differently.
    if git diff --cached -U0 -- legacy.c | grep -E '^\+[^+]' >/dev/null; then
        echo "  SKIP: delete_only_changes_pass (diff format produced + lines; not a pure delete)"
        cleanup_repo
        return
    fi
    run_hook
    expect_pass "delete_only_changes_pass"
    cleanup_repo
}

# ──────────────────────────────────────────────────────────────
# Test 8: New file with tab indent (-0,0 +1,N hunk header) → PASS
# Identical setup to test 1 but kept as a separate fixture to make the
# new-file hunk-header path explicit (issue #548 calls this out).
# ──────────────────────────────────────────────────────────────
test_new_file_tab_indent_passes() {
    echo "Test: new_file_tab_indent_passes"
    make_repo
    printf 'void g(void) {\n\tint a = 1;\n\tint b = 2;\n\t(void)a;\n\t(void)b;\n}\n' > newfile.h
    git add newfile.h
    run_hook
    expect_pass "new_file_tab_indent_passes"
    cleanup_repo
}

# ──────────────────────────────────────────────────────────────
# Test 9: New file with space indent → FAIL with line numbers from 1
# Verifies that the awk lineno reconstruction starts at the correct value
# from the +1,N hunk header.
# ──────────────────────────────────────────────────────────────
test_new_file_space_indent_fails() {
    echo "Test: new_file_space_indent_fails"
    make_repo
    # 5-line file, all body lines space-indented.
    printf 'void g(void) {\n    int a = 1;\n    int b = 2;\n    (void)a;\n}\n' > newfile.c
    git add newfile.c
    run_hook
    expect_fail "new_file_space_indent_fails" 2 3 4
    cleanup_repo
}

# ──────────────────────────────────────────────────────────────
# Test 10: Mid-file insertion into an existing tab-indented file → FAIL with
# the SOURCE line number of the inserted line, not the diff-output line.
# This is the bug fix from PR #544 round 1 (replaced grep -n with awk).
# ──────────────────────────────────────────────────────────────
test_midfile_insertion_reports_source_lineno() {
    echo "Test: midfile_insertion_reports_source_lineno"
    make_repo
    # Seed a tab-indented file with the hook disabled. The seed would pass
    # the hook anyway, but disabling it keeps the test self-contained even
    # if the hook gains future seed-incompatible checks.
    printf 'int main(void) {\n\tint a = 1;\n\tint b = 2;\n\tint c = 3;\n\tint d = 4;\n\tint e = 5;\n\treturn a + b + c + d + e;\n}\n' > insert.c
    git -c core.hooksPath=/dev/null add insert.c
    git -c core.hooksPath=/dev/null commit -q -m "seed"
    # Insert a space-indented line between line 4 (int c) and line 5 (int d).
    # In source line numbering the new line becomes line 5.
    printf 'int main(void) {\n\tint a = 1;\n\tint b = 2;\n\tint c = 3;\n    int extra = 99;\n\tint d = 4;\n\tint e = 5;\n\treturn a + b + c + d + e;\n}\n' > insert.c
    git add insert.c
    run_hook
    expect_fail "midfile_insertion_reports_source_lineno" 5
    cleanup_repo
}

# ──────────────────────────────────────────────────────────────
# Test 11: File path containing spaces, tab indent → PASS
# Round-3 fix: while-read loop over $C_FILES_CHANGED instead of for-in,
# so paths with spaces iterate as single entries.
# ──────────────────────────────────────────────────────────────
test_path_with_spaces_tab_indent_passes() {
    echo "Test: path_with_spaces_tab_indent_passes"
    make_repo
    mkdir -p "my dir"
    printf 'int main(void) {\n\treturn 0;\n}\n' > "my dir/ok.c"
    git add "my dir/ok.c"
    run_hook
    expect_pass "path_with_spaces_tab_indent_passes"
    cleanup_repo
}

# ──────────────────────────────────────────────────────────────
# Test 12: File path containing spaces, space indent → FAIL with the
# correct path reported in the violation block.
# ──────────────────────────────────────────────────────────────
test_path_with_spaces_space_indent_fails() {
    echo "Test: path_with_spaces_space_indent_fails"
    make_repo
    mkdir -p "my dir"
    printf 'int main(void) {\n    return 0;\n}\n' > "my dir/bad.c"
    git add "my dir/bad.c"
    run_hook
    expect_fail_with "path_with_spaces_space_indent_fails" "my dir/bad.c" 2
    cleanup_repo
}

# ──────────────────────────────────────────────────────────────
# Test 13: Empty staged C-file change set (no .c/.h staged) → PASS trivially
# Sanity check that the C-indent block is fully gated by C_FILES_CHANGED.
# ──────────────────────────────────────────────────────────────
test_no_c_files_staged_passes() {
    echo "Test: no_c_files_staged_passes"
    make_repo
    # Stage a non-C file. The C-indent branch should not run at all.
    printf 'just text\n' > note.txt
    git add note.txt
    run_hook
    expect_pass "no_c_files_staged_passes"
    cleanup_repo
}

# ──────────────────────────────────────────────────────────────
# Test 14: Header file (.h) with space indent → FAIL
# Test 8 covered the .h-passes case; this confirms the `*.c|*.h` filter
# is symmetric and that `.h` violations are flagged the same way as `.c`.
# ──────────────────────────────────────────────────────────────
test_header_file_space_indent_fails() {
    echo "Test: header_file_space_indent_fails"
    make_repo
    printf 'struct s {\n    int x;\n    int y;\n};\n' > types.h
    git add types.h
    run_hook
    expect_fail "header_file_space_indent_fails" 2 3
    cleanup_repo
}

# ──────────────────────────────────────────────────────────────
# Test 15: Space-containing indent on block-comment continuation → PASS
# Genuinely exercises the round-2/4 comment carve-out: lines whose leading
# whitespace contains spaces AND whose first non-blank char is "*" or "/"
# (in /*, */, "* text", or bare "*") are excluded from the indent check.
#
# Without the carve-out, lines 3, 4, and 5 below would be flagged for
# having spaces in their leading-whitespace run. The carve-out lets
# them pass. The function body itself is tab-indented so the test isolates
# the carve-out behavior.
# ──────────────────────────────────────────────────────────────
test_space_block_comment_continuation_passes() {
    echo "Test: space_block_comment_continuation_passes"
    make_repo
    # Mix tab indent (for the function body) with a space-indented block
    # comment whose continuation lines start with " * ..." or " */". The
    # space-then-* pattern is what the carve-out targets.
    printf 'int main(void) {\n\t/* opener\n  * continuation with content\n  *\n  */\n\treturn 0;\n}\n' > carveout.c
    git add carveout.c
    run_hook
    expect_pass "space_block_comment_continuation_passes"
    cleanup_repo
}

# ──────────────────────────────────────────────────────────────
# Run all tests
# ──────────────────────────────────────────────────────────────

test_pure_tab_indent_passes
test_pure_space_indent_fails
test_mixed_tab_space_indent_fails
test_tab_indented_block_comment_passes
test_space_indented_pointer_deref_fails
test_rename_with_space_indent_fails
test_delete_only_changes_pass
test_new_file_tab_indent_passes
test_new_file_space_indent_fails
test_midfile_insertion_reports_source_lineno
test_path_with_spaces_tab_indent_passes
test_path_with_spaces_space_indent_fails
test_no_c_files_staged_passes
test_header_file_space_indent_fails
test_space_block_comment_continuation_passes

echo ""
echo "─────────────────────────────────────────"
echo "Results: $PASS_COUNT passed, $FAIL_COUNT failed"
if [ "$FAIL_COUNT" -gt 0 ]; then
    echo "Failed tests:"
    for t in "${FAILED_TESTS[@]}"; do
        echo "  - $t"
    done
    exit 1
fi
exit 0
