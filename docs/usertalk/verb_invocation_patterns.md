# Verb Invocation Patterns

Pulled in when: a verb call errors with "wrong number of parameters", behavior diverges from the docserver reference, or you're tracking down why `string (string.length)` looks different from what you expect.

The premise: a UserTalk "verb" isn't one thing. `string.length` is a three-layer dispatch — ODB path, glue script, C kernel — and burndown bugs cluster at the seams between layers. Knowing which layer is in play (and which one is lying to you) is the actual skill.

The primer's Section 1 says "UserTalk is dispatch-by-name through the ODB" in one line. This file unpacks that line into the diagnostic toolkit.

---

## 1. The three layers

A verb call like `string.length ("hello")` traverses three layers, in order:

### Layer 1: name resolution

`string.length` is a dotted path. The runtime walks `system.paths` looking for a table where the dotted path resolves. In practice that's `system.verbs.builtins.string.length`. The value at that path is a `scriptType` — a compiled UserTalk script object stored in the ODB.

Name resolution doesn't care about parameters or behavior; it just produces a script value.

### Layer 2: glue script execution

The `scriptType` object compiles (if not already cached) and runs with the call-site's arguments. Most glue scripts are thin pass-throughs:

```
on length (s) {kernel (string.length)}
```

Some glue scripts do real work — parameter validation, default-value injection, type coercion, post-processing of the kernel's return. The `on <name> (...)` declaration in the glue script is the **authoritative parameter list** for what UserTalk callers can pass.

### Layer 3: C kernel implementation

`kernel (<callname>)` is a special form that hands control to the C verb table. The callname argument is not arbitrary — it identifies an entry in the dispatch table that the headless build registered at startup. The C implementation lives somewhere under `Common/source/` — often in a file named after the verb family (`stringverbs.c`, `tableverbs.c`, etc.) or in a `*_headless.c` companion.

The C implementation reads its parameters off the parameter list the glue script forwarded. If the glue script's `on` line and the C implementation's parameter reads don't agree on count or order, you get the "wrong number of parameters" runtime error.

---

## 2. Why this layering causes burndown bugs

Three failure modes recur often enough to be worth naming:

1. **Glue/kernel signature drift.** The glue script's `on` line says `(s, n, opts)` but the C kernel only reads two parameters. Caller passes three, glue forwards three, kernel errors. Or vice versa.
2. **Headless overload.** A verb has a Mac-era C implementation that took complex types (a `filespecType`, a `WindowPtr`, an `RGBColor`). The headless build registers a different implementation under the same name with a simpler signature. The glue script may or may not have been updated to match.
3. **Docserver vs reality.** The docserver was generated from the legacy Mac codebase. Some verbs have been stubbed, no-op'd, or signature-changed in the headless build and the docserver still shows the old shape.

Whenever a verb's behavior surprises you, the first question is **which layer is wrong** — not "what's the bug in the kernel."

---

## 3. Discovering a verb's actual signature

Walk the layers, top down:

### Step 1 — read the glue script

```
local (s = string (string.length))
```

This returns the script source text. The `on <name> (...)` line at the top is the parameter list as UserTalk sees it. The body shows you whether it's a thin `kernel ()` pass-through or has its own logic.

Remember: `string (verb.path)` takes the **value**, not the address. `string (@string.length)` returns the stringified address — useless for inspection. See the primer Section 2.3.

### Step 2 — find the C kernel implementation

If the glue script ends in `kernel (someName)`, grep `Common/source` for that callname. The grep target is the literal symbol inside `kernel ()` — often it matches the verb path (`string.length`) but not always. Some kernel callnames are short tags like `getstringlength` that bear no obvious relation to the ODB path.

The C function dispatched to is typically a `boolean` returning function that reads parameters off a parameter list (`getstringvalue (hparam1, &h)` or similar) and pushes a return via `setbooleanvalue` / `setstringvalue` / etc.

### Step 3 — check for a headless overload

Search `*_headless.c` files for the same callname or for `ADD_VERB` registrations naming the verb. The headless build registers verbs through a different table than legacy Mac; a verb can be present in both with different signatures, and the headless registration wins at runtime in the headless build.

A symptom that points at this: docserver says the verb takes 4 parameters, glue script `on` line says 4, but the call errors at 4 and works at 3. The headless C dropped a parameter; the glue script may not have been updated.

---

## 4. The `kernel ()` bridge

`kernel (callname)` inside a glue script invokes the C verb table entry registered under `callname`. A few properties of `kernel ()` worth knowing:

- The callname is **not** a UserTalk identifier resolution — it's a symbolic tag into the C dispatch table. You can't redefine it from UserTalk.
- The callname doesn't have to match the verb's ODB path. `string.upper` could in principle dispatch to a callname like `uppercasestring`. In practice most verbs use matching names, but don't assume.
- The kernel call reads its parameters from the **current glue script's parameter list** — whatever the glue's `on` line declared, scoped at the time of the `kernel ()` call.
- The kernel call returns into the glue script's expression context. Glue scripts often end with `kernel (...)` as the last expression so its return becomes the glue's return.

A glue that does more than pass through:

```
on length (s, fastPath=false) {
    if fastPath {
        return (sizeOf (s))};
    return (kernel (string.length))}
```

When you see a glue like this, the C kernel is only one of two paths. Failures in the glue path won't reach the kernel; failures in the kernel won't trip glue-side validation.

---

## 5. Address-versus-script-reference gotchas

A class of bug that consistently bites:

```
local (s = string (string.length));      // ✓ value form — returns the script's source text
local (s = string (@string.length));     // ✗ returns the stringified address "system.verbs.builtins.string.length"
```

`@string.length` is an address VALUE. `string ()` doesn't dereference — it stringifies. The address stringifies to its dotted path. You'll get back a string that looks superficially plausible but is not the script source. `typeof ()` on the result is `'TEXT'`, not `'scpt'`.

Always dereference when you have an address and want to inspect the target:

```
local (adr = @string.length);
local (s = string (adr^))                // ✓ dereference, then inspect
```

This is the same value-vs-address rule the primer covers for `string ()` / `typeof ()` / `nameOf ()`. Verb introspection is the place it stings most often because the verb path naturally reads like a callable.

---

## 6. Calling a verb dynamically by address

You can store a verb reference as an address and invoke through it:

```
local (adr = @string.length);
local (n = adr^ ("hello"))               // ✓ dereference then call
```

The `adr^` produces the script value; the `(...)` then calls it. This is how dispatch tables, callback registrations, and some menu wiring work — store the address, dereference and call at invocation time.

If you forget the `^`:

```
local (n = adr ("hello"))                // ✗ tries to call an addressType value — runtime error
```

---

## 7. When a verb fails with "name hasn't been defined"

Error: `Can't call the script because the name X hasn't been defined.`

The likely causes, in roughly decreasing frequency:

1. **System root not loaded.** Verbs live under `system.verbs.builtins.*`; without `--system-root databases/Frontier.root` on the `frontier-cli` invocation, that subtree doesn't exist and name resolution comes up empty. This is the #1 cause and worth checking first every time.
2. **Late-binding miss.** UserTalk compiles a script even when a called name doesn't yet resolve — failures only surface when the script runs and the name still doesn't resolve. The compile-time check is purely syntactic. Use the protocol debugger to step to the failing call. See `debugging_workflow.md`.
3. **Path-leaf collision with a global or keyword.** If your verb path ends in a leaf like `stringType`, `contains`, `tableType` — the parser sees the global, not your member. Bracket form bypasses: `@my.path.["myMember"]`. The primer Section 2.4 covers this.
4. **Verb is in the `.ut` file but not installed in the ODB.** Editing a `.ut` file alone doesn't install anything. Install via protocol with `script.newScriptObject` and `fileMenu.save`. See `../ODB_SCRIPT_EDITING.md`.

If none of these explain it, the verb may have been renamed or removed in headless. Check the docserver, then verify against `system.verbs.builtins` walked at runtime.

---

## 8. Decoder: "wrong number of parameters"

Error: `Can't call X because there aren't enough parameters.` (Or "too many.")

The diagnostic sequence:

1. **Read the glue script** — `string (verb.path)` and look at the `on <name> (...)` line. Note required vs default-valued parameters.
2. **Check for a headless overload** — grep `*_headless.c` for the callname inside the glue's `kernel ()` invocation. Compare the parameter reads in the C code against the glue's `on` line.
3. **Confirm the call site** — count what you're actually passing. UserTalk's "too few" message can fire on an off-by-one in a comma-separated literal that's harder to eyeball than you'd think.
4. **Suspect recent refactoring.** If the verb was touched recently, the glue and the C may have diverged. `git log -- Common/source/<file>` and `git log -- usertalk_scripts/Frontier.root/system/verbs/builtins/<path>.ut` tell you who moved last.

A specific anti-pattern: a glue script declares a parameter with a default value (`on length (s, mode="fast")`), the caller relies on the default, and the kernel implementation doesn't know about `mode` and reads only one parameter. The default lets the call shape look right but the kernel does the wrong thing. Read the glue body, not just the `on` line.

---

## 9. Headless overload patterns

Some verbs that took UI-flavored parameters on legacy Mac have been simplified or stubbed in headless. Treat the list below as **patterns to look for**, not authoritative claims — verify each against the current code before relying on the behavior.

- **`dialog.alert ()` and friends.** On Mac, pops a UI alert. In headless, may be a no-op or return immediately without surfacing output. Don't rely on it to communicate results — return values or write to a temp path you can read back. The primer Section 5 makes the same point.
- **File verbs.** On Mac, some accept `filespecType` parameters that carry resource fork metadata, alias info, etc. In headless, many accept plain string paths instead. The glue may have been kept signature-compatible by stringifying internally, or it may have been changed. Read it.
- **Menu verbs.** On Mac, attach behaviors to actual menus. In headless, may operate purely on data structures (`menu.list`, `menu.describe`) without any UI side effect. Behavioral assertions on these need to inspect the data, not "did the menu update."
- **Window/editor verbs.** On Mac, manipulate live windows. In headless, often stubbed, no-op'd, or restricted to a virtual window list.

The asymmetry: a verb that's a no-op in headless still **succeeds** — the call returns cleanly, just without the side effect you wanted. Tests that wait for an alert dialog will hang. Tests that assert on a return value will get the no-op's default (usually `true`).

When porting an integration test that worked on Mac, the most productive first hypothesis for an unexplained failure is "this verb is a headless overload that no-ops."

---

## 10. Adding a kernel verb (summary)

The full SOP is in `../VERB_IMPLEMENTATION_GUIDE.md` and `../ODB_SCRIPT_EDITING.md`. Pulling it together for context:

1. Implement the C function in the appropriate `Common/source/*verbs*.c` file.
2. Register in `headless_*_verbs.c` — enum, switch case, `ADD_VERB` call.
3. Register in `kernelverbs.r`.
4. Write the glue script and place it under `usertalk_scripts/Frontier.root/system/verbs/builtins/...`.
5. Install in the ODB via protocol (`script.newScriptObject` + `fileMenu.save`). Editing the `.ut` file alone isn't enough.
6. Sync the `.ut` file so source-control reflects the ODB state.
7. Write integration tests that drive the verb end-to-end through the dispatch path.

The protocol-based ODB editing workflow is the only supported way to install a verb. Do not edit `.root` files directly.

---

## 11. Cross-references

- `CLAUDE_PRIMER.md` — the entry primer; Section 1 names the dispatch model, Section 2.3 covers `string ()` value-vs-address
- `debugging_workflow.md` — protocol debugger for stepping through verb dispatch and inspecting kernel/glue boundaries
- `records_and_tables.md` — how `system.verbs.builtins.*` is laid out as nested tables; what `string ()` does to non-script values
- `../VERB_RESOLUTION_ARCHITECTURE.md` — name resolution and dispatch details at the C level; `langhandlercall ()` / `langgethandlercode ()` paths
- `../VERB_IMPLEMENTATION_GUIDE.md` — kernel verb implementation in C, the full registration walk
- `../ODB_SCRIPT_EDITING.md` — installing glue scripts via protocol, `script.newScriptObject`, `.ut` sync
- `docserver/` — verb-by-verb reference; **caveat**: generated from Mac sources, may not reflect headless overloads
