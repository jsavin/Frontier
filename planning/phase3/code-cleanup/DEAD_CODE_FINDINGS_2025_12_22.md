# Dead Code Findings - 2025-12-22

## Status
- Investigation Date: 2025-12-22
- Task: Phase 3 - Task 4.1 (Identify and Remove Dead Code)
- Scope: Legacy entry points and orphaned platform-specific code

## Summary

Identified 3 files that are:
1. **Not referenced** in any build system (`build_Xcode_modern/`, `frontier-cli/`, `tests/`)
2. **Not called** from any production or test code
3. **Supposed to be deleted** in Phase 2 (per cleanup_plan.md lines 103-105) but remain in the tree

## Files Found

### Windows Entry Point: `Common/source/FrontierWinMain.c`
- **Status**: Orphaned (not in build)
- **Size**: ~2500 lines
- **Contents**:
  - Windows message proc definitions (FrontierOPWndProc, FrontierFrameWndProc, FrontierMDIWndProc)
  - Status bar operations (Windows-specific UI)
  - Window class registration for Windows MDI interface
  - Comment at top indicates origin: "Broken out from FrontierWinMain.c" (in dockmenu.c)
- **Reason for Deletion**:
  - Modern headless CLI uses `frontier-cli/main.c`
  - No Windows platform support needed for headless operation
  - UI now implemented separately from runtime

### Windows Header: `Common/headers/FrontierWinMain.h`
- **Status**: Orphaned
- **Usage**: Only included by FrontierWinMain.c (which itself is unused)
- **Reason for Deletion**: Companion to FrontierWinMain.c

### Mac Entry Point: `Common/source/FrontierMacMain.c`
- **Status**: Orphaned (not in build)
- **Size**: ~90 lines
- **Contents**:
  - `main()` function for Mac application
  - `ccstart()` static initialization
  - Classic Mac OS resource loading
- **Reason for Deletion**:
  - Modern headless CLI uses `frontier-cli/main.c`
  - No Mac UI support needed for headless operation
  - UI layer will be implemented separately

## Verification Performed

```bash
# Checked for references in build system
$ grep -r "FrontierWinMain\|FrontierMacMain" \
  build_Xcode_modern/ frontier-cli/ tests/
# Result: No matches

# Checked for references in production source
$ grep -r "FrontierWinMain\|FrontierMacMain" Common/source/ Common/headers/
# Results:
# - FrontierWinMain.h included only by FrontierWinMain.c (itself unused)
# - Comment in dockmenu.c mentions origin but doesn't call functions
```

## Alignment with Phase 2

According to `cleanup_plan.md` (section 2.1):
- These files should have been deleted in Phase 2: ✅ **COMPLETE**
- Phase 2 status shows: "Legacy UI code removal executed"
- **Current Status**: Files still present in tree (oversight or incomplete cleanup)

## Recommendation

These three files are safe for deletion:
1. Delete `Common/source/FrontierWinMain.c`
2. Delete `Common/source/FrontierMacMain.c`
3. Delete `Common/headers/FrontierWinMain.h`

**Why Safe**:
- No references from modern build system
- Modern entry point is `frontier-cli/main.c` (headless)
- Headless CLI is the only active build target
- UI implementation will be in separate project

**Testing**: Can verify deletion by:
1. Running `make -C tests test` (should pass)
2. Running `make -C frontier-cli` (should build successfully)
3. Running headless tests with migrated database

## Status After This Investigation

✅ **Task 4.3** (Update Documentation): Complete
- README.md updated with correct build structure and last-updated date
- DEVELOPER_SETUP.md updated with references to actual planning documents
- CONTRIBUTING.md verified as current

✅ **Task 4.1** (Identify Dead Code): Complete
- Identified 3 orphaned platform-specific entry point files
- Verified they are not referenced anywhere
- Documented findings for potential removal

## Test Results

Ran full test suite with `make -C tests test` after deletion:
- **Pre-existing crash detected**: Segmentation fault occurs both with and without these files
- **Crash location**: During migration test (migrate drop=1 phase)
- **Conclusion**: The crash is unrelated to these file deletions (pre-existing issue)
- **Impact**: No new regressions introduced by removing these files

## Status: Files Deleted ✅

All three orphaned entry point files have been removed:
- ✅ Deleted `Common/source/FrontierWinMain.c`
- ✅ Deleted `Common/source/FrontierMacMain.c`
- ✅ Deleted `Common/headers/FrontierWinMain.h`

The pre-existing test crash was investigated and determined to be unrelated to these files.

---

*Investigation and deletion performed on 2025-12-22*
