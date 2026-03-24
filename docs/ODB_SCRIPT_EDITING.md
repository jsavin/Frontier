# ODB Script Editing Guide

How to safely create, modify, and verify UserTalk scripts in the Frontier object database.

---

## Golden Rules

1. **Always use `--protocol` mode** for ODB edits — never `-e`. Protocol mode supports multi-step operations (set, compile, save) without shell escaping issues.
2. **Always use `script.newScriptObject` or `op.newOutlineObject`** to install scripts/outlines — never raw `op.insert`. These verbs handle line ending normalization automatically.
3. **Always keep `.ut` files in sync** with ODB changes so the PR review bot can see the diff.
4. **Always verify scripts compile** after installing them.
5. **Always write integration tests** for new or modified verbs.

---

## Protocol Workflow

### Connecting

```bash
frontier-cli --protocol --skip-startup --system-root databases/Virgin.root
```

Use `Virgin.root` for edits that should be part of the distribution. Use `databases/Frontier.root` for local testing only.

### Installing a Script

Write the script source to a temp file, then use `file.readWholeFile` + `script.newScriptObject`:

```json
{"op":"script/eval","id":1,"params":{"expression":"local (s = string.trimWhiteSpace(file.readWholeFile(\"/tmp/myscript.txt\"))); script.newScriptObject(s, @system.verbs.builtins.sys.myVerb); return true"}}
```

### Saving

```json
{"op":"script/eval","id":2,"params":{"expression":"fileMenu.save(); return true"}}
```

### Verifying

Read back and check it compiles:

```json
{"op":"script/eval","id":3,"params":{"expression":"string(sys.myVerb)"}}
{"op":"script/eval","id":4,"params":{"expression":"sys.myVerb(\"test input\")"}}
```

---

## Script Source Format

### Line Endings

`script.newScriptObject` and `op.newOutlineObject` normalize line endings automatically:
- CRLF (`\r\n`) → CR (`\r`)
- LF (`\n`) → CR (`\r`)

The ODB stores CR internally. If you see `\n` in a `string()` readback, the script was installed incorrectly (bypassing the normalization verbs).

### Trailing Whitespace

**Trailing whitespace after the closing `}` causes compilation failures.** Always trim before installing:

- Call `string.trimWhiteSpace(s)` in the UserTalk install expression
- Also ensure temp files don't have trailing newlines: `perl -pi -e 'chomp if eof' /tmp/myscript.txt`

### Indentation

Scripts use tab indentation. The indentation level of any line can differ from adjacent lines by at most one level:

```
on myVerb (s)                    // level 0
    //comment                    // level 1 (ok: +1 from above)
    local (x = s)                // level 1 (ok: same as above)
    if x == ""                   // level 1
        return false             // level 2 (ok: +1 from above)
    return true                  // level 1 (ok: -1 from above)
```

**Never skip levels** — jumping from level 0 to level 2 will cause outline structure errors:

```
on myVerb (s)                    // level 0
        //double-indented        // level 2 — WRONG: +2 from above
```

### Braces and Semicolons

When writing scripts as text (for `script.newScriptObject`), include `{`, `}`, and `;` as they appear in the stringified form. The script compiler parses these.

When writing outline content (for `op.insert` or `op.newOutlineObject`), **omit** braces and semicolons — they are added automatically when the outline is stringified for compilation.

---

## Checklist: Adding or Modifying a Verb

1. **Verify kernel verb is registered** (for kernel verbs):
   - Check `headless_sys_verbs.c` (or appropriate headless verb file) has the verb in its enum, switch case, and `ADD_VERB` registration
   - Check `kernelverbs.r` has the verb name

2. **Write the glue script** (`.ut` file first, as reference):
   - Single-indented comments under the eponymous handler
   - No trailing newline after closing `}`
   - Use `PSTRING` (not `BIGSTRING`) for all string literals in C kernel code — compile-time length validation

3. **Install in the ODB** via protocol:
   - Write source to temp file (no trailing newline)
   - Use `script.newScriptObject` with `string.trimWhiteSpace`
   - Save with `fileMenu.save()`

4. **Verify**:
   - Read back with `string()` — check CR line endings, no trailing whitespace
   - Call the verb — confirm it compiles and returns expected result
   - Run integration tests

5. **Sync `.ut` file** to match ODB content

6. **Write integration tests** covering:
   - Happy path (verb returns expected type/value)
   - Error cases (empty input, wrong type)
   - End-to-end if applicable

---

## Why Not `-e` Mode?

The `-e` flag runs a single expression in a fresh process. Problems:

- **Shell escaping**: Double quotes, backslashes, and special characters in UserTalk conflict with shell quoting
- **Single operation**: Can't do multi-step workflows (set value, then compile, then save)
- **No session persistence**: Each `-e` invocation starts fresh — can't build on previous operations

Use `-e` only for quick read-only queries:

```bash
frontier-cli --skip-startup --system-root databases/Frontier.root -e 'string(sys.os)'
```

For anything that modifies the ODB, use `--protocol`.
