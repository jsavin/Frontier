# Phase 2 Analysis — Beyond Stubs and `#if`

Status
- State: Draft
- Phase: 2
- Last Updated: 2025-09-29
- Notes: Documents coupling, risks, and boundary strategy.

Related Docs
- planning/ui_abstraction/phase2/architecture.md
- planning/ui_abstraction/phase2/migration_plan.md
- planning/ui_abstraction/phase2/patterns_and_choices.md
- planning/ui_abstraction/phase2/UIServices_stub.md
- planning/ui_abstraction/PHASES.md
- planning/ui_abstraction/ui_abstraction_overview.md

Change Log
- 2025-09-29: Initialized template sections (Status/Related Docs/Change Log).

## Table of Contents
- [Summary](#summary)
- [Evidence of Tight Coupling in Tree](#evidence-of-tight-coupling-in-tree)
- [Why Stubs and Gating Won’t Scale](#why-stubs-and-gating-wont-scale)
- [Target Architecture (Ports and Adapters)](#target-architecture-ports-and-adapters)
- [Enforcement Tactics](#enforcement-tactics)
- [Migration Strategy (High Level)](#migration-strategy-high-level)
- [Patterns: MVC vs. MVVM vs. Hexagonal](#patterns-mvc-vs-mvvm-vs-hexagonal)
- [Pilot Scope (Suggested)](#pilot-scope-suggested)
- [Success Criteria](#success-criteria)

## Summary
- Stubbing and conditional compilation are effective to unblock headless builds, but they do not scale. The long-term direction should separate the runtime from UI frameworks through explicit interfaces (ports/adapters). UI-specific patterns (MVC/MVVM) live inside adapters; the core remains UI-agnostic.

## Evidence of Tight Coupling in Tree
- Headless gating scattered across core: e.g., `Common/source/ops.c`, `Common/source/opedit.c`, `Common/source/langsystem7.c`, `Common/headers/file.h`.
- Headless stubs with a large AppleEvent and Toolbox surface: `Common/headers/headless_stubs.h`.
- UI types in shared headers expose UI details to core: `Common/headers/op.h` declares `WindowPtr outlinewindow`; `Common/headers/cancooninternal.h` exposes `WindowPtr` globals.
- Legacy umbrella headers remain: `Common/headers/xMacHeaders.c` pulls classic Mac OS UI headers.
- CLI build still links AppKit/Carbon, indicating dependency leakage: `frontier-cli/Makefile` uses `-framework AppKit -framework Carbon`.

## Why Stubs and Gating Won’t Scale
- Surface area growth: Each new UI touchpoint demands stubs and new `#if` branches; maintenance burden increases and behavior diverges.
- Header coupling: UI symbols in headers force core code to be aware of UI or proliferate guards.
- Duplicate logic: Headless branches reimplement behavior (e.g., numeric formatting in `ops.c`), inviting drift.
- Build friction: Even headless targets may need to link UI frameworks, complicating CI and portability.

## Target Architecture (Ports and Adapters)
- Define a narrow `UIServices` interface for runtime-to-UI requests (alerts, menus, dialogs, clipboard, event loop, text rendering hooks, automation/AppleEvents bridging).
- Core depends only on `UIServices` (provided at startup via a function table/context). No platform headers in core.
- Implement adapters:
  - Headless: no-op/logging adapter used by CLI, tests, and servers.
  - macOS: AppKit/Cocoa adapter (possibly with MVC or MVVM internally).
  - Windows: Win32 adapter (MVP internally acceptable).
  - Future web: out-of-process adapter via IPC/REST/WebSocket.

## Enforcement Tactics
- Header hygiene: Provide a minimal `core_includes.h`; forbid inclusion of platform UI headers when building `libfrontier_core` (e.g., compile-time guards, CI checks).
- Link hygiene: Build `libfrontier_core` with no UI frameworks; fail CI if UI symbols appear in the core link graph.
- Dependency boundaries: Move any `WindowPtr`/`HWND`/AppKit types behind opaque IDs or adapter-only code.

## Migration Strategy (High Level)
1. Inventory UI entry points used by core hot paths.
2. Introduce `UIServices` with just enough functions to cover those paths.
3. Implement headless adapter and switch gated core call sites to use the interface.
4. Remove UI framework links from CLI/headless builds.
5. Expand interface to cover remaining UI verbs and editor flows.

## Patterns: MVC vs. MVVM vs. Hexagonal
- MVC/MVVM are UI-internal choices. They don’t, by themselves, decouple the core. Use hexagonal/ports-and-adapters for the boundary; let each adapter choose MVC (AppKit), MVVM (SwiftUI), or MVP (Win32) as appropriate.

## Pilot Scope (Suggested)
- `UIServices` v0: logging/status, timers/backgrounding, basic alerts/prompts, clipboard, file open/save dialogs, menu enable/disable.
- Swap a handful of `#if FRONTIER_HEADLESS` sites to `UIServices` calls.
- Remove `AppKit/Carbon` from `frontier-cli` linking and validate headless tests.

## Success Criteria
- `libfrontier_core` compiles/links without UI frameworks and exposes a stable C interface.
- Adapters build separately; swapping adapters requires no core changes.
- Headless and UI shells share behavior because logic is centralized in core, with UI-only concerns localized.
