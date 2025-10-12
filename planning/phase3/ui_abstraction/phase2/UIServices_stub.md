# UIServices — Proposed Interface (Documentation Stub)

Status
- State: Draft
- Phase: 2
- Last Updated: 2025-09-29
- Notes: Reference design only; not a source header.

Related Docs
- planning/ui_abstraction/phase2/architecture.md

Change Log
- 2025-09-29: Initialized template sections (Status/Related Docs/Change Log).

Proposed C Interface
```c
typedef struct FrontierUIServices {
    /* Yield/Timers */
    void (*yield)(void);
    void (*scheduleTimer)(int ms);

    /* Logging/Alerts */
    void (*logInfo)(const char *msg);
    void (*logError)(const char *msg);
    int  (*alert)(const char *title, const char *message);

    /* File dialogs */
    int  (*fileOpen)(char *outPath, int outLen);
    int  (*fileSave)(char *outPath, int outLen);

    /* Clipboard */
    int  (*clipboardSetText)(const char *utf8, int len);
    int  (*clipboardGetText)(char *outUtf8, int outLen);

    /* Menus */
    void (*menuEnable)(int menuId, int itemId, int enable);

    /* Automation bridge (opaque) */
    int  (*automationSend)(const void *req, int reqLen, void *resp, int respLen);
} FrontierUIServices;

/* Install services at startup. Core calls this before using UI services. */
void frontier_set_ui_services(const FrontierUIServices *svc);
```

Guidance
- Keep surface minimal; expand only for concrete core use cases.
- Headless adapter must return predictable results for all functions.

References
- planning/ui_abstraction/phase2/architecture.md
