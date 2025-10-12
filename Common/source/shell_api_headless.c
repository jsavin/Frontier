#include "frontier.h"
#include "standard.h"

#include <stdio.h>

#ifndef FRONTIER_HEADLESS
#include "langinternal.h"
#include "strings.h"
#endif
#include "shell_api.h"

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
    fprintf(stderr, "%s\n", message);
#endif

    return false;
}

void shell_api_use_headless(void) {
    shell_api_install(&kShellApiHeadless);
}
