#!/usr/bin/env python3
"""
inspect_table_payload.py

Parse a single table block inside a Frontier .root file and dump the header,
records, and string pool entries. Useful for documenting the on-disk layout of
legacy (v6) and modern (v7) tables.

2025-11-08 Codex: Added script to make hexdump-backed table inspections repeatable.
"""

import argparse
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional

HEADER_SIZE = 16  # sizeof(tydisktablerecord)
RECORD_SIZE = 10  # sizeof(tydisksymbolrecord)

def read_block(data: bytes, offset: int) -> Optional[bytes]:
    """Return the block payload (without 8-byte header/trailer) at a given offset."""
    if offset < 0 or offset + 8 > len(data):
        return None
    size = struct.unpack_from('>I', data, offset)[0]
    variance = struct.unpack_from('>I', data, offset + 4)[0]
    payload_start = offset + 8
    payload_end = payload_start + size
    if payload_end + 4 > len(data):
        return None
    trailer = struct.unpack_from('>I', data, payload_end)[0]
    if trailer & 0x7FFFFFFF != size:
        return None
    return data[payload_start:payload_end]

@dataclass
class TableRecord:
    index: int
    name: str
    valuetype: int
    version: int
    data: int

def parse_table(payload: bytes) -> Optional[List[TableRecord]]:
    if len(payload) < 8:
        return None
    outer_size = struct.unpack_from('>I', payload, 0)[0]
    if outer_size == 0 or outer_size + 4 > len(payload):
        return None
    inner = payload[4:4+outer_size]
    if len(inner) < 4:
        return None
    inner_size = struct.unpack_from('>I', inner, 0)[0]
    if inner_size + 4 > len(inner):
        return None
    header = inner[4:4+HEADER_SIZE]
    records = inner[4+HEADER_SIZE:4+inner_size]
    strings = inner[4+inner_size:]
    result: List[TableRecord] = []
    for i in range(0, len(records), RECORD_SIZE):
        rec = records[i:i+RECORD_SIZE]
        if len(rec) < RECORD_SIZE:
            break
        ixkey = struct.unpack_from('>I', rec, 0)[0]
        valuetype = rec[4]
        version = rec[5]
        data = struct.unpack_from('>I', rec, 6)[0]
        if ixkey == 0 and valuetype == 0 and version == 0 and data == 0:
            continue
        name = f'<ix {ixkey}>'
        if ixkey < len(strings):
            strlen = strings[ixkey]
            if ixkey + 1 + strlen <= len(strings):
                raw = strings[ixkey+1:ixkey+1+strlen]
                try:
                    name = raw.decode('mac_roman')
                except UnicodeDecodeError:
                    name = raw.decode('latin1', errors='ignore')
        result.append(TableRecord(i // RECORD_SIZE, name, valuetype, version, data))
    return result

def main() -> None:
    parser = argparse.ArgumentParser(description="Inspect a Frontier table payload")
    parser.add_argument('database', help=".root file to read")
    parser.add_argument('--offset', type=lambda x: int(x, 0), required=True,
                        help="byte offset of the block header (e.g. 0x5d0ee9)")
    args = parser.parse_args()

    data = Path(args.database).read_bytes()
    payload = read_block(data, args.offset)
    if payload is None:
        raise SystemExit(f"No block payload at offset 0x{args.offset:x}")

    records = parse_table(payload)
    if records is None:
        raise SystemExit("Block did not parse as a table")

    print(f"Block 0x{args.offset:x} contains {len(records)} records")
    for rec in records:
        print(f"  [{rec.index}] name={rec.name!r} type={rec.valuetype} version={rec.version} data=0x{rec.data:08x}")

if __name__ == '__main__':
    main()
