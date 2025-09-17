#include <assert.h>
#include <stdio.h>

#include "frontier.h"
#include "shell_api.h"

static void test_shell_api_headless_toggle(void) {
    shell_api_use_headless();
    assert(shell_api_is_headless());
}

int main(void) {
    test_shell_api_headless_toggle();
    printf("core_tests: headless smoke test executed\n");
    return 0;
}
