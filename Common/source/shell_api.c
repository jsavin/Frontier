#include "frontier.h"
#include "standard.h"

#include "shell_api.h"

static Boolean shell_allow_capability(tyshellcapability capability, const char *verbname);

static const tyshellapi kShellApiDefault = {
    false,
    shell_allow_capability
};

static const tyshellapi *gShellApi = &kShellApiDefault;

static Boolean shell_allow_capability(tyshellcapability capability, const char *verbname) {
    #pragma unused(capability, verbname)
    return true;
}

void shell_api_install(const tyshellapi *api) {
    gShellApi = (api != NULL) ? api : &kShellApiDefault;
}

void shell_api_use_default(void) {
    gShellApi = &kShellApiDefault;
}

const tyshellapi *shell_api_current(void) {
    return gShellApi;
}

Boolean shell_api_require(tyshellcapability capability, const char *verbname) {
    const tyshellapi *api = gShellApi;

    if ((api == NULL) || (api->require_capability == NULL))
        return true;

    return (*api->require_capability)(capability, verbname);
}

const char *shell_api_capability_name(tyshellcapability capability) {
    switch (capability) {
        case kShellCapabilityWindows:
            return "windows";
        case kShellCapabilityMenus:
            return "menus";
        case kShellCapabilityDialogs:
            return "dialogs";
        case kShellCapabilityDebugger:
            return "debugger";
        case kShellCapabilityAlerts:
            return "alerts";
        case kShellCapabilityPrinting:
            return "printing";
        case kShellCapabilityEventLoop:
            return "event loop";
        case kShellCapabilityToolbar:
            return "toolbar";
    }

    return "unknown";
}

Boolean shell_api_is_headless(void) {
    const tyshellapi *api = gShellApi;

    return (api != NULL) ? api->is_headless : false;
}
