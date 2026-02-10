# Phase 7: Testing & Rollout

**Status**: Planned
**Risk**: Medium
**Breakage**: Depends on scope of rollout
**Depends On**: All previous phases

---

## Goal

Validate the entire text modernization stack end-to-end, stage the rollout behind feature flags, and provide a clear communication and downgrade path for users.

---

## Test Coverage

### 1. UTF-8 Correctness

**Multi-byte test corpus** — every test case exercised with:

| Category | Examples | Byte Lengths |
|----------|----------|-------------|
| ASCII | `"hello"`, `"test123"` | 1 byte/char |
| Latin Extended | `"cafe\u0301"`, `"na\u00efve"`, `"Stra\u00dfe"` | 2 bytes/char |
| CJK | `"\u4e16\u754c"` (世界) | 3 bytes/char |
| Emoji | `"\U0001f600"` (😀), `"\U0001f1fa\U0001f1f8"` (🇺🇸) | 4 bytes/char |
| Combining marks | `"e\u0301"` (e + combining acute) | Base + combiner |
| RTL | Arabic, Hebrew text | Mixed directionality |
| Zero-width | ZWJ sequences, ZWNJ | Zero-width joiners |
| Edge cases | Empty string, single codepoint, max-length string | Boundary conditions |

### 2. Database Round-Trip

- Create v7 database with known non-ASCII content (MacRoman encoded)
- Migrate to v8
- Verify all string values correctly transcoded to UTF-8
- Save v8 database, reopen, verify again
- Export back to v7 (downgrade tool), verify MacRoman content matches original

### 3. Hash Table Distribution

- Create tables with pathological key sets (padded numbers, prefixed identifiers)
- Verify FNV-1a distribution across buckets
- Benchmark lookup time vs. legacy algorithm
- Verify round-trip: create v8 table → save → reopen → same distribution

### 4. UserTalk Script Compatibility

- Run all scripts in `system.verbs` against the new runtime
- Run integration test suite with UTF-8 content
- Verify encoding conversion verbs produce correct results
- Verify deprecated verbs emit warnings
- Test script analyzer on known-affected patterns

### 5. Performance Benchmarks

| Operation | Baseline (v7) | Target (v8) | Acceptable Regression |
|-----------|--------------|-------------|----------------------|
| String comparison (ASCII) | — | — | < 5% |
| String comparison (UTF-8 multi-byte) | N/A | — | N/A (new capability) |
| Hash table lookup (10 items) | — | — | < 5% |
| Hash table lookup (1000 items) | — | — | 2-5x improvement expected |
| Database open (small) | — | — | < 10% |
| Database open (large) | — | — | < 20% (includes rehash) |
| v7→v8 migration (typical DB) | N/A | < 30s | — |

---

## Rollout Strategy

### Stage 1: Preview Build

- All changes behind `FRONTIER_UTF8_MODE` compile-time flag
- Available as opt-in preview build
- v8 migration requires explicit user action (no automatic upgrade)
- Collect feedback from early adopters

### Stage 2: Default-On with Opt-Out

- UTF-8 mode enabled by default in new releases
- Existing databases remain v7 until explicitly upgraded
- New databases created as v8
- Runtime flag to revert to legacy behavior if needed

### Stage 3: Full Rollout

- Remove compile-time flag
- UTF-8 is the only supported internal encoding
- Legacy encoding verbs still available for reading old data
- Deprecated verbs scheduled for removal in future release

---

## Downgrade Path

### v8 → v7 Export Tool

For users who need to revert to an older Frontier version:

```
frontier-cli --export-v7 source-v8.root output-v7.root
```

**Behavior**:
- Transcode all UTF-8 strings to MacRoman
- Characters not representable in MacRoman → replaced with `?` (with warning log)
- Redistribute hash nodes into 11 fixed buckets using legacy hash function
- Write as v7 database format
- Report count of lossy conversions

### Backup Strategy

- v7→v8 migration always creates `.v7.rbk` backup before modifying
- Backup path displayed to user with instructions for reverting
- Backup is a complete, functional v7 database

---

## Communication Plan

### Before Release

1. **Deprecation notices** in release notes for 1-2 releases before breaking changes
2. **Migration guide** published to documentation site
3. **Script analyzer** available for download
4. **Blog post/announcement** explaining the rationale and migration path

### With Release

1. **Release notes** with clear list of breaking changes
2. **In-app messaging** when opening v7 databases (offer upgrade, explain implications)
3. **Documentation** for all new and changed verbs

### After Release

1. **Support channels** for migration questions
2. **Known issues** tracking for edge cases discovered in production
3. **Hotfix process** for critical migration bugs

---

## Runtime Validation (Debug Builds)

In debug/development builds, add runtime assertions to catch encoding errors early:

```c
// Assert that a string value contains valid UTF-8
#ifdef FRONTIER_DEBUG
    assert(utf8_validate(stringbaseaddress(bs), stringlength(bs)));
#endif
```

Places to add validation:
- `setstringvalue()` — every new string value must be valid UTF-8
- `hashtablelookup()` — every key lookup must use UTF-8 key
- `hashpacktable()` — every packed key must be valid UTF-8
- Database load path — every string loaded from v8 database must validate

---

## Verification

- All unit tests pass
- All integration tests pass with UTF-8 content
- Performance benchmarks meet targets
- v7→v8→v7 round-trip produces identical databases (for ASCII/MacRoman content)
- Script analyzer correctly identifies affected patterns in test corpus
- Downgrade tool produces functional v7 databases from v8 input
- Preview build feedback incorporated before default-on stage

---

## Risks

**Medium**. The primary risk at this stage is incomplete test coverage — a missed edge case that only manifests in production.

1. **Untested code paths**: Some subsystems may not be exercised by the test suite. Comprehensive coverage of the multi-byte corpus across all verb categories mitigates this.
2. **Production data diversity**: Real-world databases may contain unexpected encoding mixtures from years of use. The migration tool must handle malformed input gracefully (warn and skip, don't crash).
3. **Performance surprise**: Some operation may be much slower with UTF-8 than with single-byte encoding. Benchmarking across representative workloads catches this before release.
4. **Rollback friction**: If a critical bug is found post-release, users need the downgrade tool to revert. The tool must be ready and tested before v8 ships.
