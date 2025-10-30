# QuickDraw Dependency Map
**Date:** October 31, 2025  
**Owner:** Codex  

The goal is to enumerate where QuickDraw-era helpers (`stringpixels`, `pushcliprgn`, `grayframerrgn`, `globaltolocalrgn`, `movepento`, etc.) still touch the shared code so we can isolate them to desktop builds.

## Summary
- **Primary concentration:** `Common/source/quickdraw.c` implements the helpers; downstream usage is almost entirely UI (shell windows, table display, progress bars).  
- **Headless exposure:** `Common/source/strings.c` calls `stringpixels` for truncation logic, pulling QuickDraw into the core build.  
- **Strategy:**  
  1. Provide a headless-safe string measurement heuristic inside `strings.c` (no QuickDraw include).  
  2. Move remaining UI callers under desktop-specific modules or guard with `#if !FRONTIER_HEADLESS`.  
  3. Retire `quickdraw.h` from the portable include tree once core callers are gone.

## Occurrence Snapshot
| Symbol | Top Callers | Count | Area | Proposed Action | Status |
| --- | --- | --- | --- | --- | --- |
| `stringpixels` | `strings.c` (4), `claylinelayout.c` (4), `quickdraw.c` (3), `tabledisplay.c` (2) | 17 | Core + UI | Replace with width-less truncation for core; leave UI under desktop guard. | Core work **Todo** |
| `pushcliprgn` | `quickdraw.c` (5), `shellupdate.c` (1) | 6 | UI plumbing | Keep inside desktop-only QuickDraw module. | Defer |
| `grayframerrgn` | `quickdraw.c` (2), `wpengine.c` (1) | 3 | UI | Desktop-only. | Defer |
| `globaltolocalrgn` | `quickdraw.c` (3), `shellupdate.c` (1) | 4 | UI | Desktop-only. | Defer |
| `movepento` | `progressbar.c` (6), `tabledisplay.c` (5), `quickdraw.c` (5), `about.c` (5), others | 28 | UI drawing | Desktop-only. | Defer |
| `RGBColor` structs | Spread across `langhash.c`, `colorverbs.c`, `quickdraw.c` | 9 | Mixed | Keep struct definitions in neutral header; limit color math to optional features. | Review |

Additional QuickDraw references live in:
- `Common/source/shellwindow.c`, `shellbuttons.c`, `tablecallbacks.c`, `progressbar.c`, `opdisplay.c`, etc. — all desktop-only UI modules.
- `tests/headless_mac_compat.c` provides stub replacements for a subset; once the core runtime no longer includes QuickDraw, the stub can be reduced to the handful of legacy tests that still rely on it.

## Next Steps
1. **Refactor `strings.c` truncation logic** to use character counts/UTF-8 width instead of QuickDraw metrics.  
2. **Split `quickdraw.c` into desktop module** compiled only for GUI builds; remove it from headless targets.  
3. **Annotate `_CURRENT_STATUS.md` and `status_log.md`** when the core dependency is removed.  
4. **Audit remaining QuickDraw includes** (`quickdraw.h`, `qd` macros) and eliminate them from portable headers.
