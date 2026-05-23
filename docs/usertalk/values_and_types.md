# Values and Types

Pulled in when: you hit an unexpected `typeof ()` return value, you need to understand coercion across types, or you're debugging a "Can't coerce X value to Y" error.

The primer (`CLAUDE_PRIMER.md`) and `records_and_tables.md` cover the four collection types in depth. This file covers the full type taxonomy, coercion, and type-testing patterns.

---

## Type constants

Types are identified by 4-byte OSType codes. The runtime-accessible constants live in `system.compiler.language.constants` as `string4` scalars with names ending in `Type`:

```
typeof (val) == stringType        // ✓ — stringType is the OSType constant 'TEXT'
typeof (val) == 'TEXT'            // ✓ — same thing, literal form
typeof (val) == "string"          // ✗ — always false; RHS is a string, not an OSType
```

`typeof ()` returns an OSType, not a descriptive string. Never try to compare it to a plain string.

---

## The most common types

| Constant | OSType | Description |
|---|---|---|
| `stringType` | `'TEXT'` | String (Pascal-string up to 255 bytes, or Handle for longer) |
| `longType` | `'long'` | 32-bit integer |
| `doubleType` | `'doub'` | 64-bit float |
| `booleanType` | `'bool'` | Boolean (`true` / `false`) |
| `charType` | `'char'` | Single character — `'X'` literal form, NOT a single-char string |
| `addressType` | `'addr'` | ODB address (produced by `@x.y.z`) |
| `tableType` | `'tabl'` | ODB table — dotted-name access, alpha-sorted in legacy, insertion-order in headless |
| `recordType` | `'reco'` | Record — insertion-ordered key/value, ordinal access only |
| `listType` | `'list'` | List — positional, heterogeneous, `{1, "two", true}` literal |
| `scriptType` | `'scpt'` | Script object — stored as compiled outline, stringified with `string ()` |
| `outlineType` | `'optl'` | Outline object — tree of nodes with attributes |
| `wptextType` | `'wptx'` | Word-processor text object |
| `dateType` | `'date'` | Date/time value |
| `binaryType` | `'bina'` | Raw binary data |
| `filespecType` | `'fss '` | File system reference (note trailing space in OSType) |

`nil` has no meaningful type: `typeof (nil)` returns `'????'` (unknown). `defined (nil)` returns `true` — `nil` is a value, it just has no type.

---

## Numeric types

UserTalk has three numeric types:

- **`longType`** (`'long'`): 32-bit signed integer. The default for integer literals and `for i = 1 to n` loop counters.
- **`doubleType`** (`'doub'`): 64-bit float. Produced by floating-point literals (`3.14`) or float arithmetic.
- **`numberType`** (`'nmbr'`): Legacy numeric type from early Frontier. Rarely produced in modern code; you may encounter it in old ODB values.

Arithmetic between `long` and `double` coerces toward `double`. Integer division truncates (5/2 = 2).

---

## Coercion

UserTalk coerces values across types in many positions — sometimes helpfully, sometimes silently wrong.

**Safe coercions** (lossless or well-defined):
- `long` ↔ `double` — numeric widening/narrowing
- `long` → `string` — stringification via `string ()`
- `boolean` → `string` — `"true"` / `"false"`
- `char` → `string` — single-char string

**Coercion at the `==` operator**: UserTalk will try to coerce operands to a common type. `1 == "1"` may return `true` depending on context — don't rely on this. Use explicit `string ()` or `number ()` when crossing type boundaries.

**No coercion**: you cannot use `contains`, `beginsWith`, `endsWith` on `recordType` values — "Can't coerce X value to a record" error. See `operators_and_idioms.md`.

**Protocol boundary coercion**: the NDJSON protocol serializes all values to strings in the `"value"` field. A `long` 42 arrives as `{"value":"42","type":"long"}`. When `expected_result` in a yaml test compares against protocol output, you're always comparing strings. See `testing_patterns.md`.

---

## Type testing patterns

```
// Check type before operating
if typeof (val) == tableType {
    for adrM in @val {...}}

// Safe member access with type guard  
if typeof (val) == recordType {
    // must iterate ordinally — no dotted-name access
    for v in val {...}}

// Distinguish long from double
if typeof (n) == longType {
    local (rounded = n)}
else {
    local (rounded = number.round (n))}
```

For the collection types (table, record, list), use `records_and_tables.md` for iteration patterns.

---

## The `nil` value

`nil` is a special value that represents "nothing." It has no type (`typeof` returns `'????'`), but it IS defined (`defined (nil)` → `true`).

Uses:
- Initializing a variable that will hold an address: `local (adrFound = nil)`
- Sentinel value in a loop that may or may not find a match
- Return from a verb to signal "not found"

Test for nil by comparing to the literal: `if adrFound == nil {...}`. Don't test `!defined (adrFound)` — that checks scope resolution, not the value.

---

## `dateType` traps

The `date.set` verb has a non-obvious parameter order. See `../TESTING_GUIDE.md` for the documented trap. Don't construct `dateType` values by hand in tests without reading that first.

---

## See also

- `CLAUDE_PRIMER.md` — mental model and 5 core idioms
- `records_and_tables.md` — tableType, recordType, listType, addressType in depth
- `TYPEOF.md` — typeof semantics and the "never return descriptive strings" historical note
- `operators_and_idioms.md` — type-testing idioms and `typeof ()` usage
