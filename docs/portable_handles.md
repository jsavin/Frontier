# Portable Handle Runtime

Status
- State: In Progress
- Phase: 0.5.x
- Last Updated: 2025-09-29
- Notes: Portable handle semantics implemented; see planning/0.5.21_portable_handle_runtime.md.

Related Docs
- planning/0.5.21_portable_handle_runtime.md
- planning/DEVELOPER_QUICKSTART_HEADLESS.md

Change Log
- 2025-09-29: Initialized template sections (Status/Related Docs/Change Log).

This module re-implements classic Mac OS "Handle" behaviour in pure C so the
Frontier runtime can allocate movable blocks on platforms that no longer expose
Toolbox memory APIs.  It mirrors the semantics relied on throughout the core:

* Handles are stable pointers to pointers. The data buffer may move when
  resized, but the handle value itself never changes.
* `HLock`/`HUnlock` bump a lock count so callers can assert correct pairs while
  receiving the raw data pointer.
* Resizing preserves existing contents and zero-fills any newly allocated tail
  bytes to match historical expectations.

The implementation lives in `Common/source/portable_handles.c` with public
prototypes in `Common/headers/portable_handles.h`.  Platforms that do not ship
Carbon headers automatically enable the portable layer via
`FRONTIER_USE_PORTABLE_HANDLES` (set in `frontierdefs.h`).  Projects targeting
other environments may opt-in explicitly by defining the same macro on their
compiler command line.

## Integration notes

* Existing code continues to call `NewHandle`, `DisposeHandle`, etc. through
  `macconv.h`. When the portable flag is active, those macros now map directly
  onto the new runtime.
* The Windows implementation in `Common/stubs/megastubs.c` is left untouched; it
  continues to provide the classic behaviour on that platform.  The portable
  layer covers macOS (when Carbon is unavailable) and POSIX targets such as
  Linux. Non-macOS targets should also pick up `Common/headers/osincludes_portable.h`
  (pulled in automatically by `frontier.h`) to avoid Carbon dependencies.
  QuickTime support has been removed entirely, so the portable path no longer
  needs to emulate those APIs.
* `TempNewHandle`/`TempDisposeHandle` are backed by the same allocator. They set
  `memFullErr` on failure to preserve existing error handling logic.

## Next steps

* Audit the call sites that rely on Carbon headers and move them behind the same
  portability guards so the core can be built on Linux without pulling in
  deprecated Apple frameworks.
* Add focussed unit tests that exercise the handle manager (allocation,
  resizing, lock counting). A first smoke test lives in tests/handle_tests.c
  (build with make -C tests).
