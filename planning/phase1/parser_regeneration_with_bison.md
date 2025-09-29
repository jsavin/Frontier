# Phase 1 Sub‑Task: Regenerate langparser.c with Modern Bison

Status
- State: Planned
- Phase: 1 (Headless/CLI enablement and testing)
- Owner: Language Runtime
- Last Updated: 2025-09-29

Motivation
- Current generated parser (`Common/source/langparser.c`) originates from an older Yacc/Bison skeleton that uses pre‑begin stack pointer patterns (e.g., setting stack pointers to `&stack[-1]`).
- Under modern sanitizers (UBSan/ASan), this triggers warnings like “index -1 out of bounds,” even though legacy skeletons increment pointers before dereference.
- Regenerating the parser with a modern, sanitizer‑friendly Bison skeleton reduces undefined‑behavior warnings, improves 64‑bit safety, and simplifies maintenance.

Scope
- Regenerate `langparser.c` and any associated headers (`yytab.h`/`langparser.h` equivalents) from `Common/source/langparser.y` using a modern Bison (3.x+).
- Keep the existing grammar semantics and token/value types intact (`YYSTYPE == hdltreenode`).
- Maintain current public interfaces used by `lang.c`, `langscan.c`, and call sites.

Deliverables
- New generated `langparser.c` (and headers) checked in.
- Build system updates to ensure regeneration rules are documented (optional) and the repo can build without Bison present (generated sources remain committed).
- Sanitizer runs (ASan/UBSan) of headless tests with parser stack warnings eliminated.
- Brief migration note summarizing any code adjustments.

Acceptance Criteria
- `runtime_tests` passes under ASan/UBSan with no parser stack UB warnings.
- No functional regressions in script parsing; smoke tests (e.g., `3+4`, basic comparisons, constants) parse/execute identically.

Implementation Plan
1) Environment
   - Pin a known‑good Bison version (e.g., 3.8+). Capture version in docs.
   - Confirm `langparser.y` compiles standalone with Bison and generates C output.

2) Generation
   - Run Bison with options for modern C skeleton and reentrancy if feasible:
     - Script: `scripts/gen_langparser.sh` (dry-run; writes to `tmp/parser/`)
     - Apply changes: `scripts/gen_langparser.sh --apply` (C only) or `--apply --apply-header` (C+H)
     - Direct example: `bison -y -o langparser.c --defines=langparser.h Common/source/langparser.y`
     - Prefer `%defines` and `%header` in grammar to standardize headers.
   - Ensure `%union`/`YYSTYPE` (hdltreenode) matches existing code.

3) Integration
   - Replace existing `langparser.c/langparser.h` with regenerated versions.
   - Adjust includes where necessary (e.g., `y.tab.h` vs `langparser.h`).
   - Update `langscan.c` and `lang.c` if token enums or prototypes shift.
   - Verify `YYMAXDEPTH` (stack size) and other parser params are sane for large scripts.

4) Validation
   - Build with sanitizers enabled; run `runtime_tests` and other parsing tests.
   - Confirm no UBSan “index -1” warnings and no new UB introduced.
   - Add CI note or a local target to regenerate and diff parser outputs (optional).

Risks & Mitigations
- Risk: Subtle behavior changes from new skeleton.
  - Mitigation: Keep grammar intact; extensive smoke tests; diff of key code paths.
- Risk: Toolchain dependency drift.
  - Mitigation: Commit generated C/headers; document Bison version; no hard build dep. Use the script and review diffs before applying.
- Risk: Stack size differences in new skeleton.
  - Mitigation: Set `%define api.push_pull` or stack size macros as needed; test large inputs.

Notes
- This work is orthogonal to hash/strings modernization; it’s a Phase 1 runtime quality task that improves headless test signal.
- Follow‑up (optional): Similarly modernize the scanner if needed and ensure consistent token header usage across modules.
