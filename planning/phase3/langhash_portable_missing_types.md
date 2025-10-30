# Headless `langhash.c` Dependency Inventory

Status
- State: In Progress
- Phase: 3 (Headless Runtime)
- Last Updated: 2025-10-30
- Notes: Track every legacy helper the portable build still needs; keep this list in sync with `planning/carbon_migration/inventory.md`.

Related Docs
- `planning/carbon_migration/inventory.md`
- `planning/phase3/headless_legacy_table_loader.md`

Change Log
- 2025-10-30: Restored from archive and aligned with the Carbon migration plan.
- 2025-10-28: Captured the initial dependency inventory from the portable build.

Overview
- `langhash.c` now compiles against the real runtime in headless mode, exposing historical QuickDraw and AppleEvent helpers the portable layer lacks.
- This document catalogs every missing symbol so we can either provide portable replacements or gate the code behind `#if !FRONTIER_HEADLESS`.

Details

| Symbol / Type | Where `langhash.c` uses it | Legacy purpose | Headless action items | Status |
| --- | --- | --- | --- | --- |
| `getstringcharacter` | Hash seeding (`langhash.c:989`) | Macro in `standard.h` that indexes a Pascal string. Expected to work in all builds; the current error means the macro expands only when `standard.h` comes through the classic include path. | Ensure the `FRONTIER_HEADLESS` include path still defines the macro (double-check that `standard.h` is visible before we reach the macro call). | Todo |
| `getstringlist` | Error reporting path (`langhash.c:2380`) | Loads a resource-backed string list to format hash errors. | Either stub this out to use the modern logging pathway or provide a headless-safe wrapper that pulls diagnostics from the in-memory list. | Todo |
| `recttodiskrect`, `diskrecttorect` | Table serialization of `rect` values (`langhash.c:2526`, `3003`) | QuickDraw utilities that convert between in-memory `Rect` and the “diskrect” structure stored on disk. | Portable layer already has trivial implementations in `headless_mac_compat.c`; expose prototypes through a headless header so `langhash.c` sees them. | Todo |
| `rgbtodiskrgb`, `diskrgbtorgb` | Packing RGB color values (`langhash.c:2539`) | Converts QuickDraw RGB triplets to the on-disk “diskrgb” representation. | Same story as rectangles—surface the portable prototypes and provide real conversions (rather than the all-zero stubs we have today). | Todo |
| `dtox80` / `xtodouble` | Scalar coercion (`langhash.c:2554`) | Converts between IEEE doubles and the classic 80-bit extended format used in legacy tables. Implemented in `Common/source/FastTimes.c`. | Export the helper through a portable header (or wrap with `#if !FRONTIER_HEADLESS` if we plan to replace the serializer). | Todo |
| `pushcliprgn`, `globaltolocalrgn`, `grayframerrgn`, `fillrect`, `rectinregion` | Drawing diagnostics inside the hash table viz path | QuickDraw GUI helpers. | For headless we can either stub these with no-ops or refactor the diagnostic code behind `#if !FRONTIER_HEADLESS`. The routines only affect on-screen debugging. | Todo |
| `langipcconvertaelist`, `langipcbuildsubroutineevent` and friends | Legacy AppleEvent conversions when unpacking lists/records | Converts AppleEvent descriptors back into UserTalk values. Only needed when processing verb tables packed as AE records. | We already gated the primary `langipcconvertaelist` call behind `#if !FRONTIER_HEADLESS`, but any remaining references should either be similarly gated or redirected to a headless equivalent. | Todo |
| `DebugStr`, `Debugger` | Assertion path (`langhash.c:8540`) | Classic Mac debugging breakpoints. | Provide no-op implementations (done in `headless_mac_compat.c`) and declare them in the portable headers so other modules can see them. | Todo |
| `FastMilliseconds` | Profiling support (`lang.c`, indirectly used by hash logging) | Reads the OS high-resolution clock. | Headless shim should return `clock_gettime` values; we added the implementation but still need the prototype exported globally. | Todo |

> **Note**: Keep every symbol in the table for traceability—update the Status column (e.g., Todo → In Progress → Done) rather than deleting rows once a helper is resolved.

Open Questions
- Are there additional dependencies introduced when we enable the GUI debugging paths, or can they remain permanently gated for headless?
- Which of these helpers are best solved with true portable replacements versus short-term shims while Carbon retirement proceeds?

Next Steps
- Wire the confirmed helpers (`getstringcharacter`, `dtox80`, `FastMilliseconds`) into the portable headers and close them out in the Carbon inventory.
- Audit the `langipc` AppleEvent paths to decide whether headless needs coverage or if we can defer them until after the Carbon migration.
- Re-run `make -C tests test_migration` once each item is addressed and strike it from this list when the build goes clean.
