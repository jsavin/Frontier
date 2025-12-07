# Processor Audit: `launch`

**Status:** ✅ **Partial Headless Compatibility** (3/5 verbs)
**Audit Date:** 2025-12-05
**Auditor:** Claude (Sonnet 4.5)

---

## Basic Information

| Property | Value |
|----------|-------|
| **Processor Name** | `launch` |
| **EFP ID** | 1007 (lang block) |
| **Verb Count** | 5 |
| **Window Required** | NO (core verbs are headless-compatible; Mac-specific ones skipped) |
| **Documentation** | [launch/](../../../docs/usertalk/docserver.userland.com/launch/index.html) |
| **Stub Implementation** | [headless_launch_verbs.c](../../../tests/headless_launch_verbs.c) |

---

## Category Assessment

**Category:** ✅ **Mixed - Core + Legacy Mac-Specific**

**Rationale:**
Core launch verbs (`application`, `appWithDocument`, `anything`) are process-spawning utilities that work on any platform (Windows, macOS, Linux). Legacy verbs (`appleMenu`, `resource`) are Mac OS Classic specific and can be skipped. The processor supports headless server operation via basic process launching (e.g., watchdog daemons, automation).

**Headless Compatibility:** ✅ **Partial** (3/5 verbs compatible)

**Blocking Verbs:**
- Mac-specific: `appleMenu`, `resource` (can skip)

---

## Verb Inventory

### Headless-Compatible Verbs (3/5)

| # | Verb Name | Signature | Description | Headless Status |
|---|-----------|-----------|-------------|-----------------|
| 1 | `application` | `launch.application(path) -> boolean` | Launch application/executable | ✅ IMPLEMENT (spawn process) |
| 2 | `appwithdocument` | `launch.appWithDocument(apppath, docpath) -> boolean` | Launch app with document argument | ✅ IMPLEMENT (spawn with args) |
| 3 | `anything` | `launch.anything(path) -> boolean` | Launch any file (generic file opener) | ✅ IMPLEMENT (spawn appropriate handler) |

### Mac-Specific Legacy Verbs (2/5 - SKIP)

| # | Verb Name | Signature | Description | Why Skip |
|---|-----------|-----------|-------------|----------|
| 4 | `applemenu` | `launch.appleMenu(itemname) -> boolean` | Launch item from Apple menu | Mac OS Classic only |
| 5 | `resource` | `launch.resource(path) -> boolean` | Launch Mac code resource | Mac resource fork (obsolete) |

**Note:** Documentation shows additional verbs (controlPanel, usingID) not in kernelverbs.rc - these are Mac OS Classic specific and likely obsolete.

---

## Implementation Analysis

### Complexity: **LOW to MEDIUM** (core verbs use standard process spawning)

### Dependencies

- **Other Processors:** sys (optional; similar capabilities)
- **External Services:** None
- **OS-Specific Functionality:** YES (process spawning varies by platform)
- **GUI/Window Context:** NO (core verbs are headless-compatible)

### Key Implementation Notes

**Mac OS Classic Origins:**
All launch verbs were designed for Mac OS Classic (System 7+) desktop environment:
- Finder integration for opening files
- Apple menu items
- Resource fork code resources (obsolete)
- Application launch via Launch Services (Classic)

**Modern Platform Equivalents:**

**macOS:**
```c
// launch.application() - Modern macOS
// Use NSWorkspace launchApplication:
[[NSWorkspace sharedWorkspace] launchApplication:@"/Applications/TextEdit.app"];

// OR use system() for command-line
system("open /Applications/TextEdit.app");

// launch.anything() - Open any file
system("open /path/to/document.pdf");  // Opens with default app
```

**Linux:**
```bash
# launch.application() - Linux
xdg-open /path/to/application  # Uses XDG desktop standards
gtk-launch application.desktop

# launch.anything() - Open file with default app
xdg-open /path/to/document.pdf
```

**Windows:**
```c
// launch.application() - Windows
ShellExecute(NULL, "open", "C:\\Program Files\\App\\app.exe", NULL, NULL, SW_SHOWNORMAL);

// launch.anything() - Open file
ShellExecute(NULL, "open", "C:\\Documents\\file.pdf", NULL, NULL, SW_SHOWNORMAL);
```

**Headless Server Reality:**
Core launch verbs ARE valid in headless mode:
- `launch.application()` spawns processes (watchdog daemons, helper executables)
- `launch.appWithDocument()` spawns with arguments (batch processing)
- `launch.anything()` opens files with handlers (format conversion, processing)
- Example: keepFrontierRunning.exe monitored Frontier process and relaunched if crashed

**Implementation Strategy:**
- **Unix/Linux:** Use `fork()/execvp()` or `posix_spawn()` for process launching
- **Windows:** Use `CreateProcess()` for process launching
- **macOS:** Use `fork()/execvp()` (or `NSTask` for app bundles)
- Skip GUI-specific features (appleMenu, Finder integration)

**Edge Cases:**
- macOS resource forks are obsolete (macOS X+ uses extended attributes)
- Apple menu is macOS Classic only (no longer exists)
- Control Panels are macOS Classic only
- Finder integration requires GUI session

**Type Coercion:**
- File paths are strings (Mac Classic used ":" separators, modern uses "/")
- Returns boolean (success/failure)

**Security Considerations:**
- Launching arbitrary executables is security-sensitive
- Path traversal vulnerabilities
- Privilege escalation if running as root/admin
- Code injection via malicious applications

---

## UserTalk Documentation Notes

From docserver.userland.com/launch/:

**launch.application(path)**
- Launches application at specified path
- Returns true if successful
- Does NOT activate/bring to front (use sys.bringAppToFront)
- Returns true immediately if app already running
- Faster than launch.anything
- Example (Mac Classic): `launch.application("System:System Folder:TeachText")`

**launch.anything(path)**
- Launches any file the Finder can open
- Opens documents with associated applications
- Can launch Control Panels, applications, documents
- **Requires System 7+** (Mac Classic)
- Always brings Finder to front before launching
- Slower than launch.application
- Example: `launch.anything("System:Hot Stuff:Great Frontier Ideas")`

**launch.appWithDocument(apppath, docpath)**
- Launches application and opens specific document
- Useful for opening files in non-default applications
- Example: `launch.appWithDocument("TeachText", "MyDocument")`

**launch.appleMenu(itemname)**
- Launches item from Mac OS Classic Apple menu
- **Completely obsolete** (Apple menu changed in macOS X+)

**launch.resource(path)**
- Launches Mac Classic code resource
- **Completely obsolete** (resource forks no longer used)

---

## Testing Requirements

**Core Verbs (Headless-Compatible):**
- `launch.application()`: Test spawning executables on Unix/Windows/macOS
- `launch.appWithDocument()`: Test spawning with command-line arguments
- `launch.anything()`: Test file handler association (generic open)
- Test error handling: non-existent paths, permission errors, invalid executables
- Test return values: success/failure
- Test with various executable types (scripts, binaries, applications)

**Mac-Specific Verbs (Skip Testing):**
- appleMenu, resource - Mac Classic only, not testable on modern platforms

---

## Implementation Effort

**Estimated Time:** 12-18 hours (core verbs only)

**Phase 1: Process Spawning Implementation (8-12 hours)**
- Unix/Linux: `fork()/execvp()` or `posix_spawn()` (3-4 hours)
- Windows: `CreateProcess()` implementation (3-4 hours)
- macOS: `fork()/execvp()` or `NSTask` wrapper (2-3 hours)

**Phase 2: File Handler Association (2-3 hours)**
- `launch.anything()` - determine file handler by extension/MIME type
- Platform-specific file association lookup

**Phase 3: Testing & Documentation (2-3 hours)**
- Unit tests for each verb on all platforms
- Integration testing with real executables
- Documentation and examples

**Confidence:** MEDIUM-HIGH - Process spawning is well-understood cross-platform operation

---

## Priority & Sequencing

**Priority:** 🟡 **MEDIUM-HIGH** (Phase 2 - Core Automation/System Integration)

**Recommended Implementation Order:** Phase 2, after file processor

**Blockers/Prerequisites:**
- sys processor already has similar capabilities (alternative)
- File processor implementation (for launch.anything())

**Implementation Sequence:**

**Phase 1 (High Priority):**
1. Implement `launch.application()` - spawn executable
2. Implement `launch.appWithDocument()` - spawn with arguments

**Phase 2 (Medium Priority):**
3. Implement `launch.anything()` - file handler-based opening
4. Add comprehensive cross-platform testing

**Skip:**
- `appleMenu` - Mac Classic only
- `resource` - Mac Classic resource forks (obsolete)

**Alternative Approach:**
For headless server needs, recommend:
1. Use `sys.unixShellCommand()` for process launching
2. Use direct process spawning (fork/exec, CreateProcess)
3. Use system utilities (systemd, launchd, Windows Services)
4. Document that launch.* verbs are not available in headless mode

---

## Headless Compatibility Analysis

**Fully Compatible:** ✅ 3/5 verbs

**Core Process Spawning (Headless-Compatible):**
- ✅ `launch.application()` - Spawn executable (watchdog daemons, automation)
- ✅ `launch.appWithDocument()` - Spawn with arguments (batch processing)
- ✅ `launch.anything()` - File handler-based opening (format conversion, processing)

**Mac-Specific Legacy (Not Compatible):**
- ❌ `appleMenu` - Mac Classic Apple menu (obsolete)
- ❌ `resource` - Mac Classic resource forks (obsolete)

**Recommendation:** **Implement 3 core verbs** for headless process spawning. Skip 2 Mac-specific verbs. Estimated effort: 12-18 hours.

---

## Related Processors

- **sys** - System operations (sys.bringAppToFront complementary)
- **file** - File operations (alternative for file handling)
- **Finder** - Finder integration (Mac Classic, also obsolete)

---

## Special Considerations

**Mac OS Classic Legacy:**
This entire processor is designed for Mac OS Classic (1984-2001) and is fundamentally incompatible with:
- Modern macOS (Darwin/Unix-based)
- Linux
- Windows
- Any headless environment

**Obsolete Concepts:**
- **Apple Menu:** Doesn't exist in macOS X+ (replaced by Application menu)
- **Resource Forks:** Obsolete data storage method (replaced by extended attributes)
- **Control Panels:** Mac Classic only (replaced by System Preferences/Settings)
- **Finder Launch Services:** Different architecture in modern macOS

**Modern Equivalents:**
For GUI Frontier (if ever implemented):

**macOS:**
```c
// Use NSWorkspace (Cocoa)
[[NSWorkspace sharedWorkspace] launchApplication:appPath];
[[NSWorkspace sharedWorkspace] openFile:docPath];

// Or use 'open' command
system("open /path/to/file");
```

**Linux:**
```bash
xdg-open /path/to/file        # Standard way
gtk-launch app.desktop         # GTK way
kde-open /path/to/file        # KDE way
```

**Windows:**
```c
ShellExecute(NULL, "open", path, NULL, NULL, SW_SHOWNORMAL);
// or CreateProcess() for more control
```

**Headless Server Alternative:**
```c
// Instead of launch.application("/usr/bin/myapp")
sys.unixShellCommand("/usr/bin/myapp &")  // Background process

// Instead of launch.anything("document.pdf")
// Don't open documents on headless server!
// Serve via HTTP, process programmatically, etc.
```

**Security Note:**
Launching applications from scripts is inherently dangerous:
- Arbitrary code execution
- Privilege escalation
- Malicious application launching
- Path traversal attacks

Modern systems use sandboxing, permissions, and user confirmation for security.

**Cross-Platform Challenges:**
- macOS, Linux, Windows have completely different launch mechanisms
- No single API works across platforms
- Desktop environments vary (GNOME, KDE, XFCE, etc.)
- File associations handled differently

**Why Headless DOES Need This:**
Headless servers often need process spawning for:
- Watchdog daemons (e.g., keepFrontierRunning.exe monitoring Frontier process)
- Batch processing (spawn external tools for format conversion, processing)
- Automation (trigger helper scripts, post-processors)
- Integration (launch external services, webhooks)

Modern alternatives (systemd, Docker, etc.) don't replace UserTalk script automation.
The launch.* verbs provide script-level process control, which is essential for automation.

---

## Alternative Solutions

**For Headless Process Launching:**

1. **Use sys.unixShellCommand():**
   ```usertalk
   // Instead of launch.application()
   sys.unixShellCommand("/usr/bin/myapp &")  // Background
   sys.unixShellCommand("nohup /usr/bin/myapp > /dev/null 2>&1 &")  // Daemon-like
   ```

2. **Use dedicated process spawning:**
   - Implement new `process.spawn()` verb
   - Modern API (fork/exec on Unix, CreateProcess on Windows)
   - Return process ID for monitoring
   - Non-blocking execution

3. **Use system service managers:**
   ```usertalk
   sys.unixShellCommand("systemctl start myservice")  // Linux systemd
   sys.unixShellCommand("launchctl start com.example.service")  // macOS
   ```

4. **Use containerization:**
   ```usertalk
   sys.unixShellCommand("docker run -d mycontainer")
   sys.unixShellCommand("podman run -d mycontainer")
   ```

**For Future GUI Frontier:**
If GUI mode is ever implemented:
1. Create modern `desktop.*` processor
2. Use platform-specific APIs (NSWorkspace, xdg-open, ShellExecute)
3. Support modern file associations
4. Support URL handlers (http://, mailto:, etc.)
5. Support drag-and-drop integration

---

## References

**Documentation:**
- DocServer: `docs/usertalk/docserver.userland.com/launch/`
- Individual verb pages: `docs/usertalk/docserver.userland.com/launch/{verb}.html.tmp`

**Implementation:**
- Stub: `tests/headless_launch_verbs.c`
- Canonical verb list: `Common/resources/Win32/kernelverbs.rc` (line 823-831)

**Mac OS Classic:**
- Inside Macintosh: Process Manager
- Inside Macintosh: Finder Interface
- Launch Services (obsolete API)

**Modern Alternatives:**
- macOS: NSWorkspace (Cocoa)
- Linux: XDG Desktop Entry Specification
- Windows: ShellExecute API
- Cross-platform: Qt QDesktopServices

---

## Recommendation

**For Headless Frontier:** ✅ **IMPLEMENT 3 CORE VERBS**

**Rationale for Implementation:**
1. `launch.application()`, `launch.appWithDocument()`, `launch.anything()` are process-spawning utilities
2. Essential for headless automation (watchdog daemons, batch processing, integration)
3. Real-world example: keepFrontierRunning.exe relied on process spawning
4. Effort is reasonable (12-18 hours for cross-platform implementation)
5. Complements sys.unixShellCommand() with more structured API

**Skip Mac-Specific Verbs:**
- `appleMenu` - Mac Classic Apple menu (obsolete)
- `resource` - Mac Classic resource forks (obsolete)

**Implementation Priority:** Phase 2 (Medium-High) - After file processor, before dialog system

**Estimated Timeline:**
- Phase 1: Core process spawning (8-12 hours)
- Phase 2: File handler association (2-3 hours)
- Phase 3: Testing & documentation (2-3 hours)
- **Total: 12-18 hours**

**Future Enhancement:**
Consider future `desktop.*` processor if GUI Frontier is ever built for Mac/Windows platforms.

---

## Next Steps

1. ✅ Audit complete - **ready for implementation**
2. ⏳ Implement Phase 1: `launch.application()` and `launch.appWithDocument()`
3. ⏳ Implement Phase 2: `launch.anything()` with file handler association
4. ⏳ Write comprehensive tests across Unix/Windows/macOS platforms
5. ⏳ Skip: `appleMenu`, `resource` (Mac Classic obsolete verbs)
6. ⏳ Update implementation status as "Headless-Compatible (3/5 verbs)"
7. ⏳ Document examples for watchdog daemons, batch processing, automation

---

**Audit Status:** ✅ Complete - **Partial Headless Implementation Recommended** (3/5 verbs)

**Core Verbs (Implement):**
- `launch.application()` - Spawn executable process
- `launch.appWithDocument()` - Spawn with command-line arguments
- `launch.anything()` - Open file with associated handler

**Legacy Verbs (Skip):**
- `appleMenu` - Mac Classic obsolete
- `resource` - Mac Classic resource forks (obsolete)

**Implementation Priority:** Phase 2 (Medium-High)
**Estimated Effort:** 12-18 hours
