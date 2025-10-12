# Phase 2 Architecture — Ports and Adapters

Status
- State: Draft
- Phase: 2
- Last Updated: 2025-09-29
- Notes: Proposes `UIServices` interface and build split.

Related Docs
- planning/ui_abstraction/phase2/analysis.md
- planning/ui_abstraction/phase2/migration_plan.md
- planning/ui_abstraction/phase2/UIServices_stub.md
- planning/ui_abstraction/phase2/migration_playbook.md
- planning/no_ui_linkage_policy.md
- docs/diagrams/placeholder.txt

Change Log
- 2025-09-29: Initialized template sections (Status/Related Docs/Change Log).

## Table of Contents
- [Module Split](#module-split)
- [Proposed `UIServices` Interface (C, function table)](#proposed-uiservices-interface-c-function-table)
- [Notes](#notes)
- [Boundary Rules](#boundary-rules)
- [Build and CI](#build-and-ci)

## Module Split
- `libfrontier_core` (pure C): evaluation engine, DB, threading, core verbs; depends only on standard C and the `UIServices` interface.
- `ui_adapters/`:
  - `headless_adapter`: no-op/logging implementation; used by CLI and tests.
  - `mac_appkit_adapter`: AppKit/Cocoa implementation; can use MVC or MVVM internally.
  - `win32_adapter`: Win32 implementation; MVP acceptable.
  - `web_adapter` (future): out-of-process via REST/WebSocket.
- Apps:
  - `frontier-cli`: links `libfrontier_core` + `headless_adapter` only.
  - Native UIs: link `libfrontier_core` + respective adapter.

## Proposed `UIServices` Interface (C, function table)

```c
typedef struct FrontierUIServices {
    /* Lifecycle / event loop */
    void (*yield)(void);                 /* allow UI to process events */
    void (*scheduleTimer)(int ms);       /* coarse timers for backgrounding */

    /* Messaging */
    void (*logInfo)(const char *msg);
    void (*logError)(const char *msg);
    int  (*alert)(const char *title, const char *message); /* returns button id */

    /* Dialogs */
    int  (*fileOpen)(char *outPath, int outLen);
    int  (*fileSave)(char *outPath, int outLen);

    /* Clipboard */
    int  (*clipboardSetText)(const char *utf8, int len);
    int  (*clipboardGetText)(char *outUtf8, int outLen);

    /* Menus */
    void (*menuEnable)(int menuId, int itemId, int enable);

    /* Automation/AppleEvents bridge (opaque) */
    int  (*automationSend)(const void *req, int reqLen, void *resp, int respLen);
} FrontierUIServices;

/* Core entry point to install UI services at startup */
void frontier_set_ui_services(const FrontierUIServices *svc);
```

## Notes
- Keep the surface minimal. Expand only when a core use case requires it.
- All functions must be safe to call in headless mode; the headless adapter can return failure codes or raise script errors via core mechanisms.

## Boundary Rules
- No platform UI headers/types in core. Use opaque IDs or primitives only.
- All UI conditionals live inside adapters. Core selects behavior through the interface, not `#if`.
- Adapters own threading/event-loop specifics. Core requests background/yield via `yield/scheduleTimer`.

## Build and CI
- Build `libfrontier_core` as a standalone library with `-Werror` and checks forbidding UI headers.
- Build adapters separately; link-time ensure `frontier-cli` does not pull AppKit/Carbon/Win32.
- Add a CI step that fails if UI symbols appear in the core binary.
