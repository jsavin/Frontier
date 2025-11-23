#!/usr/bin/env python3
"""
extract_legacy_table.py

Utility for extracting the raw legacy (v6) table payload for a specific path
from a Frontier .root database. The output is a binary blob containing the
merged handle exactly as stored on disk (outer merge prefix + inner header/
records/strings + format blob).

Usage:
  python3 scripts/extract_legacy_table.py \
      --db databases/Frontier-v6.root \
      --path system.verbs.globals \
      --out planning/phase3/carbon_migration/data/system_verbs_globals_legacy.bin
"""

import argparse
from pathlib import Path
from typing import Dict, List, Tuple


EXTERNAL_VALUE_TYPE = 13


def read_be32(buf: bytes, offset: int) -> int:
    return int.from_bytes(buf[offset:offset + 4], byteorder="big")


def read_db_header(buf: bytes) -> Tuple[int, int]:
    version = buf[1]
    if version not in (6, 7):
        raise ValueError(f"Unsupported database version: {version}")
    pointer = read_be32(buf, 10) if version == 6 else int.from_bytes(buf[0x0E:0x16], byteorder="big")
    return version, pointer


def resolve_root_address(buf: bytes, version: int, pointer: int) -> int:
    if version == 6:
        _, payload = read_block(buf, pointer)
        if len(payload) < 6:
            raise ValueError("Cancoon record truncated")
        return read_be32(payload, 2)
    return pointer


def read_block(buf: bytes, addr: int) -> Tuple[int, bytes]:
    if addr + 12 > len(buf):
        raise ValueError(f"Address 0x{addr:08x} beyond file size")
    size = read_be32(buf, addr) & 0x7FFFFFFF
    payload_start = addr + 8
    payload_end = payload_start + size
    if payload_end + 4 > len(buf):
        raise ValueError(f"Block at 0x{addr:08x} overflows file")
    payload = buf[payload_start:payload_end]
    return size, payload


def read_pascal_string(strings: bytes, offset: int) -> str:
    if offset >= len(strings):
        return ""
    length = strings[offset]
    end = offset + 1 + length
    if end > len(strings):
        end = len(strings)
    return strings[offset + 1:end].decode("mac_roman", errors="replace")


def extract_external_address(strings: bytes, offset: int) -> int:
    if offset + 12 > len(strings):
        raise ValueError("External payload truncated")
    # Layout: [u32 length][u16 version][u8 tyexternalid][u8 flags][u32 dbaddress]...
    length = read_be32(strings, offset)
    if offset + 4 + length > len(strings):
        raise ValueError("External payload length exceeds string table")
    dbaddr = read_be32(strings, offset + 8)
    return dbaddr


def parse_table(payload: bytes) -> Dict:
    if len(payload) < 8:
        raise ValueError("Payload too small to contain merged handle")
    outer_size = read_be32(payload, 0)
    if outer_size + 4 > len(payload):
        raise ValueError("Outer merged handle truncated")
    inner = payload[4:4 + outer_size]
    if len(inner) < 4:
        raise ValueError("Inner merged handle truncated")
    inner_size = read_be32(inner, 0)
    if inner_size + 4 > len(inner):
        raise ValueError("Inner payload length mismatch")

    header = inner[4:4 + 16]
    records_bytes = inner[4 + 16:4 + inner_size]
    strings = inner[4 + inner_size:]

    if len(records_bytes) % 10 != 0:
        raise ValueError("Record section not aligned to 10-byte entries")

    entries = []
    for i in range(0, len(records_bytes), 10):
        rec = records_bytes[i:i + 10]
        ixkey = read_be32(rec, 0)
        valuetype = rec[4]
        version = rec[5]
        data_offset = read_be32(rec, 6)
        name = read_pascal_string(strings, ixkey)
        entries.append({
            "name": name,
            "valuetype": valuetype,
            "version": version,
            "data_offset": data_offset,
        })

    return {
        "header": header,
        "entries": entries,
        "strings": strings,
    }


def locate_table(buf: bytes, addr: int, components: List[str]) -> Tuple[int, bytes]:
    _, payload = read_block(buf, addr)
    if not components:
        return addr, payload

    table = parse_table(payload)
    target = components[0]

    for entry in table["entries"]:
        if entry["name"] != target:
            continue
        if entry["valuetype"] != EXTERNAL_VALUE_TYPE:
            raise ValueError(f"{target} exists at 0x{addr:08x} but is not a table")
        child_addr = extract_external_address(table["strings"], entry["data_offset"])
        return locate_table(buf, child_addr, components[1:])

    raise ValueError(f"Component '{target}' not found under table at 0x{addr:08x}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Extract legacy table payload")
    parser.add_argument("--db", required=True, help="Path to .root database (v6)")
    parser.add_argument("--path", required=True, help="Dot-delimited table path (e.g., system.verbs.globals)")
    parser.add_argument("--out", required=True, help="Output file for raw payload bytes")
    args = parser.parse_args()

    db_path = Path(args.db)
    data = db_path.read_bytes()
    version, pointer = read_db_header(data[:256])
    root_addr = resolve_root_address(data, version, pointer)
    if version != 6:
        raise ValueError(f"{db_path} is version {version}; expected v6 legacy database")

    components = args.path.split(".")
    addr, payload = locate_table(data, root_addr, components)

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(payload)

    print(f"Wrote {len(payload)} bytes from table '{args.path}' at 0x{addr:08x} to {out_path}")


if __name__ == "__main__":
    main()
