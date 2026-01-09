#include "frontier.h"
#include "standard.h"

#include <stdio.h>

#ifndef FRONTIER_HEADLESS
#include "langinternal.h"
#include "strings.h"
#endif
#include "shell_api.h"
#include "logging.h"

static boolean shell_headless_require(tyshellcapability capability, const char *verbname);

static const tyshellapi kShellApiHeadless = {
    true,
    shell_headless_require
};

static boolean shell_headless_require(tyshellcapability capability, const char *verbname) {
    const char *capability_name = shell_api_capability_name(capability);
    char message[256];

    if ((verbname == NULL) || (*verbname == '\0'))
        snprintf(message, sizeof (message),
            "UI capability \"%s\" is unavailable in headless mode.",
            capability_name);
    else
        snprintf(message, sizeof (message),
            "UI verb \"%s\" requires %s support, which is unavailable in headless mode.",
            verbname,
            capability_name);

#ifndef FRONTIER_HEADLESS
    {
        bigstring bserror;
        copyctopstring(message, bserror);
        langerrormessage(bserror);
    }
#else
    log_error(LOG_COMP_GENERAL, "%s", message);
#endif

    return false;
}

void shell_api_use_headless(void) {
    shell_api_install(&kShellApiHeadless);
}

/*
 * ADR-006 Phase 2: Shell globals stubs for headless mode
 *
 * In GUI mode, shellpushdefaultglobals() and shellpopglobals() manage a stack
 * of shell configuration contexts (config.filecreator, config.filetype, etc.).
 * These are used by dbverbs.c and other components to temporarily switch configs.
 *
 * In headless mode, shell globals don't exist since shell.c/shellcallbacks.c
 * aren't compiled. These no-op stubs allow code that calls these functions to
 * compile and run without error in headless builds.
 *
 * NOTE: This is a temporary solution for ADR-006 implementation. The long-term
 * fix would be to migrate shell config to thread-local storage similar to
 * outline context, but that requires more investigation since shell code isn't
 * currently compiled in headless mode.
 */
boolean shellpushdefaultglobals(void) {
    /* No-op in headless mode - shell config doesn't exist */
    return true;
}
