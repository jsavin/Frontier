# WPText On-Disk Format (Legacy Reference)

**Sources**
- `../../tedchoward/Frontier/Common/source/wpengine.c:80-360, 2100-2340`
- `../../tedchoward/Frontier/Common/source/wpverbs.c:480-760`
- `../../tedchoward/Frontier/Common/Paige/headers/PAIGE.H`

## High-Level Layout
`wpverbpack()` calls `wppack()` ( `wpengine.c:2205` ) which serializes the current Paige document into a handle containing:

```
[Paige document bytes][tywpheader trailer]
```

- The Paige bytes are produced by `pgSaveDoc()` / `pgTerminateFile()` ( `wpengine.c:2160-2189` ).
- A `tywpheader` struct ( `wpengine.c:108-134` ) is appended via `pushhandle`.
- `wpverbpack` writes the resulting handle to the database via `dbsavehandle`, so every WPText value in a `.root` file is a contiguous Paige blob plus header trailer.

## `tywpheader` Structure (trailer)
Field summary (all big-endian on disk):

| Field | Size | Notes |
| --- | --- | --- |
| `versionnumber` | 2 | Currently `1`. |
| `timecreated`, `timelastsave` | 4 + 4 | Ticks, preserved on load/save. |
| `ctsaves` | 4 | Incremented on every pack. |
| `fontname[32]`, `fontsize`, `fontstyle` | default display font for the document. |
| `maxpos` | 4 | Document length (characters). |
| `unused[4]` | reserved. |
| `flags` | bitmask (`floneline`, `flruleron`, etc). |
| `windowrect` | saved window position/size (`diskrect`). |
| `varlistsize` | 4 | Size of the variable list block. Legacy code leaves this at `0`. |
| `buffersize` | 4 | Number of bytes in the Paige blob that precedes the header. |
| `waste[52]` | slack space for future expansion. |

`wpunpack()` ( `wpengine.c:2240-2340` ) pops this header, resizes the handle to `buffersize`, and passes the Paige payload to `wpnewbuffer`. Window prefs and flags are restored from the header.

## Paige Payload
The payload is the native Paige document format (a serialized `pg_ref`). `wppacktext()` calls:

1. `pgSaveDoc(pg, ...)` – writes the entire document (text, style runs, embedded refs) into a temporary `file_ref`.
2. `pgTerminateFile()` – flushes end-of-file data.
3. The resulting memory buffer is copied byte-for-byte into the final handle.

Key implications:

- Styles, fonts, ruler settings, and embedded objects are all encoded inside the Paige stream; no additional metadata is stored by Frontier.
- The Paige headers (`PAIGE.H`) expose import/export helpers. Of particular interest: `pgExportFileFromC(..., pgRTFfile, ...)` and `pgImportFileFromC(...)`. These are the most reliable way to convert a Paige document to/from RTF without reverse-engineering the binary stream.

## Portable `tywpportableheader` (modern `WPRT` format)
Starting November 2025 the headless runtime repacks WPText nodes into a portable layout:

```
[tywpportableheader][UTF-8 RTF bytes]
```

`tywpportableheader` lives at the start of the value and is stored big-endian just like the legacy trailer:

| Field | Size | Notes |
| --- | --- | --- |
| `magic` | 4 | `'WPRT'` sentinel. |
| `version` | 2 | Starts at `1`. |
| `flags` | 2 | Bit `0` (`0x0001`) indicates UTF-8 RTF payload. |
| `timecreated`, `timelastsave`, `ctsaves` | 4 + 4 + 4 | Preserved from the legacy header for script compatibility. |
| `textlength` | 4 | Character count (mirrors `maxpos`). |
| `utf8bytelen` | 4 | Number of bytes in the following RTF stream. |
| `reservedlength` | 4 | Currently `0`; future use. |
| `reserved[1024]` | 1024 | Zeroed expansion slab (mirrors tables/outlines). |

The RTF bytes are emitted via `pgExportFileFromC(... pg_rtf_type ..., EXPORT_EVERYTHING_FLAG | EXPORT_UNICODE_FLAG ...)` and immediately wrapped with the header. Importing follows the inverse path: detect `'WPRT'`, split the payload, and feed it back to Paige via `pgImportFileFromC` **only** to hydrate legacy UIs/headless tests. Paige is intentionally limited to this conversion role—once the database is migrated, the runtime treats the UTF-8 RTF blob as canonical storage so future editors can swap Paige out entirely.

## Packing Path
- `wpverbpack()` ( `wpverbs.c:580-720` ) forces the document into memory when `flconvertingolddatabase` or `fldatabasesaveas` is set.
- In-memory docs call `wpverbpackrecord()`, which delegates to `wppack()`.
- Packed docs (already serialized) are written back as-is via `dbassignhandle`.

## Migration / Modernization Notes
- Because the legacy migrator never forced repacks, v7 roots still contain Paige blobs with the header trailer; the new migrator now flips `flconvertingolddatabase` so every WPText goes through `wppack()`.
- To drop Paige dependency we can replace `wppack()` with a portable serializer that calls `pgExportFileFromC(...pgRTFfile...)` and stores the emitted RTF alongside a slim header (e.g., keep `tywpheader` for UI prefs). Runtime reads could feed the RTF back into Paige via `pgImportFileFromC` when the classic UI is present, or a native RTF renderer in headless mode.
- Until we implement the portable packer, `tablesavesystemtable()` fails because `wppack()` still writes Paige/QuickDraw data. Any repack-driven migration (like `--upgrade-system-root`) will continue to trip once it reaches `examples.song`, which is a WPText document.

## RTF/UTF-8 Migration Plan
Goal: replace the legacy Paige binary + `tywpheader` payload with `[tywpportableheader][UTF-8 RTF]`, so headless builds can serialize/deserialize WPText without QuickDraw/Carbon dependencies.

### 1. Build Paige for modern targets
1. Add the open-source Paige repo as a submodule (done: `third_party/Paige`).
2. Create a build script/CMake target to produce a macOS static library (x86_64 + arm64). We can reuse `third_party/Paige/CMakeLists.txt`, but we’ll need to:
   - Disable or stub GUI-specific code (QuickDraw, Windows GDI). For headless use we only need text serialization, so the platform layer can provide no-op implementations for drawing/scrolling, or we can port the Windows version (which uses GDI) to Cocoa/CoreGraphics.
   - Ensure `pgExportFileFromC` / `pgImportFileFromC` and supporting modules compile cleanly with Clang/LLVM (fix warnings, replace deprecated APIs).
3. Integrate the resulting lib (e.g., `libpaige.a`) into `frontier-cli/Makefile` and `tests/Makefile`.
4. Remove `tests/headless_wp_stubs.c` (or gate it) so headless builds use the real `wpengine.c`, `wpverbs.c`, `wpvariables.c`.

### 2. Headless-safe shims
1. Paige expects QuickDraw handles/windows on macOS. Use `portable/quickdraw_portable.h` or add new shims so functions like `recttodiskrect`, `diskrecttorect`, `RGBColor` conversions, etc., work without Carbon.
2. Provide dummy implementations for UI callbacks (cursor blinking, selection painting, clipboard). For migration we only need pack/unpack paths, so these can be empty.
3. Ensure `wpstart()` initializes the Paige globals even without a window. Add a `wp_portable_init()` invoked during CLI startup.

### 3. Portable header definition (`tywpportableheader`)
Fields to include:
| Field | Purpose |
| --- | --- |
| `magic` (`'WPRT'`) + `version` | Identifies the portable format. |
| `timecreated`, `timelastsave`, `ctsaves` | Preserve script-visible metadata. |
| `text_length_utf8` | Length (code points or bytes) of the UTF-8 payload. |
| `reserved[1024]` | Zeroed expansion block (mirrors table/outline additions). |

We no longer serialize font defaults, window rect, or UI flags; that state will live in per-user prefs later.

### 4. Serialize to UTF-8 RTF
1. Add `wp_portable_pack()`:
   - Ensure document is in memory (`wpverbinmemory`).
   - Call Paige exporter: `pgExportFileFromC(pg_ref, pgRTFfile, feature_flags | pgExportUTF8, ...)`, passing `pgScrapMemoryWrite` to collect bytes in memory.
   - Build `[tywpportableheader][rtf bytes]` and return it to `wpverbpack`.
2. Add `wp_portable_unpack()`:
   - Detect the magic, read the header, and split the RTF blob.
   - Desktop builds: call `pgImportFileFromC` with `pgRTFfile` to rebuild the Paige document, then proceed as usual.
   - Headless builds: keep the RTF blob as the “packed” representation (scripts can fetch it; runtime tests can verify round-trips).
3. Update `wpverbpack` / `wpverbunpack`:
   - When `flconvertingolddatabase` or a new `FRONTIER_PORTABLE_WP` flag is set, call the portable helpers.
   - Legacy payloads: detect the old header (size, version) and either leave them alone or convert on load before re-saving.

### 5. Migrator integration
1. With `tableverbpack` already forcing repacks during `migrate_32bit_to_64bit`, re-running the migrator will automatically rewrite WPText blobs once the portable packer is active.
2. Add logging/tests to ensure the regenerated root contains the `WPRT` magic.

### 6. Testing
1. Capture fixtures under `planning/carbon_migration/data/` with sample WPText documents (plain text, styled text).
2. Add unit tests exercising `wp_portable_pack()` / `wp_portable_unpack()` round-trips:
   - Verify header fields (timestamps, reserved block zeroed, text length).
   - Compare plaintext between original Paige buffer and the RTF re-imported document.
3. Add a CLI integration test (or extend `tests/cli_system_defined`) that runs the migrator on a sample root and confirms WPText tables no longer cause “Table is undefined”.

### 7. Cleanup / Future Work
1. Once all WPTexts are in the portable format, we can decide whether to keep Paige linked (for editing) or treat RTF as the canonical form and use another renderer.
2. Document the final header layout and migration behavior in this file, so future changes don’t guess.

Dependencies / Risks:
- Building Paige on macOS/arm64 may require additional porting beyond the text above.
- Until Paige is available in our toolchain, the migration work remains blocked; if porting proves infeasible, we would have to fall back to reverse-engineering the binary format, which is significantly more effort.
- Menubar and other externals still need similar treatment; once WPText migration is in place, repeat the pattern for those types.

## Open Questions
- `varlistsize` is always zero in modern builds; legacy code once supported variable expansion blocks but there’s no evidence of use in the system root. Confirm before discarding.
- `pgSaveDoc`’s binary format is opaque; if we need byte-level access (e.g., to audit fonts) we may need to instrument Paige or rely on `pgExportFileFromC` to produce an intermediate RTF/HTML representation.

These notes should be kept up to date as we prototype the RTF-based serializer, so future work has a canonical reference for the legacy layout.
