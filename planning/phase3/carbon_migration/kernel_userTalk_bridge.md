# Kernel ↔ UserTalk “Limbic System” Overview

Status
- State: Reference
- Phase: Carbon Migration / Runtime Architecture
- Last Updated: 2025-11-20
- Owner: Codex
- Notes: Canonical description of kernel↔UserTalk interactions; update when bridge behavior changes.

## Purpose
Capture the architectural principles behind Frontier’s mixed C/UserTalk runtime so we stop rediscovering them every time we debug `system.paths`, built-in tokens, or table hydration. This document focuses on how kernel verbs, serialized tables, and the search path interact—what Dave/Brent called the system’s “limbic system”.

## High-Level Flow
1. **Bootstrap**  
   - `tableverbinmemory` hydrates serialized externals (`idtableprocessor`, `idscriptprocessor`, etc.) from the root DB.
   - `checktablestructure()` finds/creates the canonical tables (`system`, `system.verbs`, `system.paths`, etc.) and stores their handles in globals (`systemtable`, `verbstable`, …).
   - `langinitbuiltins()` seeds `system.compiler.language.builtins` with token entries sourced from `langverbs.c`. Each entry is a hash node whose value is a `token` with a numeric ID.

2. **Name Resolution (`langgetdotparams`/`langsearchpathvisit`)**  
   - Parsing a dotted name (`system.verbs.clock.now`) produces a syntax tree. During evaluation the runtime calls `langgetdotparams()` which:
     1. Walks the explicit path (`system` → `verbs` → `clock` → `now`) by looking up each node in the current hashtable.
     2. If a segment isn’t found, falls back to `langsearchpathvisit()` which iterates the tables listed in `system.paths.pathN` and tries `langfindsymbol()` in each.
   - Once a hash node is found, the evaluator inspects the `tyvaluerecord`:
     * `valuetype == externalvaluetype` → call `langexternalgettable()` which unwraps the external handle (table/script/pict/etc.) and returns the in-memory object.
     * `valuetype == tokenvaluetype` → treat the entry as a kernel token and dispatch to `langfunctionvalue()` with the token ID.

3. **Execution Paths**
   - **Tokenized verbs (kernel)**: Entries like `system.compiler.language.builtins.pack` store the token ID (e.g., `token = 6`). The parser emits that token, and during evaluation `langfunctionvalue()` hits the `case packfunc:` branch, calling the C implementation directly.
   - **Script glue**: Entries such as `system.verbs.clock.sleepFor` store an outline (script). `langexternalgettable()` returns the outline/table handle; the evaluator compiles it (if needed) via `opverbscriptunpack()` and runs it in the current process stack.
   - **External tables**: Regular tables (`system.examples`, `system.paths`) are externals unpacked via `tableverbunpack()` + `hashunpacktable()`. Their `hdltablevariable` retains the DB address so subsequent saves can reserialize them.

## Design Principles (Historical Context)
- **Tokens for hot paths**: Frequently used verbs or ones requiring OS calls (`pack`, `clock.*`, `file.*`) stayed in `langverbs.c` as tokens so they could jump straight into C without script overhead. That’s why you won’t find a UserTalk definition for `pack`.
- **Everything is a table**: Scripts, outlines, menus—the classic kernel serializes each as a `tytableprocessor`/`idscriptprocessor` external so the DB only needs one packing format (hash table records + string pool + optional format blob).
- **Runtime search scope**: `system.paths` wasn’t a static DSL trick; it was the live scope chain. Adding/removing entries there changes where `langsearchpathvisit()` looks at runtime, which is how guest databases (`system.compiler.files`) are made visible.
- **Bridge responsibilities**:
  * The kernel owns serialization, external IDs, and token dispatch.
  * UserTalk scripts (in `system.verbs`, `system.compiler.*`) orchestrate higher-level behavior but ultimately rely on the kernel to locate tables and run tokens.
  * The DB header (`views[]`) gives the kernel the root pointer; resolvers work entirely in-memory after hydration.

## Implications for Modernization
- When a lookup fails with bizarre names (`macintoshcintosh…`), it’s usually logging noise (Pascal strings without manual termination) or malformed search-path entries—not necessarily a loader bug. Instrumentation should copy/terminate Pascal strings before printing.
- Regenerating `Frontier-v7.root` requires the legacy table converter to understand every `idtableprocessor` payload (`system.examples`, etc.). Each block follows `[outer merge size][inner header][records][strings][formats]`; once we decode that, the bridge logic automatically sees genuine tables.
- Modern portable behavior (e.g., `clock.*`) still depends on the token table: replacing the implementation means updating the token case in `langverbs.c`, not editing `system.verbs.clock`.

## TODO / Open Questions
- Document the exact token list (token ID ↔ verb name) and keep it in sync with `system.compiler.language.builtins`.
- Describe how guest databases populate `system.compiler.files` and how the kernel uses that table during `langsearchpathvisit()`.
- Capture the historical rationale for `system.paths` ordering and how `system.macintosh` glue was expected to behave on non-Mac platforms.

---
*Please update this file whenever new architectural insights surface. Link it from `_CURRENT_STATUS.md` and `planning/INDEX.md` so future passes don’t lose the context.* 
