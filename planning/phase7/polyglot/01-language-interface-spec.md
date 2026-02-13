# Abstract Language Interface Specification

Status
- State: Draft
- Phase: Phase 7 — Polyglot Scripting
- Last Updated: 2026-02-13
- Owner: Jake
- Notes: Specification only. Defines the C-level interface that language engines implement.

Related Docs
- `planning/phase7/polyglot/00-polyglot-vision.md` — Vision & Architecture Overview
- `planning/phase7/polyglot/02-javascript-integration.md` — JavaScript Integration
- `planning/phase7/polyglot/03-python-integration.md` — Python Integration
- `planning/phase7/polyglot/04-compiled-extensions.md` — Compiled Extensions
- `Common/headers/lang.h` — `tyvaluerecord`, `tyvaluetype`, `tylangcallbacks`
- `Common/headers/langexternal.h` — `tyexternalvariable`, `idscriptprocessor`
- `Common/source/kernel_verbs_headless.c` — EFP registration pattern
- `portable/script_portable.c` — Script dispatch by signature
- `Common/headers/processinternal.h` — `tythreadglobals`, GIL model

Change Log
- 2026-02-13: Initialized document.

## Overview

This document specifies the `FrontierLanguage` C interface — a vtable of function pointers that each language engine must implement to integrate with Frontier. It also defines the language registry, the value bridge API, error handling, GIL interaction protocol, and script lifecycle.

The design is inspired by PostgreSQL's Procedural Language framework and Godot's GDExtension pattern, adapted for Frontier's existing dispatch-by-signature architecture.

### Design Principles

1. **Minimal surface area** — The vtable contains only what the kernel needs. Engine-specific functionality stays inside the engine.
2. **Consistent with existing patterns** — The interface follows Frontier's EFP registration pattern (`newfunctionprocessor`, `langaddkeyword`).
3. **Value-level interop** — All data crosses the boundary as `tyvaluerecord` values. Engines convert to/from native types internally.
4. **Fail-safe** — Every function returns a boolean success flag. Errors propagate through a string buffer, matching Frontier's existing error conventions.

## Decisions

### D1: Vtable, Not Object-Oriented

The interface is a plain C struct of function pointers, not a C++ class or Objective-C protocol. This ensures ABI stability and compatibility with any language that can produce C-callable functions.

### D2: Registration by Signature

Languages register with a 4-byte OSType signature (e.g., `'JSFT'`). This reuses the existing script-signature dispatch in `scriptbuildtree()` and ODB script storage.

### D3: Value Bridge at the Boundary

Engines receive and return `tyvaluerecord` values. The bridge API provides helpers for conversion, but engines are responsible for managing their own native types internally.

### D4: GIL Acquired on Entry

All vtable functions are called with the Frontier GIL held. Engines that need to release the GIL (e.g., for I/O) must use the GIL release/reacquire protocol.

### D5: Error Strings, Not Exceptions

Frontier uses `bigstring` error buffers (255-byte Pascal strings). Engines must convert their native errors to this format. The bridge provides helpers.

## Details

### FrontierLanguage Vtable

```c
/*
 * FrontierLanguage — vtable for a language engine.
 *
 * Each language engine (UserTalk, JavaScript, Python, etc.) provides
 * an instance of this struct, populated with function pointers.
 *
 * All functions are called with the Frontier GIL held unless noted.
 * All functions return boolean (true = success, false = error).
 * On failure, the engine must set an error string via
 * frontier_lang_set_error().
 */

typedef struct FrontierLanguage {

    /* ── Identity ── */

    OSType          signature;      /* 4-byte language ID (e.g., 'JSFT') */
    const char*     name;           /* Human-readable name ("JavaScript") */
    const char*     version;        /* Engine version string */
    const char*     file_extension; /* Default file extension ("js") */

    /* ── Lifecycle ── */

    /*
     * init — Initialize the language engine.
     * Called once at startup (or on first use if lazy-loaded).
     * Engine should allocate global state, initialize its runtime.
     */
    boolean (*init)(void);

    /*
     * shutdown — Shut down the language engine.
     * Called once at process exit. Engine should free all resources.
     */
    boolean (*shutdown)(void);

    /* ── Compilation ── */

    /*
     * compile — Compile script source text to an engine-specific
     *           compiled form.
     *
     * htext:      Handle to source text (owned by caller)
     * hcompiled:  Output — opaque compiled form (Handle)
     *             Ownership transfers to caller.
     *
     * The compiled form is opaque to the kernel. It will be passed
     * back to execute() and dispose_compiled().
     */
    boolean (*compile)(Handle htext, Handle *hcompiled);

    /*
     * dispose_compiled — Free a compiled script.
     *
     * Called when a compiled script is unloaded from memory.
     */
    boolean (*dispose_compiled)(Handle hcompiled);

    /* ── Execution ── */

    /*
     * execute — Execute a compiled script.
     *
     * hcompiled:  Compiled form (from compile())
     * result:     Output value. Engine must set this.
     *
     * The script runs in the current thread context. The GIL is held.
     */
    boolean (*execute)(Handle hcompiled, tyvaluerecord *result);

    /*
     * call_function — Call a named function within a compiled script.
     *
     * hcompiled:    Compiled script containing the function
     * function_name: Name of the function to call
     * params:       Array of parameter values
     * ctparams:     Number of parameters
     * result:       Output value
     *
     * If the engine doesn't support named functions, return false
     * with an appropriate error.
     */
    boolean (*call_function)(Handle hcompiled,
                             const char *function_name,
                             tyvaluerecord *params, short ctparams,
                             tyvaluerecord *result);

    /* ── Verb Registration ── */

    /*
     * register_verbs — Register kernel verbs accessible from this
     *                  language.
     *
     * Called after init(). The engine should use the Verb Registration
     * API (below) to expose kernel verbs in its namespace.
     *
     * Can be NULL if the engine handles verb projection internally.
     */
    boolean (*register_verbs)(void);

    /* ── Value Bridge ── */

    /*
     * get_value — Read a value from the engine's global scope.
     *
     * name:   Variable name in engine's namespace
     * value:  Output — Frontier value
     *
     * Used for inspecting engine state (debugging, REPL).
     */
    boolean (*get_value)(const char *name, tyvaluerecord *value);

    /*
     * set_value — Set a value in the engine's global scope.
     *
     * name:   Variable name
     * value:  Frontier value to set
     *
     * Used for injecting context before execution.
     */
    boolean (*set_value)(const char *name, const tyvaluerecord *value);

    /* ── GIL Interaction ── */

    /*
     * yield — Cooperative yield point.
     *
     * Called by the kernel during long-running operations.
     * The engine should save any state needed to resume.
     * The GIL may be released and reacquired during this call.
     *
     * Returns false if the script should be killed (user cancel).
     *
     * Can be NULL if the engine manages its own yield points.
     */
    boolean (*yield)(void);

    /* ── Thread State ── */

    /*
     * save_thread_state / restore_thread_state
     *
     * Called when the Frontier scheduler swaps threads.
     * Engines with per-thread state must save/restore it here.
     *
     * state_out/state_in: Opaque pointer to engine-specific
     *                     thread state. Managed by the engine.
     *
     * Can be NULL if the engine has no per-thread state.
     */
    boolean (*save_thread_state)(void **state_out);
    boolean (*restore_thread_state)(void *state_in);

} FrontierLanguage;
```

### Language Registry

The registry maps signatures to language vtables. It is a simple array (Frontier typically has 2-4 languages).

```c
/*
 * frontier_register_language — Register a language engine.
 *
 * lang: Pointer to a FrontierLanguage struct. The struct must have
 *       static lifetime (typically a global or module-level variable).
 *
 * Returns false if the signature is already registered or the
 * registry is full.
 *
 * Call this during engine initialization (e.g., from a plugin's
 * init function or from the main startup sequence).
 */
boolean frontier_register_language(const FrontierLanguage *lang);

/*
 * frontier_find_language — Look up a language by signature.
 *
 * signature: 4-byte language identifier
 *
 * Returns a pointer to the registered FrontierLanguage, or NULL
 * if no language is registered with that signature.
 */
const FrontierLanguage* frontier_find_language(OSType signature);

/*
 * frontier_find_language_by_extension — Look up by file extension.
 *
 * extension: File extension without dot (e.g., "js", "py")
 *
 * Returns the first registered language with a matching
 * file_extension, or NULL.
 */
const FrontierLanguage* frontier_find_language_by_extension(
    const char *extension);

/*
 * frontier_get_registered_languages — Enumerate all registered
 *                                     languages.
 *
 * langs:     Output array (caller-allocated)
 * max_langs: Size of output array
 *
 * Returns the number of languages written to the array.
 */
short frontier_get_registered_languages(
    const FrontierLanguage **langs, short max_langs);
```

### Integration with Existing Dispatch

The registry integrates with `scriptbuildtree()` in `portable/script_portable.c`:

```c
/* Current dispatch (simplified): */
boolean scriptbuildtree(Handle htext, long signature, hdltreenode *hcode) {
    if (signature == typeLAND) {
        return langbuildtree(htext, true, hcode);
    }
    /* Fall through to OSA */
    return osagetcode(htext, signature, false, &codeval);
}

/* Proposed dispatch: */
boolean scriptbuildtree(Handle htext, long signature, hdltreenode *hcode) {
    if (signature == typeLAND) {
        return langbuildtree(htext, true, hcode);
    }

    /* Check language registry */
    const FrontierLanguage *lang = frontier_find_language(signature);
    if (lang != NULL) {
        Handle hcompiled;
        if (!lang->compile(htext, &hcompiled))
            return false;
        /* Store compiled form as a code value */
        /* (details in script lifecycle section) */
        return true;
    }

    /* Legacy fallback to OSA */
    return osagetcode(htext, signature, false, &codeval);
}
```

### Value Bridge API

Helper functions for converting between `tyvaluerecord` and C types commonly used by engines:

```c
/* ── Frontier → C (for engines to extract values) ── */

boolean frontier_value_to_cstring(const tyvaluerecord *v,
                                   char *buf, size_t bufsize);
boolean frontier_value_to_long(const tyvaluerecord *v, long *out);
boolean frontier_value_to_double(const tyvaluerecord *v, double *out);
boolean frontier_value_to_bool(const tyvaluerecord *v, boolean *out);
boolean frontier_value_to_handle(const tyvaluerecord *v, Handle *out);

/* ── C → Frontier (for engines to construct return values) ── */

boolean frontier_cstring_to_value(const char *s, tyvaluerecord *v);
boolean frontier_long_to_value(long n, tyvaluerecord *v);
boolean frontier_double_to_value(double d, tyvaluerecord *v);
boolean frontier_bool_to_value(boolean b, tyvaluerecord *v);
boolean frontier_handle_to_value(Handle h, tyvaluetype vtype,
                                  tyvaluerecord *v);

/* ── Composite types ── */

/*
 * frontier_list_to_values — Unpack a Frontier list into an array.
 *
 * list:       Input list value (listvaluetype)
 * values:     Output array (caller-allocated)
 * max_values: Array capacity
 * ct_out:     Number of values written
 */
boolean frontier_list_to_values(const tyvaluerecord *list,
                                 tyvaluerecord *values,
                                 short max_values, short *ct_out);

/*
 * frontier_values_to_list — Pack an array into a Frontier list.
 */
boolean frontier_values_to_list(const tyvaluerecord *values,
                                 short ct, tyvaluerecord *list);

/*
 * frontier_record_get — Get a field from a Frontier record.
 *
 * record:     Input record value (recordvaluetype)
 * field_name: Field name
 * value:      Output value
 */
boolean frontier_record_get(const tyvaluerecord *record,
                             const char *field_name,
                             tyvaluerecord *value);

/*
 * frontier_record_set — Set a field in a Frontier record.
 */
boolean frontier_record_set(tyvaluerecord *record,
                             const char *field_name,
                             const tyvaluerecord *value);
```

### Error Handling

Errors propagate through a thread-local error string:

```c
/*
 * frontier_lang_set_error — Set the current error message.
 *
 * Called by engine vtable functions on failure. The kernel reads
 * this error and propagates it to the user (error dialog,
 * try/catch, etc.).
 *
 * fmt: printf-style format string
 */
void frontier_lang_set_error(const char *fmt, ...);

/*
 * frontier_lang_get_error — Read the current error message.
 *
 * bserror: Output buffer (bigstring, 255 bytes max)
 */
void frontier_lang_get_error(bigstring bserror);

/*
 * frontier_lang_clear_error — Clear the error state.
 */
void frontier_lang_clear_error(void);
```

Error propagation across language boundaries:

```
JS calls frontier.db.runScript("user.scripts.buggyUT")
  → UserTalk engine executes script, hits error
  → UserTalk sets error: "Can't divide by zero"
  → Kernel propagates error back to JS
  → JS engine receives error, converts to JS Error object
  → JS try/catch can handle it
```

### GIL Interaction Protocol

The GIL protocol follows ADR-014's model, extended for multiple engines:

```c
/*
 * frontier_gil_release — Release the Frontier GIL.
 *
 * Called by engines before blocking I/O or CPU-intensive work
 * that doesn't access Frontier data structures.
 *
 * Returns an opaque state token that must be passed to
 * frontier_gil_reacquire().
 *
 * IMPORTANT: While the GIL is released, the engine MUST NOT:
 * - Access tyvaluerecord values
 * - Read/write ODB data
 * - Call kernel verb functions
 * - Access tythreadglobals
 */
void* frontier_gil_release(void);

/*
 * frontier_gil_reacquire — Reacquire the Frontier GIL.
 *
 * state: Token from frontier_gil_release()
 *
 * This call may block if another thread holds the GIL.
 */
void frontier_gil_reacquire(void *state);

/*
 * frontier_gil_yield — Yield the GIL to other threads.
 *
 * Equivalent to release + reacquire, but allows the scheduler
 * to run other threads. This is the polyglot equivalent of
 * langbackgroundtask().
 *
 * Returns false if the current script should be killed
 * (user pressed Cmd+Period or equivalent).
 */
boolean frontier_gil_yield(void);
```

### ODB Access API

Functions exposed to language engines for reading/writing the Object Database:

```c
/*
 * frontier_odb_get — Read a value from the ODB.
 *
 * path:   Dot-separated ODB path (e.g., "user.prefs.name")
 * value:  Output value
 */
boolean frontier_odb_get(const char *path, tyvaluerecord *value);

/*
 * frontier_odb_set — Write a value to the ODB.
 *
 * path:   Dot-separated ODB path
 * value:  Value to write
 *
 * Creates intermediate tables if necessary.
 */
boolean frontier_odb_set(const char *path, const tyvaluerecord *value);

/*
 * frontier_odb_exists — Check if a path exists in the ODB.
 */
boolean frontier_odb_exists(const char *path, boolean *exists);

/*
 * frontier_odb_delete — Delete a value from the ODB.
 */
boolean frontier_odb_delete(const char *path);

/*
 * frontier_odb_get_type — Get the type of a value at a path.
 */
boolean frontier_odb_get_type(const char *path, tyvaluetype *vtype);

/*
 * frontier_odb_list_table — List children of a table node.
 *
 * path:      ODB path to a table
 * names:     Output array of child names (caller frees)
 * ct_out:    Number of children
 */
boolean frontier_odb_list_table(const char *path,
                                 char ***names, short *ct_out);

/*
 * frontier_odb_run_script — Execute a script stored in the ODB.
 *
 * path:      ODB path to a script node
 * params:    Parameter values (or NULL)
 * ctparams:  Number of parameters
 * result:    Output value
 *
 * Determines the script's language from its signature and
 * dispatches to the appropriate engine.
 */
boolean frontier_odb_run_script(const char *path,
                                 tyvaluerecord *params, short ctparams,
                                 tyvaluerecord *result);
```

### Verb Registration API

Engines use these functions to register kernel verbs callable from scripts:

```c
/*
 * frontier_register_verb_table — Create a verb table.
 *
 * Wraps newfunctionprocessor(). Creates a hash table in the
 * kernel's verb namespace.
 *
 * name:      Table name (e.g., "mylib")
 * callback:  C function dispatching verb calls
 * htable:    Output — handle to the verb table
 *
 * After this call, verbs can be added with frontier_add_verb().
 */
boolean frontier_register_verb_table(const char *name,
                                      langvaluecallback callback,
                                      hdlhashtable *htable);

/*
 * frontier_add_verb — Register a verb in a table.
 *
 * Wraps langaddkeyword().
 *
 * htable:    Verb table (from frontier_register_verb_table)
 * verb_name: Name of the verb
 * verb_index: Dispatch index (passed to the callback)
 */
boolean frontier_add_verb(hdlhashtable htable,
                           const char *verb_name,
                           short verb_index);
```

### Script Lifecycle

The complete lifecycle of a polyglot script in Frontier:

```
┌─────────┐    ┌──────────┐    ┌────────┐    ┌──────┐    ┌─────────┐
│ Create  │───→│ Compile  │───→│ Store  │───→│ Load │───→│ Execute │
│ (edit)  │    │ (engine) │    │ (ODB)  │    │(cache)│    │(engine) │
└─────────┘    └──────────┘    └────────┘    └──────┘    └─────────┘
                                                              │
                                                         ┌────┴────┐
                                                         │  Debug  │
                                                         │(engine) │
                                                         └─────────┘
```

**1. Create** — User writes script source in an editor (built-in or external). Source is text.

**2. Compile** — Engine's `compile()` function converts source to a compiled form. The compiled form is opaque to the kernel (a Handle). For UserTalk, this is a code tree. For JS, it might be bytecode. For Python, `.pyc`-style compiled code.

**3. Store** — The compiled form is cached in memory. The source text is stored in the ODB as an `externalvaluetype` node with `idscriptprocessor` type and the engine's signature.

**4. Load** — When a stored script is needed, the source is loaded from ODB, and `compile()` is called to produce the compiled form. Compiled forms can be cached to avoid recompilation.

**5. Execute** — Engine's `execute()` function runs the compiled form. The GIL is held. Results are returned as `tyvaluerecord`.

**6. Debug** — Engine-specific. Engines that support debugging can implement breakpoints, stepping, and variable inspection. The interface for this is engine-specific and not part of the core vtable (future extension).

### Refactoring UserTalk as a Language Engine

To validate the interface, UserTalk itself should be refactored to implement `FrontierLanguage`:

```c
static FrontierLanguage usertalk_language = {
    .signature      = typeLAND,           /* 'LAND' */
    .name           = "UserTalk",
    .version        = "7.0",
    .file_extension = "ut",
    .init           = usertalk_init,      /* existing langstartup() */
    .shutdown       = usertalk_shutdown,  /* existing langshutdown() */
    .compile        = usertalk_compile,   /* wraps langbuildtree() */
    .dispose_compiled = usertalk_dispose, /* wraps langdisposetree() */
    .execute        = usertalk_execute,   /* wraps langrun() */
    .call_function  = usertalk_call,      /* wraps langrunscript() */
    .register_verbs = NULL,               /* already registered */
    .get_value      = usertalk_get_value,
    .set_value      = usertalk_set_value,
    .yield          = usertalk_yield,     /* wraps langbackgroundtask() */
    .save_thread_state    = NULL,         /* uses tythreadglobals */
    .restore_thread_state = NULL,
};
```

This refactoring proves that the interface is sufficient to support Frontier's most complex language — its native one.

## Open Questions

1. **Compiled form caching** — Should compiled forms be persisted to the ODB alongside source? This would speed up startup but adds cache invalidation complexity.

2. **Vtable versioning** — If the vtable grows over time, how do older engines handle new fields? Possible approach: version field + size field, like COM's `QueryInterface`.

3. **Debugging interface** — Should a `FrontierLanguageDebug` extension vtable be defined for engines that support debugging? Or is debugging entirely engine-internal?

4. **Memory ownership** — The current spec says `compile()` transfers ownership of `hcompiled` to the caller. Should there be a reference-counting model instead, allowing shared compiled forms?

5. **Lazy initialization** — Should `init()` be called at startup or on first use? Lazy init avoids loading engines that aren't used but adds complexity.

## Next Steps

1. Review vtable design against QuickJS and CPython APIs
2. Prototype `FrontierLanguage` struct in a header file
3. Refactor UserTalk to implement the vtable (validation)
4. Implement language registry (array-based, simple)
5. Modify `scriptbuildtree()` to check registry before OSA fallback
