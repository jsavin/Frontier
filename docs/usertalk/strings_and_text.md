# Strings and Text in UserTalk

Load this when a string is getting truncated, an escape isn't doing what you expect, encoding is mangling things, or you're staring at "Cant coerce the string X to a character" and need to know what shape the verb really wants.

Cross-references for things this file deliberately doesn't repeat:

- `CLAUDE_PRIMER.md` — entry point and quoting basics
- `operators_and_idioms.md` — `+` for concat, `contains` / `beginsWith` / `endsWith`
- `../ODB_SCRIPT_EDITING.md` — `string.trimWhiteSpace` requirement before installing, `PSTRING` vs `BIGSTRING` in C kernel
- `../TESTING_GUIDE.md` — single-quote vs double-quote error patterns

---

## 1. Two string representations

UserTalk has two string flavors that look identical at the script level but behave differently at the boundary with C:

1. **Pascal string** (`PSTRING`) — a 1-byte length prefix followed by up to **255 content bytes**. This is the dominant form for short strings, function parameters, and many verb returns. Max content length: **255**. Length 256 is unrepresentable in this form.

2. **`Handle`** (sometimes called `bigstring` in older kernel contexts) — heap-allocated, arbitrary-length. Used for script bodies, file contents, large WP text bodies, anything that obviously can't fit in 255 bytes.

The runtime sometimes converts between these silently. The boundary cases are where bugs hide: a script-level value that's been a `Handle` all along can be passed to a verb that internally re-marshals it as a `PSTRING`, and you lose bytes 256+ with no error.

You cannot directly inspect which form a value currently holds from script — `typeof (s)` is `'TEXT'` for both. The distinction only matters when something truncates or when reading kernel source.

---

## 2. The 255-byte truncation landmine

Many verbs that take a string parameter marshal it through the Pascal-string form. If your content exceeds 255 bytes, it gets **silently truncated** — no error, no log line, just shorter output. Related historical bugs: PRs #580, #481.

The pattern that bites:

```
local (s = "");
for adrM in @examples.bigTable {
    s = s + adrM^ + "\r"};
someVerb (s)                                  // s is 1200 bytes; verb sees 255
```

The string built fine. Concatenation in UserTalk produces a `Handle`-backed value once it outgrows 255. The truncation only happens when the verb on the receiving side coerces back to `PSTRING`.

### How to defend

- **Check `sizeOf (s)` before passing to a verb you don't trust:**

    ```
    if sizeOf (s) > 255 {
        scriptError ("too long for someVerb: " + sizeOf (s) + " bytes")}
    ```

- **Use file I/O for long content.** Round-trip through disk is awkward but reliable:

    ```
    file.writeWholeFile ("/tmp/buf.txt", s);
    local (back = file.readWholeFile ("/tmp/buf.txt"))
    ```

- **In C kernel code: prefer `Handle` over `PSTRING`** for verbs that may receive long strings. See `../ODB_SCRIPT_EDITING.md` for the `PSTRING` vs `BIGSTRING` rule and the compile-time length validation discipline.

If you're chasing a bug where a verb's output is exactly 255 bytes long, this is almost certainly what happened.

---

## 3. String literal forms

Three quote forms exist. Two are useful; one is a trap.

### 3.1 Double-quote — the only safe form

```
"hello"                                       // ASCII, the default
"line one\rline two"                          // embedded CR
"quote: \" inside"                            // escaped quote
```

This is the only literal form that's safe across all contexts including yaml integration tests. When in doubt, use double quotes.

### 3.2 Single-quote — character constant OR OSType, never a string

Single quotes do **not** make a string. They make either a 1-character constant or a 4-byte OSType code:

```
'X'                                           // 1-character constant
'TEXT'                                        // 4-byte OSType (a `long` literal: 0x54455854)
'hello'                                       // SYNTAX ERROR — neither 1 nor 4 chars
```

Common mistake:

```
string.upper ('hello')                        // ✗ 'hello' is 5 chars — syntax error
string.upper ("hello")                        // ✓
```

The error message is `Character constant isnt correctly specified. Must be of the form 'c'.` — confusing if you were trying to write a string.

`typeof` returns are OSTypes: `'TEXT'`, `'tabl'`, `'reco'`, `'list'`, `'long'`, `'bool'`. The protocol layer JSON-serializes these as 4-char strings, so in yaml expectations you write `"TEXT"`, not `stringType`.

### 3.3 Guillemets `«…»` — legacy MacRoman quote form

```
«hello»                                       // works in .ut files; AVOID in yaml script: blocks
```

The bytes are MacRoman 0xC7 / 0xC8. They render as French-style double quotes. They function as a string literal in `.ut` files and the outline editor, but yaml's byte handling can mangle them in `script:` blocks. The same characters double as the **legacy comment delimiters** (`«this is a comment»`), so the meaning depends on parser state.

Rule of thumb: never type guillemets into a yaml `script:` block. In `.ut` files they're fine, but `"..."` is cleaner.

---

## 4. Escape sequences

Inside double-quoted strings:

| Escape | Meaning |
|---|---|
| `\"` | literal double quote |
| `\\` | backslash |
| `\r` | CR (0x0D) — UserTalk's canonical line terminator |
| `\n` | LF (0x0A) — see "Line endings" below |
| `\t` | tab (0x09) |
| `\xNN` | hex byte |

`\r` is the one you reach for most — line breaks in script source and multi-line string output are CR-terminated.

---

## 5. Line endings: `\r` is canonical

UserTalk's canonical line terminator is **CR** (`\r`, 0x0D), inherited from classic Mac. Scripts in the ODB are stored with CR. Multi-line string literals embed `\r` between lines. When you `return` a string with multiple lines and read it back through the protocol, the separator is `\r`.

LF (`\n`) has historically been illegal in script source. The parser was relaxed in **#586** to treat bare LF as a line terminator inside `//` comments and as whitespace elsewhere — that change targets script source where the file got Unix-translated, not full escape-sequence equivalence with `\r`. Treat `\n` as parser-tolerated rather than first-class. Specifically:

- Don't rely on `\n` inside a string literal to behave identically to `\r` everywhere downstream — some verbs walk the string looking for CR explicitly.
- Don't mix CR and LF in the same source — pick one. The outline editor stores CR; if you import from Unix tooling, normalize before installing.
- yaml `script:` block scalars (`|`) deliver LF line breaks to the protocol layer. This currently works because the protocol/parser path normalizes; do not take this as a guarantee that LF is interchangeable with CR at every layer.

`script.newScriptObject ()` normalizes line endings (CRLF → CR, LF → CR) when installing scripts. Use it instead of raw `op.insert` so you don't store mixed terminators in the ODB.

(Judgment call: the exact #586 boundary on `\n`-in-string-literals isn't documented in the PR title, only the parser side. If you hit a case where `\n` in a literal misbehaves, treat it as a probe item, not a doc bug.)

---

## 6. Most-used `string.*` verbs

Quick reference. Full signatures in `docserver/`.

| Verb | Returns | Notes |
|---|---|---|
| `string.length (s)` | char count | same as `sizeOf (s)` for strings |
| `string.upper (s)` | string | ASCII case conversion |
| `string.lower (s)` | string | ASCII case conversion |
| `string.trimWhiteSpace (s)` | string | leading/trailing whitespace removed |
| `string.patternMatch (pattern, s)` | long | wildcards `*` and `?`; returns 1-based match position, 0 = no match |
| `string.replaceAll (s, find, replace)` | string | all occurrences |
| `string.nthCharacter (s, n)` | char | 1-indexed |
| `string.nthField (s, delimiter, n)` | string | split on delimiter, return nth (1-indexed) |
| `string.countFields (s, delimiter)` | long | number of fields |
| `string.padWithSpaces (s, width)` | string | right-pad to width |
| `string.lowerWord (s)` | string | lowercase first word |
| `string.upperFirstLetter (s)` | string | capitalize initial |

`string.trimWhiteSpace (s)` is **required** before installing script source — trailing whitespace breaks compile. See `../ODB_SCRIPT_EDITING.md` for the install discipline.

The infix operators `contains`, `beginsWith`, `endsWith` work on strings directly — no verb needed. See `operators_and_idioms.md`.

---

## 7. Common string-building idiom

```
local (s = "");
for adrM in @examples.colors {
    s = s + nameOf (adrM^) + ": " + adrM^ + "\r"};
return s
```

Notes:

- `nameOf (adrM^)` gives the **member name** (e.g. `"red"`). `nameOf (adrM)` would give `"adrM"` — almost never what you want.
- `+` is overloaded for string concat. The right-hand side is coerced to string if it isn't already.
- The accumulator outgrows `PSTRING` once you cross 255 bytes. Returning it through the protocol is fine; passing it to a length-sensitive verb is not. See section 2.

For long output, either:

- write to a file and return the path, or
- split into segments and return a list / record, or
- store under `system.temp.*` and have the caller read it back.

---

## 8. Encoding gotchas

- UserTalk historically used **MacRoman**. The modern headless build uses UTF-8 in most contexts, but the encoding model is not always explicit at the boundary — verbs may or may not preserve high-bit bytes faithfully.
- `«` (0xC7) and `»` (0xC8) are MacRoman characters that render as French-style double quotes. They double as both string-literal delimiters and the legacy comment form.
- Em-dash, smart quotes, and other non-ASCII characters may be rejected as `illegal character` by some scanner paths — particularly older glue scripts. If a paste from a word processor breaks compile, suspect smart-quoted text or em-dash.
- LF (`\n`) was historically illegal in script source; relaxed in #586 but not uniformly across all parser paths (see section 5).

When in doubt: stick to ASCII in inline script bodies. For content that needs broader encoding, write to a file with `file.writeWholeFile ()` and read back with `file.readWholeFile ()` — those paths preserve bytes.

---

## 9. Inspection patterns

```
typeof (s)                                    // 'TEXT' for both PSTRING and Handle forms
sizeOf (s)                                    // byte count — use this to check the 255 boundary
string.length (s)                             // char count (same as sizeOf for ASCII)
string (val)                                  // stringify a value (NOT an address)
```

Watch out for the address-vs-value trap from the primer: `string (val)` operates on the value. If you have an address, dereference: `string (adr^)`. `string (@some.path)` returns the literal string `"@some.path"`, not the value at that path.

---

## 10. Failure-mode quick decoder

| Symptom | Cause | Fix |
|---|---|---|
| `Character constant isnt correctly specified. Must be of the form 'c'.` | Used `'foo'` for a string | `"foo"` |
| `Cant coerce the string X to a character.` | Passed multi-char string to verb expecting `char` | `'X'` for a real char constant, or use the 1-char string form the verb expects |
| Output exactly 255 bytes long | `PSTRING` truncation at a verb boundary | Section 2 — file I/O or splitting |
| `illegal character` on paste | Smart quotes, em-dash, or other non-ASCII bytes | Normalize to ASCII |
| Compile fails on script with trailing whitespace | Missing `string.trimWhiteSpace` before install | Add the trim — `../ODB_SCRIPT_EDITING.md` |
| String concat returns wrong type | Right-hand side is a non-stringifiable value (table address, etc.) | `string (val)` it first; dereference any addresses |

---

## 11. When to escalate

- If you suspect a `PSTRING` truncation in C kernel code, that's a `system-architect` question.
- If a verb's marshaling is mis-typed (declared `PSTRING` but should be `Handle`), file an issue and tag it as a 255-byte landmine.
- If `\n` behaves inconsistently across paths, file a probe item — the #586 fix targets parser, not literal-escape semantics, and there may be follow-on work needed.
