# v6 to v7 Database Migration: Known Gaps

Status
- State: Planning
- Phase: 3
- Last Updated: 2025-10-22
- Notes: Catalog of external value types requiring conversion logic for v6→v7 migration

## Overview

The v6→v7 database format migration involves:
1. **Header conversion** (✅ Complete - see db_format.c)
2. **Table payload conversion** (✅ Complete - see tableexternal_common.c)
3. **Other external type conversions** (❌ TODO - documented below)

## Completed Conversions

### ✅ Table Type (tablevaluetype = externalvaluetype with idtableprocessor)
- **Status**: Complete
- **Implementation**: `Common/source/tableexternal_common.c:headless_convert_legacy_table_payload()`
- **Format**: Converts legacy `[header][strings][records]` to modern `[outer_size][inner_merged][formats]`
- **Tests**: Validated via system root loading in frontier-cli

## Required Conversions

### 🔴 Script Type (scriptvaluetype = externalvaluetype with idoutlineprocessor + flscript flag)
**Priority**: HIGH - System tables contain many scripts

**Current Status**: Not implemented

**v6 Format**:
- Outline external with script metadata
- May include compiled code attachment (`codevaluetype`)

**v7 Format**:
- Preserve outline structure and source text
- **Drop compiled code** (JIT compilation on first call)
- Keep script metadata (timestamps, attributes)

**Migration Strategy**:
1. Parse v6 script external structure
2. Extract outline hierarchy and source text
3. Discard any `codevaluetype` attachment
4. Write v7 script external (no compiled code)

**Challenges**:
- Outline external format may use legacy serialization
- Need to preserve script outline structure correctly
- Source text may be in outline nodes or separate

**Test Cases Needed**:
- Simple script with no compiled code
- Script with compiled code (discard it)
- Script with complex outline structure
- Script with metadata (timestamps, etc.)

---

### 🔴 Outline Type (outlinevaluetype = externalvaluetype with idoutlineprocessor)
**Priority**: HIGH - Used by scripts and user data

**Current Status**: Not implemented

**v6 Format**:
- Hierarchical outline structure
- Each node: text, attributes, refcon (binary blob)
- May use Pascal-era serialization

**v7 Format**:
- Same logical structure
- Modern serialization format
- Preserve all node metadata

**Migration Strategy**:
1. Parse v6 outline structure
2. Traverse node hierarchy
3. Convert each node's data to v7 format
4. Preserve refcon data, attributes, structure

**Challenges**:
- Complex nested structure
- Refcon data is opaque binary
- Need to preserve exact node relationships
- Potential format differences in node serialization

**Test Cases Needed**:
- Simple flat outline
- Deeply nested outline
- Outline with refcon data
- Outline with various node attributes

---

### 🔴 WPText Type (wordvaluetype = externalvaluetype with idwpprocessor)
**Priority**: MEDIUM - Some documentation/text may use this

**Current Status**: Not implemented

**v6 Format**:
- Mac Toolbox rich text format
- 32KB size limit
- 8-bit ASCII only
- Platform-specific binary format

**v7 Format Options**:
1. Keep Mac Toolbox format (preserves fidelity, limits portability)
2. Convert to RTF (cross-platform, may lose some formatting)
3. Convert to plain text + metadata (simple, loses formatting)

**Migration Strategy** (TBD - needs design decision):
- Parse Mac Toolbox rich text structure
- Extract text content and formatting
- Convert to chosen v7 format
- Handle size limits and encoding

**Challenges**:
- Mac Toolbox format is complex and poorly documented
- 32KB limit may truncate data
- 8-bit ASCII doesn't support Unicode
- Formatting preservation vs. portability tradeoff
- **Major modernization effort** - see planning/phase3/cli_usertalk_invocation_plan.md

**Test Cases Needed**:
- Plain text in WPText wrapper
- Formatted text (fonts, styles)
- Text at/near 32KB limit
- Text with special characters

---

### 🟡 List Type (listvaluetype)
**Priority**: HIGH - Can contain ANY value type recursively

**Current Status**: Not implemented

**v6 Format**:
- Array of values
- Each element can be any value type
- Including nested lists, records, externals

**v7 Format**:
- Same logical structure
- Modern serialization format

**Migration Strategy**:
1. Parse v6 list structure
2. For each element:
   - Identify element type
   - Recursively convert element
   - Handle external types properly
3. Write v7 list structure

**Challenges**:
- **Recursive type system**: Lists can contain lists, records, externals
- Each element may need conversion
- Need to handle all value types in element conversion
- Potential for deep nesting

**Test Cases Needed**:
- List of scalars (ints, strings, etc.)
- List containing lists
- List containing records
- List containing external types (tables, scripts, etc.)
- Mixed-type list
- Deeply nested list structure

---

### 🟡 Record Type (recordvaluetype)
**Priority**: HIGH - Can contain ANY value type recursively

**Current Status**: Not implemented

**v6 Format**:
- Key-value map
- Each value can be any value type
- Including nested records, lists, externals

**v7 Format**:
- Same logical structure
- Modern serialization format

**Migration Strategy**:
1. Parse v6 record structure
2. For each key-value pair:
   - Identify value type
   - Recursively convert value
   - Handle external types properly
3. Write v7 record structure

**Challenges**:
- **Recursive type system**: Records can contain records, lists, externals
- Each value may need conversion
- Need to handle all value types in value conversion
- Potential for deep nesting
- Key encoding (string format)

**Test Cases Needed**:
- Record with scalar values
- Record containing records
- Record containing lists
- Record containing external types
- Mixed-type record values
- Deeply nested record structure

---

### 🟢 Menu Type (menuvaluetype = externalvaluetype with idmenuprocessor)
**Priority**: LOW - Headless doesn't use menus; may be absent

**Current Status**: Not implemented

**v6 Format**:
- Mac menu bar structure
- Menu items, shortcuts, hierarchies

**v7 Format**:
- TBD - May skip for headless
- Could preserve as data structure

**Migration Strategy** (TBD):
- May be safe to skip for headless use cases
- Or preserve as generic external data

**Test Cases Needed**:
- Only if menus found in actual databases

---

### 🟢 Pict Type (pictvaluetype = externalvaluetype with idpictprocessor)
**Priority**: LOW - Headless doesn't render pictures; may be absent

**Current Status**: Not implemented

**v6 Format**:
- Mac PICT format
- Platform-specific

**v7 Format**:
- TBD - May skip for headless
- Could preserve as binary blob

**Migration Strategy** (TBD):
- May be safe to skip for headless use cases
- Or preserve as generic external data

**Test Cases Needed**:
- Only if picts found in actual databases

---

## Scalar Types (Already Compatible)

The following scalar types use simple serialization that's compatible between v6 and v7:
- ✅ novaluetype
- ✅ charvaluetype
- ✅ intvaluetype
- ✅ longvaluetype
- ✅ binaryvaluetype
- ✅ booleanvaluetype
- ✅ tokenvaluetype
- ✅ datevaluetype
- ✅ addressvaluetype
- ✅ doublevaluetype
- ✅ stringvaluetype
- ✅ directionvaluetype
- ✅ passwordvaluetype
- ✅ ostypevaluetype
- ✅ pointvaluetype
- ✅ rectvaluetype
- ✅ patternvaluetype
- ✅ rgbvaluetype
- ✅ fixedvaluetype
- ✅ singlevaluetype
- ✅ objspecvaluetype
- ✅ filespecvaluetype
- ✅ aliasvaluetype
- ✅ enumvaluetype

Note: `codevaluetype` (compiled scripts) is explicitly **dropped** in v7.

## Implementation Phases

### Phase 1: Database Survey (Current)
- ✅ Create scanner tool
- 🔄 Scan Frontier.root to identify actual types present
- 🔄 Prioritize based on frequency and criticality

### Phase 2: Core External Types
1. Script type (used throughout system tables)
2. Outline type (used by scripts and data)
3. List type (recursive, used widely)
4. Record type (recursive, used widely)

### Phase 3: Specialized Types (As Needed)
1. WPText (if found in databases)
2. Menu (if needed for migration completeness)
3. Pict (if needed for migration completeness)

### Phase 4: Integration & Testing
- Comprehensive migration tests
- Round-trip validation
- Real-world database migration testing

## Tools Needed

### 🔄 Database Type Scanner (In Progress)
**Purpose**: Identify which types are actually present in v6 databases
**Location**: `scripts/scan_database_types.py`
**Status**: Basic framework exists; needs enhancement to:
- Parse table records recursively
- Identify all value types in nested structures
- Report frequency and nesting depth
- Identify which external types are used

### ❌ Migration Test Suite (TODO)
**Purpose**: Validate each type conversion
**Location**: `tests/components/test_external_migrations.c`
**Coverage Needed**:
- Unit tests for each external type converter
- Integration tests with real v6 data
- Round-trip tests (v6 → v7 → verify)

### ❌ Format Documentation (TODO)
**Purpose**: Document byte-level formats for each external type
**Location**: `docs/external_value_formats.md`
**Contents**:
- v6 serialization format for each type
- v7 serialization format for each type
- Mapping/conversion rules

## Risk Assessment

### High Risk Areas
1. **Recursive types (lists, records)**: Must handle arbitrary nesting and all contained types
2. **WPText**: Complex Mac-specific format, modernization effort
3. **Scripts**: Must preserve outline structure and source correctly
4. **Outlines**: Complex hierarchy with binary refcon data

### Medium Risk Areas
1. **Menu/Pict types**: May not be needed for headless, unclear if present

### Low Risk Areas
1. **Scalar types**: Already compatible
2. **Tables**: Conversion complete and tested

## Success Criteria

- [ ] All external types found in Frontier.root have converters
- [ ] All converters have unit tests
- [ ] Real-world v6 database migrates successfully
- [ ] Migrated data loads and executes correctly in v7
- [ ] Round-trip validation passes
- [ ] No data loss or corruption

## References

- Header migration: `Common/source/db_format.c`
- Table migration: `Common/source/tableexternal_common.c`
- Value type definitions: `Common/headers/lang.h`
- External type IDs: `Common/headers/langexternal.h`
- Legacy bootstrap: `docs/legacy_frontier_bootstrap.md`
- Overall plan: `planning/phase3/frontier_root_headless_plan.md`
