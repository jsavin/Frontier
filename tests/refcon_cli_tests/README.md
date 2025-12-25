# Refcon CLI Tests - Phase 2: Serialization

## Overview

Phase 2 of the refcon test suite validates refcon persistence through outline pack/unpack operations using UserTalk CLI tests. This builds on Phase 1 (low-level C tests in `refcon_tests.c`) and uses the frontier-cli to test production code paths.

**Current Status**: These tests are **SKIPPED** in the test suite because the required UserTalk verb bindings (outlineType, op.insert, op.setRefcon, op.getRefcon) are not yet implemented in the headless CLI runtime. The test infrastructure is complete and ready to execute once verb binding work is completed (tracked as future work separate from the v6→v7 migration PR).

## Test Pattern

- **Pattern**: Execute UserTalk scripts via frontier-cli, parse output to verify results
- **Reference**: `table_verb_tests.c` (lines 68-120) for CLI execution pattern
- **Error Handling**: Tests SKIP gracefully if required verbs are not implemented

## Test Coverage

### Test 2.1: Single headline pack/unpack roundtrip
- **Goal**: Create outline with one headline, set 32-byte binary blob refcon, pack outline, unpack back, verify refcon survived round-trip
- **Required Verbs**:
  - `new (outlineType, @var)` - Create new outline
  - `op.insert` - Add headline to outline
  - `op.setRefcon` - Set refcon data
  - `op.getRefcon` - Retrieve refcon data
  - `pack` / `unpack` - Serialize/deserialize outline
- **Status**: SKIP (CLI segfaults when creating outlines with database)

### Test 2.2: Multiple headlines with mixed refcon sizes
- **Goal**: Create outline with 5 headlines with refcon sizes: 8, 16, 64, NULL, 32. Pack/unpack entire outline. Verify each headline's refcon survived.
- **Required Verbs**:
  - All from 2.1, plus:
  - `op.go` - Navigate between headlines
  - `op.hasRefcon` - Check if headline has refcon
  - Loop constructs (for i = 1 to 5)
- **Status**: SKIP (CLI segfaults when creating outlines with database)

### Test 2.3: Nested outline with refcons
- **Goal**: Create parent outline with 2 headlines, create nested child outline under headline 1, set refcons on parent and child headlines, pack/unpack entire structure, verify nested structure and refcons survived.
- **Required Verbs**:
  - All from 2.2, plus:
  - `op.insertOutline` - Insert child outline
- **Status**: SKIP (CLI segfaults when creating outlines with database)

### Test 2.4: Outline with mixed content (text + refcons)
- **Goal**: Create outline with 3 headlines (mix of some with/without refcons), add varied content (text, refcons, empty), pack/unpack, verify all content preserved.
- **Required Verbs**:
  - All from 2.2, plus:
  - `op.setLineText` - Set headline text
  - `op.getLineText` - Retrieve headline text
- **Status**: SKIP (CLI segfaults when creating outlines with database)

## Current Issues

### Database Errors
All tests currently SKIP due to database loading errors when running with `Frontier-v6-v7.root`:

```
[db-ERROR] db.c:618: dbread read failed fnum=1 adr=0x9cf8aa bytes=8 saveas=0 source=0x0 current=0x600000c37ad8 dest=0x0
[table-ERROR] tableexternal_common.c:347: dbnormalizeaddress failed for adr=0x9cf396
[db-ERROR] db.c:618: dbread read failed fnum=1 adr=0x9cf8aa bytes=8 saveas=0 source=0x0 current=0x600000c37ad8 dest=0x0
[table-ERROR] tableexternal_common.c:386: legacy table conversion failed
Segmentation fault: 11
```

This indicates:
1. Database has corrupt/invalid external table addresses
2. Attempting to create outlineType objects triggers database lookups that fail
3. Need to either fix database migration or test without database dependency

### Verb Implementation Status

Based on `opverbs.c` inspection:
- ✅ `op.setRefcon` - Implemented (case setrefconfunc, line 3833)
- ✅ `op.getRefcon` - Implemented (case getrefconfunc, line 3822)
- ❓ `op.hasRefcon` - Need to verify
- ❓ `op.insert` - Need to verify (case insertfunc exists, line 3579)
- ❓ `op.go` - Need to verify
- ❓ `op.setLineText` - Implemented (case setlinetextfunc, line 3615)
- ❓ `op.getLineText` - Need to verify
- ❓ `op.insertOutline` - Implemented (case insertoutlinefunc, line 3906)
- ❓ `pack` / `unpack` - Need to verify outline-level pack/unpack verbs

## Running Tests

### Build
```bash
cd tests
make refcon_phase2_tests
```

### Execute
```bash
./refcon_phase2_tests
```

### Expected Output
Tests should either:
- **PASS**: Refcon operations work correctly
- **FAIL**: Verbs exist but refcon data doesn't survive serialization
- **SKIP**: Required verbs missing or CLI errors

## Next Steps

1. **Fix Database Issues**: Resolve `Frontier-v6-v7.root` external table address errors
2. **Verify Verb Bindings**: Check which op.* verbs are actually wired up in headless mode
3. **Alternative Test Approach**: Consider testing without database dependency (pure in-memory outlines)
4. **Phase 3**: Once Phase 2 passes, implement Phase 3 (v6→v7 migration) tests

## Implementation Notes

- Test harness follows `table_verb_tests.c` pattern exactly
- Uses `FRONTIER_HEADLESS_SKIP_STARTUP=1` to avoid startup script dependencies
- Uses `FRONTIER_LOG_LEVEL=error` to suppress noise
- Captures both stdout and stderr (2>&1) to detect all error conditions
- Gracefully handles missing verbs with clear error messages
- Does not crash or assert if verbs are missing - just reports SKIP

## File Structure

```
tests/
├── refcon_cli_tests/
│   ├── README.md                    # This file
│   └── phase2_serialization.c       # Test harness
├── refcon_tests.c                   # Phase 1: Low-level C tests
├── refcon_phase2_tests              # Built executable
└── Makefile                         # Build configuration
```
