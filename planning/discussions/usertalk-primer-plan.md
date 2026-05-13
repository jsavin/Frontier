# Plan: UserTalk Primer for Claude (progressive-discovery doc)

**Status:** Plan only. Drafting will happen in a separate session.
**Author:** Claude, 2026-05-13 (session that surfaced #618, #621, #623).
**Motivation:** This session's evidence that I treat UserTalk as "C-ish, infer-the-rest" and burn time getting basic idioms wrong. See conversation context for the specific mistakes (string.contains vs `contains`, string.patternMatch wildcards, defined() call forms, string(adrScript^) vs string(@addr), records vs tables vs addresses, etc.).

## Goals

1. **Entry-point primer that fits in eager context.** I load it at the start of any session touching UserTalk. Under 300 lines so the cost of loading is small.
2. **Progressive discovery.** Primer links out to deeper files; I drill in only when the current task needs that depth. Avoids force-loading 5000 lines I won't use.
3. **Decoder for "I just saw X error / I want to do Y."** The fast path from "stuck" to "right idiom." Reduces probe-and-experiment loops.
4. **Durable across sessions.** Lives in the repo (not just my memory) so future agents and a fresh me get the same head start.

## Audience

- **Primary:** Claude (any session) doing one of: writing inline UserTalk in a yaml integration test, reading an existing .ut file to understand what it does, writing C kernel code that affects UserTalk semantics (parser, runtime, verb dispatch), debugging why a UserTalk script returns the wrong value.
- **Secondary:** human contributors who haven't internalized UserTalk's quirks. The primer should be useful to them too.
- **Not the audience:** the docserver reference. That's the verb-by-verb dictionary; the primer is the "language tour" that explains the *idioms* the dictionary assumes you already know.

## Structure

```
docs/usertalk/CLAUDE_PRIMER.md          ← entry point, ~250 lines
├── values_and_types.md                ← what addresses, records, tables really are
├── operators_and_idioms.md            ← contains, ==, ^, @, ;
├── strings_and_text.md                ← Pascal-strings, bigstring, " vs «
├── records_and_tables.md              ← field access, structure traversal
├── verb_invocation_patterns.md        ← kernel verbs vs .ut handlers, headless overloads
├── scripts_vs_handlers.md             ← `on foo()` vs top-level statements
├── comments_and_whitespace.md         ← « // # behaviors, structural sensitivity in .ut/yaml
├── testing_patterns.md                ← yaml integration tests, eval-trap, with-wrapper
└── debugging_workflow.md              ← protocol probe, debug/getSource, string(), op walk
```

`docs/usertalk/CLAUDE_PRIMER.md` is the only file I load eagerly. The rest are pulled in on demand when the primer points to them or a task names them.

## Primer outline (≈250 lines)

### Section 1: Mental model — "this is not Python or C"

About 30 lines. The 5–7 things that go wrong if you bring a Python/C mental model. Each one with one line of "what I'd assume" and one line of "what's true." Examples:

| What I'd assume | What's actually true |
|---|---|
| Strings are heap objects with identity | Strings are byte literals, pass-by-value; no aliasing |
| `string.contains(a, b)` is a method | `contains` is an infix operator; no method |
| `a.b` is field access on any value | `a.b` is sub-table lookup; records use different access |
| `defined(@x.y.z)` means "does x.y.z exist" | Parent resolution only: did x.y resolve to a table? |
| `f(x, y)` always takes positional args | Some verbs take addresses (`@table`), some take values, some take addresses-to-write-back |
| `;` is a statement terminator like C | `;` separates statements *within a level*; outline level encodes structure |

The point: list the assumptions so I can mentally compare against them when I'm about to write something.

### Section 2: The five things you'll write inline 90% of the time

About 50 lines. Recipes for the actual idioms with one canonical example each. Each recipe also says "what the wrong version looks like and why."

1. Substring check → `str contains "sub"`. NOT `string.contains(str, "sub")`, NOT `string.patternMatch("*sub*", str)`.
2. Existence check → `defined(dot.address.of.object)` or `defined(adrObject^)`. NOT `defined(@addr)` unless you specifically want parent-resolution.
3. Reading a script body → `string(adrScript^)` (or `string(scriptObject)` when you have the value directly). NOT `string(@addr)`.
4. Field access on a record → ??? (you'll fill this in; I don't actually know the right form yet — this is what tripped me on `menu.describe`)
5. Iteration → `for i = 1 to sizeOf(table) { … }`. NOT `for x in table` or `for (int i = 0; …)`.

This is the section I'd quote in my own outputs when I'm about to write inline UserTalk in a yaml script.

### Section 3: The top gotchas with decoder

About 80 lines. Failure-mode-indexed: when I see this error or unexpected output, look here.

- `Can't find a sub-table named X` — usually means tried `rec.field` on a record-typed value
- `Can't coerce the string X to a character` — passed multi-char string to a verb expecting `char` (single quote)
- `Can't call X because there aren't enough parameters` — verb has a headless overload with different signature; look in `menuverbs_headless.c` / `tcpverbs.c` for actual signature
- Script "succeeded" but `value: "true"` and the body actually failed — eval-trap (#618) — but also: `with` wrapper returns true even when its body's last statement was a no-op; check what the body's tail actually is
- `illegal character` — `#` or em-dash inside script body; also bare LF in some contexts (#586)
- patternMatch returns 0 even though substring is present — wildcards not supported; returns 1-based position
- Inline `{` and `}` on the same line in source — round-trip bug pre-#621; post-#621 should work but inline blocks are still subtle
- string mutated unexpectedly — strings are pass-by-value, you might be looking at a stale copy

For each: 2–3 line description, what the right form is, link to the deep doc if there's more.

### Section 4: Authoring UserTalk in yaml integration tests

About 50 lines. The "this is where I do most of my UserTalk writing" section. Specifically:

- The eval-trap is fixed (#618). Compile errors now surface as `success: false`.
- Use `expected_success: true` AND `expected_result: "true"` (quoted — yaml will treat `true` as boolean otherwise; the runner compares to the string the script returned)
- Comments: `//` only inside script bodies. NO `#`. NO `«»` (per `feedback_usertalk_comments.md`).
- Line continuation: scripts split by `\r` in yaml block scalars usually work; LF is OK post-#586.
- Multi-line strings: yaml folded scalars (`>`) vs literal (`|`) — use `|` for code.
- The with-wrapper: every yaml script runs inside `with system.temp.FrontierREPL.variables { ... }`. So locals declared in the script become attributes of that table during evaluation. Don't share state across tests (it does persist within a worker).
- Cleanup: `delete(@system.temp.X)` at the end of any test that creates state.

### Section 5: Pointers to deeper docs

About 20 lines. Each entry: one line on what's in that file, when to read it. Examples:

> `strings_and_text.md` — Read when: writing inline string literals with escapes; touching C code that takes a `bigstring`; debugging a "string truncated at 255 bytes" failure. Covers Pascal-string layout, bigstring vs Handle, `"..."` vs `«...»` quotes, escape sequences.

> `verb_invocation_patterns.md` — Read when: a verb call fails with parameter-count error; writing or reviewing kernel-side verb implementations; deciding whether an .ut wrapper is needed. Covers the kernel-verb / .ut-wrapper / headless-overload distinction.

> `testing_patterns.md` — Read when: writing or fixing an integration test. Covers the with-wrapper, skip block syntax, the false-green test history, behavioral vs source-inspection assertions.

## Deeper files — sketch only

I'll list per-file what content goes in each, leaving the actual prose for the drafting session.

### values_and_types.md
- The 5 fundamental kinds: numbers, strings, addresses, tables, records (and a quick mention of outlines/scripts as external types)
- `addressType` is first-class — what an `@x.y.z` value actually IS
- `tableType` is the namespace primitive; sub-tables nest
- `recordType` is the inline structure you get from verbs like `menu.describe` — different from tables, different access syntax
- typeof() returns OSType codes (`'TEXT'`, `'tabl'`, `'addr'`, `'reco'`, etc.) — `feedback_usertalk_comments.md` already touches this
- Coercion rules between types

### operators_and_idioms.md
- Comparison: `==` not `===`, no `!=` (use `not`)
- `contains` infix
- `not x` not `!x`
- `^` dereference
- `@` address-of
- `=` assignment AND `=` equality test in some places (yes really; how to disambiguate)
- `;` separator semantics in outline-encoded scripts vs flat eval

### strings_and_text.md
- Pascal-string layout: byte 0 is length, bytes 1..N are content
- `bigstring` is 256 bytes total (1 length byte + 255 content), the max-length-shortest-string type
- Long content uses `Handle` (heap-allocated, full length)
- 255-byte truncation as a recurring landmine (PRs #580, #481, etc.)
- String literal quote types: `"..."` and `«...»` (smart quotes, byte 0xD2/0xD3) — backslash escapes in both
- Escape sequences: `\"`, `\\`, `\r`, `\n`, `\t`, `\xNN`
- `cr` is the canonical line ending in UserTalk (CR, 0x0D); LF support was added in #586 but has caveats

### records_and_tables.md
- Records are inline structured values (typeof returns `'reco'`); tables are persistent ODB sub-tables
- Field access on tables: `table.field` works
- Field access on records: ??? (you fill this — it bit me on `rec.script contains "..."` failing with "Can't find a sub-table named rec")
- Probably involves `nameOf`/`nthField`-style accessors or a special syntax I haven't found

### verb_invocation_patterns.md
- Kernel verbs are C functions registered in a hashtable; UserTalk dispatches by name
- `.ut` wrappers in `usertalk_scripts/Frontier.root/system/verbs/builtins/...` are UserTalk handlers that either pass-through to the kernel via `kernel(verb)` or implement entirely in UserTalk
- Headless overloads in `*_headless.c` may take different parameter shapes than the legacy Mac signature
- Discovery: `grep` for the verb name in `Common/source/*verbs*.c`, then check headless overload
- When verbs return false: how to surface the error (langtraperror, the #618 mechanism)

### scripts_vs_handlers.md
- `on foo (a, b) { … }` declares a handler named `foo`
- Top-level statements outside any `on` are evaluated immediately when the script runs (e.g., `system.startup.startupScript`)
- A script object's `string()` form returns the source text; the runtime compiles on demand
- Calling a script: `address ()` — UserTalk auto-loads, auto-compiles, auto-invokes the first handler in the script
- Multiple handlers in one script: subsequent handlers are visible to the first one's scope
- Comment-line markers (`«`, `//`) at outline node start mark `flcomment=true` — affects compile

### comments_and_whitespace.md
- Three comment forms: `«…\r` (legacy, byte 0xC7), `//…\r` (modern), and chendcomment terminator
- Outline-stored scripts store comments differently than inline source — see scriptcommentvisit
- yaml: `#` works for yaml-level comments but NOT inside `script:` blocks (UserTalk-level)
- `\r` is canonical UserTalk line terminator; LF support per #586 but inconsistent in some paths
- Structural sensitivity: an `on foo () {` line at indent 0 with a `}` on a deeper-indent line — both characters are structural (parsed); the langstripstructuremarkers fix in #616/#621 ensures round-trip
- em-dash (U+2014) and other multi-byte UTF-8 — treated as illegal-character by scanner; #586 partially relaxed this for LF only

### testing_patterns.md
- The yaml runner wraps each `script:` body in `with system.temp.FrontierREPL.variables { ... }`
- `expected_success: true/false` — protocol-level success/fail
- `expected_result: "true"` — STRING comparison against script's return value (yaml unquoted `true` would be boolean and never match)
- `skip: true` + `skip_reason: "..."` — runner skips and reports
- Pre-#618, `langruntraperror` returning true on compile errors masked many test bugs as false-green; the test suite has 108 such tests skipped under #620
- Behavioral assertions vs source inspection: tests should `return computed_value` and assert via `expected_result`, NOT inspect the script source itself
- File-level metadata: `sequential: true`, `needs_guest_dbs: true`, etc. — when to use them
- Common pitfalls: `«…»` inside script body, `#` inside script body, untrimmed whitespace, residual state across tests

### debugging_workflow.md
- `script/eval` via protocol mode is the fastest probe — `--protocol --skip-startup --allow-mutate --system-root <db>`
- `debug/getSource` reads script source via `opgetlangtext` (post-export form, with `«` and structural braces re-emitted)
- `op.firstSummit / op.fullExpand / op.go(flatdown,1) / op.getLineText` walks outline nodes to see raw stored text — bypasses export pass
- `string(scriptObject)` returns the post-export form (same path as debug/getSource)
- `typeof(value)` returns the OSType — useful for "what kind of thing am I holding"
- `nameof(@addr)` returns the leaf name from an address
- `dialog.alert` doesn't work in headless — use `return` or `msg` (or in tests, just put the value in the script's return)
- When a yaml test fails inscrutably: probe the same expression via `script/eval` in a manual `frontier-cli --protocol` session; the error messages are more visible

## Drafting order (for the separate session)

1. Write the primer first — that's the load-bearing piece. Aim for 250 lines.
2. Fill in `records_and_tables.md` and `operators_and_idioms.md` next — those cover the most-bit places.
3. Fill in `strings_and_text.md` and `testing_patterns.md` — high reference value.
4. The rest can be stubs initially with TODO markers; flesh out as each topic comes up.

## Where to anchor it from

- `CLAUDE.md` (project) gets a new section "## UserTalk language primer (load before touching .ut or yaml-script work)" with a one-line pointer to `docs/usertalk/CLAUDE_PRIMER.md`.
- `docs/QUICK_REFERENCE.md` or `docs/AI_SHARED_GUIDELINES.md` adds a "Before writing UserTalk" pointer.
- The existing memory files (`feedback_usertalk_*`) can stay; they're tighter than the primer (one-rule-each) and remain useful as quick-recall items. Cross-link them from the primer.

## Success criteria

- After loading `CLAUDE_PRIMER.md`, I can write a yaml integration test using `contains`, `defined(adr^)`, record field access, and proper escape sequences without probing the runtime to find the syntax.
- For deeper questions, the primer points me at the specific deeper doc rather than the docserver verb-by-verb dictionary.
- The session-specific gotchas I learned this round (especially around the burndown tests) are findable via the failure-mode decoder in Section 3.
- The primer is short enough to load in every relevant session without context cost worth thinking about.

## What's explicitly out of scope

- Replacing or duplicating `docs/usertalk/docserver/` — that's the verb reference, a different artifact.
- Detailed coverage of every kernel-side implementation. Pointers to source files are fine; reproducing them is not.
- UserTalk parser internals — that's in `langscan.c` / `langparser.c` comments. The primer is consumer-facing.
- Mac-UI semantics. Headless-only.

---

When you've drafted it, ping me in the main session and I'll integrate it into CLAUDE.md / pointers / etc.
