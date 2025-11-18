# Carbon Migration – Current Status

**Last Updated**: November 17, 2025 (day)  \
**Branches in flight**: `feature/carbon-migration-plan`, `feature/headless-system-bootstrap`

## Summary
- Guard-malloc traces still show `docinfo.subject` underflowing after the doc-info block fires an extra `UnuseMemory` (observed in `pgReadHandlerProc`/`pgScrapMemoryRead`), so the handle balance never stabilizes before `DisposeMemory`.
- Paige now logs doc-info handle assignments and the headless runtime re-registers those handles during load, which gives visibility into the failing reference counts even though the crash persists.
- The Paige CMake build automatically rebuilds `libpaige.a` whenever sources change, ensuring the instrumentation stays tied to the latest runtime artifacts.

## Next Steps
- Remove/guard the redundant `UnuseMemory` in `pgReadHandlerProc` after the doc-info snapshot (and any matching calls in the scrap reader) so handles stay non-negative.
- Confirm the scrap reader’s `UnuseMemory` path gets balanced with a corresponding `UseMemory` (audit `pgScrapMemoryRead` and dependent hooks).
- Once the unbalanced path is fixed, rerun the guard-malloc runtime test to ensure we can close out the doc-info underflow case.
