# opattributes Stub Tests

## Overview

This test verifies that the opattributes C verb stubs are properly integrated into the headless build system.

**Test File**: `tests/opattributes_integration_tests.c` (70 lines)

**Implementation File**: `tests/headless_opattributes_verbs.c`

## Design Decision

The five opattributes verbs are GUI-centric features designed to manage binary attributes on outline nodes:

```usertalk
op.attributes.addGroup()    # Add group of attributes to node
op.attributes.getOne()      # Get single attribute by name
op.attributes.getAll()      # Get all attributes as table
op.attributes.setOne()      # Set/update single attribute
op.attributes.makeEmpty()   # Clear all attributes
```

These verbs are not suited for headless CLI mode because:

1. **No Outline Target**: op.attributes operates on the "current node" which is a GUI concept
2. **Complex Serialization**: Full implementation would require binary pack/unpack with BE64 compliance
3. **No Use Cases**: No meaningful CLI operations require outline attributes
4. **Consistent Pattern**: Follows approach used for other GUI-only verbs (op.hoist, op.dehoist, op.expand, op.collapse)

## Implementation

The stub returns a clear error message instead of undefined behavior:

```c
seterrorstring("op.attributes verbs are not supported in headless mode", bserror);
return false;
```

## Running the Test

```bash
cd tests
make opattributes_integration_tests
./opattributes_integration_tests
```

**Expected Output**:
```
opattributes_integration_tests: Testing headless opattributes stubs
========================================================================
TEST: opattributes verbs should be stubbed in headless mode
  ✓ opattributes verbs are correctly stubbed as not supported
    - addgroup: Not supported in headless
    - getall:   Not supported in headless
    - getone:   Not supported in headless
    - makeempty: Not supported in headless
    - setone:   Not supported in headless

========================================================================
All tests passed
```

## Future Work

For GUI-capable builds, implement full opattributes with:

1. **Outline target management** - Access current node in outline
2. **Binary serialization** - Pack/unpack with proper BE64 compliance
3. **Full CRUD operations** - Add, get, set, clear attributes on outline nodes
4. **Comprehensive testing** - Tests would include:
   - Phase 1: Test infrastructure helpers
   - Phase 2: Basic operations (create, retrieve, set, clear)
   - Phase 3: Multi-attribute operations
   - Phase 4: BE64 compliance validation (values > 2^32, negative numbers, dates)
   - Phase 5: Round-trip fidelity (pack/unpack preservation)
   - Phase 6: Edge cases (empty refcon, overwrites, case sensitivity)
   - Phase 7: Integration tests (multi-node isolation, nested structures)

## Files

- `headless_opattributes_verbs.c` - Stub implementation (returns error)
- `opattributes_integration_tests.c` - Stub verification test
