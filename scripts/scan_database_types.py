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
EXTERNAL_TYPES = {
    0: 'outline',
    1: 'wptext',
    2: 'table',
    3: 'script',
    4: 'menu',
    5: 'pict',
}

class DatabaseScanner:
    def __init__(self, db_path):
        self.db_path = Path(db_path)
        self.type_counts = Counter()
        self.external_type_counts = Counter()
        self.type_locations = defaultdict(list)

    def scan(self):
        """Scan the database and catalog types."""
        print(f"Scanning {self.db_path}...")

        with open(self.db_path, 'rb') as f:
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
        # Extract root table address from v6 header (offset 10, 4 bytes big-endian)
        root_addr = struct.unpack('>I', header[10:14])[0]
        print(f"Root table at: 0x{root_addr:08x}")

        if root_addr == 0:
            print("No root table found")
            return

        # Read root table
        self._scan_table_at(f, root_addr, "root", is_v6=True)

    def _scan_v7(self, f, header):
        """Scan v7 database format."""
        # Extract root table address from v7 header
        # views[0] is at offset 512+32 in 64-bit header
        f.seek(544)
        root_addr = struct.unpack('<Q', f.read(8))[0]
        print(f"Root table at: 0x{root_addr:08x}")

        if root_addr == 0:
            print("No root table found")
            return

        # Read root table
        self._scan_table_at(f, root_addr, "root", is_v6=False)

    def _scan_table_at(self, f, addr, path, is_v6):
        """Scan a table at the given address."""
        try:
            # Read block header (8 bytes: size + variance)
            f.seek(addr)
            size_bytes, variance_bytes = struct.unpack('>II', f.read(8))

            # Read payload
            payload = f.read(size_bytes)

            # Check if legacy format (no merge prefix)
            if len(payload) >= 4:
                first_bytes = struct.unpack('>I', payload[:4])[0]
                is_legacy = first_bytes < 16 or first_bytes > (len(payload) - 4)

                if is_legacy:
                    # Legacy format: [header][strings][records]
                    self._scan_legacy_table_payload(payload, path)
                else:
                    # Modern format: [outer_size][inner_merged][formats]
                    self._scan_modern_table_payload(payload, path)

        except Exception as e:
            print(f"Error scanning table at {path}: {e}")

    def _scan_legacy_table_payload(self, payload, path):
        """Scan legacy table format."""
        # Skip header (16 bytes)
        # Find records section (after strings, 10 bytes per record)
        # For now, just note we found a table
        self.type_counts['table'] += 1
        self.type_locations['table'].append(path)

        # TODO: Parse records to find value types of entries
        # This requires finding the strings/records split point
        print(f"  Found legacy table: {path}")

    def _scan_modern_table_payload(self, payload, path):
        """Scan modern merged table format."""
        self.type_counts['table'] += 1
        self.type_locations['table'].append(path)

        # TODO: Unmerge and parse records
        print(f"  Found modern table: {path}")

    def _report(self):
        """Generate report of findings."""
        print("\n" + "="*60)
        print("SCAN RESULTS")
        print("="*60)

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
            print("  No complex types found (incomplete scan)")

def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <database.root>")
        sys.exit(1)

    db_path = sys.argv[1]
    if not Path(db_path).exists():
        print(f"Error: {db_path} not found")
        sys.exit(1)

    scanner = DatabaseScanner(db_path)
    scanner.scan()

if __name__ == '__main__':
    main()
