# Processor Audit: `globals`

**Status:** ✅ **Fully Headless-Compatible**
**Audit Date:** 2025-12-06
**Auditor:** Claude (Haiku 4.5)

---

## Basic Information

| Property | Value |
|----------|-------|
| **Processor Name** | `system.verbs.globals` |
| **Verb Count** | 41 verbs |
| **Window Required** | NO |
| **Implementation Type** | Script Verbs (UserTalk) |
| **Location** | `system.verbs.globals/` (41 `.ut` files) |

---

## Category Assessment

**Category:** ✅ **Type Constructors & Utilities**

**Rationale:**
Global verbs are type constructor wrappers and utility functions that provide the foundation for UserTalk data type manipulation. All are script-based implementations that wrap kernel verbs or provide pure utilities. No GUI dependencies; fully headless-compatible.

**Headless Compatibility:** ✅ **Full** (41/41 verbs)

**Blocking Verbs:** None

---

## Verb Inventory

### Type Constructors (31 verbs)

| Verb | Type | Description |
|------|------|-------------|
| `binary` | Constructor | Create binary object |
| `boolean` | Constructor | Create/coerce to boolean |
| `char` | Constructor | Create character value |
| `date` | Constructor | Create date value |
| `double` | Constructor | Create double-precision float |
| `enum` | Constructor | Create enumerated value |
| `filespec` | Constructor | Create file specification |
| `fixed` | Constructor | Create fixed-point number |
| `list` | Constructor | Create list value |
| `long` | Constructor | Create 32-bit integer |
| `number` | Constructor | Create generic number |
| `point` | Constructor | Create point (x, y) |
| `record` | Constructor | Create record value |
| `rect` | Constructor | Create rectangle (l, t, r, b) |
| `rgb` | Constructor | Create color (r, g, b) |
| `short` | Constructor | Create 16-bit integer |
| `single` | Constructor | Create single-precision float |
| `string` | Constructor | Create string value |
| `string4` | Constructor | Create 4-byte string |
| `direction` | Utility | Direction constants |
| `address` | Utility | Address/reference utility |
| `alias` | Utility | Alias reference utility |

### Utility Verbs (10 verbs)

| Verb | Description |
|------|-------------|
| `abs` | Absolute value |
| `mod` | Modulo operator |
| `random` | Random number generation |
| `callScript` | Call script by address |
| `evaluate` | Evaluate expression string |
| `edit` | Edit object (no-op in headless until GUI) |
| `delete` | Delete object |
| `new` | Create new object |
| `close` | Close resource |
| `msg` | Display message on stdout |

### Query/Metadata Verbs (5 verbs)

| Verb | Description |
|------|-------------|
| `getBinaryType` | Get binary object type |
| `setBinaryType` | Set binary object type |
| `displayString` | Get display representation |
| `timeCreated` | Get creation timestamp |
| `timeModified` | Get modification timestamp |

### Error Handling (2 verbs)

| Verb | Description |
|------|-------------|
| `scriptError` | Handle script error |
| `rollBeachBall` | Show busy cursor (no-op in headless) |

### Special (2 verbs)

| Verb | Description |
|------|-------------|
| `runSelection` | Run selected text (no-op in headless) |
| `memAvail` | Available memory (delegates to sys.memAvail) |

---

## Implementation Analysis

### Complexity: **LOW** (Wrapper Scripts)

### Dependencies

- **Other Processors:** kernel verbs, sys, date, lang
- **External Services:** None
- **GUI/Window Context:** Minimal (edit, msg, rollBeachBall can be stubbed)

### Key Implementation Notes

**Script-Based Wrappers:**
Each verb is a simple UserTalk script that:
1. Wraps a kernel verb or existing function
2. Provides convenient naming/syntax
3. Handles type coercion

Example: `abs.ut`
```usertalk
on abs (number) {
    return (kernel (lang.abs, number))
}
```

**Headless Considerations:**

1. **Type Constructors (100% compatible)**
   - `binary`, `boolean`, `char`, `date`, `double`, etc.
   - All wrap kernel verbs; no GUI dependencies

2. **Math Utilities (100% compatible)**
   - `abs`, `mod`, `random` - Pure computation

3. **Data Structure Constructors (100% compatible)**
   - `point`, `rect`, `rgb`, `record`, `list` - Data creation

4. **Headless-Compatible Message Verb (100% compatible)**
   - `msg` - Display message on stdout (write to console/log)

5. **No-op Verbs in Headless (4 verbs)**
   - `edit` - Open editor (no-op until GUI mode implemented)
   - `rollBeachBall` - Show busy cursor (no-op in headless)
   - `runSelection` - Run editor selection (no-op in headless)
   - No errors; just return immediately

6. **Script Execution (100% compatible)**
   - `callScript`, `evaluate` - Pure script evaluation
   - `delete`, `new`, `close` - Data object management

---

## Headless Compatibility Analysis

**Fully Compatible:** ✅ 41/41 verbs

**Headless-Compatible Categories:**
- ✅ All type constructors (31 verbs)
- ✅ Math utilities (3 verbs: abs, mod, random)
- ✅ Script execution (2 verbs: callScript, evaluate)
- ✅ Data object management (3 verbs: delete, new, close)
- ✅ Metadata queries (5 verbs: getBinaryType, setBinaryType, displayString, timeCreated, timeModified)
- ✅ Error handling (1 verb: scriptError)
- ✅ Message output (1 verb: msg - outputs to stdout/log)

**No-op Verbs in Headless (4 verbs - safe, non-breaking):**
- ⚠️ `edit` - No-op in headless (return immediately; no error)
- ⚠️ `rollBeachBall` - No-op in headless (return immediately; no error)
- ⚠️ `runSelection` - No-op in headless (return immediately; no error)
- Scripts calling these won't error; they'll just be silently skipped

---

## Implementation Strategy

**Phase 1 - All Verbs (Automatic + Simple Implementations)**
- All 41 verbs work in headless context
- Script wrappers work automatically once kernel verbs are implemented
- 4 GUI verbs implemented as no-ops (safe, non-breaking):
  - `edit()` - No-op (return immediately)
  - `msg()` - Write message to stdout/log file
  - `rollBeachBall()` - No-op (return immediately)
  - `runSelection()` - No-op (return immediately)

---

## Related Processors

- **lang** - Language runtime (type coercion, evaluation)
- **sys** - System operations (memAvail)
- **date** - Date operations
- **string** - String manipulation
- **All kernel processors** - Referenced by type constructors

---

## Summary

**Status:** ✅ **Fully Viable for Headless** (41/41 compatible)

**Key Findings:**
1. All verbs are UserTalk scripts (simple wrappers around kernel verbs)
2. Pure type constructors and utilities—no architectural complexity
3. All 41 verbs work in headless context (4 as no-ops, not errors)
4. Automatically headless-compatible once kernel verbs are implemented

**Recommendation:** Implement all 41 verbs. The 4 GUI verbs (edit, rollBeachBall, runSelection) are simple no-ops; msg writes to stdout/log instead of GUI dialog.

---

**Audit Status:** ✅ Complete and Approved for Headless Implementation
