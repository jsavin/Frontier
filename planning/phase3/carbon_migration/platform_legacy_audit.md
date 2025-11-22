# Platform Legacy Audit

Status
- State: In Progress
- Phase: Carbon Migration
- Last Updated: 2025-11-20
- Owner: Codex
- Notes: Catalog of platform-specific dependencies; update sections as code is modernized.

**Date:** October 30, 2025  
**Owner:** Codex

This document inventories the remaining dependencies on classic Mac OS tooling, HFS/resource-fork semantics, and other platform-specific code paths that still touch the headless build. Each section notes where the legacy surface appears, how the Windows port handled similar logic, and a recommended next step for the Carbon migration workstream.

---

## 1. Classic Memory Manager (`Handle`, `MemError`, `MaxBlock`)
| Aspect | Findings | Key Locations | Notes / Next Step |
| --- | --- | --- | --- |
| Allocation helpers | `memory.c` calls `NewHandle`, `DisposeHandle`, `SetHandleSize`, checks `MemError()` and `MaxBlock()` directly. | `Common/source/memory.c`, `Common/headers/memory.h`, `Common/headers/portable_handles.h` | Refactor to use the portable handle layer exclusively (e.g., `ClassicNewHandle` etc.), and surface status through the portable API (`ClassicMemError`, `ClassicMaxBlock`). |
| Portable implementation | `portable/classic_handle.{c,h}` emulates handles but does not yet track `MemError`/`MaxBlock` semantics. | `portable/classic_handle.c` | Extend to store last error / capacity hints so the core no longer depends on platform calls. |
| Windows precedent | Win32 build already bypasses the Mac memory manager (uses Win32 heap / `GlobalAlloc`). | `Common/headers/shell.win.h`, `Common/source/portable_handles.c` | Mirror that separation by keeping classic APIs behind desktop build flags. |

## 2. Resource Forks & STR# Lists (`getstringlist`, dialog strings, menu resources)
| Aspect | Findings | Key Locations | Notes / Next Step |
| --- | --- | --- | --- |
| String tables | `getstringlist` pulls dialog/error strings from STR# resources. Used widely for error reporting. | `Common/source/resources.c`, `Common/headers/resources.h`, numerous callers (`langhash.c`, `db.c`, `tableops.c`, etc.) | Replace with a portable string table (C arrays or modern resource files). Remove STR#/resource-fork logic from headless builds entirely. |
| Resource operations | APIs such as `GetResource`, `saveresource`, `loadresourcehandle` assume dual-fork files. | `Common/source/resources.c`, `Common/headers/resources.h` | Gate all resource-fork operations behind desktop-only macros. Introduce headless-safe replacements or migrate data to plain files. |
| Windows precedent | Win32 build bundles strings/resources in PE resources but many call sites fallback to constants. | `Common/windows` sources, resource scripts. | Use the same approach: embed constants or load from modern formats. |

## 3. Filespecs & Alias Manager (`tyfilespec`, `AliasHandle`, `langpackfileval`)
| Aspect | Findings | Key Locations | Notes / Next Step |
| --- | --- | --- | --- |
| Filespec struct usage | `tyfilespec` and `FSRef`/`FSSpec` appear in language verbs, database ops, and file verbs. | `Common/headers/langsystem7.h`, `Common/source/langhash.c`, `Common/source/file*.c`, `Common/source/langsystem7.c` | Design a modern path abstraction (POSIX path + metadata). Temporarily gate alias conversions for headless builds while new API lands. |
| Alias conversion | Functions `filespectoalias`, `aliastofilespec`, `langpackfileval` rely on Alias Manager. | Same as above plus `Common/source/langsystypes.c` | Replace with cross-platform serialization (e.g., absolute paths, UUID-based bookmarks). |
| Windows precedent | Windows port already uses native path strings (no aliases). | `Common/source/langwinipc.c`, `Common/source/fileverbs.c` (Win branches) | Align headless/Linux path handling with Windows logic. |

## 4. QuickDraw & UI Geometry Helpers (`Rect`, `RGBColor`, `stringpixels`)
| Aspect | Findings | Key Locations | Notes / Next Step |
| --- | --- | --- | --- |
| Drawing utilities | Functions such as `stringpixels`, `pushcliprgn`, `grayframerrgn` originate in QuickDraw but are called from core modules (`strings.c`, `langhash.c`). | `Common/source/quickdraw.c`, `Common/headers/quickdraw.h`, `Common/source/strings.c` | Extract UI-only helpers into desktop modules. Provide headless-friendly substitutes (e.g., measure strings via simple width heuristics or avoid pixel math altogether). |
| Color structs | `RGBColor`, `diskrgb` conversions currently defined in QuickDraw headers. | `Common/source/langhash.c`, `Common/headers/quickdraw.h` | Keep conversions but decouple from QuickDraw by moving structs into a neutral header shared with headless builds. |

## 5. AppleEvents & OSA Toolkit (`land.h`, `processinternal.h`, `langipc`)
| Aspect | Findings | Key Locations | Notes / Next Step |
| --- | --- | --- | --- |
| Event structures | Types like `AERecord`, `AEAddressDesc`, AppleEvent verbs appear across LANG/CLI layers. | `Common/SystemHeaders/land.h`, `Common/source/langipc.c`, `Common/source/osacomponent.c`, `Common/headers/processinternal.h` | Wrap these behind desktop-only macros. Implement a direct table lookup or stub that bypasses AppleEvents in headless builds. |
| Windows precedent | Windows port replaces AE with custom IPC (e.g., `langwinipc.c`). | `Common/source/langwinipc.c`, `Common/headers/langwinipc.h` | Use a similar approach: direct dispatch instead of AE when `FRONTIER_HEADLESS` (or non-Mac platform). |

## 6. Pascal String Macros & Legacy Character Helpers
| Aspect | Findings | Key Locations | Notes / Next Step |
| --- | --- | --- | --- |
| Macros (`getstringcharacter`, `setstringlength`, etc.) | Defined only in `standard.h` (classic include path). Headless builds need equivalents. | `Common/SystemHeaders/standard.h` | Ensure portable definitions live in `standard_portable.h` so all builds have them without depending on classic headers. |

## 7. OS Header Entanglement (`osincludes_portable.h`, `headless_stubs.h`)
| Aspect | Findings | Key Locations | Notes / Next Step |
| --- | --- | --- | --- |
| Duplication of typedefs | Both headers define `AEDesc`, `Component`, etc., causing redefinition warnings. | `Common/headers/osincludes_portable.h`, `Common/headers/headless_stubs.h` | Add guard macros so the portable header is authoritative and `headless_stubs.h` only fills gaps. |
| Inclusion tree | Many source files include `resources.h`, `quickdraw.h`, etc., even when not needed headless. | Entire `Common/source` tree | During each themed refactor, trim includes so headless builds only pull portable headers. |

## 8. Windows-Specific Hooks
Even though the focus is removing Mac dependencies, the Windows path also has bespoke logic (`frontierwindows.c`, `osawindows.c`). As part of the cleanup we should classify which parts are shared across desktop platforms and which should remain Windows-only modules. This will help prevent headless code from accidentally inheriting Windows APIs.

Key files to inspect:
- `Common/headers/shell.win.h`
- `Common/source/frontierwindows.c`
- `Common/source/osawindows.c`
- `Common/source/winregistry.c`

---

## Next Steps
1. **Document call-site mappings:**  
   - Generate lists of where each legacy helper (`getstringlist`, `filespectoalias`, `pushcliprgn`, etc.) is invoked.  
   - Record whether the usage is essential or can be replaced with constants/core logic.

2. **Sequence refactor branches using this audit:**  
   - Memory layer → Strings/resources → Filespec/Alias → QuickDraw → AppleEvents → Header cleanup.

3. **Update `_CURRENT_STATUS.md` and `status_log.md`:**  
   - Link this audit, and create a tracking checklist for refactor PRs.

4. **Consult Windows implementation details where noted** to avoid re-inventing platform-neutral replacements.

The audit will evolve as we explore each subsystem. All updates should continue to be logged in the Carbon migration status log.

---

## Detailed Task Breakdown

### Step 1 — Call-Site Mapping
1. **Search & extract**
   - Run `rg`/ctags queries for each legacy helper (e.g., `getstringlist`, `langpackfileval`, `pushcliprgn`, `AERecord`, resource APIs).
   - Capture file/line summaries into temporary CSV/markdown files.
2. **Classify usage**
   - Tag each occurrence as “core/headless-critical”, “desktop-only”, or “legacy/dead”.
   - Note why the dependency exists (error string, UI, IPC, etc.).
3. **Summarise replacements**
   - For core usages, draft the intended replacement (constant string, POSIX path struct, direct table lookup).
   - Flag uncertainties or design questions for review.
4. **Publish mapping tables**
   - Store per-category tables under `planning/phase3/carbon_migration/maps/` (new directory).
   - Reference each table from this audit for easy navigation.
5. **Status updates**
   - When a category’s map is complete, add an entry to `status_log.md`.
   - Reflect overall progress in `_CURRENT_STATUS.md` under the Quick Status section.

### Step 2 — Refactor Sequencing
1. **Define branch scopes**
   - Memory layer cleanup  
   - Strings/resource replacement  
   - Filespec/Alias retirement  
   - QuickDraw/UI extraction  
   - AppleEvents isolation  
   - Header/portable guard cleanup
2. **List prerequisites**
   - Required call-site maps, replacement helpers, and any data migrations.
   - Tests/documentation that must exist before landing the branch.
3. **Implementation checklist**
   - Outline code edits, new helpers, and tests for each branch.
   - Include verification steps (CI targets, headless regression tests).
4. **Scheduling matrix**
   - Create a table pairing branch → prerequisites → owner/ETA.
   - Update `_CURRENT_STATUS.md` and `status_log.md` when a branch starts/completes.
5. **Follow-up tracking**
   - Record post-merge cleanup or follow-on tasks in the status log.
   - Ensure each branch has a documentation addendum describing removed legacy behaviour.
