# Pascal-Era Data Modernization Plan

Status
- State: In Progress
- Phase: 3 (Headless Runtime)
- Last Updated: 2025-10-30
- Notes: Documents the wider effort to retire Pascal-style serialization after we finish the immediate table loader fixes.

Related Docs
- `planning/phase3/headless_legacy_table_loader.md`
- `planning/phase3/langhash_portable_missing_types.md`
- `docs/legacy_frontier_bootstrap.md`

Change Log
- 2025-10-30: Restored from archive, reformatted, and clarified near/mid-term objectives.
- 2025-10-22: Captured risks revealed by the headless table loader work.

Overview
- Frontier’s legacy serializer depends on Pascal conventions (length-prefixed strings, handle-based blocks, big-endian fields) that clash with modern portable expectations.
- As long as tables and externals retain these layouts, the headless runtime requires ad-hoc converters and the migrator risks losing data fidelity.

Drivers
- **Correctness:** Without an explicit translation layer, `tableunpacktable` and related routines crash or silently corrupt data.
- **Maintainability:** Pascal assumptions are scattered through `langhash.c`, `tablepack.c`, and friends; documenting them is critical before touching the code.
- **Future modernization:** UTF-8, streaming IO, and eventual little-endian support depend on separating logical data from the legacy binary packing.

Details
1. **Document today’s layout**
   - Capture the exact structure of `tydisktablerecord`, `tydisksymbolrecord`, Pascal string pools, and any helper structures (`disktableformat`, etc.).
   - Annotate the new loader helpers once we reverse-engineer each portion so future contributors understand invariants.
2. **Introduce shared helpers**
   - Create reusable utilities for Pascal strings, classic symbol records, and legacy merge-handled buffers instead of repeating pointer math.
   - Add round-trip unit tests for each helper using v6 fixtures.
3. **Plan the real modernization**
   - Decide how we migrate persistent data to modern layouts (length-prefixed UTF-8, flat buffers, explicit endianness).
   - Provide tooling to rewrite existing `.root` files once the new format is stable.

Open Questions
- Which portions of the Pascal layout must stay for backward compatibility, and which can be rewritten during migration?
- Do we normalize payloads eagerly (during v6→v7 migration) or lazily (at load time) to reduce risk?
- What additional documentation do we need for non-table externals (menus, wptext, pict) before attempting format changes?

Next Steps
- Finish documenting the exact byte layouts uncovered while fixing the headless loader and add diagrams/examples here.
- Identify shared helper APIs we can land during the Carbon migration without destabilizing the desktop build.
- Draft a modernization proposal (target format, migration tooling, compatibility story) once table loaders and external converters are reliable.
