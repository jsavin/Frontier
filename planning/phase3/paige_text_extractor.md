# Paige Text Extractor Plan

Status
- State: In Progress (Phase 3 Planning Active)
- Phase: Carbon Migration / Runtime Modernization
- Last Updated: 2025-11-20
- Owner: Codex
- Notes: Phase 1/2 completed; Phase 3 font-table work planned but not yet started.

**Date:** November 17, 2025  
**Owner:** Codex (headless runtime)  
**Related:** `planning/wptext_planC.md`, `_CURRENT_STATUS.md`

## Motivation
- Paige continues to crash in the headless build (allocator underflows, doc-info disposal bugs, `pgNewShell` mismanaging handles). After three days of debugging we still cannot reliably call `pgReadDoc`.
- Our migration requirement is now divided into two tracks per storage format:
	- WS blob compatibility (legacy WordSolutions writer) — handled separately via the existing WS reader.
	- Paige-packed WPText records (this document) — decode Paige keys without calling Paige.
	- Portable RTF payloads (`WPRT` header) — already UTF‑8; future work will expand those readers.
- Within the Paige track, we still stage feature phases:
	- Phase 1: Plain UTF‑8 text only — formatting, rulers, embeds, etc. can be dropped.
	- Phase 2: Add support for basic inline formatting supported in RTF: bold, underline, italics, point size, etc.
- The existing Paige path is massive overkill and a stability risk. 
- We need a deterministic, headless-safe extractor that understands just enough of the Paige-packed WPText blob to emit the raw characters and basic formatting without touching Paige.

## Known Format Clues
- WPText externals are stored via Paige’s packer (`pgReadDoc` + `pgScrapMemoryRead`). The file is a sequence of `pg_file_key` records (see `PGHEADER/PAIGE.H`):
  - `paige_key`, `text_block_key`, `text_key`, `line_key` come first; style/paragraph keys follow; then doc info, embeds, etc.
  - The emit order is fixed (see `PGSOURCE/PGWRITE.C`), so our parser can expect the style/paragraph metadata to precede the text payloads.
- **`text_block_key` / `text_key`**:
  - `text_block_key` encodes the block metadata (`text_block` struct; begin/end offsets, bounding rects, counts).
  - `text_key` carries the actual character bytes (`block->text` handle). `pgUnpackTextBlock` reads three packed arrays per block: `TBLines`, `TBHandle` (characters), and `TBCtl` (control runs).
  - Each text block’s `lines` handle stores `point_start` entries (baseline, flags, bounds). `line_key` in the stream serializes those arrays; `unpack_point_starts` shows the exact layout (offset, extra/tab info, baseline, flags, cell info, `r_num`, bounding rect).
  - Characters are stored as MacRoman unless the header marks `UNICODE_TEXT`; `pgUnpackPtrBytes` simply appends raw bytes.
- **Style keys**:
  - `style_run_key` is a run table of `[offset, style_item]` pairs (see `pgUnpackStyleRun` in `PGREAD.C`). Each entry marks the start of a character style; the style stays active until the next run’s offset.
  - `style_info_key` carries full `style_info` structs (font index, colors, spacing, user IDs, timestamps, etc.). `unpack_style_info` shows the serialized fields.
  - Paragraph formatting uses the analogous `par_run_key` / `par_info_key` pair. `unpack_par_info` lists the serialized fields (justification, indents, tabs, user data, outline level, named style indices).
  - We can skip these payloads entirely when we only need plain text, but the parser must advance by their `size` to stay in sync.
- **Embeds / doc info**:
  - Keys such as `embedded_item_key`, `doc_info_key`, `containers_key`, `exclusions_key`, `url_list_key` follow later. They refer to handles elsewhere (URL lists, embed refs). For a text-only extractor we can drop or replace them with placeholders.
- **Control byte markers**:
  - Paige occasionally emits non-printable markers (e.g., 0x01 for style tags, 0x14 for tables). `wpgettexthandle` doesn’t filter them—it just concatenates and leaves higher layers to interpret. For the extractor we can strip non-printable ASCII or leave them as-is.

## Design Overview
Phase the work so we can start with plain text while keeping boundaries between the three downstream readers:
- `ws_text_extractor.{c,h}` (existing) — legacy WS blobs.
- `paige_text_extractor.{c,h}` (this plan) — Paige-packed WPText handles.
- RTF/portable extractor (future) — `rtf_text_extractor.{c,h}` or equivalent.
Only the Paige extractor is in scope here.

### Phase 1: Plain Text
Build a tiny reader that works directly on the packed handle stored in the external variable:

1. **Blob Loader**
   - In `langhash_prepare_wordprocessor_value`, detect wptext externals and fetch the packed handle (`(**hv).variabledata` if in memory, or load it from disk first).
   - If the value is still on disk, use the existing `langexternalgetvalue` helpers to bring it into memory (so we read from a `Handle`).

2. **Key Scanner**
   - Treat the handle as a byte stream. Each Paige key is encoded as:
     ```
     struct pg_file_key {
         long key;
         long size;
         … payload …
     }
     ```
   - Loop through the stream: read `key`, `size`, then skip/consume `size` bytes.
   - For `text_key` (Paige constant `text_key = 'text'`), call the `text_block` parser (below).
   - For keys we don’t care about, advance by `size` (the packer is self-delimiting).

3. **Text Block Parser**
   - Each `text_key` payload is itself a packed structure:
     - 4-byte block count
     - For each block: packed lengths for `TBLines`, `TBHandle`, `TBCtl`, then the raw bytes for each section.
   - We only need the `TBHandle` bytes (actual characters). Control bytes:
     - Paige uses CR (`\r`) as paragraph delimiters; `wpgettexthandle` converts LF-only lines to CRLF.
     - Control markers (0x01..0x1F) indicate Windows-specific constructs; we can strip them or replace with whitespace.
   - Implementation sketch:
    ```c
    static boolean wptext_unpack_paige_blocks(const unsigned char *data, size_t size, Handle *hout);
    ```
     - Walk each block, append its `TBHandle` bytes to a growable buffer (Frontier handle via `enlargehandle`).
     - Optional: normalize line endings (`\r\n`).

4. **Embed / Control Handling**
   - If we encounter embed keys (`embed_ref_key`, `url_list_key`), inject a sentinel string (“[embedded object removed]”) so users know that the embedded content was dropped.
   - Document this behavior in `wptext_planC.md`.

5. **Integration**
- Replace the call to `wp_portable_extract_plaintext` in `langhash_prepare_wordprocessor_value` with `paige_extract_text_and_styles`.
   - Remove Paige-specific logging once the extractor is stable, but keep the old code available behind an `#ifdef` until we’re confident.

6. **Testing**
   - Unit-test the parser on known Paige samples (create small wptext objects in the test suite, serialize them via the existing runtime).
   - Run `FRONTIER_REGEN_ROOT=… ./tests/runtime_tests` to ensure the migrator converts all wptexts without invoking Paige.
   - Spot-check output strings (length matches original text, embedded markers handled as expected).


# Phase 1 Implementation Plan (Detailed)

**Status:** Completed (Nov 20 2025) — plaintext extractor + integration shipped in headless/runtime builds.

**Goal:** replace Paige usage with a deterministic UTF-8 extractor that works on both in-memory and disk-backed `wptext` externals.

**Primary integration points**
- `Common/source/langhash.c:langhash_prepare_wordprocessor_value` (materializes `wptext` into strings before packing tables).
- `portable/wptext_runtime.c:wpverbpacktotext` (called via `langexternalpacktotext`/`wpverbpacktotext` whenever `string()` hits a `wptext`).
- `portable/wptext_runtime.c:wp_portable_extract_plaintext` (currently Paige-backed; we will keep the signature and swap the implementation).
- `portable/wptext_portable.h` (new prototypes so both langhash + wpverbs can reach the extractor).

**Module layout**
- Introduce `portable/paige_text_extractor.h` and `.c`.
  - Define lightweight structs: `paige_blob_view` (pointer + length), `paige_pack_stream` (mirrors Paige’s `pack_walk` fields we need: cursor, remaining bytes, last code/value, repeat counter), and `paige_extract_stats` (flags + counts for logging).
  - Export `boolean paige_extract_text_and_styles(const uint8_t *bytes, long len, Handle *hout_utf8, paige_extract_stats *stats, char *errbuf, size_t errlen);`
  - Export `boolean paige_text_extract_from_external(hdlexternalvariable hv, Handle *hout_utf8, paige_extract_stats *stats);` – wrapper that hides DB fetch/state logic.

**Work breakdown**
1. **Blob loader and entry point (`portable/wptext_runtime.c`)**
   - Add helper `static boolean paige_state_load_blob(hdlexternalvariable hv, paige_blob_view *blob, boolean *is_portable_utf8, wp_portable_state **out_state);` that wraps `wp_portable_state_dbref`.
   - Portable payloads (`WPRT` magic) already store UTF-8; just copy bytes after the header into a new Frontier handle and skip the parser.
  - For Paige blobs, pass the raw bytes + length into `paige_extract_text_and_styles`.
   - Update `wp_portable_extract_plaintext` to call the new helper and log `[wp-plain] drop/converted` tags alongside parser stats (number of keys, unicode flag, fallback reason).

2. **Packed stream walker (`portable/paige_text_extractor.c`)**
   - Reimplement a minimal form of Paige’s packer:
     - Constants from `third_party/Paige/PGHEADER/PACKDEFS.H` (`CODE_MASK`, `REPEAT_LAST_VALUE`, etc.).
     - Functions mirroring `pgUnpackHex`, `pgUnpackNum`, `pgUnpackBytes`, `pgUnpackPtrBytes`, `pgGetUnpackedSize`.
     - A bounds-checked `paige_pack_advance` that refuses to read past the blob and returns descriptive error codes.
   - Only handle `byte_data`, `short_data`, `long_data`, and `terminator_data`; anything else triggers a parser error surfaced to callers.

3. **Key scanner**
   - Implement `static boolean paige_next_key(paige_blob_view *blob, short *out_key, long *out_size, long *out_element, const uint8_t **out_payload, char *errbuf, size_t errlen)` that mimics `pgReadKey`:
     - Consume 4 hex chars → `pg_file_key`.
     - Consume 8 hex chars → payload byte length.
     - Consume 8 hex chars → `element_info`.
     - Return pointer to the payload and advance the blob view by header+payload bytes.
- Track `paige_key` metadata: parse `version`, `flags`, `flags2`, and record whether flag bits imply platform quirks we should know about (future-proofing even though legacy Frontier never set `UNICODE_SAVED`).
   - Skip keys we do not need (`text_block_key`, style/paragraph/doc_info/etc.) by just advancing the cursor by `size`.
   - Validate EOF: when we run out of bytes or see `pg_eof_key`, stop cleanly; otherwise return an error if headers claim more data than available.

4. **Text aggregation**
   - Maintain a growable scratch handle (`Handle hmac`) for the concatenated MacRoman bytes.
   - For each `text_key`, initialize a `paige_pack_stream` at `payload`, use `paige_pack_get_unpacked_size` to determine the byte count, then copy into the scratch handle at the logical offset given by `element_info`.
   - Record stats (`stats->text_key_count`, `stats->total_text_bytes`). Optionally strip control bytes < 0x09 (except `\t`) and normalize CR/LF; flag any removals.
   - After walking all keys call `macromantoutf8`. If the conversion fails, fall back to returning the MacRoman bytes and set a `stats->returned_macroman` flag so callers can log it.

5. **Public API + callers**
   - Update `portable/wptext_portable.h` with prototypes for the new extractor helpers/stat struct.
   - Change `wp_portable_extract_plaintext` to simply call the extractor and return the UTF-8 handle (no Paige doc loads).
   - Implement `wpverbpacktotext` (headless build) so it calls `wp_portable_extract_plaintext`, pushes the returned handle into `htext`, and keeps existing unload semantics (if we had to swap in-memory state, release it afterward).
   - `langhash_prepare_wordprocessor_value` already frees/replaces the external; no signature changes required.

6. **Tests + fixtures**
   - Add `tests/fixtures/wptext/hello_macroman.bin` captured from real v6 roots (commit binary fixture).
   - Add `tests/components/paige_text_tests.c` with table-driven cases:
  - Feed fixture bytes into `paige_extract_text_and_styles` and compare the UTF-8 output with expected literals.
     - Inject malformed blobs (truncated header, bogus hex, unknown codes) to ensure we surface `[wp-plain] drop ...` and return `false`.
   - Extend `tests/runtime_tests` to include a `wptext_roundtrip` case: build a tiny table containing a `wptext`, call `langexternalpacktotext`, and assert we get the same UTF-8 as inserting via `wp_portable_pack_text_for_test`.
   - Make sure `make -C tests test` and `SANITIZE=1 make -C tests` include the new files.

7. **Docs & logging**
   - Update this file plus `planning/_CURRENT_STATUS.md` once the parser lands; describe the downgrade (plain text only) in `docs/database_architecture.md`’s WPText section.
   - Emit concise `[wp-plain] parse-start`, `[wp-plain] parse-ok ... stats=...`, and `[wp-plain] drop ... reason=` logs from the extractor so production logs explain what happened without LLDB.
   - Capture any new format discoveries (e.g., Unicode detection, control markers) here immediately so Phase 2 has a reliable reference.

**Exit criteria**
- Every headless code path (`langhash`, `langexternalpacktotext`, CLI tests) obtains UTF-8 via the new parser; Paige no longer runs during migrations/tests.
- Fixture-based tests exercise MacRoman inputs, and parser errors are deterministic with clear log output.
- Sanitized test runs stay green, demonstrating the parser never reads past the blob boundaries.

## Open Questions / TODO
- **Key constants:** Confirm the numeric IDs for `text_key`, `pg_signature`, `globals_key`, etc. (available in `PGSOURCE/PGFILEID.H`).
- **Block layout:** Verify the exact structure of `text_block` payloads by reading `PGSOURCE/PGTXTBLK.C` and `PGHEADER/PGTXTBLK.H`.
- **Encoded control bytes:** Identify which control values should be stripped vs. preserved (Paige might use 0xFF for hard line breaks, 0x01 for style markers, etc.).
- **On-disk vs. in-memory:** Ensure we can access the packed handle when the external is still a disk value (the migrator frequently keeps wptexts on disk until touched).
- **Performance:** Many wptexts must be processed without excessive copying into memory — prefer streaming append to a Frontier handle rather than bulk read followed by bulk write.


# Phase 2: Scoped Style Support (Detailed Plan)

**Status:** Completed (Nov 20 2025) — style-aware parser + RTF emitter landed with fixtures/tests.

Phase 2 adds enough style awareness to emit lightweight RTF that survives the v6→v7 migration. Scope stays narrow: inline emphasis, superscript/subscript, simple color/font info, paragraph breaks. No rulers, tables, embedded items, or layout geometry.

#### 1. Data contracts & structures
- **Files touched**: `portable/paige_text_extractor.{c,h}`, `portable/wptext_runtime.c`, `portable/wptext_portable.h`, `docs/database_architecture.md`, `tests/components/paige_text_tests.c`.
- Extend `paige_extract_stats` with counters for style runs discovered/emitted and a flag when styles were downgraded.
- Add local structs in `portable/paige_text_extractor.c`:
  ```c
  typedef struct {
      long offset;    /* character index where this run starts */
      short style_id; /* maps into style_info array */
  } paige_style_run;

  typedef struct {
      boolean bold, italic, underline, dbl_underline, strikeout;
      boolean superscript, subscript, all_caps, small_caps, hidden;
      boolean outline, shadow, word_underline, dotted_underline;
      short  point_size;   /* whole points (multiply by 2 for RTF half-points) */
  } paige_style_flags;
  ```
- Keep the parsed style infos in a growable array keyed by `style_item`. We only need the handful of boolean flags plus `point` (font size) and maybe `font_index` for future work.

#### 2. Style metadata parser (`style_run_key`, `style_info_key`)
- **Functionality**:
  1. When the key scanner sees `style_info_key`, parse the payload into `paige_style_flags`. Reference `PGHEADER/PGSTYLE.H` for the bit layout (`style_info.style_var`, `style_info.user_var`, etc.).
  2. When the scanner sees `style_run_key`, build a sorted array of `{offset, style_id}` entries. Offsets are absolute character indices within the entire document; `element_info` on each key tells us which block we’re in.
  3. Enforce monotonic offsets; log and drop any run that regresses to avoid infinite loops.
- **Implementation sketch**:
  - Add `static boolean paige_parse_style_info(paige_blob_view *, paige_style_table *, ...);`
  - Add `static boolean paige_parse_style_runs(...)` that uses the existing pack-stream helpers (`paige_pack_stream`) to decode the `[offset, value]` pairs (`pgUnpackLongArray` clone).
  - Track the default style ID so text preceding the first run inherits a sane baseline (Paige typically stores the doc default at offset 0).

#### 3. Paragraph metadata parser (`par_info_key`, `par_run_key`)
- Only gather what Phase 2 needs:
  - `par_info.justification` → map to `\ql`, `\qc`, `\qr`, `\qj`.
  - Flags for “single paragraph spacing” if we later want `\sb`/`\sa`.
  - Tab stops, margins, and outline levels stay out of scope.
- Represent paragraph runs as `{offset, par_style_id}` similar to character styles.

#### 4. Streaming RTF emitter
- Build a single helper `static boolean wptext_emit_rtf_from_paige_data(paige_text_context *, Handle *hout_rtf, paige_extract_stats *stats, char *errbuf, size_t errlen);`
  - Input: concatenated MacRoman text (`hmac` from Phase 1), style run array, paragraph run array.
  - Output: `hout_rtf` handle with:
    ```
    {\rtf1\ansi\deff0
    {\fonttbl\f0\fcharset77 Geneva;}
    {\colortbl;\red0\green0\blue0;}
    <control words + escaped text + \par sequences>
    }
    ```
  - For each character index:
    1. Determine the active style based on the nearest preceding run.
    2. Emit enter/exit control words when toggling booleans (e.g. `\b` / `\b0`, `\i` / `\i0`, `\ul` / `\ulnone`, `\strike`, `\super`, `\sub`, `\caps`, `\scaps`, `\outline`, `\shad`).
    3. When `point_size` changes, emit `\fsN` where `N = point_size * 2` (RTF stores half-points).
    4. Escape text characters per RTF rules; convert CR to `\par`, tabs to `\tab`.
  - Keep a small stack to avoid redundant control words (e.g., don’t emit `\b` twice).
  - Record stats: number of style transitions, flags emitted, fallback counts when we had to drop unsupported attributes (hidden text currently becomes visible, outline/shadow limited to `\outline`/`\shad` toggles).

#### 5. Integration into runtime
- `paige_extract_text_and_styles()` grows an optional `paige_style_plan` argument (or stores parsed data in `paige_extract_stats`). When Phase 2 is enabled, it should:
  1. Parse text blocks as Phase 1 already does.
  2. Parse style/paragraph keys into temporary arrays.
  3. Invoke `paige_emit_rtf` to produce a UTF-8 RTF handle via the existing `wp_portable_wrap_rtf_payload`.
- `wp_portable_state_cache_rtf()` and `wp_portable_state_pack_portable()` should skip the legacy “wrap plaintext in `{\rtf1 ...}`” helper when the extractor already supplied RTF.
- Add a build-time flag (e.g., `PAIGE_EXTRACTOR_ENABLE_STYLES`) so we can land Phase 2 incrementally: when unset, skip the parser and behave like Phase 1; when set, parse & emit RTF.

#### 6. Tests & fixtures
- **Fixtures**:
  - Use `scripts/extract_wordprocessor_blob.py` to capture at least two Paige documents containing rich formatting (bold/italic/underline, superscript/subscript, mixed point sizes). Save the raw blobs plus the expected UTF-8 RTF payloads under `tests/fixtures/wptext/`.
  - Optionally add a synthetic RTF expectation (hand-authored) to make assertions deterministic.
- **Unit tests** (`tests/components/paige_text_tests.c`):
  - Extend the test harness with a “Phase 2” mode that compares the emitted RTF handle against fixture text.
  - Verify toggling at run boundaries, e.g. `bold→italic→normal` transitions, nested superscript segments, double underline vs single underline.
  - Add negative tests: truncated style run table, style IDs pointing beyond the parsed style array, inconsistent offsets → ensure we log and downgrade gracefully.
- **Runtime tests**:
  - Add a `wptext_rtf_roundtrip` case in `tests/runtime_tests.c` that loads a known Paige blob, runs the migrator, and asserts that the cached `WPRT` payload contains the expected control words.
  - Gate assertions on `PAIGE_EXTRACTOR_ENABLE_STYLES` so Phase 1 builds still pass.
- **Manual validation**:
  - Add a tiny CLI helper (e.g., `tools/wptext_dump_rtf`) that reads a `.bin` fixture from `tests/fixtures/wptext/`, calls `paige_extract_text_and_styles` + `wptext_emit_rtf_from_paige_data`, and writes the resulting RTF to disk.
  - Use that tool to generate `.rtf` files for human inspection in TextEdit/Word without having to locate blobs inside `.root` databases each time.

#### 7. Documentation & rollout
- Update `docs/database_architecture.md` to describe how v7 stores `WPRT` externals once Phase 2 is active (mention inline RTF subset, ignored features).
- Expand this planning doc with any newly discovered flags (e.g., confirm `style_info.style_var & bold_var` semantics) so future contributors know which bits we interpret.
- Track rollout in `_CURRENT_STATUS.md` with timestamps (Phase 2 toggled on/off), and note any caveats (e.g., “hidden text becomes visible”).
- Once Phase 2 passes fixtures + runtime tests, flip the flag in default builds and remove the Phase 1 fallback log spam.

### Phase 3: Font Table & Typeface Preservation

**Status:** Planned (Priority P1) — waiting on multi-font fixtures before implementation begins.
Phase 3 pulls in Paige font selections so migrated RTF payloads keep their typeface choices. This builds on Phase 2’s style infrastructure.

#### 1. Font table ingestion
- **Key**: `font_table_key` (`pg_font_table_key` in `PGFILEID.H`). Each entry holds:
  - `font_id` / `style_item` reference.
  - Pascal string font name (MacRoman).
  - Platform-specific script IDs (can be ignored initially).
- **Implementation**:
  - Extend the key scanner with `static boolean paige_parse_font_table(...)` that walks the packed array (header + entries) and stores them in:
    ```c
    typedef struct {
        short font_id;
        char  name[64]; /* MacRoman, null-terminated */
    } paige_font_entry;
    ```
  - Maintain a growable array sorted by `font_id` for quick lookups.
  - Normalize font names to UTF-8 immediately via `macromantoutf8`, storing both the original MacRoman bytes (for compatibility) and a UTF-8 copy for RTF emission.
  - Provide helper `const paige_font_entry *paige_find_font(short font_id);` returning a default (e.g., Geneva) if missing.

#### 2. RTF font table emitter
- Enhance `wptext_emit_rtf_from_paige_data` to:
  1. Build a deterministic font list covering every `style_info.font_index` encountered. A simple approach: iterate runs in order, append unique IDs to `font_usage[]`.
  2. Emit the RTF `{\fonttbl ... }` header once, taking care to escape any special characters in the font names (`\`, `{`, `}`) and convert non-ASCII bytes to `\'hh`.
  3. Track the active font alongside other style toggles; emit `\fN` when switching fonts (`N` is the index within our emitted font table, not necessarily the original Paige ID).
  4. Expose font usage stats in `paige_extract_stats` (e.g., `fonts_emitted`, `fonts_dropped`).
- Cap the document font table at 256 distinct entries (designers occasionally mix dozens of faces in a single doc). If we ever encounter more, log a downgrade and map excess fonts to the default so we maintain RTF validity without surprising users.

#### 3. Tests & fixtures
- Capture Paige blobs that use multiple fonts (body text vs headings vs code).
- Add fixture expectations containing the exact RTF font table and control-word sequence.
- Tests should cover:
  - Font table ordering (insertion order vs sorted by ID).
  - Font name escaping (`Chicago Bold+` → `Chicago Bold\+`).
  - Missing font entries (style references ID with no table entry) falling back to `\f0`.
  - Non-ASCII names (MacRoman -> UTF-8 -> RTF `\'hh`).

#### 4. Documentation & rollout
- Document the new behavior in `docs/database_architecture.md` (“`WPRT` payloads now include font tables matching source Paige `font_table_key` entries”).
- Note in `_CURRENT_STATUS.md` when Phase 3 ships; highlight that only names are preserved (not metrics, kerning tables, etc.).
- Consider updating `planning/wptext_planC.md` to reflect the richer RTF output.

#### 5. Optional follow-ups
- Surface platform/script IDs in the RTF font table (`\fprq`, `\fcharset`) if we need precise mappings. For now we can default to `\fcharset77` (MacRoman) since legacy blobs never encoded Unicode.
- Decide whether to expose font info to UserTalk (e.g., via a future `wp.getFontTable` verb) once the extractor has a reliable representation.

### Frontier Runtime Hooks
Regardless of how much formatting we carry forward, we also need to wire the extractor into the headless runtime so existing verbs keep working:
1. `langhash_prepare_wordprocessor_value` already swaps externals for strings. Ensure the new extractor feeds that path so migrated v7 tables contain plain strings (or RTF strings if we enable Phase 2).
2. `langexternalpacktotext`/`wpverbpacktotext` (used by `string(adrWptext^)`) should invoke the same extractor instead of Paige; this means `coercetostring()` and `string()` won’t pull in Paige anymore.
3. If/when we emit lightweight RTF, teach the same coercion path to parse those control words back into plain text for callers that expect strings (e.g. strip RTF tags or re-run the extractor on the stored blob).
4. Update the relevant tests (`runtime_tests`, `langhash` unit tests, `langverbs` string tests) so we always exercise both caller types (`string()` and migrator) against the new code.
5. Longer-term, expose the same parser/output through new verbs (`wp.wpToHtml`, `wp.wpToMarkdown`, etc.) so headless tooling can export rich text in modern formats without going through Paige. Once Phase 2 lands, adding alternative emitters is just a matter of swapping the writer.

## Milestones
1. Prototype `paige_parse_key_stream()` that walks the packed handle and dumps key IDs/sizes to logs (sanity check).
2. Implement `paige_extract_text_and_styles()` that concatenates `text_block` payloads into a Frontier handle; hook it into `langhash_prepare_wordprocessor_value`.
3. Add regression tests (create sample wptexts, run the extractor, verify the output matches the original string).
4. (Optional) Implement the Phase 2 style parser/emitter and add style-centric fixtures.
5. Remove Paige dependencies from the migration path, update `_CURRENT_STATUS.md`, and archive the temporary instrumentation.
