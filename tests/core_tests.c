#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "frontier.h"
#include "shell_api.h"

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

int main(void) {
    test_shell_api_capability_names();
    test_shell_api_default_mode();
    test_shell_api_headless_mode();
    printf("core_tests: shell_api smoke tests executed\n");
    return 0;
}
