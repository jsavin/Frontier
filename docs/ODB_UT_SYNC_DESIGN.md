# Bidirectional ODB <-> .ut Sync: Design

**Audience**: internal technical (JES, Claude, future agents).

**Status**: design proposal. No implementation yet. Written 2026-06-03 after an infrastructure audit of the current ODB, the `.ut` corpus, the existing sync verifier, and the boot sequence.

**Goal** (JES): get all UserTalk under source control as `.ut` files while still being able to edit live in Frontier. Edits flow both ways automatically:

- Save a script in the ODB -> its `.ut` file updates.
- Launch the app with updated `.ut` files -> they are imported early in startup.
- Conflicts resolved by modification time (newer wins).

The motivating failure: `system.menus.buildMenubar` (the authoritative menubar builder) exists only in the live legacy ODB and was never exported to the corpus. Nothing detected the gap. A reliable sync system makes that class of silent drift impossible.

---

## TL;DR for the impatient

1. The corpus path-mapping, the read-only drift verifier, the script-write path, and a clean pre-startupScript boot window all **already exist**. Good foundation.
2. **The mtime-wins conflict rule cannot be implemented as stated today.** The ODB has no per-*script* timestamp — only a per-*table* `timelastsave` shared by every script in that table. Editing one script bumps the timestamp for all its siblings. So "ODB script timestamp" is not a real per-script quantity. This is the one decision that needs revisiting (Section 5).
3. Recommended path: a **sidecar sync manifest** (per-script content hash + last-synced time) as the source of conflict truth, with mtime as a tie-breaker hint. This is robust against the per-table-timestamp problem and against clock skew, and it makes "both sides changed" a detectable conflict rather than a silent clobber.
4. Build order: exporter -> manifest -> verifier-as-gate -> boot import pass -> save hook. Each step is independently useful.

---

## What exists today (audit results)

| Capability | Status | Location |
|---|---|---|
| Path mapping `a/b/c.ut` <-> `a.b.c` | EXISTS (lexical, no escaping) | `tools/verify_virgin_root_sync.py:30-34` |
| Read-only drift verifier (ODB vs corpus, byte-compare) | EXISTS | `tools/verify_virgin_root_sync.{py,sh}`, `make verify-odb-sync` (`Makefile:33-34`) |
| Script write path (source -> ODB node) | EXISTS | `script.newScriptObject` (`Common/source/opverbs.c:2335-2389`); `langcompiletext` (`lang.c:1003`); `hashtableassign` (`opverbs.c:2382`) |
| Clean pre-startupScript boot window | EXISTS | between `hydrate_system_root_database` (`main.c:582`) and `loadsystemscripts` (`main.c:594`) |
| Per-table mod timestamp | EXISTS | `tyhashtable.timelastsave` (`lang.h:532`); `odbGetModDate` / `db.getModDate` (`odbengine.c:1237-1267`) |
| Kernel-body / comment normalization (round-trip) | PARTIAL | `normalize_kernel_body` in the verifier (`verify_virgin_root_sync.py:355-414`) |
| ODB -> .ut exporter (committed tool) | **MISSING** | corpus was produced ad hoc |
| .ut -> ODB importer (committed tool) | **MISSING** | issue #676 proposes `tools/build-virgin-root.sh` |
| Per-*script* mod timestamp | **MISSING** | ODB has only per-table (see Section 5) |
| Save hook (ODB write -> .ut export) | **MISSING** | no write-time chokepoint today |
| Boot import pass (.ut -> ODB) | **MISSING** | the `main.c:582-594` window is unused |
| Pre-commit / CI gating | **MISSING** | verifier is manual-only; #675 deferred CI |

**Stale-path note to fix in passing**: the verifier's `DEFAULT_CORPUS_ROOT` and issue #675's prose reference `databases/usertalk_scripts/`, but the live corpus is `usertalk_scripts/Frontier.root/` (4677 `.ut` files). Reconcile during Phase 1.

---

## Round-trip fidelity: the hard correctness surface

ODB <-> .ut is not a byte-identity transform. The known transforms (from `normalize_kernel_body`):

- **Encoding**: kernel `string()` emits **MacRoman**; the `.ut` corpus is **UTF-8**. Protocol JSON with high-bit bytes is not valid UTF-8 (the verifier parses with regex, not `json.loads`, for this reason).
- **Line endings**: kernel emits CR (`0x0D`); `.ut` uses LF.
- **Comment markers**: kernel emits MacRoman `0xC7`/`«` (line-comment open) and `0xC8`/`»` (block-comment close); `.ut` uses `//` and drops `»`. The substitution **must be literal-aware** — `«»` inside string/char literals are content, not comment syntax (a prior bug; see task #52).
- **Trailing newline**: `.ut` has none; the ODB form may.
- **Brace-wrapped comment subtrees**: the historical ad-hoc exporter **dropped** these. Any new exporter must preserve them or round-trip is lossy.

**Design requirement**: there must be exactly ONE canonicalization library, shared by exporter, importer, and verifier. Today the normalization logic lives only in the verifier. If the exporter re-implements it independently, the two will drift and we recreate the original problem one level up. Extract `normalize`/`canonicalize` into a single module that all three tools import.

---

## The two directions

### Direction A: ODB -> .ut (on save)

**Trigger.** There is no single "save" chokepoint today. Two sub-cases:

- **GUI editor (future)**: a script-editor save is a natural hook point. When it lands, fire export-for-this-script.
- **Protocol mode (now)**: agents edit via `script`/`eval` + `hashtableassign`. There is no write callback on script nodes. Options:
  - **A1 (explicit, available now)**: an export command/verb the agent calls after editing — `export script <path>` writes the `.ut`. This is the current manual discipline, formalized as a tool. Low risk, no kernel change.
  - **A2 (automatic, needs kernel work)**: a write-hook on script-typed nodes that marks them dirty and exports on transaction/idle. Requires adding a dirty-flag + hook to the ODB write path. Higher risk, deferred.

**Recommendation**: ship A1 first (formalized exporter + "export after edit" in the protocol workflow), design A2 as a later enhancement once the GUI editor defines the real save semantics.

**Mechanism.** Read the script's source from the ODB (protocol `debug/getSource` or in-process), run the shared canonicalizer (ODB-form -> .ut-form), write the `.ut` at the mapped path, update the sync manifest (Section 5).

### Direction B: .ut -> ODB (at boot)

**Where.** The import pass runs in the `main.c:582-594` window: after `hydrate_system_root_database` (ODB open, interpreter ready) and **before** `loadsystemscripts` (which runs `startup.startupScript`). At that point no user script has executed.

**Chicken-and-egg risk.** The import pass itself uses the interpreter (`langcompiletext`, `hashtableassign`) — fine, the ODB is hydrated. But if an imported `.ut` is one that boot transitively depends on (`startup.startupScript`, `installReplMenubar`, anything they call), importing a *broken* edited version could brick boot. Mitigations:
- The import pass **only compiles and assigns** — it never *executes* imported scripts.
- Compile failures are non-fatal: log, skip that script, leave the ODB copy intact, continue boot. A bad `.ut` edit must not prevent the app from starting.
- Reuse `script.newScriptObject`'s outline-construction path (per issue #676) so imported scripts match the structure the exporter reads back.

**Scope control.** Importing all 4677 scripts every boot is wasteful and risky. The manifest (Section 5) lets the pass import only scripts whose `.ut` changed since last sync.

---

## Section 5: Conflict resolution — why mtime alone fails, and the fix

### The problem with the chosen rule

The decision was "file mtime vs ODB timestamp, newer wins." The audit shows **the ODB has no per-script timestamp**. `tyhashnode` (the per-identifier struct, `lang.h:436-461`) has no time field. The only timestamp is `tyhashtable.timelastsave` (`lang.h:532`), per *table*. `odbGetModDate` returns the *parent table's* `timelastsave` for a non-table item (`odbengine.c:1263`). So:

- Every script in `system.menus.*` shares one timestamp.
- Editing `system.menus.installReplMenubar` bumps the timestamp that `system.menus.buildMenubar` also reports.
- You cannot tell *which* script in a table changed, or compare a single script's ODB time against its `.ut` mtime meaningfully.

Implementing "newer wins" on this would mis-resolve constantly: a `.ut` edit to script X would look "older" than the ODB simply because an unrelated sibling Y was saved later, or vice versa.

### Three ways to get real per-script time (pick one)

1. **Add a timestamp to `tyhashnode`.** Truest fidelity, but it's a disk-format change to a `pack(2)` legacy struct — high risk, affects every ODB, needs a migration. Not recommended now.
2. **One script per table.** Wrap each script node in its own single-item table so `timelastsave` becomes effectively per-script. Invasive to the ODB shape and to every consumer that walks these tables. Not recommended.
3. **Sidecar sync manifest (recommended).** A version-controlled file (e.g. `usertalk_scripts/.sync-manifest.json`) mapping each script path to `{ ut_hash, odb_hash_at_last_sync, last_synced_utc }`. Conflict logic uses *content hashes*, not timestamps:
   - At sync time, compute current `.ut` hash and current ODB hash (both canonicalized).
   - Compare each against the manifest's last-synced hashes:
     - Only `.ut` changed -> import `.ut` -> ODB.
     - Only ODB changed -> export ODB -> `.ut`.
     - **Both changed -> CONFLICT**: halt that script, report it, do not clobber. (mtime can *rank* the conflict list, but never auto-resolves a two-sided edit.)
     - Neither changed -> skip.
   - Update the manifest after each successful sync.

The manifest makes "both sides edited since last sync" a *detectable, surfaced* event instead of a silent loss — which is the whole point of putting this under source control. mtime becomes a hint (ordering the conflict report, or a fast pre-filter to skip hashing unchanged files), not the arbiter.

**Revised conflict rule**: content-hash three-way compare via the manifest; conflicts halt and report; mtime is an optimization/tiebreaker only. This supersedes the pure-mtime decision — flagged for JES sign-off.

---

## Proposed build sequence

Each phase is independently shippable and de-risks the next. (Phases 1-2 are the ODB->.ut half; 3 makes drift impossible to commit; 4-5 are the .ut->ODB half.)

**Phase 1 — Canonicalizer + exporter (ODB -> .ut).**
Extract the shared canonicalization module (from `normalize_kernel_body`), build `tools/export_odb_to_ut.py` that exports the full script tree (or one path) from a `--protocol` session. Verify it round-trips against the existing corpus (the 4677 files should re-export byte-identical). Preserve brace-wrapped comment subtrees. Fix the `DEFAULT_CORPUS_ROOT` stale path. *Output: a committed, reproducible exporter — closes the "buildMenubar was never exported" class of bug for ODB->.ut.*

**Phase 2 — Sync manifest.**
Define the manifest format and a `tools/sync_manifest.py` that computes/updates it. Backfill it for the current corpus + Virgin.root. *Output: a content-hash baseline that conflict logic and CI both consume.*

**Phase 3 — Verifier as a gate.**
Wire `verify-odb-sync` (extended to consult the manifest) into the pre-commit hook and, when CI exists, into CI. Re-validate issue #675's "gates commits" exit criterion (currently unmet — the verifier ships but isn't wired in). *Output: drift can no longer be committed silently.*

**Phase 4 — Boot import pass (.ut -> ODB).**
Implement the import pass in the `main.c:582-594` window: manifest-driven (import only changed `.ut`), compile-and-assign only (no execution), non-fatal on compile error, reuse `script.newScriptObject` construction. Gate behind a flag initially (e.g. `FRONTIER_UT_IMPORT=1`) so it's opt-in until proven. Add an integration test that edits a `.ut`, boots, and asserts the ODB picked it up — and one that feeds a broken `.ut` and asserts boot still completes. *Output: agent edits to `.ut` land in the running app automatically.* This is the .ut->ODB half of issue #676.

**Phase 5 — Save hook (ODB -> .ut), A1 then A2.**
A1: a protocol/CLI `export script <path>` command + a documented "export after edit" step. A2 (later, with the GUI editor): an ODB write-hook that auto-exports on save. *Output: ODB edits flow back to `.ut` without a manual re-export.*

---

## Open questions for JES

1. **Conflict model** (Section 5): confirm the move from pure-mtime to **manifest content-hash + conflict-halt** (mtime as tiebreaker). This is the one decision the audit forces a change on.
2. **Canonical source of truth.** Issue #676 wants the `.ut` corpus to be authoritative (Virgin.root becomes a build artifact). The boot-import design (Direction B) is compatible with that. But Direction A (ODB->.ut on save) implies the ODB is *also* authoritative between syncs. The manifest reconciles them, but: do you want a steady-state where **both** are live-editable (manifest mediates), or an eventual end-state where **`.ut` is the only source** and the ODB is always rebuilt from it (#676)? This affects how aggressive Phase 4/5 should be.
3. **A2 timing.** Is the automatic ODB-write-hook worth building before the GUI editor exists, or is the A1 explicit-export command enough for the current AI-agent-via-protocol workflow?
4. **Startup ordering edge case.** Should the import pass run before *or* after guest-database / EFP linking (also in the hydrate path)? Default proposal: after hydrate (system root fully linked), before `startup.startupScript`. Confirm no script the import pass needs is loaded later than that.

---

## See also

- `docs/LEGACY_MENU_SYSTEM.md` gap #1 — the `system.menus.buildMenubar` absence that motivated this.
- Issue #675 (closed) — Virgin.root sync verification (the read-only verifier; CI gating deferred).
- Issue #676 (open) — Virgin.root as a build artifact derived from the `.ut` corpus (the .ut->ODB half).
- `tools/verify_virgin_root_sync.py` — existing verifier + `normalize_kernel_body` canonicalization.
- Memory: `project_usertalk_transition.md`, `feedback_usertalk_workflow.md` — the C->UserTalk transition and the change-ODB-first-then-export discipline this design automates.
