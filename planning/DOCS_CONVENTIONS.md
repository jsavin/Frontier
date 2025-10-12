# Documentation Conventions

Status
- State: In Progress
- Phase: Cross-Cutting
- Last Updated: 2025-09-29
- Notes: Use this guide when adding or editing docs.

Related Docs
- planning/_TEMPLATE.md
- planning/INDEX.md

Change Log
- 2025-09-29: Initial version.

File Naming
- Use short, descriptive names. For phase-specific docs, place under the phase folder (e.g., `planning/ui_abstraction/phase2/`).
- Avoid spaces; prefer hyphens or underscores.
- Keep historical phase-0 docs at top-level as-is; new UI abstraction docs live under `planning/ui_abstraction/`.

Status Block
- Include at top of every Markdown file:
  - State (Draft/In Progress/Completed/Archived)
  - Phase
  - Last Updated
  - Optional: Owner, Last Reviewed, Notes

Related Docs
- Add a short list of the most relevant upstream/downstream docs.
- Include a back-link to `planning/INDEX.md` or the phase hub when appropriate.

Change Log
- Add a one-line entry when the document is created or meaningfully updated.

Headings & Anchors
- Use ATX-style headings (`#`, `##`, `###`).
- Keep section titles short and noun-phrase oriented.
- The link-checker slugifies anchors GitHub-style; avoid punctuation in headings.

Links
- Prefer relative repo paths. Avoid external links where an internal reference exists.
- For moved docs, update links immediately and reference the new path.

Tables of Contents
- Add a short ToC to longer docs (6–8 headings) for scanability.

Lifecycle
- Mark older/historical docs as `Archived` if they are not expected to change.
- For living docs, refresh `Last Updated` and `Last Reviewed` when substantive changes are made.

