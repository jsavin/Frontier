#!/usr/bin/env python3
"""
Scan a Frontier database and catalog all value types present.
Used to prioritize v6->v7 migration work based on actual usage.
"""

import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path

# Value type enum from Common/headers/lang.h
VALUE_TYPES = {
    -1: 'uninitialized',
    0: 'noval',
    1: 'char',
    2: 'int',
    3: 'long',
    4: 'oldstring',
    5: 'binary',
    6: 'boolean',
    7: 'token',
    8: 'date',
    9: 'address',
    10: 'code',
    11: 'double',
    12: 'string',
    13: 'external',
    14: 'direction',
    15: 'password',
    16: 'ostype',
    17: 'unused2',
    18: 'point',
    19: 'rect',
    20: 'pattern',
    21: 'rgb',
    22: 'fixed',
    23: 'single',
    24: 'olddouble',
    25: 'objspec',
    26: 'filespec',
    27: 'alias',
    28: 'enum',
    29: 'list',
    30: 'record',
}

# External type IDs (for externalvaluetype)
# From langexternal.h tyexternalid enum
EXTERNAL_TYPES = {
    0: 'outline',
    1: 'wptext',
    2: 'headrecord',
    3: 'table',
    4: 'script',
    5: 'menu',
    6: 'pict',
}

class DatabaseScanner:
    def __init__(self, db_path, deep=False, max_depth=10):
        self.db_path = Path(db_path)
        self.type_counts = Counter()
        self.external_type_counts = Counter()
        self.type_locations = defaultdict(list)
        self.max_depth = 0
        self.depth_histogram = Counter()
        self.file = None
        self.deep = deep  # Enable deep traversal following external references
        self.max_depth_limit = max_depth  # Prevent infinite recursion
        self.visited_addrs = set()  # Track visited addresses to prevent loops
        self.total_entries = 0  # Total entries scanned across all tables

    def scan(self):
        """Scan the database and catalog types."""
        print(f"Scanning {self.db_path}...")

        with open(self.db_path, 'rb') as f:
            self.file = f
            # Read header
            header = f.read(256)
            version = header[1]

            print(f"Database version: {version}")

            if version == 6:
                self._scan_v6(f, header)
            elif version == 7:
                self._scan_v7(f, header)
            else:
                print(f"Unknown version: {version}")
                return

        self._report()

    def _scan_v6(self, f, header):
        """Scan v6 database format."""
        # In v6 format, root table address is at offset 10 (4 bytes big-endian)
        # This is the legacy location before the views array was used
        root_addr = struct.unpack('>I', header[10:14])[0]
        print(f"Root table at: 0x{root_addr:08x} (from offset 10)")

        if root_addr == 0:
            print("No root table found")
            return

        # Read root table
        self._scan_table_at(f, root_addr, "root", depth=0)

    def _scan_v7(self, f, header):
        """Scan v7 database format."""
        # Extract root table address from v7 header
        # views[0] is at offset 0x0E (14), stored as big-endian 64-bit value
        view_base = 0x0E
        root_addr_bytes = header[view_base:view_base + 8]
        root_addr = struct.unpack('>Q', root_addr_bytes)[0]
        print(f"Root table at: 0x{root_addr:08x}")

        if root_addr == 0:
            print("No root table found")
            return

        # Read root table
        self._scan_table_at(f, root_addr, "root", depth=0)

    def _scan_table_at(self, f, addr, path, depth):
        """Scan a table at the given address."""
        # Check depth limit
        if depth > self.max_depth_limit:
            if self.deep:
                print(f"{'  ' * depth}[{path}] Max depth reached, stopping")
            return

        # Check if already visited (prevent infinite loops)
        if addr in self.visited_addrs:
            if self.deep:
                print(f"{'  ' * depth}[{path}] Already visited @ 0x{addr:08x}, skipping")
            return

        self.visited_addrs.add(addr)

        try:
            # Track depth
            self.max_depth = max(self.max_depth, depth)
            self.depth_histogram[depth] += 1

            # Read block header (8 bytes: size + variance)
            f.seek(addr)
            size_bytes, variance_bytes = struct.unpack('>II', f.read(8))

            # Read payload
            payload = f.read(size_bytes)

            # Check if legacy format (no merge prefix)
            if len(payload) >= 4:
                first_bytes = struct.unpack('>I', payload[:4])[0]
                is_legacy = first_bytes < 16 or first_bytes > (len(payload) - 4)

                format_type = "legacy" if is_legacy else "modern"
                print(f"  [{path}] Detected {format_type} table format (first_bytes=0x{first_bytes:08x}, payload_len={len(payload)})")

                if is_legacy:
                    # Legacy format: [header][strings][records]
                    self._scan_legacy_table_payload(payload, path, depth, f, addr)
                else:
                    # Modern format: [outer_size][inner_merged][formats]
                    self._scan_modern_table_payload(payload, path, depth, f)

        except Exception as e:
            print(f"  Error scanning table at {path} (depth {depth}): {e}")

    def _scan_legacy_table_payload(self, payload, path, depth, f, addr):
        """Scan legacy table format and extract value types.

        Based on hashunpacktable() in langhash.c, the structure is:
        - payload contains: [header][records...][strings]
        - Records are sequential, not hash buckets
        - Sentinel records (all zeros) are skipped
        - String pool comes after records
        """
        LEGACY_HEADER_SIZE = 16  # sizeof(tydisktablerecord)
        LEGACY_RECORD_SIZE = 10  # sizeof(tydisksymbolrecord)

        self.type_counts['table'] += 1

        if len(payload) < LEGACY_HEADER_SIZE + LEGACY_RECORD_SIZE:
            return

        # Read header to understand table structure
        header = payload[:LEGACY_HEADER_SIZE]
        # Header structure (from tablestructure.h):
        # - short versionnumber (2 bytes)
        # - short sortorder (2 bytes)
        # - long timecreated (4 bytes)
        # - long timemodified (4 bytes)
        # - long flags (4 bytes)

        # Find the split between strings and records by trying different candidates
        # Format is: [header][strings][records]
        # We try different string pool sizes and validate each one

        strings_len = 0
        records_start = 0
        valid_records = []

        # Try different string pool sizes (must be 10-byte aligned)
        for candidate_strings in range(0, len(payload) - LEGACY_HEADER_SIZE + 1, LEGACY_RECORD_SIZE):
            candidate_records_start = LEGACY_HEADER_SIZE + candidate_strings

            # Must have room for at least one record
            if candidate_records_start + LEGACY_RECORD_SIZE > len(payload):
                break

            # Validate this candidate by checking all records
            valid = True
            found_sentinel = False
            candidate_records = []

            for rec_offset in range(candidate_records_start, len(payload), LEGACY_RECORD_SIZE):
                if rec_offset + LEGACY_RECORD_SIZE > len(payload):
                    break

                rec = payload[rec_offset:rec_offset + LEGACY_RECORD_SIZE]
                ixkey = struct.unpack('>I', rec[0:4])[0]
                valuetype = rec[4]
                version_byte = rec[5]
                dataval = struct.unpack('>I', rec[6:10])[0]

                # Check for sentinel
                if ixkey == 0 and valuetype == 0 and version_byte == 0 and dataval == 0:
                    found_sentinel = True
                    continue  # Sentinels are valid, keep checking

                # ixkey must be valid offset into string pool
                if ixkey >= candidate_strings:
                    valid = False
                    break

                # This is a valid record for this candidate
                candidate_records.append((rec_offset, ixkey, valuetype, version_byte, dataval))

            # Accept this candidate if it has valid records and at least one sentinel
            if valid and found_sentinel and (len(candidate_records) > 0 or candidate_strings > 0):
                strings_len = candidate_strings
                records_start = candidate_records_start
                valid_records = candidate_records
                break

        if records_start == 0:
            print(f"  [{path}] Failed to find valid strings/records split")
            return

        if not self.deep:
            print(f"  [{path}] Found {len(valid_records)} valid records, strings={strings_len} bytes, records start at offset {records_start}")
        else:
            indent = '  ' * depth
            print(f"{indent}[{path}] Table @ 0x{addr:08x} ({len(valid_records)} entries)")

        # Extract string pool (strings are between header and records)
        strings = payload[LEGACY_HEADER_SIZE:LEGACY_HEADER_SIZE + strings_len]

        # Second pass: process each record
        record_count = 0
        for rec_offset, ixkey, valuetype, version_byte, dataval in valid_records:
            record_count += 1
            self.total_entries += 1

            # Track this value type
            type_name = VALUE_TYPES.get(valuetype, f'unknown_{valuetype}')
            self.type_counts[type_name] += 1

            # Extract key name from string pool
            # ixkey is offset into the strings section
            if ixkey < len(strings):
                # Pascal string: length byte followed by characters
                str_len = strings[ixkey]
                if ixkey + 1 + str_len <= len(strings):
                    key_bytes = strings[ixkey + 1:ixkey + 1 + str_len]
                    try:
                        key_name = key_bytes.decode('ascii', errors='ignore')
                    except:
                        key_name = '???'
                else:
                    key_name = '(truncated)'
            else:
                key_name = '(out of bounds)'

            # Show entry in tree format if deep mode
            if self.deep:
                indent = '  ' * (depth + 1)
                print(f"{indent}├─ {key_name!r} [{type_name}] = 0x{dataval:08x}")
            else:
                print(f"  [{path}] Record {record_count}: key={key_name!r} type={type_name} dataval=0x{dataval:08x}")

            # For external types, try to determine which external type and recurse if deep mode
            if valuetype == 13:  # externalvaluetype
                # dataval is the dbaddress of the external value
                if dataval != 0:
                    self._scan_external_at(f, dataval, f"{path}.{key_name}", depth, key_name)

            # TODO: Handle list (29) and record (30) types when we understand their format

    def _scan_modern_table_payload(self, payload, path, depth, f):
        """Scan modern merged table format."""
        self.type_counts['table'] += 1

        # Modern format: [outer_size][inner_merged][formats]
        # First 4 bytes is outer_size
        if len(payload) < 4:
            return

        outer_size = struct.unpack('>I', payload[0:4])[0]
        if outer_size + 4 > len(payload):
            return

        # Inner merged: [inner_size][header+records][strings]
        inner_merged = payload[4:4 + outer_size]
        if len(inner_merged) < 4:
            return

        inner_size = struct.unpack('>I', inner_merged[0:4])[0]
        if inner_size + 4 > len(inner_merged):
            return

        header_and_records = inner_merged[4:4 + inner_size]
        strings = inner_merged[4 + inner_size:]

        # Parse records from header_and_records
        LEGACY_HEADER_SIZE = 16
        LEGACY_RECORD_SIZE = 10

        if len(header_and_records) < LEGACY_HEADER_SIZE:
            return

        records = header_and_records[LEGACY_HEADER_SIZE:]

        # Parse each record
        for rec_offset in range(0, len(records), LEGACY_RECORD_SIZE):
            if rec_offset + LEGACY_RECORD_SIZE > len(records):
                break

            rec = records[rec_offset:rec_offset + LEGACY_RECORD_SIZE]
            ixkey = struct.unpack('>I', rec[0:4])[0]
            valuetype = rec[4]
            dataval = struct.unpack('>I', rec[6:10])[0]

            # Sentinel record
            if ixkey == 0 and valuetype == 0 and dataval == 0:
                continue

            # Track this value type
            type_name = VALUE_TYPES.get(valuetype, f'unknown_{valuetype}')
            self.type_counts[type_name] += 1

            # For external types, try to determine which external type
            if valuetype == 13:  # externalvaluetype
                if dataval != 0:
                    self._scan_external_at(f, dataval, path, depth, '')

    def _scan_external_at(self, f, addr, path, depth, key_name=''):
        """Scan an external value to determine its type."""
        try:
            # Read external value header
            # External values are stored on disk with a block header, then the external data
            saved_pos = f.tell()
            f.seek(addr)

            # Read block header (8 bytes)
            size_bytes, variance_bytes = struct.unpack('>II', f.read(8))

            # Read full external value data
            ext_data = f.read(size_bytes)

            if len(ext_data) < 16:  # Need at least header: version(2) + id(2) + flags(2) + variabledata(4) + ...
                f.seek(saved_pos)
                return

            # tydiskexternalhandle structure:
            # - short versionnumber (2 bytes)
            # - tyexternalid id (2 bytes, but really a short in the enum)
            versionnumber = struct.unpack('>H', ext_data[0:2])[0]
            idprocessor = struct.unpack('>H', ext_data[2:4])[0]

            external_type = EXTERNAL_TYPES.get(idprocessor, f'unknown_external_{idprocessor}')
            self.external_type_counts[external_type] += 1

            # If it's a table and deep mode is enabled, recurse into it
            if idprocessor == 3 and self.deep:  # idtableprocessor
                # Table external format on disk: [versionnumber][id][table_payload]
                # The table_payload is what we need to scan
                table_payload = ext_data[4:]  # Skip version(2) + id(2)

                # Check if this is a legacy or modern table payload
                if len(table_payload) >= 4:
                    first_bytes = struct.unpack('>I', table_payload[:4])[0]
                    is_legacy = first_bytes < 16 or first_bytes > (len(table_payload) - 4)

                    if is_legacy:
                        self._scan_legacy_table_payload(table_payload, f"{path}.<table>", depth + 1, f)
                    else:
                        self._scan_modern_table_payload(table_payload, f"{path}.<table>", depth + 1, f)

            f.seek(saved_pos)

        except Exception as e:
            print(f"  Error scanning external at 0x{addr:08x}: {e}")

    def _report(self):
        """Generate report of findings."""
        print("\n" + "="*60)
        print("SCAN RESULTS")
        print("="*60)

        print(f"\nTotal entries scanned: {self.total_entries}")
        print(f"Total tables visited: {len(self.visited_addrs)}")
        print(f"Max nesting depth: {self.max_depth}")

        print("\nDepth histogram:")
        for d in sorted(self.depth_histogram.keys()):
            print(f"  Depth {d:2d}: {self.depth_histogram[d]:5d} tables")

        print("\nValue Types Found:")
        for vtype, count in sorted(self.type_counts.items(), key=lambda x: -x[1]):
            print(f"  {vtype:20s}: {count:5d}")

        if self.external_type_counts:
            print("\nExternal Types Found:")
            for etype, count in sorted(self.external_type_counts.items(), key=lambda x: -x[1]):
                print(f"  {etype:20s}: {count:5d}")

        print("\nPriority Migration Targets:")
        complex_types = ['external', 'list', 'record', 'table']
        found_complex = [(t, self.type_counts[t]) for t in complex_types if self.type_counts[t] > 0]

        if found_complex:
            for vtype, count in sorted(found_complex, key=lambda x: -x[1]):
                print(f"  {vtype:20s}: {count:5d} occurrences")
        else:
            print("  No complex types found")

        # Migration recommendations
        print("\n" + "="*60)
        print("MIGRATION PRIORITIES")
        print("="*60)

        if self.external_type_counts:
            print("\nExternal type converters needed (by frequency):")
            for etype, count in sorted(self.external_type_counts.items(), key=lambda x: -x[1]):
                priority = "HIGH" if count > 100 else "MEDIUM" if count > 10 else "LOW"
                print(f"  [{priority:6s}] {etype:20s}: {count:5d} occurrences")

def main():
    import argparse

    parser = argparse.ArgumentParser(
        description='Scan a Frontier database and catalog all value types present.'
    )
    parser.add_argument('database', help='Path to database file (e.g., Frontier.root)')
    parser.add_argument('--deep', action='store_true',
                        help='Enable deep traversal following external table references')
    parser.add_argument('--max-depth', type=int, default=10,
                        help='Maximum recursion depth for deep traversal (default: 10)')

    args = parser.parse_args()

    if not Path(args.database).exists():
        print(f"Error: {args.database} not found")
        sys.exit(1)

    scanner = DatabaseScanner(args.database, deep=args.deep, max_depth=args.max_depth)
    scanner.scan()

if __name__ == '__main__':
    main()
