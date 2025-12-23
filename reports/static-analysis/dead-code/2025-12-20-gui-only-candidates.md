# GUI-Only Code Candidates for Removal

Analysis Date: 2025-12-20

## Overview

Frontier is now headless-only. The following files are explicitly GUI/display-related and are candidates for removal or wrapping with `#ifdef` guards to exclude them from headless builds.

## Files Identified (28 total)

These files contain menu, window, dialog, and display management code specific to the macOS GUI:

### Menu-Related (9 files)
- `menu.c` - Core menu handling
- `menubar.c` - Menu bar management
- `menueditor.c` - Menu editor UI
- `menufind.c` - Menu find functionality
- `menuresize.c` - Menu resizing
- `menupack.c` - Menu verbs/scripting interface
- `menuverbs.c` - Menu verb bindings
- `shellmenu.c` - Shell menu integration
- `shellwindowmenu.c` - Window-specific menu handling

### Window/Dialog-Related (10 files)
- `dialogs.c` - Dialog box management
- `filedialog.c` - File open/save dialogs
- `frontierwindows.c` - Core window management
- `langdialog.c` - DialogScript verb bindings
- `langerrorwindow.c` - Error dialog display
- `miniwindow.c` - Mini window support
- `shellwindow.c` - Shell window integration
- `shellwindowverbs.c` - Window verb bindings
- `tablewindow.c` - Table display windows
- `cancoonwindow.c` - Cancoon (browser) window

### Display/Drawing-Related (7 files)
- `opdisplay.c` - Outliner display
- `opdisplay_desktop.c` - Desktop/layout display
- `tabledisplay.c` - Table display rendering
- `textdisplay.c` - Text editor display
- `dockmenu.c` - Dock menu
- `langipcmenus.c` - IPC menu handling
- `osawindows.c` - OSA window management

## Next Steps

### Immediate Actions
1. Check which of these files are referenced from non-GUI code
2. If referenced: wrap with `#ifdef GUI_BUILD` guards
3. If not referenced: mark for deletion as part of v7 cleanup

### Investigation Needed
- Which files have exported verbs that users might depend on?
- Which files define structures used by other modules?
- Are any menu/window operations critical to database scripting?

## Notes

- Some files may be referenced indirectly through verb tables
- Dialog-related code might be used by UserTalk scripts
- Menu verbs may need to be stubbed rather than completely removed
