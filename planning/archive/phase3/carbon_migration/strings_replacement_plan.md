# STR# Replacement Plan

Status
- State: In Progress
- Phase: Carbon Migration (String Tables)
- Last Updated: 2025-11-20
- Owner: Codex
- Notes: Tracks migration from STR# resources to portable tables; update as phases progress.

**Date:** October 31, 2025  
**Owner:** Codex  

Goal: retire classic Mac OS STR# resources and the legacy Resource Manager code path while keeping feature parity with the legacy UserTalk loader. We will tackle this in two phases: first by vendoring libyaml to regain parity quickly, then by iterating on the bespoke bison pipeline once the system is stable.

---

## Phase 1 — Vendor libyaml (current)

### 1. Target Format
- **Source files:** Maintain human-editable YAML (preferred) or JSON under `resources/strings/`. Each file stores a named table (`langerrorlist`, `dberrorlist`, etc.) with stable keys and Pascal-string metadata where applicable.
- **Generated headers:** A vendored libyaml-backed tool will emit:
  - `generated/strings_tables.c` – const `char` arrays and lookup tables.
  - `generated/strings_tables.h` – declarations (`extern const char *langerrorlist[];`, counts, helper enums).
- **Runtime API:** Replace `getstringlist`/`tablegetstringlist` with a portable lookup helper (`strings_lookup(table_id, index, bigstring out)`), implemented in `Common/source/strings_portable.c`.

---

### 2. Tooling (libyaml)
- **Parser:** Vendor the libyaml C reference implementation under `third_party/libyaml/` and wrap it with `tools/strings_compiler/strings_yaml_loader.c`, which feeds YAML events into the existing `strings_document` builder.
- **Compilation flow:**
  1. Add build rules so the compiler and tests link against the bundled libyaml library.
  2. Replace the current minimal parser logic with the libyaml-backed loader inside `tools/strings_compiler`.
  3. Continue emitting manifest/headers/C using the existing generation code.
- **Build integration:** Keep the `make strings_generated` target, but have it depend on the libyaml-enabled compiler. Ensure the vendored sources build on macOS/Linux (and document prerequisites).

---

### 3. Migration Steps
1. **Bootstrap data**  
   - Extract existing STR# tables via current loader (temporary script) and serialize to YAML (commit these files).
   - Run the libyaml-powered `strings_compiler` and confirm existing code can compile against the new tables behind a feature flag (`FRONTIER_PORTABLE_STRINGS`).
2. **Introduce lookup helper**  
   - Implement `strings_lookup` in `strings_portable.c`.
   - Update `langerror.c`, `langregexp.c`, `db.c`, etc., to call the helper instead of `getstringlist`.
   - Gate legacy `resources.c` usage behind `#if !defined(FRONTIER_PORTABLE_STRINGS)`.
3. **Retire Resource Manager**  
   - Remove STR# loader from the headless build once all core modules use the portable helper.
   - Desktop build can continue to compile legacy path until the modern GUI migrates.
4. **Testing**  
   - Extend `tests/runtime_tests.c` to assert known string IDs (e.g., `langerrorlist[unknownfunctionerror]`).
   - Add CLI smoke test verifying `clock.now()` and other verbs that depended on STR# strings produce expected output.

---

### 4. Risk Mitigation
- Keep source YAML files under version control; include a checksum in generated headers to detect drift.
- Provide a lint script (`scripts/check_strings.py`) that ensures no duplicate indices and catches missing translations (can reuse libyaml wrapper).
- Document the workflow in `docs/developer/strings.md` so contributors regenerate tables correctly.
- Track libyaml updates and security fixes (document upstream version + URL).

---

### 5. Hand-off Checklist
- Vendored libyaml and wrapper committed and referenced in `_CURRENT_STATUS.md`.
- All headless builds link only against generated tables (Resource Manager excluded).
- Tests updated to cover key tables.
- Legacy STR# code marked desktop-only or deleted once desktop migration is complete.

---

## Phase 2 — Enhanced bison/flex pipeline (future)
- Once the libyaml path is stable, revisit the bespoke `tools/strings_compiler` parser to remove the dependency and regain fine-grained control.
- Required work:
  1. Extend the grammar and scanner to support the full YAML subset used historically (indentation aware, nested maps/sequences, folded strings, additional scalar types).
  2. Eliminate the current shift/reduce conflict and add regression fixtures ported from the legacy loader.
  3. Provide exhaustive tests that compare bison output against libyaml results to guarantee parity.
- Track this effort in `TODO_future_improvements.md` and schedule when we have bandwidth; libyaml remains the fall-back until parity is proven.
