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

*(To be added as planning progresses)*
