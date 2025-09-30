# Phase 5 — Bison 3 Migration (Gated)

Status
- State: Planned (Deferred until Apple toolchain update)
- Last Updated: 2025-09-30
- Owner: Runtime/Language

Objective
- Migrate the UserTalk parser from legacy MacYACC/Bison 2.3 assumptions to a parser regenerated with Bison 3.x while keeping the repo buildable on systems that only have Apple’s stock toolchain.

Guiding Principles
- No new mandatory tool dependencies for contributors. Generated sources are committed.
- Maintain a single grammar file (`Common/source/langparser.y`) that continues to work with Bison 2.3.
- Stage Bison 3 features behind compatibility guards; only adopt them when they do not break Bison 2.3 generation.
- Do not require Bison at build time; CI and maintainers may regenerate as needed.

Out of Scope (for this phase)
- Rewriting scanner or tokenization.
- Enforcing reentrancy (`%define api.pure full`) unless trivially compatible.

Current State
- Parser builds from a legacy, generated `langparser.c` (MacYACC-style). Runtime tests pass, but UB warnings exist in old skeleton paths.
- Grammar updates in progress to replace legacy action flow with Bison-friendly `YYABORT/YYACCEPT` and remove stack poking and `goto` patterns.

Constraints
- Apple’s system `bison` is typically 2.3. We will not require Homebrew/MacPorts.
- All developers must be able to build without `bison` present.

Tooling Policy
- Commit generated parser C (and header if necessary).
- Prefer Bison 3.x in CI/maintainer environments for regeneration; keep 2.3 compatibility.
- Scripts:
  - `scripts/gen_langparser.sh` — regenerate and show diffs; `--apply` to overwrite C; coerces `YYSTYPE` to `hdltreenode` if Bison omits it.
  - Optional future: `BISON=/path/to/bison3 scripts/gen_langparser.sh` to force a specific binary.

Regeneration Tooling
- Script usage
  - Dry run (diff only): `scripts/gen_langparser.sh`
  - Apply C only: `scripts/gen_langparser.sh --apply`
  - Apply C + header: `scripts/gen_langparser.sh --apply --apply-header`
- Selecting Bison
  - Set `BISON=/absolute/path/to/bison` to pick a specific binary.
  - The script falls back to the first available in `$PATH`, then tries common Homebrew locations:
    - `/opt/homebrew/opt/bison/bin/bison` (Apple Silicon)
    - `/usr/local/opt/bison/bin/bison` (Intel)
- Version expectations
  - System `bison` on macOS is often 2.3; generation still works with our 2.3‑compatible grammar.
  - When Bison emits `typedef int YYSTYPE;` (common on older versions), the script post‑fixes the generated C to use `typedef hdltreenode YYSTYPE;` to align with the runtime.
- Source control
  - Always commit the regenerated `Common/source/langparser.c` (and header if needed) so contributors can build without Bison.
  - Include the Bison version in the regeneration commit message (e.g., `regen(parser): bison 3.8.2`).

Compatibility Strategy
- Keep using a single `.y` file; optional fragments may exist for authoring but compose into one file before regeneration.
- Use only syntax accepted by Bison 2.3 in the `.y` file. When Bison 3 features are desirable (e.g., `%code`, `%printer`, `%destructor`), prefer equivalent C preprocessor guards in the prologue/epilogue instead of grammar directives.
- Configure semantic value type via preprocessor (e.g., `#define YYSTYPE hdltreenode`) so both 2.3 and 3.x agree.
- Provide Bison-version-agnostic shims in the prologue:
  - Prototypes for `yylex`/`yyerror`.
  - Map legacy globals: `pcyyerrct→yynerrs`, `yypv→yyvsp`, `yyv→yyvs`.
  - Expose an explicit result handle (e.g., `langparser_result`) instead of reading `yyval` outside `yyparse`.

Migration Steps
1) Action Flow Cleanup (compatible with 2.3)
   - Replace `goto cleanexit`/manual stack disposal with `YYABORT`/`YYACCEPT` at appropriate points.
   - Remove label-based clean-up blocks; ensure rule actions allocate/assign defensively.
   - Keep existing error reporting semantics (`yyerror` + `parseerror`).

2) Result Handling
   - Set `langparser_result` in the top-level rule on success; switch runtime code to read it.
   - Retain fallback to legacy behavior until regenerated parser is validated.

3) Regeneration & Validation (maintainer/CI only)
   - Generate via `scripts/gen_langparser.sh` (Bison 3.x preferred) and apply C.
   - Build and run runtime tests (with and without sanitizers); confirm parity.
   - If 2.3 and 3.x produce materially different diagnostics, document them.

4) Optional 3.x Enhancements (guarded)
   - Introduce `%destructor` where beneficial for automatic cleanup, only if we can keep 2.3 compatibility (or guard via preprocessor in code blocks rather than grammar directives).
   - Consider `%printer` for debug output if guarded.

Gating Criteria
- Apple updates the system toolchain to ship Bison 3.x, or
- We decide to regenerate with 3.x in maintainer/CI environments only (still committing generated C), without requiring contributors to install it.

Risks & Mitigations
- Divergent behavior between 2.3 and 3.x: keep rule actions deterministic and avoid skeleton-specific assumptions.
- Build instability during transition: maintain legacy `langparser.c` until tests pass with regenerated C; revert is trivial.
- Tool version drift: pin Bison version used in CI and record it in the commit message when regenerating.

Test Plan
- Targeted tests for parser basics (expressions, statements, handlers).
- Full `runtime_tests` with sanitizers enabled.
- Optional golden outputs for error messages (coarse-grained) to avoid overfitting to skeleton wording.

Deliverables
- Updated `langparser.y` with action flow cleaned up and compatibility shims.
- Regenerated `langparser.c` (Bison 3.x), committed once validated.
- Documentation updates: this plan, regeneration instructions, and CI notes.

Rollback Plan
- If regenerated parser causes regressions, restore the last known-good `langparser.c` and open a follow-up task to address diffs incrementally.
