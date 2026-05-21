# UserTalk Syntax Reference

Quick reference for UserTalk syntax rules and gotchas.

For the full language tour and idioms, see [CLAUDE_PRIMER.md](CLAUDE_PRIMER.md).

---

## String Quotes

**CRITICAL**: Double quotes for strings, single quotes for character constants and OSTypes.

```usertalk
sizeOf ("hello")    // ✅ CORRECT — double quotes for strings
sizeOf ('hello')    // ❌ WRONG — syntax error (single quotes for single chars or 4-byte OSType only)
typeof (x) == 'TEXT'  // ✅ CORRECT — 'TEXT' is a 4-byte OSType code
```

This is the **opposite** of JavaScript/Python where `'x'` and `"x"` are equivalent. UserTalk treats single quotes as character constants (like C). A 4-byte single-quoted literal like `'TEXT'` is an OSType (a 4-byte unsigned integer commonly used for type tags).

---

## Comments

UserTalk has **two** comment markers:

1. `//` — single-line, to end of line (`\r`)
2. `«…»` — multi-line, using MacRoman bytes 0xC7 and 0xC8

**`/* */` is NOT a UserTalk comment marker.** Using `/* */` either fails to compile or, worse, silently parses as a division-multiplication expression and corrupts the script's return value. Never use it.

```usertalk
// ✅ Single-line comment at top level
local (x = 1)

on handler () {
    // ✅ Single-line comment between statements inside a block — OK
    local (x = 1);
    return x}                            // ✅ Comment after closing brace — OK

«this is a legacy multi-line comment that
spans multiple lines using the MacRoman
guillemet characters»
```

### Comments inside `{ }` blocks

`//` is valid in most positions inside `{ }` blocks. The confirmed failure is when `//` appears on the same physical line *before* a closing `}` — this can interact with the parser's brace tracking. Safe pattern: put `//` between statements OR after the closing `}`, not in front of it.

### Comments in yaml integration tests

- `//` OK between statements in yaml `script:` blocks
- `«…»` AVOID in yaml — the bytes 0xC7/0xC8 get mangled by YAML; OK in `.ut` files
- `/* */` NEVER — not a UserTalk comment
- `#` is a yaml comment outside scripts; UserTalk has no `#` comment

See [CLAUDE_PRIMER.md](CLAUDE_PRIMER.md) § 4 and [testing_patterns.md](testing_patterns.md).

---

## Closing braces and semicolons

Idiomatically, closing `}` braces are **inline at the end of the last statement** of a verb or bundle, NOT on their own line. Same for trailing `;`. This matches the outline ↔ text round-trip — outlines don't have free-standing braces.

```usertalk
on f (x) {
    local (y = x + 1);
    return y}                            // ✅ idiomatic: } inline at end of last statement

on f (x) {
    local (y = x + 1);
    return y
}                                        // ⚠️ non-idiomatic but compiles
```

---

## Verb call style: space before `(`

UserLand UserTalk style: a space between the verb name and the opening paren of its parameter list.

```usertalk
string (foo)                             // ✅ idiomatic
defined (path)                           // ✅ idiomatic
return f ()                              // ✅ idiomatic

string(foo)                              // compiles, but not the convention
```

---

## Indentation

### Blank Lines Inside Blocks

Blank lines inside indented blocks must match the surrounding indentation level. The parser tracks indentation strictly.

```usertalk
// ✅ CORRECT — blank line matches surrounding indent (or is omitted entirely)
on handler () {
    local (x = 1);
    return x}

// ❌ WRONG — blank line with zero indentation inside a block
on handler () {
	local (x = 1)

	return x}                            // parse error: blank line above has wrong indentation
```

**Best practice**: use compact formatting without blank lines inside `{ }` blocks. Blank lines are for separating top-level constructs.

### Outline structure rules

Scripts are stored as outlines. Indentation encodes parent-child relationships in the outline hierarchy. Each line's indentation can differ from the line above by **at most one level** (same, +1, or -1). Never skip levels.

Sub-indented comments are a valid Frontier convention for change logs:

```usertalk
on myVerb (s)                            // level 0
    // 3/24/26 by JES                    // level 1 (+1 from on-line)
        // Added new feature.            // level 2 (+1 from comment above)
    // 8/16/98 by DW                     // level 1 (back to comment block)
        // Original implementation       // level 2 (+1)
    local (x = s);                       // level 1 (back to code)
    return x
```

Never jump +2 or more levels — creates malformed outline structure.

See [docs/ODB_SCRIPT_EDITING.md](../ODB_SCRIPT_EDITING.md) for the full indentation rules.

---

## Historical Context

**Why these gotchas exist:**

UserTalk was designed for outline editing in the original Frontier environment:
- Indentation was automatic (handled by outline editor)
- Braces `{`, `}`, and semicolons `;` were rarely typed manually — added on stringification
- Text-based editing was not the primary workflow

Modern text-based development requires awareness of these parser constraints.

---

## See Also

- [CLAUDE_PRIMER.md](CLAUDE_PRIMER.md) — language tour, idioms, failure-mode decoder
- [TYPEOF.md](TYPEOF.md) — `typeof ()` semantics and OSType codes
- [records_and_tables.md](records_and_tables.md) — container types and access syntax
- [strings_and_text.md](strings_and_text.md) — string representations, 255-byte landmine
- [FILE_AND_DB.md](FILE_AND_DB.md) — file/DB verb path requirements
- [docs/TESTING_GUIDE.md](../TESTING_GUIDE.md) § UserTalk Syntax Guide
