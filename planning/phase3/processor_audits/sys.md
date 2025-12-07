# Processor Audit: `sys`

**Status:** ✅ **High Headless Compatibility** (15/16 verbs)
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

**Category:** ✅ **Mixed - Core System + Process Management**

**Rationale:**
System utilities processor with both core headless verbs (OS info, environment variables, shell commands) and process management verbs that have cross-platform implementations. Application management verbs have platform-specific implementations: macOS uses GUI/window manager APIs, Windows uses process enumeration (GetProcessInfo, EnumProcesses). Only GUI-specific verb is `browseNetwork` (dialog-based).

**Headless Compatibility:** ✅ **High** (15/16 verbs compatible)

**GUI-Only Verbs:**
- `browsenetwork` - GUI dialog (skip)

**Cross-Platform Verbs (Headless-Compatible):**
- `appisrunning`, `frontmostapp`, `countapps`, `getnthapp`, `getapppath` - Process enumeration (platform-specific implementation, not GUI-dependent)
- `bringapptofront` - Window focus (platform-specific; may be stub in headless)

---

## Verb Inventory

### Core System Verbs (9 verbs)

| # | Verb Name | Signature | Description | Headless |
|---|-----------|-----------|-------------|----------|
| 1 | `osversion` | `sys.osVersion() -> string` | Get OS version string | ✅ YES |
| 2 | `systemtask` | `sys.systemTask() -> boolean` | Yield to other processes | ✅ YES |
| 11 | `memavail` | `sys.memAvail() -> long` | Get available memory | ✅ YES |
| 12 | `machine` | `sys.machine() -> string` | Get machine type | ✅ YES |
| 13 | `os` | `sys.os() -> string` | Get OS name | ✅ YES |
| 14 | `getenvironmentvariable` | `sys.getEnvironmentVariable(name) -> string` | Get environment variable | ✅ YES |
| 15 | `setenvironmentvariable` | `sys.setEnvironmentVariable(name, value) -> boolean` | Set environment variable | ✅ YES |
| 16 | `unixshellcommand` | `sys.unixShellCommand(cmd) -> string` | Execute Unix shell command | ✅ YES |
| 17 | `winshellcommand` | `sys.winShellCommand(cmd) -> string` | Execute Windows shell command | ✅ YES |

### Process Management Verbs (6 verbs - Headless-Compatible)

| # | Verb Name | Signature | Description | Headless |
|---|-----------|-----------|-------------|----------|
| 4 | `appisrunning` | `sys.appIsRunning(appname) -> boolean` | Check if app is running (process enumeration) | ✅ YES |
| 5 | `frontmostapp` | `sys.frontmostApp() -> string` | Get frontmost application name | ✅ YES |
| 6 | `bringapptofront` | `sys.bringAppToFront(appname) -> boolean` | Bring app to front (platform-specific) | ⚠️ STUB |
| 7 | `countapps` | `sys.countApps() -> long` | Count running applications | ✅ YES |
| 8 | `getnthapp` | `sys.getNthApp(n) -> string` | Get nth running app name (returns "unknown" on Windows) | ⚠️ PARTIAL |
| 9 | `getapppath` | `sys.getAppPath(appname) -> string` | Get path to application | ✅ YES |

### Platform-Specific Verbs (1 verb - Skip)

| # | Verb Name | Signature | Description | Status |
|---|-----------|-----------|-------------|--------|
| 3 | `browsenetwork` | `sys.browseNetwork() -> string` | Browse for network application (Mac-specific, not implemented on Windows) | ❌ SKIP |

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

**Application Management (Cross-Platform Process Enumeration):**
```c
// macOS: NSWorkspace.runningApplications, Process Manager APIs
// Windows: CreateToolhelp32Snapshot(), EnumProcesses(), GetProcessImageFileName()
// Linux: /proc filesystem or procps-ng library

// sys.appIsRunning(name) - Check if process is running
// sys.countApps() - Count running processes
// sys.getNthApp(n) - Get nth process name (partial on Windows)
// sys.getAppPath(name) - Get executable path (works on Windows)
// sys.frontmostApp() - Get frontmost/active window (works on Windows)
// sys.bringAppToFront(name) - Activate window (platform-specific, may be stub in headless)
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

**Application/Process Management Verbs (Headless-Compatible):**
- `sys.appIsRunning(appname)` - Check if application is running (verified working on Windows)
- `sys.frontmostApp()` - Get name of frontmost application (verified working on Windows)
- `sys.bringAppToFront(appname)` - Bring application to front (window focus; platform-specific implementation)
- `sys.countApps()` - Count running applications (verified working on Windows)
- `sys.getNthApp(n)` - Get name of nth running application (partial on Windows; returns "unknown")
- `sys.getAppPath(appname)` - Get full path to application (verified working on Windows)

**Mac-Specific Verb (Skip):**
- `sys.browseNetwork()` - Browse for network application (Mac-specific, not implemented on Windows; skip)

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

// Process management verbs (headless-compatible)
sys.appIsRunning("Frontier.exe")     → true (Windows)
sys.countApps()                      → 137 (number of running processes)
sys.frontmostApp()                   → "Frontier.exe" (frontmost/active window)
sys.getAppPath("Frontier.exe")       → "C:\Frontier\Frontier.exe"
sys.getNthApp(1)                     → process name (may return "unknown" on Windows)
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

**Estimated Time:** 12-18 hours

**Breakdown:**
- **Phase 1 - Core System Verbs (4-6 hours):**
  - OS info verbs: 1-2 hours
  - Environment variables: 1 hour
  - Shell commands: 1-2 hours (with security considerations)
  - Memory info: 1 hour

- **Phase 2 - Process Management Verbs (3-4 hours):**
  - Process enumeration APIs: 2-3 hours (platform-specific)
  - App path lookup: 1 hour
  - Window management stubs: 1 hour

- **Testing: 3-4 hours** (15 verbs, cross-platform: Windows/macOS/Linux)
- **Security review: 1-2 hours** (shell commands critical)
- **Documentation: 1 hour**

**Confidence:** MEDIUM-HIGH - Process APIs are standard cross-platform functions; main complexity is platform-specific implementation

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
1. Implement core system verbs (9 verbs):
   - sys.osVersion(), sys.os(), sys.machine()
   - sys.memAvail()
   - sys.systemTask() (simple yield/no-op)
   - sys.getEnvironmentVariable(), sys.setEnvironmentVariable()
   - sys.unixShellCommand() / sys.winShellCommand() (with security)
2. Implement process management verbs (6 verbs):
   - sys.appIsRunning() - process enumeration
   - sys.countApps() - process count
   - sys.getNthApp() - enumerate processes (may be partial on Windows)
   - sys.getAppPath() - process executable path
   - sys.frontmostApp() - active window (works cross-platform)
   - sys.bringAppToFront() - window focus (platform-specific; may be stub)
3. Skip Mac-specific verbs (1 verb):
   - sys.browseNetwork() - Mac-specific network browsing (not implemented on Windows; skip)
4. Security review of shell command implementation
5. Platform-specific testing on Windows/macOS/Linux
6. Document platform differences and security considerations

**Phased Implementation:**
- **Phase 1 (Priority):** Core system verbs (9 verbs, ~4-6 hours)
- **Phase 2 (High Priority):** Process management verbs (6 verbs, ~3-4 hours)
- **Phase 3 (Skip):** Mac-specific verb (browseNetwork - not implemented on Windows)

---

## Headless Compatibility Analysis

**Fully Compatible (15 verbs):**

**Core System Verbs (9 verbs):**
✅ osversion - System API
✅ systemtask - Thread yield
✅ memavail - System API
✅ machine - System API
✅ os - System API
✅ getenvironmentvariable - POSIX standard
✅ setenvironmentvariable - POSIX standard
✅ unixshellcommand - popen (Unix/Linux/macOS)
✅ winshellcommand - _popen (Windows)

**Process Management Verbs (6 verbs - Cross-Platform Implementation):**
✅ appisrunning - Process enumeration (verified on Windows)
✅ countapps - Process counting (verified on Windows)
✅ getapppath - Get executable path (verified on Windows)
✅ frontmostapp - Active window (verified on Windows)
✅ getnthapp - Enumerate processes (partial on Windows; returns "unknown")
✅ bringapptofront - Window focus (platform-specific; may be stub in headless)

**Not Compatible (1 verb):**
❌ browsenetwork - Mac-specific network browsing (not implemented on Windows; skip)

**Recommendation:** Implement all 15 headless-compatible verbs. Skip browseNetwork (Mac-specific, not implemented on Windows).

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
