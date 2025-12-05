# Analysis of shell*.c Files - What to Keep vs Delete

## Files You Were Concerned About

### 1. **shellops.c** - SAFE TO DELETE

**What it does**: Contains utility functions for window display/styling operations
- `shellsetscrollbarinfo()` - Updates scrollbar display parameters
- `shellsetdefaultstyle()` - Sets font style for window UI
- `shellsetselectioninfo()` - Keeps window font/size in sync with UI settings

**Usage**: Only called from other UI components (shell*.c files that are being deleted)

**Headless Impact**: Zero. These are pure UI display functions. No headless code needs them.

**Stubs Available**: None needed (no calls from headless code)

**Decision**: ✅ **DELETE**

---

### 2. **shellscrap.c** - NEEDS CAREFUL CONSIDERATION

**What it does**: Manages the "scrap" (clipboard/pasteboard) system with typed data
- `shellsetscrap()` - Set clipboard data with type and callbacks
- `shellgetscrap()` - Get clipboard data from system
- `shellexportscrap()` - Export clipboard to system scrap
- `shelldisposescrap()` - Clean up clipboard data

**Actual Callers** (in headless-capable code):
1. `scripts.c` - Sets/gets script outline in clipboard
2. `menupack.c` - Sets/gets menu structures in clipboard
3. `pictverbs.c` - Sets/gets picture data in clipboard
4. `tablewindow.c` - Gets table data from clipboard
5. `claybrowserstruc.c` - Sets/gets hash table outline in clipboard
6. `langerrorwindow.c` - Sets error text in clipboard
7. `tablewindow.c`, `tablescrap.c` - Table scrap operations
8. `outlineland.c` - Outline data in clipboard

**The Problem**: These callers are mixed UI/non-UI code. For example:
- `scripts.c` (language engine) calls scrap functions to manage cut/copy/paste of script outlines
- `tablewindow.c` (UI window) calls scrap for table operations
- `outlineland.c` (outline data structure) calls scrap

**Headless Reality**:
- Headless has **NO clipboard interaction**. It has no GUI to paste into.
- The `headless_mac_compat.c` stub provides dummy implementations:
  ```c
  boolean shellsetscrap (void *p, ...) { return false; }
  boolean shellgetscrap (Handle *hh, ...) { if (hh) *hh=nil; return false; }
  ```
- Headless code paths that try to use clipboard just get `false` returns

**Decision**: ⚠️ **KEEP FOR NOW - WITH CAVEATS**
- The real scrap functionality is not used in headless
- But removing it would require auditing many mixed files (scripts.c, menupack.c, etc.) to ensure they handle `shellsetscrap()` returning false gracefully
- Since headless already has working stubs that return false, keeping this file is low-risk for now
- **Future**: Once clipboard-using code is refactored or deleted (UI components), this can go

---

### 3. **shellcallbacks.c** - KEEP FOR FUTURE UI CUSTOMIZATION

**What it does**: Manages the "callbacks" system - a global registry of customization hooks
- `shellnewcallbacks()` - Allocate a new callback context
- `shellfindcallbacks()` - Look up callback context by resource ID

**Data Structure**: `globalsarray` - Array of `tyshellglobals` (context structures)
- Stores per-context state (scripts, tables, outlines, etc.)
- Each context type can have its own callback handlers

**The Callback Architecture**:
This is NOT just UI callbacks. The callback system enables runtime customization at multiple levels:
- **UI callbacks**: Window display, menu handling, drawing (currently UI-only in separate project)
- **System callbacks**: Save/write overrides, file operations, extensibility hooks (needed for headless too!)
- **Data callbacks**: Custom behavior for different data types

**Example**: The Save command has callbacks that enable scripts to take over how files are written to disk. This functionality is needed in headless mode for customization, but currently lives in the same structure as UI callbacks.

**Actual Callers** (every major subsystem):
1. `scripts.c` - Script editor callbacks
2. `langerrorwindow.c` - Error handling callbacks
3. `dbstats.c` - Stats window callbacks
4. `pictverbs.c` - Picture handling callbacks
5. `tablewindow.c` - Table editor callbacks
6. `wpverbs.c` - Word processor callbacks
7. `menuverbs.c` - Menu editor callbacks
8. `langexternal.c` - External function processor (system callbacks!)

**Headless Reality**:
- ❌ There is NO headless stub for `shellnewcallbacks()` or `shellfindcallbacks()`
- ✅ Yet `scripts.c` compiles and runs in headless builds
- This means either: (a) The callbacks are used in headless but conditionally, or (b) The code path doesn't execute in tests

**The Forward-Looking Plan**:
Once the UI is in a separate process, the callback system will be the interface between headless and UI:
1. Core runtime maintains callback registry in headless
2. UI process registers/calls callbacks via IPC for UI-specific operations
3. Non-UI system callbacks (Save hooks, extensibility) work directly in headless

**Decision**: ✅ **KEEP - CRITICAL FOR FUTURE ARCHITECTURE**

This file should NOT be deleted because:
1. It's the extensibility interface for custom save operations and system hooks
2. When UI moves to a separate process, this becomes the IPC boundary
3. Removing it now would require refactoring callbacks into the headless core (unnecessary work)
4. The current architecture is actually well-positioned for client/server separation

**Pre-Architecture Verification**:
Before any changes, should verify:
1. Which callbacks are actually UI-only (these can eventually be removed or delegated to UI process)
2. Which callbacks are system-level (these must stay in headless core)
3. How `scripts.c` and other callers handle headless vs UI contexts

---

### 4. **shellsysverbs.c** - DEFINITELY RELEVANT TO HEADLESS

**What it does**: Implements system-level verbs available to UserTalk scripts
- `sys.*` verbs: `system.version`, `system.task`, `system.machine`, `system.os`, etc.
- `frontier.*` verbs: `frontier.version`, `frontier.isRuntime`, `frontier.machineInfo`, etc.
- `launch.*` verbs: `launch.app`, `launch.appWithDoc`
- `clipboard.*` verbs: `clipboard.getScrap`, `clipboard.putScrap`
- `thread.*` verbs: `thread.count`

**System Verbs Implemented** (1,439 lines):
```
systemversionfunc     - system.version
systemtaskfunc        - system.task
machinefunc           - system.machine
osfunc                - system.os
getenvironmentvariablefunc   - system.getEnvironmentVariable
setenvironmentvariablefunc   - system.setEnvironmentVariable
unixshellcommandfunc  - system.unixShellCommand ⭐ HEADLESS-RELEVANT
frontierversionfunc   - frontier.version ⭐ HEADLESS-RELEVANT
hashstatsfunc         - frontier.hashStats
isruntimefunc         - frontier.isRuntime ⭐ HEADLESS-RELEVANT
countthreadsfunc      - frontier.countThreads ⭐ HEADLESS-RELEVANT
```

**UI-Only Verbs** (in the same file, but not usable in headless):
```
bringapptofrontfunc   - Bring app to foreground (macOS)
frontappfunc          - Get front app (macOS)
browsenetworkfunc     - Browse network (UI dialog)
launchappfunc         - Launch application (may not work headless)
apprunningfunc        - Check if app running
getscrapfunc          - clipboard.getScrap (requires UI scrap system)
putscrapfunc          - clipboard.putScrap (requires UI scrap system)
```

**Headless Stubs**:
- ❌ There's NO `headless_sysverbs_stub.c`
- This file is NOT in the stub list, which is suspicious

**Actual Usage in Headless**:
- `unixshellcommandfunc` - Needed for shell execution on Unix
- `system.os` - Useful to know what OS we're running on
- `frontier.version` - Basic system info
- `frontier.isRuntime` - May be useful
- `frontier.countThreads` - Thread info

**Compatibility Approach**:
- The file likely has `#ifdef` guards around UI verbs
- OR the headless build defines stub versions somewhere

**Decision**: ✅ **KEEP - WITH CONDITIONAL CLEANUP**
- This file is USED in headless mode
- Some verbs (launch.app, clipboard.*, bring to front) won't work but that's expected
- Some verbs (system.unixShellCommand, frontier.version) ARE useful in headless
- Better approach: Keep the file, wrap UI-only verbs in stubs that return reasonable errors
- Don't wholesale delete it

---

## Summary & Recommendations

### DELETE with Confidence:
- ✅ `shellops.c` - Pure UI display functions (scrollbar, font styling)

### KEEP (Already Handled by Stubs):
- ✅ `shellscrap.c` - Clipboard system (headless stubs return false gracefully)

### KEEP (Core Architectural):
- ✅ `shellcallbacks.c` - Extensibility/customization framework
  - Not just UI callbacks; includes system hooks (Save, file operations)
  - Critical for future client/server architecture with IPC
  - Will become the boundary between headless core and UI process

- ✅ `shellsysverbs.c` - System verb implementations
  - Actively used in headless (system.unixShellCommand, frontier.version)
  - UI verbs fail gracefully, don't break headless

### Updated Plan Changes:

**Section 2.2 should be revised to:**

```markdown
### 2.2 Remove Most Legacy Shell/Window UI Components (REVISED)

Delete:
- shell.c, shellwindow.c, shellactivate.c, shellscroll.c, shellfile.c
- shellhooks.c, shellverbs.c, shellbuttons.c, shelljuggler.c, shellprint.c
- shellmouse.c, shellblocker.c, shellupdate.c, shellkb.c, shellmenu.c
- shellundo.c, shellwindowmenu.c, shellwindowverbs.c, shell_api.c
- shellops.c ← Pure UI (scrollbar/font styling)

KEEP (Already Stubs Gracefully):
- shellscrap.c ← Clipboard system (stubs return false, callers handle it)

KEEP (Core Architecture - NOT UI-only):
- shellcallbacks.c ← Extensibility framework for customization hooks
  - Includes system callbacks (Save overrides, file operations, extensibility)
  - Will become IPC boundary when UI moves to separate process
  - Needed for future client/server architecture

- shellsysverbs.c ← System verb implementations
  - Actively used in headless (system.unixShellCommand, frontier.version, etc.)
  - UI verbs (launch.app, clipboard.*) fail gracefully
  - Essential for headless operation
```

---

## Pre-Deletion Test Procedure

Before final deletion of `shellcallbacks.c`:

1. Search for `shellnewcallbacks` calls:
   ```bash
   grep -r "shellnewcallbacks" tests/*.c
   ```
   If no matches, it's safe to delete.

2. Search for `shellfindcallbacks` calls:
   ```bash
   grep -r "shellfindcallbacks" tests/*.c
   ```
   If no matches, it's safe to delete.

3. Verify `scripts.c` works without callbacks:
   - Run script execution tests
   - If they pass, callbacks not needed in headless path

If all three checks pass: Safe to delete `shellcallbacks.c`
