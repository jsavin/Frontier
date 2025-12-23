# AppleEvent / OSA Dependency Map

Status
- State: In Progress
- Phase: Carbon Migration (Layer 3)
- Last Updated: 2025-11-20
- Owner: Codex
- Notes: Tracks remaining AppleEvent/OSA references; update counts as files are cleaned.

**Date:** October 31, 2025  
**Owner:** Codex  

This inventory covers the AppleEvent, AERecord, and related OSA Toolkit usage that still pulls classic Mac IPC into shared sources. Counts derived from `rg "AppleEvent"` across `Common/headers` and `Common/source`.

## Summary
- **High-impact modules:** `langipc.c` (AppleEvent dispatch from the language runtime) and `osacomponent.c` (OSA client glue) account for ~70% of all references.  
- **Headless impact:** Headless builds stub the APIs in `headless_stubs.h`, but many codepaths still expect real AppleEvents, causing runtime failures when evaluated without the desktop.  
- **Strategy:**  
  1. Introduce a headless IPC abstraction that bypasses AppleEvents (likely reusing Windows IPC approach).  
  2. Move OSA-specific code into macOS-only modules; provide portability layer for shared evaluator features.  
  3. Remove `AppleEvents.h` includes from the portable header tree once call sites are isolated.

## Occurrence Table
| Path | Count | Area | Notes | Status |
| --- | --- | --- | --- | --- |
| `Common/source/osacomponent.c` | 47 | Desktop OSA | Implements OSA component interaction, event construction, and menu sync. | Desktop-only refactor pending |
| `Common/source/langipc.c` | 20 | Core language IPC | Builds and sends AppleEvents for `launch`, `call`, `defined` etc.; headless currently fails here. | Replace with direct dispatch |
| `Common/headers/headless_stubs.h` | 6 | Headless scaffolding | Provides dummy typedefs for AppleEvent structs. | Remove once headless bypass exists |
| `Common/headers/langipc.h` | 4 | Shared header | Exposes AppleEvent-based APIs to language layer. | Redesign interface |
| `Common/source/osamenus.c` | 3 | Desktop menu sharing | Manages shared menu IPC via AppleEvents. | Desktop-only; gate |
| `Common/source/scripts.c` | 2 | Script recorder | Uses AppleEvents for recording. | Desktop-only |
| `Common/source/launch.m` | 2 | macOS launcher | Sends AppleEvents to Finder. | Desktop-only |
| `Common/source/langsystem7.c` | 2 | Classic glue | Gestalt checks for AppleEvents. | Remove |
| `Common/source/osawindows.c` | 1 | Windows port | Contains compatibility layer; good reference for headless design. | Use as model |
| `Common/headers/processinternal.h` | 2 | Process manager | Callback prototypes referencing AppleEvents. | Update after refactor |
| `Common/headers/osacomponent.h` | 2 | OSA header | AppleEvent typedef + function signatures. | Update |
| `Common/headers/osincludes_portable.h` | 2 | Portable include | Re-typedefs `AppleEvent` when Apple headers unavailable. | Remove after refactor |
| `Common/headers/macconv.h` | 2 | Legacy constants | Enumerations keep AppleEvent FourCCs. | Decide whether to keep for compatibility |
| `Common/headers/osamenus.h` | 1 | Header | Desktop-only. | Gate |
| `Common/headers/osainternal.h` | 1 | Header | Stores OSA callback UPPs. | Update |
| `Common/headers/xMacHeaders.c`, `Common/headers/shellheaders.c`, `Common/headers/carbonheaders.c` | 1 each | Aggregated includes | Remove from headless tree. | Todo |

## Next Steps
1. **Design headless IPC replacement:** Evaluate Windows implementation (`langwinipc.c`) and define a shared interface for script dispatch without AppleEvents.  
2. **Split AppleEvent modules:** Move `langipc.c` AppleEvent code behind macOS-only compilation units; headless build links new implementation.  
3. **Update portable headers:** Once the split exists, delete AppleEvent typedefs from `osincludes_portable.h` / `headless_stubs.h`.  
4. **Document migration plan in `_CURRENT_STATUS.md` and `status_log.md`** when the replacement path is defined.
