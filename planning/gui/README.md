# GUI Planning

## Purpose

This directory contains planning documents for building GUI applications that communicate with `frontier-cli` — the headless core runtime — to provide user-facing capabilities similar to the legacy all-in-one Frontier application.

## Context

The Frontier architecture separates the runtime engine (`frontier-cli`) from presentation concerns. This enables:

- **Headless operation** — scripting, automation, and server deployments without GUI overhead
- **Multiple frontends** — native apps, web interfaces, or hybrid approaches can all talk to the same runtime
- **Platform flexibility** — GUI can be built with platform-native toolkits while the core remains portable C

## Scope

Planning in this directory covers:

- **Architecture decisions** — How GUI clients communicate with frontier-cli (IPC, sockets, etc.)
- **Feature mapping** — Which legacy Frontier UI capabilities to implement and in what order
- **Technology selection** — Framework/toolkit choices for each target platform
- **Prototyping plans** — Incremental steps toward a functional GUI

## Related Documentation

- `planning/INDEX.md` — Navigation for all planning workstreams
- `docs/CLI_USAGE_GUIDE.md` — Current frontier-cli capabilities
- Legacy Frontier source: `/Users/jake/dev/tedchoward/Frontier`

## Documents

| Document | Description |
|----------|-------------|
| [`ARCHITECTURE.md`](./ARCHITECTURE.md) | Overall GUI architecture, multi-user model, authentication, federation |
| [`PROTOCOL.md`](./PROTOCOL.md) | JSON protocol specification for client-server communication |
| [`TABLE_BROWSER.md`](./TABLE_BROWSER.md) | Table browser / ODB navigator specification |
| [`SCRIPT_EDITOR.md`](./SCRIPT_EDITOR.md) | Outline-based script editor with debugging |
| [`OUTLINE_EDITOR.md`](./OUTLINE_EDITOR.md) | Outline editor with hoisting, attributes, render modes |
| [`MENU_EDITOR.md`](./MENU_EDITOR.md) | Menu bar and popup menu editor |
| [`WPTEXT_EDITOR.md`](./WPTEXT_EDITOR.md) | Rich text (RTF) editor |
| [`CONSOLE.md`](./CONSOLE.md) | Unified REPL and QuickScript console |

## External Types and Editors

| Type | Editor | Notes |
|------|--------|-------|
| `table` | Table Browser | Hierarchical ODB navigation and editing |
| `script` | Script Editor | Outline-based with debugging support |
| `outline` | Outline Editor | General-purpose hierarchical editor |
| `menubar` | Menu Editor | Menu structure with script attachment |
| `wptext` | wptext Editor | Rich text using platform RTF components |
| `filespec` | Table Browser | Edited inline as string values |
| `picture` | *(none)* | No native editor in legacy or planned |
| `binary` | *(none)* | No native editor in legacy or planned |
| `alias` | *(deprecated)* | Not supported in v7 |
| `objspec` | *(deprecated)* | Legacy Apple Events type, not supported |
