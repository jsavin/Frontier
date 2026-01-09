# v6 Test Artifact Specification for Refcon Migration Testing

## Overview

This document specifies the v6 database artifact needed to test refcon migration and 64-bit value widening when migrating from v6 to v7.

**Purpose**: Verify that refcon values stored in v6 (32-bit) are correctly widened to 64-bit in v7 without data loss.

**Created by**: User, using Windows legacy Frontier app
**Used by**: Migration tests in frontier-cli headless mode

---

## File Details

**Filename**: `refcon_migration_test_v6.root`
**Location**: `tests/fixtures/refcon_migration_test_v6.root`
**Format**: Frontier v6 database (32-bit format)
**Size**: Small (minimal test data only)

**Creation Method**: Create a NEW standalone v6 database (not modifying Frontier-v6.root)

---

## Database Creation Steps

### Step 1: Create New v6 Database in Windows Frontier

```usertalk
// Create new empty database
db.new("refcon_migration_test_v6.root")
```

**Note**: All objects below will be created in the ROOT of this new database (not in workspace)

### Step 2: Create Test Objects

Follow the implementation scripts in sections 1-4 below to create all test objects.

### Step 3: Close Database

```usertalk
// After creating all test objects, close the database (auto-saves)
db.close()
```

**Important**: The database file `refcon_migration_test_v6.root` should now exist. Copy it to `tests/fixtures/` in the Frontier repository.

---

## Database Structure

### 1. Outline: `refconTests`

Create an outline at the ROOT level named `refconTests` with the following structure:

```
refconTests (outline)
├─ "Test Case 1: Zero refcon"
│  └─ refcon = 0
├─ "Test Case 2: Positive small"
│  └─ refcon = 42
├─ "Test Case 3: Positive large (near 2^31)"
│  └─ refcon = 2147483647  (max 32-bit signed positive)
├─ "Test Case 4: Negative small"
│  └─ refcon = -123
├─ "Test Case 5: Negative large (min 32-bit)"
│  └─ refcon = -2147483648  (min 32-bit signed negative)
├─ "Test Case 6: Positive medium"
│  └─ refcon = 1000000
├─ "Test Case 7: Negative medium"
│  └─ refcon = -1000000
├─ "Test Case 8: Nil refcon (not set)"
│  └─ refcon = <not set / nil>
├─ "Test Case 9: Nested outline with refcon"
│  ├─ refcon = 999
│  └─ children:
│     ├─ "Child 1"
│     │  └─ refcon = 111
│     └─ "Child 2"
│        └─ refcon = 222
└─ "Test Case 10: Multiple levels"
   ├─ refcon = 100
   └─ children:
      └─ "Level 2"
         ├─ refcon = 200
         └─ children:
            └─ "Level 3"
               └─ refcon = 300
```

**Implementation in Windows Frontier**:
```usertalk
// Create the outline at root level
lang.new(outlineType, @refconTests)
target.set(@refconTests)

// Test Case 1: Zero
op.insert("Test Case 1: Zero refcon", down)
op.attributes.setOne("refcon", 0)

// Test Case 2: Positive small
op.insert("Test Case 2: Positive small", down)
op.attributes.setOne("refcon", 42)

// Test Case 3: Positive large
op.insert("Test Case 3: Positive large (near 2^31)", down)
op.attributes.setOne("refcon", 2147483647)

// Test Case 4: Negative small
op.insert("Test Case 4: Negative small", down)
op.attributes.setOne("refcon", -123)

// Test Case 5: Negative large
op.insert("Test Case 5: Negative large (min 32-bit)", down)
op.attributes.setOne("refcon", -2147483648)

// Test Case 6: Positive medium
op.insert("Test Case 6: Positive medium", down)
op.attributes.setOne("refcon", 1000000)

// Test Case 7: Negative medium
op.insert("Test Case 7: Negative medium", down)
op.attributes.setOne("refcon", -1000000)

// Test Case 8: Nil refcon (don't set refcon)
op.insert("Test Case 8: Nil refcon (not set)", down)
// DON'T call setOne for this one - leave refcon unset

// Test Case 9: Nested outline
op.insert("Test Case 9: Nested outline with refcon", down)
op.attributes.setOne("refcon", 999)
op.insert("Child 1", right)
op.attributes.setOne("refcon", 111)
op.insert("Child 2", down)
op.attributes.setOne("refcon", 222)
op.go(left, 2)  // Back to parent level

// Test Case 10: Multiple levels
op.insert("Test Case 10: Multiple levels", down)
op.attributes.setOne("refcon", 100)
op.insert("Level 2", right)
op.attributes.setOne("refcon", 200)
op.insert("Level 3", right)
op.attributes.setOne("refcon", 300)
```

---

### 2. Table: `longValueTests`

Create a table at the ROOT level named `longValueTests` with the following scalar long values:

```
longValueTests (table)
├─ zeroLong = 0
├─ positiveSmall = 42
├─ positiveLarge = 2147483647  (max 32-bit signed)
├─ negativeSmall = -123
├─ negativeLarge = -2147483648  (min 32-bit signed)
├─ positiveMedium = 1000000
├─ negativeMedium = -1000000
└─ nestedTable (table)
   ├─ innerPositive = 999999
   └─ innerNegative = -999999
```

**Implementation in Windows Frontier**:
```usertalk
// Create the table at root level
lang.new(tableType, @longValueTests)

// Scalar long values
longValueTests.zeroLong = 0
longValueTests.positiveSmall = 42
longValueTests.positiveLarge = 2147483647
longValueTests.negativeSmall = -123
longValueTests.negativeLarge = -2147483648
longValueTests.positiveMedium = 1000000
longValueTests.negativeMedium = -1000000

// Nested table
lang.new(tableType, @longValueTests.nestedTable)
longValueTests.nestedTable.innerPositive = 999999
longValueTests.nestedTable.innerNegative = -999999
```

---

### 3. Table: `dateValueTests`

Create a table at the ROOT level named `dateValueTests` with date values that will test Year 2038 compliance:

```
dateValueTests (table)
├─ date1970 = January 1, 1970 00:00:00
├─ date2000 = January 1, 2000 00:00:00
├─ date2030 = January 1, 2030 00:00:00
├─ date2037 = December 31, 2037 23:59:59  (just before 2038 problem)
└─ date2038 = January 1, 2038 00:00:00   (triggers 2038 problem in 32-bit)
```

**Implementation in Windows Frontier**:
```usertalk
// Create the table at root level
lang.new(tableType, @dateValueTests)

// Date values
dateValueTests.date1970 = date.set(1970, 1, 1, 0, 0, 0)
dateValueTests.date2000 = date.set(2000, 1, 1, 0, 0, 0)
dateValueTests.date2030 = date.set(2030, 1, 1, 0, 0, 0)
dateValueTests.date2037 = date.set(2037, 12, 31, 23, 59, 59)
dateValueTests.date2038 = date.set(2038, 1, 1, 0, 0, 0)
```

---

### 4. Outline: `refconDateTests`

Create an outline at the ROOT level named `refconDateTests` where refcons store packed date values (edge case for Year 2038):

```
refconDateTests (outline)
├─ "Date as refcon: 2030"
│  └─ refcon = <packed date: 2030-01-01>
└─ "Date as refcon: 2037"
   └─ refcon = <packed date: 2037-12-31>
```

**Implementation in Windows Frontier**:
```usertalk
// Create the outline at root level
lang.new(outlineType, @refconDateTests)
target.set(@refconDateTests)

// Date as refcon (store as packed long value)
local(d1 = date.set(2030, 1, 1, 0, 0, 0))
op.insert("Date as refcon: 2030", down)
// Note: In v6, dates are stored as unsigned long (seconds since 1904)
// Store the raw long value as refcon
op.attributes.setOne("refcon", long(d1))

local(d2 = date.set(2037, 12, 31, 23, 59, 59))
op.insert("Date as refcon: 2037", down)
op.attributes.setOne("refcon", long(d2))
```

---

## Verification Checklist (After Creating)

Before providing the artifact, verify in Windows Frontier:

### Outline Refcons
- [ ] `refconTests` exists at root level and is an outline
- [ ] Can navigate to each test case headline
- [ ] `op.attributes.getOne("refcon")` returns correct value for each case
- [ ] Test Case 8 has no refcon attribute (nil)
- [ ] Nested outlines maintain their refcons

### Table Long Values
- [ ] `longValueTests` exists at root level and is a table
- [ ] Each scalar value matches specification
- [ ] Nested table values are correct
- [ ] `typeOf(longValueTests.positiveLarge) == longType`

### Date Values
- [ ] `dateValueTests` exists at root level and is a table
- [ ] Each date value is correct
- [ ] `typeOf(dateValueTests.date2030) == dateType`

### Refcon Date Tests
- [ ] `refconDateTests` exists at root level and is an outline
- [ ] Refcons store date values as longs
- [ ] Values can be read back and converted to dates

---

## Expected Test Results (After Migration to v7)

After migrating this v6 database to v7 format, we expect:

### Refcon Values
- ✅ All refcon values preserved exactly (no data loss)
- ✅ Negative values remain negative (not converted to unsigned)
- ✅ Zero remains zero
- ✅ Nil refcons remain nil
- ✅ Nested outline refcons preserved

### Long Values
- ✅ All table long values preserved exactly
- ✅ Negative values remain negative
- ✅ Nested table values preserved

### Date Values
- ✅ All dates preserved correctly
- ✅ Dates after 2038 still valid (Year 2038 compliance)

### Post-Migration 64-bit Support
After migration, v7 should support NEW refcon values > 2^32:
- ✅ Can set refcon to 4294967296 (2^32)
- ✅ Can set refcon to -4294967296
- ✅ Values round-trip correctly without truncation

---

## File Delivery

**Once created, provide the file at**:
`tests/fixtures/refcon_migration_test_v6.root`

**Also provide** (optional but helpful):
- Screenshot of outline structure in Windows Frontier
- Output of verification script showing all values

---

## Notes

1. **Why these specific values?**
   - `2147483647` = 2^31-1 (max 32-bit signed positive)
   - `-2147483648` = -2^31 (min 32-bit signed negative)
   - These are the edge cases most likely to expose truncation bugs

2. **Why dates before 2038?**
   - v6 uses 32-bit unsigned seconds since 1904
   - Dates after 2038 overflow 32-bit
   - We test that v7 can handle these correctly after widening

3. **Why nested outlines?**
   - Tests that refcon migration works recursively
   - Ensures child nodes aren't missed during migration

4. **Why nil refcon test case?**
   - Verifies that unset refcons don't get garbage values
   - Tests that migration handles optional refcons correctly

---

## Questions / Issues

If you encounter any issues creating this artifact, please note:
- Which test cases are problematic
- Any Windows Frontier limitations
- Alternative approaches you recommend

We can adjust the specification based on what's practical in the legacy app.
