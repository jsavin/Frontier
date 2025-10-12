# Frontier Refactoring — FAQ

Status
- State: In Progress
- Phase: Cross-Cutting
- Last Updated: 2025-10-12
- Notes: Answers to recurring questions about headless work, UI boundaries, and planning docs.

Related Docs
- `planning/INDEX.md`
- `planning/phase3/ui_abstraction/PHASES.md`
- `planning/phase3/DEVELOPER_QUICKSTART_HEADLESS.md`
- `planning/phase_overview.md`

Change Log
- 2025-10-12: Updated links and terminology to match the new phase structure.
- 2025-09-29: Initial version.

**Q: Why not keep using `#if FRONTIER_HEADLESS` and scattered stubs?**
- **A:** It doesn’t scale. Conditional branches sprawl, diverge, and are hard to audit. Phase 3 adopts a ports-and-adapters model (`UIServices`) so the core talks to an abstract interface and adapters supply headless/AppKit/Win32 behaviour.

**Q: What does “ports-and-adapters” mean here?**
- **A:** The core depends on an abstract `UIServices` contract (the “port”). Implementations (“adapters”) provide behaviour for headless, AppKit, Win32, or web without changing the core. Design notes live in `planning/phase3/ui_abstraction/`.

**Q: What works in headless mode today?**
- **A:** Language and database operations, and any verbs that don’t require UI. UI-only verbs return a predictable error or no-op. See `planning/phase3/headless_stubbed_behavior_matrix.md` for the latest matrix.

**Q: Where should new UI-related docs go?**
- **A:** Place them under `planning/phase3/ui_abstraction/` so they stay with the rest of the ports-and-adapters work.

**Q: How do I keep doc links healthy after moving files?**
- **A:** Run `python3 scripts/check_doc_links.py` before you land large reorganisations. Update references and add a Change Log entry.

**Q: Which docs are authoritative for current planning?**
- **A:** Start with `planning/INDEX.md` and `planning/phase_overview.md`. Phase-specific details are inside the corresponding `phaseN/` directory.

**Q: How should I set the Status block?**
- **A:** Use Draft / In Progress / Completed / Deprecated / Archived. Most Phase 1 docs are historical (Completed); newer phases are In Progress.

**Q: My planning doc touches more than one phase. Where should it live?**
- **A:** Place it under the primary phase and cross-link from other phases or the index. If it represents a lasting architectural decision, also create/update an ADR (`planning/adr/`).

**Q: How do I update exit criteria or the phase roadmap?**
- **A:** Edit `planning/phase_overview.md` first, then update `planning/phase_gates.md` in the same branch so the gates stay aligned.

**Q: What if I need to reference the legacy 0.x numbering?**
- **A:** Mention the legacy identifier in the Change Log of the new location. The filenames inside each `phaseN/` directory still carry the old numbers for traceability.
