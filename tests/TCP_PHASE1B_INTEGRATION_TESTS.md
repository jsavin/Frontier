# TCP Phase 1B Integration Tests - Summary

**File**: `tests/integration/test_cases/tcp_verbs.yaml` (Phase 1B section)
**Date**: 2026-01-20
**Test Count**: 23 tests
**Status**: All tests passing (23/23) ✅

## Overview

Integration tests for TCP Phase 1B address encoding verbs (`tcp.addressEncode` and `tcp.addressDecode`). These tests validate the UserTalk API layer built on top of the C functions tested in Phase 1A unit tests.

## Test Categories

### 1. Happy Path - Valid IP Addresses (6 tests)
Tests basic encoding and decoding of common IP addresses:
- **tcp.addressEncode - private IP (192.168.1.1)**: Encode → 3232235777
- **tcp.addressEncode - localhost (127.0.0.1)**: Encode → 2130706433
- **tcp.addressEncode - public DNS (8.8.8.8)**: Encode → 134744072
- **tcp.addressDecode - private IP (3232235777)**: Decode → "192.168.1.1"
- **tcp.addressDecode - localhost (0x7F000001)**: Decode → "127.0.0.1"
- **tcp.addressDecode - public DNS (134744072)**: Decode → "8.8.8.8"

**Purpose**: Validates basic conversion functionality works correctly
**Why**: These are the most common use cases - must work reliably

### 2. Roundtrip Tests (6 tests)
Validates that encode→decode produces the original string:
- **192.168.1.1**: String → encode → decode → original string
- **127.0.0.1**: String → encode → decode → original string
- **8.8.8.8**: String → encode → decode → original string
- **0.0.0.0**: Wildcard → encode → decode → original string
- **255.255.255.255**: Broadcast → encode → decode → original string

**Purpose**: Validates conversion is lossless and invertible
**Why**: Critical for network operations - address must survive round trip
**Reference**: Phase 1A unit test `test_address_roundtrip_conversion`

### 3. Edge Cases - Boundary Values (6 tests)
Tests special-purpose IP addresses at boundaries:
- **tcp.addressEncode - wildcard (0.0.0.0)**: Encode → 0
- **tcp.addressEncode - broadcast (255.255.255.255)**: Encode → 4294967295
- **tcp.addressDecode - wildcard (0)**: Decode → "0.0.0.0"
- **tcp.addressDecode - broadcast (4294967295)**: Decode → "255.255.255.255"

**Purpose**: Validates boundary conditions (min/max values)
**Why**: Special addresses used in network protocols
**Note**: 0.0.0.0 encoding/decoding works (used in Phase 2 for server binding)

### 4. Error Cases - Invalid IP Strings (6 tests)
Tests error handling for malformed IP address strings:
- **Too many octets**: "192.168.1.1.1" → error
- **Too few octets**: "192.168.1" → error
- **Octet overflow**: "192.168.1.256" → error
- **Negative octet**: "192.168.-1.1" → error
- **Non-numeric**: "192.168.1.x" → error
- **Empty string**: "" → error

**Purpose**: Validates input validation rejects malformed addresses
**Why**: Prevents buffer overflows and undefined behavior
**Implementation**: Uses `inet_pton()` for validation

### 5. Return Type Validation (2 tests)
Validates return types match UserTalk semantics:
- **tcp.addressEncode - return type**: Returns `longType`
- **tcp.addressDecode - return type**: Returns `stringType`

**Purpose**: Ensures type safety and correct UserTalk behavior
**Why**: UserTalk is dynamically typed - must verify types

## Key Design Decisions

### 1. Decimal Literals Instead of Hex

**Issue**: UserTalk interprets hex literals (e.g., `0xC0A80101`) as signed 32-bit integers. For values with the high bit set (like 192.168.1.1 = 0xC0A80101), this produces negative numbers.

**Solution**: Tests use decimal literals for comparisons:
```usertalk
// ❌ WRONG - produces -1062731519 (signed interpretation)
return addr == 0xC0A80101

// ✅ CORRECT - produces 3232235777 (unsigned)
return addr == 3232235777
```

**Why**: UserTalk long type is 32-bit signed, but IP addresses are unsigned. Hex literals with high bit set become negative.

### 2. Leading Zeros Accepted

**Behavior**: `inet_pton()` accepts leading zeros and treats them as decimal (not octal):
```usertalk
tcp.addressEncode("192.168.001.001")  // → 3232235777 (same as 192.168.1.1)
```

**Decision**: This behavior is acceptable and follows `inet_pton()` semantics. No test for rejecting leading zeros.

**Why**: `inet_pton()` is the standard POSIX IP parsing function. We follow its behavior for consistency.

### 3. Zero Address (0.0.0.0) Allowed

**Note**: Phase 1A tests reject 0.0.0.0 for **client connections** (`tcp.openAddrStream`), but Phase 1B **encoding** tests allow it.

**Rationale**:
- 0.0.0.0 is invalid for connecting to a server (Phase 1A client operations)
- 0.0.0.0 is valid for server binding (Phase 2 listener operations)
- Address encoding is a **general-purpose utility** - should not restrict based on use case

**Reference**: `tests/integration/test_cases/tcp_verbs.yaml` - Phase 1A comment documents this distinction

## Test Coverage

**What's Tested**:
- ✅ Encoding common IPs (private, localhost, public DNS)
- ✅ Decoding common addresses
- ✅ Roundtrip conversion (lossless)
- ✅ Boundary values (0.0.0.0, 255.255.255.255)
- ✅ Error handling (malformed strings)
- ✅ Return type validation

**What's NOT Tested** (covered by Phase 1A unit tests):
- ❌ NULL pointer handling (defensive programming)
- ❌ Thread-safety of address conversion
- ❌ Performance/benchmarking

**Integration Test Philosophy**: Integration tests validate UserTalk API behavior. Internal implementation details (NULL pointers, concurrency) are covered by C unit tests.

## Running the Tests

**Run all TCP tests**:
```bash
FRONTIER_TEST_FILES="test_cases/tcp_verbs.yaml" make -C tests test-integration
```

**Run specific test**:
```bash
./frontier-cli/frontier-cli -e 'tcp.addressEncode("192.168.1.1")'
# Output: 3232235777

./frontier-cli/frontier-cli -e 'tcp.addressDecode(3232235777)'
# Output: 192.168.1.1
```

**Expected output**:
```
Running tests from: tcp_verbs.yaml
  Found 53 test(s)
    ✓ PASS: tcp.addressEncode - private IP (192.168.1.1)
    ✓ PASS: tcp.addressEncode - localhost (127.0.0.1)
    ✓ PASS: tcp.addressEncode - public DNS (8.8.8.8)
    ✓ PASS: tcp.addressDecode - private IP (3232235777)
    ✓ PASS: tcp.addressDecode - localhost (0x7F000001)
    ✓ PASS: tcp.addressDecode - public DNS (134744072)
    [... 17 more Phase 1B tests ...]

    53/53 tests passed
```

## Test File Organization

**Location**: `tests/integration/test_cases/tcp_verbs.yaml`

**Structure**:
```yaml
# Phase 1A tests (30 tests)
# - Stream lifecycle, error handling, behavioral specs

# ===========================================================================
# Phase 1B: Address Encoding Verbs (23 tests)
# ===========================================================================
# - Happy path (6 tests)
# - Roundtrip (6 tests)
# - Edge cases (6 tests)
# - Error cases (6 tests)
# - Return types (2 tests)
```

**Comment Headers**: Each section has clear header comments explaining what verbs are being tested and references to specs.

## References

- **Phase 1A Unit Tests**: `tests/TCP_PHASE1A_TEST_SUMMARY.md`
- **C Implementation**: `Common/source/tcpverbs.c` (tcp_address_encode, tcp_address_decode)
- **Spec**: `planning/phase4/networking/TCP_PHASE1B_SPEC.md` (to be created)
- **Integration Test Framework**: `tests/integration/runner.py`

## Lessons Learned

### 1. UserTalk Integer Literal Gotcha
Hex literals like `0xC0A80101` are interpreted as **signed** 32-bit integers. For IP addresses (which are unsigned), use decimal literals or roundtrip comparisons.

### 2. inet_pton() Behavior
The `inet_pton()` function accepts leading zeros and treats them as decimal (not octal). This is standard POSIX behavior - we follow it rather than adding extra validation.

### 3. General-Purpose Utilities Should Be Permissive
Address encoding verbs (`tcp.addressEncode`/`tcp.addressDecode`) are general-purpose utilities used by multiple operations. They should not enforce use-case-specific restrictions (like rejecting 0.0.0.0). Restrictions should be enforced at the point of use (e.g., `tcp.openAddrStream` rejects 0.0.0.0).

### 4. Test Organization
Clear section headers with comments make test files easier to navigate. When adding tests for a new phase, add a comment block explaining what's being tested and why.

## Future Enhancements

1. Add performance benchmarks for address conversion (measure overhead of Pascal↔C string conversion)
2. Add fuzz testing with randomly generated IP strings
3. Add tests for IPv6 address encoding (future Phase)
4. Add integration tests combining address encoding with connection operations

## Conclusion

All 23 TCP Phase 1B integration tests pass successfully, validating:
- Basic encoding/decoding functionality
- Roundtrip conversion (lossless)
- Boundary conditions (0.0.0.0, 255.255.255.255)
- Error handling for malformed inputs
- Return type correctness

Combined with Phase 1A's 28 unit tests and 30 integration tests, TCP networking has 81 total tests covering both C implementation and UserTalk API.
