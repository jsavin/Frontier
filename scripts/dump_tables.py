#!/usr/bin/env python3
"""
Utility to enumerate table-like blocks inside a Frontier .root database.
Used for diagnosing how legacy root placeholders map to modern tables.
"""

import argparse
import struct
from pathlib import Path

LEGACY_HEADER_SIZE = 16
LEGACY_RECORD_SIZE = 10


def read_blocks(data):
    """Yield (offset, size, free_flag, variance, payload) for each block."""
    offset = 0x58  # legacy v6 header length
    filesize = len(data)
    while offset + 12 <= filesize:
        raw_size = struct.unpack('>I', data[offset:offset + 4])[0]
        free = (raw_size & 0x80000000) != 0
        size = raw_size & 0x7FFFFFFF
        variance = struct.unpack('>I', data[offset + 4:offset + 8])[0]
        payload_start = offset + 8
        payload_end = payload_start + size
        if payload_end + 4 > filesize:
            break
        payload = data[payload_start:payload_end]
        trailer = struct.unpack('>I', data[payload_end:payload_end + 4])[0]
        if (trailer & 0x7FFFFFFF) != size:
            break
        yield offset, size, free, variance, payload
        offset = payload_end + 4

def try_parse_modern(payload):
    if len(payload) < 8:
        return None
    outer_size = struct.unpack('>I', payload[:4])[0]
    if outer_size + 4 > len(payload) or outer_size == 0:
        return None
    inner = payload[4:4 + outer_size]
    if len(inner) < 4:
        return None
    inner_size = struct.unpack('>I', inner[:4])[0]
    if inner_size + 4 > len(inner):
        return None
    records = inner[4 + LEGACY_HEADER_SIZE:4 + inner_size]
    strings = inner[4 + inner_size:]
    entries = []
    for i in range(0, len(records), LEGACY_RECORD_SIZE):
        rec = records[i:i + LEGACY_RECORD_SIZE]
        if len(rec) < LEGACY_RECORD_SIZE:
            break
        ixkey = struct.unpack('>I', rec[:4])[0]
        valuetype = rec[4]
        version = rec[5]
        dataval = struct.unpack('>I', rec[6:10])[0]
        if ixkey == 0 and valuetype == 0 and version == 0 and dataval == 0:
            continue
        name = None
        if ixkey < len(strings):
            strlen = strings[ixkey]
            if ixkey + 1 + strlen <= len(strings):
                raw = strings[ixkey + 1:ixkey + 1 + strlen]
                try:
                    name = raw.decode('mac_roman')
                except Exception:
                    name = raw.decode('latin1', errors='replace')
        entries.append((name or f"<ix {ixkey}>", valuetype, dataval))
    return entries

def main():
    ap = argparse.ArgumentParser(description="Dump modern tables in a Frontier database")
    ap.add_argument("database", help=".root file to inspect")
    args = ap.parse_args()

    data = Path(args.database).read_bytes()
    for offset, size, free_flag, variance, payload in read_blocks(data):
        modern_entries = try_parse_modern(payload)
        if modern_entries:
            print(f"Block 0x{offset:04x} size={size} entries={len(modern_entries)}")
            for name, vtype, dataval in modern_entries:
                print(f"  - {name!r} type={vtype} dataval=0x{dataval:08x}")
            print()

if __name__ == "__main__":
    main()
