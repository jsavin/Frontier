# Processor Audit: `clipboard`

**Status:** ✅ **Fully Headless-Compatible**
**Audit Date:** 2025-12-06
**Auditor:** Claude (Haiku 4.5)

---

## Basic Information

| Property | Value |
|----------|-------|
| **Processor Name** | `clipboard` |
| **EFP ID** | 1015 |
| **Verb Count** | 4 kernel verbs |
| **Window Required** | NO |
| **Implementation Type** | Kernel verbs |

---

## Category Assessment

**Category:** ✅ **Text Data Exchange (Headless-Compatible via In-Memory Buffer)**

**Rationale:**
Clipboard processor manages text data exchange. In a headless environment, implements as in-memory text buffer rather than OS clipboard. Provides same interface without GUI dependency.

**Headless Compatibility:** ✅ **Full** (4/4 verbs)

**Headless Strategy:** Replace OS clipboard with in-memory buffer per thread/execution context.

---

## Verb Inventory

| Verb       | Parameters                   | Returns | Headless                 |
| ---------- | ---------------------------- | ------- | ------------------------ |
| `get`      | (string4 type, address addr) | boolean | ✅ YES (in-memory buffer) |
| `getValue` | (string4 type)               | any     | ✅ YES (in-memory buffer) |
| `put`      | (string4 type, address addr) | boolean | ✅ YES (in-memory buffer) |
| `putValue` | (any value)                  | boolean | ✅ YES (in-memory buffer) |

---

## Implementation Analysis

### Complexity: **TRIVIAL** (Simple text buffer management)

### Dependencies
- **Other Processors:** None
- **External Services:** None (OS clipboard not required in headless)
- **GUI/Window Context:** None (can be stubbed)

### Key Implementation Notes

**Headless Implementation Strategy:**

In headless mode, implement clipboard as per-thread in-memory buffer supporting typed data:

```c
// Headless clipboard: thread-local storage
struct HeadlessClipboard {
    binary content;          // Binary data in clipboard
    string4 type;            // Resource type (e.g., 'TEXT', 'PICT')
    time_t lastModified;
};

// clipboard.get - Retrieve clipboard content as binary with type
// Returns true if clipboard has data of specified type, false if empty or type mismatch
boolean clipboardget(string type, address addr) {
    HeadlessClipboard* clip = getThreadClipboard();
    if (clip->content == NULL || clip->type != type) {
        return false;
    }
    addr^ = clip->content;   // Assign binary value to address
    return true;
}

// clipboard.getValue - Retrieve clipboard content as interpreted value
// Attempts to unpack binary data as the corresponding Frontier datatype
any clipboardgetvalue(string type) {
    HeadlessClipboard* clip = getThreadClipboard();
    if (clip->content == NULL || clip->type != type) {
        return NULL;
    }
    return unpackBinary(clip->content, type);  // Convert binary to appropriate type
}

// clipboard.put - Set clipboard content from binary at address
// Creates item of type type from data at addr and replaces clipboard contents
boolean clipboardput(string type, address addr) {
    HeadlessClipboard* clip = getThreadClipboard();
    clip->content = addr^;          // Get binary from address
    clip->type = type;
    clip->lastModified = time(NULL);
    return true;
}

// clipboard.putValue - Set clipboard content from any value
// Places value into clipboard, sets type appropriately (type of value or binary type)
boolean clipboardputvalue(any value) {
    HeadlessClipboard* clip = getThreadClipboard();
    if (getBinaryType(value) != NULL) {
        clip->type = getBinaryType(value);
        clip->content = value;
    } else {
        clip->type = typeOf(value);  // e.g., 'TEXT' for string
        clip->content = packBinary(value, typeOf(value));
    }
    clip->lastModified = time(NULL);
    return true;
}
```

**Key Design Notes:**
- **Type-aware storage**: Each clipboard operation specifies a resource type (string4) like 'TEXT', 'PICT'
- **Binary vs. Value variants**: `get`/`put` work with binary data at addresses; `getValue`/`putValue` work with interpreted values
- **Thread isolation**: Each thread has isolated clipboard (safe for multi-threaded operation)
- **No OS dependency**: In-memory implementation requires no OS clipboard access
- **Automatic type conversion**: `getValue`/`putValue` handle type conversion automatically

---

## Headless Compatibility Analysis

**Fully Compatible:** ✅ 4/4 verbs (100%)

**Verb Breakdown:**
- **`get` (binary)**: Retrieves clipboard as binary data - headless compatible (in-memory storage)
- **`getValue` (typed)**: Retrieves clipboard as interpreted value - headless compatible (unpacks binary)
- **`put` (binary)**: Sets clipboard from binary at address - headless compatible (in-memory storage)
- **`putValue` (typed)**: Sets clipboard from any value - headless compatible (auto type detection)

**Use Cases in Headless:**
- Text exchange via clipboard (TEXT type)
- Binary data exchange (PICT, AIFF, etc.)
- Script-to-script data passing within threads
- Simulating copy/paste operations in tests
- Multi-format clipboard operations

**Advantages in Headless:**
- No OS clipboard dependencies
- Each thread has isolated clipboard (thread-safe)
- Supports both binary and typed operations
- Automatic type conversion via getValue/putValue variants

---

## Implementation Effort

**Estimated Time:** 2-3 hours

**Breakdown:**
- Implement thread-local storage: 1 hour
- Implement get: 15 minutes
- Implement put: 15 minutes
- Testing: 30 minutes
- Documentation: 30 minutes

**Confidence:** VERY HIGH (simple in-memory buffer)

**Blockers:** None (thread-local storage must be available)

---

## Priority & Sequencing

**Priority:** 🟡 **MEDIUM** (Tier 2 - Useful for scripting, not critical)

**Recommended Sequence:** After thread processor (needs thread-local storage)

**Prerequisites:**
- Thread processor (for thread-local storage management)
- String type support

---

## Testing Strategy

**Basic Binary Operations (get/put):**
```usertalk
// Put TEXT data
local (s = "Hello, World!")
clipboard.put('TEXT', @s)
assert(clipboard.get('TEXT', @result) == true)
assert(result == "Hello, World!")

// Type mismatch returns false
assert(clipboard.get('PICT', @result) == false)

// Put binary PICT data
local (binaryData = newBinary('PICT'))
clipboard.put('PICT', @binaryData)
assert(clipboard.get('PICT', @result) == true)
```

**Value Operations (getValue/putValue):**
```usertalk
// putValue with string (auto-detects type as TEXT)
clipboard.putValue("Sample text")
assert(clipboard.getValue('TEXT') == "Sample text")

// putValue with number (auto-detects type)
clipboard.putValue(42)
// getValue unpacks it back as a number
assert(clipboard.getValue(typeOf(42)) == 42)

// putValue with binary value
local (binaryData = newBinary('PICT'))
clipboard.putValue(binaryData)
assert(clipboard.getValue('PICT') == binaryData)
```

**Thread Isolation:**
```usertalk
// Thread 1: set TEXT
clipboard.putValue("Thread 1 data")
assert(clipboard.getValue('TEXT') == "Thread 1 data")

// Thread 2: set different data
thread.evaluate("
    clipboard.putValue('Thread 2 data')
    assert(clipboard.getValue('TEXT') == 'Thread 2 data')
")

// Back in thread 1: should be unchanged
assert(clipboard.getValue('TEXT') == "Thread 1 data")
```

**Edge Cases:**
- Empty clipboard returns false from get/NULL from getValue
- Type parameter ('TEXT', 'PICT', etc.) determines storage type
- putValue automatically detects and stores appropriate type

---

## Related Processors

- **thread** - Thread-local storage management
- **string** - String manipulation
- **editmenu** - Cut/copy/paste operations (may use clipboard)

---

## Special Considerations

**Thread Isolation:**
- Each thread has its own clipboard
- Allows safe concurrent use
- Different from OS clipboard (expected in headless)

**Large Content:**
- No practical limits on clipboard size (unlike OS clipboard)
- Can hold multi-megabyte strings

**Persistence:**
- Clipboard clears when thread ends
- Not persisted between program runs (expected)

---

## Summary

**Status:** ✅ **Viable for Headless** (100% compatible)

**Key Findings:**
1. All four verbs are trivial typed buffer operations
2. Two binary variants (get/put) for raw data at addresses
3. Two value variants (getValue/putValue) for typed automatic conversions
4. No GUI or OS clipboard required
5. Thread-local per-thread clipboard provides thread safety
6. Simple implementation effort (2-3 hours)

**Recommendation:** MEDIUM priority (useful utility for clipboard-dependent code, quick win with simple implementation)

---

**Audit Status:** ✅ Complete and Approved for Headless Implementation
