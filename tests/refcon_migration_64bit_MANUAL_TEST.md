# 64-bit Refcon Migration Manual Test Procedure

## Purpose
Validate that the Phase 4B 64-bit value packing implementation correctly preserves refcon values during v6→v7 migration, with particular emphasis on sign preservation for negative values.

## Background
The v6 format stored long values as 32-bit integers. The v7 format must store them as 64-bit integers using proper sign extension. Incorrect implementation (e.g., casting to unsigned before widening) causes negative values to become large positive values.

## Critical Test Case
**Negative small value (-123)** - This is the key test that validates sign extension:
- v6 storage: 0xFFFFFF85 (32-bit two's complement for -123)
- Incorrect v7: 0x00000000FFFFFF85 (4,294,967,173 - wrong!)
- Correct v7:   0xFFFFFFFFFFFFFF85 (-123 with sign extension - correct!)

## Test Database
Fixture: `tests/fixtures/refcon_migration_test_v6.root`

Contains:
- `longValueTests` table with scalar long values
  - `zeroLong = 0`
  - `positiveSmall = 42`
  - `positiveLarge = 2147483647` (2^31-1)
  - `negativeSmall = -123` ← **CRITICAL**
  - `negativeLarge = -2147483648` (-2^31)
  - `positiveMedium = 1000000`
  - `negativeMedium = -1000000`
  - `nestedTable.innerPositive = 999`
  - `nestedTable.innerNegative = -777`

- `refconDateTests` table with date values
  - `date2030` (pre-Y2038)
  - `date2037` (approaching Y2038)

- `refconTests` outline with refcon values stored in headlines
  - Test Cases 1-10 with various refcon values

## Manual Test Procedure

### Step 1: Verify Fixture Exists
```bash
cd /path/to/Frontier
ls -lh tests/fixtures/refcon_migration_test_v6.root
```

Expected: File exists, approximately 2-3 KB

### Step 2: Perform Migration
```bash
rm -f tests/tmp/migration/refcon_test*.root
cp tests/fixtures/refcon_migration_test_v6.root tests/tmp/migration/refcon_test_v6.root

# Migrate using the migration function
./tests/save_migration_tests  # Or use frontier-cli with upgrade flag
```

### Step 3: Inspect Migrated Binary with xxd
```bash
# Look at the migrated database
xxd tests/tmp/migration/refcon_test_v6-v7.root | head -50
```

Check for:
- Database header version: 0x0007 (first 2 bytes)
- 64-bit addresses (8-byte values instead of 4-byte)

### Step 4: Verify Values via Direct Inspection

Since the test fixture doesn't have a full system table structure, we cannot use the CLI to evaluate expressions. Instead:

#### Option A: Use Debugger Inspection
```bash
# Build debug version
make -C tests refcon_migration_64bit_test

# Run under lldb
lldb tests/refcon_migration_64bit_test
(lldb) b test_long_value
(lldb) run
# Inspect values at breakpoint
```

#### Option B: Full Frontier.root Migration Test
Create test data in a full Frontier database:

```bash
# 1. Copy Frontier.root to working location
cp databases/Frontier.root tests/tmp/migration/full_refcon_test_v6.root

# 2. Add test data via CLI
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  --system-root tests/tmp/migration/full_refcon_test_v6.root \
  -e '
new(tableType, @refconMigrationTests);
refconMigrationTests.zero = 0;
refconMigrationTests.positiveSmall = 42;
refconMigrationTests.positiveLarge = 2147483647;
refconMigrationTests.negativeSmall = -123;
refconMigrationTests.negativeLarge = -2147483648;
refconMigrationTests.positiveMedium = 1000000;
refconMigrationTests.negativeMedium = -1000000;
new(tableType, @refconMigrationTests.nested);
refconMigrationTests.nested.innerPos = 999;
refconMigrationTests.nested.innerNeg = -777;
return "Test data created"
'

# 3. Migrate to v7
./frontier-cli/frontier-cli --migrate tests/tmp/migration/full_refcon_test_v6.root

# 4. Verify values in migrated database
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  --system-root tests/tmp/migration/full_refcon_test_v6-v7.root \
  -e 'refconMigrationTests.negativeSmall'

# Expected output: -123
# Bug output: 4294967173 (if unsigned cast bug present)
```

### Step 5: Validate All Test Values

Run each test individually:

```bash
DB="tests/tmp/migration/full_refcon_test_v6-v7.root"
CLI="./frontier-cli/frontier-cli"

# Zero value
$CLI --system-root "$DB" -e 'refconMigrationTests.zero'
# Expected: 0

# Positive small
$CLI --system-root "$DB" -e 'refconMigrationTests.positiveSmall'
# Expected: 42

# Positive large (near 2^31)
$CLI --system-root "$DB" -e 'refconMigrationTests.positiveLarge'
# Expected: 2147483647

# Negative small (CRITICAL TEST)
$CLI --system-root "$DB" -e 'refconMigrationTests.negativeSmall'
# Expected: -123
# Bug output: 4294967173

# Negative large (-2^31)
$CLI --system-root "$DB" -e 'refconMigrationTests.negativeLarge'
# Expected: -2147483648

# Positive medium
$CLI --system-root "$DB" -e 'refconMigrationTests.positiveMedium'
# Expected: 1000000

# Negative medium
$CLI --system-root "$DB" -e 'refconMigrationTests.negativeMedium'
# Expected: -1000000

# Nested positive
$CLI --system-root "$DB" -e 'refconMigrationTests.nested.innerPos'
# Expected: 999

# Nested negative
$CLI --system-root "$DB" -e 'refconMigrationTests.nested.innerNeg'
# Expected: -777
```

## Expected Results

All test values should match their expected values exactly.

### Pass Criteria
- ✅ All positive values remain positive
- ✅ All negative values remain negative (not converted to large positive)
- ✅ Zero remains zero
- ✅ Nested table values preserved correctly
- ✅ Values at boundaries (2^31-1, -2^31) preserved correctly

### Failure Symptoms
If the bug is present (unsigned cast instead of sign-extended cast):
- ❌ `negativeSmall (-123)` becomes `4294967173`
- ❌ `negativeLarge (-2147483648)` becomes `2147483648`
- ❌ `negativeMedium (-1000000)` becomes `4293967296`
- ❌ `nested.innerNeg (-777)` becomes `4294966519`

## Implementation Reference
The fix is in:
- `Common/source/langpack.c` - `langpackvalue_modern()` function
- `Common/source/langhash.c` - `hashpackvalue_modern()` function

Both must use:
```c
int64_t value64 = (int64_t)val.data.longvalue;  // ✅ Correct: sign-extended cast
```

Not:
```c
uint64_t value64 = (uint64_t)((uint32_t)val.data.longvalue);  // ❌ Wrong: unsigned cast
```

## Automation Note
This test procedure is documented here because:
1. The minimal test fixture (`refcon_migration_test_v6.root`) lacks system table structure required by CLI
2. C test infrastructure requires significant boilerplate for database access
3. The most reliable test is manual verification via CLI with a full Frontier database

For CI/CD automation, use the full Frontier.root database approach (Option B above).

## References
- `planning/phase3/LONG_VALUE_PACKING_DATA_LOSS_ANALYSIS.md` - Root cause analysis
- `planning/phase3/V7_64BIT_VALUE_PACKING_PLAN.md` - Implementation plan
- Issue #XXX - 64-bit value packing implementation
