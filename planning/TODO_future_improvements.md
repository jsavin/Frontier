# TODO: Future Improvements

Status
- State: In Progress
- Phase: Multi-Phase Roadmap
- Last Updated: 2025-11-20 (Night)
- Owner: Codex
- Notes: Master backlog for post-carbon follow-ups; items move to `_STATUS_ARCHIVE.md` once completed.

Status
- State: Living Document
- Phases: Multi-Phase Roadmap
- Last Updated: 2025-11-08
- Notes: Organised to mirror the current phase plan (see `planning/phase_overview.md`).

Related Docs
- planning/Frontier_Refactoring_Plan.md
- planning/INDEX.md
- planning/phase3/ui_abstraction/PHASES.md

Change Log
- 2025-11-08: Consolidated ADR/issues backlog into this doc with P-level priorities.
- 2025-11-08: Added UserTalk language server and interop bridge planning notes.
- 2025-10-25: Captured Cancoon/About window UI preservation requirement.
- 2025-10-12: Updated links, clarified timelines by phase.
- 2025-09-29: Initial draft (memory management audit, hash table modernisation notes).

Priority Key
- **P0 (Must / Do First):** Blocks current roadmap milestones or security/stability goals.
- **P1 (Pre-Prod Critical):** Required before wider rollout/production; can follow P0 work.
- **P2 (Future / Nice-to-Have):** Valuable improvements once P0/P1 are on track.

## Phase 1/2 — Memory Management Audit (Rolling)

**Priority:** P0 — Must stay ahead of crash/UB risk  
**Timeline:** Begin immediately; finish core audit alongside Phase 2 architecture work.

Goals
- Identify and fix unsafe or leaky patterns across the legacy C codebase.
- Standardise ownership and lifetime for heap objects and Handles.
- Reduce Undefined Behaviour (UB) / ASan / UBSan findings (alignment, VLAs, function pointer casts).

Scope (examples, not exhaustive)
- Remove or replace variable-length arrays (VLAs) with fixed or heap buffers.
- Fix misaligned reads/writes (e.g., Handle stores) with safe copies.
- Audit `malloc`/`newclearhandle`/`newhandle`/`newtexthandle` call sites for matching free/dispose patterns.
- Ensure error paths and early returns release allocations.
- Verify temp stack usage (`pushvalueontmpstack`/`cleartmpstack`/`exemptfromtmpstack`) for all heap values.
- Replace unsafe pointer casts (e.g., function pointer mismatches) with shims/adapters.
- Prefer `size_t` for sizes/lengths; validate bounds before copy/move.

Deliverables
- Tracking issue/checklist per module (lang, memory, strings, op*, db, tables, UI stubs).
- Sanitiser-clean headless test runs with documented suppressions where unavoidable.
- Coding guidelines covering ownership conventions and helper APIs.

Initial Targets
- `Common/source/langstartup.c`: charset initialisation allocations — verify post‑audit.
- `Common/source/memory*.c`: alignment-safe Handle ops and memcpy patterns — in progress.
- `Common/source/langcallbacks.c`: remove VLAs in error printing — done; re-audit other debug paths.
- `Common/source/langtree.c`: LP64 packing guards for treenodes — done; verify other packed structs.

Process
- Enable ASan/UBSan in CI for tests; treat new sanitiser errors as must-fix.
- Add optional leak checks where feasible; label noisy false positives.
- Document ownership for public APIs in headers and planning notes.

## Phase 2 — UI Boundary via Ports & Adapters

**Priority:** P0 — Core must stay UI-agnostic for headless + CLI builds.  
**Timeline:** Active during Phase 2 UI abstraction.

Goals
- Define a narrow `UIServices` interface reachable from core modules only.
- Move AppKit/Carbon and `#if FRONTIER_HEADLESS` conditionals into adapter layers.
- Split builds into `libfrontier_core` + adapters (`headless`, `mac-ui`, future shells).

Scope
- Introduce adapter targets for tests/headless that satisfy `UIServices`.
- Audit headers for UI types and push them behind the boundary.
- Ensure CLI/test harness link only headless adapters; UI shells link UI adapter.

Reference Docs
- `planning/ui_abstraction/phase2/analysis.md`
- `planning/ui_abstraction/phase2/architecture.md`
- `planning/ui_abstraction/phase2/migration_plan.md`

## Phase 2 — Global Runtime Context & Lifecycle

**Priority:** P0 — Blocks reentrancy, testing, and future threading.  
**Timeline:** Start now alongside UI boundary refactor.

Goals
- Replace implicit globals with explicit `FrontierContext`-style objects that carry subsystem handles/configuration.
- Standardize init → attach databases → run → teardown lifecycle hooks.
- Provide a compatibility shim so legacy entry points can pass/use contexts without massive rewrites.

Scope
- Update public APIs to accept contexts; forbid new APIs from reaching globals.
- Add helpers to construct/destroy contexts in tests and CLI.
- Document lifecycle guarantees in headers and `planning/Frontier_Refactoring_Plan.md`.

## Phase 2 — Headless EFP Routing Parity

**Priority:** P0 — Needed to keep headless/kernel routing consistent.  
**Timeline:** Execute during current headless bootstrap work.

Goals
- Keep `system.verbs -> kernelcall -> EFP` as the single routing path.
- Treat the dotted-name shim as a bootstrap fallback behind a `FRONTIER_HEADLESS_BOOTSTRAP_EFP` flag.
- Provide generated wrappers or minimal `system.verbs` loaders so headless gets the same routing semantics without persisting a shim DB.

Scope
- Add explicit `kernel.call(family, verb, args)` hooks for tests.
- Gate/remove the shim by default; document deprecation path.
- Write parity tests asserting shim vs. kernelcall behavior until shim is deleted.

Reference Docs
- `planning/EFP_HEADLESS_NOTES.md`
- `planning/DECISIONS.md`

## Phase 2 — Networking Architecture & Security

**Priority:** P0 — Default CLI/server exposure must be safe.  
**Timeline:** Implement before broad CLI distribution.

Goals
- Ship HTTP/WebSocket scaffolding with secure defaults (loopback bind, sane timeouts, request limits).
- Provide middleware hooks for auth, rate limiting, logging, and CORS.
- Document TLS expectations (reverse proxy first; revisit embedded TLS later).

Scope
- Centralize configuration via flags/env and document precedence.
- Build regression tests for default security posture.
- Capture observability hooks (structured logs/metrics) for future automation.

Reference Docs
- `planning/1.0_phase1_cli_implementation_plan.md`
- `planning/1.1_phase1_implementation_summary.md`

## Phase 2 — OSA / IPC Strategy

**Priority:** P1 — Necessary before macOS UI + headless automation converge.  
**Timeline:** Design alongside networking work; implement before desktop beta.

Goals
- Keep OSA/AppleEvents inside the UI adapter only; never link into headless builds.
- Provide JSON-RPC (HTTP/WebSocket) automation endpoints for headless/CLI.
- Define a transport-agnostic RPC surface (stdio/socket) to unblock future adapters.

Scope
- Stub/migrate OSA entry points so tests can mock automation without macOS frameworks.
- Document capability matrix for headless vs. UI builds.
- Prototype RPC spec and authentication story.

Reference Docs
- `planning/headless_stubbed_behavior_matrix.md`
- `planning/no_ui_linkage_policy.md`

## Phase 2 — File I/O & Path Policy

**Priority:** P1 — Prevents path bugs as we move cross-platform.  
**Timeline:** Align with database migration tooling updates.

Goals
- Canonicalize internal paths to POSIX style; keep legacy parsing at the boundaries.
- Support project-root relative paths with an eventual alias registry.
- Provide migration helpers for stored absolute paths inside databases.

Scope
- Update file verbs and CLI commands to emit canonical paths.
- Capture compatibility notes for macOS/HFS edge cases.
- Add regression tests covering relative/absolute conversions.

Reference Docs
- `planning/database_path_canonicalization.md`

## Phase 2 — Unicode Strategy

**Priority:** P1 — Required before accepting modern data + APIs.  
**Timeline:** Start once memory audit stabilizes; finish before major format changes.

Goals
- Adopt UTF-8 internally for strings, script parsing, and file paths.
- Normalize ingress text to NFC while offering a compatibility bypass for migrations.
- Define locale-independent casing rules for identifiers and table keys.

Scope
- Audit string/token APIs, hashing, comparisons, and serialization formats.
- Update tests to cover normalization/case-folding.
- Document encoding expectations in headers and developer docs.

Reference Docs
- `planning/Frontier_Refactoring_Plan.md`
- `planning/0.5.13_usertalk_language_summary.md`
## Phase 3 — Hash Table Modernisation

**Priority:** P1 — Needed before Phase 3 ships to users  
**Timeline:** Execute after core architecture upgrades stabilise (Phase 2 exit).

Background
- Current hash table uses first/last character only; bucket count fixed at 11.
- Poor distribution causes performance issues with large tables.

Proposed Changes
- Version 8 database format with modern hash tables.
- FNV-1a (or similar) hash implementation.
- Dynamic bucket sizing and load-factor-based resizing.

Migration Strategy
- Automatic conversion from Version 7 → Version 8 with rollback path.
- Maintain backward compatibility mode where needed.
- Benchmark improvements using representative databases.

Reference Docs
- `planning/phase2/0.5.16_hash_table_modernization_strategy.md`
- `planning/phase2/0.5.19_phase1_migration_implementation_complete.md`

## Phase 3 — Concurrency Model & Task Contexts

**Priority:** P1 — Needed before headless/server builds scale.  
**Timeline:** Kick off during late Phase 2; land early Phase 3.

Goals
- Establish C11/pthreads-based abstractions (with optional GCD adapter) for multi-threaded workloads.
- Adopt single-writer/multi-reader rules for databases and the language runtime.
- Introduce explicit task/request contexts for CLI/server operations; eliminate hidden thread-local globals.

Scope
- Wrap key subsystems (DB, language engine, I/O) in thread-safe guards or queues.
- Add stress tests for concurrent verb execution and database operations.
- Document synchronization primitives and non-goals (e.g., no shared mutable state without locks).

Reference Docs
- `planning/Frontier_Refactoring_Plan.md`
- `planning/INDEX.md`

## Phase 3 — Headless Migration Options

**Priority:** P2 — Useful once CLI/adapter work is steady  
**Timeline:** After Phase 3 CLI/adapter work is stable.

Background
- Some deployments need automated database migration without interactive prompts.

Proposed Changes
- Add configuration surface (CLI flag, environment variable, preference) to auto-migrate.
- Ensure headless builds honour the setting while retaining safe defaults for interactive shells.

Reference Docs
- `planning/phase3/system_verbs_bootstrap_plan.md`
- `planning/phase3/DEVELOPER_QUICKSTART_HEADLESS.md`

## Phase 3 — Date/Time Representation Modernization

**Priority:** P1 — Needed for accurate headless behavior and future interop.  
**Timeline:** Start once the `clock.*` and `script.*` verbs run cleanly via search paths.

Goals
- Preserve the legacy Mac epoch semantics (seconds since the Frontier “fixed date”) so migrated databases remain faithful.
- Add a portable conversion layer that can emit and consume POSIX-friendly timestamps (milliseconds since the Unix epoch) without losing timezone fidelity.
- Codify the historical timezone heuristic (user override → server-configured TZ → local system clock) so headless/CLI builds match the classic UI.

Scope
- Document the current behavior in the docserver (`clock.now`, table metadata) and `docs/database_architecture.md`, noting when the portable formatter should be used.
- Implement helpers in `langdate.c`/`clock.*` that convert between the Mac epoch and POSIX milliseconds, factoring in timezone heuristics.
- Extend tests (CLI + runtime) to cover raw numeric values, formatted strings, and round-trips across multiple timezones.
- Long-term: expose dual representations (raw number + ISO 8601 string) via the CLI and automation APIs once the converter is proven.

## Phase 3 — Remote Runtime + Local Guest Databases

**Priority:** P1 — Critical for the North Star multi-client workflow (but not required for the first headless runtime milestone).  
**Timeline:** Design after the base headless runtime is stable; implement before remote/daemon scenarios ship broadly.

Goals
- Allow a local client (CLI or desktop UI) connected to a remote or daemon-owned runtime to mount additional guest `.root` files that live on the client machine.
- Present those client-hosted guests to the runtime exactly as classic Frontier would via `system.compiler.files`, so compiled scripts see one consistent scope regardless of where the guest file resides—while keeping the scope **private to the user’s session** (user-owned scripts can see local guests; daemon-owned/global scripts cannot).
- Ensure security/isolation: remote runtimes must explicitly accept/deny local guest mounts, and the transport must handle streaming table data safely.

Scope
- Extend the runtime’s guest-database registry/handshake to accept remote attachments (likely via RPC) and expose them under `system.compiler.files` with stable identifiers.
- Define the protocol for syncing guest table contents (initial snapshot + change notifications) between client and daemon.
- Update docs/tests to cover the multi-host scenario (local guest, remote runtime, shared namespace) so future shells don’t regress this behaviour.

## Future Considerations (Phase 4/5 and Beyond)


## Phase 3+ — Rich Text Type (Future Consideration)

**Priority:** P2 — Nice-to-have once migration/parity work is done.

Goals
- Define a native `richtextType` that can carry lightweight RTF/HTML payloads without hard depending on Paige.
- Update coercion and serialization paths (`string()`, `langexternalpacktotext`, migrator) to treat `richtextType` as a first-class value.
- Expose verbs for converting to/from Markdown/HTML when the runtime grows those emitters.

Notes
- This stays on the shelf until the WPText extractor + styled RTF exporter land and we understand how often users need richer formats.
- Capture design decisions/TODOs under `planning/paige_text_extractor.md` so the work can restart when timing is right.

### WPText → RTF Migration [P2]
- Canonicalize `WPText` objects to UTF-8 aware RTF, keeping minimal metadata (creator, conversion info) for backward compatibility.
- Provide conversion verbs/tooling for `WPText ↔ RTF`, plus CLI commands to export/import `.rtf`.
- Maintain readers for legacy `WPText` while flagging new writes as RTF; update storage to drop the 32KB ceiling and test round-trips.

### Performance Optimisations [P2]
- Investigate memory-mapped I/O for large database files.
- Optional compression for on-disk data.
- Improved caching strategies for frequently accessed tables.

### User Experience Enhancements [P2]
- Progress indicators for long-running migrations.
- Batch migration tooling for multiple databases.
- Easy rollback/downgrade support.
- **Cancoon/About window disentanglement:** preserve the 442-byte tyversion2cancoonrecord (About/Home window state) while designing the next UI layer. Eventually we need a per-user UI app that can render the Cancoon window when the runtime runs headless (daemon or service) without losing the msg()/agent log. This will require new IPC hooks so the long-running process can surface the window state safely.
- **Per-user view preferences:** move table font choices, window rects, scroll offsets, etc., out of the shared `.root` and into a user-owned preference store so multiple operators don’t stomp each other’s view state; tackle once the clean v7 rewrite lands.

### Developer Experience [P1]
- Better database inspection/validation tools.
- Automated database integrity checkers.
- Comprehensive API documentation refresh once new infrastructure lands.
- **Strings pipeline (Phase 2 follow-up):** After libyaml-based ingestion is stable, re-enable the bespoke bison/flex YAML parser to match libyaml parity while dropping the third-party dependency. Includes full YAML subset support (indent/dedent, folded strings, metadata fields) and regression tests comparing both pipelines.

### UserTalk Language Server & Bridge (Phase 5+ exploration) [P2]
- **Background:** IDE exploration keeps coming up; we need a language server to power auto-complete, hover docs, and go-to-definition for UserTalk plus a bridge for scripting from Python/Rust/Objective-C.
- **Language Server Goals:** Define an LSP-compliant service that can parse `.ftop`/database-backed scripts, expose incremental parse trees, surface runtime metadata (verbs, tables, glossary), and cache per-database symbol indexes so IDEs can offer completions without launching the full Frontier runtime.
- **Bridge Goals:** Provide foreign-function shims so host languages can evaluate UserTalk snippets, call verbs, and subscribe to table change events. Target initial bindings for Python (automation/testing), Rust (systems integrations), and Objective-C (macOS app embedding).
- **Open Questions:** Where to run the LSP (inside `frontier-cli` vs. standalone daemon), how to secure bridge calls against untrusted inputs, and what API surfaces need refactoring for re-entrancy/thread safety.
- **Next Steps:** Spike a thin protocol doc under `planning/phase5/` capturing LSP capabilities and interop surfaces, prototype symbol extraction atop existing parser code, and identify runtime boundaries that need refactoring before we can host the bridge in-process.

These items provide a parking lot for work that spans or follows the current phases. Revisit after each phase review to reprioritise.
