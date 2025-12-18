# Database Path Canonicalization and Cross-DB References

Status
- State: 📋 Draft/Deferred
- Last Updated: 2025-09-30
- Notes: **NOT ACTIVE** - Early-stage exploration for future multi-database path handling. Depends on Phase 3 completion.

Problem
- Frontier stores and exchanges references to objects in “guest” databases using native platform path strings (e.g., `"Macintosh HD:Applications:Frontier:Guest Databases:www:myWebsite.root"`).
- Scripts and values may embed these paths directly (e.g., `@["<path>"] .tableName`).
- This causes fragility across:
  - Platform changes (Mac ↔ Windows),
  - On‑disk relocations (application folder moved),
  - Build/runtime contexts (headless vs GUI),
  - Future containerization.

Goals
- Make cross‑DB references portable and resilient while maintaining backward compatibility.
- Avoid breaking existing content that serializes path strings.

Options (with tradeoffs)
1) Internal canonicalization (POSIX‑style)
   - Normalize all paths internally to `"/Volumes/…"` or simple POSIX style while still accepting legacy strings.
   - Pros: Single format in memory; simplifies resolution code.
   - Cons: Requires compatibility layer for legacy display, UI, and serialization.

2) Relative project paths
   - Store paths relative to a configured “root” (e.g., Frontier application folder or a workspace root), using ‘/’ separators.
   - Pros: Moves/resolves cleanly within a project; easier to sync.
   - Cons: Requires a root registry; absolute lookups still needed for external DBs.

3) Logical IDs with alias registry
   - Introduce stable DB ids (e.g., UUID) and maintain a registry mapping id ↔ current path.
   - Pros: Portable and resilient; paths become a transport concern.
   - Cons: Requires new storage for ids; needs migration and a resolver.

4) URI‑like scheme
   - Formalize `frontierdb://` URIs with components (host, id, path, logical table).
   - Pros: Self‑describing; transportable.
   - Cons: Adds new syntax; needs broad adoption.

Recommended phased approach
Phase A (Compatibility)
- Add a resolver that accepts legacy native paths and POSIX paths and resolves them to an FSRef/filespec.
- Canonicalize to POSIX internally for comparison and caching.
- Record a project/workspace root; resolve `./` and `../` relative paths.

Phase B (Stability)
- Introduce an optional DB identifier stored in the DB header or companion file.
- Maintain a path alias registry for recent locations.
- When serializing, prefer relative POSIX paths when under the root; fall back to absolute POSIX; retain an option to serialize legacy native format for backward‑compat.

Phase C (Authoring & Migration)
- Add tools to analyze and convert embedded legacy paths to canonical POSIX (with backups/preview).
- Provide a report for unresolved paths and suggested fixes.

Interoperability rules
- Always accept both legacy native and POSIX inputs at the API boundary.
- Prefer POSIX for new serialization and UI display in headless and modern builds; provide a preference for legacy display where needed.

Testing
- Round‑trip paths across Mac/Windows samples.
- Move/rename Frontier app folder and verify resolution via alias registry and relative paths.
- Validate embedded references in real guest DBs continue to resolve.

