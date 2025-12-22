# Headless Window Operations Implementation Plan

## Status
- State: Planning Phase
- Phase: 3
- Last Updated: 2025-12-21
- Notes: Design for selective window verb implementations needed for ODB object operations

**Created**: 2025-12-21
**Status**: DRAFT - Based on user requirements
**Context**: Window verbs need selective headless implementations for ODB object operations

---

## Executive Summary

**Finding**: Window verbs aren't purely display operations. Several must work with ODB objects in headless mode.

**Required Implementations** (14 verbs):
1. **open/close** - Set/clear current target
2. **getTitle** - Return name of ODB object
3. **getFile** - Return file associated with database
4. **isReadOnly, isModified, setModified** - Operate on ODB objects
5. **msg** - Output to stdout
6. **dbStats, quickTime** - Return errors (not supported)
7. **isMenuScript** - Detect menu scripts
8. **runSelection** - Edge case, defer for now

**Noop Implementations** (13 verbs):
- Navigation/Focus: about, bringToFront, frontmost, isFront, next, sendToBack
- Visibility: hide, show
- Position/Size: getPosition, setPosition, getSize, setSize, zoom
- Window Properties: setTitle, isOpen
- Content: scroll, update, quickScript

**No Implementation Needed**:
- Glue scripts: getType, isHidden, visit (stay as-is, use kernel verbs)

---

## Part 1: Required Implementations

### Category A: Target Management

#### `open(adr)` - Open and set target
```usertalk
// Current stub: return false
// Headless requirement: target.set(adr); return true
```

**Implementation**:
- Extract external object at address
- Call `target.set(adr)` to make it current
- Return true

**Effort**: 0.5 days

---

#### `close()` - Close and clear target
```usertalk
// Current stub: return false
// Headless requirement: target.clear(); return true
```

**Implementation**:
- Call `target.clear()`
- Return true

**Effort**: 0.5 days

---

### Category B: ODB Object Properties

#### `getTitle(adr)` - Get object name at address
```usertalk
// Current stub: return "not implemented"
// Headless requirement: return name of object
```

**Implementation**:
- Dereference address to get object
- Extract object name (using nameOf equivalent in C)
- Return as string

**Effort**: 1 day

---

#### `getFile(adr)` - Get file for database
```usertalk
// Current stub: return "not implemented"
// Headless requirement: return file path
```

**Implementation**:
- Given address, determine which database it belongs to
- Return the file path of that database
- Critical for persistence operations

**Effort**: 1-2 days

**Note**: User says "getFile absolutely has to work!" - high priority

---

#### `isReadOnly(adr)` - Check if ODB object is read-only
```usertalk
// Current stub: return false
// Headless requirement: check ODB object flags
```

**Implementation**:
- Dereference address to get object
- Check read-only flag (tableType, outlineType, scriptType all have this)
- Return boolean

**Effort**: 1 day

---

#### `isModified(adr)` - Check if ODB object is modified
```usertalk
// Current stub: return false
// Headless requirement: check ODB object flags
```

**Implementation**:
- Dereference address to get object
- Check modified/dirty flag
- Return boolean

**Effort**: 1 day

---

#### `setModified(adr, flag)` - Set modified flag on ODB object
```usertalk
// Current stub: return false
// Headless requirement: set dirty flag
```

**Implementation**:
- Dereference address to get object
- Set modified/dirty flag
- Return true

**Effort**: 1 day

---

### Category C: Output/Errors

#### `msg(text)` - Display message
```usertalk
// Current stub: return false
// Headless requirement: output to stdout
```

**Implementation**:
- Write text to stdout (or logging system)
- Return true

**Effort**: 0.5 days

**Example**: `window.msg("Processing file.txt")` → prints to console

---

#### `dbStats()` - Show database statistics
```usertalk
// Current stub: return false
// Headless requirement: return error
```

**Implementation**:
- Return error: "Can't display database statistics because GUI is not available in headless mode"
- Return false

**Effort**: 0.5 days

---

#### `quickTime()` - QuickTime support check
```usertalk
// Current stub: return false
// Headless requirement: return error
```

**Implementation**:
- Return error: "QuickTime is not supported on this platform"
- Return false

**Effort**: 0.5 days

---

### Category D: Special Cases

#### `isMenuScript(adr)` - Detect menu scripts
```usertalk
// Current stub: return false
// Headless requirement: return true if address points to menu script
```

**Implementation**:
- Dereference address
- Check if it's an idmenuprocessor external variable
- Return boolean

**Effort**: 1 day

**Context**: User notes: "there's no direct way to target a menu script, so you have to turn off display, open the menu script, and then use the op verbs to modify or inspect the script"

---

#### `runSelection()` - Run selected text as script
```usertalk
// Current stub: return false
// Headless requirement: defer or noop
```

**User assessment**: "edge-case. I doubt any purely headless code will run it"

**Recommendation**: Noop for now (return false)

**Effort**: 0 days (defer)

---

## Part 2: Noop Implementations (No Effort)

These can all be stubs that return silently:

**Navigation/Focus** (6 verbs):
- about, bringToFront, frontmost, isFront, next, sendToBack
- Return: false or empty

**Visibility** (2 verbs):
- hide, show
- Return: true (pretend they worked)

**Position/Size** (5 verbs):
- getPosition (return empty/default), setPosition, getSize, setSize, zoom
- Return: true or empty

**Window Properties** (2 verbs):
- setTitle (noop), isOpen (return false)
- Return: appropriate default

**Content Operations** (2 verbs):
- scroll, update
- Return: true

**GUI Only** (1 verb):
- quickScript
- Return: false

---

## Part 3: Implementation Dependencies

### Required Existing Functions

Check if these already exist in headless build:

**For getTitle/isReadOnly/isModified/setModified**:
- `langexternalgetdatavalue()` - Get external variable from address ✓ (likely exists)
- `nameOf()` equivalent - Get object name (need to check)
- External variable property accessors (read-only, modified flags)

**For getFile**:
- Database path lookup from address
- `dbopenfile()` or equivalent to get file path ✓ (exists)

**For isMenuScript**:
- Check external variable ID: `(**hv).id == idmenuprocessor`

**For msg**:
- Logging system or stdout writer

---

## Part 4: Implementation Plan Phases

### Phase 1: Core Target Management (1 day)
- [ ] Implement `open()` - call target.set()
- [ ] Implement `close()` - call target.clear()
- [ ] Test with simple scripts using target operations

**Deliverable**: window.open/close manage current target

---

### Phase 2: ODB Object Properties (3-4 days)
- [ ] Implement `getTitle()` - extract object name
- [ ] Implement `isReadOnly()` - check ODB flags
- [ ] Implement `isModified()` - check dirty flag
- [ ] Implement `setModified()` - set dirty flag
- [ ] Test with table/outline/script objects

**Deliverable**: window properties work with ODB objects

---

### Phase 3: Database File Access (1-2 days)
- [ ] Implement `getFile()` - return database file path
- [ ] Test with objects from different databases

**Deliverable**: getFile works for persistence operations

---

### Phase 4: Special Cases & Output (1-2 days)
- [ ] Implement `msg()` - output to stdout
- [ ] Implement `isMenuScript()` - detect menu scripts
- [ ] Implement error returns for `dbStats()`, `quickTime()`
- [ ] Test menu script detection

**Deliverable**: Special verbs work correctly

---

### Phase 5: Testing & Documentation (1 day)
- [ ] Create unit tests for all implemented verbs
- [ ] Integration tests with UserTalk scripts
- [ ] Document behavior differences from GUI mode
- [ ] Update CONTRIBUTING.md

**Deliverable**: All tests passing, documentation complete

---

**Total Estimated Effort**: 7-10 days

---

## Part 5: Risk Assessment

| Implementation | Risk | Notes |
|---|---|---|
| open/close (target.set/clear) | LOW | Already have target verbs working |
| getTitle | LOW | Straightforward object name lookup |
| getFile | MEDIUM | Need database file path lookup |
| isReadOnly/isModified/setModified | MEDIUM | Need to understand ODB object flag storage |
| msg | LOW | Simple stdout/logging |
| isMenuScript | MEDIUM | Need external variable type checking |
| dbStats/quickTime errors | LOW | Just return appropriate errors |

---

## Part 6: Updated Dead Code Removal Strategy

### Phase Structure

**Phase 1: Dead Code Explicit Markers** (1 week)
- Remove xxx-prefixed blocks
- Remove OBSOLETE, NEVER markers
- ✅ NO change needed

**Phase 2: Obsolete Platforms** (1 week)
- Remove oldMACVERSION blocks
- Remove commented WIN95VERSION blocks
- ✅ NO change needed

**Phase 3: Headless Window Operations** (2 weeks)
- Implement selective window verb support
- NOT dead code removal - these verbs are needed
- See this document
- ✅ NEW PHASE

**Phase 4: Headless Menu Operations** (1-2 weeks)
- Implement menu data manipulation verbs
- See HEADLESS_MENU_OPERATIONS_PLAN.md
- ✅ NEW PHASE (if approved)

**Phase 5: Legacy Format Writers** (2 weeks)
- Remove v6 writer functions
- ✅ EXISTING

**Phase 6: Stub Function Cleanup** (1 week)
- Remove obvious stub functions
- ✅ EXISTING

**Phase 7: Documentation** (1 day)
- Document Makefile-based GUI exclusion
- Explain headless verb availability
- ✅ NEW PHASE

---

## Part 7: Key Insight: Not Removal, But Implementation

**Original assumption**: Phase 3 would "remove GUI dead code"

**Reality**: Phase 3 is actually "implement selective headless support for window verbs"

**This changes the strategy fundamentally**:
- GUI files aren't removed, they're excluded
- Some GUI-adjacent verbs need headless implementations
- This is similar to menu verb situation

**Revised understanding**:
1. **Dead code removal** (Phases 1, 2, 5, 6) - Remove truly unused code
2. **Headless implementation** (Phases 3, 4) - Make key verbs work without GUI
3. **Documentation** (Phase 7) - Explain the approach

---

## Part 8: Success Criteria

Implementation is complete when:

1. ✅ `window.open(adr)` sets current target
2. ✅ `window.close()` clears current target
3. ✅ `window.getTitle(adr)` returns object name
4. ✅ `window.getFile(adr)` returns database file path
5. ✅ `window.isReadOnly(adr)` checks ODB object flags
6. ✅ `window.isModified(adr)` checks ODB object flags
7. ✅ `window.setModified(adr, flag)` sets ODB object flags
8. ✅ `window.msg(text)` outputs to stdout
9. ✅ `window.isMenuScript(adr)` detects menu scripts
10. ✅ `window.dbStats()` returns appropriate error
11. ✅ `window.quickTime()` returns appropriate error
12. ✅ All noop verbs silently succeed or fail appropriately
13. ✅ All existing tests still pass
14. ✅ New tests for window verbs pass
15. ✅ Documentation updated with verb behavior matrix

---

## Part 9: User Questions/Decisions

### 1. Priority Order

**Recommendation**: Implement in dependency order:
1. Target management (open/close) - foundation
2. ODB properties (getTitle, flags) - most common
3. getFile - critical for persistence
4. Special cases (msg, isMenuScript, etc.)

**Question**: Does this order make sense?

---

### 2. msg() Implementation

**Options for `window.msg(text)` output**:
- A. Use logging system (log_debug, etc.)
- B. Direct printf to stdout
- C. Route through UserTalk logger

**Recommendation**: Option A (logging system) for consistency

**Decision needed**: Which approach?

---

### 3. getFile() Complexity

**Question**: How complex is getting database file path from address?
- Can we look it up from dbaddress?
- Or do we need to track database open state?

---

### 4. Phase Timing

**Should headless implementations be part of dead code removal strategy, or separate?**

**My recommendation**: **Separate strategic plan** (e.g., "HEADLESS_IMPLEMENTATION_PHASES.md")
- Dead code removal is cleanup (remove unused code)
- Headless implementation is enablement (make key verbs work)
- Different risk profiles and purposes

**Decision needed**: Include in dead code strategy or separate plan?

---

## Appendix A: Window Verbs Implementation Matrix

| Verb | Current | Headless Implementation | Effort | Risk |
|------|---------|------------------------|--------|------|
| open | Stub | target.set(adr) | 0.5d | LOW |
| close | Stub | target.clear() | 0.5d | LOW |
| getTitle | Stub | Return object name | 1d | LOW |
| getFile | Stub | Return DB file path | 1-2d | MED |
| isReadOnly | Stub | Check ODB flags | 1d | MED |
| isModified | Stub | Check ODB flags | 1d | MED |
| setModified | Stub | Set ODB flags | 1d | MED |
| msg | Stub | Output to stdout | 0.5d | LOW |
| dbStats | Stub | Return error | 0.5d | LOW |
| quickTime | Stub | Return error | 0.5d | LOW |
| isMenuScript | Stub | Check external ID | 1d | MED |
| runSelection | Stub | Defer (noop) | 0d | N/A |
| about | Stub | Noop | 0d | N/A |
| bringToFront | Stub | Noop | 0d | N/A |
| frontmost | Stub | Noop | 0d | N/A |
| isFront | Stub | Noop | 0d | N/A |
| next | Stub | Noop | 0d | N/A |
| sendToBack | Stub | Noop | 0d | N/A |
| hide | Stub | Noop | 0d | N/A |
| show | Stub | Noop | 0d | N/A |
| getPosition | Stub | Noop | 0d | N/A |
| setPosition | Stub | Noop | 0d | N/A |
| getSize | Stub | Noop | 0d | N/A |
| setSize | Stub | Noop | 0d | N/A |
| zoom | Stub | Noop | 0d | N/A |
| setTitle | Stub | Noop | 0d | N/A |
| isOpen | Stub | Return false | 0d | N/A |
| scroll | Stub | Noop | 0d | N/A |
| update | Stub | Noop | 0d | N/A |
| quickScript | Stub | Noop | 0d | N/A |

---

## Appendix B: Glue Scripts (No Changes Needed)

These stay as-is since they use kernel verbs:

**getType.ut**:
```usertalk
// Uses: window.isMenuScript, defined(), typeOf
// Returns: type of object or unknownType
```

**isHidden.ut**:
```usertalk
// Uses: window.isVisible
// Returns: not window.isVisible(adr)
```

**visit.ut**:
```usertalk
// Uses: window.frontmost, window.next
// Iterates through all open windows
// Calls visitor proc on each
```

---

*Plan created: 2025-12-21*
*Total effort estimate: 7-10 days*
*Status: DRAFT - Awaiting user decisions*
