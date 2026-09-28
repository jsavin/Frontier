# Preflight: frontier

Status
- State: Complete
- Date: 2026-09-27
- Source: `--source .` (this repository, `jsavin/Frontier` at `develop` `1e6377d4e`). No `legacy/frontier` link was created: by agreement (see `INTENT.md`), this repository is itself the system under analysis, and outputs go to `planning/modernization/frontier/` rather than `analysis/frontier/`.

## Answers

**1. Scope.** Is this code the complete system, or one slice of a larger codebase?

> Complete system

**2. Build & test locally.** Can this environment restore, build, and run the tests? Roughly how long does the full CI pipeline take?

> Yes, both suites run here

Duration was not given; see the Check 3 measurement.

**3. Bespoke build infrastructure.** Is there organization-specific build or dependency machinery a newcomer would not guess?

> The ones you listed [Bison parser gen, Virgin.root -> dist copy, CC=/usr/bin/clang SDK override, generated/ codegen]

Follow-up from JES (verbatim):

> On bespoke builds, see also how we handle setting up a "virgin" Frontier.root. That's important.

The Virgin.root pipeline is written up under Check 3a below.

**4. Prior attempts.** Has anyone tried to modernize this before? What went wrong?

> There is only one other modernization attempt which is very different from this one and started with implementing the UserTalk interpreter in node.js. That one (code-name Atlantis by Dave Winer) is completely separate from this.

**5. Off limits.** Is anything in this code not allowed to change in this pass?

> Nothing off limits

(The `archive/*` permanent branches must still never be merged to `develop`, per `CLAUDE.md`. That is a merge rule, not an edit freeze.)

## Scope boundary (Check 6)

**Standalone repository.** `git rev-parse --show-toplevel` is this directory; there is no enclosing monorepo, solution, or reactor. There are no inbound consumers in the source-code sense.

One caveat matters for the migration invariant in `INTENT.md`: **the real "consumers" of this system are legacy `.root` databases outside the repository**, meaning users' UserTalk applications. They depend on the kernel verb surface, UserTalk semantics, and the ODB format (v6 read and migrate, v7 native). No manifest or grep can enumerate them. The in-tree proxies are the shipped roots: `Frontier.root`/`Virgin.root`, mainResponder, Manila, and three Tools roots. `assess` and `extract-rules` must treat the kernel verb surface and the v6-to-v7 migration path as a public API with unknown external callers.

## Checks

| # | Check | Status | Found | Fix |
|---|-------|--------|-------|-----|
| 0 | Human answers | ✅ | All five answered, plus the Virgin.root note and the 2018 baseline fork | none |
| 1 | Stack | ✅ | C (624 files, 250k code lines) and headers (344 files, 22k). **UserTalk lives in binary `.root` ODBs**, with a 4,678-file `.ut` mirror. Also Python (18k lines, tooling and test runner), shell (6k), YAML integration tests (30k), Bison grammar, and some Objective-C (4 files). make-based. The HTML, CSS and JS are docserver and webserver assets. | none |
| 2 | Analysis tooling | ⚠️ | `scc` 3.7.0, `python3` 3.14.6, `cppcheck` 2.20, `clang-tidy` 21.1.8 present. `lizard`, `semgrep`, `flawfinder` missing. | `pipx install lizard semgrep flawfinder` (none blocks `assess`; `semgrep`/`flawfinder` widen `harden`) |
| 3a | Build definition | ⚠️ | CI covers only `frontier-cli/**` paths and unit tests; `Common/` and `databases/` changes are never CI-built; no integration tests in CI. Toolchain unpinned; SDK landmine; Bison 2.3. The Virgin.root pipeline is the bespoke machinery (see below). | Widen `boxen-ci.yml` path triggers to `Common/**`, `databases/**`, `tests/**` |
| 3b | Smoke test | ⚠️ | Full `make build` succeeds. **Unit gate is red on `develop`**: the datetime linter false-positives on #897 (`oppack_v7.c:193-196`). Past the linter: 1,007 unit tests pass; 2 suites crash (#857 intermittent, #827 deterministic). Integration: 2,313 pass, 17 baselined, 161 skipped, 1 fail (environmental: port 8080 held by a local ssh forward). | Fix or allowlist the linter on `offsetof` casts; the rest are known issues |
| 4 | Source completeness | ⚠️ | No missing includes. **Non-script ODB values have no source form.** The pristine 9.5 v6 roots are in git history (`5002c10ca`, MD5s match `PROVENANCE.md`); the doc does not say that the working copies are now v7 descendants. | Root-build-from-source Steps 2-7; add a HEAD-vs-history note to `PROVENANCE.md` |
| 5 | Optional context | ✅ | Full git history (2,370 commits), 198 open issues, plus the pre-modernization baseline at `/Users/jake/dev/tedchoward/Frontier`. No telemetry (none exists). | none |
| 6 | Scope boundary | ✅ | Standalone repository. The external consumers are legacy `.root` apps (see above). | none |
| 7 | Edit protection | ⚠️ | No `Edit` deny rules. Intentional: the live dev tree is the subject. | none (the analysis commands are read-only by instruction) |

## Check 3a: the build definition, and the Virgin.root pipeline

**CI:** `.github/workflows/boxen-ci.yml` runs on `macos-14`: `make -C frontier-cli`, then `./tools/run_headless_tests.sh`, then the `tb2-smoke` target. **It only triggers on `frontier-cli/**` paths**, so changes to `Common/` (the kernel, most of the C) and to `databases/` get no CI build or test at all. Integration tests never run in CI. The real gate is local: the `/auto` manifest in `CLAUDE.md` and `/gate`.

**Pinned toolchain:** nothing is pinned. `-std=c99` (tests) and `-std=c17` (CLI), with clang-specific flags (`-fpascal-strings`, Mach-O linker flags), so the build is macOS/clang only today. The known landmine: bare `clang` resolves to a nonexistent `MacOSX26.sdk`, so pass `CC=/usr/bin/clang` as a make argument. Bison here is **2.3** (Apple's system copy); the Bison 3 migration is Phase 5 planning. There are no third-party package feeds; `third_party/` is vendored.

**Codegen:** `generated/` holds `kernel_verbs_init.c`, `strings_tables.{c,h}` and `strings_manifest.json`, produced by `frontier-cli/Makefile` targets (`kernel_verbs_generated`, `strings_generated`). There is also a PSTRING/BIGSTRING length validator (`validate-bigstrings`).

**The Virgin.root pipeline (the part a newcomer would never guess).** UserTalk "source code" is not text in this repository. It lives inside binary ODB databases:

1. **Origin.** `databases/*.root` descend from the Frontier 9.5 distribution (UserLand, June 2005), migrated v6 to v7 (`databases/regenerate_v7.c`, `frontier-cli --migrate`). All shipped roots are now v7 (`0007` header).
2. **Source of truth.** `databases/Virgin.root` (18 MB, 20 commits of history) is the authoritative shipped system root. `databases/Frontier.root` is a local working copy that gets overwritten.
3. **Editing.** Edits happen by driving `frontier-cli --protocol` against Virgin.root (`tools/edit_virgin_root.sh`), installing scripts with `script.newScriptObject`/`op.newOutlineObject` so line endings are normalized. Each edit rewrites the whole 18 MB binary in git.
4. **Text mirror.** `usertalk_scripts/` holds 4,678 `.ut` files mirroring the ODB script tree (Virgin.root, mainResponder, Manila, config). It is kept in sync by `--ut-sync-dir` export on shutdown, with a compile-gated import and `.ut-sync-state` conflict refusal. `make verify-odb-sync` (`tools/verify_virgin_root_sync.sh`) compares **scripts only**; non-script values (tables, scalars, outlines, wptext, binaries, menubars) have no text form and no drift check.
5. **Distribution.** `make dist` copies `Virgin.root` to `dist/Frontier.root` plus the guest roots. `make clean-root` then runs `userland.cleanRoot()` via protocol against the dist copy (strips user state, keeps scripts). It also emits `dist/Virgin.root` as a side artifact.
6. **In flight: root-build-from-source** (`planning/ROOT_BUILD_FROM_SOURCE_PLAN.md`, Active). This makes `.root` files build artifacts generated from a git-native `sources/` tree (`.ut` scripts plus `#table.utv` value manifests and typed sibling files, UTF-8 in git and MacRoman in the root). Step 0 (census) is done; Step 1 (`--diff-roots` equality walker) merged as #897 today. Steps 2-8 remain: exporter, assembler, full type surface, repair-in-text, dual-source, flip, retire ut-sync. `sources/` does not exist yet.

**Why this matters for modernization.** The migration invariant ("legacy `.root` applications migrate cleanly and run") is tested today only against roots that were themselves hand-maintained by binary edits. This root-build plan is the mechanism that makes the shipped roots reviewable, diffable and reproducible. It is also the natural harness for proving that *any* legacy root round-trips. `assess` should weigh it as foundational infrastructure rather than as a side project.

**Provenance (corrected 2026-09-27).** The pristine Frontier 9.5 v6 roots (32-bit) **are in the repository's history**: `databases/Frontier.root`, `mainResponder.root`, `manila.root` and the other guest roots were first committed in `5002c10ca` (2025-08-09) with `0006` headers, and those blobs' MD5s match `databases/PROVENANCE.md` exactly (verified with `git show 5002c10ca:<path> | md5`). The HEAD working copies are v7 descendants (`0007`), so their MD5s differ by design. `PROVENANCE.md` only needs a line saying its checksums describe the first-commit blobs, not HEAD, and that `Virgin.root` is a derived working root. Any legacy-baseline comparison, including #868, can use `git show 5002c10ca:<path>` and needs nothing from the `.dmg`.

*Correction note: an earlier version of this report said the v6 originals were not in the repository. That was wrong: it compared HEAD MD5s without checking history.*

**Test-run side effect (open item).** After the test runs in this preflight, `databases/Frontier.root` had a fresh mtime (2026-09-27 23:18) and read-only mode (`r--r--r--`). Its **content is byte-identical to HEAD** (`git diff --quiet HEAD -- databases` is clean), so no data changed. But some step in `run_headless_tests.sh`, `make -C tests test` or the integration runner touches, rewrites or chmods the canonical `databases/` copy in place, which PR #555's staging isolation is meant to prevent. The responsible step is not yet identified.

## Check 3b: smoke test on this code

Run on this Mac, `develop` at `1e6377d4e`, 2026-09-27, `CC=/usr/bin/clang`.

**Level 2 (full build):** `make build` succeeded in 8 s (incremental), with 77 compiler warnings in the log, mostly typedef redefinitions between `headless_stubs.h` and `osincludes_portable.h`.

**Unit (`./tools/run_headless_tests.sh`): ❌ red on `develop`, before any test runs.** The pre-test linter `tools/check_datetime_types.sh` flags `Common/source/oppack_v7.c:193-196`, lines added by **#897 (`--diff-roots`, merged 2026-09-27)**. It is a false positive: `(long) offsetof(typortablediskheader, timecreated)` is a struct offset, not a stored timestamp. CI missed it because `boxen-ci.yml` does not trigger on `Common/**`. Every local unit run, `/auto` push gate and `/gate` fails until this is fixed.

Running `make -C tests test` directly, past the linter, takes 133 s:

- 48 suites, 503/503 pass, until `test_window_bridge` segfaults (SIGSEGV 139). That is **#857** (intermittent init-phase SIGSEGV, about 66% of runs; 3/3 standalone reruns passed). The crash **aborts the make recipe**, so every suite after it silently does not run.
- The 19 suites after it, run individually: 18 pass (504 tests, including #897's `diff_roots_tests` 20/20). `boxen_outline_tests` segfaults deterministically: **#827** (8.9 MB stack-local struct).
- **Total: 1,007 unit tests pass; 2 suites crash, both tracked.** The runner's fail-fast behavior means a crash hides the results of later suites.

**Integration (`cd tests && make test-integration`):** 128 s, 8 workers.

- Total 2,492 = 2,313 passed + 161 skipped + 17 known-fail (baselined) + **1 failed**.
- The failure, `inetd.startOne - start HTTP listener on port 8080`, is **environmental**: port 8080 on this Mac is held by an `ssh` port-forward. Not a code defect, but it does show that the suite hard-codes a common port.
- The baseline contains an entry with no tracking issue (`html.getpref - page table override`), and one baselined entry was skipped this run (`tcp.readStream - zero byte count`); the runner itself asks for both to be rechecked.
- The 161 skips and 17 baselined failures are what goal criterion 6 ("no skips or baseline entries hiding the path") must drive toward zero.

No target stack is set (goal: understand), so there was no throwaway target-stack build.

**Baseline for "before":** `/Users/jake/dev/tedchoward/Frontier` is the fork's pre-modernization starting point (last commit `ea71529`, 2018-05-01, "Removes `TARGET_API_MAC_OSX` `#ifdef`s"): about 153k lines of C, 42k of headers, 27k of MSBuild (Windows project files). `assess` can diff against it to measure what the modernization has changed.

## Check 4: source completeness

- **Missing includes:** none. The full build compiles and links (`make build`), so every header resolves.
- **Deployment/config descriptors:** `databases/frontierStartupCommands.txt`, `StartupTasks.root`, `Guest Databases/www/prefs.root`, and the `config.root` mirror in `usertalk_scripts/`. The webserver and responder configuration is data inside the roots (`user.webserver.responders.*`), not files.
- **Data definitions:** the ODB format is defined in C (`Common/source/oppack_v7.c`, `db*.c`, `hashpack.c`) and documented in `docs/` and `planning/phase2`. There is no DDL.
- **Binary-only artifacts:** (a) **all UserTalk code is primarily binary** (inside `.root` files); the `.ut` mirror covers scripts only, and non-script values have no source form (see 3a). (b) `databases/DLLs/` (`odbc.dll`, `regexcarbon.dll`: Windows, historical, unused headless). (c) `Extras/Altivec Support` (PowerPC binary). (d) `Appearance/Icons` (resource-fork data, 0 bytes in git). About 629 `.root`/binary files exist outside `tests/tmp`, most of them test fixtures.

## Check 5: optional context

- **Telemetry:** none. There is no production deployment and no APM MCP connected. Runtime evidence comes from the integration suite, the protocol debugger, and logs.
- **Version control:** full git history (2,370 commits on `develop`) plus the GitHub issue tracker (198 open issues). There is enough change-frequency signal for risk ranking.

## Check 7: is the source protected from edits?

⚠️ **No `Edit` deny rules** in `.claude/settings.json`, `.claude/settings.local.json`, or `~/.claude/settings.json`. This is **expected and correct here**: the source is the live `develop` checkout of an in-place modernization, not a frozen legacy copy, so a deny rule would block normal development. The analysis commands in this run are read-only by instruction, and all output goes to `planning/modernization/frontier/`. The plugin's usual `Edit(/legacy/**)` snippet does not apply, since there is no `legacy/` link. A permission rule only covers Claude's file tools and the shell commands it recognizes, not scripts that open files themselves; the hard guarantee would be a read-only mount.

## Readiness by command

| Command | Verdict | Why |
|---------|---------|-----|
| `assess` | **Ready** | `scc` and `python3` present; full history; `lizard` only matters for `--portfolio`. |
| `map` | **Ready** | Complete source, no missing includes, `compile_commands.json` present. **Caveat:** UserTalk call graphs live in binary roots, so map UserTalk from the `.ut` mirror (scripts only). |
| `extract-rules` | **Ready-with-gaps** | Kernel C is fully readable. UserTalk rules come from the `.ut` mirror; non-script ODB values (tables, prefs, outlines) are opaque until root-build Step 2 (the exporter) lands. |
| `harden` | **Ready-with-gaps** | `cppcheck` and `clang-tidy` present; `semgrep`/`flawfinder` missing (install for wider coverage). |
| `brief` | **Ready** | Needs only discovery artifacts. |
| `transform` / `reimagine` / `uplift` | **n/a for now** | Goal is "understand"; the road is decided after `brief`. For any code-changing road, the red unit gate (datetime linter) and the fail-fast runner must be fixed first. |

**Single most important fix:** the unit gate on `develop` is red because the datetime linter false-positives on #897's `offsetof` casts in `oppack_v7.c`. Every test gate fails locally until it's fixed, and the fact that CI skips `Common/**` is how it got merged.

**Next step:** `/code-modernization:modernize-assess frontier`
