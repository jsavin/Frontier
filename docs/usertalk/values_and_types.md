# Values and Types

Stub. The full type taxonomy reference; intended as the "what *is* a `scriptType`?" / "what's the difference between `numberType` and `longType`?" lookup. Flesh out as questions arise.

---

## Status

**STUB — TODO**. Most of what's needed is currently scattered across:

- `CLAUDE_PRIMER.md` § 1, 2.5 — mental model + iteration distinction
- `records_and_tables.md` — `tableType`, `recordType`, `listType`, `addressType` deep-dive
- `strings_and_text.md` — `'TEXT'` string representations
- `operators_and_idioms.md` — `typeof ()`, `sizeOf ()`, `nameOf ()` usage
- `TYPEOF.md` — typeof semantics, OSType code lookup table, "never change typeof to return strings" historical note
- Agent def `usertalk-engineer.md` — 28-type summary, the 10 most common (numberType, stringType, booleanType, addressType, arrayType, recordType, tableType, outlineType, scriptType, wptextType)

Consolidate here when:
- A burndown task surfaces a type confusion not covered by the above
- A reader asks "what's the difference between `numberType`, `longType`, `floatType`?" (these may overlap or differ subtly)
- Coercion rules need documenting

## TODO topics

- Full enumeration of the 28 types
- Numeric subtypes: `longType` vs `floatType` vs `numberType` — coercion and storage
- Date/time types: `dateType` + the `date.set` parameter-order trap (already in `../TESTING_GUIDE.md`)
- File-related types: `filespecType` (`'fss '` — note trailing space), `binaryType`, `bigstring`
- UI types that may be headless-no-ops: window/menu/dialog types
- Coercion rules across types
- When `typeof (value)` returns one OSType but `system.compiler.language.constants.*` claims another — divergence
- Type predicates: `defined ()`, `typeof () == sometype` patterns

---

## See also

- `CLAUDE_PRIMER.md`
- `records_and_tables.md`
- `TYPEOF.md`
- `SYNTAX.md`
