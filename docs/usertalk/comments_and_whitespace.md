# Comments and Whitespace

Pulled in when: you're hitting a parse error you suspect is a comment, you're confused about CR vs LF in a script, or you want to understand how comments survive the outline ↔ flat-text round-trip.

---

## Comment forms

UserTalk has two comment forms. `/* */` is not one of them.

### `//` — line comment

Consumes from `//` to the end of the physical line. Valid in most positions, including inside `{ }` blocks:

```
local (x = 5); // ✓ between statements
if x > 0 {     // ✓ after opening brace
    log (x)}   // ✓ on a statement line before the closing brace (} on same line is fine here)
```

**The one confirmed failure mode**: `//` and `}` on the same physical line, where the `}` closes a block and the `//` comes BEFORE it:

```
if x > 0 {log (x) // comment}    // ✗ COMPILE ERROR — } after // on same line
if x > 0 {log (x) // comment
}                                 // ✓ } on the next line — works
if x > 0 {log (x)}               // ✓ no comment — fine
```

The parser sees the `//` as consuming the rest of the line including the `}`, so the block is never closed.

**In yaml `script:` blocks**: the same rule applies. If you write an inline block that ends with `// comment}` all on one physical line of YAML, it will fail to compile.

### `«…»` — delimited comment

Legacy multi-line comment syntax. `«` is 0xC7 (MacRoman left double guillemet), `»` is 0xC8.

```
«This is a comment that can span multiple lines»
```

**In `.ut` files**: fine.

**In yaml `script:` blocks**: AVOID. YAML may mangle the extended bytes, producing a compile error or silent corruption. Use `//` instead.

### What is NOT a comment

| Syntax | What UserTalk actually does with it |
|---|---|
| `/* ... */` | Silently miscompiles — parsed as divide-then-multiply, corrupting your script's return value, or produces a compile error. NEVER USE. |
| `#` | Not a comment. `#` inside a UserTalk script is a syntax error. |

---

## Line terminators

`\r` (CR, 0x0D) is the canonical UserTalk line terminator. The ODB stores scripts with CR between statements.

- `script.newScriptObject ()` normalizes CRLF (`\r\n`) and LF (`\n`) → CR on install.
- `\n` (LF) support was added in #586 so scripts loaded from Unix files work. The runtime accepts LF in most positions.
- **Mixing CR and LF in the same script is unsafe.** Pick one. In `.ut` files checked into the repo, use LF (Unix convention) and let `script.newScriptObject` normalize on install.

**In yaml integration tests**: the YAML parser delivers the script to the runtime as a flat string. Literal `\n` sequences in YAML block scalars become actual LF bytes. The runtime accepts these since #586.

**The with-wrapper and `\r`**: Issue #624 (P0) is specifically about `local` scope being dropped across line boundaries under the protocol with-wrapper. The fix is `;` to join statements rather than relying on cross-line scope — not about CR vs LF, but related to how line boundaries are handled.

---

## Comments in the outline vs flat-text

Scripts are stored in the ODB as outline objects. Each line of the script is an outline node. A line beginning with `//` gets the `flcomment=true` attribute on its outline node — the `//` is stripped in the outline representation and restored on stringification.

In practice:
- If you read a script body with `string (scriptObject)`, you see the `//` prefix on comment lines.
- If you install a script with `script.newScriptObject` from a string that has `//` comments, the outline nodes get `flcomment=true` set automatically.
- The round-trip is reliable — `//` in → `flcomment` attribute → `//` out.

`«…»` comments become a single outline node with `flcomment=true` as well. The round-trip for `«…»` through the flat-text form uses `//` — so `«This comment»` round-trips as `// This comment` in the stringified form.

---

## UserLand whitespace conventions

- **Space before `(`** in verb calls: `string.upper ("hello")` not `string.upper("hello")`. Both compile; only the spaced form is idiomatic.
- **Closing `}` inline**: the last statement in a block ends with `}` on the same physical line — `{local (x = 5); return (x)}`. Closing `}` on its own line is non-idiomatic (and if followed by a `//` comment on the previous line, causes a compile error).
- **Tab indentation**: the ODB outline editor uses tabs for indentation. The plain-text stringified form uses tabs too. Never use spaces for indentation in UserTalk scripts.

---

## See also

- `CLAUDE_PRIMER.md` § 4 — comments inside yaml `script:` blocks
- `strings_and_text.md` — line endings and encoding in depth
- `SYNTAX.md` — grammar-level rules for comment placement
- `../ODB_SCRIPT_EDITING.md` — dated-change-comment convention, indentation rules
