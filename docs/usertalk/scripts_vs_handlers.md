# Scripts vs Handlers

Pulled in when: you're confused about `on foo () {...}` vs top-level statements, you want to understand how a verb call resolves to a script in the ODB, or you're debugging "the name X hasn't been defined" and want to trace the dispatch chain.

---

## Vocabulary

Three overlapping terms — know the distinction:

- **Script object**: a value of `scriptType` stored at a path in the ODB. It contains compiled UserTalk. `string (scriptObject)` shows its source.
- **Handler**: a named, callable unit of code declared with `on name (params) { ... }`. A script object may contain one or more handlers, plus top-level statements.
- **Verb**: a handler that's callable from anywhere by dotted name — e.g. `string.upper`. The verb name is the ODB path to the script object that contains the handler.

When you call `string.upper ("hello")`, the runtime:
1. Resolves the name `string.upper` through the ODB to find the script object at `system.verbs.builtins.string.upper`
2. Compiles the script if not already compiled
3. Invokes the first handler it finds (conventionally named to match the leaf path)

---

## Script structure

A script object's source is text. When stringified it looks like:

```
on upper (s) {
	return (string.upper (s))}
```

- `on name (params) { ... }` declares a **handler**. The handler runs when the script is invoked by that name.
- Everything outside an `on` block is a **top-level statement** — it runs on the first call to the script, before any handler dispatch. Used for initialization, startup scripts (`system.startup.startupScript`), and one-shot scripts.
- A script with only top-level statements (no `on` block) runs those statements when invoked from any call site.

---

## Calling a script by address

```
local (adrVerb = @system.verbs.builtins.string.upper)
local (result = adrVerb^ ("hello"))         // ✓ dereference + call
```

Dereferencing an address and calling the resulting script object invokes it. The runtime auto-compiles if needed. The handler name dispatched is the first `on` block in the script.

---

## Multiple handlers in one script

A script can contain multiple `on` blocks:

```
on init () { ... }
on cleanup () { ... }
on doWork (x) { ... }
```

Calling `system.temp.myScript.init ()` invokes the `init` handler. Calling `system.temp.myScript ()` (no handler name specified) invokes the first handler found.

Visibility between handlers in the same script: handlers in the same script can call each other by name directly, without the full ODB path. The name resolution searches the script's own scope before searching the ODB.

---

## Late binding: the hidden runtime failure

A script that **compiles cleanly** can still fail at runtime if it calls a name that doesn't resolve at dispatch time. Examples:

- Script calls `target.set (...)` — compiles, but `target.set` might not be installed in the headless build.
- Script calls a verb that was registered after the script was compiled.
- Script calls a name that only exists in a guest database that isn't open.

This is why **tests must drive handlers through their production dispatch path** and assert on observable side effects — not just `script.compile (s) == true`. See `testing_patterns.md`.

---

## `global` and `persistent` declarations

Inside a handler, variables default to local scope. Two special declarations extend lifetime:

```
global (sharedVar)         // sharedVar is shared across all scripts in this Frontier session
persistent (cachedVal)     // cachedVal persists between calls to THIS script object
```

`global` variables live for the session. `persistent` variables survive across calls but not across Frontier restarts. In headless/protocol mode, `global` and `persistent` behave the same as in interactive mode — they're stored in the ODB's thread globals and the script's persistent-storage slot respectively.

Avoid `global` in new code where possible — it's shared mutable state that makes reasoning about concurrent execution difficult (see ADR-014 on the GIL model).

---

## The dated-change-comment convention

When modifying a handler, add a comment in the format:

```
on myVerb (arg) {
    // 2026-05-22 JES — added null check for arg
    if arg == nil {return (false)}
    ...}
```

Date first (ISO 8601), then initials, then brief description. Newest changes at the top of the handler. This convention is enforced by code review, not the compiler.

---

## See also

- `verb_invocation_patterns.md` — how name resolution finds the ODB path, kernel verb vs glue script distinction, dispatch chain
- `testing_patterns.md` — handler end-to-end execution requirement; why compile-check is not enough
- `operators_and_idioms.md` — `with` block scoping (overlaps with top-level script scope)
- `../ODB_SCRIPT_EDITING.md` — installing and editing scripts via protocol; `script.newScriptObject` workflow
- `CLAUDE_PRIMER.md` § 1 — the "verb dispatch through the ODB" mental-model row
