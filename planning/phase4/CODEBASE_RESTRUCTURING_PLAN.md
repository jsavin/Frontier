# Codebase Restructuring Plan: Incremental Subsystem Reorganization

## Context

The Frontier codebase has 233 .c files in a flat `Common/source/` directory and 187 .h files in a flat `Common/headers/`. The largest files exceed 10K lines. ~50+ UI files are compiled only as stubs for the headless CLI. Third-party code is scattered inconsistently. This makes the codebase hard to navigate and reason about.

**Goal**: Reorganize into a logical directory hierarchy reflecting major subsystems, co-locating headers with sources, isolating dead UI code behind a clean boundary, and splitting oversized files — all without regressions, done incrementally one subsystem per PR.

## Strategy: Phase 0 Include Path Trick

**Key insight**: Before moving any files, add `-I` flags for every planned subdirectory to both Makefiles. Once `Common/source/odb/` is on the include path, moving `db.h` there requires zero changes to the 15 files that `#include "db.h"` — the compiler still finds it. This decouples "move files" from "update includes."

This means each subsequent PR only needs to:
1. `git mv` files to their new subdirectory (preserves history)
2. Update Makefile source paths
3. Verify build + tests pass

No `#include` directives in source files need to change until an optional final cleanup pass.

## Target Directory Structure

```
Common/source/
  infra/          # memory, strings, error, logging, ops, timedate, handles, threading
  crypto/         # md5, sha1, whirlpool, base64, langcrypt
  odb/            # db, db_format, db_reader/writer, odbengine, findinfile, dbverbs
  lang/           # lang core: parser, scanner, evaluator, hash, value, external, verbs, etc.
  langext/        # verb domains: html, xml, regexp, ipc, dll, system7, dialog, stringverbs
  op/             # outline: structure, edit, expand, hoist, visit, pack, verbs, xml
  table/          # table: ops, structure, external, pack, verbs, context
  file/           # file I/O: file, filepath, alias, loop, verbs, portable_posix
  net/            # networking: tcpverbs, WinSockNetEvents
  menu/           # menu: core, verbs, bar, editor, pack, resize, find
  script/         # scripting/OSA: scripts, osacomponent, osadroplet, etc.
  shell/          # headless API boundary + UI shell files
  cancoon/        # app framework: cancoon, window, popup, verbs
  ui/             # pure UI (never compiled headless)
    op/           # UI-only outline display files
    table/        # UI-only table display files
    clay/         # Clay browser (UI-only)
    wptext/       # WP text engine (UI-only)
  legacy/         # dead platform code, old format readers

Common/legacy/    # dead third-party: IowaRuntime, windoidWDEF, MoreFiles, etc.
```

## Phased Execution

Each phase is a single PR. Every PR must leave the build working and all tests passing.

### Phase 0: Include Path Infrastructure

- Create all empty subdirectories under `Common/source/`
- Add `-I` flags for each new subdirectory to `frontier-cli/Makefile` and `tests/Makefile`
- **No files move** — pure no-op from the compiler's perspective
- Also delete stale temp files: `.!14526!shellwindowmenu.c`, `.!14531!popup.c`, `.!16354!popup.c`, `shellsysverbs.c.bak`
- Risk: Very low

### Phase 1: Dead Third-Party Cleanup

- Move to `Common/legacy/`: IowaRuntime/, windoidWDEF/, MoreFiles/, FSCopyObject/, UserLandIACToolkit/, old Paige libs under Common/, MySQL/, stubs/
- None of these are compiled into the headless CLI; zero Makefile changes needed
- Risk: Very low

### Phase 2: Crypto (6 files)

**Files**: md5.c/h, sha1dgst.c + sha.h + sha_locl.h, whirlpool.c/h, base64.c + BASE64.H, langcrypt.c/h

- Move to `Common/source/crypto/`
- Nearly self-contained with minimal dependencies on the rest of the codebase
- Update ~6 paths in RUNTIME_SOURCES
- Risk: Low

### Phase 3: ODB (~8 files)

**Files**: db.c, db_format.c/h, db_reader_v7.c, db_reader.h, db_writer_v7.c/h, db_reader_legacy.c, odbengine.c, odbinternal.h, findinfile.c, dbverbs.c, dbinternal.h

- Move to `Common/source/odb/`
- Cohesive unit and active refactoring target (databasedata global elimination)
- db.h is included by ~15 files, but Phase 0 include paths handle resolution
- Update ~7 paths in DATABASE_SOURCES
- Risk: Moderate

### Phase 4: Outline Processor (~15 data-model files)

**Files**: op.c/h, op_context.c/h, opinit.c, opexpand.c, ophoist.c, opvisit.c, opedit.c, opops.c, oplist.c/h, opstructure.c, oprefcon.c, oppack_v7.c, oplangtext.c, opverbs.c/h, opxml.c/h, opinternal.h

- Move to `Common/source/op/`
- Move legacy files (oppack_legacy.c, opverbs_legacy.c) to `Common/source/op/legacy/`
- UI op files (opdisplay, opbuttons, etc.) stay in `Common/source/` for Phase 14
- op.h is included by ~40 files (mostly UI, not compiled)
- Update ~15 paths in RUNTIME_SOURCES
- Risk: Moderate

### Phase 5: Table System (~8 files)

**Files**: tableops.c, tablestructure.c/h, tableexternal.c, tableexternal_common.c/h, tablepack.c, tableverbs.c/h, table_context.c/h, tableinternal.h

- Move to `Common/source/table/`
- Move legacy file (tablepack_legacy.c) to `Common/source/table/legacy/`
- tableexternal.c includes shell.h and cancoon.h but those headers haven't moved and are still found via existing `-I` paths
- Update ~7 paths in RUNTIME_SOURCES
- Risk: Moderate

### Phase 6: Infrastructure (~15 files)

**Files**: memory.c/h, strings.c/h, stringdefs.h, strings_extras.c, error.c/h, logging.c/h, ops.c/h, timedate.c/h, config.c/h, command.c/h, portable_handles.c/h, threadregistry.c/h, frontierdebug.c/h, assert.c, byteorder.h

- Move to `Common/source/infra/`
- **Highest coupling in the codebase** — memory.h is included by 143 files, strings.h by 148
- Phase 0 include paths are essential — without them this phase would require touching 150+ files
- Update ~12 paths in RUNTIME_SOURCES
- Risk: Medium-high

### Phase 7: Language Runtime Core (~25 files)

**Files**: lang.c/h, langvalue.c, langhash.c, langparser.c/h + yytab.h, langscan.c + langtokens.h, langevaluate.c, langexternal.c/h, langverbs.c, langsystypes.c, langops.c, langmath.c/h, langlist.c, langpack.c, langcallbacks.c, langstartup.c, langtmpstack.c, langtrace.c, langtree.c, langwarnings.c/h, langerror.c, langdate.c, langinternal.h, kernelverbdefs.h, kernelverbs.h

- Move to `Common/source/lang/`
- Move legacy file (langexternal_legacy.c) to `Common/source/lang/legacy/`
- lang.h is included by 85 files
- Update ~25 paths in RUNTIME_SOURCES
- Risk: Medium-high (sheer volume of files)

### Phase 8: Language Extensions (~15 files)

**Files**: langhtml.c/h, langxml.c/h, langregexp.c/h, langipc.c/h, langdll.c/h, langsystem7.c/h, langdialog.c, stringverbs.c, and others

- Move to `Common/source/langext/`
- Most are not compiled in the headless build (only langhtml.c and langxml.c are)
- Update 2-3 paths in RUNTIME_SOURCES
- Risk: Low

### Phase 9: File I/O (~8 files)

**Files**: file.c/h, filepath.c, filealias.c/h, filedialog.c, fileloop.c/h, fileverbs.c, file_portable_posix.c/h

- Move to `Common/source/file/`
- Only file_portable_posix.c is compiled in headless (portable replacements exist for the rest)
- Update 1 path
- Risk: Low

### Phase 10: Networking (2-3 files)

**Files**: tcpverbs.c/h, WinSockNetEvents.c

- Move to `Common/source/net/`
- Update 1 path (tcpverbs.c)
- Risk: Very low

### Phase 11: Menu System (~7 files)

**Files**: menu.c/h, menuverbs.c/h, menubar.c/h, menueditor.c/h, menupack.c, menuresize.c, menufind.c

- Move to `Common/source/menu/`
- Only menupack.c and menuverbs.c are compiled in headless
- Update 2 paths
- Risk: Low

### Phase 12: Scripting/OSA (~6 files)

**Files**: scripts.c/h, osacomponent.c/h, osadroplet.c/h, osawindows.c/h, osaparseaete.c/h, osamenus.c/h

- Move to `Common/source/script/`
- None compiled in headless
- Risk: Very low

### Phase 13: Shell Boundary + Cancoon

**Shell files**: shell_api.c/h, shell_api_headless.c, headless_selection.c/h, headless_stubs.c/h, sysshellcall.c/h + all UI shell files (shell.c, shellwindow.c, shellmenu.c, shellcallbacks.c, etc.)

**Cancoon files**: cancoon.c/h, cancoonwindow.c, cancoonpopup.c, cancoonverbs.c

- Move to `Common/source/shell/` and `Common/source/cancoon/` respectively
- shell.h is included by 89 files
- Update ~5 paths in RUNTIME_SOURCES (headless API files)
- Risk: Moderate

### Phase 14: UI Isolation

- Move all remaining pure-UI files to `Common/source/ui/` and its subdirectories:
  - `ui/op/` — opdisplay.c, opbuttons.c, opicons.c, oplineheight.c, etc.
  - `ui/table/` — tabledisplay.c, tableedit.c, tableformats.c, etc.
  - `ui/clay/` — claybrowserexpand.c, claybrowserstruc.c, claycallbacks.c, etc.
  - `ui/wptext/` — wpengine.c, wpvariables.c, wpverbs.c
  - `ui/` — about.c, bitmaps.c, cursor.c, dialogs.c, font.c, icon.c, quickdraw.c, pict.c, scrollbar.c, etc.
- None compiled in headless; zero Makefile changes
- Risk: Very low

### Phase 15: Dead Platform Code to Legacy

- Move to `Common/source/legacy/`: FastTimes.c/h, CallMachOFrameWork.c/h, OSXSpecifics.c/h, winregistry.c/h, memory.track.c, .m files (fileops.m, launch.m, main.m, MacDateHelpers.m)
- Risk: Very low

## File Splitting (Follow-Up PRs)

After files are in their subsystem directories, split oversized files as separate PRs:

| File | Lines | Proposed Split |
|------|-------|----------------|
| langhtml.c | 10,151 | langhtml_render.c + langhtml_parse.c + langhtml_verbs.c |
| langvalue.c | 9,083 | langvalue_coerce.c + langvalue_ops.c + langvalue_compare.c |
| langhash.c | 5,046 | langhash_core.c + langhash_verbs.c |
| opverbs.c | 4,773 | opverbs_core.c + opverbs_edit.c |
| wpengine.c | 4,700 | Deferred (UI-only, low priority) |
| langverbs.c | 4,401 | Assess after move — may split by verb domain |
| scripts.c | 4,190 | Deferred (not compiled in headless) |

Each split PR introduces an internal header (e.g., `langhtml_internal.h`) for shared static declarations, moves groups of related functions into the new files, and updates the Makefile.

## Verification Checklist (Every PR)

1. `make -C frontier-cli` — clean build, no new warnings
2. `./tools/run_headless_tests.sh` — all unit tests pass
3. `cd tests && make test-integration` — all integration tests pass
4. `git diff --stat` review: only Makefile paths + `git mv` operations, no .c/.h content changes (except for file splits)
5. Spot-check: `grep -r '#include "moved_header.h"'` confirms includes still resolve

## Key Build Files Modified

- `frontier-cli/Makefile` — INCLUDES, RUNTIME_SOURCES, DATABASE_SOURCES, HEADLESS_STUBS
- `tests/Makefile` — compile paths for test executables
- `tests/headless_verbs.mk` — shared verb file list (if any referenced files move)

## Risk Summary

| Phase | Risk | Primary Concern |
|-------|------|-----------------|
| 0 — Include paths | Very low | Pure additive Makefile change |
| 1 — Dead third-party | Very low | Nothing compiled |
| 2 — Crypto | Low | Nearly standalone |
| 3 — ODB | Moderate | db.h included by ~15 files |
| 4 — Outline | Moderate | op.h included by ~40 files (mostly UI) |
| 5 — Table | Moderate | Coupling with shell/cancoon/clay headers |
| 6 — Infrastructure | Medium-high | memory.h in 143 files |
| 7 — Lang runtime | Medium-high | 25+ files, lang.h in 85 files |
| 8 — Lang extensions | Low | Mostly not compiled |
| 9 — File I/O | Low | Mostly not compiled |
| 10 — Networking | Very low | 2 files |
| 11 — Menu | Low | 2 compiled files |
| 12 — Script/OSA | Very low | Not compiled |
| 13 — Shell/Cancoon | Moderate | shell.h in 89 files |
| 14 — UI | Very low | Not compiled |
| 15 — Dead platform | Very low | Not compiled |

## Future Work

After all phases complete:
- **Optional include cleanup**: Convert bare `#include "header.h"` to `#include "subsystem/header.h"` and remove excess `-I` flags
- **UI boundary formalization**: Define `shell_api.h` as the contract between any future UI and the headless runtime
- **New UI development**: The `ui/` directory tree clearly marks what is "old UI" — a new UI project would implement against `shell_api.h` without touching `ui/`
