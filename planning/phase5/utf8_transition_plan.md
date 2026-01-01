# UTF-8 Transition Plan

Status
- State: Planning
- Phase: 4 (Text Modernization)
- Last Updated: 2025-11-20
- Owner: Codex
- Notes: Blueprint for migrating Frontier to UTF-8; update as phases start.

## Context and Goals
- Frontier’s core still treats every string as Pascal `bigstring` (1-byte characters) or raw byte handles. MacRoman/ANSI assumptions leak into kernel APIs, database packs, and script verbs.
- Modern UX (portable runtime, headless tests, CLI) would benefit from a canonical UTF-8 internal representation that aligns with contemporary platforms.
- We accept that moving to UTF-8 is a breaking change for some UserTalk scripts; compatibility tooling will mitigate the impact.

## Current String Model Snapshot

### Core Structures
- `bigstring` (`Str255`) lives in `Common/SystemHeaders/standard.h:283-305`; helper macros assume single-byte characters.
- Long strings reside in `Handle`s manipulated by helpers in `Common/headers/memory.h` and `Common/source/strings.c`.
- Case conversion uses `lowercasetable[256]` (`Common/source/strings.c:2567-2570`).
- Parsers rely on `isalpha/isdigit` (`Common/source/langscan.c:44-115`), so tokenisation is byte-oriented.

### Encoding Helpers
- Conversion verbs still call Carbon’s Text Encoding Converter (`Common/source/strings.c:2269-2338`).
- UserTalk exposes `string.macRomanToUtf8`, `string.utf8ToMacRoman`, and ANSI counterparts (`Common/source/stringverbs.c:2250-2278`).

## Platform-Specific Hotspots
- macOS UI / FS bridges create CFStrings using `kCFStringEncodingMacRoman` (e.g. `filedialog.c:312-337`, `filepath.c:96`, `fileops.m:173,2198,2893`).
- Legacy encoding tables referenced in `langstartup.c:507-510` and AppleEvent glue (`langsystypes.c:1454-1466,1886-1894`).
- Windows code paths convert between UTF-16 and Windows Latin-1 (`Common/source/strings.c:2367-2449`).
- Headless stubs define compatibility constants (`Common/headers/headless_stubs.h:114-118`).

## Script-Surface Behaviour
- Most `string.*` verbs rely on byte offsets; switching to UTF-8 will change semantics that scripts might depend on.
- `string.macRomanToUtf8` et al. provide partial relief but still require MacRoman source data.

## Migration Strategy

### Phase 0 – Specification & Audit
- Catalogue every kernel API crossing encoding boundaries (strings, memory, database pack/unpack, AppleEvents, filesystem). References include `langhash.c:1952-2074`, `tablepack.c:320-343`, `langxml.c:2470-2497`, `op*`, etc.
- Decide canonical internal encoding (UTF-8) and document new invariants (handles contain valid UTF-8; Pascal strings transitional only).

### Phase 1 – UTF-8 Infrastructure
- Introduce UTF-8 utilities: validation, codepoint iteration, length, case-mapping (consider ICU/CF or embedded lib).
- Replace `lowercasetable`/`isalpha` assumptions with helpers that work per codepoint.
- Add APIs differentiating byte length vs. character count.

### Phase 2 – Core Runtime Conversion
- Update `strings.c`, `memory.c`, `langvalue.c`, and `langscan.c` to treat handles as UTF-8, switching word/field/token logic to codepoint-aware iteration.
- Replace TEC usage with modern APIs (CFString on macOS, ICU/Win32 MultiByte elsewhere) and supply equivalents in headless builds.

### Phase 3 – Persistence & IPC
- Define DB migration: either lazy conversion with encoding flags or a versioned rewrite of string data (v7→v8 style).
- Update AppleEvent and XML/HTTP bridges to send/receive UTF-8 (`langsystypes.c`, `langipc.c`, `langxml.c`).
- Ensure headless stubs mirror the new behaviour.

### Phase 4 – UserTalk Compatibility
- Decide compatibility model (e.g. keep byte-oriented verbs & add UTF-8 variants vs. flip defaults with a toggle).
- Reimplement `string.macRomanToUtf8` etc. on top of new helpers for backward compatibility.
- Produce migration docs and tooling (script analyzer for byte-index assumptions).

### Phase 5 – Testing & Tooling
- Expand test coverage (runtime, parser, database, AppleEvents) with multi-byte inputs (Latin accents, emoji, combining marks, RTL).
- Add asserts/diagnostics to catch non-UTF-8 data early.
- Provide DB upgrade/migration tooling and automated backups.

### Phase 6 – Rollout
- Stage changes behind a feature flag / preview build until parity achieved.
- Communicate breaking changes, offer downgrade path, collect feedback before making UTF-8 the default.

## Risks & Mitigations
- **Silent corruption** if subsystems remain byte-based → exhaustive audits + runtime validation.
- **Performance regressions** from repeated UTF-8 scans → cache codepoint counts or leverage libraries optimized for UTF-8.
- **Script breakage** → compatibility mode, migration tools, documentation.
- **Dependency drift** (TEC removal) → plan replacements for macOS and headless builds (e.g., ICU).

## Open Questions / Follow-ups
- Final decision on DB migration strategy (lazy vs. version bump).
- Case-mapping rules (locale-aware? Unicode normalisation?).
- How to expose UTF-8 aware APIs at the script level without breaking classic code.
- Resource bundle / AppleEvent descriptor formats when moving to UTF-8 (`typeUTF8Text` vs. `typeUnicodeText`).
- Windows host requirements (do we standardise on UTF-8 console/files?).

---
*Last updated: 2025-10-12*
