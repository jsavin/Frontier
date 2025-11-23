#!/usr/bin/env python3
"""
extract_wordprocessor_blob.py

Traverse a Frontier v6 database, locate word processor externals (Paige blobs),
and dump the raw handle for a selected entry.

Usage:
    python3 scripts/extract_wordprocessor_blob.py \
        --db databases/Frontier-v6-v7.root \
        --path system.misc.about \
        --out tests/fixtures/wptext/hello_macroman.bin

If --path is omitted, the first discovered word processor blob is dumped and the
full list is printed for reference.
"""

from __future__ import annotations

import argparse
from pathlib import Path
from typing import Dict, List, Tuple

EXTERNAL_VALUE_TYPE = 13
ID_OUTLINE_PROCESSOR = 0
ID_WORDPROCESSOR = 1
ID_TABLEPROCESSOR = 3


def read_be32(buf: bytes, offset: int) -> int:
    return int.from_bytes(buf[offset:offset + 4], byteorder="big")


def read_db_header(buf: bytes) -> Tuple[int, int]:
    version = buf[1]
    if version not in (6, 7):
        raise ValueError(f"Unsupported database version: {version}")
    pointer = read_be32(buf, 10) if version == 6 else int.from_bytes(buf[0x0E:0x16], byteorder="big")
    return version, pointer


def read_block(buf: bytes, addr: int) -> Tuple[int, bytes]:
    if addr + 12 > len(buf):
        raise ValueError(f"Address 0x{addr:08X} beyond file size")
    size = read_be32(buf, addr) & 0x7FFFFFFF
    payload_start = addr + 8
    payload_end = payload_start + size
    if payload_end + 4 > len(buf):
        raise ValueError(f"Block at 0x{addr:08X} overflows file")
    payload = buf[payload_start:payload_end]
    return size, payload


def read_pascal_string(strings: bytes, offset: int) -> str:
    if offset >= len(strings):
        return ""
    length = strings[offset]
    end = min(offset + 1 + length, len(strings))
    return strings[offset + 1:end].decode("mac_roman", errors="replace")


def parse_table(payload: bytes) -> Dict:
    if len(payload) < 8:
        raise ValueError("Payload too small to contain merged handle")
    outer_size = read_be32(payload, 0)
    inner = payload[4:4 + outer_size]
    if len(inner) < 4:
        raise ValueError("Inner merged handle truncated")
    inner_size = read_be32(inner, 0)
    header = inner[4:4 + 16]
    records_bytes = inner[4 + 16:4 + inner_size]
    strings = inner[4 + inner_size:]

    entries = []
    if len(records_bytes) % 10 != 0:
        raise ValueError("Record section not aligned to 10-byte entries")

    for i in range(0, len(records_bytes), 10):
        rec = records_bytes[i:i + 10]
        ixkey = read_be32(rec, 0)
        valuetype = rec[4]
        data_offset = read_be32(rec, 6)
        name = read_pascal_string(strings, ixkey)
        entries.append({
            "name": name,
            "valuetype": valuetype,
            "data_offset": data_offset,
        })

    return {
        "header": header,
        "entries": entries,
        "strings": strings,
    }


def parse_external_metadata(strings: bytes, offset: int) -> Tuple[int, int, int, int, int]:
    if offset + 12 > len(strings):
        raise ValueError("External payload truncated")
    length = read_be32(strings, offset)
    version = int.from_bytes(strings[offset + 4:offset + 6], byteorder="big")
    ext_id = strings[offset + 6]
    flags = strings[offset + 7]
    dbaddr = read_be32(strings, offset + 8)
    return length, version, ext_id, flags, dbaddr


def walk_tables(buf: bytes, addr: int, prefix: List[str], out: List[Dict]) -> None:
    _, payload = read_block(buf, addr)
    table = parse_table(payload)
    strings = table["strings"]

    for entry in table["entries"]:
        if entry["valuetype"] != EXTERNAL_VALUE_TYPE:
            continue
        length, version, ext_id, flags, child_addr = parse_external_metadata(strings, entry["data_offset"])
        name = entry["name"]
        new_prefix = prefix + [name]
        if ext_id == ID_TABLEPROCESSOR:
            walk_tables(buf, child_addr, new_prefix, out)
        elif ext_id == ID_WORDPROCESSOR:
            out.append({
                "path": new_prefix,
                "address": child_addr,
                "length": length,
                "version": version,
                "flags": flags,
            })


def resolve_root_address(buf: bytes, version: int, pointer: int) -> int:
    if version == 6:
        _, payload = read_block(buf, pointer)
        if len(payload) < 6:
            raise ValueError("Cancoon record truncated")
        return read_be32(payload, 2)
    return pointer


def main() -> None:
    parser = argparse.ArgumentParser(description="Extract a word processor Paige blob from a Frontier v6 database.")
    parser.add_argument("--db", required=True, help="Path to .root database (v6)")
    parser.add_argument("--out", required=True, help="Output file for the Paige blob")
    parser.add_argument("--path", help="Dot path to a specific entry (e.g., system.misc.about)")
    args = parser.parse_args()

    db_path = Path(args.db)
    data = db_path.read_bytes()
    version, pointer = read_db_header(data[:256])
    if version != 6:
        raise ValueError(f"{db_path} is version {version}; expected v6 legacy database")

    root_addr = resolve_root_address(data, version, pointer)
    hits: List[Dict] = []
    walk_tables(data, root_addr, [], hits)

    if not hits:
        raise RuntimeError("No word processor entries found in database")

    selected = None
    if args.path:
        target = args.path.split(".")
        for hit in hits:
            if hit["path"] == target:
                selected = hit
                break
        if selected is None:
            raise RuntimeError(f"Path '{args.path}' not found among word processor entries")
    else:
        selected = hits[0]
        print("Discovered word processor entries (first used by default):")
        for hit in hits:
            print("  -", ".".join(hit["path"]), f"@0x{hit['address']:08X}")

    _, blob = read_block(data, selected["address"])
    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(blob)
    print(f"Wrote {len(blob)} bytes from {'.'.join(selected['path'])} at 0x{selected['address']:08X} to {out_path}")


if __name__ == "__main__":
    main()
