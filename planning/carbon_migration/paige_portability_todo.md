# Paige Portability TODO

**Purpose**: Track the remaining work required to make the vendored Paige engine usable in the headless runtime (macOS/Windows/Linux) and, down the road, ensure the classic UI builds can link against the same sources without Carbon dependencies.

## Goals
- Provide a headless-safe implementation of the Paige “machine layer” so packing/unpacking WPText objects does not rely on QuickDraw/GDI at runtime.
- Ensure the same Paige sources can be built for both legacy UI targets and the new headless builds with minimal divergence.
- Cover macOS (arm64/x86_64) immediately and document what is needed for Linux/Windows.

## Work Items

| # | Area | Description | Files | Status |
|---|------|-------------|-------|--------|
| 1 | **Machine init** | Implement `pgMachineInit/pgMachineShutdown` equivalents that populate `pg_globals` without QuickDraw/GDI (reuse defaults from Windows/Mac sources, but back them with no-op graf devices). | `PGPLATFO/PGMAC.C`, `PGPLATFO/PGWIN.C`, new `PGPLATFO/PGUNX.C` (or similar), `PGHEADER/machine.h` | ☑ |
| 2 | **Graf/clip stubs** | Provide portable versions of `pgClipGrafDevice`, `pgEraseRect`, `pgPrepareOffscreen`, `pgScrollRect`, etc., so drawing-related calls short‑circuit safely while still updating Paige bookkeeping. | same as above | ☑ (baseline headless implementations live in `PGPLATFO/PGUNX.C`; revisit once UI hooks are needed) |
| 3 | **Font/style defaults** | Port `pgInitDefaultFont/pgInitDefaultStyle/pgInitDefaultPar` to rely on script-level defaults instead of OS font APIs. Verify the structs line up with what Frontier expects (`style_info`, `font_info`). | `PGPLATFO/PGMAC.C`, `PGPLATFO/PGWIN.C` | ☐ |
| 4 | **File I/O glue** | Ensure the UNIX `pgIO` layer is sufficient for `pgExportFileFromC` / `pgImportFileFromC` (clipboard support is not needed in headless). | `PGPLATFO/PGIO.C` | ☐ |
| 5 | **Memory traps** | Finish mapping the legacy handle macros (`pgAllocMemory`, `pgMemorySize`, etc.) to the portable allocator so Paige no longer references the classic trap names (remove reliance on `_DeleteMemory`, etc.). | `PGHEADER/pgMTraps.h`, `PGPLATFO/PGMEMMGR.C` | ☑ (done) |
| 6 | **Conditional build plumbing** | Add a real `UNIX_PLATFORM` (or `HEADLESS_PLATFORM`) branch in the Paige sources so the headless machine layer is compiled instead of `PGMAC.C`/`PGWIN.C`. Update `CMakeLists.txt` to build the new file and drop the unused platform modules. | `CMakeLists.txt`, `PGHEADER/CPUDefs.h` | ☐ |
| 7 | **Frontier hookup** | Replace the headless WP stubs so CLI/tests exercise Paige-backed metadata and set the stage for real serialization work. | `tests/Makefile`, `frontier-cli/Makefile`, `portable/wptext_portable.c`, `portable/wptext_runtime.c` | ☐ (headless init runs Paige and the new wrapper loads header metadata, but we still need full pack/unpack + editing support before calling this done) |
| 8 | **Runtime validation** | Add unit/integration tests that call `wpverbpack/wpverbunpack` under the headless build to confirm the real engine runs without a UI. | `tests/runtime_tests.c`, new fixtures | ☐ |
| 9 | **Cross-platform notes** | Document Windows/Linux deltas (e.g., Windows might still prefer GDI for desktop builds) so future work can reuse the headless layer or a thin wrapper around it. | this doc + `_CURRENT_STATUS.md` | ☐ |

## Notes
- Most undefined symbols reported by `nm -u libpaige.a` are graphics helpers declared in `PGPLATFO/PGMAC.C`/`PGPLATFO/PGWIN.C`. Implementing the headless graf layer (Items 1–3) is the gating factor before we can drop `-undefined dynamic_lookup`.
- Once Item 7 is complete, the WPText tracker (`wptext_rtf_tracker.md`) can proceed to the portable serializer work; until then we must retain `tests/headless_wp_stubs.c`.
- Keep Paige-specific portability details here; summarize high-level progress in `_CURRENT_STATUS.md` so other agents know whether real WP pack/unpack is available yet.
