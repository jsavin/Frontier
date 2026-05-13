# Comments and Whitespace

Stub. The deep dive on `//` and `«…»` comment forms, outline vs text-source storage of comments, and the `\r`-as-canonical-line-terminator rules. Flesh out as questions arise.

---

## Status

**STUB — TODO**. Most of the content already exists scattered across:

- `CLAUDE_PRIMER.md` § 4 — comments inside yaml `script:` blocks
- `strings_and_text.md` — line endings, `\r` canonical, `\n` post-#586
- `SYNTAX.md` — top-level vs inside-block `//` rules, `«…»` legacy comments, **CORRECTED to remove the false `/* */` claim**
- `../ODB_SCRIPT_EDITING.md` — indentation, sub-indented comment blocks, dated-change-comment convention
- Agent def `usertalk-engineer.md` — parser constraints (no inline `//` inside blocks — though recent probing in CLAUDE_PRIMER.md drafting suggests this is overstated; `//` works in more positions than the agent def implies)

## Key facts to consolidate

- `//` consumes text to end-of-line. Valid in most positions including inside `{ }` blocks; the only confirmed failure mode is when `//` and `}` collide on the same physical line in some structural arrangements.
- `«…»` are legacy multi-line comments (bytes 0xC7/0xC8 in MacRoman). AVOID inside yaml `script:` blocks (YAML may mangle the bytes); OK in `.ut` files.
- **`/* */` is NOT a UserTalk comment.** Never use it. Causes either a compile error or (worse) silently miscompiles by parsing as a division-multiplication expression.
- `#` is not a UserTalk comment either.
- `\r` (CR, 0x0D) is the canonical line terminator. The ODB stores scripts with CR. `script.newScriptObject ()` normalizes CRLF/LF → CR on install.
- LF (`\n`) support was added in #586 for ease of working with files from Unix tools, but mixing CR and LF in the same script is unsafe.
- Outline-stored comments use `isComment=true` on the outline heading; the `//` prefix is stripped on import and added back on export. The text-source form always includes `//`.

## TODO topics

- Exact rule for `//` near closing `}` — probe and document
- Comment-line attributes in the outline (`flcomment`, etc.)
- Outline vs flat-text divergence: stringification adds `//` and structural braces; importing strips them — round-trip subtleties from #621
- Em-dash / extended-character handling in script source
- Whitespace inside `( )` parameter lists — UserLand convention (space after the verb but not inside the parens? Or inside too?)

---

## See also

- `CLAUDE_PRIMER.md`
- `SYNTAX.md`
- `strings_and_text.md`
- `../ODB_SCRIPT_EDITING.md`
