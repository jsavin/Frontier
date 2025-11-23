# WPText RTF Migration Tracker

Status
- State: In Progress
- Phase: Carbon Migration (WPText)
- Last Updated: 2025-11-20
- Owner: Codex
- Notes: Checklist for Paige/WPRT modernization; mark items as they complete.

**Purpose**: Track the Paige build + WPText → RTF/UTF‑8 migration work. Paige is **conversion-only**: we use it strictly to read legacy blobs and emit UTF‑8 RTF during migration. Long-term editing/rendering will switch to different libraries, so avoid adding new runtime features or UI dependencies to Paige. Move this doc to `planning/archive/` once complete to preserve the decision history.

## Task Checklist

### 1. Build Paige for modern toolchains
- [x] Add `third_party/Paige` (HERMES Paige) as a submodule.
- [x] Vendor a portable CMake toolchain:
  - [x] Download CMake 3.29.6 source under `third_party/cmake-src/`.
  - [x] Patch `Utilities/cmzlib/zutil.h` so modern macOS (`TARGET_OS_OSX`) doesn’t redefine `fdopen`.
  - [x] Build and install CMake locally (`third_party/cmake-install/`) for use by the Paige build.
- [x] Produce a macOS static library (x86_64 + arm64):
  - [x] Audit/trim platform-specific code (QuickDraw/GDI) to allow headless builds (prefer Windows layer or stub UI hooks).
  - [x] Add a build target (CMake or custom) to compile Paige with Clang.
  - [x] Generate `libpaige.a` and verify it contains the required architectures (`lipo -info`).
- [x] Wire Paige into our builds:
  - [x] Update `frontier-cli/Makefile` and `tests/Makefile` to link the new library (new `$(PAIGE_LIB)` rule drives the CMake build and injects the archive at link time).
  - [x] Drop the legacy headless WP stubs and build against the shared `portable/wptext_runtime.c`.

### 2. Headless-safe Paige initialization
- [x] Add minimal shims for QuickDraw/Carbon calls (cursor blinking, window rectangles, clipboard) using the Paige UNIX platform layer (`PGPLATFO/PGUNX.C`) plus headless no-ops.
- [x] Ensure `wp_portable_init()` runs during CLI/tests bootstrap so Paige allocators are live long before `wpverbs` fire.
- [x] Confirm pack/unpack paths work without a UI by pushing real WP verbs through `portable/wptext_runtime.c` (runtime tests now link the true library).
- [ ] Document remaining platform gaps (clipboard, menu events) before we strip additional Carbon code from the desktop target.

### 3. Portable header definition & serializer
- [x] Define `tywpportableheader` (`'WPRT'` magic, version, timestamps, text length, UTF-8 flag, 1 KB reserved block). Document it in `wptext_format.md`.
- [x] Implement `wp_portable_pack()`:
  - [x] Force doc into memory (`wpverbinmemory`).
  - [x] Call `pgExportFileFromC(...pgRTFfile..., pgExportUTF8, …)` with a memory-backed writer.
  - [x] Emit `[tywpportableheader][UTF‑8 RTF bytes]`.
- [x] Implement `wp_portable_unpack()`:
  - [x] Detect the magic + header.
  - [x] Desktop/headless: reuse Paige only to hydrate legacy docs, treating the UTF‑8 RTF blob as canonical storage.
- [x] Update `wpverbpack`/`wpverbunpack` to switch between legacy and portable paths based on `flconvertingolddatabase` / `use_64bit_format` (legacy trailers are still read for back-compat).

#### Detailed RTF Conversion Steps
1. **Portable Header Finalization**
   - [x] Confirm exact field order/packing of `tywpportableheader` and reserve 1024 bytes for future metadata (refcons, view state, etc.).
   - [x] Add helper routines to read/write the header (with endian swaps) and validate the `'WPRT'` magic + version.
   - [ ] Update `wptext_format.md` with diagrams + sample hex dumps for both legacy headers and the new portable header.
2. **Exporter Plumbing**
   - [x] Extend `portable/wptext_runtime.c` with an exporter helper that:
     - [x] Calls `wpverbinmemory` to guarantee a Paige doc exists.
     - [x] Uses `pgExportFileFromC` with `pgRTFfile | pgExportUTF8` into a scratch `memory_ref`.
     - [x] Copies the resulting RTF bytes into a classic handle, records their UTF-8 length, and updates the header timestamps/ctsaves.
   - [x] Embed the portable header + RTF payload into a merged handle compatible with `dbassignhandle`.
3. **Importer Plumbing**
   - [x] Teach the runtime to detect `'WPRT'` magic: split header/RTF without assuming legacy trailers.
   - [x] Desktop/headless: invoke `pgImportFileFromC` as a temporary bridge so scripts still see real Paige docs; keep legacy loader as fallback.
4. **Runtime Hooks**
   - [x] Update `wpverbpack`/`wpverbunpack` so migration (`flconvertingolddatabase` / `use_64bit_format`) forces the portable path.
   - [ ] Ensure timestamps (`timecreated`, `timelastsave`, `ctsaves`) survive every conversion; add logging/tests to prove the values stay stable across pack/unpack.
5. **Validation**
   - [ ] Build fixtures covering styled text, embedded outlines, and large documents; round-trip them through the portable serializer.
   - [ ] Add unit/integration tests that inspect the stored bytes (magic, header contents, UTF-8 lengths) and verify the CLI can open/migrate roots containing the new format.

### Legacy Word Solutions compatibility (added November 18, 2025)
- [x] Detect WS payloads via header structs mirrored from `WSE.h` and log each encounter in `planning/phase3/carbon_migration/ws_drop_log.md`.
- [x] Provide a placeholder string so `langhash_prepare_wordprocessor_value` never dereferences NULL while we finish the real parser.
- [x] Port the plaintext traversal logic from `unpackwordsolutionenginefile` (see `../tedchoward/Frontier/Common/source/wpengine.c`) into `wp_portable_extract_ws_text`, ignoring rulers/formatting but preserving the legacy timestamps. (We now slice the raw header, grab the text payload, and feed a synthesized RTF string back through Paige.)
- [ ] Add regression tests that feed captured WS blobs (e.g., `*Release Notes*`, `examples.readme`) through the importer and confirm we emit UTF-8 text plus a proper `WPRT` header rather than dropping the nodes.
- [ ] Retire or behind-a-flag the placeholder path once conversion succeeds so WS data always migrates with preserved text.

### 4. Migrator integration
- [ ] Re-run `migrate_32bit_to_64bit` and ensure WPText nodes reserialize using the portable format.
- [ ] Add logging/tests verifying the `WPRT` magic appears in the regenerated root.

### 5. Tests & fixtures
- [ ] Capture sample WPText payloads (legacy + portable) under `planning/phase3/carbon_migration/data/`.
- [x] Add unit tests covering `wp_portable_pack()`/`wp_portable_unpack()` round-trips (plaintext comparison, header validation). `tests/runtime_tests` now calls `wp_portable_pack_text_for_test` and asserts the `WPRT` header, payload sizing, and brace balance for a sample UTF-8 string.
- [ ] Extend CLI integration tests (or add a new script) that triggers WPText packing during migration and verifies the CLI no longer crashes when it hits `examples.song`.

### 6. Cleanup / follow-up
- [ ] Once all WPTexts are migrated, decide whether Paige remains linked or if we treat RTF as the canonical stored form.
- [ ] Remove temporary guards/stubs no longer needed.
- [ ] Move this tracker to `planning/archive/` with a short summary of the outcome.

- **CMake toolchain**: Local CMake 3.29.6 now lives under `third_party/cmake-install/`; use `third_party/cmake-install/bin/cmake` when building Paige.
- **Paige source**: Submodule added (`third_party/Paige`). UNIX platform layer (`PGPLATFO/PGUNX.C`) now provides the handle/memory/shapes shims the headless runtime needs.
- **Build output**: `third_party/cmake-install/bin/cmake --build third_party/Paige/build-headless` emits a universal `libpaige.a` (arm64 + x86_64). Those slices are linked into both CLI and tests.
- **Runtime integration**: `portable/wptext_runtime.c` now owns WPText pack/unpack for headless builds, using real Paige docs plus legacy trailer handling. The legacy stub file is gone.
- **Next focus**: finish the portable header + RTF serializer plan above, then switch the migrator to emit `WPRT` payloads so v7 roots no longer store opaque Paige binaries.
- **Related docs**: `planning/phase3/carbon_migration/wptext_format.md`, `planning/_CURRENT_STATUS.md`, `planning/phase3/carbon_migration/data/` (fixtures TBD).
