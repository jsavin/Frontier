# Frontier Utility Scripts

> 2025-11-08 Codex: Added a quick catalog so future contributors know what lives in `scripts/`.

This directory collects small, repo-specific helpers that aren’t part of the main build.

| Script | Purpose | Typical Usage |
| --- | --- | --- |
| `build_mysql_client.sh` | Rebuild the legacy MySQL client shim used by older integration tests. | `./scripts/build_mysql_client.sh` (runs in-place; expects MySQL headers in the environment). |
| `check_doc_links.py` | Scans Markdown files and verifies that local/remote links resolve. | `python3 scripts/check_doc_links.py docs/` |
| `dump_tables.py` | Iterates every block in a `.root` file and prints any table payloads it recognizes (modern two-merge format). Handy for spotting top-level tables without loading Frontier. | `python3 scripts/dump_tables.py databases/Frontier-v6.root` |
| `gen_langparser.sh` | Rebuilds the generated language parser from the Bison/Flex sources under `Common/source/langparser/`. | `./scripts/gen_langparser.sh` (called automatically by the build, but useful when editing the grammar). |
| `inspect_table_payload.py` | Given a byte offset, dumps the header/records/strings of a single table block so we can document on-disk layouts. Works for both v6 and v7 blocks. | `python3 scripts/inspect_table_payload.py databases/test.root --offset 0x5db` |
| `extract_legacy_table.py` | Resolves a dot-path (e.g., `system.verbs.globals`) inside a v6 root, then writes the raw merged table payload to disk for golden fixtures. | `python3 scripts/extract_legacy_table.py --db databases/Frontier-v6.root --path system.verbs.globals --out /tmp/globals.bin` |
| `scan_database_types.py` | Catalogs every value type (and optional nested tables) inside a `.root` file. Useful for migration sizing. | `python3 scripts/scan_database_types.py --deep databases/Frontier-v6.root` |

If you add new helpers, update this README so we can keep the tooling discoverable.
