# Session Summary — Headless Frontier.root Bring-up

## Focus
- Move headless CLI/tests toward loading the real sanitized `Frontier.root` instead of relying on the temporary EFP shim.
- Diagnose why dotted lookups (`system.*`, `system.verbs.*`, etc.) still failed even though the sanitized database should contain those tables.

## Key Investigations
- Reviewed headless CLI bootstrap (`frontier-cli/main.c`) to confirm the loader pulls in external table modules and sets up `roottable`, `systemtable`, `verbstable`, etc.
- Ran the CLI against the sanitized database; observed repeated `langgetdotparams` failures despite tables appearing in memory.
- Instrumented symbol lookup paths (`langexternalgettable`, `langsearchpathlookup`, `initialize_minimal_system_tables`) to verify which tables were being resolved and whether they were cached.

## Changes Made (working tree, uncommitted)
1. **langexternal.c** — added headless fallbacks that:
   - Return cached tables for well-known names (`system`, `verbs`, `builtins`, `agents`).
   - Fall back to raw hash lookups in `roottable` / `systemtable` when sanitized roots omit EFP wrappers.
2. **langvalue.c** — ensured `langsearchpathlookup` recognizes `system` as a special case in headless mode.
3. **frontier-cli/main.c** — reworked the minimal bootstrap:
   - Loads external table variables into memory via `load_external_table_value`.
   - Forces the CLI table stack to contain the real root/system tables and logs what it finds.
   - Adds temporary diagnostics (walk `system.verbs`, report missing families) to aid ongoing debugging.

## Current State
- Headless CLI now locates `system`, `system.verbs`, `system.verbs.builtins`, etc., and dotted lookups produce table handles instead of falling back to the shim.
- Higher-level verbs (e.g., `defined(@system.verbs)` or `system.verbs.file.exists`) still fail because sanitized content lacks those scripts; loader now surfaces that gap clearly.
- Several debug prints remain; they will need pruning or guarding before finalizing.

## Next Steps
- Decide whether to hydrate the sanitized database (e.g., via the existing CLI hydration path) or regenerate the missing `system.verbs` families.
- Reduce or remove the verbose logging once the bring-up stabilizes.
- Add targeted tests that invoke real `system.verbs.*` handlers once the sanitized root includes them.
