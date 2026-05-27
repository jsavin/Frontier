#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "frontier.h"
#include "shell_api.h"
#include "test_report.h"

static void test_shell_api_capability_names(void) {
    shell_api_use_default();
    assert(strcmp(shell_api_capability_name(kShellCapabilityWindows), "windows") == 0);
    assert(strcmp(shell_api_capability_name(kShellCapabilityMenus), "menus") == 0);
}

static void test_shell_api_default_mode(void) {
    shell_api_use_default();
    assert(shell_api_is_headless() == 0);
    assert(shell_api_require(kShellCapabilityWindows, "window.open"));
}

static void test_shell_api_headless_mode(void) {
    shell_api_use_headless();
    assert(shell_api_is_headless());
    assert(!shell_api_require(kShellCapabilityWindows, "window.open"));
}

/* Issue #649: shell_api_lock_opened_roots is the in-process source of truth
 * for the --lock-opened-roots flag, replacing FRONTIER_LOCK_OPENED_ROOTS as a
 * cross-module signal. The four tests below cover default state, set->get
 * roundtrip in both directions, and idempotency. */

static void test_shell_api_lock_opened_roots_default_false(void) {
    /* The static initializer guarantees this is false; explicitly reset to
     * defend the test against ordering with siblings that flip it. */
    shell_api_set_lock_opened_roots(false);
    assert(shell_api_lock_opened_roots() == false);
}

static void test_shell_api_lock_opened_roots_set_true_get_true(void) {
    shell_api_set_lock_opened_roots(false);
    shell_api_set_lock_opened_roots(true);
    assert(shell_api_lock_opened_roots() == true);
}

static void test_shell_api_lock_opened_roots_set_false_get_false(void) {
    shell_api_set_lock_opened_roots(true);
    shell_api_set_lock_opened_roots(false);
    assert(shell_api_lock_opened_roots() == false);
}

static void test_shell_api_lock_opened_roots_set_idempotent(void) {
    shell_api_set_lock_opened_roots(true);
    shell_api_set_lock_opened_roots(true);
    assert(shell_api_lock_opened_roots() == true);
    shell_api_set_lock_opened_roots(false);
    shell_api_set_lock_opened_roots(false);
    assert(shell_api_lock_opened_roots() == false);
}

int main(void) {
    TR_INIT("core_tests");
    TR_RUN(test_shell_api_capability_names);
    TR_RUN(test_shell_api_default_mode);
    TR_RUN(test_shell_api_headless_mode);
    TR_RUN(test_shell_api_lock_opened_roots_default_false);
    TR_RUN(test_shell_api_lock_opened_roots_set_true_get_true);
    TR_RUN(test_shell_api_lock_opened_roots_set_false_get_false);
    TR_RUN(test_shell_api_lock_opened_roots_set_idempotent);
    TR_SUMMARY();
    return TR_EXIT_CODE();
}
