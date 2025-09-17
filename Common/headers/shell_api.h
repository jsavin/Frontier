/*
    shell_api.h -- Shared shell abstraction for Frontier UI integration

    Provides a capability-based interface so the runtime can query whether
    UI features (windows, menus, dialogs, etc.) are available. Headless
    builds install an implementation that raises ScriptError when code
    touches UI-only verbs, while desktop builds can wire in real handlers.
*/

#ifndef frontier_shell_api_h
#define frontier_shell_api_h

#include "frontier.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
    Enumerates broad classes of UI features that scripts may depend on.
    These values let headless builds fail fast when an unavailable feature
    is invoked, while remaining extensible for future shells.
*/
typedef enum tyshellcapability {
    kShellCapabilityWindows = 0,
    kShellCapabilityMenus,
    kShellCapabilityDialogs,
    kShellCapabilityDebugger,
    kShellCapabilityAlerts,
    kShellCapabilityPrinting,
    kShellCapabilityEventLoop,
    kShellCapabilityToolbar
} tyshellcapability;

typedef struct tyshellapi {
    boolean is_headless; /* true when this implementation rejects UI access */
    boolean (*require_capability)(tyshellcapability capability, const char *verbname);
} tyshellapi;

void shell_api_install(const tyshellapi *api);
void shell_api_use_default(void);
const tyshellapi *shell_api_current(void);

boolean shell_api_require(tyshellcapability capability, const char *verbname);
const char *shell_api_capability_name(tyshellcapability capability);
boolean shell_api_is_headless(void);

/* Convenience for tests/headless servers to install the strict implementation. */
void shell_api_use_headless(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* frontier_shell_api_h */
