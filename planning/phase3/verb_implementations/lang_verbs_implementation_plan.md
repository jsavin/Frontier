# Lang Verbs Implementation Plan

## Current State (5/61 implemented = 8%)

### Already Implemented ✅
1. `lang.new` - Create new objects (partial - some types stubbed)
2. `lang.boolean` - Convert to boolean
3. `lang.char` - Convert to character
4. `lang.int` (short) - Convert to short integer
5. `lang.long` - Convert to long integer

### Stubbed Verbs (56 remaining)

## Implementation Strategy

We'll implement in 5 phases with test gates and PRs between each phase.

---

## Phase 1: Type Conversion Verbs (Priority: HIGH)
**Goal:** Complete all basic type conversion operations
**PR:** #1 - Type conversions
**Test Coverage:** Integration tests for each verb

### Verbs to Implement (7 verbs)
1. `lang.double` - Convert to double-precision float
2. `lang.single` - Convert to single-precision float
3. `lang.fixed` - Convert to fixed-point
4. `lang.string4` - Convert to 4-character string (OSType)
5. `lang.short` - Alias for lang.int (already exists, verify)
6. `lang.ostype` - Convert to OSType (may be same as string4)
7. `lang.direction` - Convert to direction enum

### Implementation Notes
- These are straightforward type coercion functions
- Use existing patterns from lang.long, lang.boolean
- Each takes 1 parameter, returns converted value
- Error handling: return error for unconvertible types

### Test Plan
- Create `tests/integration/test_cases/lang_type_verbs.yaml`
- Test each conversion with valid inputs
- Test error cases (unconvertible types)
- Test round-trip conversions (e.g., long→double→long)

---

## Phase 2: Memory & Utility Verbs (Priority: HIGH)
**Goal:** System utility functions
**PR:** #2 - Memory and utility verbs
**Test Coverage:** Integration + unit tests

### Verbs to Implement (4 verbs)
1. `lang.memavail` - Return available memory
2. `lang.flushmemory` - Flush/compact memory
3. `lang.abs` - Absolute value
4. `lang.random` - Random number generator

### Implementation Notes
- `lang.memavail`: Use portable memory query functions
- `lang.flushmemory`: May be no-op in modern systems (document)
- `lang.abs`: Simple math operation, handle all numeric types
- `lang.random`: Use existing random infrastructure

### Test Plan
- Memory functions: verify return values are reasonable
- `lang.abs`: test with positive, negative, zero, all types
- `lang.random`: test range, distribution (basic)

---

## Phase 3: Core Lang Operations (Priority: CRITICAL)
**Goal:** Essential language operations
**PR:** #3 - Core language operations
**Test Coverage:** Extensive integration tests

### Verbs to Implement (4 verbs)
1. `lang.delete` - Delete variable/object
2. `lang.evaluate` - Evaluate UserTalk code string
3. `lang.callscript` - Call a script by name
4. `lang.msg` - Display message (headless: log or return)

### Implementation Notes
- `lang.delete`: Use existing hashdelete infrastructure
- `lang.evaluate`: Parse and execute UserTalk string
- `lang.callscript`: Lookup script in current context, execute
- `lang.msg`: In headless mode, log to console or return string

### Test Plan
- Test delete with various object types
- Test evaluate with valid/invalid code
- Test callscript with existing/missing scripts
- Test msg output

---

## Phase 4: Date/Time Verbs (Priority: MEDIUM)
**Goal:** Timestamp manipulation
**PR:** #4 - Date/time verbs
**Test Coverage:** Integration tests

### Verbs to Implement (4 verbs)
1. `lang.timecreated` - Get creation time of object
2. `lang.timemodified` - Get modification time of object
3. `lang.settimecreated` - Set creation time
4. `lang.settimemodified` - Set modification time

### Implementation Notes
- Already have token definitions (in enum)
- Use existing frontier_time_t infrastructure
- These operate on hash table entries
- May need ODB address parameters

### Test Plan
- Create object, verify timestamps
- Modify object, verify timemodified updates
- Set explicit timestamps, verify persistence

---

## Phase 5: Binary & Advanced Types (Priority: MEDIUM)
**Goal:** Advanced type operations
**PR:** #5 - Binary and advanced types
**Test Coverage:** Integration tests

### Verbs to Implement (6 verbs)
1. `lang.binary` - Create/convert binary data
2. `lang.getbinarytype` - Get OSType of binary
3. `lang.setbinarytype` - Set OSType of binary
4. `lang.filespec` - Create file specification
5. `lang.address` - Create address value
6. `lang.alias` - Create alias value

### Implementation Notes
- Binary operations use existing binary value infrastructure
- filespec: create from path string
- address: create from hash table reference
- alias: Mac-specific, may be stub for now

### Test Plan
- Binary: create, get type, set type, round-trip
- filespec: create from path, verify
- address: create, dereference

---

## Deferred Verbs (Will Not Implement in This Branch)

### Apple Event Verbs (Mac GUI-specific, 9 verbs)
- `lang.coerceappleitem`
- `lang.countapplelistitems`
- `lang.getapplelistitem`
- `lang.putapplelistitem`
- `lang.geteventattribute`
- `lang.seteventtimeout`
- `lang.seteventtransactionid`
- `lang.seteventinteractionlevel`
- `lang.transactionevent`

### Window/UI Verbs (GUI-specific, 4 verbs)
- `lang.packwindow`
- `lang.unpackwindow`
- `lang.rollbeachball` (UI spinner)
- `lang.edit`

### Platform-Specific Verbs (Windows/legacy, 3 verbs)
- `lang.ddeevent` (Windows DDE)
- `lang.calldll` (Windows DLL)
- `lang.callxcmd` (Mac XCMD)

### Complex Verbs (Future work, 8 verbs)
- `lang.list` - List type (needs list infrastructure)
- `lang.record` - Record type (needs record infrastructure)
- `lang.enum` - Enum type
- `lang.pattern` - Pattern type
- `lang.point` - Point type
- `lang.rect` - Rectangle type
- `lang.rgb` - RGB color type
- `lang.scripterror` - Script error handling

### Threading Verb (Future, 1 verb)
- `lang.evaluatethread` - Threaded evaluation

### Display Verb (1 verb)
- `lang.displaystring` - String representation

---

## Success Criteria

After completing Phases 1-5:
- **31 new verbs implemented** (from 5 to 36 total)
- **Coverage increases from 8% to 59%**
- **All critical language operations work**
- **5 PRs merged with test gates**
- **Foundation for remaining verbs established**

## Testing Strategy

### Integration Tests
- Create YAML test files for each phase
- Test success cases and error cases
- Verify type conversions work correctly
- Ensure headless mode compatibility

### Unit Tests
- Add C unit tests for complex verbs
- Test edge cases and error handling
- Verify memory safety

### PR Gates
- All tests must pass before merge
- Code review required for each PR
- No regressions in existing functionality

## Timeline

- **Phase 1:** ~2-3 hours (straightforward conversions)
- **Phase 2:** ~2-3 hours (utility functions)
- **Phase 3:** ~4-6 hours (complex operations)
- **Phase 4:** ~2-3 hours (date/time)
- **Phase 5:** ~3-4 hours (advanced types)

**Total estimated effort:** 13-19 hours over multiple sessions

## Next Steps

1. Start with Phase 1 (type conversions)
2. Implement verbs one by one
3. Write integration tests for each
4. Run test suite, verify no regressions
5. Code review with bar-raiser agent
6. Create PR #1
7. Repeat for subsequent phases
