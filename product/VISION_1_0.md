# Frontier 1.0 — Product Vision

**Date:** 2026-08-09
**Status:** Approved (JES, 2026-08-09)
**Supersedes:** the 1.0 framing in `planning/kickstarter/KICKSTARTER_CAMPAIGN.md` (abandoned). This is the only current definition of 1.0.
**Companion:** research basis in `product/research/2026-08-09-frontier-1-0-research.md`.

This directory (`product/`) is the home for product-level direction going forward — the reset point. Engineering phase docs remain in `planning/`; where they conflict with this document, this document wins.

---

## Identity

**Frontier 1.0 is the integrated box**: scripting language + object database + web server + outliner + debugger in one artifact — the only system of its kind in 2026 — with **Manila as its flagship application** and the boxen terminal environment as its authoring face.

Priority order of identities (each real, in this order):

1. The integrated box (Frontier as Frontier)
2. The authoring environment (boxen REPL, editors, debugger)
3. Manila reborn (browser-based publishing)
4. AI-scriptable personal server (protocol mode, agent-drivable debugger) — load-bearing infrastructure, not the headline

**Users, in concentric rings**: Jake (ring zero) → Dave Winer → old Frontier hands (Ted Howard, Rogers Cadenhead, Oliver Wrede, ex-UserLand orbit) → curious developers → writers. Every ring gets a door into the box; the install-to-aha path is optimized inward-out.

**Relationship to Dave Winer's Node.js port** (UserTalk + ODB on Node, announced 2026-08-07): **friendly parallel**. Mutual awareness via blogs and Mastodon, no formal coordination, full autonomy for both projects. They naturally complement: his is the UserTalk language on a modern runtime; this is the full system — kernel fidelity, on-disk ODB format, debugger, and uniquely Manila + mainResponder + the webserver.

---

## 1.0 Release Criteria

No release criteria existed before this document (last tag: v1.0.0-alpha.7, 2026-02-16). 1.0 ships when both proofs pass:

1. **Full-circle demo** — on a fresh Linux VPS *and* a fresh Mac: install, first-run creates a Manila site, edit a page from a browser via Edit This Page, the site survives `ab -c 10 -n 1000`, and the same box runs the user's own UserTalk scripts from the boxen REPL.
2. **Deployment proof** — Manila running on Jake's own hardware.

### Constraints

| Dimension | 1.0 position |
| --- | --- |
| Platforms | macOS + Linux, both first-class; Docker image is the server distribution unit; Windows deferred (termbox2 choice keeps it viable) |
| Manila | Faithful first: legacy Manila as-is behind a bundled reverse proxy, fix only what is broken; modernization proposed in explicit phases after it demonstrably works |
| UserTalk | Compatibility-frozen: 1.0 runs legacy UserTalk faithfully; language evolution (UTF-8 strings, 64-bit defaults, new idioms) is a versioned post-1.0 track |
| Security | Hardened core, proxied edge: resolve #88 for the runtime (bind policy, authn story, input limits, no known memory-unsafety on network-reachable paths); modern password hashing + session handling in Manila; TLS delegated to a bundled reverse-proxy (Caddy) config. Public-internet deployable |
| UI | Manila's browser UI is the primary face; boxen second; native GUI post-1.0 |
| License / posture | Open source, no funding gate. Kernel is MIT (relicensed from GPL with Dave Winer's sign-off; documented in source history). GitHub is the community home; jakesav.in is the voice. Kickstarter stays retired |

---

## Build Order

### Phase 0 — Clear the decks (now)

- Land input_decoder M6 (cutover, on branch) and M7 (mouse-mode policy + `/mouse` toggle).
- In parallel: begin the boxen extraction as an independent open-source project (boxen Phase E plan: standalone repo, MIT, its own CI; Frontier consumes it back as a vendored submodule).
- Status-refresh pass on the actively misleading planning docs (`_CURRENT_STATUS.md`, `_CURRENT_TODO_LIST.md`, `INDEX.md`, `phase4/PROGRESS.md`, `phase4/p0a-critical-thread-safety/README.md`, ADR `README.md`) and correct the Phase A/B status table in `MANILA_MAINRESPONDER_E2E.md`. For an agent-driven project, stale ground-truth docs are launch-blocking hygiene.
- Mirror frontier.userland.com, docserver.userland.com, and manila.userland.com locally — still live in 2026, HTTP-only, single-homed, one hosting lapse from gone.

### Phase 1 — Web stack revival

- Triage umbrella issue #620: un-skip the 47 affected tests (all webserver/inetd/mainResponder E2E among them) and learn what actually still works. The March 2026 "HTTP pipeline works" milestone rested partly on a broken eval-trap assertion; current state is unknown.
- Fix the two known accept-callback kernel bugs: (a) server-side `writeStream` fires before the accepted stream is writable; (b) server closes the stream before the client polls (needs stream event notification).
- Make the integration-test baseline reproducible from a fresh clone (the accepted 21-known-failures list currently lives in `/tmp`).

### Phase 2 — Manila end-to-end

- Phase C of `MANILA_MAINRESPONDER_E2E.md`: inetd + mainResponder integration.
- Startup stabilization (`planning/phase4/STARTUP_STABILIZATION_PLAN.md`): first run → mainResponder + Manila install → HTTP server running → setupFrontier page. Known risks already enumerated there (betty.init, pikeRenderer.init, wp.newTextObject headless).
- Phase D: headless Manila installation + first served page (never attempted; human-in-loop).

### Phase 3 — Linux + Docker

- Port the runtime to Linux (endianness settled big-endian; the clang/Mach-O macOS-isms are bounded), add runtime Linux CI (today only boxen has an ubuntu job), ship the Docker image.

### Phase 4 — Hardening

- Resolve #88 (networking / HTTP security model) per the "hardened core, proxied edge" posture.
- Manila password hashing + sessions; bundled Caddy config.
- Fix the data-loss trio: #264 (db.close+db.open segfault), #270 (duplicate-open guard), #271 (auto-migration TOCTOU race).

### Phase 5 — Proof and ship

- Phase E concurrent-load safety audit (human-led) — the genuine unknown: GIL-era integration seams under real load.
- Full-circle demo on both platforms; deployment proof on Jake's hardware; tag v1.0.0.

---

## Post-1.0 Tracks

In rough order:

1. **Native GUI clients on the protocol layer** — an important post-1.0 goal, explicitly on the roadmap: GUI apps built against the already-shipped protocol interfaces (NDJSON stdio protocol, implemented; JSON-RPC 2.0 / WebSocket per `planning/gui/PROTOCOL.md`, `ws_server.c` foundations in place). Sequencing: web-technology client via Electron first, then a **native SwiftUI macOS app**. The runtime stays authoritative; GUIs are stateless presentation layers, as the existing architecture docs specify.
2. **Manila modernization, in proposed phases** — auth/UX/theming/Markdown/mobile, each proposed and approved separately once faithful Manila is live.
3. **UserTalk evolution** — versioned language track (UTF-8, 64-bit defaults, idioms) without breaking frozen-compatibility mode.
4. **AI surface as headline** — MCP server over the protocol layer; agent-drivable debugger promoted from infrastructure to feature.
5. **Interop expansions** — RSS/OPML/WebSub first-class from heritage; ActivityPub and/or ATProto bridges as differentiators.
6. **Windows** via the boxen/termbox2 path.

## Non-Goals for 1.0

Native GUI apps (post-1.0 track 1), UserTalk language changes, Windows, federation protocols, funding campaigns, and GUI-dependent verbs (~170 remain; the realistic headless ceiling of ~60-70% verb coverage is acceptable).

---

## Top Risks

1. **GIL-era integration seams under concurrent load** — the one true unknown; Phase E exists precisely for it. ADR-014's GIL is a deliberate deferral; #332 (ODB handle refcounting) gates ever removing it.
2. **The false-green pattern recurring** — #620 proved CI can lie via masked assertions; countermeasures are un-skipped tests and reproducible baselines, plus the E2E plan's own rule that load, visual, and data-loss verification stay human-led.
3. **Linux port unknowns** in a ~500k-line legacy C codebase.
4. **Solo-maintainer bus factor** — mitigated by unusually good docs, tests, and AI-legibility of the codebase.
