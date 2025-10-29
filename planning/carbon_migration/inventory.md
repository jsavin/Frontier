# Carbon Dependency Inventory

This table tracks every remaining classic Mac / Carbon dependency we must eliminate. When you start work on an item, flip its status to `in-progress`; once removed (or replaced with modern equivalents), mark it `done` and link to the PR/commit.

| Area | Symbol / Header | Usage (examples) | Owner | Status | Notes |
| --- | --- | --- | --- | --- | --- |
| QuickDraw handles | `RgnHandle`, `ControlHandle`, `MenuHandle`, `GrafPtr`, `Rect` macros | `quickdraw.h`, `langhash.c`, UI shells | @assistant | pending | Replace with portable structs or guard under desktop-only code. |
| Moveable handles | `Ptr`, `Handle`, `NewHandle`, `MemError`, `MaxBlock` | `memory.c`, `portable_handles.h` | @assistant | in-progress | Portable handle layer exists; finish swapping consumers to modern APIs. |
| AppleEvents / AE utils | `AEDesc`, `AERecord`, `TargetID`, `DescType`, AE creation helpers | `langhash.c`, `land.h`, `processinternal.h` | @assistant | pending | Need headless-compatible serialization or removal. |
| Text Encoding Converter | `TECCountAvailableTextEncodings`, `TECConvertText`, `kCFStringEncodingUTF8` | `strings.c` | @assistant | pending | Replace with portable UTF-8 conversions. |
| Alias manager | `AliasHandle`, `aliastofilespec`, `filespecsize` | `langhash.c`, `langsystem7.c` | @assistant | pending | Decide whether to support aliases or strip feature. |
| Resource manager | `GetString`, `Str255` resource tables | `strings.c`, `langhash.c` | @assistant | pending | Replace with internal tables or data files. |
| QuickDraw geometry helpers | `recttodiskrect`, `diskrecttorect`, `rgbtodiskrgb` | `langhash.c`, `quickdraw.c` | @assistant | in-progress | Portable macros exist; ensure prototypes exported in headless build. |
| Classic constants/macros | `chnul`, `chspace`, `isnumeric`, `ctdirections` | `strings.c`, `standard.h` | @assistant | in-progress | Ensure portable header defines these for headless build. |
| Extended float helpers | `dtox80`, `x80tod`, `safeldtox80` | `langhash.c`, `langpack.c`, `FastTimes.c` | @assistant | pending | Evaluate whether we still need 80-bit support; otherwise convert to double. |
| `land.h` dependencies | `typeChar`, `typeAlias`, `typeRGBColor`, etc. | `land.h`, AppleEvent glue | @assistant | pending | Replace with desktop-only build or new abstractions. |

> Add new rows whenever we discover another dependency. If you are unsure whether something is "Carbon", log it here and we will decide in the `decision_log.md`.
