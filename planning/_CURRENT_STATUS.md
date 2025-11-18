# Carbon Migration – Current Status

**Last Updated**: November 17, 2025 (day)  \
**Branches in flight**: `feature/carbon-migration-plan`, `feature/headless-system-bootstrap`

## Summary
- Guard-malloc traces still show `docinfo.subject` underflowing after the doc-info block fires an extra `UnuseMemory` (observed in `pgReadHandlerProc`/`pgScrapMemoryRead`), so the handle balance never stabilizes before `DisposeMemory`.
- Paige now logs doc-info handle assignments and the headless runtime re-registers those handles during load, which gives visibility into the failing reference counts even though the crash persists.
- The Paige CMake build automatically rebuilds `libpaige.a` whenever sources change, ensuring the instrumentation stays tied to the latest runtime artifacts.
 
## Medium-Term Goals
- Resolve the doc-info handle underflow path so the carbon migration guard-malloc run no longer crashes before doc-info serialization.
- Confirm runtime parity between the headless bootstrap and UI routes (e.g., `system.verbs → kernelcall → EFP`) once Paige integration stops triggering handles use/unuse miscounts.
- Keep the automation/IPC boundary documented so headless builds can safely expose JSON-RPC while UI builds retain OSA, as tracked in `planning/TODO_future_improvements.md`.

## Long-Term Goals
- Finish the Phase 2 runtime context refactor so simultaneous CLI/headless clients share `FrontierContext` backend handles without touching globals.
- Land the Phase 3 concurrency/task-context plan, widen paging/tracing to multi-threaded guard-malloc tests, and keep `planning/TODO_future_improvements.md` aligned with heading priorities.
- Expand developer tooling (OSS compliance, doc server, LSP work) so future IDE/bridge projects and release automation can rely on the documented TODO backlog.

## Next Steps
- Remove/guard the redundant `UnuseMemory` in `pgReadHandlerProc` after the doc-info snapshot (and any matching calls in the scrap reader) so handles stay non-negative.
- Confirm the scrap reader’s `UnuseMemory` path gets balanced with a corresponding `UseMemory` (audit `pgScrapMemoryRead` and dependent hooks).
- Once the unbalanced path is fixed, rerun the guard-malloc runtime test to ensure we can close out the doc-info underflow case.
