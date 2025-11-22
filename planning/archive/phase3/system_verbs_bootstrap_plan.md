> **Archived:** This Phase 3 document is preserved for historical context. The active plan lives in [`planning/phase3/carbon_migration/README.md`](../../carbon_migration/README.md).

# System Verbs Bootstrap (Temporary)

Purpose
- Unblock headless/testing before Frontier.root loads by registering a minimal `system.verbs` programmatically. This is a temporary bridge to preserve canonical routing (`system.verbs -> kernelcall -> EFP`).

Approach (no persisted DB)
- Export a manifest of `system.verbs` from a production Frontier (Windows) instance (subset OK). Include: family, verb, parameters (names/types if known), and notes.
- Generate C code at build time that registers wrappers calling `kernelcall(family, verb, args)`.
- Guard behind `FRONTIER_HEADLESS_BOOTSTRAP_EFP=1`; default OFF in release builds.

Manifest sketch (JSON)
```
[
  {"family":"file","verb":"open","params":[{"name":"path","type":"string"}],"notes":"Opens file; headless uses portable file layer"},
  {"family":"file","verb":"exists","params":[{"name":"path","type":"string"}]}
]
```

Build integration
- Place manifest at `planning/manifests/system_verbs.json` (not required to ship).
- Add a small generator (C or script) to emit `system_verbs_bootstrap.c` compiled only when the flag is ON.

Removal plan
- Delete generator and bootstrap code once Frontier.root loads and parity tests pass via `kernelcall`.
- Keep tests; they should run against the real `system.verbs` thereafter.

Risks & Mitigations
- Drift from real scripts: minimize by deriving wrappers from exported manifest; keep the set small.
- Overuse: clearly documented as temporary; default OFF; lint in CI to prevent enabling for releases (once CI exists).

Links
- planning/adr/0010-efp-headless-routing.md
- planning/EFP_HEADLESS_NOTES.md
