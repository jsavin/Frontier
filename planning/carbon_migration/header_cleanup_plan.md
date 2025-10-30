# Header Cleanup Plan
**Date:** October 31, 2025  
**Owner:** Codex  

This note tracks the staged cleanup of the portable header surface so headless builds no longer drag classic UI/API baggage. Work is ordered to minimise churn.

## Step A — QuickDraw / UI Extraction (Current)
Focus: remove QuickDraw geometry and UI-only helpers from the headless path.

1. **Inventory**
   - Types/macros currently defined in `osincludes_portable.h`: `Rect`, `Point`, `GrafPtr`, `WindowPtr`, `RGBColor`, menu/control handles, `FastMilliseconds`, string pixel helpers.
   - Callers: `strings.c`, `tabledisplay.c`, `quickdraw.c`, UI verbs under `Common/source`.

2. **Changes**
   - Introduce `Common/headers/desktop_quickdraw.h` (or similar) containing the UI structs/macros; include it only from desktop targets.
   - For headless build:
     * Provide minimal data structs in `portable` code only when needed (e.g., plain `Rect` for non-UI logic), or move logic into desktop modules.
     * Replace `stringpixels` and other UI metrics in shared code with portable alternatives (character-based heuristics or conditional compilation).
   - Update `tests/headless_mac_compat.c` and stubs to match the new structure.

3. **Verification**
   - `make -C tools/strings_compiler`
   - `make -C tests handle_tests`
   - `make -C tests strings_generated`

## Step B — TextEncoding / TEC Macros (Next)
Focus: consolidate Text Encoding Converter constants and stubs.

1. Move `kTextEncoding*`, `TECConvertText`, and related typedefs into a single portable shim (`portable/standard_portable.h` or new `portable/text_encoding_portable.h`).
2. Remove duplicate definitions from `headless_stubs.h` once the shim is in place.
3. Ensure headless code paths gracefully no-op (e.g., returning `kTextUnsupportedEncodingErr`).
4. Rebuild key tests as in Step A.

## Step C — AppleEvent / Desktop Split
Focus: keep AppleEvent/Component types away from headless builds.

1. Create macOS-only headers (`Common/headers/appleevent_desktop.h`) with the real AppleEvent typedefs and prototypes.
2. Modify `land.h`, `langipc.h`, `osacomponent.h`, etc., to include either the desktop header or a small headless shim depending on build flags.
3. Remove residual AppleEvent typedefs from `osincludes_portable.h` and `headless_stubs.h` once the split is complete.
4. Run CLI/headless tests to confirm IPC stubs still compile.

## Step D — Include Audit & Regression Build
Focus: ensure the include graph is minimal and consistent.

1. Scan core sources for redundant `#include "headless_stubs.h"` or `#include "osincludes_portable.h"` statements; trim accordingly.
2. Verify `frontier.h` remains the first include in each source.
3. Full rebuild:
   - `make -C tools/strings_compiler`
   - `make -C tests all`
   - A representative `frontier-cli` build.
4. Update status docs and remove temporary notes.

---

Track progress in `_CURRENT_STATUS.md` and this document. Update each section with findings or blockers so future sessions can resume easily.
