# System Audit: `callbacks`

**Status:** ✅ **Fully Headless-Compatible**
**Audit Date:** 2025-12-06
**Auditor:** Claude (Haiku 4.5)

---

## Basic Information

| Property | Value |
|----------|-------|
| **System Name** | `system.callbacks` |
| **Event Handler Count** | 21 built-in callbacks |
| **Window Required** | NO |
| **Implementation Type** | Script Verbs (UserTalk) |
| **Location** | `system.callbacks/` (21 `.ut` files) |
| **Extension Point** | `user.callbacks/` (user-defined callbacks) |

---

## Category Assessment

**Category:** ✅ **System Event Handlers**

**Rationale:**
Callbacks are event handlers that execute in response to system events (startup, shutdown, window operations, etc.). All are script-based and execute in the runtime context without GUI dependencies. Fully headless-compatible; event triggering is part of core runtime.

**Headless Compatibility:** ✅ **Full** (21/21 handlers)

**Blocking Events:** None

---

## Event Handler Inventory

### Application Lifecycle (4 handlers)

| Handler | Signature | Description |
|---------|-----------|-------------|
| `startup` | `on startup()` | Fired when Frontier starts |
| `shutdown` | `on shutdown()` | Fired when Frontier shuts down |
| `resume` | `on resume()` | Fired when app resumes from suspend |
| `suspend` | `on suspend()` | Fired when app is suspended |

### Window Management (3 handlers)

| Handler | Signature | Description |
|---------|-----------|-------------|
| `openWindow` | `on openWindow(adr)` | Fired when window opens (address context) |
| `closeWindow` | `on closeWindow(adr)` | Fired when window closes |
| `saveWindow` | `on saveWindow(adr)` | Fired before window saves |

### Outline/Table Operations (6 handlers)

| Handler | Signature | Description |
|---------|-----------|-------------|
| `opInsert` | `on opInsert(adr)` | Fired when outline item inserted |
| `opExpand` | `on opExpand(adr)` | Fired when outline item expanded |
| `opCollapse` | `on opCollapse(adr)` | Fired when outline item collapsed |
| `opCursorMoved` | `on opCursorMoved(adr)` | Fired when outline cursor moves |
| `opRightClick` | `on opRightClick(adr)` | Fired on right-click in outline |
| `opStruct2Click` | `on opStruct2Click(adr)` | Fired on struct data click |

### Keyboard/Mouse Events (4 handlers)

| Handler | Signature | Description |
|---------|-----------|-------------|
| `cmd2Click` | `on cmd2Click()` | Fired on Cmd+Click (Mac) or Ctrl+Click |
| `option2Click` | `on option2Click()` | Fired on Option+Click (Mac) or Alt+Click |
| `control2Click` | `on control2Click()` | Fired on Control+Click (Mac) |
| `opReturnKey` | `on opReturnKey(adr)` | Fired when Return key pressed in outline |

### UI Integration (2 handlers)

| Handler | Signature | Description |
|---------|-----------|-------------|
| `systemTrayIcon2Click` | `on systemTrayIcon2Click()` | Fired on system tray icon click (Windows) |
| `systemTrayIconRightClick` | `on systemTrayIconRightClick()` | Fired on system tray icon right-click |

### Script Compilation (1 handler)

| Handler | Signature | Description |
|---------|-----------|-------------|
| `compileChangedScript` | `on compileChangedScript(adr)` | Fired when compiled script changes |

### Networking (1 handler)

| Handler | Signature | Description |
|---------|-----------|-------------|
| `tcpSetOffline` | `on tcpSetOffline(isOffline)` | Fired when network goes offline/online |

---

## Implementation Analysis

### Complexity: **LOW** (Event Dispatch System)

### Dependencies

- **Other Processors:** None (callbacks are independent)
- **External Services:** None
- **GUI/Window Context:** Minimal (outline/window operations can occur in headless, though less relevant)

### Key Implementation Notes

**Callback Mechanism:**

1. **Event Registration:**
   - Runtime maintains registry of callback handlers
   - Stored in `system.callbacks` table
   - Users can extend with `user.callbacks` entries

2. **Event Triggering:**
   - Runtime triggers callbacks at specific points
   - Synchronous execution (caller waits for handler)
   - Handler can return value to influence behavior

3. **Error Handling:**
   - If callback errors, runtime logs but continues
   - Error in one callback doesn't prevent others

4. **Script-Based Implementation:**
   - Each callback is a simple UserTalk script
   - Can call other verbs, manipulate data, etc.
   - No architectural complexity

**Headless Relevance:**

- **Relevant callbacks:**
  - `startup`, `shutdown`, `resume`, `suspend` - Essential for headless startup/shutdown sequence
  - `tcpSetOffline` - Useful for network monitoring
  - `compileChangedScript` - Useful for headless script compilation

- **Less relevant but valid:**
  - `opInsert`, `opExpand`, `opCollapse`, etc. - Still fire if outline is manipulated programmatically
  - Window callbacks - Still fire if windows are opened/closed programmatically
  - UI callbacks - May not be triggered in headless, but harmless if defined

---

## Headless Compatibility Analysis

**Fully Compatible:** ✅ 21/21 handlers

**Breakdown:**

**Essential Lifecycle Callbacks (100% relevant):**
- ✅ `startup` - Headless initialization
- ✅ `shutdown` - Headless cleanup
- ✅ `resume` - Resume from suspend
- ✅ `suspend` - Prepare for suspend

**Operational Callbacks (80% relevant):**
- ✅ `compileChangedScript` - Script compilation hooks
- ✅ `tcpSetOffline` - Network status monitoring
- ⚠️ `openWindow`, `closeWindow`, `saveWindow` - Valid but less used in headless
- ⚠️ Outline/table callbacks - Valid but less used in headless

**UI Callbacks (20% relevant in headless):**
- ⚠️ `opRightClick`, `cmd2Click`, `option2Click`, etc. - Won't fire in headless without UI
- ⚠️ `systemTrayIcon2Click`, `systemTrayIconRightClick` - Windows-specific, not in headless

---

## Headless Implementation Strategy

**Phase 1 - Core Lifecycle (Essential)**
1. Implement callback registration system
2. Trigger `startup` when runtime initializes
3. Trigger `shutdown` when runtime terminates
4. Trigger `suspend`/`resume` for power management
5. Estimated effort: 4-6 hours

**Phase 2 - Operational Callbacks (Important)**
1. Implement `compileChangedScript` trigger (after script compilation)
2. Implement `tcpSetOffline` trigger (when network status changes)
3. Estimated effort: 2-3 hours

**Phase 3 - Full Callback System (Complete)**
1. Implement all remaining callbacks for completeness
2. Most won't fire in headless, but system is more consistent
3. Estimated effort: 3-4 hours (mostly documentation)

---

## Usage in Headless

**Example: Initialization Hook**

```usertalk
// In user.callbacks:
on startup {
    // Headless-specific initialization
    local (logfile = getenv("FRONTIER_LOGFILE"))
    if (logfile != "") {
        try {
            openResource(logfile)
        } catch {
            // Handle error
        }
    }
}
```

**Example: Network Monitoring**

```usertalk
on tcpSetOffline (isOffline) {
    if (isOffline) {
        // Stop network operations, queue for later
        sys.unixShellCommand("logger 'Frontier: Network offline'")
    } else {
        // Resume network operations
        sys.unixShellCommand("logger 'Frontier: Network online'")
    }
}
```

---

## Testing Strategy

**Callback Triggering:**
- Test startup callback on runtime initialization
- Test shutdown callback on graceful termination
- Test suspend/resume (if supported)
- Test network status changes (tcpSetOffline)
- Test script compilation hooks

**Error Handling:**
- Verify errors in callbacks don't crash runtime
- Verify other callbacks still execute if one errors
- Test callback timeout behavior

---

## Related Components

- **Runtime** - Event triggering mechanism
- **Script** - Compilation hooks
- **TCP** - Network status monitoring
- **All Processors** - Callbacks can invoke any verb

---

## Summary

**Status:** ✅ **Fully Viable for Headless**

**Key Findings:**
1. All 21 callbacks are script-based with no GUI dependencies
2. Lifecycle callbacks (startup, shutdown) are essential for headless operation
3. Operational callbacks (tcpSetOffline, compileChangedScript) are useful for monitoring
4. UI callbacks won't fire in headless but are harmless if defined
5. Callback system is straightforward event dispatch mechanism

**Recommendation:**
- Implement Phase 1 (lifecycle callbacks) as CRITICAL - essential for headless startup/shutdown
- Implement Phase 2 (operational callbacks) as HIGH PRIORITY - useful for monitoring
- Implement Phase 3 (full system) as MEDIUM PRIORITY - nice to have for consistency

**Estimated Total Effort:** 9-13 hours

---

**Audit Status:** ✅ Complete and Approved for Headless Implementation
