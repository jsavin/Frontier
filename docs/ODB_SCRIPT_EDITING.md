# ODB Script Editing Guide

How to safely create, modify, and verify UserTalk scripts in the Frontier object database.

---

## Golden Rules

1. **Edit `databases/Virgin.root`** for changes that should persist in builds. `Virgin.root` is the source of truth — `make dist` copies it to `dist/Frontier.root`. Edits to `databases/Frontier.root` are local only and will be overwritten by the next dist build.
2. **Always use `--protocol` mode** for ODB edits — never `-e`. Protocol mode supports multi-step operations (set, compile, save) without shell escaping issues.
3. **Always use `script.newScriptObject` or `op.newOutlineObject`** to install scripts/outlines — never raw `op.insert`. These verbs handle line ending normalization automatically.
4. **Always keep `.ut` files in sync** with ODB changes so reviewers (human and `/gate`) can see a meaningful diff — the binary `.root` change alone is unreviewable.
5. **Always verify scripts compile** after installing them.
6. **Always write integration tests** for new or modified verbs.

---

## Protocol Workflow

### Connecting

For **read-only inspection** (no on-disk changes):

```bash
frontier-cli --protocol --skip-startup --system-root databases/Virgin.root
```

Since issue #588, `--protocol --system-root` defaults to **read-only**. `fileMenu.save()` will fail with "system root is read-only" until you opt into mutation.

For **editing** (changes that persist to the .root file), use the stage-and-confirm wrapper:

```bash
tools/edit_virgin_root.sh
```

The wrapper copies `databases/Virgin.root` to `/tmp/frontier-edit-<hash>/Virgin.root`, spawns `frontier-cli --protocol --skip-startup --system-root <staged>`, and prompts before promoting the changed copy back over the canonical file. A killed session or runaway script cannot corrupt the source — worst case you discard the staged copy. See `tools/edit_virgin_root.sh --help` and issue #644 for the rationale.

**Emergency / experts only** — if you genuinely need to edit the canonical file in place (e.g., recovery work, scripted batch edits where the prompt would be in the way), invoke frontier-cli directly:

```bash
frontier-cli --protocol --skip-startup --system-root databases/Virgin.root
```

A leaked or killed session against the canonical file can corrupt `databases/Virgin.root` locally — recover via `git checkout databases/Virgin.root`.

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

## Editing Guest Databases

Guest databases (`databases/Guest Databases/apps/*.root`) ship with the dist build. To edit them, load the system root first (so system verbs like `script.newScriptObject` are available), then open the guest DB as a secondary database. Use the wrapper so the system root stays staged:

```bash
tools/edit_virgin_root.sh
```

(Emergency / experts only — direct invocation against the canonical file:)

```bash
frontier-cli --protocol --skip-startup --system-root databases/Virgin.root
```

```json
{"op":"script/eval","id":1,"params":{"expression":"db.open(\"databases/Guest Databases/apps/mainResponder.root\", false); return true"}}
{"op":"script/eval","id":2,"params":{"expression":"local (s = string.trimWhiteSpace(file.readWholeFile(\"/tmp/myscript.txt\"))); script.newScriptObject(s, @mainResponder.someVerb); return true"}}
{"op":"script/eval","id":3,"params":{"expression":"db.save(\"databases/Guest Databases/apps/mainResponder.root\"); return true"}}
{"op":"script/eval","id":4,"params":{"expression":"db.close(\"databases/Guest Databases/apps/mainResponder.root\"); return true"}}
```

Key points:
- Use `--skip-startup` to avoid spinning up unnecessary services
- The guest DB path is relative to the working directory (project root)
- `db.open(path, false)` opens for read-write (`false` = not read-only)
- Save the guest DB explicitly with `db.save` — `fileMenu.save()` only saves the system root
- Close the guest DB when done to release the file handle
- The source copies under `databases/Guest Databases/` are what `make dist` copies to `dist/`

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

Scripts use tab indentation. Each line's indentation can differ from the line above by **at most one level** (same, +1, or -1). Never skip levels.

**Basic code indentation:**

```
on myVerb (s)                    // level 0
    local (x = s)                // level 1 (ok: +1)
    if x == ""                   // level 1 (ok: same)
        return false             // level 2 (ok: +1)
    return true                  // level 1 (ok: -1)
```

**Sub-indented comments are valid** — this is a historical Frontier convention for change logs and documentation. Each comment block indents one level under its parent:

```
on myVerb (s)                    // level 0
    //3/24/26 by JES             // level 1 (ok: +1)
        //Added new feature.     // level 2 (ok: +1 from comment above)
    //8/16/98 by DW              // level 1 (ok: -1, new comment block)
        //Original implementation // level 2 (ok: +1)
            //Detail about impl  // level 3 (ok: +1)
    local (x = s)                // level 1 (ok: back to code)
```

**Never skip levels** — jumping +2 or more from the line above causes outline structure errors:

```
on myVerb (s)                    // level 0
        //double-indented        // level 2 — WRONG: +2 from level 0
```

**Why this matters:** Scripts are stored as outlines. Each indentation change creates a parent-child relationship in the outline hierarchy. Skipping levels creates malformed outline structure, and when stringified for compilation, extra `{` and `}` get inserted at the wrong places.

### Braces and Semicolons

When writing scripts as text (for `script.newScriptObject`), include `{`, `}`, and `;` as they appear in the stringified form. The script compiler parses these.

When writing outline content (for `op.insert` or `op.newOutlineObject`), **omit** braces and semicolons — they are added automatically when the outline is stringified for compilation.

---

## Checklist: Adding or Modifying a Verb

1. **Verify kernel verb is registered** (for kernel verbs):
   - Check `headless_sys_verbs.c` (or appropriate headless verb file) has the verb in its enum, switch case, and `ADD_VERB` registration
   - Check `kernelverbs.r` has the verb name

2. **Write the glue script** (`.ut` file first, as reference):
   - Comments under the eponymous handler: one level deeper per sub-block (never skip levels)
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

7. **Debug if needed** — use `debug/run` via protocol to step through and verify script behavior (see `docs/DEBUGGING_GUIDE.md` for the UserTalk debugger reference)

---

## SOP: Script Edits in a PR

When a PR involves changes to UserTalk scripts in `.root` databases, follow this procedure:

### 1. Create feature branch

```bash
git checkout -b feature/my-script-change
```

### 2. Edit the `.ut` file

Make your changes in the `.ut` file under `usertalk_scripts/`. This is the human-readable form that reviewers (human and `/gate`) will diff.

- No trailing newline after closing `}`
- Comments indented one level per sub-block (never skip levels)
- Use tab indentation

### 3. Install in Virgin.root via protocol

Recommended — use the stage-and-confirm wrapper (issue #644):

```bash
tools/edit_virgin_root.sh
```

Emergency / experts only — direct invocation against the canonical file:

```bash
frontier-cli --protocol --skip-startup --system-root databases/Virgin.root
```

```json
{"op":"script/eval","id":1,"params":{"expression":"local (s = string.trimWhiteSpace(file.readWholeFile(\"/path/to/.ut/file\"))); script.newScriptObject(s, @system.verbs.builtins.category.verbName); return true"}}
{"op":"script/eval","id":2,"params":{"expression":"fileMenu.save(); return true"}}
```

For guest databases, open them explicitly first:

```json
{"op":"script/eval","id":1,"params":{"expression":"fileMenu.open(\"/path/to/guest.root\", false); return true"}}
{"op":"script/eval","id":2,"params":{"expression":"local (s = ...); script.newScriptObject(s, @guestDbRoot.verbName); return true"}}
{"op":"script/eval","id":3,"params":{"expression":"fileMenu.save(\"/path/to/guest.root\"); return true"}}
```

### 4. Verify

```json
{"op":"script/eval","id":3,"params":{"expression":"string(category.verbName)"}}
{"op":"script/eval","id":4,"params":{"expression":"category.verbName(\"test input\")"}}
```

- Read back with `string()` — confirm CR line endings, no trailing whitespace
- Call the verb — confirm it compiles and returns expected result

### 5. Commit both files

```bash
git add usertalk_scripts/.../verbName.ut databases/Virgin.root
git commit -m "feat: Add/modify verbName"
```

Both the `.ut` file (reviewable diff) and the binary `.root` file (actual ODB change) must be committed together. Reviewers diff the `.ut` text; the `.root` binary carries the change into builds.

### 6. Write integration tests

Add tests to the appropriate YAML file under `tests/integration/test_cases/`.

### 7. Push and create PR

The standard `/doit` workflow applies from here.

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
