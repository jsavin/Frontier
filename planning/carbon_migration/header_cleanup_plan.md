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

## Step B — TextEncoding / TEC Macros (In progress)
Focus: consolidate Text Encoding Converter constants and stubs.

1. Move `kTextEncoding*`, `TECConvertText`, and related typedefs into a single portable shim (`portable/text_encoding_portable.h`). **Done**
2. Remove duplicate definitions from `headless_stubs.h` once the shim is in place. **Done**
3. Ensure headless code paths gracefully no-op (shim returns `kTextUnsupportedEncodingErr`; callers fall back or surface errors). **Done**
4. Rebuild key tests as in Step A. **Done**

## Step C — AppleEvent / Desktop Split (In progress)
Focus: keep AppleEvent/Component types away from headless builds.

1. Added portable header/shim (`appleevent_portable.h`) so headless builds avoid Carbon while desktop builds keep the real API. **Done**
2. Updated `langipc.h`, `osacomponent.h`, and `osincludes_portable.h` to route through the portable header and removed duplicate typedefs. **Done**
3. TODO: audit remaining AppleEvent typedefs/macros in shared headers (`macconv.h`, `processinternal.h`, etc.) and confine them to desktop-only modules.
4. Continue running CLI/headless tests as the split progresses.

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
