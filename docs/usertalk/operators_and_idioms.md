# Operators and Idioms

Pulled in when: an operator doesn't behave the way you'd expect from another language, you can't remember which verb does a substring check, you want to know whether `with` is safe in this context, or you're hunting an idiomatic one-liner.

The primer (`CLAUDE_PRIMER.md`) gives the five idioms you'll write 90% of the time. This file is the rest of the operator surface — comparison, logical, arithmetic, address, `contains`, pattern matching, the `with` block, and a greatest-hits idiom list.

---

## Comparison operators

UserTalk supports both **symbol** and **word** forms. They compile identically. UserLand convention is the symbol forms.

| Symbol | Word | Meaning |
|---|---|---|
| `==` | `equals` | Equal |
| `!=` | `notEquals` | Not equal |
| `<` | `lessThan` | Less than |
| `<=` | `lessThanOrEqual` | Less than or equal |
| `>` | `greaterThan` | Greater than |
| `>=` | `greaterThanOrEqual` | Greater than or equal |

```
if examples.count == 0 {...}                   // ✓ idiomatic
if examples.count equals 0 {...}               // compiles, non-idiomatic
```

There is **no `===`** (no JavaScript-style strict-equality). UserTalk will coerce across types for cross-type comparison — exactly which coercions happen is not well-specified at the language level. When the two operands might be different types and the answer matters, `typeof ()` both sides first:

```
if typeof (a) == typeof (b) && a == b {...}    // explicit type-then-value check
```

A common burndown trap: comparing a numeric `1` against a stringified `"1"` from protocol JSON. Add explicit `string ()` or `number ()` coercion when crossing the protocol boundary.

### Equality on tables, records, lists, addresses

- **Tables**: `==` on two table values is rarely meaningful — most table comparisons should compare addresses (`@a == @b`) or contents (walk both).
- **Addresses**: `@x == @y` is path-string equality, NOT "do these resolve to the same object."
- **Records and lists**: `==` is a value comparison; member-by-member equality.

If you need deep-equal semantics, write a helper. The built-in `==` doesn't try hard.

---

## Logical operators

Same symbol/word duality as comparisons:

| Symbol | Word |
|---|---|
| `&&` | `and` |
| `\|\|` | `or` |
| `!` | `not` |

```
if defined (@examples.foo) && examples.foo > 0 {...}
if !found {...}
```

Short-circuit evaluation works as you'd expect — `&&` stops on the first false, `||` stops on the first true. This means `defined (@x) && x^ > 0` is safe when `x^` would error on a missing target.

---

## Arithmetic operators

| Op | Meaning |
|---|---|
| `+` | Add (numeric) or concat (string) — see below |
| `-` | Subtract / negate |
| `*` | Multiply |
| `/` | Divide |
| `%` | Modulo |
| `^` | Power (when applied to two numbers — NOT to be confused with the address-dereference suffix) |
| `++` | Pre/post increment on a numeric variable |
| `--` | Pre/post decrement on a numeric variable |

### `+` is context-sensitive

- `number + number` → numeric addition
- `string + anything` → concatenation (the right operand is coerced to a string)
- `anything + string` → concatenation

There is **no `&` operator** for concatenation (that's AppleScript / VBScript / SQL). Use `+`:

```
local (s = "hello, " + userName + "!");        // ✓
local (s = "count = " + examples.count);       // ✓ — examples.count gets stringified
```

If both operands are numeric but you wanted concatenation, force a string first:

```
local (s = string (a) + string (b))            // ✓ "12" + "34" = "1234"
local (n = a + b)                              // ✗ this is 12 + 34 = 46
```

### `^` is ambiguous between power and dereference — context disambiguates

`a ^ b` between two numeric expressions is power. `adr^` as a postfix on an address is dereference. The parser distinguishes by position; you almost never see real ambiguity. If a script is doing math on a value also used as an address, name those variables clearly.

---

## Address operators

The two operators that define UserTalk's relationship to the ODB.

### `@` — address-of

`@x.y.z` produces an `addressType` value naming the path `x.y.z`. It does NOT resolve the path. The address is well-formed as long as the **parent path** exists:

```
local (adr1 = @examples.foo)                   // ✓ — address value; defined whether examples.foo exists or not
local (adr2 = @examples.foo.bar)               // ✓ if examples.foo is a table (parent path resolves)
local (adr3 = @nonexistent.thing.leaf)         // address value, but defined (adr3) is false
```

See `CLAUDE_PRIMER.md` § 2.2 and `records_and_tables.md` for the full `defined ()` semantics.

### `^` — dereference

`adr^` resolves the address and produces the value at that location. Runtime error if no object exists there:

```
local (val = adr^)                             // value at adr — errors if missing
```

Wrap in `try { ... }` if the target might not exist, or check `defined (adr^)` first.

### Common use cases

| Pattern | Why |
|---|---|
| `dialog.ask ("?", @result)` | Output parameter — verb writes back through the address |
| `local (adrThing = @examples.bar)` | Hold a reference for repeated dereference (cheaper than re-resolving) |
| `verb (@bigTable)` | Pass by address instead of copying the table value |
| `for adrM in @t {...}` | Table iteration — visitor is the address of each member |

---

## The `contains` operator (and friends)

`contains`, `beginsWith`, `endsWith` are **infix string operators**. There is **no `string.contains` verb**.

```
if userInput contains "yes" {...}              // ✓
if filename endsWith ".ut" {...}               // ✓
if path beginsWith "/Users/" {...}             // ✓
```

NOT:

```
if string.contains (userInput, "yes") {...}    // ✗ no such verb
```

These are plain substring tests — no wildcards, no regex. For wildcards, see `string.patternMatch` below.

`contains` also works on lists and (sometimes) records — it tests for value membership, not substring. Confirm with `typeof ()` first if you're not sure what you're holding.

---

## Pattern matching

`string.patternMatch (pattern, source)` does wildcard matching:

- `*` matches any sequence of characters (zero or more)
- `?` matches a single character

```
local (pos = string.patternMatch ("*.ut", "foo.ut"))      // pos == 1 (matched at position 1)
local (pos = string.patternMatch ("foo*", "foobar"))      // pos == 1
local (pos = string.patternMatch ("*bar", "foobar"))      // pos == 4
local (pos = string.patternMatch ("xyz", "foobar"))       // pos == 0 (no match)
```

**The return is a 1-based position, NOT a boolean.** Zero means no match; anything else is the position. Don't write `if string.patternMatch (...)` and expect Python-style truthiness — write `if string.patternMatch (...) > 0`.

For simple substring checks (no wildcards), use `contains`, which is cheaper and clearer.

---

## Assignment vs equality

`=` is **assignment** in statement position. `==` is **equality**. UserTalk does not have BASIC-style "single `=` is equality in expressions." Always:

```
x = 5                                          // assignment
if x == 5 {...}                                // equality test
```

Mixing these up usually gives a compile error rather than a silent bug, because `=` in an `if` condition is a syntax error — but be deliberate.

---

## The `with` block

`with path.to.table { ... }` brings all top-level members of the table into local scope inside the block:

```
with examples.colors {
    local (val = red);                         // reads examples.colors.red
    log (green)}                               // reads examples.colors.green
```

Useful for reading several members of the same table without repeating the prefix.

### Risks

**Shadowing.** Inside the block, names from the table shadow your outer locals:

```
local (x = "outer");
with examples.colors {
    x = "blue"}                                // if examples.colors.x exists, this writes to examples.colors.x
                                                // — NOT to your local x
log (x)                                        // your local x is still "outer"
```

If you have a `with` over a table you didn't fully audit, you might be writing to ODB members instead of locals. Use a different variable name in the outer scope, or avoid `with` over tables with unknown contents.

**The catastrophic anti-pattern**: `with` over a system table followed by `delete`:

```
with clock {delete (@now)}                     // ✗ deletes system.verbs.builtins.clock.now
                                                // — a glue script in the system root
```

`@now` inside the `with` resolves under `clock`, not at the top level. This has historically corrupted Frontier installs. Don't `delete` inside a `with` unless you've manually expanded the path and you're sure.

**Protocol-mode interaction.** Every `script:` block in protocol-mode yaml tests is wrapped in `with system.temp.FrontierREPL.variables { ... }`. Your script's top-level `local`s become members of that table, with all the shadowing risks above. This is the source of issue #624 (P0). Full details: `testing_patterns.md`.

---

## The `;` separator

`;` separates statements at the same outline level. It's how you put multiple statements on one logical line:

```
local (x = 1); local (y = 2); return x + y
```

In outline-stored scripts, the outline indentation also encodes block structure — but the stringified form uses `;` AND `{` / `}` to encode the same structure for the compiler. When you write inline UserTalk in a yaml `script:` block, you're writing the **stringified** form, so use `;` and `{` / `}` explicitly.

This matters for the `with`-wrapper trap (#624): `local (x = 1)\rreturn x` does NOT survive the wrapper, because the wrapper drops scope across `\r`. Use `;` to keep statements glued: `local (x = 1); return x`.

---

## `nameOf`, `typeof`, `sizeOf`, `string`

These four inspection verbs all operate on **values**, not addresses. When you hold an address, dereference first with `^`:

| Verb | What it returns | On an address (no `^`) |
|---|---|---|
| `nameOf (adr^)` | The member name in its parent table | `nameOf (adr)` returns the local variable name — usually not what you want |
| `typeof (val)` | OSType code (e.g. `'TEXT'`, `'tabl'`, `'reco'`, `'list'`, `'addr'`) | `typeof (@x)` always returns `'addr'` (the value's type IS address) |
| `sizeOf (val)` | Member/character count | Doesn't really apply — sizeOf on an address value is meaningless |
| `string (val)` | Stringified value (script source, number-as-text, etc.) | `string (@x.y)` returns the dotted path `"@x.y"` — NOT the value at that path |

The `string (val)` vs `string (@addr)` distinction is the most common slip — see `CLAUDE_PRIMER.md` § 2.3.

`typeof ()` returns an OSType (`'TEXT'`), **not a descriptive string** (`"string"`). Compare with the matching `*Type` constant from `system.compiler.language.constants`, or against a literal OSType:

```
if typeof (val) == stringType {...}            // ✓ stringType is the constant 'TEXT'
if typeof (val) == 'TEXT' {...}                // ✓ also works
if typeof (val) == "string" {...}              // ✗ always false — RHS is a multi-char string, not an OSType
```

---

## Idioms (greatest hits)

Ten one-liners (or near one-liners) that show off idiomatic UserTalk. Most of these solve "I want to be safe about something that might not exist" or "I want a quick walk of a structure."

```
// 1. Conditional assignment with default — when defined-or-default matters
if defined (@examples.foo) {local (val = examples.foo)} else {local (val = "default")}

// 2. Safe member read via try
try {local (val = examples.foo.bar)} else {local (val = "missing")}

// 3. Count members matching a predicate
local (n = 0); for adrM in @examples.things {if adrM^ contains "match" {n = n + 1}}; return n

// 4. Build a comma-joined string of member names
local (s = ""); for adrM in @examples.t {s = s + nameOf (adrM^) + ", "}; return s

// 5. Test a verb is callable before calling
if defined (@system.verbs.builtins.string.upper) {return string.upper ("hi")}

// 6. Sum a list of numbers
local (total = 0); for n in {1, 2, 3, 4} {total = total + n}; return total

// 7. Find first member whose value equals X
local (adrFound = nil); for adrM in @examples.t {if adrM^ == "X" {adrFound = adrM; break}}; return adrFound

// 8. Stringify a typeof for human-readable logging (OSType → text)
local (t = typeof (val)); if t == stringType {return "string"} else {if t == tableType {return "table"} else {return "other"}}

// 9. Create-if-missing pattern for a sub-table
if !defined (examples.thing) {new (tableType, @examples.thing)}

// 10. Compare by name across two tables — assumes ordered iteration
local (ok = true); for adrM in @examples.t1 {if !defined (examples.t2.[nameOf (adrM^)]) {ok = false; break}}; return ok
```

**On idiom 7**: `break` exits the innermost loop. UserTalk also has `continue` (skip to next iteration). Both work in `for` and `loop`.

**On idiom 8**: a `case` statement reads better than nested `if`/`else`. Used here to keep the example to one line.

**On idiom 9**: `new (tableType, @path)` creates the table at `@path`. The parent (`examples`) must exist; this verb does NOT create intermediate parent tables.

**On idiom 10**: this is technically a name-presence comparison, not a deep equality. Real "structures equal?" needs a recursive walk.

---

## See also

- `CLAUDE_PRIMER.md` — load first. Section 2 covers the five idioms you'll write most often
- `records_and_tables.md` — when you need the full picture on tables vs records vs lists vs addresses, and on iteration patterns
- `strings_and_text.md` — string-specific operators, Pascal-string vs `Handle` truncation, escaping rules (stub at time of writing)
- `testing_patterns.md` — the protocol-mode `with`-wrapper trap (#624) and yaml integration test constraints
- `debugging_workflow.md` — when an operator behaves unexpectedly and you want to step through it
