# Phase 4: TCP Networking - Planning Index

**Created**: 2026-01-16
**Status**: Planning complete, ready for Phase 1 execution

---

## Quick Start

**To begin implementation**:
1. Read [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md) - Phased execution plan
2. Create worktree: `git worktree add ../Frontier-tcp-phase1a -b feature/tcp-phase1a`
3. Follow `/doit` process for Phase 1A

---

## Documents

| Document | Purpose | Lines |
|----------|---------|-------|
| [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md) | **Primary execution plan** - phases, tasks, Haiku/Sonnet markers | ~450 |
| [TCP_VERBS_ANALYSIS.md](TCP_VERBS_ANALYSIS.md) | Complete API analysis - 22 kernel verbs, signatures, behaviors | ~800 |
| [NETWORKING_ARCHITECTURE.md](NETWORKING_ARCHITECTURE.md) | POSIX C architecture - data structures, code examples | ~1100 |
| [DEPENDENCIES.md](DEPENDENCIES.md) | Build system, headers, platform requirements | ~200 |
| [EXISTING_IMPLEMENTATION_NOTES.md](EXISTING_IMPLEMENTATION_NOTES.md) | Analysis of WinSockNetEvents.c reference code | ~170 |
| [PRELIMINARY_FINDINGS.md](PRELIMINARY_FINDINGS.md) | Initial discovery notes (superseded by above) | ~120 |

---

## Phase Summary

| Phase | Scope | Model | Status |
|-------|-------|-------|--------|
| **1A** | Core Socket (7 verbs) | 🟢 Haiku | Ready |
| **1B** | DNS/Address (6 verbs) | 🟢 Haiku | Ready |
| **2** | Buffered I/O (4 verbs) | 🟡 Sonnet | After 1A/1B |
| **3** | Server Ops (3 verbs) | 🟡 Sonnet | Needs threading |
| **4** | Advanced (2 verbs) | 🟢 Haiku | After 2 |

**Total**: 22 kernel verbs

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

- **After Phase 2**: Basic HTTP GET works
- **After Phase 2**: `tcp.httpClient` script works
- **After Phase 3**: Echo server works
