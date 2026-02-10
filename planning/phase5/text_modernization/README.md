# Text Modernization Roadmap

**Status**: Planning
**Last Updated**: 2026-02-09
**Scope**: UTF-8 adoption, Pascal string elimination, hashtable algorithm rework, database format v8

---

## Strategic Vision

Frontier was born in the Classic Mac OS era. Its core text handling reflects that heritage: Pascal strings (`bigstring` / `Str255`) with a 255-byte limit, MacRoman encoding assumptions, byte-oriented parsing, and a hashtable algorithm that only examines the first and last characters of keys.

This worked in 1992. It doesn't work for a cross-platform, modern runtime.

**The goal**: Make UTF-8 the canonical internal text encoding across the entire system — from the language runtime through the database format to the UserTalk scripting surface. Simultaneously, modernize the hashtable algorithm that underpins the ODB, since both changes touch the same on-disk structures and should ship as a single database format bump (v8).

**Why now**: Text handling is foundational to what Frontier is and to the applications built on top of it. Every subsystem touches strings. Every database stores them. Deferring this work means every new feature inherits legacy encoding debt. The longer we wait, the more code accumulates that assumes single-byte characters.

### Acceptable Breakage Policy

Text encoding and timezone handling are the **only two areas** where UserTalk-level breaking changes are acceptable. This is a deliberate, one-time investment to move the platform into the modern era. The migration path must be clear, well-documented, and provide tooling to help script authors update their code.

---

## Phase Overview

| Phase | Name | Risk | Breakage | Depends On | Can Parallel With |
|-------|------|------|----------|------------|-------------------|
| 1 | [Bridging & Safety](phase1_bridging_and_safety.md) | Low | None | — | 3 |
| 2 | [UTF-8 Infrastructure](phase2_utf8_infrastructure.md) | Low | None | 1 | 3 |
| 3 | [Hashtable Modernization](phase3_hashtable_modernization.md) | Medium | None (in-memory) | — | 1, 2 |
| 4 | [Core Runtime Conversion](phase4_core_runtime_conversion.md) | High | None (internal) | 1, 2 | — |
| 5 | [Database Format v8](phase5_database_format_v8.md) | Very High | Format change | 3, 4 | — |
| 6 | [UserTalk Compatibility](phase6_usertalk_compatibility.md) | High | **Yes** | 4 | 5 |
| 7 | [Testing & Rollout](phase7_testing_and_rollout.md) | Medium | Depends | All | — |

### Dependency Graph

```
Phase 1 (Bridging) ──→ Phase 2 (UTF-8 Infra) ──→ Phase 4 (Core Runtime) ──┐
                                                                            ├──→ Phase 5 (DB v8) ──→ Phase 7 (Rollout)
Phase 3 (Hashtable) ────────────────────────────────────────────────────────┘         ↑
                                                                                      │
                                            Phase 6 (UserTalk Compat) ────────────────┘
```

**Key insight**: The string modernization track (Phases 1→2→4) and the hashtable track (Phase 3) are independent workstreams that converge at Phase 5 — one database format bump, not two.

---

## Database Format Evolution

| Version | Era | Addresses | Hash Algorithm | Buckets | String Encoding |
|---------|-----|-----------|----------------|---------|-----------------|
| v6 | Legacy | 32-bit LE | first+last char | 11 fixed | MacRoman (Pascal) |
| v7 | Current | 64-bit BE | first+last char | 11 fixed | MacRoman (Pascal) |
| v8 | **This plan** | 64-bit BE | FNV-1a (all chars) | Dynamic | UTF-8 |

The v7→v8 migration combines:
- Modern hash algorithm with dynamic bucket sizing
- UTF-8 encoded string keys (replacing Pascal length-prefixed MacRoman)
- Encoding metadata in database header
- Extends the ADR-002 `db_context` pattern with encoding and hash algorithm fields

---

## Current State of String Usage

**Scale**: ~6,550 BIGSTRING usages across 348 files; ~1,113 stack-allocated `bigstring` locals across 141 source files; 35+ callback typedefs use `bigstring` in their signature.

**Already completed**:
- Safe bridging helpers: `bs_from_c()` and `c_from_bs()` in `strings_extras.c`
- New code in `portable/` and `frontier-cli/` uses C strings at boundaries
- ADR-002 context-based format versioning pattern established (for v6→v7)

**Hashtable algorithm**:
- Current `hashfunction()` in `langhash.c:1339` uses only first + last character, modulo 11 buckets
- Padded numbers ("0000001" through "0000999") distribute pathologically — keys sharing first/last chars cluster
- No dynamic resizing; tables with 1000+ entries have average chain length ~90

---

## Related Documents

### Within This Directory

- [Phase 1: Bridging & Safety](phase1_bridging_and_safety.md)
- [Phase 2: UTF-8 Infrastructure](phase2_utf8_infrastructure.md)
- [Phase 3: Hashtable Modernization](phase3_hashtable_modernization.md)
- [Phase 4: Core Runtime Conversion](phase4_core_runtime_conversion.md)
- [Phase 5: Database Format v8](phase5_database_format_v8.md)
- [Phase 6: UserTalk Compatibility](phase6_usertalk_compatibility.md)
- [Phase 7: Testing & Rollout](phase7_testing_and_rollout.md)

### Superseded Documents (Historical Reference)

- [superseded_string_and_text_modernization.md](superseded_string_and_text_modernization.md) — was `planning/phase5/string_and_text_modernization.md`
- [superseded_utf8_transition_plan.md](superseded_utf8_transition_plan.md) — was `planning/phase5/utf8_transition_plan.md`
- [superseded_hash_table_modernization_strategy.md](superseded_hash_table_modernization_strategy.md) — was `planning/phase2/0.5.16_hash_table_modernization_strategy.md`

### Elsewhere in the Codebase

- `planning/architectural_decision_records/ADR-002-context-based-format-versioning.md` — The `db_context` pattern that Phase 5 extends
- `planning/phase3/processor_audits/string.md` — String verb inventory (60 verbs, encoding tier analysis)
- `planning/archive/phase3/carbon_migration/strings_replacement_plan.md` — STR# resource migration (separate concern)
- `docs/VERB_IMPLEMENTATION_GUIDE.md` — Verb implementation patterns
- `Common/source/langhash.c` — Current hash function and hashtable implementation
- `Common/headers/lang.h` — `tyhashtable`, `tyhashnode`, `ctbuckets` definitions
- `Common/source/strings.c` — Core Pascal string utilities
- `Common/headers/strings.h` — String API declarations
