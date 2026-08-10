# ODB <-> .ut Sync: Agent Workflow Guide

**Audience**: AI agents (and JES) editing, debugging, or reviewing UserTalk code.

**Load this when**: you are editing UserTalk under `--ut-sync-dir`, debugging why a
`.ut` file did or did not update, reviewing a PR that touches `.ut` files, or
reasoning about how filesystem `.ut` edits reach the live ODB.

This is the *workflow* reference. For the design rationale and the full mechanism
audit see `../ODB_UT_SYNC_DESIGN.md` (note: that doc's "no implementation yet"
status line is stale — the feature shipped via #698/#699; this doc describes the
shipped behavior).

---

## TL;DR for an agent

1. **Edit and debug UserTalk in `--protocol` mode, not by hand-editing `.ut` files.**
   Protocol mode gives you the live runtime: the debugger, ODB storage verbs,
   `typeof`/`defined`/address introspection, and — critically — the real verb
   *interdependencies* (a script that calls other verbs only behaves correctly
   when evaluated in the running system, not when read as text).
2. **Let the outbound `.ut` sync mirror your edits to the filesystem for you.**
   When sync is active, a clean shutdown exports every dirty script to its `.ut`
   file. You edit in the ODB; the `.ut` corpus stays current as a side effect.
3. **Treat `.ut` files as a read-mostly mirror**, not the primary edit surface.
   Editing a `.ut` by hand works (last-write-wins import on next boot), but you
   lose the runtime — no compile check, no debugger, no diagnostics. Reserve
   hand-edits for bulk/mechanical changes you can't easily express via protocol.

The ODB is authoritative. The `.ut` tree is a text projection of it.

---

## Why protocol mode is the primary edit/debug surface

A UserTalk script is not self-contained text. Its correctness depends on:

- **Other verbs it calls** — resolved through the live verb-resolution chain, not
  visible in the `.ut` file.
- **ODB state it reads/writes** — tables, addresses, in-memory values.
- **Runtime behavior** — the debugger, error locations, `typeof()` of results.

Editing a `.ut` file in a text editor gives you none of this. You are editing
text that *looks* like the script but can't be run, compiled, or introspected in
isolation. So:

- **To change a script**: install it via protocol
  (`script.newScriptObject(s, @path)` — see `../ODB_SCRIPT_EDITING.md`), then
  verify by reading it back and *calling it*. The outbound sync writes the `.ut`.
- **To debug a script**: drive it in protocol mode, use the protocol debugger
  (`debugging_workflow.md`), inspect ODB state with storage verbs.
- **To reconcile the filesystem on demand** (e.g. after dropping new `.ut`
  files): call `repl.syncscan()` over the protocol — see "Import-discovery scan".

The `.ut` sync exists so this protocol-first loop *automatically* keeps a
git-trackable text mirror up to date. It is not a substitute for the runtime.

---

## Turning sync on

Sync is off unless you pass the flag:

```
frontier-cli --protocol --system-root <path>.root --ut-sync-dir <DIR> [--skip-startup]
```

- `--ut-sync-dir DIR` (or `FRONTIER_UT_SYNC_DIR` env var; the CLI flag wins).
  Accessor in code: `cli_get_ut_sync_dir()`. Every direction (export, import,
  boot scan, the REPL verb) early-returns to a no-op when this is NULL.
- `--lock-opened-roots` suppresses the on-exit save — and therefore the export.
  Use it for read-only inspection sessions.

Path mapping: ODB dotted path `a.b.c` <-> `<DIR>/<rootBasename>/a/b/c.ut`. For a
system root named `Frontier.root`, `workspace.miniApp.foo` maps to
`<DIR>/Frontier.root/workspace/miniApp/foo.ut`.

---

## Outbound: ODB -> .ut (export)

**Trigger: clean shutdown only.** Export fires from `save_system_root_on_exit()`
during process teardown — for protocol mode, that's `{"op":"shutdown"}` (or EOF
on stdin). It walks every dirty script/outline external and writes its `.ut`.

> **Sharp edge — `fileMenu.save()` does NOT export.** Saving inside a session
> persists to the ODB binary but does not write `.ut` files. The `.ut` corpus
> only catches up at shutdown. If you save and inspect the `.ut` tree without
> exiting, you'll see stale files. To force the export, end the session
> (`{"op":"shutdown"}`).

What export does per script (`ut_export_script` in `frontier-cli/ut_sync.c`):

1. Canonicalize the outline text (MacRoman -> UTF-8, CR -> LF).
2. Map the dotted ODB path to a filesystem path.
3. `mkdir -p` the parent, write to a temp file, atomic `rename()` into place.
4. **Stamp the file's mtime to the ODB script's modified time** (see below).

### Lossless path encoding (the dotted-key problem)

ODB keys can contain characters that collide with the path grammar: `.` (the
segment separator), `/` (the path separator), `:`, `"`, `\`, control bytes, and
a leading `-`. A key like `com.example.plugin` must NOT split into
`com/example/plugin/`.

Export percent-encodes each segment before joining
(`ut_pct_encode_segment`), escape-the-escape-char-first, emitting uppercase
`%XX`:

| Raw | Encoded |
|-----|---------|
| `%` | `%25` (escaped first, always) |
| `.` | `%2E` |
| `/` | `%2F` |
| `:` | `%3A` |
| `"` | `%22` |
| `\` | `%5C` |
| control bytes (`< 0x20`), DEL (`0x7F`) | their own `%XX` |
| leading `-` only | `%2D` |

Everything else passes through literally — so the ~360 well-formed existing
paths are byte-identical and round-trip cleanly.

Example: ODB key `com.example.plugin` under `workspace.miniApp` exports to a
**single file** `workspace/miniApp/com%2Eexample%2Eplugin.ut` — no phantom
intermediate tables. On import the segment is percent-decoded back to the exact
raw key.

---

## Inbound: .ut -> ODB (import, last-write-wins)

A `.ut` file is imported (its content replaces the ODB script body) **only when
its mtime is strictly newer** than the ODB script's recorded modified time:

```c
/* ut_sync.c, import check */
dot_mac_mtime = (int64_t)st.st_mtime + UT_MAC_TO_UNIX_EPOCH_OFFSET;
if (dot_mac_mtime <= odb_mac_mtime) {
    return 0;   /* .ut not newer -> ODB wins, no import */
}
```

The comparison is **strict `>`**. Equal mtime is a no-op (ODB wins). This is the
basis of the no-redundant-reimport guarantee: because export stamps the `.ut`'s
mtime to *exactly* the ODB script time (using the same
`UT_MAC_TO_UNIX_EPOCH_OFFSET = 2082844800` to convert Mac<->Unix epoch), an
unchanged `.ut` compares equal on the next boot and is **not** reimported. You
edit a `.ut` (bumping its mtime past the ODB time) only when you actually want
the filesystem to win.

Import runs from the kernel materialize hook, so it catches boot-time loads,
lazy loads, and live out-of-band edits.

> **Broken `.ut` files are rejected loudly (compile gate).** Before installing
> an inbound script `.ut`, the importer compile-checks it through the same
> chain the runtime uses to compile installed scripts. A `.ut` that does not
> compile is **rejected**: the existing ODB version is kept, the `.ut` is left
> on disk untouched, and an error naming the script is logged
> (`ut-sync import REJECTED for <path>: the .ut does not compile (<error>)`).
> The same gate applies to the import-discovery scan: a broken brand-new `.ut`
> does not create an ODB node (`ut-scan: REJECTED <file>`). Plain outlines
> (non-script) are not compiled, matching their runtime semantics. If you
> hand-edit a `.ut` and the behavior doesn't change, suspect (a) mtime not
> newer, (b) a compile rejection — check the log, or (c) a two-sided conflict
> (see below). Verify by *calling the verb* in protocol mode, not by
> re-reading the file.

### Two-sided conflict detection (`.ut-sync-state`)

Every successful export or import records a per-script content-hash pair in
`<sync_base>/.ut-sync-state` (one line per script; per-machine state,
gitignored — not part of the corpus). A side "changed since the last sync"
when its current hash differs from its recorded hash:

- Only the `.ut` changed → import (normal mtime rule applies).
- Only the ODB changed → export on shutdown (as always). A `.ut` whose mtime
  was bumped but whose *content* is unchanged no longer re-imports over an
  ODB edit — the ODB wins, with a warning.
- **Both changed → CONFLICT.** Nothing is clobbered in either direction: the
  import is refused (ODB keeps its edit) *and* the shutdown export refuses to
  overwrite the `.ut`. Both refusals log errors naming the script
  (`ut-sync CONFLICT for <path>` / `ut-sync export CONFLICT for <path>`).
  There is no auto-merge. **To resolve**: make one side current (edit the
  `.ut` or the ODB so they agree), then delete that script's line from
  `.ut-sync-state` (or the whole file) — the next reconcile falls back to
  newest-wins and re-records the sync point.

Scripts with no recorded line (trees that predate the manifest) keep the
legacy last-write-wins behavior; state accrues from the first sync.

---

## Import-discovery scan (auto-create from dropped `.ut`)

Dropping a brand-new `.ut` file (one with no existing ODB node) into the sync
tree creates the node automatically:

- **At boot**: a scan runs after full hydration. Any `.ut` leaf with no
  in-memory ODB node gets its intermediate tables + leaf script created, marked
  dirty, and persisted on the next save.
- **On demand from protocol mode**: call the REPL verb `repl.syncscan()`.
  - The token is **all-lowercase `syncscan`** (prose sometimes writes
    `repl.syncScan`, but the live identifier is lowercase).
  - It works in **both** `--repl` and `--protocol` modes.
  - Returns the count of nodes created. Safe no-op (returns 0) when
    `--ut-sync-dir` is inactive.

This is how an agent reconciles the filesystem into the live runtime without
restarting: drop or generate `.ut` files, then
`{"op":"script/eval","id":N,"params":{"expression":"repl.syncscan()"}}`.

---

## Implications for reviewing UserTalk PRs

When reviewing a change that touches UserTalk:

- **Expect both artifacts to move together.** A script change should show up as
  *both* a `.root` binary diff and the matching `.ut` text diff. A `.ut`-only or
  `.root`-only change is a smell — confirm the author didn't hand-edit one side.
  (See `../AI_SHARED_GUIDELINES.md`: commit the `.ut` and the `.root` together.)
- **Read the `.ut` for the human-reviewable diff**, but remember it's a text
  projection — it can't be compiled or run on its own. Correctness lives in the
  runtime.
- **A new dotted-key node** should appear as a single `%XX`-encoded `.ut` file,
  not a nested directory tree. If you see literal `.`-split directories for a key
  that contains dots, the encoding path was bypassed.
- **Don't trust a green `.ut` text diff as proof the script behaves.** Import
  compile-checks (a non-compiling `.ut` is rejected, so it never silently
  lands in the ODB), but compiling is not running. Verify behavior via tests /
  protocol-mode calls.

---

## Known limitations / caveats

- **Future-dated `.ut` clobber (#697) — largely closed by the sync-state
  manifest.** Once a script has a recorded sync point in `.ut-sync-state`, a
  future-dated `.ut` can no longer silently overwrite an in-session ODB edit:
  unchanged-content re-dating is skipped (ODB wins), and a genuine two-sided
  edit is a refused, loudly-reported conflict (see "Two-sided conflict
  detection" above). The mtime-only clobber remains possible only for scripts
  with no recorded sync point (never exported or imported since the manifest
  shipped). Remaining hardening options (mtime clamp, logical clocks) stay
  tracked in #697. Tests age `.ut` files into the past to sidestep mtime
  ambiguity.
- **Deletions do NOT sync, and deleted nodes resurrect on next boot (#702).**
  Sync mirrors *creates* and *edits* in both directions, but **not deletions**.
  Deleting a script/table from the ODB does **not** remove the corresponding
  `.ut` file(s) from disk (the export walk is write-only), and the orphaned `.ut`
  is then **re-created in the ODB on the next boot** by the import-discovery scan.
  So deletion does not round-trip — and the resurrection bug means a delete
  silently comes back. Until #702 lands: to remove a node, delete it from the ODB
  *and* manually delete its `.ut` file (for a table, the whole subtree directory).
  Don't rely on either side propagating the deletion.
- **Export fires on clean shutdown, not `fileMenu.save()`** (see above).
- **Broken-`.ut` import is rejected with a logged error**, not installed (see
  the compile-gate note above). The rejection repeats on every import attempt
  until the `.ut` is fixed, so a stale broken `.ut` keeps announcing itself.
- **Verifier / corpus follow-ups (#700).** A structural-existence probe fix for
  the git-time verifier and re-export of a handful of RSS module-driver `.ut`
  files to the `%XX` scheme are tracked separately. Do not re-export the corpus
  or touch `verify_virgin_root_sync.py` as part of unrelated work.

---

## Quick protocol recipes

Install/replace a script (outbound sync writes the `.ut` on shutdown):

```
{"op":"script/eval","id":1,"params":{"expression":"local (s = string.trimWhiteSpace(file.readWholeFile(\"/tmp/x.txt\"))); script.newScriptObject(s, @workspace.miniApp.foo)"}}
{"op":"shutdown","id":2}
```

Reconcile dropped `.ut` files into the live runtime:

```
{"op":"script/eval","id":1,"params":{"expression":"repl.syncscan()"}}
```

Read a script body back without compiling (drift inspection):

```
{"op":"script/eval","id":1,"params":{"expression":"string(@workspace.miniApp.foo)"}}
```

---

## Where this fits

- Mechanism deep-dive + design rationale: `../ODB_UT_SYNC_DESIGN.md`
- How to install/edit scripts in the ODB: `../ODB_SCRIPT_EDITING.md`
- Protocol debugger with worked examples: `debugging_workflow.md`
- UserTalk language primer: `CLAUDE_PRIMER.md`
- Cross-agent ODB/`.ut` PR rules: `../AI_SHARED_GUIDELINES.md`
