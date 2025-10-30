# Windows Parity Review
**Date:** October 31, 2025  
**Owner:** Codex  

Goal: understand how the legacy Win32 build avoided classic Mac APIs so we can reuse those strategies for the headless/portable refactor.

---

## Key Artifacts Reviewed
- `Common/headers/shell.win.h`
- `Common/source/frontierwindows.c`
- `Common/source/langwinipc.c`
- Win32 resource scripts (`Common/resources/Win32/*.rc`)

---

## Findings

### shell.win.h
- Declares the build-time feature matrix for Win32 (e.g., `PACKFLIPPED`, `noextended`, `flregexpverbs`). Many of these flags describe runtime behaviour rather than OS quirks and can be repurposed as semantic feature toggles for headless builds.
- Disables Mac-only facilities (`macBirdRuntime`, `IOAinsideApp`, `coderesource`), indicating the runtime already tolerated their absence.
- Sets `fldebug 1` and `FrontierCOM 1`, so some debug logging or COM-specific paths may be active; verify whether portable builds want the same defaults.

### frontierwindows.c
- Implements GUI plumbing (window creation, movement, invalidation) using Win32 APIs (`MoveWindow`, `SizeWindow`, `GetNewDialog`, etc.). The code is a 1:1 analogue of the Carbon UI helpers; it does not introduce reusable logic for headless beyond demonstrating that UI code can be isolated in platform-specific modules.
- All routines sit behind `frontierwindows.h` and are only needed for desktop UI. For headless we should exclude the entire unit rather than port it.

### langwinipc.c
- Provides a complete AppleEvents-free implementation of `langipcrunscript` and related helpers. All logic is platform-neutral: it manipulates Frontier processes, builds parameter lists, and invokes verbs without touching Win32 APIs.
- Core helpers of interest:
  * `langwinipcerrorroutine`: captures error output into a value record without depending on the Lang error window.
  * `langwinipcruncode`: spins up a Frontier process, runs compiled code, and tears it down—mirrors the AE dispatch path in `langipc.c` but without descriptors.
  * `langkernelbuildparamlist`: converts an `oplist` of parameters into executable code, including root-table lookups and named parameter support.
  * `langipcrunscript`: resolves dotted names against the search path, compiles scripts on demand, and funnels them through `langwinipcruncode`.
- Because this file avoids Windows-specific headers (only Frontier core headers are included), we can adapt it directly into a new headless IPC module to replace AppleEvents during the Carbon cleanup (Phase 5).

### Win32 resource tables
- String/list definitions for verbs (`kernelverbs.rc`, `WinLand.rc`) live in `.rc` files instead of STR# resources. The structures are plain text arrays, reinforcing that we can migrate STR# data to generated tables or text-based assets without losing functionality.

---

## Reuse Opportunities
1. **Headless IPC (Phase 5 AppleEvent isolation)**
   - Start from `langwinipc.c` to build a shared `langheadlessipc.c` that bypasses AppleEvents on every platform. Only minimal refactoring is required (naming, header guards, removing Windows-specific include paths).
   - Reuse `langwinipcerrorroutine`, `langkernelbuildparamlist`, and `langwinipcruncode` as-is to supply the new dispatcher.

2. **Feature flags**
   - Evaluate the macro set in `shell.win.h`; flags such as `PACKFLIPPED` (little-endian DB), `noextended`, or `threadverbs` can become portable feature toggles documented in the Carbon plan rather than OS-specific hacks.

3. **String resources**
   - The `.rc` string tables demonstrate a pre-existing non-STR# representation. Use them as a reference format when designing the portable string/error table generator (Phase 2).

---

## Open Questions / Follow-Ups
- Confirm whether any desktop Mac code relies on behaviour toggled by `shell.win.h` macros (e.g., `noextended`). If yes, define cross-platform equivalents before removing Carbon headers.
- Catalogue Win32-only modules that still pull classic Mac headers (if any) so we can keep the include graph clean once the AppleEvent/QuickDraw work is done.
- When cloning `langwinipc.c`, decide whether to keep the existing file and share implementation via `#if` or create a new platform-neutral module to avoid touching the Windows desktop build.
