# Polyglot Scripting — Vision & Architecture Overview

Status
- State: Draft
- Phase: Phase 7 — Polyglot Scripting
- Last Updated: 2026-02-13
- Owner: Jake
- Notes: Specification only. No code changes. Phase 6 (CRDT) precedes this work.

Related Docs
- `planning/phase7/polyglot/01-language-interface-spec.md` — Abstract Language Interface
- `planning/phase7/polyglot/02-javascript-integration.md` — JavaScript Integration
- `planning/phase7/polyglot/03-python-integration.md` — Python Integration
- `planning/phase7/polyglot/04-compiled-extensions.md` — Go, Rust & Compiled Extensions
- `planning/phase6/CRDT_FOUNDATION_ROADMAP.md` — Phase 6 (prerequisite)
- `planning/phase4/threading/README.md` — Threading & GIL model (ADR-014)
- `Common/headers/lang.h` — Value type system (`tyvaluerecord`, `tyvaluetype`)
- `Common/source/langpython.c` — Legacy Python 1.6 experiment
- `Common/headers/osacomponent.h` — Legacy AppleScript/OSA integration

Change Log
- 2026-02-13: Initialized document.

## Overview

Frontier historically supported multiple scripting languages — UserTalk natively, AppleScript via the OS's Open Scripting Architecture (OSA), and even had an experimental Python 1.6 integration (`langpython.c`). As the platform modernizes, the question is: what would it take to add JavaScript and Python as first-class scripting languages in the modern Frontier runtime?

This document lays out the vision, analyzes prior art, proposes two architectural approaches, and defines the roadmap for multi-language support. The remaining documents in this series detail the abstract interface, specific language integrations, and compiled extension support.

### Why Multi-Language Support Matters

1. **Developer accessibility** — UserTalk is Frontier's native language but has no ecosystem outside Frontier. JavaScript and Python are the two most widely-used scripting languages, with massive package ecosystems. Supporting them lowers the barrier to adoption.

2. **ODB as a polyglot application server** — Frontier's Object Database is a hierarchical data store that can host scripts, data, and configuration. Making it addressable from multiple languages transforms it from a UserTalk-only scripting environment into a general-purpose application platform.

3. **Incremental modernization** — New features can be prototyped in JS or Python while the UserTalk runtime continues serving legacy scripts. No rewrite required.

4. **Extension ecosystem** — Compiled extensions (Go, Rust) enable high-performance modules for cryptography, networking, data processing, etc.

### Scope

This specification covers:
- Architecture for embedding multiple scripting engines
- Type bridging between Frontier values and language-native types
- ODB access from non-UserTalk languages
- Script storage and lifecycle
- Threading model interactions
- Cross-language calling semantics

This specification does NOT cover:
- Code changes or implementation
- Timeline or resource estimates
- Changes to UserTalk itself

## Decisions

### D1: Licensing Model

Frontier is currently GPLv2 licensed (forked from the GPLv2 release of the original codebase). A relicensing effort is underway with the de facto IP owner to dual-license the original code as GPLv2+MIT, after which this fork will migrate to MIT. All engine candidates should be permissively licensed to remain compatible with the target MIT license. This eliminates GPL-only options but leaves a wide field of candidates (QuickJS, JavaScriptCore, Duktape, CPython, MicroPython, etc.).

### D2: Architecture Decision Deferred

Two viable architectures are analyzed below. The decision of which to implement is deferred to implementation time, informed by POC results.

### D3: ODB Access — Full Read/Write

All scripting languages receive the same trust model as UserTalk: full read/write access to the ODB. Security boundaries (if needed) are a future concern, not part of this specification.

### D4: Script Storage — ODB-Native + Filesystem

Scripts can be stored as:
- **ODB-native**: Script nodes in the database (existing `externalvaluetype` / `idscriptprocessor` pattern)
- **Filesystem**: `.js`, `.py` files loaded from disk

Both mechanisms are first-class.

### D5: Cross-Language Calling — Bidirectional via Kernel Projection

Languages can call each other through the kernel verb layer. Each language projects the same kernel API, so `frontier.string.upper()` in JavaScript calls the same C implementation as `string.upper()` in UserTalk. Direct JS↔Python calling is not a goal — the kernel is the hub.

### D6: Threading — Shared GIL First

The initial implementation shares Frontier's existing GIL (ADR-014) across all language engines. Per-engine GIL is an evolution path documented but not required for v1.

### D7: JavaScript Engine — Deferred

The spec defines the abstract interface. Engine choice (QuickJS vs JavaScriptCore vs V8 vs Duktape) is deferred to POC phase.

## Details

### Prior Art Analysis

Five projects demonstrate successful polyglot scripting architectures:

#### Godot Engine — GDExtension

Godot defines a C ABI (`GDExtension`) that any language can implement. GDScript is built-in; C#, Rust, and Python are community-maintained extensions implementing the same ABI.

**Relevant pattern**: Abstract C interface with function pointers. Language-agnostic. Each engine is a plugin.

**Applicability to Frontier**: High. The EFP (External Function Processor) pattern is already a verb-level plugin system. Extending this to full language engines is a natural evolution.

#### Neovim — In-Process + RPC Hybrid

Neovim embeds Lua (LuaJIT) for high-performance in-process scripting. All other languages (Python, Node.js, Ruby) communicate via MessagePack-RPC over stdio.

**Relevant pattern**: Primary language embedded for performance; secondary languages via RPC for flexibility.

**Applicability to Frontier**: Moderate. The RPC approach is attractive for isolation but adds latency. Suitable for Python (which has its own GIL complications) but not ideal for tight loops.

#### Blender — Embedded CPython

Blender embeds CPython directly and exposes its entire API as a Python module (`bpy`). All scripting — from UI layout to physics simulations — goes through Python.

**Relevant pattern**: Deep single-language embedding with full API exposure.

**Applicability to Frontier**: Partially relevant. Shows how to bridge a C application's API into Python comprehensively. The `frontier` module API in `03-python-integration.md` follows this pattern.

#### Redis — Embedded Lua (+ RESP Protocol)

Redis embeds Lua for atomic server-side scripting. External languages interact via the RESP protocol. Redis 7.0 added Redis Functions with a module API.

**Relevant pattern**: Embedded scripting for atomicity, protocol-based access for external languages, module system for compiled extensions.

**Applicability to Frontier**: The atomicity model maps to Frontier's GIL — scripts hold the lock for their entire execution, ensuring consistent ODB state.

#### PostgreSQL — Procedural Language Framework (PL)

PostgreSQL defines a C-level `PLTemplate` interface that language handlers implement. PL/pgSQL is built-in; PL/Python, PL/Perl, PL/V8 (JavaScript) are extensions implementing the same handler interface.

**Relevant pattern**: Abstract handler interface. Each language is a shared library implementing `plhandler()`. Registration, compilation, and execution are mediated by the framework.

**Applicability to Frontier**: Very high. PostgreSQL's PL interface is the closest analogue to what Frontier needs. The `FrontierLanguage` vtable in `01-language-interface-spec.md` is directly inspired by this model.

### Architectural Approaches

#### Option A: Abstract Language Interface (Godot/PostgreSQL Pattern)

Each language engine implements a C vtable (`FrontierLanguage`). The kernel dispatches to engines based on script signature, just as it dispatches to the UserTalk compiler today via the `typeLAND` signature.

```
┌─────────────────────────────────────────────┐
│              Frontier Kernel                 │
│  (GIL, ODB, Verb Tables, Value System)      │
├──────────┬──────────┬───────────────────────┤
│ UserTalk │ JS Engine│ Python Engine │ ...    │
│ (native) │ (vtable) │ (vtable)     │        │
└──────────┴──────────┴───────────────────────┘
```

**How it works**:
1. Language engine registers via `frontier_register_language(signature, &vtable)`
2. Script nodes in ODB carry a 4-byte signature identifying the language
3. `scriptbuildtree()` dispatches to the registered engine's `compile` function
4. Execution dispatches to the engine's `execute` function
5. All engines share the Frontier GIL and value type system

**Advantages**:
- Clean, modular architecture
- Each engine is independent — can be compiled in/out
- Follows Frontier's existing dispatch-by-signature pattern
- Low latency (all in-process)

**Disadvantages**:
- Engine crashes can take down the process
- GIL contention with CPU-intensive scripts
- Memory management complexity (each engine has its own allocator)

#### Option B: In-Process + RPC Hybrid (Neovim Pattern)

UserTalk and one "primary" engine (likely JavaScript via QuickJS) run in-process. Other languages run in separate processes, communicating via RPC (MessagePack, JSON-RPC, or a custom protocol).

```
┌─────────────────────────────────┐
│         Frontier Process        │
│  ┌──────────┬─────────────┐    │
│  │ UserTalk │ JS (QuickJS) │    │
│  └──────────┴─────────────┘    │
│       ↕ RPC (stdio/socket)      │
├─────────────────────────────────┤
│  Python Process │ Go Process    │
│  (CPython)      │ (plugin)      │
└─────────────────────────────────┘
```

**How it works**:
1. In-process engines use the vtable interface (same as Option A)
2. Out-of-process engines communicate via RPC
3. An RPC bridge translates between Frontier values and the wire format
4. ODB operations from out-of-process engines go through the RPC bridge

**Advantages**:
- Engine isolation (Python crash doesn't kill Frontier)
- Solves the double-GIL problem for CPython (see `03-python-integration.md`)
- Can support compiled language runtimes (Go, Rust) easily

**Disadvantages**:
- Higher latency for out-of-process calls
- Complex RPC serialization for ODB values
- Two different integration patterns to maintain

#### Recommendation

Start with **Option A** (pure vtable). It is simpler, aligns with existing patterns, and delivers the lowest latency. If CPython's double-GIL proves unworkable in-process (see `03-python-integration.md`), Python can be moved out-of-process while keeping the vtable interface for in-process engines.

### Value Type Bridge

The bridge maps Frontier's `tyvaluerecord` types to language-native types:

| Frontier Type | C Constant | JS Type | Python Type |
|---|---|---|---|
| No value | `novaluetype` | `undefined` | `None` |
| Boolean | `booleanvaluetype` | `boolean` | `bool` |
| Char | `charvaluetype` | `string` (length 1) | `str` (length 1) |
| Integer | `intvaluetype` | `number` | `int` |
| Long | `longvaluetype` | `number` | `int` |
| Double | `doublevaluetype` | `number` | `float` |
| String | `stringvaluetype` | `string` | `str` |
| Date | `datevaluetype` | `Date` | `datetime.datetime` |
| Address | `addressvaluetype` | `string` (ODB path) | `str` (ODB path) |
| Binary | `binaryvaluetype` | `ArrayBuffer` | `bytes` |
| List | `listvaluetype` | `Array` | `list` |
| Record | `recordvaluetype` | `Object` | `dict` |
| OSType | `ostypevaluetype` | `string` (4-char) | `str` (4-char) |
| File spec | `filespecvaluetype` | `string` (path) | `pathlib.Path` |
| External (table) | `externalvaluetype` | Proxy object | Proxy object |
| External (script) | `externalvaluetype` | N/A (callable) | N/A (callable) |
| Code | `codevaluetype` | N/A (internal) | N/A (internal) |

**Key design rule**: Conversions are performed at the language boundary. Inside the engine, values are native. Inside the kernel, values are `tyvaluerecord`. The bridge converts in both directions.

### Cross-Language Calling

Languages do not call each other directly. All cross-language interaction goes through the kernel:

```
JS script calls frontier.string.upper("hello")
  → JS engine converts "hello" to tyvaluerecord (stringvaluetype)
  → Kernel dispatches to string.upper verb implementation (C)
  → Kernel returns tyvaluerecord result
  → JS engine converts result to JS string "HELLO"
```

```
UserTalk script calls string.upper("hello")
  → UserTalk evaluates parameter, produces tyvaluerecord
  → Kernel dispatches to string.upper verb implementation (C)
  → Kernel returns tyvaluerecord result
  → UserTalk stores result
```

Both languages call the same C verb implementation. This is the "kernel projection" model — each language projects a language-idiomatic view of the same underlying verb table.

For calling scripts in other languages:

```
JS script: frontier.db.runScript("user.scripts.myUserTalkScript")
  → Bridge resolves ODB path to script node
  → Identifies script signature ('LAND' = UserTalk)
  → Dispatches to UserTalk engine's execute() via vtable
  → Converts return value back to JS
```

### Threading Model

**Phase 1: Shared GIL (Initial)**

All language engines share Frontier's single GIL. Only one script runs at a time, regardless of language. This is the simplest model and matches Frontier's current semantics.

- Engines must release the GIL at yield points (`langbackgroundtask()`)
- Thread state (`tythreadglobals`) already contains per-thread hash table stacks, callbacks, and execution state
- New engines add their own thread-local state to `tythreadglobals` (or a language-specific extension)

**Phase 2: Per-Engine GIL (Evolution)**

If performance requires it, each engine can have its own GIL:
- UserTalk GIL, JS GIL, Python GIL (CPython already has one)
- Cross-engine calls acquire the target engine's GIL
- ODB access requires the Frontier GIL (serializes database operations)

This is more complex but enables true parallelism between engines.

### Script Storage

#### ODB-Native Storage

Scripts stored in the ODB use the existing `externalvaluetype` / `idscriptprocessor` mechanism:

1. Script source stored as a handle (text or outline)
2. Language identified by 4-byte signature (e.g., `'LAND'` = UserTalk, `'JSFT'` = JavaScript, `'PYFT'` = Python)
3. Compiled form cached in memory (engine-specific)
4. Signature stored in the script node header — `scriptbuildtree()` already dispatches on this

New language signatures:

| Language | Signature | Notes |
|---|---|---|
| UserTalk | `'LAND'` | Existing |
| AppleScript | `'ascr'` | Existing (legacy) |
| JavaScript | `'JSFT'` | New — "JavaScript for Frontier" |
| Python | `'PYFT'` | New — "Python for Frontier" |

#### Filesystem Storage

Scripts on disk use standard file extensions:
- `.js` — JavaScript
- `.py` — Python
- `.ut` — UserTalk (new convention)

The kernel verb `file.runScript(path)` (or equivalent) loads, compiles, and executes a script from disk, using the file extension to determine the language.

### Implementation Roadmap

#### Milestone 1: Abstract Interface + Registry
- Implement `FrontierLanguage` vtable and language registry
- Refactor UserTalk to implement the vtable (proving the interface)
- No new languages yet — just the framework

#### Milestone 2: JavaScript POC
- Integrate one JS engine (likely QuickJS for simplicity)
- Implement value bridge for core types
- Expose `frontier` global with basic ODB access
- Run simple JS scripts from ODB and filesystem

#### Milestone 3: Python POC
- Integrate CPython
- Solve the double-GIL problem (or demonstrate the RPC fallback)
- Implement value bridge
- Expose `frontier` module

#### Milestone 4: Cross-Language Calling
- JS and Python scripts can call UserTalk scripts (and vice versa)
- Full verb table projection in both languages

#### Milestone 5: Compiled Extensions
- Define shared library plugin ABI
- Demonstrate a Rust or Go extension registering verbs

## Open Questions

1. **Signature allocation** — Should new language signatures be registered centrally, or can they be arbitrary? (UserTalk's `'LAND'` and AppleScript's `'ascr'` were historically assigned by Apple.)

2. **Debugging** — How do language-specific debuggers interact with Frontier's debugging infrastructure? UserTalk has line-level debugging wired into the outline editor.

3. **Security sandboxing** — Should non-UserTalk scripts be sandboxable (restricted ODB access, no file I/O)? Not required for v1 but worth planning for.

4. **Package management** — How do npm/pip packages interact with ODB-stored scripts? Can `node_modules` or virtualenvs be ODB-rooted?

5. **Script editor integration** — Frontier's built-in editor is outline-based (designed for UserTalk). How do JS/Python scripts get edited? External editor integration? Plain text editor pane?

## Next Steps

1. Review this specification and the four companion documents
2. Validate the `FrontierLanguage` vtable against real engine APIs (QuickJS, CPython)
3. Build a minimal POC to test the vtable pattern with one engine
4. Update `planning/INDEX.md` and `planning/phase_overview.md` to include Phase 7
