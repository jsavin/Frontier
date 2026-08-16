# Root-Build-From-Source Implementation Plan

Status
- State: Active (decisions resolved: 9.4 preserve-dates, 9.6 sources/, 9.7 UTF-8-migration [2026-08-15, maintainer]; 9.1 colocated / 9.2 name:type=value / 9.3 RTF-when-styled / 9.5 short-transition adopted per recommendation without objection [reversible]. Step 1 dispatched 2026-08-15.)
- Phase: Cross-cutting infrastructure (post-Phase-3 wave)
- Last Updated: 2026-08-11
- Notes: Maintainer directive 2026-08-11: get out of diff-against-binary; make .root files generateable from sources. This doc is the granular landing plan. Census (in flight) and provenance results refine scope numbers, not the design.

Related Docs
- planning/_CURRENT_GOAL.md
- docs/usertalk/UT_SYNC_WORKFLOW.md (the layer this plan retires)
- docs/ODB_SCRIPT_EDITING.md
- planning/phase3/SESSION_REPORT_2026-08-10_webserver_milestones.md

Change Log
- 2026-08-11: Initial draft.

## 1. Goal and non-goals

**Goal:** `databases/*.root` become BUILD ARTIFACTS, deterministically generated from a git-native source tree: UserTalk code as text files in a hierarchy matching the ODB structure they land in, non-textual values as discrete typed files. Git diffs/blame/review/merge operate on sources. The monolithic binary leaves the review path entirely.

**Non-goals:** runtime databases (user data) are NOT source-managed — this plan covers only SHIPPED roots (Virgin.root, and per Section 8, the guest DBs). No change to the v7 on-disk format. No change to how the runtime uses ODBs.

**Why now:** the import direction was the dangerous half of ut-sync, and its defect classes are now understood and fixed or fenced: #866 (install scanner corruption — fixed), #881 (LF import corruption — mechanism known), #880 (UTF-8 mangling — trap documented), #878 (round-trip corpus — becomes this plan's acceptance suite). A build-time import with per-script compile gates is a controlled context, unlike live bidirectional sync.

## 2. Current state (what exists to build on)

- `usertalk_scripts/<Root>/path/to/script.ut` — text tree for SCRIPTS only, hierarchy already matches ODB paths. Name/charset handling exists (ut_sync pctcodec + canonicalize modules, each with unit tests).
- ut-sync machinery: export-on-shutdown (Direction A — keep, becomes the migration exporter's basis), import-on-materialize (Direction B — retire), `.ut-sync-state` conflict tracking (retire).
- `verify_virgin_root_sync.sh` — script-only ODB↔text comparison (retire in favor of the round-trip equality walker, Section 5).
- Known gaps this plan absorbs: #861 (ODB scripts with no export — e.g. `responders.default.methods.any`), #868 (mirror/binary divergence — mooted once text is truth), #869 (no guest-DB verification), #878 (65 non-idempotent round-trips), #854-adjacent repo churn (+1.2MB per binary save).

## 3. Source tree layout

```
sources/
  Frontier.root/                    <- one directory per shipped root
    #root.yaml                      <- root-level metadata (format version, build options)
    system/
      #table.utv                    <- this table's non-table, non-script values (Section 3.2)
      verbs/
        builtins/
          inetd/
            #table.utv
            startOne.ut             <- script (existing .ut format, unchanged)
            supervisor.ut
    user/
      webserver/
        #table.utv
        responders/
          default/
            #table.utv              <- scalar fields: condition, enabled, flPreFilter...
            methods/
              any.ut                <- closes the #861 gap by construction
    assets/                          <- DECISION 9.1: colocated vs separate (see below)
```

Rules:
- **Directory = table.** Nesting mirrors ODB paths exactly. Name encoding via the existing pctcodec (handles path-hostile chars); case preserved as stored (lookups are case-insensitive, storage is case-preserving — the tree stores the canonical spelling).
- **`name.ut` = script**, current format. Line endings LF in git, normalized to CR at install (existing newScriptObject behavior, now trustworthy post-#866).
- **`#table.utv` = the table's scalar/simple values**, one file per table (Section 3.2). `#`-prefix keeps metadata files sorted first and impossible to collide with ODB names (pctcodec escapes a genuine leading `#`).
- **Binary-typed values = sibling files with typed extensions** (Section 3.3), referenced from `#table.utv` by entry.

### 3.2 `#table.utv` — the value manifest (DECISION 9.2 on exact syntax)

One line per non-script, non-table entry, in UserTalk literal syntax with an explicit type. Proposed shape (readable, greppable, single-line-per-value => line-diffs are value-diffs):

```
enabled        : boolean  = true
port           : long     = 8080
condition      : string   = "(string.lower (path) beginsWith \"mainResponder\")"
lastBuildDate  : date     = "Mon, 10 Aug 2026 19:55:00 GMT"
adrFilter      : address  = @system.verbs.builtins.webserver.dispatch
ftpText        : binary   = file:ftpText.bin  ('TEXT')
logo           : picture  = file:logo.pict
docs           : wptext   = file:docs.rtf     <- or docs.txt; DECISION 9.3
menu           : menubar  = file:menu.outline
prefsOutline   : outline  = file:prefsOutline.outline
modified       : meta     = "..."             <- per-value mod-date policy, DECISION 9.4
```

Parsing is a small dedicated reader in the build tool — NOT the interpreter (no eval anywhere in the build; the #859 lesson generalized). Strings are the only quoted/escaped form; escapes limited to \" \\ \xNN. All file I/O byte-preserving; the manifest is ASCII-with-\xNN so MacRoman bytes survive any editor (the #880 lesson).

### 3.3 Non-textual values

| ODB type | Source form | Notes |
|---|---|---|
| binary (arbitrary, typed) | `name.bin` + 4-char type in manifest | byte-exact |
| picture | `name.pict` | byte-exact PICT payload |
| wptext | DECISION 9.3: `.rtf` if styled, `.txt` if plain | v7 keeps styles only inside stored RTF, per format rules |
| outline / menubar | tab-indented text (`.outline`) | Frontier-native projection; dirline flags in a header line if needed |
| filespec / alias | manifest literal (path string) | legacy aliases that can't resolve become build errors, not silent drops |
| everything scalar | manifest literal | string, long, short, boolean, char, double, date, address, direction, rgb, point, rect, 4-char types |

**Loud-fail rule:** any value whose type the assembler cannot represent FAILS THE BUILD with the full ODB path. Nothing is ever silently dropped — "lost artifacts" become red builds by construction.

## 4. The three tools (all frontier-cli modes; C, no interpreter on any path)

1. **`--export-root <root> <srcdir>`** (disassembler): walks the ODB, emits the source tree. Deterministic order (sorted by canonical name), byte-preserving, one file per rule above. Used once per root for migration, and forever after in the equality check. Builds on ut_sync export internals (canonicalize/pctcodec) but is a one-shot tool, not a shutdown hook.
2. **`--build-root <srcdir> <root>`** (assembler): creates a fresh v7 root, walks the tree depth-first sorted, creates tables, installs scripts via the newScriptObject path with a PER-SCRIPT COMPILE GATE (install→read-back→compile; any failure = build error naming the script), inserts manifest values, imports assets. Output timestamps: deterministic (DECISION 9.4). Two runs from the same tree produce semantically identical roots (bit-identical is NOT required — block layout may vary; semantic identity is what the walker checks).
3. **`--diff-roots <a> <b>`** (equality walker): recursive value-level comparison — every path, type, and payload bytes; scripts compared as stored text; reports every difference with full paths; exit 0 only on empty diff. This is the acceptance gate and the CI check.

## 5. Acceptance harness

- **Round-trip law:** `diff-roots(build(export(R)), R) == empty` for every shipped root. The #878 corpus (65 known non-idempotent scripts) is the seed regression list; each must round-trip byte-stable or have its normalization divergence fixed/documented.
- **Idempotence law:** `export(build(T)) == T` (tree-level diff empty) — proves the tree is canonical form.
- **Legacy cross-check:** run diff-roots between our built roots and the Frontier 9.5 package's mainResponder.root/Manila.root (#868 baseline) — differences become an explicit, reviewed delta list rather than unknown drift.
- **Suite invariant:** full integration suite green against BUILT roots before any flip step.

## 6. Migration sequence (granular, each step = one gated unit)

**Step 0 (running now):** census — type histogram + non-textual inventory of all three shipped roots. Output sizes Steps 3-4 and validates Section 3.3's table covers reality.

**Step 1 — equality walker first.** Build `--diff-roots` alone and prove it: diff a root against itself (empty), against a copy with one known mutation (exactly that finding). The walker precedes everything because every later step is verified with it. (~1 unit)

**Step 2 — exporter.** `--export-root` for the full type surface the census found. Acceptance: export Virgin.root twice → identical trees; spot-verify scripts match existing .ut mirror where the mirror is intact (divergences recorded — they're #868-class data, not blockers). (~1-2 units)

**Step 3 — assembler MVP: scripts + tables + scalars.** Build a SUBSET root (e.g. system.verbs.builtins.inetd) from exported sources; walker-verify against the original subtree. Per-script compile gate mandatory from day one. (~1-2 units)

**Step 4 — full type surface.** Assets, wptext, outlines, menubars, filespecs. Acceptance: full Virgin.root round-trip law passes. This is the hard gate of the whole plan. (~2-3 units, census-dependent)

**Step 5 — repair-in-text.** The source tree becomes where #879/#881-class damage gets FIXED (broken scripts repaired as text, reviewed as text diffs — including openURL/isValidUrl if not already repaired by then, cleanRoot, the LF class). Provenance results feed this. Build, walker-verify, suite green. First root ever built better than its binary ancestor.

**Step 6 — dual-source transition.** CI (or make) builds Virgin.root from sources on every relevant change; the committed binary stays temporarily with a drift check (build-and-diff-roots against committed — red on divergence). All ODB edits now happen in SOURCES; protocol-mode editing of shipped roots is demoted to experiments. DECISION 9.5 sets the transition length.

**Step 7 — flip.** Committed `databases/Virgin.root` removed from git (built into `dist/` and a gitignored `databases/` slot at build time). Guest DBs follow the same Steps 2-7 path, seeded per the #868 decision (our binary vs 9.5 baseline vs mirror-carried fixes — explicit reviewed choice per divergence).

**Step 8 — retire the bootstrap.** Delete ut-sync Direction B (import-on-materialize), `.ut-sync-state` conflict machinery, `--ut-sync-dir` shutdown export, verify_virgin_root_sync.sh. Update ODB_SCRIPT_EDITING.md, UT_SYNC_WORKFLOW.md (tombstone), CLAUDE.md workflow rules ("edit Virgin.root via protocol" becomes "edit sources; build"). Close/moot: #861, #868, #869; #878 becomes the round-trip regression list; #854's repo-churn half dissolves (compaction bug remains for runtime DBs).

## 7. Testing / tooling notes

- All three tools get C unit tests (walker: known-diff fixtures; assembler: each type; exporter: each type) + integration yamls where behavior spans a boot.
- Every step's unit runs the full suite before merge (existing gate discipline).
- Encoding invariant enforced mechanically: a build-time check that no source file under `sources/` is valid-UTF-8-with-multibyte-sequences where MacRoman was intended is impossible to state generally — instead the manifest format is ASCII+\xNN by construction, .ut files keep the existing convention, and the round-trip law catches any mangling (#880 class) structurally.
- The runner.py UTF-8 write trap (#880) gets fixed before Step 3 (assembler tests will hit it otherwise).

## 8. Scope order across roots

1. Virgin.root (system root; source of truth; most value).
2. mainResponder.root + Manila.root (guest DBs; gated on the #868 baseline decision; the 9.5 package supplies the legacy-virgin reference).
3. Any other shipped guest DBs found by the census.

## 9. DECISIONS (maintainer)

1. **Asset placement:** colocated (`inetd/ftpText.bin` next to its manifest — my recommendation: locality wins) vs a parallel `assets/` tree.
2. **Manifest syntax:** the `name : type = value` line format above (my recommendation) vs pure UserTalk record literal blocks vs OPML. Criteria: line-diff = value-diff; no interpreter needed; greppable.
3. **wptext form:** RTF file when styled, plain .txt when not (my recommendation) vs always-RTF.
4. **Per-value modification dates:** DECIDED 2026-08-15 (maintainer): PRESERVE in manifest. Keeps UserLand-era provenance (per-VALUE mod/create dates back to the 1990s; `timeModified(@adr)`) and makes builds fully deterministic with no build-time clock. Git blame carries OUR history; the manifest carries UserLand's.
5. **Transition length (Step 6):** one wave of dual-source with drift check (my recommendation — short; the walker makes long overlap pointless) vs longer.
6. **Tree root name:** DECIDED 2026-08-11 (maintainer): `sources/`. Existing `usertalk_scripts/` history carries over via `git mv` during Step 2.
7. **Codepage:** DECIDED 2026-08-15 (maintainer): UTF-8 MIGRATION, as its own gated unit. `sources/` is UTF-8; the assembler converts to MacRoman on the way into the root; the exporter converts on the way out. Makes UserTalk source natively editable by humans and agents and permanently kills the #880 mangling class. Treated as a first-class unit with its own round-trip proof (byte-exact MacRoman round-trip through UTF-8 for the full corpus, including the 0xC7 comment marker, guillemets, and every high-bit byte the census found) and its own gate. Sequencing: the conversion layer must exist and be proven BEFORE Step 2 (exporter) emits a tree, so the tree is born UTF-8 rather than converted later.

## 9.5 Workflow contract after the flip (added 2026-08-11)

Live editing in a running Frontier remains the development/debugging surface (protocol debugger substrate). Promoting a live change to shipped truth means expressing it in sources/ and rebuilding: the live root is a scratchpad, never a source. This is a ONE-WAY VALVE by design; any tooling that captures live edits back into sources/ must be an explicit, reviewed export action, never an automatic sync (that would be the retired UT-sync layer reinventing itself).

## 10. Risks

- **Round-trip completeness** is the schedule risk — unknown/legacy value types in 25-year-old roots (census will surface them; loud-fail contains them).
- **The assembler inherits install-path bugs**: mitigated by per-script compile gates + the #866 fix + the walker; any new normalization divergence fails the round-trip law loudly.
- **Two sources of truth during Step 6**: contained by the drift check being red-on-divergence, and by demoting protocol edits of shipped roots to experiments immediately at Step 6 entry.
- **Guest-DB seeding** embeds the #868 decision — explicitly a maintainer call per divergence, never a default.
