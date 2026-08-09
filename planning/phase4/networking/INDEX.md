# Phase 4: TCP Networking - Planning Index

**Created**: 2026-01-16
**Last Updated**: 2026-01-25
**Status**: Phase 1A/1B/3 COMPLETE ✅, Phase 2 (Buffered I/O) NEXT 🚀

---

## Status correction (2026-08-09)

**TCP is done: 23/23 verbs complete, landed in PR #361 (`c2ec8d109`), which also migrated the
implementation off the legacy API.** Everything below describing remaining phases is obsolete:

- The "Phase 2 (Buffered I/O) NEXT" status line, the Quick Start worktree instructions, and the
  Phase Summary table's "🚀 NEXT" / "After 2" rows all describe January 2026 state. There is no
  remaining TCP phase to kick off.
- The "**Total**: 22 kernel verbs (13 complete, 9 remaining)" count is wrong twice over — the final
  surface is **23 verbs, all implemented**.
- The verb lists under "Completed Verbs (13)" and "Next Priority (Phase 2 - 4 verbs)" are likewise
  a partial snapshot; the Phase 2 and Phase 4 verbs they list as pending are implemented.

The reference material below (architecture, dependencies, security findings) is still useful. Only
the phase status and verb counts are stale. Direction of record for current work:
`product/VISION_1_0.md` and `product/plans/2026-08-09-phase-0-1-execution-plan.md`.

---

## Quick Start

> **Obsolete as of 2026-08-09** — TCP Phase 2 was completed in PR #361. Do not create this worktree.

**Current Priority: TCP Phase 2 (Buffered I/O)**
1. Read [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md) - Phased execution plan
2. Create worktree: `git worktree add ../Frontier-tcp-phase2 -b feature/tcp-phase2`
3. Follow `/doit` process for Phase 2

---

## Documents

| Document | Purpose | Lines | Status |
|----------|---------|-------|--------|
| [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md) | **Primary execution plan** - phases, tasks, Haiku/Sonnet markers | ~450 | Active |
| [TCP_VERBS_ANALYSIS.md](TCP_VERBS_ANALYSIS.md) | Complete API analysis - 22 kernel verbs, signatures, behaviors | ~800 | Reference |
| [NETWORKING_ARCHITECTURE.md](NETWORKING_ARCHITECTURE.md) | POSIX C architecture - data structures, code examples | ~1100 | Reference |
| [DEPENDENCIES.md](DEPENDENCIES.md) | Build system, headers, platform requirements | ~200 | Reference |
| [EXISTING_IMPLEMENTATION_NOTES.md](EXISTING_IMPLEMENTATION_NOTES.md) | Analysis of WinSockNetEvents.c reference code | ~170 | Reference |
| [PRELIMINARY_FINDINGS.md](PRELIMINARY_FINDINGS.md) | Initial discovery notes (superseded by above) | ~120 | Reference |

**Archived Documents** (see `planning/archive/completed-workstreams/`):
- **TCP_PHASE1A_PREFLIGHT.md** - Pre-flight checklist for Phase 1A (✅ COMPLETE, PRs #327, #329, #330)

---

## Phase Summary

| Phase | Scope | Model | Status |
|-------|-------|-------|--------|
| **1A** | Core Socket (7 verbs) | 🟢 Haiku | ✅ COMPLETE (PR #327) |
| **1B** | DNS/Address (5 verbs) | 🟢 Haiku | ✅ COMPLETE (PR #330) |
| **3** | Server Ops (2 verbs) | 🟡 Sonnet | ✅ COMPLETE (PR #330) |
| **2** | Buffered I/O (4 verbs) | 🟢 Haiku | ✅ COMPLETE (PR #361) |
| **4** | Advanced (2 verbs) | 🟢 Haiku | ✅ COMPLETE (PR #361) |

**Total** *(corrected 2026-08-09)*: **23 kernel verbs — 23 complete, 0 remaining** (PR #361). The
original estimate of 22 verbs and the "13 complete, 9 remaining" split below are superseded.

**Completed Verbs** (13):
- Phase 1A: `tcp.openAddrStream`, `tcp.openNameStream`, `tcp.readStream`, `tcp.writeStream`, `tcp.closeStream`, `tcp.abortStream`, `tcp.countConnections`
- Phase 1B: `tcp.addressEncode`, `tcp.addressDecode`, `tcp.nameToAddress`, `tcp.addressToName`
- Phase 3: `tcp.listenStream`, `tcp.closeListen`

**Phase 2 verbs** — implemented in PR #361, not pending:
- `tcp.flushStream`, `tcp.setStreamBuffer`, `tcp.getStreamBuffer`, `tcp.drainStream`

---

## Key Findings

1. **22 kernel verbs** need C implementation
2. **Existing reference**: WinSockNetEvents.c has POSIX (GUSI) code paths
3. **Thread safety critical**: Global `sockstack[]` needs mutex
4. **Blocking dependency**: Phase 3 requires **general-purpose parameterized callback infrastructure** (P0a work)
   - See: `planning/phase4/p0a-critical-thread-safety/CALLBACK_INFRASTRUCTURE.md`
   - NOT TCP-specific - enables ALL parameterized callbacks (TCP, window, outline, system)
   - Extends existing `langopruncallbackscripts()` pattern to pass parameters
5. **HTTP client works after Phase 2**: Existing UserTalk scripts use primitives

---

## Open Questions

See [IMPLEMENTATION_PLAN.md#open-questions-for-user](IMPLEMENTATION_PLAN.md#open-questions-for-user):

1. Phase 3 timing vs. threading infrastructure
2. IPv6 support in Phase 1
3. Test environment network access
4. Windows parallel development

---

## Validation Milestones

- ✅ **After Phase 1A**: Basic TCP connectivity works (PR #327)
- ✅ **After Phase 1B**: DNS resolution works (PR #330)
- ✅ **After Phase 3**: TCP server listener works (PR #330)
- ✅ **After Phase 2**: Basic HTTP GET works (buffered I/O milestone) — PR #361
- ✅ **After Phase 2**: `tcp.httpClient` script works — PR #361
- ✅ **After Phase 4**: Advanced features complete — PR #361

## Completed Work

**PR #327** (2026-01-19) - TCP Phase 1A: Core Socket Operations
- 6 verbs implemented, 710 lines C code
- 28 C unit tests for stream lifecycle, thread-safety, security
- POSIX BSD sockets with mutex protection

**PR #329** (2026-01-19) - TCP Testing Infrastructure
- 93 total tests (28 C unit, 29 local integration, 20 network integration)
- Security features validated (SSRF/DNS rebinding protection)

**PR #330** (2026-01-24) - TCP Phase 1B (DNS/Address) + Phase 3 (Server Operations)
- Phase 1B: 5 verbs for IP address encoding/decoding and DNS operations
- Phase 3: 2 verbs for server-side operations (listener infrastructure)
- 57 integration tests (23 for Phase 1B, 34 for Phase 3)
- Listener registry, per-listener accept threads, callback dispatch

**PR #361** (`c2ec8d109`) - TCP 100% Coverage + Legacy API Migration
- Completed Phase 2 (buffered I/O) and Phase 4 (advanced), bringing TCP to **23/23 verbs**
- Migrated the implementation off the legacy API
- Planning docs updated in follow-up commit `7395d053c`

**Impact**: Frontier now has production-ready TCP networking layer supporting client-server architecture.
