# Frontier 1.0 Research Report

**Date:** 2026-08-09
**Purpose:** Consolidated findings from five research streams (repo/planning state, Manila/web stack, boxen/CLI, UserLand history, modern landscape) to ground a product-direction interview and a feasibility read on "Frontier and Manila running again."

---

## Verdict up front

**Yes — substantial progress toward Frontier and Manila running again is clearly achievable, and the remaining work is smaller than the work already done.** Frontier-the-runtime is essentially revived: it boots, migrates v6→v7, runs the full startup script, and ships a boxen TUI with REPL, outline editor, and a working UserTalk debugger. Manila's entire source survives in this repo (and only this repo), the mainResponder dispatcher already ran headlessly on first try, and the HTTP pipeline worked end-to-end in March 2026. What stands between today and "Manila serves a page" is a bounded list: re-verify the web stack (issue #620 triage), fix two known accept-callback kernel bugs, run Phase C integration, then Phase D installation. The genuinely hard residual risks are concurrency under load (GIL-era seams) and data-loss bugs — not missing functionality.

The strategic picture changed two days ago: **Dave Winer now has his own Node.js port of UserTalk + the ODB (Aug 7), is planning a UI for it, and is publicly tracking this project by name.** He has also said explicitly that what he wants is to *run Manila on his own hardware* — which his port does not have and this repo uniquely does. Positioning the two efforts deliberately (complementary vs. overlapping) is now a 1.0-shaping decision.

---

## 1. Where the project actually is

### Working today
- **frontier-cli** on macOS (arm64/x86_64): boxen REPL (default since June 26), `--plain` legacy REPL, `--protocol` NDJSON machine interface, headless script execution, v6→v7 migration, ODB↔.ut bidirectional sync, WebSocket server, browser-agent integration.
- **boxen**: portable terminal window-manager library (termbox2 backend, swappable vtable), built for eventual extraction as standalone open source. Debugger TUI shipped (breakpoints, stepping, watchpoints), outline editor windows shipped (read-only), input_decoder at M5/M7 (Kitty keyboard, SGR mouse, bracketed paste).
- **Verb coverage**: 68% (482/710) as of April — all core processors at 100% (file, db, lang, op, string, table, xml, date, sys, html, script, dialog), TCP 23/23. ~170 of the remainder are GUI-dependent; realistic headless ceiling ≈ 60–70%... meaning headless verb work is nearly done.
- **Tests**: ~750 unit + ~2,300–2,400 integration cases, 8-way parallel in ~37s.
- **Velocity**: ~90 commits/month, steady. 2,338 commits on develop back to the 2004 CVS import.

### Parked / at risk
- **The web stack is unverified.** All 22–25 webserver/inetd/mainResponder tests are `skip: true` under umbrella issue #620: PR #618 removed an eval-trap that had been reporting failing evals as `success: true`. Whether the March HTTP pipeline still works is unknown — the assertions that said it did were broken. 47 tests total skipped under #620 (also menu_data, migration_externals, op_verbs).
- **The green baseline is not trustworthy**: "0 failures" is achieved partly by skipping; the current accepted baseline is "2186 passing / 21 known failures," and the baseline file lives in /tmp (unreproducible from a fresh clone).
- **Two real kernel bugs** in the accept-callback path: (a) server-side `writeStream` in the accept callback fires before the accepted stream is writable; (b) server closes stream before client polls, with no stream-event notification.
- **Data-loss trio**: #264 (db.close+db.open segfault), #270 (no duplicate-open guard), #271 (TOCTOU race in auto-migration).
- **Manila E2E plan stopped at Phase B** (of A–F). Phases A and B are merged (#541, #543) — but the plan doc's status table says otherwise and would misdirect a resuming session. Phase B's good news: the ~1000-line legacy mainResponder dispatcher worked headlessly *without any stubs or workarounds*.

### The quiet re-scope
The last two months of work (29 commits) are entirely boxen/REPL/TUI. The newest planning docs never mention Manila, mainResponder, or the native GUI — the March north star ("Startup Flow Stabilization: first run → mainResponder installs → Manila installs → HTTP server starts → browser opens setupFrontier") was parked by substitution, never formally deferred. Nothing states whether boxen replaces or precedes the planned native/protocol GUI client (~8,500 lines of draft specs, JSON-RPC over WebSocket, never started).

### Architecture facts that shape 1.0
- **ADR-014 GIL** is a deliberate deferral of thread-safety, not a solution: one UserTalk instruction at a time, forever, until the global-state work lands. **#332 ODB handle refcounting** is the gate on ever removing it. A live "split-brain" exists (`currenthashtable` thread-local, `hashtablestack` global) that already caused one hang (#706).
- **ADR-017** (filesystem-canonical ODB sources; `.root` as build artifact) is the newest open strategic decision — motivated by real, observed ODB↔.ut drift including a Virgin.root startup script that fails to compile.
- **ADR-012**: calling UserTalk from background pthreads segfaults; only a TCP-specific dispatch queue exists. Any new background-thread→UserTalk path re-hits this.
- **#88** (networking architecture & HTTP security model) is explicitly gated "before broad CLI distribution" — the TCP layer is solid but an HTTP-level security policy model does not exist. Given Jake's #1 priority (security by design), this is on the 1.0 critical path for anything server-shaped.
- **No 1.0 definition exists.** Last tag v1.0.0-alpha.7 (Feb 16). The only "1.0" ever written down is in the abandoned January Kickstarter doc. P0b "LAUNCH READY" is unstarted.
- **Six heavily-referenced planning docs actively misinform** (_CURRENT_STATUS, _CURRENT_TODO_LIST, INDEX, phase4/PROGRESS, p0a README, ADR README): they claim P0a unstarted (it's largely done), TCP at 13/22 (it's 23/23), ADRs stop at 011 (they go to 017), next step is native GUI (it isn't). For a project that runs on agents reading these files, a status-refresh pass is arguably launch-blocking.

---

## 2. Shortest credible path to "Manila serves a page"

1. **Triage #620** — un-skip webserver/inetd/mainResponder tests; learn what actually still works. Highest-leverage single move in the repo.
2. **Fix the two accept-callback kernel bugs** (writeStream-before-writable; close-before-poll / stream event notification).
3. **Phase C** — inetd + mainResponder integration test.
4. **Phase D** — Manila installation headless + first served page (human-in-loop; never attempted).
5. **Startup stabilization** (phase4 plan) — first-run flow: `userland.firstRootRun()` → install mainResponder.root + manila.root → `inetd.isDaemonRunning` → browser opens setupFrontier. Known risks already enumerated (betty.init, pikeRenderer.init, wp.newTextObject headless).
6. **Phase E** — concurrent-load safety audit (`ab -c 10 -n 1000`) — the GIL-era integration seams are the real unknown, and the plan correctly marks this human-required.

The E2E plan's own framing is right: the unknowns are not in the verb layer (mature) but in integration seams — startup sequencing, responder wiring, GIL behavior under concurrent HTTP load, guest-DB context guards. And its warning has already come true once: "Green CI ≠ working system... autonomous workflow produces a false sense of done" — which is exactly what the eval-trap did.

---

## 3. What Manila was (and why it still matters)

Released Nov 1999 with Frontier 6.1 ($899); "Content Management for the Rest of Us." Distinctive then, still distinctive now:

- **"Edit This Page"** — editing in situ, identity-based: the URL you read is the URL you edit. No separate admin dashboard.
- **Everything in one object database** — content, templates, membership, discussion, config; no schema, no SQL, applications ship as `.root` databases.
- **The external API exposed everything** — templates and site structure, not just posts; a level of remote control most modern CMSes still don't offer.
- **Editorial workflow + membership + discussion built in**; calendar navigation; shortcuts for link management; automatic tag balancing.
- EditThisPage.com launched free hosting days after release; Daily Kos, Joel Spolsky, Robert Scoble, and Doc Searls started there — before Movable Type, Tumblr, WordPress.
- Died by slow wind-down (~2004), not by being out-competed on the concept.

**Frontier's protocol legacy**: XML-RPC (1998, born of Frontier's cross-platform need), SOAP co-authorship, RSS 0.91→2.0, RSS enclosures (basis of podcasting), OPML. Interop is this platform's birthright.

**The original documentation is still live** — frontier.userland.com, docserver.userland.com, manila.userland.com all respond over plain HTTP in 2026 (no HTTPS, which is why tooling appears to fail). Manila User's Guide, DocServer verb reference, mainResponder docs, Dr. Matt's website-framework material. **One hosting lapse from vanishing; mirroring them locally is cheap insurance worth doing soon.** (A Manila User's Guide PDF is already in databases/.)

**Community**: no dedicated forum/Discord exists. The conversation is scripting.com, Dave's Mastodon, jakesav.in, GitHub issues, and adjacent old-timers (Oliver Wrede's May 2026 piece floated Frontier as an AI-agent coordination platform).

---

## 4. The landscape a 1.0 launches into

### The Dave Winer development (Aug 7, 2026)
- Claude Code ported UserTalk + the ODB + verb set to Node.js in ~2 weeks; his acceptance test was running his real UserTalk build scripts "flawlessly." He is planning a UI for it. He is actively exporting data from his live Frontier ODBs (July: ODB→JSON exporter).
- He names this project: "BTW, Jake Savin is using Claude to build Frontier on Node too." (His phrasing — the C revival isn't Node, but he's watching.)
- His stated desire (May): "I especially want to run Manila on one of my home computers, and use it for Linux server apps."
- His current stack: RSS.chat (primary focus), Drummer/Electric Drummer, FeedLand, WordLand, OldSchool; scripting.com is still generated by Frontier-derived infrastructure. He has pivoted away from WordPress alignment (dislikes Automattic's ATProto bet).
- His mission framing: textcasting.org; "a dozen great editors"; interop between people before interop between apps.

**Read**: two revivals now exist. His is the language/runtime on Node; this one is the full system — kernel fidelity, ODB on-disk format, debugger, and uniquely Manila + mainResponder + the webserver. The natural resolution is complementarity (he runs what this project ships, especially Manila; the projects share formats and protocols), but that should be chosen deliberately, and probably discussed with him directly, rather than left to drift.

### Market gaps (ranked by vacancy)
1. **AI-scriptable personal server with a real object database** — nearly vacant. Val Town is the closest analogue and publicly admits PMF trouble. Everyone in "agentic CMS" (Sanity, Storyblok, WP plugins) is retrofitting agent interfaces onto content stores; Frontier already *is* a uniform object DB with a scriptable verb namespace and a working debugger an agent can drive.
2. **Outliner-native publishing** — Logseq/Roam/Tana/Obsidian all treat publishing as an export afterthought. "The outline IS the site" is Frontier/Manila/Drummer's native model and essentially nobody else's.
3. **The editor middle ground** — between a tweet box and Gutenberg there's almost nothing; Dave has campaigned on this for years.
4. **RSS ↔ ATProto bridging** — ATProto is where the energy is (IETF WG chartered 2026; standard.site lexicons are "RSS items with different names"); Dave is bridging by hand this week and asking for help.
5. **Server-side scripting for non-professionals** — Glitch's death left it empty; self-hosting grew 45% (2023–26) but remains sysadmin work.
6. **WordPress's governance vacuum** — ~36%→33% and falling (HTTP Archive), trust damage from the WP Engine fight. "I want out of WordPress but SSGs are too much" is a real segment.
7. **Integration itself as the product** — scripting + DB + server + outliner in one box is what made Frontier remarkable; in 2026 those are four tools and a build pipeline.

### Expectations a 2026 launch meets
- Federation is not mandatory, but **interop is**: RSS, OPML, WebSub at minimum; ActivityPub and/or an ATProto bridge as differentiators. RSS itself is resurging (algorithm fatigue).
- Minimalist writer-first platforms (Bear ~20k blogs, micro.blog, Pika) are the growth story — "writers want fewer features, not more."
- Self-host unit of account: a $4–6/mo VPS (DigitalOcean/Hetzner) or a home machine; Docker-compose is the lingua franca. macOS-only is a non-starter for the server story — **Linux is on the 1.0 critical path** for anything Manila-shaped.

---

## 5. Feasibility summary

| Goal | Distance | Blockers |
|---|---|---|
| Frontier (authoring env) running | **Done in substance** | Polish: outline editor still read-only; test-baseline trust; data-loss trio |
| HTTP server verified again | Short (likely days–weeks) | #620 triage; 2 accept-callback kernel bugs |
| Manila installed + serving | Medium (weeks of focused work) | Phases C/D; startup stabilization; betty/pikeRenderer init |
| Manila under real concurrent load | The true unknown | GIL seams, guest-DB context guards, mode-stack under yields (Phase E, human-led) |
| Manila on a Linux VPS | Not started | No Linux build/CI for the runtime (boxen CI has ubuntu; runtime doesn't); `-fpascal-strings`/Mach-O flags are macOS-isms |
| 1.0 | Undefined | No release criteria exist; P0b unstarted; security model #88 required before broad distribution |

**Honest risk list**: (1) integration-seam unknowns under load — the GIL makes it safe-but-serial, and nobody has load-tested; (2) the false-green pattern recurring — #620 proved CI can lie, and the project leans on autonomous agents; (3) stale planning docs actively misdirecting those agents; (4) Linux port unknowns for the runtime; (5) positioning drift vs. Dave's Node port; (6) solo-maintainer bus factor on a ~500k-line C codebase (mitigated by unusually good docs/tests/AI-legibility).

None of these look fatal. The project's own materials underestimate how far along it is.

---

## 6. Open questions for the product interview

1. **Center of gravity for 1.0**: authoring environment (boxen/CLI), Manila-on-a-VPS (publishing server), AI-scriptable personal server, or the integrated box?
2. **Relationship to Dave's Node port**: coordinate/complement/ignore? Talk to him before scoping 1.0?
3. **Who is the first user** of 1.0 — you, Dave, old Frontier hands, indie-web writers, AI-agent developers?
4. **Manila fidelity vs. reimagining**: pixel-faithful revival first, or modernize (auth, HTTPS, Markdown, themes) on the way up?
5. **Platform sequencing**: how early does Linux land? Docker as the distribution unit?
6. **UI strategy**: boxen-only for 1.0? Native/protocol GUI? Or is *Manila in the browser* the GUI?
7. **The AI surface**: is agent-drivability (protocol mode, MCP, debugger) a headline feature of 1.0 or infrastructure?
8. **UserTalk evolution**: frozen for compatibility, or allowed to grow (UTF-8, 64-bit, new idioms)?
9. **Open source posture and sustainability**: license clarity, boxen extraction timing, funding (the Kickstarter idea — dead or dormant?).
10. **What does "done" mean**: define 1.0 release criteria and the demo that proves it (e.g., "Manila site editable from a browser on a $6 VPS, surviving `ab -c 10`").
