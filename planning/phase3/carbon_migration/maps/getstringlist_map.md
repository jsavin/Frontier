# `getstringlist` / `langgetstringlist` Call-Site Map

Status
- State: In Progress
- Phase: Carbon Migration (Resource Manager Retirement)
- Last Updated: 2025-11-20
- Owner: Codex
- Notes: Track STR#/string-list dependencies; update counts as modules switch to portable tables.

**Date:** October 31, 2025  
**Owner:** Codex  

This map captures every reference to the STR#-backed string list helpers so we can plan the resource‑fork retirement work. Counts are taken from `rg "getstringlist"` (includes `tablegetstringlist` and `langgetstringlist`). Classification reflects the primary module role; status indicates whether a concrete replacement plan exists.

## Summary
- **Total references:** 170 (source + headers + stubs).  
- **Core runtime impact:** 58 refs across evaluator, language verbs, database/runtime modules — these will need a portable string table.  
- **Desktop/UI only:** 87 refs (menus, dialogs, shell UI, QuickDraw drawing). Candidate for desktop-only gating or removal.  
- **Legacy/SDK samples:** 25 refs (old stubs, SDK toolkits) — safe to archive once replacements exist.

## Occurrence Table
| Path | Count | Area | Notes | Status |
| --- | --- | --- | --- | --- |
| `Common/source/strings.c` | 7 | Core runtime | Truncation helpers rely on pixel width strings. Needs portable replacement for error text + ellipsis logic. | Todo |
| `Common/source/langops.c` | 7 | Core runtime | Error reporting and user-level diagnostics. Must migrate to table-driven constant strings. | Todo |
| `Common/source/langipc.c` | 7 | Desktop (AppleEvents) | AppleEvent plumbing; strings unused in headless after AE removal. | Blocked on AE plan |
| `Common/source/opverbs.c` | 6 | Desktop UI | Outliner verbs; safe to defer until UI split. | Defer |
| `Common/source/langsqlite.c` | 5 | Core runtime | Database error paths; portable replacements required. | Todo |
| `Common/source/langregexp.c` | 5 | Core runtime | RegExp diagnostics; should move to const tables. | Todo |
| `Common/source/frontierconfig.c` | 5 | Desktop UI | Preferences UI. Gated by UI refactor. | Defer |
| `Common/source/config.c` | 5 | Core runtime | Configuration loader errors. Needs new string table. | Todo |
| `Common/source/tablescrap.c` | 4 | Desktop UI | Clipboard routines; likely desktop-only. | Defer |
| `Common/source/tablepopup.c` | 4 | Desktop UI | Popup UI; candidate for desktop gating. | Defer |
| `Common/source/shellsysverbs.c` | 4 | Desktop UI | Menu verbs; drop from headless. | Defer |
| `Common/source/scripts.c` | 4 | Desktop (recording) | Script recorder UI; remove with AE cleanup. | Defer |
| `Common/source/tableops.c` | 3 | Core runtime | Table error flow; headless-critical. | Todo |
| `Common/source/tablecallbacks.c` | 3 | Desktop UI | Table window sizing; UI only. | Defer |
| `Common/source/shellfile.c` | 3 | Desktop UI | File dialogs; refactor out. | Defer |
| `Common/source/resources.c` | 3 | Legacy infra | Core STR# loader; will be replaced entirely. | In progress |
| `Common/source/pictverbs.c` | 3 | Desktop UI | QuickDraw verbs. | Defer |
| `Common/source/osacomponent.c` | 3 | Desktop (OSA) | AppleScript integration; to be isolated. | Blocked on AE plan |
| `Common/source/langxml.c` | 3 | Core runtime | XML verbs; require portable error strings. | Todo |
| `Common/source/langevaluate.c` | 3 | Core runtime | Thread naming/error prompts. Provide const strings. | Todo |
| `Common/source/langerrorwindow.c` | 3 | Desktop UI | Error window presentation. | Defer |
| `Common/source/command.c` | 3 | Desktop UI | Command palette UI. | Defer |
| `Common/source/wpverbs.c` | 2 | Desktop UI | Word processing verbs; UI. | Defer |
| `Common/source/tableverbs.c` | 2 | Core runtime | Table verbs; needs replacement strings. | Todo |
| `Common/source/tablestructure.c` | 2 | Core runtime | System table bootstrap references; headless-critical. | Todo |
| `Common/source/tableformats.c` | 2 | Core runtime | Format inspector; evaluate whether headless cares. | Review |
| `Common/source/tabledisplay.c` | 2 | Desktop UI | Window display; UI. | Defer |
| `Common/source/shellwindowverbs.c` | 2 | Desktop UI | Window verbs; UI only. | Defer |
| `Common/source/shellverbs.c` | 2 | Desktop UI | Classic shell verbs. | Defer |
| `Common/source/shellcallbacks.c` | 2 | Desktop UI | Undo UI glue; drop in headless. | Defer |
| `Common/source/process.c` | 2 | Desktop | Process manager integration. | Defer |
| `Common/source/menuverbs.c` | 2 | Desktop UI | Menu verbs; UI. | Defer |
| `Common/source/langerror.c` | 2 | Core runtime | Language error formatting. Needs new table. | Todo |
| `Common/source/lang.c` | 2 | Core runtime | Generic error propagation; must switch to portable strings. | Todo |
| `Common/source/fileverbs.c` | 2 | Core runtime | File verbs; replace with const tables. | Todo |
| `Common/source/error.c` | 2 | Core runtime | Error dialog wrappers; evaluate for headless removal. | Review |
| `Common/source/dialogs.c` | 2 | Desktop UI | Alert scaffolding; UI only. | Defer |
| `Common/source/db.c` | 2 | Core runtime | DB layer errors; new string table needed. | Todo |
| `Common/source/langhash.c` | 1 | Core runtime | Hash errors; priority for headless tests. | Todo |
| `Common/source/langsystem7.c` | 1 | Desktop (System 7) | Legacy alias error message. Remove with alias work. | Defer |
| `Common/source/langmysql.c` | 1 | Core runtime | MySQL verb error; convert to const string. | Todo |
| `Common/source/langmath.c` | 1 | Core runtime | Math verb error; convert to const string. | Todo |
| `Common/source/langhtml.c` | 1 | Core runtime | HTML verbs; convert to const string. | Todo |
| `Common/source/langcrypt.c` | 1 | Core runtime | Crypt verbs; convert to const string. | Todo |
| `Common/source/dbstats.c` | 1 | Core runtime | DB stats verb error; replace. | Todo |
| `Common/source/cancoonverbs.c` | 1 | Core runtime | Cancoon verb messages; replace. | Todo |
| `Common/source/cancoon.c` | 1 | Core runtime | Cancoon error; replace. | Todo |
| `Common/source/about.c` | 1 | Desktop UI | About box; UI only. | Defer |
| `Common/source/shellmenu.c` | 1 | Desktop UI | Menu UI; de-scope headless. | Defer |
| `Common/source/progressbar.c` | 1 | Desktop UI | Progress UI; de-scope headless. | Defer |
| `Common/source/pict.c` | 1 | Desktop UI | PICT processing; tied to QuickDraw. | Defer |
| `Common/source/oplist.c` | 1 | Desktop UI | Outliner UI; de-scope. | Defer |
| `Common/source/odbengine.c` | 1 | Desktop | ODB interactions, desktop only. | Defer |
| `Common/source/langverbs.c` | 1 | Core runtime | Verb dispatch; convert to const string. | Todo |
| `Common/source/langsystypes.c` | 1 | Legacy bridge | Alias error strings. | Blocked on alias plan |
| `Common/headers/resources.h` | 1 | Header | Prototype; will change when new API lands. | Pending |
| `Common/headers/langinternal.h` | 1 | Header | Prototype for `langgetstringlist`; update once replacement API ready. | Pending |
| `Common/headers/tableinternal.h` | 1 | Header | Prototype for `tablegetstringlist`; update with new table accessor. | Pending |
| `tests/headless_lang_runtime_more_stubs.c` | 1 | Test stub | Returns empty string list; needs to mirror new API. | Todo |
| `portable/runtime_stubs*.c/h` | 4 | Headless scaffolding | Temporary. Remove after new string table lands. | Todo |
| `docs/sdk/...` | 5 | Legacy SDK | Will be archived with legacy resource tooling. | Archive candidate |

## Notes
- Many desktop/UI call sites already include guard comments; once replacements exist we can wrap them in `#if !FRONTIER_HEADLESS`.
- Core runtime call sites should migrate first so headless builds can drop resource forks entirely.
- When the portable string table lands, update the prototypes listed under “Header” status.
