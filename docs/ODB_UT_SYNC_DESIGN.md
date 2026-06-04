# Bidirectional ODB <-> .ut Sync: Design

**Audience**: internal technical (JES, Claude, future agents).

**Status**: SHIPPED. Originally written 2026-06-03 as a design proposal; the feature shipped via #698 (core sync) and #699 (lossless percent-encoding + import-discovery scan). This doc is retained as the design-rationale / mechanism deep-dive. For the agent-facing workflow guide (how to edit/debug/review under sync), see `docs/usertalk/UT_SYNC_WORKFLOW.md`. Original audit covered the ODB, the `.ut` corpus, the existing sync verifier, and the boot sequence.

**Goal** (JES): get all UserTalk under source control as `.ut` files while still being able to edit live in Frontier. Edits flow both ways automatically:

- Save a script in the ODB -> its `.ut` file updates.
- Launch the app with updated `.ut` files -> they are imported early in startup.
- Conflicts resolved by modification time (newer wins).

The motivating failure: `system.menus.buildMenubar` (the authoritative menubar builder) exists only in the live legacy ODB and was never exported to the corpus. Nothing detected the gap. A reliable sync system makes that class of silent drift impossible.

---

## TL;DR for the impatient

1. The corpus path-mapping, the read-only drift verifier, the script-write path, and a clean pre-startupScript boot window all **already exist**. Good foundation.
2. **The mtime-wins conflict rule IS implementable.** The ODB stores a real per-*script* modification time, reachable from UserTalk as `timeModified(@scriptAddress)` (and `timeCreated(@scriptAddress)`). Verified live: two scripts in the same hashtable return different mod dates (`timeModified(@system.verbs.builtins.op)` = 3/23/2026 vs `@system.verbs.builtins.string` = 3/21/2026), which is only possible if the timestamp is stored per external value, not per table. So "ODB script timestamp" is a real per-script quantity after all. (See Section 5.)
3. Recommended path: **mtime-wins as the primary conflict rule** (your original choice), using `timeModified(@script)` on the ODB side and file mtime on the `.ut` side. A **sidecar sync manifest** (per-script content hash + last-synced time) is still worth adding as a *safety layer* — it turns a true two-sided edit into a detectable conflict instead of a silent clobber, and protects against clock skew — but it is now an optional hardening, not a forced workaround for a missing timestamp.
4. Build order: exporter -> verifier-as-gate -> boot import pass -> save hook, with the optional content-hash manifest slotted in whenever two-sided-edit safety is wanted. Each step is independently useful.

---

## What exists today (audit results)

| Capability | Status | Location |
|---|---|---|
| Path mapping `a/b/c.ut` <-> `a.b.c` | EXISTS (lexical, no escaping) | `tools/verify_virgin_root_sync.py:30-34` |
| Read-only drift verifier (ODB vs corpus, byte-compare) | EXISTS | `tools/verify_virgin_root_sync.{py,sh}`, `make verify-odb-sync` (`Makefile:33-34`) |
| Script write path (source -> ODB node) | EXISTS | `script.newScriptObject` (`Common/source/opverbs.c:2335-2389`); `langcompiletext` (`lang.c:1003`); `hashtableassign` (`opverbs.c:2382`) |
| Clean pre-startupScript boot window | EXISTS | between `hydrate_system_root_database` (`main.c:582`) and `loadsystemscripts` (`main.c:594`) |
| Per-*script* mod timestamp | EXISTS | `timeModified(@adr)` / `timeCreated(@adr)` UserTalk verbs; kernel `langexternalgettimes` -> `opverbgettimes` for scripts (`langexternal.c:3213`); verified live (siblings in one table return different dates) |
| Per-table mod timestamp | EXISTS (rarely used) | `tyhashtable.timelastsave` (`lang.h:532`); `odbGetModDate` / `db.getModDate` (`odbengine.c:1237-1267`) — JES: "we almost never used db.getModDate" |
| Kernel-body / comment normalization (round-trip) | EXISTS (C authoritative) | `ut_canonicalize_outline_text` / `ut_decanonicalize_outline_text` in `frontier-cli/ut_sync.c`; Python `normalize_kernel_body` is now a parity-tracked reference (`tests/ut_sync_canonicalize_tests.c` proves byte-equality) |
| ODB -> .ut exporter (runtime) | EXISTS | `ut_export_script` (`ut_sync.c`), driven by the save-on-exit walk `ut_export_walk_table` (`main.c`); gated by `--ut-sync-dir` / `FRONTIER_UT_SYNC_DIR` |
| .ut -> ODB importer (runtime) | EXISTS | `ut_import_hook` + `ut_import_check` fired from `opverbinmemory` (`opverbs.c`) on hydrate/materialize; last-write-wins by mtime |
| Save hook (ODB write -> .ut export) | EXISTS | `save_system_root_on_exit` -> `ut_export_walk_table` (`main.c`) |
| Boot import pass (.ut -> ODB) | EXISTS | import site inside the kernel `opverbinmemory` materialize path (catches live out-of-band .ut edits) |
| Pre-commit / CI gating | PARTIAL | git-time drift verifier is manual / warn-only; runtime sync now lifecycle-tested (`tests/ut_sync_lifecycle_test.sh`) |

**Stale-path note to fix in passing**: the verifier's `DEFAULT_CORPUS_ROOT` and issue #675's prose reference `databases/usertalk_scripts/`, but the live corpus is `usertalk_scripts/Frontier.root/` (4677 `.ut` files). Reconcile during Phase 1.

---

## Round-trip fidelity: the hard correctness surface

ODB <-> .ut is not a byte-identity transform. The known transforms (from `normalize_kernel_body`):

- **Encoding**: kernel `string()` emits **MacRoman**; the `.ut` corpus is **UTF-8**. Protocol JSON with high-bit bytes is not valid UTF-8 (the verifier parses with regex, not `json.loads`, for this reason).
- **Line endings**: kernel emits CR (`0x0D`); `.ut` uses LF.
- **Comment markers**: kernel emits MacRoman `0xC7`/`«` (line-comment open) and `0xC8`/`»` (block-comment close); `.ut` uses `//` and drops `»`. The substitution **must be literal-aware** — `«»` inside string/char literals are content, not comment syntax (a prior bug; see task #52).
- **Trailing newline**: `.ut` has none; the ODB form may.
- **Brace-wrapped comment subtrees**: the historical ad-hoc exporter **dropped** these. Any new exporter must preserve them or round-trip is lossy.

**Design requirement (SATISFIED)**: there must be exactly ONE canonicalization library, shared by exporter, importer, and verifier. This is now `ut_canonicalize_outline_text` / `ut_decanonicalize_outline_text` in `frontier-cli/ut_sync.c`, used by both the runtime exporter (`ut_export_script`) and importer (`ut_import_check`). The Python `normalize_kernel_body` in the git-time verifier is a deliberate parity-tracked copy, not an independent reimplementation: `tests/ut_sync_canonicalize_tests.c` asserts the C output is byte-identical to the Python reference on the real corpus, so the two cannot silently drift. Rule: change the C function first, then mirror it in Python.

---

## The two directions

### Direction A: ODB -> .ut (on save)

**Trigger.** There is no single "save" chokepoint today. Two sub-cases:

- **GUI editor (future)**: a script-editor save is a natural hook point. When it lands, fire export-for-this-script.
- **Protocol mode (now)**: agents edit via `script`/`eval` + `hashtableassign`. There is no write callback on script nodes. Options:
  - **A1 (explicit, available now)**: an export command/verb the agent calls after editing — `export script <path>` writes the `.ut`. This is the current manual discipline, formalized as a tool. Low risk, no kernel change.
  - **A2 (automatic, needs kernel work)**: a write-hook on script-typed nodes that marks them dirty and exports on transaction/idle. Requires adding a dirty-flag + hook to the ODB write path. Higher risk, deferred.

**Recommendation**: ship A1 first (formalized exporter + "export after edit" in the protocol workflow), design A2 as a later enhancement once the GUI editor defines the real save semantics.

**Mechanism.** Read the script's source from the ODB (protocol `debug/getSource` or in-process), run the shared canonicalizer (ODB-form -> .ut-form), write the `.ut` at the mapped path, set the `.ut` mtime from the script's `timeModified(@script)` so the two sides agree (and update the optional sync manifest, Section 5).

### Direction B: .ut -> ODB (at boot)

**Where.** The import pass runs in the `main.c:582-594` window: after `hydrate_system_root_database` (ODB open, interpreter ready) and **before** `loadsystemscripts` (which runs `startup.startupScript`). At that point no user script has executed.

**Chicken-and-egg risk.** The import pass itself uses the interpreter (`langcompiletext`, `hashtableassign`) — fine, the ODB is hydrated. But if an imported `.ut` is one that boot transitively depends on (`startup.startupScript`, `installReplMenubar`, anything they call), importing a *broken* edited version could brick boot. Mitigations:
- The import pass **only compiles and assigns** — it never *executes* imported scripts.
- Compile failures are non-fatal: log, skip that script, leave the ODB copy intact, continue boot. A bad `.ut` edit must not prevent the app from starting.
- Reuse `script.newScriptObject`'s outline-construction path (per issue #676) so imported scripts match the structure the exporter reads back.

**Scope control.** Importing all 4677 scripts every boot is wasteful and risky. The pass should import only scripts whose `.ut` is newer than the ODB's `timeModified(@script)` (mtime-wins, Section 5) — or, if the optional manifest is in use, only scripts whose `.ut` hash changed since last sync.

---

## Section 5: Conflict resolution — mtime-wins works, with an optional safety layer

### Per-script timestamps exist (corrected finding)

The decision was "file mtime vs ODB timestamp, newer wins." An earlier draft of this doc claimed that rule was unimplementable because the ODB had only a per-*table* `timelastsave`. **That was wrong.** Frontier stores a real per-*script* modification time, exposed to UserTalk as:

- `timeModified(@scriptAddress)` -> the script's last-modified datetime
- `timeCreated(@scriptAddress)` -> the script's creation datetime
- setters `setTimeModified` / `setTimeCreated`

This was verified empirically (frontier-cli eval against a Virgin.root copy): two scripts in the **same** hashtable return **different** mod dates --
`timeModified(@system.verbs.builtins.op)` = `3/23/2026; 9:38:38 PM`,
`timeModified(@system.verbs.builtins.string)` = `3/21/2026; 8:53:05 PM`.
Different timestamps for siblings in one table is only possible if the time is stored **per external value**, not per table. (Kernel path: `langexternalgettimes`, `langexternal.c:3213`, dispatches per external type to `opverbgettimes` for script-typed values.)

Note the verb is **`timeModified`, not `lastModified`** — there is no `lastModified` keyword in the headless runtime. The per-table `db.getModDate` (`odbGetModDate`, `odbengine.c:1263`, returning the parent table's `timelastsave`) is a different, coarser thing and is rarely used.

### Conflict rule: mtime-wins (primary)

The original choice is directly implementable:

- **ODB side**: `timeModified(@script)` for the per-script ODB time.
- **`.ut` side**: filesystem mtime of the mapped `.ut`.
- **Rule**: newer wins. Only `.ut` newer -> import `.ut` -> ODB. Only ODB newer -> export ODB -> `.ut`. Neither side changed since last sync -> skip.

This is enough for the common single-editor case (one human or one agent editing at a time), which is the current reality.

### Optional safety layer: sidecar sync manifest

Pure mtime-wins has two well-known weaknesses: it silently clobbers when **both** sides changed since the last sync (it just picks the newer one), and it is vulnerable to clock skew between the ODB-host clock and the filesystem clock. To harden against those, add a version-controlled manifest (e.g. `usertalk_scripts/.sync-manifest.json`) mapping each script path to `{ ut_hash, odb_hash_at_last_sync, last_synced_utc }`:

- Only `.ut` content-hash changed -> import.
- Only ODB content-hash changed -> export.
- **Both changed -> CONFLICT**: halt that script, report it, do not clobber. mtime *ranks* the conflict list but never auto-resolves a two-sided edit.
- Neither changed -> skip.

The manifest turns "both sides edited since last sync" into a *detectable, surfaced* event instead of a silent loss. It is **optional hardening**, not a prerequisite — ship mtime-wins first; add the manifest when concurrent two-sided editing (or untrusted clocks) becomes a real risk.

**Conflict rule summary**: mtime-wins as the primary arbiter (uses `timeModified(@script)`); optional manifest content-hash layer to detect and halt true two-sided conflicts. Both honor JES's original "newer wins" choice — the manifest only adds a guard rail, it does not replace the rule.

---

## Proposed build sequence

Each phase is independently shippable and de-risks the next. (Phase 1 is the ODB->.ut half; 2 makes drift impossible to commit; 3-4 are the .ut->ODB half; the optional manifest can be added at any point after Phase 1.)

**Phase 1 — Canonicalizer + exporter (ODB -> .ut).**
Extract the shared canonicalization module (from `normalize_kernel_body`), build `tools/export_odb_to_ut.py` that exports the full script tree (or one path) from a `--protocol` session. Verify it round-trips against the existing corpus (the 4677 files should re-export byte-identical). Preserve brace-wrapped comment subtrees. Set each exported `.ut`'s mtime from `timeModified(@script)`. Fix the `DEFAULT_CORPUS_ROOT` stale path. *Output: a committed, reproducible exporter — closes the "buildMenubar was never exported" class of bug for ODB->.ut.*

**Phase 2 — Verifier as a gate.**
Wire `verify-odb-sync` into the pre-commit hook and, when CI exists, into CI. Re-validate issue #675's "gates commits" exit criterion (currently unmet — the verifier ships but isn't wired in). *Output: drift can no longer be committed silently.*

**Phase 3 — Boot import pass (.ut -> ODB).**
Implement the import pass in the `main.c:582-594` window: mtime-driven (import a `.ut` only when its mtime is newer than the ODB's `timeModified(@script)`), compile-and-assign only (no execution), non-fatal on compile error, reuse `script.newScriptObject` construction. On import, set the new ODB script's `timeModified` to match the `.ut` mtime so the next boot sees them as in-sync. Gate behind a flag initially (e.g. `FRONTIER_UT_IMPORT=1`) so it's opt-in until proven. Add an integration test that edits a `.ut`, boots, and asserts the ODB picked it up — and one that feeds a broken `.ut` and asserts boot still completes. *Output: agent edits to `.ut` land in the running app automatically.* This is the .ut->ODB half of issue #676.

**Phase 4 — Save hook (ODB -> .ut), A1 then A2.**
A1: a protocol/CLI `export script <path>` command + a documented "export after edit" step. A2 (later, with the GUI editor): an ODB write-hook that auto-exports on save. *Output: ODB edits flow back to `.ut` without a manual re-export.*

**Optional — Sync manifest (any time after Phase 1).**
Define a manifest format and a `tools/sync_manifest.py` that computes/updates per-script content hashes + last-synced time. Lets the verifier and the boot pass detect true two-sided edits (both ODB and `.ut` changed since last sync) and halt instead of clobbering — the clock-skew/concurrent-edit guard rail described in Section 5. Not required for the single-editor mtime-wins path; add it when concurrent editing becomes a real risk.

---

## Open questions for JES

1. **Conflict model** (Section 5): your original **mtime-wins** rule is implementable as-is via `timeModified(@script)` — no change forced. Open sub-question: do you want the optional content-hash manifest layer now (detects/halts true two-sided edits, guards against clock skew), or is single-editor mtime-wins enough for the current AI-agent-via-protocol workflow?
2. **Canonical source of truth.** Issue #676 wants the `.ut` corpus to be authoritative (Virgin.root becomes a build artifact). The boot-import design (Direction B) is compatible with that. But Direction A (ODB->.ut on save) implies the ODB is *also* authoritative between syncs. mtime-wins (and the optional manifest) reconciles them, but: do you want a steady-state where **both** are live-editable, or an eventual end-state where **`.ut` is the only source** and the ODB is always rebuilt from it (#676)? This affects how aggressive Phase 3/4 should be.
3. **A2 timing.** Is the automatic ODB-write-hook worth building before the GUI editor exists, or is the A1 explicit-export command enough for the current AI-agent-via-protocol workflow?
4. **Startup ordering edge case.** Should the import pass run before *or* after guest-database / EFP linking (also in the hydrate path)? Default proposal: after hydrate (system root fully linked), before `startup.startupScript`. Confirm no script the import pass needs is loaded later than that.

---

## See also

- `docs/LEGACY_MENU_SYSTEM.md` gap #1 — the `system.menus.buildMenubar` absence that motivated this.
- Issue #675 (closed) — Virgin.root sync verification (the read-only verifier; CI gating deferred).
- Issue #676 (open) — Virgin.root as a build artifact derived from the `.ut` corpus (the .ut->ODB half).
- `tools/verify_virgin_root_sync.py` — existing verifier + `normalize_kernel_body` canonicalization.
- Memory: `project_usertalk_transition.md`, `feedback_usertalk_workflow.md` — the C->UserTalk transition and the change-ODB-first-then-export discipline this design automates.
