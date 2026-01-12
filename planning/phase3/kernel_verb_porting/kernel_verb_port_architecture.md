# Kernel Verb Port Architecture

## Status
- State: Planning
- Phase: Headless Runtime Completion
- Created: 2025-12-03
- Owner: System Architect + legacy-c-application-expert
- Purpose: Systematic strategy for porting kernel functionality from legacy GUI runtime to headless CLI

## Executive Summary

The Frontier headless runtime currently fails during startup because critical kernel verbs are missing or stubbed out. This document provides a comprehensive architectural strategy for systematically identifying, categorizing, isolating UI dependencies, and porting the ~500+ kernel verbs to the headless runtime while maintaining strict separation from UI/Carbon code.

**Key Principle**: The headless runtime MUST NOT take ANY UI or Carbon dependencies. All ported code must be completely clean and portable.

## System Context

### Current Architecture

**Verb Registration System:**
- Kernel verbs defined in: `Common/resources/Win32/kernelverbs.rc` (1141 lines)
- Parser: `tools/efp_compiler/parse_rc.py` generates `generated/kernel_verbs_init.c`
- Auto-generated code calls processor-specific init functions (e.g., `fileinitverbs()`)
- Each processor registers a callback function that handles verb dispatch via token numbers

**Verb Processors (20 total):**
1. `op` (45 verbs) - Outline operations - REQUIRES UI
2. `table` (18 verbs) - Table operations - REQUIRES UI
3. `menu` (14 verbs) - Menu operations - REQUIRES UI
4. `wp` (27 verbs) - Word processor - REQUIRES UI
5. `pict` (4 verbs) - Picture operations - REQUIRES UI
6. `lang` (58 verbs) - Language runtime - MIXED (some portable, some UI)
7. `clock` (7 verbs) - Time operations - PORTABLE
8. `date` (30 verbs) - Date operations - PORTABLE
9. `dialog` (19 verbs) - Dialog operations - REQUIRES UI
10. `kb` (4 verbs) - Keyboard state - REQUIRES UI
11. `mouse` (2 verbs) - Mouse state - REQUIRES UI
12. `point` (2 verbs) - Point type operations - PORTABLE
13. `rectangle` (2 verbs) - Rectangle type operations - PORTABLE
14. `rgb` (2 verbs) - RGB color operations - PORTABLE
15. `speaker` (3 verbs) - Sound operations - PLATFORM-SPECIFIC
16. `target` (3 verbs) - Target window operations - REQUIRES UI
17. `bit` (8 verbs) - Bit operations - PORTABLE
18. `semaphore` (2 verbs) - Semaphore operations - PORTABLE (needs review)
19. `base64` (2 verbs) - Base64 encoding - PORTABLE
20. `tcp` (23 verbs) - TCP/IP networking - PORTABLE (needs socket impl)
21. `dll` (4 verbs) - DLL operations - PLATFORM-SPECIFIC (Windows)
22. `python` (1 verb) - Python integration - OPTIONAL
23. `htmlcontrol` (8 verbs) - HTML control - REQUIRES UI
24. `statusbar` (5 verbs) - Status bar - REQUIRES UI
25. `winregistry` (4 verbs) - Windows registry - PLATFORM-SPECIFIC
26. `string` (60 verbs) - String operations - PORTABLE
27. `file` (86 verbs) - File operations - MIXED (some UI for dialogs)
28. `rez` (15 verbs) - Resource operations - PLATFORM-SPECIFIC (Mac resources)
29. `window` (31 verbs) - Window operations - REQUIRES UI
30. `search` (6 verbs) - Search operations - REQUIRES UI
31. `filemenu` (10 verbs) - File menu - REQUIRES UI
32. `editmenu` (16 verbs) - Edit menu - REQUIRES UI
33. `sys` (16 verbs) - System operations - MIXED
34. `launch` (5 verbs) - App launching - PLATFORM-SPECIFIC
35. `clipboard` (2 verbs) - Clipboard operations - PLATFORM-SPECIFIC
36. `frontier` (14 verbs) - Frontier-specific operations - MIXED
37. `thread` (17 verbs) - Thread operations - PORTABLE (needs impl)
38. `mainwindow` (7 verbs) - Main window (Cancoon) - REQUIRES UI
39. `db` (13 verbs) - Database operations - PORTABLE
40. `xml` (14 verbs) - XML operations - PORTABLE
41. `html` (23 verbs + 5 searchengine + 11 mrcalendar + 7 webserver + 1 inetd) - HTML/Web - MIXED
42. `re` (10 verbs) - Regular expressions - PORTABLE (optional)
43. `math` (3 verbs) - Math operations - PORTABLE
44. `crypt` (5 verbs) - Cryptography - PORTABLE
45. `sqlite` (17 verbs) - SQLite database - PORTABLE (needs impl)
46. `mysql` (27 verbs) - MySQL database - PORTABLE (needs impl)

**Current Headless Stubs:**
- `tests/headless_file_verbs.c` - 10 file verbs implemented (basic I/O)
- `tests/headless_langverbs_stub.c` - Empty stub (returns true)
- `tests/headless_langipc_stub.c` - Empty stub
- `tests/headless_langregexp_stub.c` - Empty stub
- `tests/headless_op_stubs.c` - All op verbs stubbed
- `tests/headless_wp_stubs.c` - All wp verbs stubbed
- `tests/headless_menu_stubs.c` - All menu verbs stubbed
- `tests/headless_pict_stubs.c` - All pict verbs stubbed
- `tests/headless_table_stubs.c` - All table verbs stubbed
- `tests/headless_search_stubs.c` - All search verbs stubbed

**Legacy Implementations:**
- `Common/source/shellsysverbs.c` - sys, launch, clipboard, frontier verbs (1200+ lines)
- `Common/source/langverbs.c` - lang, clock, date, dialog, kb, mouse, etc. (4000+ lines)
- `Common/source/file.c` + related - file operations
- `Common/source/strings.c` + stringverbs - string operations
- Other scattered implementations

### Critical Constraint

**NO UI/CARBON DEPENDENCIES**: The headless runtime must be completely free of:
- Carbon API calls (QuickDraw, Window Manager, Menu Manager, Dialog Manager, etc.)
- WindowPtr, GrafPtr, ControlRef, MenuRef, DialogRef types
- Event Manager (EventRecord, GetNextEvent, etc.)
- Any Mac OS UI-specific data structures

## Assessment Strategy

### Phase 1: Systematic Inventory (1-2 days)

**Objective**: Create a complete map of all 500+ kernel verbs with categorization.

**Approach**: Automated analysis + manual review

#### Step 1.1: Parse kernelverbs.rc
Create a Python script that:
- Parses `Common/resources/Win32/kernelverbs.rc`
- Extracts all processors and their verbs
- Generates a CSV/JSON inventory file with columns:
  - Processor name
  - Verb name
  - Token number
  - Window required flag (from resource)
  - Implementation status (implemented/stubbed/missing)
  - Source file location (if implemented)

**Output**: `planning/phase3/kernel_verbs_inventory.json`

#### Step 1.2: Map to Implementation Files
For each verb, locate its implementation:
- Search pattern: Function name typically `<processor>func` (e.g., `langfunc`, `filefunc`)
- Look in known source files:
  - `Common/source/langverbs.c` - lang processor and sub-processors
  - `Common/source/shellsysverbs.c` - sys, launch, clipboard, frontier
  - `Common/source/file.c` - file operations
  - `Common/source/stringverbs.c` - string operations
  - Check header files for declarations

**Output**: Enhanced inventory with `impl_file` field

#### Step 1.3: Categorize by Portability
For each verb, determine portability category:

**Category A - PORTABLE** (can be ported as-is or with minimal changes):
- Pure computation (string, math, date, bit operations)
- Database operations (db, xml)
- Type conversions (lang.string, lang.long, etc.)
- Portable system calls (clock.now using `time()`)

**Category B - PLATFORM-SPECIFIC** (requires platform abstraction):
- File I/O (already has abstraction layer in `file.c`)
- Networking (TCP sockets - use POSIX APIs)
- System info (sys.os, sys.machine - needs detection logic)
- Process operations (launch, shell commands)

**Category C - UI-DEPENDENT** (cannot be ported, needs headless adapter):
- Window operations (window.*, op.*, wp.*, pict.*)
- Menu operations (menu.*)
- Dialog operations (dialog.alert, dialog.ask)
- Keyboard/mouse state (kb.*, mouse.*)
- Clipboard operations (needs platform-specific headless impl)

**Category D - OPTIONAL** (not critical for startup):
- Python integration (python.doscript)
- MySQL/SQLite (database connectivity)
- Regular expressions (re.*)
- DLL operations (Windows-specific)

**Output**: Enhanced inventory with `category` field

#### Step 1.4: Identify UI/Carbon Dependencies
For each implementation file:
- Use `grep` to find Carbon API usage:
  - Pattern: `(WindowPtr|GrafPtr|MenuRef|DialogRef|ControlRef|EventRecord|Point|Rect|RGBColor)`
  - Pattern: `(GetNextEvent|DrawMenuBar|GetNewDialog|NewWindow|ShowWindow)`
  - Pattern: `#include.*<Carbon|#include.*<QuickDraw|#include.*<Menus)`
- Flag functions that call UI-dependent code
- Trace call graphs to find indirect dependencies

**Output**: Enhanced inventory with `has_ui_deps` flag and `ui_calls` list

### Phase 2: Prioritization Analysis (1 day)

**Objective**: Determine which verbs are critical for startup and basic operation.

#### Step 2.1: Trace Startup Script Dependencies
Starting from the failing script execution:
- `frontier.getFilePath` (token 1 of frontier processor) - CRITICAL
- `file.folderFromPath` (token 24 of file processor) - CRITICAL
- Analyze `system.startup` scripts in the database to find other dependencies
- Build a dependency tree of verb calls

**Method**:
- Parse UserTalk scripts from `databases/Frontier-v6.root7`
- Extract verb calls using regex: `([a-z]+)\.([a-zA-Z]+)\s*\(`
- Build call graph
- Mark all verbs called from `system.startup` as PRIORITY-HIGH

#### Step 2.2: Define Tier System

**Tier 0 - STARTUP-CRITICAL** (must work for basic CLI operation):
- `frontier.getFilePath`
- `file.folderFromPath`
- `clock.now` (for testing)
- `string.*` (basic string operations)
- `lang.*` type conversions
- Any verb called from `system.startup`

**Tier 1 - CORE-FUNCTIONALITY** (needed for script execution):
- `file.*` basic I/O (open, close, read, write, readline)
- `date.*` operations
- `sys.os`, `sys.osversion` (platform detection)
- `db.*` operations (already mostly working)

**Tier 2 - EXTENDED-FUNCTIONALITY** (useful for most scripts):
- Remaining `file.*` verbs (file metadata, folder operations)
- `tcp.*` networking
- `base64.*`, `crypt.*` encoding
- `xml.*` operations

**Tier 3 - OPTIONAL** (nice-to-have):
- `sqlite.*`, `mysql.*` database connectivity
- `re.*` regular expressions
- Platform-specific operations (launch, registry)

**Tier 4 - UI-ONLY** (never port, provide stubs or adapters):
- All `window.*`, `op.*`, `wp.*`, `menu.*`, `dialog.*`
- `kb.*`, `mouse.*`, `clipboard.*`

**Output**: `planning/phase3/kernel_verbs_tiers.md`

## Dependency Isolation Architecture

### Pattern 1: Pure Extraction
**For verbs with NO UI dependencies**

**Strategy**: Copy implementation directly from legacy source to headless-specific file.

**Example**: `clock.now()` implementation
```c
// In Common/source/langverbs.c (legacy)
case nowfunc: {
    unsigned long now = timenow();
    return setlongvalue(now, v);
}

// Port to tests/headless_clock_verbs.c
static boolean clock_valueproc(short token, hdltreenode hparam1,
                                tyvaluerecord *vreturned, bigstring bserror) {
    switch (token) {
        case cv_now: {
            unsigned long now = timenow();
            return setlongvalue(now, vreturned);
        }
        // ... other clock verbs
    }
}
```

**Files to create**:
- `tests/headless_clock_verbs.c` - clock processor
- `tests/headless_date_verbs.c` - date processor
- `tests/headless_bit_verbs.c` - bit processor
- `tests/headless_math_verbs.c` - math processor

### Pattern 2: Platform Abstraction Layer
**For verbs with platform-specific dependencies (but not UI)**

**Strategy**: Create abstraction interface, implement for each platform.

**Example**: `file.folderFromPath()` (path parsing)
```c
// In Common/headers/file_portable.h
boolean portable_folderfrompath(const bigstring path, bigstring folder);

// In Common/source/file_portable_posix.c
boolean portable_folderfrompath(const bigstring path, bigstring folder) {
    // POSIX implementation using '/' as separator
    // Find last '/' and return everything before it
}

// In Common/source/file_portable_win32.c (future)
boolean portable_folderfrompath(const bigstring path, bigstring folder) {
    // Windows implementation using '\\' as separator
}

// In tests/headless_file_verbs.c
case fv_folderFromPath: {
    bigstring path, folder;
    if (!getstringvalue(hparam1, 1, path)) return false;
    if (!portable_folderfrompath(path, folder)) return false;
    return setstringvalue(folder, vreturned);
}
```

**Abstraction interfaces to create**:
- `Common/headers/file_portable.h` - File system operations
- `Common/headers/network_portable.h` - TCP/IP networking
- `Common/headers/process_portable.h` - Process launching/management
- `Common/headers/system_portable.h` - System info queries

### Pattern 3: UI Adapter Interface
**For verbs that require UI in GUI mode but can work differently in headless mode**

**Strategy**: Define interface, provide headless implementation that either:
- Returns sensible default
- Performs equivalent non-UI operation
- Raises "not supported in headless" error

**Example**: `dialog.alert()` (show alert dialog)
```c
// In Common/headers/dialog_adapter.h
typedef enum {
    DIALOG_RESULT_OK,
    DIALOG_RESULT_CANCEL,
    DIALOG_RESULT_YES,
    DIALOG_RESULT_NO
} DialogResult;

boolean adapter_alert(bigstring message, DialogResult *result);

// In Common/source/dialogs.c (GUI mode)
boolean adapter_alert(bigstring message, DialogResult *result) {
    // Use Carbon DialogRef to show actual alert
}

// In tests/headless_dialog_adapter.c (headless mode)
boolean adapter_alert(bigstring message, DialogResult *result) {
    // Print to stderr and auto-return OK
    fprintf(stderr, "[ALERT] %s\n", message);
    *result = DIALOG_RESULT_OK;
    return true;
}

// In tests/headless_lang_verbs.c
case dv_alert: {
    bigstring message;
    DialogResult result;
    if (!getstringvalue(hparam1, 1, message)) return false;
    if (!adapter_alert(message, &result)) return false;
    return setlongvalue(result, vreturned);
}
```

**Adapter interfaces to create**:
- `Common/headers/dialog_adapter.h` - Dialog operations (alert, ask, notify)
- `Common/headers/clipboard_adapter.h` - Clipboard get/put
- `Common/headers/window_adapter.h` - Window queries (for scripts that check window state)

### Pattern 4: Stub with Error
**For verbs that cannot work in headless mode**

**Strategy**: Implement verb to return error with clear message.

**Example**: `wp.gettext()` (get text from word processor window)
```c
case wpv_gettext: {
    langerrormessage(BIGSTRING("\pwp.gettext not supported in headless mode"));
    return false;
}
```

**Apply to**:
- All `window.*` operations
- All `op.*` outline operations
- All `wp.*` word processor operations
- All `menu.*` operations
- All `kb.*` and `mouse.*` operations (return false/default values)

## Implementation Phases

### Phase 1: Startup-Critical Verbs (3-5 days)

**Goal**: Get `system.startup` scripts executing successfully.

#### Phase 1.1: frontier.getFilePath (Day 1)
**Status**: BLOCKING startup

**Implementation**:
```c
// In tests/headless_frontier_verbs.c
static tyfilespec frontierfilefspec; // Set during db open

boolean headless_set_frontier_file(tyfilespec *fs) {
    frontierfilefspec = *fs;
    return true;
}

static boolean frontier_valueproc(short token, hdltreenode hparam1,
                                  tyvaluerecord *vreturned, bigstring bserror) {
    switch (token) {
        case frv_getFilePath: {
            // Return filespec of the currently open database
            return setfilespecvalue(&frontierfilefspec, vreturned);
        }
        // ... other frontier verbs
    }
}
```

**Integration point**: Call `headless_set_frontier_file()` after successful db open in CLI main.

#### Phase 1.2: file.folderFromPath (Day 1)
**Status**: BLOCKING startup (token 24 of file processor)

**Implementation**: Use Pattern 2 (Platform Abstraction)
- Create `Common/source/file_portable_posix.c`
- Implement path parsing functions
- Wire into existing `tests/headless_file_verbs.c`

#### Phase 1.3: String Verbs (Day 2)
**Status**: Likely called by startup scripts

**Strategy**: Port from `Common/source/stringverbs.c`
- Most string operations are pure computation
- Check for any UI dependencies (none expected)
- Copy implementations to `tests/headless_string_verbs.c`

**Verbs to port** (60 total - prioritize based on startup script analysis):
- `string.length`, `string.mid`, `string.nthChar`
- `string.insert`, `string.delete`
- `string.upper`, `string.lower`
- `string.replace`, `string.replaceAll`
- Others as needed

#### Phase 1.4: Date Verbs (Day 2-3)
**Status**: Likely called by startup scripts

**Strategy**: Port from `Common/source/langverbs.c`
- Date operations are portable (use standard time functions)
- Check for platform-specific date formatting (may need abstraction)
- Copy implementations to `tests/headless_date_verbs.c`

**Verbs to port** (30 total - prioritize based on startup script analysis)

#### Phase 1.5: Lang Type Conversion Verbs (Day 3)
**Status**: Essential for script execution

**Strategy**: Port from `Common/source/langverbs.c`
- Type conversions: `lang.string()`, `lang.long()`, `lang.boolean()`, etc.
- These are mostly portable
- Copy implementations to `tests/headless_lang_verbs.c`

#### Phase 1.6: Sys Verbs (Platform Info) (Day 4)
**Status**: Likely called by startup scripts

**Implementation**: Use Pattern 2 (Platform Abstraction)
- `sys.os()` - Return "macOS", "Linux", "Windows"
- `sys.osversion()` - Use `uname()` on POSIX
- `sys.machine()` - Return architecture (x86_64, arm64)

#### Phase 1.7: Dialog Adapters (Day 4-5)
**Status**: May be called by startup scripts

**Implementation**: Use Pattern 3 (UI Adapter)
- `dialog.alert()` - Print to stderr, auto-return OK
- `dialog.notify()` - Print to stderr, return true
- `dialog.ask()` - Return empty string or error in non-interactive mode
  - Consider environment variable for scripted responses

#### Phase 1.8: Testing & Integration (Day 5)
- Run `system.startup` scripts with verbose logging
- Fix any remaining missing verb errors
- Verify no UI dependencies leak in

**Success criteria**: `./frontier-cli --system-root databases/Frontier-v6.root7 -e "clock.now()"` succeeds

### Phase 2: Core File I/O (3-4 days)

**Goal**: Complete file verb implementation for script compatibility.

#### Phase 2.1: File Metadata Operations (Day 1-2)
Port remaining file verbs (currently only 10 of 86 implemented):
- `file.created()`, `file.modified()` - stat() calls
- `file.setCreated()`, `file.setModified()` - utimes() calls
- `file.type()`, `file.creator()` - Mac-specific (stub on Linux/return defaults)
- `file.isfolder()`, `file.isvolume()` - stat() S_ISDIR, statvfs()
- `file.exists()`, `file.size()` - stat() calls
- `file.delete()`, `file.rename()` - unlink(), rename()

#### Phase 2.2: File Path Operations (Day 2)
- `file.getPath()`, `file.setPath()` - work with filespec type
- `file.fileFromPath()` - extract filename
- `file.fullPath()` - construct absolute path

#### Phase 2.3: File System Operations (Day 3)
- `file.copy()` - implement using read/write loop
- `file.move()` - rename() or copy+delete across filesystems
- `file.newFolder()` - mkdir()
- `file.lock()`, `file.unlock()` - flock() on POSIX
- `file.isLocked()` - test file permissions

#### Phase 2.4: Volume Operations (Day 3-4)
- `file.freeSpaceOnVolume()` - statvfs()
- `file.volumeSize()` - statvfs()
- `file.getSystemFolderPath()` - return standard paths (HOME, /tmp, etc.)
- Platform abstraction for special folders

#### Phase 2.5: Testing (Day 4)
- Create test scripts exercising all file operations
- Test on multiple filesystems (local, network, if applicable)
- Verify no memory leaks in file handles

**Success criteria**: All file verbs work correctly in headless mode

### Phase 3: Extended Functionality (5-7 days)

**Goal**: Implement verbs needed for general script compatibility.

#### Phase 3.1: TCP Networking (Day 1-3)
Port tcp.* verbs (23 verbs):
- `tcp.openNameStream()`, `tcp.openAddrStream()` - socket(), connect()
- `tcp.readStream()`, `tcp.writeStream()` - recv(), send()
- `tcp.closeStream()` - close()
- `tcp.listenStream()` - socket(), bind(), listen(), accept()
- Use POSIX socket APIs
- Platform differences: Winsock initialization on Windows

**File**: `tests/headless_tcp_verbs.c`

#### Phase 3.2: Encoding/Crypto (Day 3-4)
Port encoding verbs:
- `base64.encode()`, `base64.decode()` (2 verbs) - portable
- `crypt.MD5()`, `crypt.SHA1()` (5 verbs) - portable (use existing implementations)

**File**: `tests/headless_encoding_verbs.c`

#### Phase 3.3: XML Operations (Day 4-5)
Port xml.* verbs (14 verbs):
- Most are already implemented in `Common/source/langxml.c`
- Check for UI dependencies (likely none)
- May just need to link existing code

#### Phase 3.4: HTML/Web Operations (Day 5-6)
Port html.* verbs (selective):
- `html.urlEncode()`, `html.urlDecode()` - portable
- `html.parseHttpArgs()` - portable
- `html.processMacros()` - check for UI deps
- Skip webserver verbs initially (Tier 3)

**File**: `tests/headless_html_verbs.c`

#### Phase 3.5: Process/Launch Operations (Day 6-7)
Port platform-specific verbs:
- `sys.unixShellCommand()` - popen() on POSIX
- `launch.application()` - fork()/exec() on POSIX, CreateProcess() on Windows
- Platform abstraction layer required

**File**: `tests/headless_process_verbs.c` with platform-specific implementations

#### Phase 3.6: Testing (Day 7)
- Integration tests for each verb category
- Cross-platform testing (macOS, Linux)
- Performance testing for networking/file I/O

**Success criteria**: All Tier 2 verbs functional

### Phase 4: Optional Extensions (As Needed)

#### Phase 4.1: Database Connectivity (Optional)
- `sqlite.*` verbs (17 verbs) - link libsqlite3
- `mysql.*` verbs (27 verbs) - link libmysqlclient
- Only implement if needed by target scripts

#### Phase 4.2: Regular Expressions (Optional)
- `re.*` verbs (10 verbs)
- Consider using PCRE library or similar
- Only implement if needed by target scripts

#### Phase 4.3: Python Integration (Optional)
- `python.doScript()` (1 verb)
- Embed Python interpreter
- Low priority

## Code Organization

### Directory Structure

```
Frontier/
├── Common/
│   ├── headers/
│   │   ├── file_portable.h          # Platform abstraction for file ops
│   │   ├── network_portable.h       # Platform abstraction for networking
│   │   ├── process_portable.h       # Platform abstraction for processes
│   │   ├── system_portable.h        # Platform abstraction for system info
│   │   ├── dialog_adapter.h         # UI adapter interface for dialogs
│   │   ├── clipboard_adapter.h      # UI adapter interface for clipboard
│   │   └── window_adapter.h         # UI adapter interface for window queries
│   └── source/
│       ├── file_portable_posix.c    # POSIX file implementations
│       ├── file_portable_win32.c    # Windows file implementations (future)
│       ├── network_portable_posix.c # POSIX networking implementations
│       ├── network_portable_win32.c # Winsock implementations (future)
│       ├── process_portable_posix.c # POSIX process implementations
│       ├── process_portable_win32.c # Windows process implementations (future)
│       └── system_portable.c        # Platform detection logic
├── tests/
│   ├── headless_frontier_verbs.c    # frontier.* implementations
│   ├── headless_file_verbs.c        # file.* implementations (EXPAND)
│   ├── headless_string_verbs.c      # string.* implementations (NEW)
│   ├── headless_clock_verbs.c       # clock.* implementations (NEW)
│   ├── headless_date_verbs.c        # date.* implementations (NEW)
│   ├── headless_bit_verbs.c         # bit.* implementations (NEW)
│   ├── headless_math_verbs.c        # math.* implementations (NEW)
│   ├── headless_lang_verbs.c        # lang.* implementations (EXPAND)
│   ├── headless_sys_verbs.c         # sys.* implementations (NEW)
│   ├── headless_tcp_verbs.c         # tcp.* implementations (NEW)
│   ├── headless_encoding_verbs.c    # base64.*, crypt.* implementations (NEW)
│   ├── headless_html_verbs.c        # html.* implementations (NEW)
│   ├── headless_process_verbs.c     # launch.*, process implementations (NEW)
│   ├── headless_dialog_adapter.c    # dialog.* adapter implementations (NEW)
│   ├── headless_clipboard_adapter.c # clipboard.* adapter implementations (NEW)
│   └── headless_ui_stubs.c          # All UI-only verbs (window, op, wp, menu, etc.)
└── planning/
    └── phase3/
        ├── kernel_verb_port_architecture.md     # This document
        ├── kernel_verbs_inventory.json          # Generated inventory
        ├── kernel_verbs_tiers.md                # Prioritization document
        └── kernel_verbs_implementation_log.md   # Daily progress log
```

### Naming Conventions

**Headless implementation files**:
- Pattern: `headless_<processor>_verbs.c` for verb implementations
- Pattern: `headless_<category>_adapter.c` for UI adapters
- Pattern: `<subsystem>_portable.h` and `<subsystem>_portable_<platform>.c` for platform abstractions

**Function names**:
- Init function: `<processor>initverbs()` (e.g., `stringinitverbs()`)
- Callback function: `<processor>_valueproc()` (e.g., `string_valueproc()`)
- Platform functions: `portable_<operation>()` (e.g., `portable_folderfrompath()`)
- Adapter functions: `adapter_<operation>()` (e.g., `adapter_alert()`)

**Token enums**:
- Pattern: `<processor>v_<verb>` (e.g., `fv_folderFromPath`, `cv_now`, `sv_upper`)
- Use descriptive names matching verb names from resource file

### Shared Code Strategy

**Maximize code reuse between GUI and headless**:

**Option A: Conditional compilation in shared file**
```c
// In Common/source/langverbs.c
#ifdef FRONTIER_HEADLESS
boolean langinitverbs(void) {
    // Headless-specific registration
    return headless_lang_init_verbs();
}
#else
boolean langinitverbs(void) {
    // GUI registration
    return gui_lang_init_verbs();
}
#endif

// Portable implementations stay in shared file
static boolean lang_coerce_string(...) {
    // Implementation used by both GUI and headless
}
```

**Option B: Extract pure functions to shared library**
```c
// In Common/source/lang_coercions.c (new file)
boolean lang_coerce_to_string(tyvaluerecord *val, bigstring bs) {
    // Pure implementation, no UI deps
}

// In Common/source/langverbs.c (GUI)
case stringfunc:
    return lang_coerce_to_string(val, result);

// In tests/headless_lang_verbs.c (headless)
case lv_string:
    return lang_coerce_to_string(val, result);
```

**Recommendation**: Use Option B for maximum clarity and testability.
- Create shared implementation files in `Common/source/`
- Keep verb dispatch and registration separate for GUI vs headless
- Share all pure computation logic

## Quality Standards

### No UI/Carbon Dependencies

**Enforcement Strategy**:

#### 1. Automated Checks (CI/build-time)
Create script: `tools/check_headless_deps.sh`
```bash
#!/bin/bash
# Check for Carbon/UI dependencies in headless code

HEADLESS_FILES="tests/headless_*.c tests/headless_*.h Common/source/*_portable*.c"

echo "Checking for UI dependencies in headless code..."

# Check for Carbon includes
if grep -r "#include.*<Carbon" $HEADLESS_FILES; then
    echo "ERROR: Carbon includes found in headless code"
    exit 1
fi

# Check for UI types
if grep -r "WindowPtr\|GrafPtr\|MenuRef\|DialogRef\|ControlRef" $HEADLESS_FILES; then
    echo "ERROR: UI types found in headless code"
    exit 1
fi

# Check for UI functions
if grep -r "GetNextEvent\|DrawMenuBar\|GetNewDialog\|NewWindow\|ShowWindow" $HEADLESS_FILES; then
    echo "ERROR: UI function calls found in headless code"
    exit 1
fi

echo "✓ No UI dependencies detected"
```

**Integration**: Add to Makefile as `make check-headless-deps`

#### 2. Link-Time Verification
- Headless binary should link successfully without Carbon framework
- Current CLI already demonstrates this (uses only Foundation/CoreFoundation)
- Monitor link flags: should NOT see `-framework Carbon` or `-framework ApplicationServices`

#### 3. Code Review Checklist
For each ported verb implementation, verify:
- [ ] No #include of Carbon/QuickDraw/WindowMgr/MenuMgr headers
- [ ] No WindowPtr, GrafPtr, MenuRef, DialogRef, ControlRef types
- [ ] No EventRecord, Event Manager calls
- [ ] No QuickDraw calls (SetPort, MoveTo, LineTo, etc.)
- [ ] No Window Manager calls (GetNewWindow, ShowWindow, etc.)
- [ ] No Menu Manager calls (NewMenu, AppendMenu, etc.)
- [ ] No Dialog Manager calls (GetNewDialog, ModalDialog, etc.)
- [ ] All file I/O uses portable abstraction layer
- [ ] All platform-specific code uses portable abstraction layer

### Testing Requirements

#### Unit Tests
Create test file: `tests/kernel_verb_tests.c`

For each ported verb:
```c
void test_string_length(void) {
    // Setup
    tyvaluerecord val;
    bigstring bs;

    copystring("\pHello", bs);
    setstringvalue(bs, &val);

    // Execute string.length()
    // Assert result equals 5

    // Cleanup
}
```

**Coverage target**: 80% of ported verbs have unit tests

#### Integration Tests
Create test scripts: `tests/scripts/verb_integration_tests.txt`

```
// Test frontier.getFilePath
msg(frontier.getFilePath())

// Test file.folderFromPath
local(path = "/Users/test/file.txt")
msg(file.folderFromPath(path))  // Should be "/Users/test"

// Test string operations
msg(string.length("Hello"))  // Should be 5
msg(string.upper("hello"))   // Should be "HELLO"

// Test clock operations
msg(clock.now())  // Should return current timestamp
```

**Run via**: `./frontier-cli --system-root db.root --script tests/scripts/verb_integration_tests.txt`

#### Memory Leak Testing
Use Valgrind/Address Sanitizer:
```bash
# Build with ASAN
make clean
CFLAGS="-fsanitize=address -g" make

# Run tests
./frontier-cli --system-root db.root -e "clock.now()"

# Check for leaks (should be none)
```

**Requirement**: Zero memory leaks in all ported verb implementations

#### Cross-Platform Testing

**Minimum test matrix**:
- macOS 14+ (ARM64) - primary development platform
- macOS 14+ (x86_64) - verify Intel compatibility
- Linux Ubuntu 22.04 (x86_64) - verify POSIX portability
- Linux Ubuntu 22.04 (ARM64) - verify ARM Linux

**Test suite**: Same integration tests must pass on all platforms

**Platform-specific tests**: For abstracted operations (file paths, network, etc.)

## Implementation Roadmap

### Week 1: Assessment & Startup-Critical Verbs
**Days 1-2**: Assessment
- Create inventory script
- Generate `kernel_verbs_inventory.json`
- Categorize all verbs
- Identify UI dependencies
- Create `kernel_verbs_tiers.md`

**Days 3-5**: Startup-Critical Implementation
- Implement `frontier.getFilePath`
- Implement `file.folderFromPath`
- Test `system.startup` execution
- Fix any additional blocking verbs discovered

**Milestone**: `clock.now()` executes successfully

### Week 2: Core File I/O & String Operations
**Days 1-3**: String verbs
- Port all string.* verbs (60 verbs)
- Create unit tests
- Integration testing

**Days 4-5**: File I/O expansion
- Implement file metadata operations
- Implement file path operations
- Create unit tests

**Milestone**: All Tier 0 and Tier 1 verbs implemented

### Week 3: Extended Functionality
**Days 1-2**: Date operations
- Port all date.* verbs (30 verbs)
- Create unit tests

**Days 3-4**: System operations
- Implement sys.* verbs with platform abstraction
- Port launch.* verbs
- Create platform-specific implementations

**Day 5**: Testing & stabilization
- Cross-platform testing
- Memory leak testing
- Performance testing

**Milestone**: All Tier 2 verbs implemented

### Week 4: Polish & Documentation
**Days 1-2**: Networking
- Implement tcp.* verbs
- Platform-specific testing

**Days 3-4**: Remaining Tier 2 verbs
- XML, HTML, encoding operations
- Integration testing

**Day 5**: Documentation & release prep
- Update developer guides
- Create verb compatibility matrix
- Document known limitations

**Milestone**: Headless runtime feature-complete for scripting

## Open Questions & Risks

### Open Questions

1. **Semaphore Implementation**: Are `semaphore.lock()` and `semaphore.unlock()` thread-safe in legacy code?
   - **Action**: Review legacy implementation for thread-safety
   - **Risk**: If not thread-safe, need to add proper synchronization primitives

2. **Thread Verbs**: What is the threading model in legacy Frontier?
   - **Action**: Analyze `Common/source/process.c` and thread implementation
   - **Risk**: Cooperative vs preemptive threading may require significant adaptation

3. **Resource Fork Operations**: `rez.*` verbs are Mac-specific
   - **Action**: Decide if these should be stubbed or if we need resource fork emulation
   - **Risk**: Some databases may rely on resource fork data

4. **Dialog Interactivity**: How should `dialog.ask()` behave in non-interactive headless mode?
   - **Option A**: Return error "not supported"
   - **Option B**: Read from environment variable or config file
   - **Option C**: Return empty string/default value
   - **Decision needed**: Depends on use cases

5. **Database Verb Coverage**: Are current `db.*` implementations complete?
   - **Action**: Test all db.* verbs systematically
   - **Risk**: Some may be stubbed or incomplete

### Risks & Mitigation

**Risk 1: Hidden UI Dependencies**
- **Description**: Legacy code may have subtle UI dependencies not caught by grep
- **Likelihood**: Medium
- **Impact**: High (can cause runtime crashes or linker errors)
- **Mitigation**:
  - Thorough code review of each ported function
  - Test with UI frameworks completely unlinked
  - Use static analysis tools to detect indirect calls

**Risk 2: Platform-Specific Behavior**
- **Description**: Code may assume Mac-specific filesystem/networking behavior
- **Likelihood**: High
- **Impact**: Medium (cross-platform testing will catch most issues)
- **Mitigation**:
  - Create comprehensive platform abstraction layer up front
  - Test on Linux early and often
  - Document platform-specific assumptions

**Risk 3: Verb Interdependencies**
- **Description**: Some verbs may depend on other verbs being implemented first
- **Likelihood**: Medium
- **Impact**: Medium (can block progress on verb porting)
- **Mitigation**:
  - Build dependency graph during assessment phase
  - Implement verbs in dependency order
  - Stub temporarily if circular dependencies found

**Risk 4: Performance Differences**
- **Description**: Portable implementations may be slower than Mac-optimized originals
- **Likelihood**: Low
- **Impact**: Low (headless usage likely less performance-critical)
- **Mitigation**:
  - Profile after implementation
  - Optimize hot paths as needed
  - Document known performance differences

**Risk 5: Testing Coverage Gaps**
- **Description**: Some verbs may be rarely used and hard to test
- **Likelihood**: High
- **Impact**: Medium (bugs in rarely-used verbs)
- **Mitigation**:
  - Create systematic test suite covering all verbs
  - Use real-world Frontier scripts for integration testing
  - Document untested/lightly-tested verbs

## Success Criteria

### Phase 1 Success
- [ ] `system.startup` scripts execute without errors
- [ ] `frontier.getFilePath` returns correct path
- [ ] `file.folderFromPath` works correctly on POSIX paths
- [ ] `clock.now()` returns valid timestamp
- [ ] All Tier 0 verbs implemented
- [ ] Zero Carbon/UI dependencies in headless code
- [ ] All tests pass on macOS

### Phase 2 Success
- [ ] All file.* I/O operations work correctly
- [ ] File metadata operations work on POSIX systems
- [ ] All Tier 1 verbs implemented
- [ ] Unit tests achieve 80% coverage
- [ ] Integration tests pass
- [ ] Zero memory leaks detected
- [ ] Tests pass on both macOS and Linux

### Phase 3 Success
- [ ] All Tier 2 verbs implemented
- [ ] TCP networking operations functional
- [ ] Platform abstraction layer complete
- [ ] Cross-platform testing complete
- [ ] Performance acceptable (no regressions vs. simple operations)

### Overall Success
- [ ] Headless runtime can execute arbitrary UserTalk scripts
- [ ] No UI/Carbon dependencies in headless codebase
- [ ] Comprehensive test coverage
- [ ] Clear documentation of implemented vs. stubbed verbs
- [ ] Known limitations documented
- [ ] Code organization follows architectural principles

## Appendix A: Verb Inventory Summary

**Total verbs**: ~500+

**By category**:
- Portable (Category A): ~200 verbs (40%)
- Platform-specific (Category B): ~100 verbs (20%)
- UI-dependent (Category C): ~180 verbs (36%)
- Optional (Category D): ~20 verbs (4%)

**By tier**:
- Tier 0 (Startup-critical): ~20 verbs
- Tier 1 (Core functionality): ~100 verbs
- Tier 2 (Extended functionality): ~200 verbs
- Tier 3 (Optional): ~50 verbs
- Tier 4 (UI-only, stub): ~180 verbs

**Current implementation status**:
- Implemented: ~20 verbs (4%)
- Stubbed: ~180 verbs (36%)
- Missing: ~300 verbs (60%)

## Appendix B: Related Documentation

- `planning/_CURRENT_STATUS.md` - Overall project status
- `planning/phase3/adapter_mode_isolation.md` - Database format adapter strategy
- `planning/phase3/frontier_root_headless_plan.md` - Headless runtime plan
- `planning/phase3/no_ui_linkage_policy.md` - UI dependency policy
- `planning/phase3/DEVELOPER_QUICKSTART_HEADLESS.md` - Developer guide

## Appendix C: Next Actions for Legacy-C-Application-Expert

The legacy-c-application-expert agent should focus on:

1. **Detailed Code Analysis**:
   - Examine `Common/source/langverbs.c` line-by-line for UI dependencies
   - Examine `Common/source/shellsysverbs.c` line-by-line for UI dependencies
   - Identify which functions are pure and can be extracted
   - Document any non-obvious Mac-specific assumptions

2. **Implementation Pattern Documentation**:
   - Document the existing verb dispatch pattern
   - Document error handling conventions
   - Document memory management patterns in verb implementations
   - Identify any gotchas or tricky aspects

3. **Priority Verb Deep-Dive**:
   - Provide detailed implementation guide for `frontier.getFilePath`
   - Provide detailed implementation guide for `file.folderFromPath`
   - Identify all functions these verbs call and their portability

4. **Dependency Graph Construction**:
   - Build call graph for startup-critical verbs
   - Identify shared utility functions needed
   - Flag any circular dependencies

5. **Testing Strategy Details**:
   - Suggest specific test cases for each verb category
   - Identify edge cases from legacy code analysis
   - Recommend testing patterns from legacy test suites (if any exist)

---

**Document Version**: 1.0
**Created**: 2025-12-03
**Last Updated**: 2025-12-03
**Status**: Ready for Implementation
