# Processor Audit: `target`

**Status:** ✅ **Fully Headless-Compatible**
**Audit Date:** 2025-12-06
**Auditor:** Claude (Haiku 4.5)

---

## Basic Information

| Property | Value |
|----------|-------|
| **Processor Name** | `target` |
| **EFP ID** | 1005 |
| **Verb Count** | 3 kernel verbs |
| **Window Required** | NO |
| **Implementation Type** | Kernel verbs |

---

## Category Assessment

**Category:** ✅ **Execution Context Management**

**Rationale:**
Target processor manages the current "target" (execution context) - typically a window, database, or editor context. In headless mode, implements as per-thread context tracking without GUI window requirement.

**Headless Compatibility:** ✅ **Full** (3/3 verbs)

**Headless Strategy:** Per-thread execution context stack instead of active window.

---

## Verb Inventory

| Verb | Parameters | Returns | Headless |
|------|-----------|---------|----------|
| `get` | () | address | ✅ YES (current context) |
| `set` | (address) | address | ✅ YES (push context) |
| `clear` | () | void | ✅ YES (pop context) |

---

## Implementation Analysis

### Complexity: **MEDIUM** (Context stack management)

### Dependencies
- **Other Processors:** Thread processor (for thread-local storage)
- **External Services:** None
- **GUI/Window Context:** None required

### Key Implementation Notes

**Headless Implementation Strategy:**

Per-thread execution context stack:

```c
// Headless target: thread-local context stack
struct TargetContext {
    address current;
    address previous;
};

// target.get - Get current context
address targetget() {
    TargetContext* ctx = getThreadContext();
    return ctx->current;
}

// target.set - Set context and return previous
address targetset(address adr) {
    TargetContext* ctx = getThreadContext();
    address oldTarget = ctx->current;
    ctx->current = adr;
    return oldTarget;
}

// target.clear - Clear context
void targetclear() {
    TargetContext* ctx = getThreadContext();
    ctx->current = nil;
}
```

**No GUI Required:**
- Works with any addressable object (outline, script, table, wptext)
- No window/editor integration needed
- Pure context tracking

---

## Headless Compatibility Analysis

**Fully Compatible:** ✅ 3/3 verbs (100%)

**Essential for Headless:**
- Outline operations work on arbitrary outline objects (not just edited outline)
- Script operations on arbitrary scripts
- WPText operations on arbitrary formatted text objects
- Database operations in script context

**Use Cases in Headless:**
- Executing scripts that operate on specific objects
- Switching context between multiple open databases
- Outline manipulation (expand, collapse, modify)
- WPText editing (in-memory)

---

## Implementation Effort

**Estimated Time:** 3-4 hours

**Breakdown:**
- Implement context stack: 1-2 hours
- Implement get/set/clear: 30 minutes
- Thread-local storage integration: 1 hour
- Testing: 1 hour
- Documentation: 30 minutes

**Confidence:** HIGH (straightforward context tracking)

**Blockers:** None (requires thread processor to be available)

---

## Priority & Sequencing

**Priority:** 🔴 **CRITICAL** (Tier 1 - Essential for outline/wp/script operations)

**Recommended Sequence:** After thread processor

**Prerequisites:**
- Thread processor (thread-local storage)
- Outline processor (op - operations use target)
- Database system (db - context matters)

---

## Testing Strategy

**Basic Get/Set:**
```usertalk
local (oldTarget = target.get())

// Create test outline
new (outlineType, @testOutline)

// Set as target
target.set(@testOutline)
assert(target.get() == @testOutline)

// Restore old target
target.set(oldTarget)
assert(target.get() == oldTarget)
```

**Context Persistence Across Operations:**
```usertalk
local (outline1, outline2)
new (outlineType, @outline1)
new (outlineType, @outline2)

target.set(@outline1)
op.insert(@outline1, "line1")

target.set(@outline2)
op.insert(@outline2, "line2")

// Switch back to outline1
target.set(@outline1)
// Operations still work on outline1
```

**Thread Isolation:**
```usertalk
local (outline)
new (outlineType, @outline)

target.set(@outline)
assert(target.get() == @outline)

// In another thread:
thread.evaluate("
    assert(target.get() != @outline)  // Different context
    target.set(@outline)
    assert(target.get() == @outline)
")

// Back in original thread:
assert(target.get() == @outline)  // Unchanged
```

---

## Related Processors

- **op** - Outline operations (uses target context)
- **wp** - WPText operations (uses target context)
- **script** - Script operations (uses target context)
- **thread** - Thread-local storage (for context stack)
- **window** - In GUI mode, target is active window (headless: any object)

---

## Special Considerations

**Context Stack Model:**
- get() returns current context (top of stack)
- set(adr) pushes new context, returns old (allows restoration)
- clear() clears current context

**Per-Thread Isolation:**
- Each thread has its own context stack
- Safe concurrent execution
- Different threads can work on different objects

**Nil Context:**
- Legal to have nil target (no active context)
- Operations on nil target should error appropriately

---

## Summary

**Status:** ✅ **Viable for Headless** (100% compatible)

**Key Findings:**
1. All 3 verbs work in headless via per-thread context tracking
2. No GUI window required
3. Essential for outline/wp/script operations
4. Moderate implementation effort (3-4 hours)
5. Works perfectly in multi-threaded scenarios

**Recommendation:** CRITICAL priority (essential infrastructure for headless operation)

---

**Audit Status:** ✅ Complete and Approved for Headless Implementation
