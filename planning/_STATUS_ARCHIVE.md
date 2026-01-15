# Carbon Migration – Status Archive

Status
- State: Archived
- Phase: Carbon Migration
- Last Updated: 2026-01-05
- Owner: Codex
- Notes: Historical snapshots of `_CURRENT_STATUS.md`; not an active plan.

> Purpose: historical snapshots of `_CURRENT_STATUS.md` milestones and major accomplishments. Use this to seed progress reports; do not treat entries here as active work.

**Snapshot:** Archived on January 5, 2026 (repository cleanup session).

## 2026-01-05 Archive (Repository Branch Cleanup & Recent Progress)

Archived the December 31, 2025 - January 5, 2026 `_CURRENT_STATUS.md` activity (comprehensive branch cleanup, PR completions). See git history for full detail; highlights:

**Jan 5 Session - Repository Cleanup:**
- Deleted 78 branches from ~87 to 3 local branches (classified: 13 low-risk, 44 merged, 6 zombie, 3 stale, 1 stash, 2 archive snapshots, 7 superseded, 2 remote zombies)
- Extracted typeof() OSType code documentation (180 lines, commit 5456c5eb)
- Significantly improved repository hygiene and branch organization
- 3 local branches remain: develop, archive/codex-sessions (permanent), feature/issue-135-phase5-callsite-migration (active)

**Recent Verb Implementation Progress:**
- PR #246: Phase 5 lang type conversion verbs (15 new verbs implemented)
- PR #245: Lang verbs fixes and refinements
- PR #241: File verb coverage completion (86/86 verbs = 100% complete)
- PR #232: wptext_runtime.c frontier_time_t migration for Y2038 readiness
- Overall verb coverage: 37% (264/710 verbs implemented)
- By category: File 100%, Lang 16%, others in various states of completion

**Architectural Lesson:**
- typeof() OSType extraction highlighted value of extracting domain-specific documentation from code into dedicated reference documents
- Improves future maintainability and makes knowledge discoverable for new developers

**Snapshot:** Archived on November 17, 2025 (post cleanup of `_CURRENT_STATUS.md`).  

## Archived Updates (for reference)

*The material below preserves the prior `_CURRENT_STATUS.md` entry so the team can revisit history when needed.*

---

**Snapshot:** Archived on November 26, 2025 (pre-trim of `_CURRENT_STATUS.md`).  

# Carbon Migration – Current Status

Status
- State: In Progress
- Phase: Carbon Migration / Runtime Modernization
- Last Updated: 2025-11-26 (Afternoon)
- Owner: Codex
- Notes: Primary hand-off summary; update whenever major milestones land.
- 2025-11-26 (Codex): Forked readers/writers into dedicated modules (`db_reader_legacy.c`, `db_reader_modern.c`, `db_writer_modern.c`) and added include guards for db internals; `make -C tests db_format_tests` now builds cleanly aside from the longstanding `__builtin_return_address` warning and an unused local helper in `db.c`. Next: finish the modern write path (drop Cancoon/legacy view, use split read/write contexts) and re-run migrator/CLI suites.
- 2025-11-26 (Codex): New plan doc tracking the clean reader/writer split lives at `planning/phase3/modern_reader_writer_split.md`.
- 2025-11-25 (Codex): Adapter now forces table repack when active and marks external addresses for BE64 writes; table writer regression proves BE64 output under use_64bit_format. Wide-write flip is deferred until write time; readers remain legacy. Table/record payload widening still pending; runtime/CLI still on legacy reader for v7 until adapter writes modern bytes.
- 2025-11-25 (Codex): Adapter repack now covers outlines/scripts/wp/menus/picts/tables, marking them dirty and enabling BE64 writes via helper; new regression exercises table repack BE64 path. `make -C tests db_format_tests` passes (known return_address warning). Added record-level BE64 regression and strict v7 reader enforcement for v7 headers. Remaining: verify any remaining record packers and run runtime/CLI suites.
- 2025-11-25 (Codex): Strict v7 reader enforcement wired into dbopenfile for v7 headers; adapter repack remains active for legacy inputs. Added record reference BE64 regression. `make -C tests runtime_tests` and `make -C tests cli_runtime_tests` pass locally (clock.now still skipped with frontier-cli exit=1).
- 2025-11-26 (Codex): `clock.now` CLI still fails: header decode now reads view0 correctly (`0x00000000006b056f`), but `dbreadheader` on that view reports an invalid variance (bytes: size=0x1da, variance=0x00030000). Regenerated roots still carry the legacy Cancoon block; migration now fails packing the system table (opunpackv2 on legacy externals) when `use_64bit_format` flips mid-run. Next: fork readers (`db_read_legacy` vs `db_read_modern`), materialize under legacy before writing modern BE64, drop Cancoon, and regenerate root to unblock `clock.now`.
- 2025-11-25 (Codex): Added regression proving table writer emits BE64 when use_64bit_format is enabled; wide-write helper now invoked by Save As/header flush paths only after legacy load completes, so reads stay legacy until write time. Table/record payload widening still TODO; runtime/CLI still on legacy reader for v7 until adapter writes modern bytes.
- 2025-11-25 (Codex): Adapter stays in legacy read mode until write time; `dbstartsaveas` now enables wide writes when adapter-active, and migrator defers wide-write flip until after legacy load. Header flush also guards with the adapter helper so outbound bytes are BE64. Table/record payload widening still TODO; runtime/CLI still on legacy reader for v7 until adapter writes modern bytes.
- 2025-11-25 (Codex): Adapter now caches widened legacy headers and exposes `db_format_adapter_enable_wide_writes`; `dbflushheader` auto-flips to wide writes when adapter-active and migrator enables wide writes after opening legacy roots. Table/record payload widening still TODO; runtime/CLI still on legacy reader for v7 until adapter writes modern bytes.
- 2025-11-25 (Codex): Added legacy adapter header widening + strict v7 loader gating (adapter keeps legacy read path, widens header for v7 packers) and a byte-level regression in `tests/db_format_tests`; `make -C tests db_format_tests` passes (expected `__builtin_return_address` warning). Adapter now caches widened header for wide writes via `db_format_adapter_enable_wide_writes`. Next: widen tables/records in adapter, route runtime/CLI to strict v7 reader, and rerun runtime/CLI suites.
- 2025-11-25 (Codex): Documentation refresh only; marked router/header guard and legacy touchpoint audit as **Done**. Next: implement legacy adapter widening + strict v7 reader on `feature/legacy_adapter_widening_and_v7_reader`; no code changes this pass.
- 2025-11-24 02:00 CST (Codex): Landed version-based reader router + header decode guard; tests updated. Next: implement legacy adapter widening and strict v7 reader on branch `feature/legacy_adapter_widening_and_v7_reader`, then tackle runtime/CLI stabilization separately.


**Last Updated**: November 26, 2025  \
**Branches in flight**: `feature/carbon-migration-plan`, `feature/headless-system-bootstrap`, `feature/legacy_adapter_widening_and_v7_reader`
- 2025-11-23 23:59 CST (Codex): Reviewed current status and next steps; no code changes this session, priorities unchanged.
- 2025-11-24 00:05 CST (Codex): Started BE/64-bit sweep on packers (outline/langpack/langtree/regexp now use explicit BE helpers for length/type fields); remaining packers/writers still need conversion; tests not run this pass.
- 2025-11-24 01:03 CST (Codex): Completed BE/64-bit sweep (db header fields, menu/lang/wptree/odb/cancoon/wpengine/memory/shell sysverbs/pict all use BE helpers). Added procedural byte-level goldens + PICT length check to `tests/db_format_tests`; tests re-run (`make -C tests db_format_tests`, `make -C tests runtime_tests`) and pass with existing `__builtin_return_address` warning. Ready for PR; cross-arch coverage now enforced via procedural goldens until CI runs on x86.
- 2025-11-24 01:15 CST (Codex): PR merged (`feature/v7-be64-writers-completion`); branch deleted. Next focus: split legacy adapter vs clean v7 reader, re-enable CLI `clock.now()` once reader parity is in, and keep procedural goldens as cross-arch sentinels until x86 CI is available.
- 2025-11-24 01:18 CST (Codex): Drafted reader split plan at `planning/phase3/v7_reader_refactor_and_legacy_adapter_plan.md` (PR breakdown for legacy adapter + clean v7 reader + coverage).
- 2025-11-24 02:15 CST (Codex): Added widening plan at `planning/phase3/v7_reader_widening_plan.md` (legacy widening + strict v7 reader tasks). Router/decode guard already merged; widening work will proceed on `feature/legacy_adapter_widening_and_v7_reader`.

- Headless builds no longer include or link any Paige headers: `portable/wptext_portable.{c,h}` collapsed to no-op bootstrap stubs, and the entire `portable/wptext_runtime.c` path now uses the new `paige_text_extractor` + UTF‑8⇄RTF helpers for packed blobs.
- `wp_portable_state_pack_portable` now caches real RTF payloads derived from the extractor (or copies RTF payloads for existing `WPRT` blobs) and wraps them with the portable header without ever touching `pg*` APIs. The same helper feeds `wpverbpack` so v7 saves always emit portable `WPRT` handles.
- `wp_portable_extract_plaintext` decodes the new RTF payloads back into UTF-8 handles (with a logged fallback if an unexpected control word shows up). `langhash_prepare_wordprocessor_value` and `wpverbpacktotext` now consume the Paige-free parser exclusively, and `langhash_materialize_external` converts `wptext` externals during table loads so migrator/CLI callers see plain strings immediately.
- `paige_text_extractor` now resizes handles through the runtime memory API (`gethandlesize/sethandlesize`) and guards against overflow, so chunk concatenation works even when we run against OS-managed handles. `tests/paige_text_tests` covers the real Paige fixtures (`hello_macroman`, `examples_testText`) with the new logic.
- Phase 2 RTF emission is live: the extractor now captures Paige style/paragraph/font metadata, `wptext_emit_rtf_from_paige_blob` streams proper RTF (fonts, inline styling) without Paige, and `wp_portable_state_cache_rtf` uses it to populate `WPRT` handles. Added a CLI helper (`tools/wptext_dump_rtf.c`) plus a regression test that verifies the emitted RTF for the `hello_macroman` fixture.
- `FRONTIER_REGEN_ROOT=databases/Frontier.root ./tests/runtime_tests` now completes end-to-end; latest log lives at `/tmp/runtime_migrate_run2.log` and produced `databases/Frontier.root7` without tripping the Paige extractor fallback path.
- Table serialization now writes a v4 header with the reserved 1 KB padding. `hashunpacktable` skips the reserved block when reading, so both 32‑bit and 64‑bit tables now satisfy the new `table_header_regression` checks.
- `wp_portable_diskheader`/`wp_portable_header` switched to fixed-width integer fields, which fixed the `utf8bytelen` bookkeeping and unblocked all WP text smoke tests. Added `wp_portable_load_portable_blob_for_test` so the runtime regression harness can validate `WPRT` blobs directly.
- Regression coverage: `tests/paige_text_tests` and `tests/runtime_tests` both pass locally (see `/tmp/runtime_tests.log` for the latest run). The migrator now succeeds on the canonical `Frontier.root` sample.
- Serializer round-trip logging tightened: `hashunpacktable` now checks remaining bytes before calling `loadfromhandle`, so the expected end-of-records path no longer emits `[headless] loadfromhandle fail` noise. Re-ran the full migrator afterward; fresh logs live at `/tmp/runtime_tests.log`.
- Database portability gap: v7 headers now write with explicit big-endian helpers. `dbflushheader` serializes `tydatabaserecord_64` via `db_format_write_header64` (runtime-only fields zeroed), `tableverbpack/tableverbunpack` emit/consume big-endian dbaddresses regardless of host endianness, and block headers/trailers now store 64-bit big-endian sizes/links (avail nodes write/read BE64 links). `make -C tests db_format_tests -B` and `make -C tests runtime_tests`/`cli_runtime_tests` pass locally (warnings only). Follow-ups (tracked in `planning/phase3/big_endian_portability_audit.md`):
  1. Extend table/record packers to emit big-endian lengths/addresses (the decoder already does this for legacy payloads). **Table dbaddress packing is fixed; double-check any remaining record-length writers that still rely on `memtodisklong`.**
  2. Add regression tests that open a root written on one architecture and verify the header/record bytes match a golden big-endian reference (include >4 GB free-span simulation/sparse file and clean up artifacts on success). In-memory >4 GB free-span encode/decode added to `tests/db_format_tests`.
  3. Update docs (`docs/database_architecture.md`, `planning/TODO_future_improvements.md`) once the on-disk format is guaranteed portable.
- CLI regression: `tests/cli_runtime_tests` currently skips the `clock.now()` check because `frontier-cli --system-root databases/Frontier.root7` fails to load v7 databases (same endianness issue above). Once the header writes are fixed, re-run the CLI tests so the `clock.now()` path becomes a real regression instead of a skip.

### Migrator status – `Frontier.root`
- Repro: `FRONTIER_REGEN_ROOT=databases/Frontier.root ./tests/runtime_tests`
- Outcome: Success as of Nov 20. See `/tmp/runtime_migrate_run2.log` for the full transcript; the run emitted `databases/Frontier.root7` with every wptext external converted through `paige_text_extractor`.
- Follow-ups:
  * Keep `/tmp/runtime_migrate.log` around for comparison (that log still shows the pre-fix crash for reference).
  * Spot-check the generated `Frontier.root7` in follow-on tests once the v7 reader path is wired up.

## Medium-Term Goals
- Finish the wptext plain-text conversion so every `langhash_prepare_wordprocessor_value` either returns UTF-8 text or a clearly logged placeholder, then ensure the migrator packs those values without leaning on Paige.
- Confirm runtime parity between the headless bootstrap and UI routes (e.g., `system.verbs → kernelcall → EFP`) once wptext and doc-info no longer destabilize the migrator.
- Keep the automation/IPC boundary documented so headless builds can safely expose JSON-RPC while UI builds retain OSA, as tracked in `planning/TODO_future_improvements.md`.

---

## 2025-12-22 Archive (pre-PR #137 merge snapshot)

Archived the December 8–22 `_CURRENT_STATUS.md` (migration double-free fix, cleanup helper extraction, test accessor implementation). See git history on branch `fix/migration-double-free` for full detail; highlights:
- Fixed critical segmentation fault caused by double-free in `cleanup_migration_database()`.
- Root cause: `dbendsaveas_context()` internally calls `dbdispose()`, but cleanup code unconditionally called it again.
- Solution: Set `databasedata = nil` immediately after `dbendsaveas*()` calls to prevent double-free.
- Removed context guards from `dbzeroreleasestack()` (guard patterns during disposal cause dangling pointers).
- Added defensive `validhandle()` check before dereferencing in cleanup.
- Extracted `cleanup_migration_database()` helper function to improve testability and document three distinct cleanup scenarios.
- Added cleanup state validation test (`save_migration_tests.c` now validates `fldatabasesaveas` flag properly reset).
- Created test accessor function `db_test_is_saveas_active()` for clean test isolation.
- All commits include planning doc references per CLAUDE.md guidelines.
- Follow-up issues filed: #138 (P1: make disposal explicit), #139 (P2: cleanup path duplication), #140 (P1: audit context guards), #141 (P2: final cleanup restructuring), #142 (P2: error path test coverage), #143 (P2: cleanup_migration_database error scenarios).
- **PR #137** merged with 7 commits addressing comprehensive code review feedback across multiple rounds.

## 2025-12-13 Archive (pre-PR #75 merge snapshot)

Archived the December 7–9 `_CURRENT_STATUS.md` (hash name corruption chase, TEC converter fix, headless verb additions). See git history on branch `fix/hash-unpack-hardening` for full detail; highlights:
- Added defensive bounds checks/logging in `hashunpacktable`, manual BE buffer packing for v7 symbol records, and CLI hydration/migration fixes.
- Fixed TEC converter disposal crash during migration.
- Implemented headless `string.upper/lower/length` and `math.random`; CLI inline eval uses `langrunhandle`.
- Migration tests passed; CLI still failing stubbed verbs (`clock.*`).

## Long-Term Goals
- Finish the Phase 2 runtime context refactor so simultaneous CLI/headless clients share `FrontierContext` backend handles without touching globals.
- Land the Phase 3 concurrency/task-context plan, widen paging/tracing to multi-threaded guard-malloc tests, and keep `planning/TODO_future_improvements.md` aligned with heading priorities.
- Expand developer tooling (OSS compliance, doc server, LSP work) so future IDE/bridge projects and release automation can rely on the documented TODO backlog.

## Next Steps
- Finish the split modern writer path: ensure view0 header writes BE64 without legacy Cancoon, keep legacy reads separate from modern writes, and rerun the migrator/CLI smoke to confirm the variance/view corruption is gone.
- Finish the legacy adapter widening for tables/records and route runtime/CLI v7 opens through the strict reader; rerun `make -C tests runtime_tests` and `make -C tests cli_runtime_tests`, logging output paths.
- Add byte-level regressions for the forked readers/writer (modern header/block writes, adapter widening output) to guard the new files.
- Re-run `FRONTIER_REGEN_ROOT=… ./tests/runtime_tests` on additional legacy roots once the modern writer is fixed; stash logs under `/tmp` with timestamps.
- Update `docs/database_architecture.md` and phase3 docs once the modern read/write split is stable; keep `planning/phase3/big_endian_portability_audit.md` aligned with any new BE checks.

# Carbon Migration – Current Status

**Last Updated**: November 16, 2025 (night)  \
**Branches in flight**: `feature/carbon-migration-plan`, `feature/headless-system-bootstrap`

## Recent Updates — November 16, 2025
- Guarded `copy_shape_param` so headless pgNewShell now seeds empty rectangles instead of calling `pgRectToShape(NULL)`. Paige rebuild + sanitised runtime_tests still crash later in doc-info (`docinfo.subject` underflow inside pgScrapMemoryRead`), but the earlier pgNewShell crash is gone. Logs: /tmp/runtime_option3.log.
- Re-ran `FRONTIER_REGEN_ROOT=databases/Frontier.root ./tests/runtime_tests` (plain + LLDB) to confirm the crash still occurs inside the doc-info read/export path. Breakpoints at `PGREAD.C:744` and `pgScrapMemoryRead` show `pgr->doc_info.subject=0x13e80e9c0` when the block finishes, and the scrap reader immediately receives the same handle (verb=io_data) before calling `UseMemory`/`UnuseMemory`. The LLDB session proves the `pgScrapMemoryRead` call unbalances the handle right after doc-info parsing, so the next change needs to guard or restructure that unuse call and re-test.
- Nuked and reconfigured `third_party/Paige/build-headless-debug`, rebuilt `libpaige.a`, relinked `tests/runtime_tests`, and reran the guard-malloc repro (`/tmp/runtime_option2h.log`). The new `pg-named_styles`/doc-info snapshot logs still do not show up, even though `strings` confirms the instrumentation is present. LLDB breakpoints at `PGBASICS.C:720` fire during the WP smoke test, so the code path is executing; next step is to step past the instrumentation (and/or move the logging to `pgShareRefs`) to see where stdout is getting lost.
- Added temporary FRONTIER_TESTS logging around the doc-info snapshot and named-styles cleanup in `PGREAD.C` to see whether the doc-info handles alias `pg->named_styles`, rebuilt `libpaige.a`/`runtime_tests`, and reran the guard-malloc repro (`/tmp/runtime_option2d.log`). The crash still reproduces at `pgReadHandlerProc + 6808` and `pgScrapMemoryRead`, but the new `[pg-docinfo] snapshot ...` line did not appear yet—so next session should confirm whether the symbol is linked (e.g., via `strings libpaige.a`) before relying on that output.
- Reintroduced `input_opt_character_ref` as a FRONTIER_TESTS-aware wrapper around `input_opt_character_ref_impl`, rebuilt `libpaige.a`, and reran the guard-malloc repro (`MallocScribble=1 MallocCheckHeapStart=1 FRONTIER_REGEN_ROOT=databases/Frontier.root ./tests/runtime_tests`). The crash still repros with the same `docinfo.subject` underflow in `pgReadHandlerProc`/`pgScrapMemoryRead`; logs saved in `/tmp/runtime_option2.log` for follow-up.
- Added first-class doc-info logging/watching inside Paige (`PGREAD.C` now prints `[pg-docinfo] doc_info_key …` lines and immediately registers each handle), plus the headless runtime (`wptext_runtime.c`) now re-watches doc-info handles as soon as we load a doc so the Paige logs stay stable. Guard-malloc `runtime_tests` still crash with `docinfo.subject` underflowing in both `pgReadHandlerProc` and `pgScrapMemoryRead`, but `/tmp/runtime_docinfo_trace13.log` now shows the full handle sizes/pointers so we can target those `UnuseMemory` imbalances next.
- Latest guard-malloc traces confirm the subject handle stays balanced during each `input_opt_character_ref_impl` call and only goes negative after the doc-info block emits one extra `UnuseMemory` (right after `doccomm`), after which `pgScrapMemoryRead` underflows it again while reading the pseudo-filemap. Next fix is to remove/guard that redundant `UnuseMemory` in `pgReadHandlerProc`, then audit the scrap reader.
- Updated `tests/Makefile` so the Paige CMake build (`libpaige.a`) now reconfigures/rebuilds whenever Paige sources or headers change; make targets that depend on `$(PAIGE_LIB)` automatically pull in the fresh static lib before relinking runtime tests.
- Instrumented the doc-info save path (`pgSetDocInfo`, `pgSaveDoc`, `pgCallTextHook`, and `pgDispose`) plus added headless stubs for missing frame/device cleanup (`PGFRAME_STUB.C`, `PGPLATFO/PGDEVICE_STUB.C`). `pgDispose` no longer jumps through null callbacks, but `DisposeMemory` now explodes while freeing the doc-info handles (e.g. ref `0x15400d7f8`); we need to trace why those `memory_ref`s become invalid before the master list is drained.
- Added handle-level tracing in `pgMemMgr` and wired `wptext_runtime` to watch doc-info strings. The latest guard-malloc run (`/tmp/runtime_docinfo19.log`) shows `docinfo.subject` repeatedly hitting negative access counts (extra `UnuseMemory` calls) before `DisposeMemory` frees the ref, so the next fix needs to focus on balancing those Use/Unuse pairs.
- Re-ran the guard-malloc repro with tracing (`/tmp/runtime_docinfo20.log`) and captured backtraces whenever a doc-info handle underflows. Every failure comes from `pgReadDoc`—one in `pgReadHandlerProc` (the doc-info key case) and another in `pgScrapMemoryRead`. Both paths call `UnuseMemory` on the doc-info handles without balancing `UseMemory`, so the handles go negative long before our runtime packs them.
- Ported the missing Paige exporter/importer stack (PGEXPORT/PGIMPORT/PGRTFEXP/PGRTFIMP/PGNATIVE/PGHTM*) into the UNIX build, added the required `PG_SIZE_PTR` helpers and UNIX defaults (font tables, HTML colors, cross-character tables), and wrapped the legacy globals in `extern "C"` so `libpaige.a` now ships the real conversion routines instead of the test stubs.
- Rebuilt `runtime_tests` against the refreshed library and reran the migrator under `MallocScribble=1 MallocCheckHeapStart=1`. The doc-info stage now completes cleanly: `pgExportFileFromC` returns `err=0` and `pack_portable` logs a valid header (see `/tmp/runtime_docinfo7.log`), so the prior crash was purely because we weren’t linking the real exporter.
- Added `FRONTIER_TESTS` instrumentation to `PGWRITE.C` so doc-info saves now log which handlers fire (and when `EXPORT_PAGE_INFO_FLAG` blocks them), rebuilt Paige/tests from a clean tree, and reran `runtime_tests` with `MallocScribble=1 MallocCheckHeapStart=1` (see `/tmp/runtime_docinfo5.log`). The run still crashes immediately after `wp_portable_force_doc_editable`; no `[pg-docinfo]` logs appear, which confirms we never reach the doc-info write handler before the heap corruption fires.
- Reintroduced the full slot-based UNIX handle shim and upgraded `pgMemMgr` so UNIX now maintains the same master-list semantics as Mac/Win (`pg_unix_master_table`, UNIX branches in `InitMemoryRef`, `extend_master_list`, `pgMemShutdown`, etc.). Paige rebuilds cleanly and the temp-file path (`pgOpenTempFile`) no longer explodes immediately.
- Rebuilt `tests/runtime_tests` and reran the migrator. The run still dies right after the `[paige-docinfo]` logging—immediately after `wp_portable_force_doc_editable`—with PC jumping to `0x0`, so the lingering corruption is in the doc-info/doc-comment path, but now we know it isn’t caused by temp-path descriptors.
- Captured LLDB stacks by setting one-shot breakpoints on `pg_unix_alloc_handle(size==148|152)`. Both break during `input_opt_character_ref → pgReadHandlerProc → pgReadDoc`, proving the doc-info optional strings are flowing through the new shim correctly. After those allocations the crash still happens (with no `malloc_error_break`).
- Added `FRONTIER_TESTS` logging (`wp_runtime_log_docinfo_handles`) so `wp_portable_state_load_doc`/`wp_portable_state_pack_legacy` dump every doc-info handle and byte count. The logs show the expected handles/sizes (e.g. doccomm 148 bytes) immediately before the crash, so the shim and loaders are working.
- Tried LLDB watchpoints on those handles/structs; LLDB can’t dereference Paige’s slots from the debugger, so the watchpoints trip at creation time and we still end up with a PC=`0x0` crash. Next step is to instrument the caller (e.g. `wp_portable_state_pack_legacy`, `pgSaveDoc`, or `pgSetDocInfo`) so we can capture the first place that dereferences the doc-info handles after they’re logged.

## Recent Updates — November 12, 2025
- Added a global `extern "C"` guard to `PAIGE.H` plus targeted guards in `pgBasics.h`, `pgMemMgr`, `PGFILES.C`, `PGREAD.C`, `PGWRITE.C`, and `PGUNX.C` so every exported Paige entry point now shares predictable C linkage even when the `.C` files build as C++.
- Normalized the Paige handler prototypes (`pgReadHandlerProc`, `pgWriteHandlerProc`, `pgDummy*`) and bridged the few call sites that still take `long*` (style run packers, hyperlink packers) to keep types consistent without rewriting legacy helpers.
- Updated the headless test toolchain to define `HEADLESS_LINKS_REAL_PAIGE`, skip the old Paige stubs in `tests/headless_mac_compat.c`, and rebuilt `libpaige.a`; `nm` now shows only `__ZL`/`__ZZ` locals with no exported mangled symbols.
- Rebuilt `tests/runtime_tests` against the refreshed library. The binary still crashes under `FRONTIER_REGEN_ROOT=databases/Frontier.root ./tests/runtime_tests`; LLDB stops in `advance_style_run` (PGEDIT.C:1942) while `wp_portable_pack_text_for_test` is manipulating style runs for the RTF smoke test.
- Fixed a long-standing `dbgetsize` stack clobber (`Common/source/db.c` now keeps the `size` local as `long`). The crash during the migration pass was the 64-bit runtime writing an eight-byte block size into a four-byte `tyvariance`, overwriting the caller’s stack and sending `logicalsize` to `_mh_execute_header`. With the fix in place, `runtime_tests` no longer segfaults and instead hits the expected Paige bootstrap failure: `pgAllocateNewRef` raises `pgFailure` inside `pgInit`, we catch it via `PG_TRY/PG_CATCH`, emit `[wp-runtime] pgInit failed error=0 ref=0x0`, and the migrator now bails cleanly after logging the failing table (`funStuff`/`examples`).
- Instrumented `pgFailure` (guarded by `FRONTIER_TESTS`) to dump the error code and return addresses, then reconfigured the Paige CMake build back to `Debug` so the PG_TRY/PG_CATCH blocks aren’t optimized away. With the Debug build in place, `pgInit`/`pgNewShell` succeed and we now hit the next blocker: `runtime_tests` aborts with `malloc: Heap corruption detected, free list is damaged` immediately after `pgNewShell` returns in `wp_portable_pack_text_for_test`.

## Recent Updates — November 11, 2025
- Replaced the headless WP stub with a real Paige-backed runtime (`portable/wptext_runtime.c`). External values now carry a `wp_portable_state` with cached header timestamps plus an optional live `pg_ref`, so `wpverbinmemory`, `wpverbgetsize`, and `wpverbgettimes` behave like the legacy desktop build.
- `wp_portable_init()` exposes `pg_globals`/`pgm_globals` through `wp_portable_pg_globals()` and `wp_portable_mem_globals()`. The CLI and runtime tests initialize Paige before evaluating scripts, which lets us unpack legacy WPText trailers, repack them, and run the migrator path without touching QuickDraw.
- The runtime now emits the new `WPRT` portable header: packing forces a Paige-to-RTF export, prepends the `[tywpportableheader][UTF-8 RTF]` blob, and marks Paige as **conversion-only** going forward. Loading detects the magic, rehydrates RTF via Paige when needed, and still falls back to legacy trailers for older roots.
- `wpverbpack` mirrors Save/Save As semantics: forced repacks (`flconvertingolddatabase`, `fldatabasesaveas`, dirty docs, or any v7 root) hydrate Paige, export the current payload, update timestamps/ctsaves, and call `dbassignhandle` before pushing the db address back on the packed handle. This gives the migrator a deterministic path for rewriting WPTexts.
- Updated `planning/phase3/carbon_migration/wptext_format.md` and `planning/phase3/carbon_migration/wptext_rtf_tracker.md` to document the `WPRT` layout, note that Paige is conversion-only, and track the remaining validation work.
- Added a headless-only helper (`wp_portable_pack_text_for_test`) plus a runtime smoke test that builds a WPText from UTF-8, packs it, and asserts the emitted handle has `WPRT` magic, consistent sizing, and balanced braces—our first automated regression that the serializer produces parseable RTF.

## Recent Updates — November 10, 2025
- Added a UNIX/headless platform definition to Paige’s core headers plus a portable CMake configuration. The Paige build now runs with Clang via `third_party/cmake-install/bin/cmake`, stalls only on the missing memory-handle traps, and gives us concrete follow-up items instead of SDK errors.
- `third_party/Paige` now builds end-to-end on the headless toolchain: `pgMTraps` has malloc-backed handles, `pgIO` exports POSIX file I/O, and the CMake target skips `PGWIN.C/PGDLL32.C` when not on Windows. The generated `build-headless/libpaige.a` is a universal archive (arm64 + x86_64) so Intel Macs can run the same bits we test on Apple Silicon.
- Headless builds (CLI + tests) link against `libpaige.a`, add `../third_party/Paige/PGHEADER` to the include path, and trigger the Paige build automatically. With the new runtime layer in place, the tests no longer depend on `headless_wp_stubs.c`.
- Documented in the WPText tracker that the immediate blockers were the memory macros in `pgMTraps.h` and the platform stubs in `PGPLATFO/PGWIN.C`, then resolved those blockers by introducing the UNIX platform module and malloc-backed handles.
- Captured the follow-up items (portable header, serializer, migrator tests) so future agents can focus on behavior instead of rediscovering the build steps.

## Recent Updates — November 9, 2025
- Table serializer now emits a v4 header with a 1 KB reserved block for future metadata (timestamped in `Common/source/langhash.c`). The migrator reads legacy v3 headers, zero-fills the reserved region, and legacy payload converters now pad modern headers accordingly, so we have guaranteed space for refcons/window state before touching outline/scripts.
- Runtime tests now include a regression that unpacks the serialized header and asserts the version/reserved bytes are correct, so future changes can’t accidentally strip the new space.
- Headless outline/script packing moved to a portable header (version 4) that strips QuickDraw UI fields and zero-fills a 1 KB reserved block; `oppack`/`opunpack` detect the new layout while still accepting legacy v2/v3 payloads.
- Legacy table converter now reconstructs modern merged handles using the documented `[outer merge][inner header+records][strings][formats]` layout. We retired the heuristic splitter, so every legacy table (including `system.verbs.*`) hydrates without manual trimming.
- `frontier-cli` no longer relies on `langrunstringnoerror`; we feed every inline script through `langrun`, then render results via `hashgetvaluestring`. That fixes “silent” failures for non-string values and gets `defined(system.verbs.globals)` working end-to-end.
- `tests/cli_system_defined` still fails at `clock.now()` because the script resolution path never reaches the real implementation—`langrun` returns false with an empty error. The logs show repeated `langsearchpathlookup` misses (e.g., `script`, `scriptions`), so the remaining work is to confirm that `pathstable` is populated before evaluation and that `langsearchpathvisit` walks those addresses in headless mode.
- Added headless-only logging inside `langsearchpathvisit/langsearchpathlookup` (see `Common/source/langvalue.c`). Running `script.getText(...)` now shows each `path##` entry visit plus the final hit, proving that the search path wiring works even though deeper script helpers still fail.
- WPText → RTF migration tracker lives at `planning/phase3/carbon_migration/wptext_rtf_tracker.md` (Paige build, portable header, serializer, tests). Refer to that document for the current checklist and status before starting any work on WP serialization.
- Captured a dedicated Paige portability TODO (`planning/phase3/carbon_migration/paige_portability_todo.md`) so the remaining machine-layer/graf/clipboard shims are tracked separately from the RTF migration work. That document should reach ✅ on items 1–7 before we remove `tests/headless_wp_stubs.c` or rely on the real Paige runtime.
- Added the first headless Paige machine layer (`third_party/Paige/PGPLATFO/PGUNX.C`) and wired it into the Paige build so the static library now resolves `pgMachineInit`, `pgClipGrafDevice`, `pgMeasureText`, etc., without depending on QuickDraw/GDI or the linker’s `-undefined dynamic_lookup` escape hatch. The snapshot is now vendored (not a submodule) and pinned to commit `a2fe9b1`.
- `portable/wptext_portable.c` now calls the real Paige bootstrap (`pgMemStartup` / `pgInit`) and exposes `wp_portable_init()` so headless callers can initialize the engine without touching the UI stack.

## Immediate Next Steps — November 16, 2025
1. **LLDB focus on doc-info handles**
   - Use the tmux-backed LLDB session to break inside `pgReadHandlerProc` right after the `doc_info_key` switch finishes unpacking `doccomm`. Watch `docinfo.subject`/`access_count` to pinpoint the redundant `UnuseMemory` and patch or guard that path.
2. **Audit scrap-reader balance**
   - After the reader path is fixed, rerun the guard-malloc repro; if `pgScrapMemoryRead` still underflows the handle, capture its stack (via the same LLDB session) and ensure it only calls `UnuseMemory` when it actually `UseMemory`'d the doc-info ref.
3. **Regression + migrator run**
   - Once both underflows are resolved, rerun `FRONTIER_REGEN_ROOT=databases/Frontier.root ./tests/runtime_tests` (normal + guard malloc) to confirm the WPText smoke test and resume the broader `advance_style_run` / migration validation.

Reference: `planning/phase3/carbon_migration/wptext_rtf_tracker.md` for the full checklist and supporting subtasks.

## Archived from _CURRENT_STATUS — 2025-12-02
- 2025-12-01 (Codex): Save As now reads headers from the destination DB; `FRONTIER_REGEN_ROOT=databases/Frontier.root ./tests/runtime_tests` succeeds and emits a v7 system root at `databases/Frontier.root7`. `frontier-cli --system-root … -e "clock.now()"` currently segfaults; needs investigation before enabling the glue tests.
- 2025-12-01 (Codex): Headless address normalization now skips legacy/v6 roots to avoid mis-marking live blocks as free during migration; migrated `system.verbs.builtins.clock` is still empty, so the packer is dropping the compiled script externals and needs a follow-up fix.
- 2025-11-28 (Codex): Migration runs now complete with canonical v7 headers: `version=7`, `headerLength=88`, view0 set to the new root, other views zeroed, Cancoon dropped. Free-block externals are skipped during packing so the packer no longer dies on stale addresses. Header logging is gated by `FRONTIER_DB_TRACE_HEADERS`. Added runtime header regression (migration path) and unit test for canonical header size/version.
- 2025-11-27 (Codex): Seeded legacy packer fork (`Common/source/legacy/*_legacy.c`) and adjusted modern `tablepack.c` to default BE64. Added thread-local format mode stack (`db_format_mode_push/pop`) and removed `use_64bit_format` flips across packers/tests/stubs; all tests rebuilt and pass (`db_format_tests`, `runtime_tests`, `cli_runtime_tests`). BE64 view0 serialization now covered by unit test. Outstanding warnings trimmed to none in tests (runtime retains known logs only).
- 2025-11-26 (Codex): Reader/writer split into `db_reader_legacy.c`, `db_reader_modern.c`, `db_writer_modern.c`; `make -C tests db_format_tests` passes with the longstanding `__builtin_return_address` warning and an unused helper in `db.c`.
- 2025-11-26 (Codex): Detailed split plan lives at `planning/phase3/modern_reader_writer_split.md`.
- 2025-11-26 (Codex): Payload widening round-trip is **critical**: plan updated to add synthetic legacy→modern→modern-read equality tests, file-level migrated root validation, and BE64 payload widening (tables/records/externals) before claiming the split complete. See `planning/phase3/modern_reader_writer_split.md`.
- 2025-11-26 (Codex): Design principle: keep modern BE64 code branch-free—fork legacy vs modern logic into separate functions/files instead of runtime format branches.
- 2025-11-26 (Codex): Naming decision: modern packers keep canonical names; legacy packers move to `legacy_*/` dirs with `_legacy` entry points so modern remains the default surface.
- Branches in flight (historic): `feature/carbon-migration-plan`, `feature/headless-system-bootstrap`, `feature/legacy_adapter_widening_and_v7_reader`.
- 2025-12-02 (Codex): Confirmed `_CURRENT_STATUS.md` per request; ready to collect crash details for `frontier-cli --system-root databases/Frontier.root7 -e "clock.now()"` so next session can resume with debugging in hand.
- 2025-12-02 (Codex): Added mode presets + scoped `modern_write_repack` pushes around Save As boundaries (migrator, Cancoon, CLI hydrate, ODB saves); `make -C tests db_format_tests` and `make -C tests save_migration_tests` now pass with adapter isolation in place.
- 2025-12-02 (Codex): Reproduced the CLI segfault under LLDB; crash occurs immediately after `findnamedtable` probes `system.temp` (first miss, second hit, then null PC). Need to verify temp table creation/linkage during `load_system_root_database` and ensure `system.temp` stays in-memory-only like the UI build.
- Historic Open Items:
  - CLI segfault when running `frontier-cli --system-root databases/Frontier.root7 -e "clock.now()"`; load path currently dies after probing `system.temp` and failing to resolve `clock.*`.
  - Migrated v7 root is missing `system.verbs.builtins.clock` contents (legacy has 9 scripts); migrator/packing is dropping those externals.
  - Adapter repack flag still logs as `0` during Save As despite migration success; confirm whether it is still needed or if the current rewrite is sufficient.
- Historic Next Steps:
  - Debug the `frontier-cli` crash on `clock.now()` against `databases/Frontier.root7`: capture useful LLDB context (regs/frames), inspect `system.temp` creation/linkage, and verify script execution push/pop state.
  - Fix migrator/packing for `system.verbs.builtins.clock`: force compiled script externals to load/repack instead of being dropped as “free” blocks, regenerate v7 root, and re-test `clock.now()/ticks/waitSeconds`.
  - Verify Save As adapter state (`adapter_repack`) and confirm whether any remaining legacy globals need to be forced dirty during migration.
  - Re-run runtime/CLI regression suites once the crash is resolved; keep the regenerated v7 root in place for the glue tests.
  - See also: planning/phase3/adapter_mode_isolation.md for the mode-split plan (legacy read vs modern write/repack vs modern read).
  - Capture the refreshed `frontier-cli` crash stack now that adapter mode isolation is active and verify the repack logs (`adapter_repack=1`) during Save As.
