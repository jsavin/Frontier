# Processor Audit: `sys`

**Status:** ⚠️ **Partial Headless Compatibility**
**Audit Date:** 2025-12-05
**Auditor:** Claude (Sonnet 4.5)

---

## Basic Information

| Property | Value |
|----------|-------|
| **Processor Name** | `sys` |
| **EFP ID** | 1006 (lang block) |
| **Verb Count** | 16 |
| **Window Required** | NO |
| **Documentation** | [sys/](../../../docs/usertalk/docserver.userland.com/sys/index.html) |
| **Stub Implementation** | [headless_sys_verbs.c](../../../tests/headless_sys_verbs.c) |

---

## Category Assessment

**Category:** ⚠️ **Mixed - Core + GUI-Dependent**

**Rationale:**
System utilities processor with both headless-compatible verbs (OS info, environment variables, shell commands) and GUI-dependent verbs (application management). Application management verbs (running apps, bringing to front) require GUI/window manager access and are not applicable in headless mode.

**Headless Compatibility:** ✅ **Partial** (9/16 verbs fully compatible)

**Blocking Verbs:**
- `browsenetwork` - GUI dialog
- `appisrunning` - Requires GUI/window manager
- `frontmostapp` - Requires GUI/window manager
- `bringapptofront` - Requires GUI/window manager
- `countapps` - Requires GUI/window manager
- `getnthapp` - Requires GUI/window manager
- `getapppath` - Requires GUI/window manager

---

## Verb Inventory

### Headless-Compatible Verbs (9 verbs)

| # | Verb Name | Signature | Description |
|---|-----------|-----------|-------------|
| 1 | `osversion` | `sys.osVersion() -> string` | Get OS version string |
| 2 | `systemtask` | `sys.systemTask() -> boolean` | Yield to other processes |
| 10 | `memavail` | `sys.memAvail() -> long` | Get available memory |
| 11 | `machine` | `sys.machine() -> string` | Get machine type |
| 12 | `os` | `sys.os() -> string` | Get OS name |
| 13 | `getenvironmentvariable` | `sys.getEnvironmentVariable(name) -> string` | Get environment variable |
| 14 | `setenvironmentvariable` | `sys.setEnvironmentVariable(name, value) -> boolean` | Set environment variable |
| 15 | `unixshellcommand` | `sys.unixShellCommand(cmd) -> string` | Execute Unix shell command |
| 16 | `winshellcommand` | `sys.winShellCommand(cmd) -> string` | Execute Windows shell command |

### GUI-Dependent Verbs (7 verbs)

| # | Verb Name | Signature | Description |
|---|-----------|-----------|-------------|
| 3 | `browsenetwork` | `sys.browseNetwork() -> string` | Browse for network application (GUI dialog) |
| 4 | `appisrunning` | `sys.appIsRunning(appname) -> boolean` | Check if app is running |
| 5 | `frontmostapp` | `sys.frontmostApp() -> string` | Get frontmost application name |
| 6 | `bringapptofront` | `sys.bringAppToFront(appname) -> boolean` | Bring app to front |
| 7 | `countapps` | `sys.countApps() -> long` | Count running applications |
| 8 | `getnthapp` | `sys.getNthApp(n) -> string` | Get nth running app name |
| 9 | `getapppath` | `sys.getAppPath(appname) -> string` | Get path to application |

**Note:** Documentation shows additional verbs (osName, appIsIACAware, getAppSize, getMinAppSize, setAppSize, setFrontApp, setMinAppSize) not in kernelverbs.rc - these may be UserTalk wrappers or obsolete Mac OS Classic verbs.

---

## Implementation Analysis

### Complexity: **MEDIUM**

### Dependencies

- **Other Processors:** None
- **External Services:** None
- **OS-Specific Functionality:** YES (all verbs are OS-specific)
- **GUI/Window Context:** YES (7/16 verbs)

### Key Implementation Notes

**Platform Detection:**
```c
// sys.os() - Identify operating system
#ifdef __APPLE__
    return "Mac OS X" or "macOS";
#elif defined(_WIN32)
    return "Windows";
#elif defined(__linux__)
    return "Linux";
#endif

// sys.machine() - Hardware architecture
// Use uname() on Unix, GetSystemInfo() on Windows
```

**OS Version Detection:**
```c
// sys.osVersion() - Get OS version string
// macOS: NSProcessInfo.operatingSystemVersionString or sw_vers
// Linux: uname() + /etc/os-release
// Windows: GetVersionEx() or RtlGetVersion()
```

**Memory Information:**
```c
// sys.memAvail() - Available memory
// macOS: vm_statistics64() via host_statistics64()
// Linux: sysinfo() or /proc/meminfo
// Windows: GlobalMemoryStatusEx()
```

**Shell Commands:**
```c
// sys.unixShellCommand() / sys.winShellCommand()
// Unix: popen() to execute command and capture output
// Windows: _popen() or CreateProcess()
// Security: Must sanitize input to prevent command injection!
```

**Environment Variables:**
```c
// sys.getEnvironmentVariable() - Read environment var
char* value = getenv(name);  // POSIX standard

// sys.setEnvironmentVariable() - Write environment var
#ifdef _WIN32
    SetEnvironmentVariable(name, value);
#else
    setenv(name, value, 1);  // POSIX
#endif
```

**Application Management (GUI-dependent):**
```c
// macOS: NSWorkspace, Process Manager APIs
// Linux: X11/Wayland (complex, desktop-dependent)
// Windows: EnumWindows(), FindWindow(), SetForegroundWindow()

// For headless mode: Return error or empty results
// These verbs are inherently GUI-dependent
```

**sys.systemTask():**
- Legacy Frontier: Yielded to Mac OS cooperative multitasking
- Modern headless: Could be no-op or sleep(0) to yield thread
- Useful for long-running loops to prevent blocking

**Edge Cases:**
- Shell command injection vulnerabilities
- Platform differences in shell syntax (bash vs cmd.exe)
- Environment variable encoding issues
- Application names with special characters
- Memory reporting on containerized/VM environments

**Type Coercion:**
- OS version strings may contain dots (can't convert to decimal)
- Memory values use `long` (may need 64-bit for modern systems)
- Application names are strings
- Shell command output includes newlines

**Security Considerations:**
- **CRITICAL:** Shell command verbs are security-sensitive!
- Must prevent command injection (e.g., "; rm -rf /")
- Consider input validation or escaping
- Environment variables could leak sensitive data
- Application management verbs could be used for privilege escalation

---

## UserTalk Documentation Notes

From docserver.userland.com/sys/:

**sys.osVersion()**
- Returns OS version as string (e.g., "7.0", "4.0.1381")
- **Important:** Version can have multiple decimal points - don't convert to number
- Example: macOS might return "14.1.2"

**sys.systemTask()**
- Yields control to other processes
- Returns true
- Used in long-running loops to prevent blocking
- Frontier is fully multithreaded, so not always necessary
- Related: clock.waitSeconds, clock.waitSixtieths, thread.sleepFor

**sys.unixShellCommand(cmd)**
- Executes string as if typed on command line
- Returns stdout as string
- Uses popen() internally
- Can chain commands with semicolon: "cd /etc;pwd"
- Can use pipes: "ls -al | grep inetd"
- Added in Frontier 7.0 (Mac OS X)
- **Security note:** Be careful with user-provided input!

**sys.os()**
- Returns OS name (e.g., "macOS", "Windows", "Linux")

**sys.machine()**
- Returns machine type/architecture

**sys.memAvail()**
- Returns available memory in bytes
- Useful for checking if enough memory for operations

**sys.getEnvironmentVariable(name)**
- Reads environment variable
- Returns empty string if not found

**sys.setEnvironmentVariable(name, value)**
- Sets environment variable
- Returns true on success
- Changes only affect current process

**Application Management Verbs (GUI-dependent):**
- `sys.appIsRunning(appname)` - Check if application is running
- `sys.frontmostApp()` - Get name of frontmost application
- `sys.bringAppToFront(appname)` - Bring application to front
- `sys.countApps()` - Count running applications
- `sys.getNthApp(n)` - Get name of nth running application (1-based)
- `sys.getAppPath(appname)` - Get full path to application
- `sys.browseNetwork()` - Show dialog to select network application

---

## Testing Requirements

**Minimum Test Cases Per Verb:**
- Platform-specific testing (macOS, Linux, Windows where applicable)
- Error handling (invalid inputs)
- Security testing (shell injection attempts)
- Edge cases (special characters, long strings)

**Test Scenarios:**
```usertalk
// sys.osVersion()
sys.osVersion()                      → "14.1.2" (macOS) or "10.0.22631" (Windows)

// sys.os()
sys.os()                             → "macOS" or "Linux" or "Windows"

// sys.machine()
sys.machine()                        → "arm64" or "x86_64" or "i386"

// sys.memAvail()
sys.memAvail()                       → large positive integer (bytes)

// sys.systemTask()
sys.systemTask()                     → true

// sys.getEnvironmentVariable()
sys.getEnvironmentVariable("PATH")   → "/usr/bin:/bin:..." (Unix)
sys.getEnvironmentVariable("NOTEXIST") → "" (empty string)

// sys.setEnvironmentVariable()
sys.setEnvironmentVariable("TEST_VAR", "value") → true
sys.getEnvironmentVariable("TEST_VAR") → "value"

// sys.unixShellCommand() - Unix only
sys.unixShellCommand("whoami")       → "username\n"
sys.unixShellCommand("echo hello")   → "hello\n"
sys.unixShellCommand("pwd")          → "/path/to/current\n"

// sys.winShellCommand() - Windows only
sys.winShellCommand("echo hello")    → "hello\r\n"

// GUI-dependent verbs (test in GUI mode, return errors in headless)
sys.appIsRunning("Finder")           → true/false (macOS) or error (headless)
sys.countApps()                      → number or error
sys.frontmostApp()                   → "AppName" or error
```

**Security Test Cases:**
```usertalk
// Shell command injection attempts (should be safely handled!)
sys.unixShellCommand("echo test; rm -rf /")  → Should NOT delete files
sys.unixShellCommand("$(malicious)")         → Should handle safely
sys.unixShellCommand("echo test && cat /etc/passwd") → Should validate
```

**Edge Case Tests:**
- Empty strings for all inputs
- Very long command strings
- Commands with special characters: `!@#$%^&*(){}[]<>?/\|`
- Unicode in application names
- Non-existent applications
- Memory reporting in containers/VMs

**Platform Differences:**
- Shell syntax varies (bash vs cmd.exe vs PowerShell)
- Application name formats differ by OS
- Memory reporting methods vary
- Environment variable conventions differ (PATH vs Path)
- Line endings (\n vs \r\n)

---

## Implementation Effort

**Estimated Time:** 8-12 hours

**Breakdown:**
- Implementation: 4-6 hours
  - OS info verbs: 1-2 hours
  - Environment variables: 1 hour
  - Shell commands: 1-2 hours (with security considerations)
  - Memory info: 1 hour
  - GUI verb stubs: 1 hour
- Testing: 3-4 hours (16 verbs, platform-specific)
- Security review: 1-2 hours (shell commands critical)
- Documentation: 1 hour

**Confidence:** MEDIUM - Platform abstraction adds complexity, security concerns

**Platform-Specific Work:**
- Abstract OS detection, version, memory APIs
- Implement shell command execution safely (popen/CreateProcess)
- Handle GUI verb differences (or return "not implemented" in headless)
- Test on macOS, Linux, Windows

---

## Priority & Sequencing

**Priority:** 🔧 **HIGH** (Tier 1 - Core Functionality subset)

**Recommended Implementation Order:** 16 (after date, before launch)

**Blockers/Prerequisites:** None

**Implementation Sequence:**
1. Implement headless-compatible verbs first (9 verbs):
   - sys.osVersion(), sys.os(), sys.machine()
   - sys.memAvail()
   - sys.systemTask() (simple yield/no-op)
   - sys.getEnvironmentVariable(), sys.setEnvironmentVariable()
   - sys.unixShellCommand() / sys.winShellCommand() (with security)
2. Stub out GUI-dependent verbs (7 verbs):
   - Return error message "not available in headless mode"
   - Document as GUI-only in implementation notes
3. Security review of shell command implementation
4. Platform-specific testing
5. Document platform differences and security considerations

**Phased Implementation:**
- **Phase 1 (Quick Win):** Headless-compatible verbs (9 verbs, ~4-6 hours)
- **Phase 2 (Optional):** GUI verb stubs (~1 hour)
- **Phase 3 (Future):** GUI verbs with actual implementation (if GUI mode added)

---

## Headless Compatibility Analysis

**Fully Compatible (9 verbs):**
✅ osversion - System API
✅ systemtask - Thread yield
✅ memavail - System API
✅ machine - System API
✅ os - System API
✅ getenvironmentvariable - POSIX standard
✅ setenvironmentvariable - POSIX standard
✅ unixshellcommand - popen (Unix/Linux/macOS)
✅ winshellcommand - _popen (Windows)

**Not Compatible (7 verbs):**
❌ browsenetwork - GUI dialog required
❌ appisrunning - Requires window manager / GUI
❌ frontmostapp - Requires window manager / GUI
❌ bringapptofront - Requires window manager / GUI
❌ countapps - Requires window manager / GUI
❌ getnthapp - Requires window manager / GUI
❌ getapppath - Requires window manager / GUI (could potentially work via process list, but not reliable)

**Recommendation:** Implement 9 headless-compatible verbs as Priority 1. GUI-dependent verbs can return "not implemented in headless mode" error.

---

## Related Processors

- **clock** - Time operations (clock.waitSeconds used with sys.systemTask)
- **thread** - Threading (thread.sleepFor alternative to sys.systemTask)
- **file** - File operations (could use shell commands, but file.* preferred)
- **launch** - Process launching (complementary to application management)

---

## Special Considerations

**Security - Shell Commands:**

**⚠️ CRITICAL SECURITY CONCERN:**
The shell command verbs (`sys.unixShellCommand` and `sys.winShellCommand`) are **extremely dangerous** if misused. They provide direct shell access and can:
- Execute arbitrary commands
- Delete files (rm -rf /)
- Access sensitive data
- Modify system configuration
- Launch malicious processes

**Security Recommendations:**
1. **Input Validation:** Sanitize all inputs
2. **Command Whitelisting:** Consider restricting to safe commands
3. **Escaping:** Properly escape special shell characters
4. **Documentation:** Warn users about security implications
5. **Audit Logging:** Log all shell command executions
6. **Sandboxing:** Run in restricted environment if possible

**Implementation Pattern:**
```c
// Example secure implementation approach
boolean unixshellcommand(bigstring cmd, bigstring result) {
    // 1. Validate input length
    if (stringlength(cmd) > MAX_COMMAND_LENGTH)
        return false;

    // 2. Check for dangerous patterns
    if (containsdangerous(cmd))  // e.g., ";", "&&", "|", etc.
        return false;

    // 3. Execute with popen
    FILE *fp = popen(cmd, "r");
    if (!fp) return false;

    // 4. Read output safely
    // ... read with size limits ...

    pclose(fp);
    return true;
}
```

**Platform Abstraction:**
- Create portable layer for OS detection
- Abstract memory APIs (different per platform)
- Handle shell command differences (bash vs cmd.exe)
- Environment variable encoding differences

**GUI Verb Handling:**
For headless mode, GUI-dependent verbs should:
1. Return `false` with clear error message
2. Document as "Not available in headless mode"
3. Consider future GUI mode support

**Threading & Yielding:**
- `sys.systemTask()` in modern OS may be no-op
- Consider `sched_yield()` on POSIX systems
- Or simple `sleep(0)` to yield thread quantum
- Document that Frontier headless uses real threads

**Memory Reporting:**
- Modern systems have complex memory hierarchy
- Report available memory (not total)
- Consider containers/VMs (may report host memory)
- 64-bit values needed for modern RAM sizes

**Cross-Platform Testing:**
- Test on macOS, Linux, Windows
- Test in containers (Docker)
- Test in VMs
- Test with different shells (bash, zsh, fish, cmd, PowerShell)

---

## References

**Documentation:**
- DocServer: `docs/usertalk/docserver.userland.com/sys/`
- Individual verb pages: `docs/usertalk/docserver.userland.com/sys/{verb}.html.tmp`

**Implementation:**
- Stub: `tests/headless_sys_verbs.c`
- Canonical verb list: `Common/resources/Win32/kernelverbs.rc` (line 799-818)

**Standards:**
- POSIX: getenv(), setenv(), popen()
- Windows: GetVersionEx(), GlobalMemoryStatusEx(), _popen()
- macOS: NSProcessInfo, vm_statistics64(), Cocoa APIs for app management

**Security References:**
- OWASP Command Injection: https://owasp.org/www-community/attacks/Command_Injection
- CWE-78: OS Command Injection
- Secure Coding: Shell command sanitization best practices

---

## Next Steps

1. ✅ Audit complete - ready for phased implementation
2. ⏳ Implement Phase 1: Headless-compatible verbs (9 verbs)
3. ⏳ Security review of shell command implementation
4. ⏳ Create platform abstraction layer
5. ⏳ Write unit tests for each platform
6. ⏳ Stub GUI-dependent verbs with "not implemented" errors
7. ⏳ Document security considerations and usage guidelines
8. ⏳ Test on macOS, Linux, Windows
9. ⏳ Update implementation status

---

**Audit Status:** ✅ Complete and Approved for Phased Implementation

**Implementation Strategy:** Implement 9 headless-compatible verbs as quick win, defer GUI-dependent verbs
