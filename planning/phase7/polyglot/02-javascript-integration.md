# JavaScript Integration Specification

Status
- State: Draft
- Phase: Phase 7 — Polyglot Scripting
- Last Updated: 2026-02-13
- Owner: Jake
- Notes: Specification only. Engine choice deferred to POC phase.

Related Docs
- `planning/phase7/polyglot/00-polyglot-vision.md` — Vision & Architecture Overview
- `planning/phase7/polyglot/01-language-interface-spec.md` — Abstract Language Interface
- `planning/phase7/polyglot/03-python-integration.md` — Python Integration
- `planning/phase7/polyglot/04-compiled-extensions.md` — Compiled Extensions
- `Common/headers/lang.h` — `tyvaluerecord`, `tyvaluetype`
- `Common/source/kernel_verbs_headless.c` — EFP registration pattern

Change Log
- 2026-02-13: Initialized document.

## Overview

JavaScript is the most widely-used scripting language in the world. Adding JS as a first-class Frontier scripting language opens the platform to the largest possible developer community and enables use of the vast npm ecosystem.

This document specifies how a JavaScript engine integrates with Frontier through the `FrontierLanguage` interface defined in `01-language-interface-spec.md`. It covers engine candidates, type mapping, the `frontier` global API, module system, and async considerations.

### Goals

- Run JavaScript scripts stored in the ODB or on the filesystem
- Expose the Frontier kernel API as a JS global object (`frontier`)
- Support bidirectional type conversion between Frontier values and JS types
- Operate within Frontier's GIL model (synchronous-first)
- Defer engine choice to POC results

## Decisions

### D1: Engine Choice Deferred

Four engines are analyzed below. The choice is deferred to the POC phase. The abstract interface in `01-language-interface-spec.md` ensures any of them can be swapped in.

### D2: Synchronous-First Execution

JavaScript in Frontier runs synchronously by default. The event loop is single-turn: a script runs to completion, then returns. Async/await and Promises are a future evolution (see Async section below).

### D3: `frontier` Global Object

The engine exposes a `frontier` global object that projects the kernel verb tables into JavaScript's namespace. This is the primary API for JS scripts to interact with Frontier.

### D4: ES Modules Mapped to ODB

`import` statements resolve ODB paths, enabling modular JS scripts stored in the database:
```javascript
import { helper } from "user.scripts.utils";
```

## Details

### Engine Comparison

| Feature | QuickJS | JavaScriptCore | V8 | Duktape |
|---|---|---|---|---|
| **License** | MIT | LGPL-2 / BSD-2 | BSD-3 | MIT |
| **Language standard** | ES2023 | ES2023+ | ES2023+ | ES5.1 + partial ES6 |
| **Binary size** | ~700 KB | ~5 MB (framework) | ~10 MB | ~300 KB |
| **Embedding API** | Simple C API | C/ObjC API | C++ API | Simple C API |
| **Async support** | Promise, async/await | Full | Full | Partial |
| **Debugging** | Basic | Web Inspector | Chrome DevTools | Basic |
| **Performance** | Interpreter (fast startup) | JIT (fast execution) | JIT (fastest) | Interpreter |
| **macOS availability** | Build from source | System framework | Build from source | Build from source |
| **Memory footprint** | ~1 MB runtime | ~10 MB runtime | ~20 MB runtime | ~256 KB runtime |

#### QuickJS

**Pros**: MIT licensed, tiny footprint, ES2023 compliant, simple C embedding API, no dependencies. Actively maintained (bellard/quickjs). Perfect for embedding.

**Cons**: Interpreter-only (no JIT), slower than V8/JSC for compute-heavy code.

**Assessment**: Best default choice for Frontier. Small, permissively licensed, full ES2023 support, trivial to embed.

#### JavaScriptCore (JSC)

**Pros**: Available as system framework on macOS (`JavaScriptCore.framework`), JIT compilation, excellent performance, mature.

**Cons**: LGPL on non-Apple platforms (limits static linking), large runtime, C++/ObjC API more complex than QuickJS's C API. Apple-platform advantage doesn't help Linux portability.

**Assessment**: Good performance choice for macOS-only deployment. LGPL concern for cross-platform distribution.

#### V8

**Pros**: Fastest JS engine, full ES2023+, excellent debugging (Chrome DevTools protocol).

**Cons**: Huge binary and runtime size (~10 MB + ~20 MB), C++ API, complex build system (GN/Ninja), significant memory overhead. Designed for browsers, not embedding.

**Assessment**: Overkill for Frontier's use case. The complexity and size aren't justified for ODB scripting.

#### Duktape

**Pros**: Smallest footprint (~300 KB), MIT licensed, extremely simple C API, very easy to embed.

**Cons**: Only ES5.1 with partial ES6 support. No async/await, no Promises, no modules. Effectively frozen at an old standard.

**Assessment**: Too limited. Missing modern JS features that developers expect.

#### Licensing Summary

All four engines have permissive licenses compatible with Frontier's MIT/BSD model:

| Engine | License | Static Linking OK? | Distribution OK? |
|---|---|---|---|
| QuickJS | MIT | Yes | Yes |
| JavaScriptCore | LGPL-2 (non-Apple) / BSD-2 (Apple) | Conditional | Yes |
| V8 | BSD-3 | Yes | Yes |
| Duktape | MIT | Yes | Yes |

### JS ↔ Frontier Type Mapping

| Frontier Type | `tyvaluetype` | JS Type | Conversion Notes |
|---|---|---|---|
| No value | `novaluetype` | `undefined` | — |
| Boolean | `booleanvaluetype` | `boolean` | Direct |
| Char | `charvaluetype` | `string` | Single character, length 1 |
| Integer | `intvaluetype` | `number` | 16-bit → JS number (no precision loss) |
| Long | `longvaluetype` | `number` | 32-bit → JS number (no precision loss) |
| Double | `doublevaluetype` | `number` | IEEE 754, direct |
| String | `stringvaluetype` | `string` | Handle → UTF-8 JS string |
| Date | `datevaluetype` | `Date` | Frontier seconds → JS epoch ms |
| Address | `addressvaluetype` | `string` | ODB path as dotted string |
| Binary | `binaryvaluetype` | `ArrayBuffer` | Raw bytes |
| List | `listvaluetype` | `Array` | Recursive element conversion |
| Record | `recordvaluetype` | plain `Object` | Keys are field names |
| OSType | `ostypevaluetype` | `string` | 4-character string |
| File spec | `filespecvaluetype` | `string` | File path |
| External (table) | `externalvaluetype` | `Proxy` object | See ODB Proxy section |
| Code | `codevaluetype` | N/A | Internal, not exposed to JS |

#### Edge Cases

- **JS `null`** → Frontier `novaluetype` (same as `undefined`)
- **JS `BigInt`** → Convert to Frontier `doublevaluetype` if fits, else error
- **JS `Symbol`** → Error (not convertible)
- **JS `function`** → Not directly storable in ODB; wrap as callback reference
- **Frontier `addressvaluetype`** → JS string. Use `frontier.db.resolve()` to dereference.

### The `frontier` Global Object

JS scripts access Frontier through a global `frontier` object that projects the kernel verb tables:

```javascript
// ── ODB Access ──
frontier.db.get("user.prefs.name")           // → "Jake"
frontier.db.set("user.prefs.name", "Alice")  // → true
frontier.db.exists("user.prefs.name")        // → true
frontier.db.delete("user.prefs.temp")        // → true
frontier.db.getType("user.prefs.name")       // → "string"
frontier.db.listTable("user.prefs")          // → ["name", "email", ...]

// ── Run Scripts ──
frontier.db.runScript("user.scripts.hello")               // no params
frontier.db.runScript("user.scripts.add", [1, 2])         // with params

// ── String Verbs ──
frontier.string.upper("hello")            // → "HELLO"
frontier.string.lower("HELLO")            // → "hello"
frontier.string.length("hello")           // → 5
frontier.string.mid("hello", 2, 3)        // → "llo"
frontier.string.patternMatch("*.txt", "readme.txt") // → true

// ── Math Verbs ──
frontier.math.sqrt(16)                    // → 4
frontier.math.random(1, 100)              // → 42

// ── File Verbs ──
frontier.file.exists("/tmp/test.txt")     // → true
frontier.file.readWholeFile("/tmp/test.txt") // → "contents..."
frontier.file.writeWholeFile("/tmp/test.txt", "new contents")
frontier.file.delete("/tmp/test.txt")

// ── Date Verbs ──
frontier.date.now()                       // → Date object
frontier.date.shortString(new Date())     // → "2/13/2026"

// ── System ──
frontier.sys.version()                    // → "7.0"
frontier.sys.os()                         // → "macos"
```

#### API Projection Pattern

Each verb table in the kernel maps to a property on the `frontier` object. The projection is generated at engine init time by walking the kernel's verb hash tables:

```
Kernel verb table "string" with verbs [upper, lower, length, ...]
  → frontier.string = { upper: <native>, lower: <native>, length: <native>, ... }

Kernel verb table "file" with verbs [exists, readWholeFile, ...]
  → frontier.file = { exists: <native>, readWholeFile: <native>, ... }
```

Each projected function:
1. Converts JS arguments to `tyvaluerecord` array
2. Calls the kernel verb implementation via the EFP callback
3. Converts the `tyvaluerecord` result back to a JS value
4. Returns the JS value (or throws a JS `Error` on failure)

### ODB Proxy Objects

When JS code accesses a table in the ODB, it gets a `Proxy` object that supports property access:

```javascript
let prefs = frontier.db.getTable("user.prefs");
prefs.name          // → reads user.prefs.name
prefs.name = "Bob"  // → writes user.prefs.name
prefs.email         // → reads user.prefs.email

// Nested tables
let user = frontier.db.getTable("user");
user.prefs.name     // → reads user.prefs.name (chained proxy)

// Iteration
for (let key of Object.keys(prefs)) {
    console.log(key, prefs[key]);
}
```

The Proxy intercepts `get`, `set`, `has`, `deleteProperty`, and `ownKeys` traps, translating them to ODB operations via the `frontier_odb_*` API.

### Module System

ES modules (`import`/`export`) are mapped to ODB paths:

```javascript
// In user.scripts.main:
import { formatDate } from "user.scripts.utils";
let result = formatDate(frontier.date.now());

// In user.scripts.utils:
export function formatDate(d) {
    return frontier.date.shortString(d);
}
```

#### Module Resolution Rules

1. **Quoted path starting with `"user."`, `"system."`, etc.** → ODB path lookup
2. **Quoted path starting with `"./"` or `"../"` relative** → Relative to current script's ODB location
3. **Quoted path starting with `"file:"` prefix** → Filesystem path
4. **Bare specifier (no dots, no prefix)** → Future: package registry or `node_modules` equivalent

```javascript
import { a } from "user.scripts.utils";    // ODB absolute
import { b } from "./helpers";              // ODB relative
import { c } from "file:/path/to/lib.js";  // Filesystem
```

#### Compilation Units

Each ODB script node is a compilation unit. The engine caches compiled modules keyed by ODB path + modification timestamp.

### Async/Await Considerations

**Phase 1 (Initial): Synchronous Only**

JavaScript scripts run synchronously to completion. The event loop does not tick during execution. `setTimeout`, `setInterval`, `fetch`, and other async Web APIs are not available.

Rationale: Frontier's GIL model requires scripts to hold the lock. Introducing an event loop that yields the GIL mid-script adds significant complexity.

**Phase 2 (Evolution): Cooperative Async**

If async support is added later:
- `await` expressions yield the GIL (call `frontier_gil_yield()`)
- Promises resolve on the next cooperative yield point
- The JS event loop runs within Frontier's thread scheduler
- `setTimeout(fn, ms)` maps to `thread.sleepTicks()` semantics

```javascript
// Future async support:
async function fetchData() {
    let response = await frontier.tcp.httpGet("http://example.com/api");
    frontier.db.set("user.data.response", response);
}
```

This is complex and should only be pursued if synchronous scripting proves insufficient for real use cases.

### Implementation of FrontierLanguage Vtable

Example implementation sketch for QuickJS:

```c
#include "quickjs.h"

static JSRuntime *qjs_runtime = NULL;
static JSContext  *qjs_context = NULL;

static boolean js_init(void) {
    qjs_runtime = JS_NewRuntime();
    if (!qjs_runtime) return false;

    qjs_context = JS_NewContext(qjs_runtime);
    if (!qjs_context) return false;

    /* Register the 'frontier' global object */
    js_register_frontier_global(qjs_context);

    return true;
}

static boolean js_shutdown(void) {
    JS_FreeContext(qjs_context);
    JS_FreeRuntime(qjs_runtime);
    return true;
}

static boolean js_compile(Handle htext, Handle *hcompiled) {
    /* Lock source text, null-terminate */
    char *source = handle_to_cstring(htext);

    /* Compile to bytecode */
    JSValue func = JS_Eval(qjs_context, source, strlen(source),
                           "<script>", JS_EVAL_FLAG_COMPILE_ONLY);
    free(source);

    if (JS_IsException(func)) {
        propagate_js_error(qjs_context);
        return false;
    }

    /* Serialize bytecode to a Handle */
    *hcompiled = js_value_to_handle(func);
    JS_FreeValue(qjs_context, func);
    return true;
}

static boolean js_execute(Handle hcompiled, tyvaluerecord *result) {
    JSValue func = handle_to_js_value(hcompiled);
    JSValue ret = JS_EvalFunction(qjs_context, func);

    if (JS_IsException(ret)) {
        propagate_js_error(qjs_context);
        return false;
    }

    boolean ok = js_value_to_frontier(qjs_context, ret, result);
    JS_FreeValue(qjs_context, ret);
    return ok;
}

static FrontierLanguage javascript_language = {
    .signature      = 'JSFT',
    .name           = "JavaScript",
    .version        = "ES2023 (QuickJS)",
    .file_extension = "js",
    .init           = js_init,
    .shutdown       = js_shutdown,
    .compile        = js_compile,
    .dispose_compiled = js_dispose_compiled,
    .execute        = js_execute,
    .call_function  = js_call_function,
    .register_verbs = js_register_verbs,
    .get_value      = js_get_value,
    .set_value      = js_set_value,
    .yield          = js_yield,
    .save_thread_state    = NULL,  /* QuickJS is single-threaded */
    .restore_thread_state = NULL,
};
```

### POC Scope

The proof-of-concept should demonstrate:

1. **Engine initialization** — QuickJS (or chosen engine) starts within Frontier
2. **Simple script execution** — `let x = 1 + 2; x;` returns `3` as a Frontier `longvaluetype`
3. **frontier.db.get/set** — Read/write a string value in the ODB from JS
4. **frontier.string.upper** — Call a kernel verb from JS
5. **ODB-stored script** — Create a JS script node in the ODB, execute it
6. **Type round-trip** — Pass Frontier values to JS and back (string, number, boolean, list)

**Out of scope for POC**: Modules, async, Proxy objects, debugging, filesystem scripts.

## Open Questions

1. **Per-context vs shared context** — Should each script execution get a fresh JS context, or should there be a shared global context with persistent state? Fresh contexts are safer but slower.

2. **Console output** — Where does `console.log()` go? Options: Frontier's `msg()` verb, a log table in the ODB, stdout in headless mode.

3. **Error stack traces** — JS stack traces reference source locations. How do these map to ODB script nodes? Need a source-map-like mechanism for ODB-stored scripts.

4. **npm integration** — Can `node_modules` be stored in the ODB? Or is filesystem-only more practical? This is a hard UX problem.

5. **TypeScript** — Should TypeScript be supported? QuickJS doesn't have built-in TS support. Options: compile TS→JS externally, or embed a TS compiler (large dependency).

## Next Steps

1. Build QuickJS as a static library, link into Frontier
2. Implement the `FrontierLanguage` vtable for QuickJS
3. Implement `frontier.db.get()` / `frontier.db.set()` via the ODB API
4. Run the POC scope items above
5. Evaluate engine performance and memory usage
