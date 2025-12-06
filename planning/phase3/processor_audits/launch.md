# Processor Audit: `launch`

**Status:** ❌ **Not Recommended for Headless Implementation**
**Audit Date:** 2025-12-05
**Auditor:** Claude (Sonnet 4.5)

---

## Basic Information

| Property | Value |
|----------|-------|
| **Processor Name** | `launch` |
| **EFP ID** | 1007 (lang block) |
| **Verb Count** | 5 |
| **Window Required** | NO (but effectively YES - all verbs are GUI-dependent) |
| **Documentation** | [launch/](../../../docs/usertalk/docserver.userland.com/launch/index.html) |
| **Stub Implementation** | [headless_launch_verbs.c](../../../tests/headless_launch_verbs.c) |

---

## Category Assessment

**Category:** ❌ **GUI-Dependent / Desktop Integration**

**Rationale:**
All verbs in the launch processor are designed for Mac OS Classic desktop environment and require GUI/Finder integration. These verbs launch applications, open documents with applications, and interact with the Finder. None are applicable to headless server operation.

**Headless Compatibility:** ❌ **None** (0/5 verbs compatible)

**Blocking Verbs:** ALL (5/5)

---

## Verb Inventory

### All Verbs (GUI-Dependent)

| # | Verb Name | Signature | Description | Why Not Headless-Compatible |
|---|-----------|-----------|-------------|------------------------------|
| 1 | `applemenu` | `launch.appleMenu(itemname) -> boolean` | Launch item from Apple menu | Requires Mac OS Classic Apple menu |
| 2 | `application` | `launch.application(path) -> boolean` | Launch application | Requires GUI/desktop environment |
| 3 | `appwithdocument` | `launch.appWithDocument(apppath, docpath) -> boolean` | Launch app with document | Requires GUI/desktop environment |
| 4 | `resource` | `launch.resource(path) -> boolean` | Launch Mac code resource | Mac Classic only (resource forks) |
| 5 | `anything` | `launch.anything(path) -> boolean` | Launch any file via Finder | Requires Finder (Mac GUI) |

**Note:** Documentation shows additional verbs (controlPanel, usingID) not in kernelverbs.rc - these are Mac OS Classic specific and likely obsolete.

---

## Implementation Analysis

### Complexity: **VERY HIGH** (for modern cross-platform headless)

### Dependencies

- **Other Processors:** sys (for application management)
- **External Services:** None
- **OS-Specific Functionality:** YES (all verbs Mac OS Classic specific)
- **GUI/Window Context:** YES (all verbs)

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
In headless mode, these operations don't make sense:
- No GUI to display launched applications
- No user to interact with launched apps
- No desktop environment (Finder, Windows Explorer, etc.)
- Server processes should use `sys.unixShellCommand()` or direct process spawning instead

**Modern Alternatives:**
For headless server operations, use:
- `sys.unixShellCommand()` / `sys.winShellCommand()` - Execute programs directly
- Process spawning APIs (fork/exec on Unix, CreateProcess on Windows)
- Background daemon/service management
- Containerized processes (Docker, systemd)

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

**Not Recommended for Implementation**

If implemented for modern desktop Frontier:
- Test on macOS with actual GUI session
- Test on Linux with X11/Wayland session
- Test on Windows with desktop session
- Test error handling for non-existent applications
- Test with various file types (documents, apps, etc.)

**Not Applicable to Headless:**
All verbs require GUI/desktop environment and cannot be tested in headless mode.

---

## Implementation Effort

**Estimated Time:** N/A - **Not Recommended**

If pursued:
- Modernization: 12-20 hours (rewrite for modern OS APIs)
- Platform abstraction: 8-12 hours
- Testing: 6-8 hours
- Documentation: 2 hours
- **Total: 28-42 hours** for limited benefit in headless context

**Confidence:** LOW - Requires fundamental redesign for modern platforms

---

## Priority & Sequencing

**Priority:** ⛔ **NOT RECOMMENDED** (Phase 4 - Desktop GUI Features)

**Recommended Implementation Order:** N/A (defer indefinitely for headless)

**Blockers/Prerequisites:**
- GUI mode implementation
- Desktop environment integration
- Modern platform-specific APIs

**Alternative Approach:**
For headless server needs, recommend:
1. Use `sys.unixShellCommand()` for process launching
2. Use direct process spawning (fork/exec, CreateProcess)
3. Use system utilities (systemd, launchd, Windows Services)
4. Document that launch.* verbs are not available in headless mode

---

## Headless Compatibility Analysis

**Fully Compatible:** ❌ None (0/5 verbs)

**Partially Compatible:** ❌ None

**Not Compatible:** ✅ All (5/5 verbs)
- ❌ applemenu - Mac Classic Apple menu (obsolete)
- ❌ application - Requires desktop environment
- ❌ appwithdocument - Requires desktop environment
- ❌ resource - Mac Classic resource forks (obsolete)
- ❌ anything - Requires Finder/desktop integration

**Recommendation:** **Do not implement** for headless mode. Return "not available in headless mode" for all verbs.

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

**Why Headless Doesn't Need This:**
Headless servers should:
- Use direct process spawning (fork/exec, CreateProcess)
- Use service managers (systemd, launchd, Windows Services)
- Use background daemons
- Use containerization (Docker, LXC)
- NOT launch GUI applications

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

**For Headless Frontier:** ⛔ **DO NOT IMPLEMENT**

**Rationale:**
1. All 5 verbs require GUI/desktop environment
2. Mac OS Classic legacy (1984-2001) - obsolete concepts
3. Not applicable to server/headless operation
4. Modern alternatives exist (sys.unixShellCommand, process spawning)
5. Would require 28-42 hours for minimal benefit
6. Security concerns with arbitrary application launching

**Alternative:**
1. Return "not implemented in headless mode" for all launch.* verbs
2. Document that users should use `sys.unixShellCommand()` for process launching
3. Consider future `process.*` processor for proper process management
4. Consider future `desktop.*` processor if GUI Frontier is ever built

**User Migration:**
For scripts using launch verbs:
```usertalk
// Old (won't work in headless):
launch.application("/Applications/MyApp.app")

// New (headless compatible):
sys.unixShellCommand("open /Applications/MyApp.app")  // macOS
sys.unixShellCommand("/Applications/MyApp.app/Contents/MacOS/MyApp &")  // Direct

// Or for background processes:
sys.unixShellCommand("nohup /usr/bin/myapp > /dev/null 2>&1 &")
```

---

## Next Steps

1. ✅ Audit complete - **not recommended for implementation**
2. ❌ Skip implementation of launch processor
3. ✅ Document as "Not available in headless mode" in user documentation
4. ⏳ Create user migration guide for scripts using launch verbs
5. ⏳ Consider `process.*` processor for proper headless process management
6. ⏳ Update implementation status as "Deferred - GUI-dependent"

---

**Audit Status:** ✅ Complete - **Not Recommended for Headless Implementation**

**Implementation Strategy:** Return error "Not available in headless mode" for all verbs. Recommend users migrate to `sys.unixShellCommand()` or future `process.*` verbs.
