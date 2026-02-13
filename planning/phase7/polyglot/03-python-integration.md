# Python Integration Specification

Status
- State: Draft
- Phase: Phase 7 — Polyglot Scripting
- Last Updated: 2026-02-13
- Owner: Jake
- Notes: Specification only. The double-GIL problem is the central challenge.

Related Docs
- `planning/phase7/polyglot/00-polyglot-vision.md` — Vision & Architecture Overview
- `planning/phase7/polyglot/01-language-interface-spec.md` — Abstract Language Interface
- `planning/phase7/polyglot/02-javascript-integration.md` — JavaScript Integration
- `planning/phase7/polyglot/04-compiled-extensions.md` — Compiled Extensions
- `Common/source/langpython.c` — Legacy Python 1.6 experiment
- `Common/headers/lang.h` — `tyvaluerecord`, `tyvaluetype`
- `Common/headers/processinternal.h` — `tythreadglobals`, GIL model (ADR-014)

Change Log
- 2026-02-13: Initialized document.

## Overview

Python is the second most popular scripting language and the dominant language for data science, automation, and web development. Adding Python support to Frontier enables access to the massive `pip` ecosystem — thousands of libraries for HTTP, JSON, databases, machine learning, and more.

However, Python integration faces a unique challenge that JavaScript does not: **the double-GIL problem**. Both Frontier and CPython have their own Global Interpreter Lock, and the interaction between the two is the central design challenge of this specification.

### Lessons from `langpython.c`

Frontier had a Python 1.6 experiment in `Common/source/langpython.c`. It was minimal:

```c
/* Simplified from the actual code */
boolean langrunpythonscript(hdltreenode hp1, tyvaluerecord *v) {
    Handle h;
    getexempttextvalue(hp1, 1, &h);    /* Get script text */
    enlargehandle(h, 1, &nilchar);      /* Null-terminate */
    initpython();                        /* Py_Initialize() once */
    lockhandle(h);
    PyRun_SimpleString(*h);              /* Execute */
    unlockhandle(h);
    disposehandle(h);
    return setbooleanvalue(true, v);     /* Always returns true */
}
```

**What it shows**: The approach was correct — initialize once, pass source text, execute. But it had no value bridge (always returned `true`), no error propagation, and no GIL awareness. The modern integration needs all of these.

**What we keep**: The basic pattern of `Py_Initialize()` + `PyRun_SimpleString()` is still the foundation. We build the value bridge and GIL protocol on top.

## Decisions

### D1: CPython as Primary Engine

CPython is the reference Python implementation and the only one that supports the full `pip` ecosystem. MicroPython is analyzed as an alternative but recommended only for constrained environments.

### D2: Double-GIL Solution — Ordered Acquisition (Default)

Of the four solutions analyzed below, ordered acquisition is the default approach. Out-of-process (RPC) is the fallback if ordered acquisition proves problematic.

### D3: `frontier` Module (Not Global)

Python uses `import frontier` rather than a global object, following Python conventions:

```python
import frontier
name = frontier.db.get("user.prefs.name")
```

### D4: Full pip Access

Python scripts can `import` any pip-installed package. Frontier does not sandbox the Python environment.

## Details

### Engine Comparison: CPython vs MicroPython

| Feature | CPython | MicroPython |
|---|---|---|
| **License** | PSF (BSD-like) | MIT |
| **Language standard** | Full Python 3.12+ | Python 3.4 subset |
| **pip ecosystem** | Full access | No pip support |
| **Binary size** | ~5 MB (libpython3.so) | ~300 KB |
| **Memory footprint** | ~15 MB runtime | ~256 KB |
| **Embedding API** | Comprehensive C API | Minimal C API |
| **Async support** | Full (asyncio) | Limited |
| **C extensions** | Full (numpy, etc.) | Limited |
| **GIL** | Yes (own GIL) | No (single-threaded) |
| **Performance** | Good (bytecode + JIT in 3.13+) | Moderate |

**CPython** is the right choice for Frontier. The `pip` ecosystem is the primary value proposition — without it, Python support is much less compelling. MicroPython's small footprint is appealing but the lack of `pip` makes it a niche option.

**MicroPython** could be considered if:
- Frontier needs to run on embedded/constrained platforms
- GIL interaction proves unworkable with CPython
- A lightweight "Python-like" scripting option is sufficient

### The Double-GIL Problem

This is the central technical challenge of Python integration.

**The problem**: Frontier has a GIL (ADR-014) — a single mutex that serializes all script execution. CPython also has a GIL — a single mutex that serializes all Python bytecode execution. When a Python script calls back into Frontier (e.g., `frontier.db.get()`), both GILs must be held. When Frontier calls into Python, both GILs must be held.

**Why it matters**: If not handled carefully, the two GILs can deadlock. Thread A holds Frontier GIL and wants Python GIL; Thread B holds Python GIL and wants Frontier GIL.

#### Solution 1: Ordered Acquisition (Recommended)

Always acquire GILs in the same order: **Frontier GIL first, then Python GIL**.

```
Frontier → Python call:
  1. Already hold Frontier GIL
  2. Acquire Python GIL (PyGILState_Ensure)
  3. Execute Python code
  4. Release Python GIL (PyGILState_Release)
  5. Still hold Frontier GIL

Python → Frontier callback (e.g., frontier.db.get()):
  1. Already hold Python GIL (in Python code)
  2. Already hold Frontier GIL (was acquired first, never released)
  3. Execute Frontier verb
  4. Return to Python
```

**Key invariant**: The Frontier GIL is never released while Python code is running. This means Python code cannot yield to other Frontier threads. This is acceptable for Phase 1 (synchronous execution) but limits concurrency.

**Pros**: Simple, deadlock-free, matches existing GIL semantics.

**Cons**: No concurrency between Python and other Frontier threads. Long-running Python scripts block everything.

#### Solution 2: GIL Release During Python Execution

Release the Frontier GIL while Python code runs (but reacquire before any callback):

```
Frontier → Python call:
  1. Hold Frontier GIL
  2. Save Frontier thread state
  3. Release Frontier GIL          ← other Frontier threads can run
  4. Acquire Python GIL
  5. Execute Python code
  6. Release Python GIL
  7. Reacquire Frontier GIL        ← blocks until available
  8. Restore Frontier thread state

Python → Frontier callback:
  1. Hold Python GIL
  2. Release Python GIL temporarily
  3. Acquire Frontier GIL           ← must reacquire for ODB access
  4. Execute Frontier verb
  5. Release Frontier GIL
  6. Reacquire Python GIL
  7. Return to Python
```

**Pros**: Better concurrency — other Frontier threads (UserTalk, JS) can run while Python executes.

**Cons**: Complex GIL juggling. Each callback crosses the boundary twice. Risk of deadlock if the ordering is violated. Thread state must be perfectly saved/restored.

#### Solution 3: Out-of-Process Python (RPC)

Run CPython in a separate process. Communication via RPC (MessagePack, JSON-RPC, or Unix socket):

```
┌─────────────────────┐     ┌──────────────────────┐
│   Frontier Process   │     │   Python Process      │
│   (Frontier GIL)     │←───→│   (Python GIL)        │
│                      │ RPC │                        │
│   frontier_odb_*()   │     │   import frontier      │
└─────────────────────┘     └──────────────────────┘
```

**Pros**: Complete GIL isolation. Python crash doesn't kill Frontier. Clean separation. Can run multiple Python workers.

**Cons**: Higher latency (IPC overhead). Complex serialization for ODB values. Harder to debug. Two processes to manage.

#### Solution 4: Python Free-Threading (3.13+)

CPython 3.13 introduced experimental free-threading mode (no GIL). If this stabilizes:

```
Frontier → Python call:
  1. Hold Frontier GIL
  2. Call Python (no Python GIL needed)
  3. Python runs freely
  4. Return to Frontier

Python → Frontier callback:
  1. In Python (no GIL)
  2. Acquire Frontier GIL
  3. Execute Frontier verb
  4. Release Frontier GIL
  5. Return to Python
```

**Pros**: Simplest model. Only one GIL to manage.

**Cons**: Free-threading is experimental in CPython 3.13, may not stabilize for years. Many C extensions (numpy, etc.) don't support it yet. Not available in older Python versions.

#### Recommendation

**Start with Solution 1 (ordered acquisition)** for simplicity and correctness. If performance requires concurrency, move to **Solution 2** (GIL release). If GIL interaction is fundamentally problematic, fall back to **Solution 3** (out-of-process). Monitor **Solution 4** as CPython matures.

### Python ↔ Frontier Type Mapping

| Frontier Type | `tyvaluetype` | Python Type | Conversion Notes |
|---|---|---|---|
| No value | `novaluetype` | `None` | Direct |
| Boolean | `booleanvaluetype` | `bool` | Direct |
| Char | `charvaluetype` | `str` | Single character |
| Integer | `intvaluetype` | `int` | 16-bit → Python arbitrary-precision int |
| Long | `longvaluetype` | `int` | 32-bit → Python int |
| Double | `doublevaluetype` | `float` | IEEE 754, direct |
| String | `stringvaluetype` | `str` | Handle → UTF-8 Python string |
| Date | `datevaluetype` | `datetime.datetime` | Frontier seconds → Python datetime |
| Address | `addressvaluetype` | `str` | ODB path as dotted string |
| Binary | `binaryvaluetype` | `bytes` | Raw bytes |
| List | `listvaluetype` | `list` | Recursive element conversion |
| Record | `recordvaluetype` | `dict` | Keys are field names (str) |
| OSType | `ostypevaluetype` | `str` | 4-character string |
| File spec | `filespecvaluetype` | `pathlib.Path` | File path object |
| External (table) | `externalvaluetype` | Proxy object | See ODB proxy section |
| Code | `codevaluetype` | N/A | Internal, not exposed |

#### Edge Cases

- **Python `int` overflow** → Frontier `longvaluetype` is 32-bit. Python ints exceeding 32-bit range produce an error. Future: extend to 64-bit or use `doublevaluetype`.
- **Python `tuple`** → Converted to Frontier `listvaluetype` (tuples and lists map to the same Frontier type)
- **Python `set`** → Converted to Frontier `listvaluetype` (unordered, deduplicated)
- **Python `complex`** → Error (no Frontier equivalent)
- **Python `bytearray`** → Converted to `binaryvaluetype` (same as `bytes`)
- **Frontier `addressvaluetype`** → Python `str`. Use `frontier.db.resolve()` to dereference.

### The `frontier` Module API

Python accesses Frontier through an importable module:

```python
import frontier

# ── ODB Access ──
name = frontier.db.get("user.prefs.name")           # → "Jake"
frontier.db.set("user.prefs.name", "Alice")
frontier.db.exists("user.prefs.name")                # → True
frontier.db.delete("user.prefs.temp")
frontier.db.get_type("user.prefs.name")              # → "string"
frontier.db.list_table("user.prefs")                 # → ["name", "email", ...]

# ── Run Scripts ──
result = frontier.db.run_script("user.scripts.hello")
result = frontier.db.run_script("user.scripts.add", 1, 2)

# ── String Verbs ──
frontier.string.upper("hello")            # → "HELLO"
frontier.string.lower("HELLO")            # → "hello"
frontier.string.length("hello")           # → 5

# ── Math Verbs ──
frontier.math.sqrt(16)                    # → 4.0
frontier.math.random(1, 100)              # → 42

# ── File Verbs ──
frontier.file.exists("/tmp/test.txt")     # → True
frontier.file.read_whole_file("/tmp/test.txt")       # → "contents..."
frontier.file.write_whole_file("/tmp/test.txt", "new contents")

# ── Date Verbs ──
frontier.date.now()                       # → datetime.datetime(...)
frontier.date.short_string(datetime.now()) # → "2/13/2026"

# ── System ──
frontier.sys.version()                    # → "7.0"
```

Note: Python naming conventions use `snake_case` for the API, even though Frontier's kernel verbs use `camelCase`. The projection layer handles the translation:

```
frontier.file.read_whole_file(path)  →  kernel: file.readWholeFile(path)
frontier.db.list_table(path)         →  kernel: db.listTable(path) (hypothetical)
```

### ODB Proxy Objects

Similar to the JavaScript Proxy pattern, Python uses `__getattr__`/`__setattr__` to create transparent ODB access:

```python
prefs = frontier.db.get_table("user.prefs")
print(prefs.name)          # reads user.prefs.name
prefs.name = "Bob"         # writes user.prefs.name

# Nested tables
user = frontier.db.get_table("user")
print(user.prefs.name)     # reads user.prefs.name

# Dict-like access
for key in prefs:
    print(key, prefs[key])

# Check membership
"name" in prefs             # → True
```

### Memory Management at the Boundary

CPython uses reference counting with a cycle collector. Frontier uses handle-based memory with `newhandle()`/`disposehandle()`. The boundary must carefully manage ownership:

#### Frontier → Python

When a Frontier value is passed to Python:
1. Extract data from `tyvaluerecord`
2. Create a Python object (increments Python refcount)
3. The Frontier value remains valid (caller still owns it)
4. Python object lives until Python GC collects it

No ownership transfer — both sides have their own copy.

#### Python → Frontier

When Python returns a value to Frontier:
1. Read the Python object's data
2. Create a `tyvaluerecord` with `newhandle()` for string/binary data
3. The Python object remains valid (Python still owns it)
4. Frontier value managed by Frontier's tmp stack / handle system

Again, no ownership transfer — copy semantics at the boundary.

#### Special Case: Large Binary Data

For large `bytes`/`ArrayBuffer` values, copying is expensive. A future optimization could use shared memory or zero-copy handles, but the initial implementation should copy for safety.

### pip / Package Ecosystem Access

Python scripts can import any pip-installed package:

```python
import frontier
import requests  # pip-installed
import json

response = requests.get("https://api.example.com/data")
data = json.loads(response.text)
frontier.db.set("user.data.api_result", data)
```

#### Package Environment

Python uses the system Python installation's package environment. The `frontier` module is injected at runtime (not installed via pip). All other packages come from the user's normal Python environment.

Questions for future consideration:
- Should Frontier manage its own virtualenv?
- Can `pip install` be triggered from within Frontier (e.g., `frontier.sys.pip_install("requests")`)?
- Should there be a `requirements.txt` equivalent in the ODB?

### Implementation of FrontierLanguage Vtable

```c
#include <Python.h>

static boolean python_initialized = false;

static boolean py_init(void) {
    if (!python_initialized) {
        Py_Initialize();
        python_initialized = true;

        /* Register the 'frontier' built-in module */
        py_register_frontier_module();
    }
    return true;
}

static boolean py_shutdown(void) {
    if (python_initialized) {
        Py_Finalize();
        python_initialized = false;
    }
    return true;
}

static boolean py_compile(Handle htext, Handle *hcompiled) {
    char *source = handle_to_cstring(htext);

    /* Compile to Python code object */
    PyObject *code = Py_CompileString(source, "<frontier-script>",
                                       Py_file_input);
    free(source);

    if (code == NULL) {
        propagate_python_error();
        return false;
    }

    /* Store code object reference as a Handle */
    *hcompiled = py_object_to_handle(code);
    /* Note: code object's refcount is now managed by the Handle */
    return true;
}

static boolean py_execute(Handle hcompiled, tyvaluerecord *result) {
    PyObject *code = handle_to_py_object(hcompiled);

    /* Create a fresh namespace for execution */
    PyObject *globals = PyDict_New();
    PyObject *builtins = PyEval_GetBuiltins();
    PyDict_SetItemString(globals, "__builtins__", builtins);

    /* Execute */
    PyObject *ret = PyEval_EvalCode(code, globals, globals);

    if (ret == NULL) {
        propagate_python_error();
        Py_DECREF(globals);
        return false;
    }

    /* Convert return value (last expression or None) */
    boolean ok = py_object_to_frontier(ret, result);

    Py_DECREF(ret);
    Py_DECREF(globals);
    return ok;
}

static FrontierLanguage python_language = {
    .signature      = 'PYFT',
    .name           = "Python",
    .version        = "3.12+ (CPython)",
    .file_extension = "py",
    .init           = py_init,
    .shutdown       = py_shutdown,
    .compile        = py_compile,
    .dispose_compiled = py_dispose_compiled,
    .execute        = py_execute,
    .call_function  = py_call_function,
    .register_verbs = NULL,
    .get_value      = py_get_value,
    .set_value      = py_set_value,
    .yield          = py_yield,
    .save_thread_state    = py_save_thread_state,
    .restore_thread_state = py_restore_thread_state,
};
```

### POC Scope

The proof-of-concept should demonstrate:

1. **CPython initialization** — `Py_Initialize()` within Frontier process
2. **Simple script execution** — `x = 1 + 2` returns `3`
3. **frontier.db.get/set** — Read/write ODB from Python
4. **frontier.string.upper** — Call a kernel verb from Python
5. **pip package usage** — `import json; json.dumps({"a": 1})` works
6. **Double-GIL validation** — Verify no deadlock with ordered acquisition under load
7. **Type round-trip** — string, int, float, bool, list, dict

**Out of scope for POC**: Async, Proxy objects, virtualenv management, package installation from Frontier.

## Open Questions

1. **Python version requirement** — Should Frontier require Python 3.12+? Or support 3.9+? Newer versions have better embedding support and (eventually) free-threading.

2. **Multiple Python versions** — Can users choose which Python installation to use? If so, how is this configured?

3. **Python GIL release timing** — In Solution 2, exactly when should the Frontier GIL be released? Before `PyEval_EvalCode()`? This affects which Frontier state must be saved.

4. **Return value semantics** — Python's `exec()` doesn't return a value (returns `None`). Should Frontier Python scripts use `eval()` semantics (expression returns value) or require an explicit `return` mechanism (e.g., `frontier.set_result(value)`)?

5. **Standard output** — Where does `print()` go? Same question as JS's `console.log()`. Options: `msg()` verb, log table, stdout.

6. **Free-threading timeline** — When will CPython's free-threading mode (PEP 703) be stable enough for production use? This would eliminate the double-GIL problem entirely.

## Next Steps

1. Build a minimal CPython embedding test (outside Frontier)
2. Validate ordered GIL acquisition under concurrent load
3. Implement the `frontier` module using CPython's C extension API
4. Integrate into Frontier via the `FrontierLanguage` vtable
5. Run the POC scope items
6. Benchmark GIL contention with concurrent UserTalk + Python scripts
