# Scripts vs Handlers

Stub. The "what's the difference between a script with top-level statements and a script with `on foo ()` handlers?" reference. Flesh out as questions arise.

---

## Status

**STUB — TODO**. Adjacent content lives in:

- `CLAUDE_PRIMER.md` § 1 (the "verb dispatch through the ODB" mental-model row)
- `verb_invocation_patterns.md` — glue scripts as the "verbs are scripts at an ODB path" story
- `operators_and_idioms.md` — `with` block scoping
- `../ODB_SCRIPT_EDITING.md` — installing scripts via `script.newScriptObject`
- Agent def `usertalk-engineer.md` — execution model, scope precedence

## TODO topics

- `on foo (a, b) { … }` syntax — declaring a named handler
- Top-level statements outside any `on` — evaluated when the script runs (used in `system.startup.startupScript` etc.)
- Calling a script via address: `adrScript^ (args)` — auto-load, auto-compile, auto-invoke the first handler
- Multiple handlers in one script — visibility rules between handlers in the same script
- Comment-line markers (`«`, `//`) and `flcomment=true` outline attribute
- The dated-change-comment convention (newest first, indented under eponymous handler)
- Script vs verb vs handler — vocabulary disambiguation
- Late-binding verb resolution (compiles clean, fails at dispatch — see `testing_patterns.md` for the testing implications)
- Persistent globals (`global (foo)`, `persistent (bar)`) — when to use, ODB storage
- `local (x = initialValue)` initialization patterns

---

## See also

- `CLAUDE_PRIMER.md`
- `verb_invocation_patterns.md`
- `../ODB_SCRIPT_EDITING.md`
- `testing_patterns.md` — handler-needs-end-to-end-execution section
