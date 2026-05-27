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
    Boolean is_headless; /* true when this implementation rejects UI access */
    Boolean (*require_capability)(tyshellcapability capability, const char *verbname);
} tyshellapi;

void shell_api_install(const tyshellapi *api);
void shell_api_use_default(void);
const tyshellapi *shell_api_current(void);

Boolean shell_api_require(tyshellcapability capability, const char *verbname);
const char *shell_api_capability_name(tyshellcapability capability);
Boolean shell_api_is_headless(void);

/* Convenience for tests/headless servers to install the strict implementation. */
void shell_api_use_headless(void);

/* Runtime flag accessors -- in-process source of truth for CLI runtime
 * flags that downstream modules need to consult without linking back to the
 * CLI parser. The CLI converges the env-var and CLI-flag inputs at parse
 * time, then calls the setter once; everything else reads via the getter.
 * See issue #649.
 *
 * Type note: `boolean` (lowercase) is intentional -- it matches the consumer
 * types (cli_options_t.lock_opened_roots, dbopenverb return). The capital
 * `Boolean` used in tyshellapi above is the Carbon-shim type for AppleEvent
 * interop; don't "unify" them. */
void shell_api_set_lock_opened_roots(boolean value);
boolean shell_api_lock_opened_roots(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* frontier_shell_api_h */
