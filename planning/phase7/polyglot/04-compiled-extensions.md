# Compiled Extensions — Go, Rust & Shared Library Plugins

Status
- State: Draft
- Phase: Phase 7 — Polyglot Scripting
- Last Updated: 2026-02-13
- Owner: Jake
- Notes: Specification only. Compiled languages as extensions, not scripting engines.

Related Docs
- `planning/phase7/polyglot/00-polyglot-vision.md` — Vision & Architecture Overview
- `planning/phase7/polyglot/01-language-interface-spec.md` — Abstract Language Interface
- `planning/phase7/polyglot/02-javascript-integration.md` — JavaScript Integration
- `planning/phase7/polyglot/03-python-integration.md` — Python Integration
- `Common/source/kernel_verbs_headless.c` — EFP registration pattern
- `Common/headers/lang.h` — `tyvaluerecord`, `tyvaluetype`

Change Log
- 2026-02-13: Initialized document.

## Overview

Go and Rust are compiled languages with strong ecosystems for systems programming, networking, and performance-critical code. However, they are fundamentally different from JavaScript and Python — they don't support `eval()`, have no REPL, and require compilation to native code before execution.

This makes them unsuitable as **scripting** languages in the traditional sense. You can't type Go code into an ODB script node and run it dynamically. Instead, this document specifies how Go and Rust (and any other compiled language that can produce C-compatible shared libraries) can be used as **extension** languages — compiled plugins that register new verb tables with the Frontier kernel.

Additionally, this document covers **Rust-implemented scripting engines** (Boa, Rhai) as a potential future path for embedding additional scripting capabilities.

### Relationship to Existing EFP Pattern

Frontier already has a plugin-like pattern: the External Function Processor (EFP). Each verb family (string, file, math, etc.) is registered via `newfunctionprocessor()` and `langaddkeyword()`. Compiled extensions are a natural evolution of this pattern — instead of being compiled into the Frontier binary, verb tables are loaded from shared libraries at runtime.

## Decisions

### D1: Compiled Languages Are Extensions, Not Scripting Engines

Go and Rust do not implement the `FrontierLanguage` vtable. They don't compile or execute scripts. They provide pre-compiled verb implementations loaded as shared libraries.

### D2: C ABI as the Interface

All compiled extensions must expose functions with C linkage (`extern "C"` in Rust/C++, `//export` in Go with cgo). This is the universal ABI for plugin systems.

### D3: Shared Library Format

Extensions are distributed as shared libraries:
- macOS: `.dylib`
- Linux: `.so`
- Windows: `.dll` (future)

### D4: Plugin Lifecycle Managed by Kernel

The kernel loads, initializes, and unloads plugins. Plugins don't manage their own lifecycle.

## Details

### Why Go/Rust Don't Work as Scripting Languages

| Requirement | Go | Rust | JS/Python |
|---|---|---|---|
| Dynamic `eval()` | No | No | Yes |
| REPL / interactive | No (compile required) | No (compile required) | Yes |
| Runtime compilation | No (ahead-of-time only) | No (ahead-of-time only) | Yes |
| Script storage in ODB | Not feasible | Not feasible | Text → compile → execute |
| Hot reload | Requires recompilation | Requires recompilation | Re-execute source |

**Go**: Compiles to native binaries. No interpreter. The `go run` command compiles then executes — it's not a scripting runtime. Go's plugin system (`plugin.Open()`) only works on Linux. cgo can produce shared libraries with C ABI.

**Rust**: Compiles to native binaries. No interpreter. `rustc` produces `.dylib`/`.so` files with `#[no_mangle] extern "C"` functions callable from C.

The right model for these languages is: developer writes code → compiles to `.dylib` → Frontier loads the library → new verbs become available.

### Compiled Extension ABI

A compiled extension is a shared library that exports a specific set of C-ABI functions:

```c
/*
 * frontier_extension_info — Return extension metadata.
 *
 * Called immediately after the library is loaded.
 * The extension must return a static struct with its identity.
 */
typedef struct FrontierExtensionInfo {
    const char *name;           /* "my-crypto-verbs" */
    const char *version;        /* "1.0.0" */
    const char *author;         /* "Jane Developer" */
    const char *description;    /* "Cryptographic verb extensions" */
    unsigned int abi_version;   /* FRONTIER_EXTENSION_ABI_VERSION */
} FrontierExtensionInfo;

/*
 * Every extension must export these three functions:
 */

/* Return extension metadata. Must be callable before init. */
FrontierExtensionInfo* frontier_extension_info(void);

/* Initialize the extension. Register verb tables here. */
boolean frontier_extension_init(const FrontierExtensionAPI *api);

/* Shut down the extension. Free resources. */
boolean frontier_extension_shutdown(void);
```

### Extension API (Provided by Kernel)

The kernel passes an API struct to `frontier_extension_init()`. This provides the functions the extension needs to register verbs and interact with Frontier:

```c
/*
 * FrontierExtensionAPI — functions provided by the kernel
 *                        to compiled extensions.
 *
 * This struct is the extension's view of the Frontier runtime.
 * It provides verb registration, value creation, ODB access,
 * and error handling.
 */
typedef struct FrontierExtensionAPI {
    unsigned int abi_version;  /* For compatibility checking */

    /* ── Verb Registration ── */
    boolean (*register_verb_table)(const char *name,
                                    langvaluecallback callback,
                                    hdlhashtable *htable);
    boolean (*add_verb)(hdlhashtable htable,
                         const char *verb_name,
                         short verb_index);

    /* ── Value Creation ── */
    boolean (*set_string_value)(const char *s, tyvaluerecord *v);
    boolean (*set_long_value)(long n, tyvaluerecord *v);
    boolean (*set_double_value)(double d, tyvaluerecord *v);
    boolean (*set_bool_value)(boolean b, tyvaluerecord *v);

    /* ── Value Reading ── */
    boolean (*get_string_value)(const tyvaluerecord *v,
                                 char *buf, size_t bufsize);
    boolean (*get_long_value)(const tyvaluerecord *v, long *out);
    boolean (*get_double_value)(const tyvaluerecord *v, double *out);
    boolean (*get_bool_value)(const tyvaluerecord *v, boolean *out);

    /* ── Parameter Access ── */
    boolean (*get_param_value)(hdltreenode params, short n,
                                tyvaluerecord *v);
    short   (*count_params)(hdltreenode params);

    /* ── ODB Access ── */
    boolean (*odb_get)(const char *path, tyvaluerecord *v);
    boolean (*odb_set)(const char *path, const tyvaluerecord *v);
    boolean (*odb_exists)(const char *path, boolean *exists);

    /* ── Error Handling ── */
    void    (*set_error)(const char *fmt, ...);

    /* ── GIL ── */
    void*   (*gil_release)(void);
    void    (*gil_reacquire)(void *state);

} FrontierExtensionAPI;

/* ABI version — increment when the API struct changes */
#define FRONTIER_EXTENSION_ABI_VERSION 1
```

### Example: Rust Extension

A Rust extension that adds cryptographic hash verbs:

```rust
// frontier_crypto_ext/src/lib.rs

use std::os::raw::{c_char, c_short, c_long};
use std::ffi::{CStr, CString};

// Opaque types from Frontier
#[repr(C)]
pub struct tyvaluerecord { /* opaque */ _data: [u8; 32] }
#[repr(C)]
pub struct hdltreenode { /* opaque */ _ptr: *mut () }
#[repr(C)]
pub struct hdlhashtable { /* opaque */ _ptr: *mut () }

// Frontier API pointer (set during init)
static mut API: *const FrontierExtensionAPI = std::ptr::null();

#[repr(C)]
pub struct FrontierExtensionInfo {
    pub name: *const c_char,
    pub version: *const c_char,
    pub author: *const c_char,
    pub description: *const c_char,
    pub abi_version: u32,
}

// Extension metadata
static INFO: FrontierExtensionInfo = FrontierExtensionInfo {
    name: b"crypto\0".as_ptr() as *const c_char,
    version: b"1.0.0\0".as_ptr() as *const c_char,
    author: b"Frontier Contributors\0".as_ptr() as *const c_char,
    description: b"SHA-256 and MD5 hash verbs\0".as_ptr() as *const c_char,
    abi_version: 1,
};

#[no_mangle]
pub extern "C" fn frontier_extension_info() -> *const FrontierExtensionInfo {
    &INFO
}

const VERB_SHA256: c_short = 0;
const VERB_MD5: c_short = 1;

#[no_mangle]
pub extern "C" fn frontier_extension_init(
    api: *const FrontierExtensionAPI
) -> bool {
    unsafe { API = api; }

    // Register verb table "crypto" with verbs sha256, md5
    let mut htable: hdlhashtable = unsafe { std::mem::zeroed() };
    unsafe {
        let register = (*API).register_verb_table;
        if !register(b"crypto\0".as_ptr() as _, crypto_callback, &mut htable) {
            return false;
        }
        let add = (*API).add_verb;
        if !add(htable, b"sha256\0".as_ptr() as _, VERB_SHA256) {
            return false;
        }
        if !add(htable, b"md5\0".as_ptr() as _, VERB_MD5) {
            return false;
        }
    }
    true
}

#[no_mangle]
pub extern "C" fn frontier_extension_shutdown() -> bool {
    true
}

extern "C" fn crypto_callback(
    verb_index: c_short,
    params: hdltreenode,
    result: *mut tyvaluerecord,
    _error: *mut c_char,
) -> bool {
    match verb_index {
        0 => crypto_sha256(params, result),
        1 => crypto_md5(params, result),
        _ => false,
    }
}

fn crypto_sha256(params: hdltreenode, result: *mut tyvaluerecord) -> bool {
    // Get string parameter
    let mut buf = [0u8; 4096];
    unsafe {
        let mut val: tyvaluerecord = std::mem::zeroed();
        if !((*API).get_param_value)(params, 1, &mut val) {
            return false;
        }
        if !((*API).get_string_value)(&val, buf.as_mut_ptr() as _, buf.len()) {
            return false;
        }
    }

    // Compute SHA-256 (using sha2 crate)
    use sha2::{Sha256, Digest};
    let input = unsafe { CStr::from_ptr(buf.as_ptr() as _) };
    let hash = Sha256::digest(input.to_bytes());
    let hex = format!("{:x}\0", hash);

    // Return result
    unsafe {
        ((*API).set_string_value)(hex.as_ptr() as _, result)
    }
}

fn crypto_md5(params: hdltreenode, result: *mut tyvaluerecord) -> bool {
    // Similar to sha256 but using md5 crate
    // ...
    true
}
```

After compiling (`cargo build --release`), the result is `libfrontier_crypto_ext.dylib`. Place it in Frontier's extension directory and it registers `crypto.sha256()` and `crypto.md5()` as kernel verbs callable from any language.

### Example: Go Extension

```go
package main

// #include "frontier_extension.h"
import "C"
import (
    "crypto/sha256"
    "fmt"
    "unsafe"
)

var api *C.FrontierExtensionAPI

//export frontier_extension_info
func frontier_extension_info() *C.FrontierExtensionInfo {
    return &C.FrontierExtensionInfo{
        name:        C.CString("go-crypto"),
        version:     C.CString("1.0.0"),
        author:      C.CString("Frontier Contributors"),
        description: C.CString("Go-based crypto verbs"),
        abi_version: 1,
    }
}

//export frontier_extension_init
func frontier_extension_init(a *C.FrontierExtensionAPI) C.boolean {
    api = a
    // Register verbs using api.register_verb_table, api.add_verb
    // ...
    return C.boolean(1)
}

//export frontier_extension_shutdown
func frontier_extension_shutdown() C.boolean {
    return C.boolean(1)
}

func main() {} // Required by cgo but unused
```

**Go caveats**:
- Go's runtime (goroutine scheduler, GC) starts when the library loads
- This adds ~5 MB memory overhead
- Go's GC may pause the thread (problematic under Frontier's GIL)
- cgo has overhead per call (~100ns)
- Not recommended for high-frequency verb calls

### Plugin Loading and Lifecycle

```
┌──────────┐   ┌──────────┐   ┌───────────────┐   ┌──────────┐
│ Discover │──→│  Load    │──→│  Verify ABI   │──→│  Init    │
│ (.dylib) │   │ (dlopen) │   │  (version ck) │   │ (register│
│          │   │          │   │               │   │  verbs)  │
└──────────┘   └──────────┘   └───────────────┘   └──────────┘
                                                        │
                     ┌──────────┐   ┌──────────┐        │
                     │ Shutdown │←──│  Active  │←───────┘
                     │ (dlclose)│   │ (serving │
                     └──────────┘   │  verbs)  │
                                    └──────────┘
```

**1. Discover** — At startup, Frontier scans an extension directory for shared libraries:
- `~/.frontier/extensions/` (user extensions)
- `/usr/local/lib/frontier/extensions/` (system extensions)
- Configurable via `system.prefs.extensionPaths`

**2. Load** — `dlopen()` loads the shared library.

**3. Verify ABI** — Call `frontier_extension_info()`. Check `abi_version` matches `FRONTIER_EXTENSION_ABI_VERSION`. If not, log a warning and skip.

**4. Init** — Call `frontier_extension_init(&api)`. The extension registers its verb tables. If init fails, unload the library.

**5. Active** — The extension's verbs are callable from any scripting language via the normal verb dispatch path.

**6. Shutdown** — At process exit (or on demand), call `frontier_extension_shutdown()`, then `dlclose()`.

```c
/* Kernel-side plugin loading (sketch) */
boolean frontier_load_extension(const char *path) {
    void *handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        log_error("Failed to load extension %s: %s", path, dlerror());
        return false;
    }

    /* Look up required symbols */
    typedef FrontierExtensionInfo* (*info_fn)(void);
    typedef boolean (*init_fn)(const FrontierExtensionAPI*);

    info_fn get_info = dlsym(handle, "frontier_extension_info");
    init_fn do_init  = dlsym(handle, "frontier_extension_init");

    if (!get_info || !do_init) {
        log_error("Extension %s missing required exports", path);
        dlclose(handle);
        return false;
    }

    /* Verify ABI version */
    FrontierExtensionInfo *info = get_info();
    if (info->abi_version != FRONTIER_EXTENSION_ABI_VERSION) {
        log_error("Extension %s ABI version %d != expected %d",
                  path, info->abi_version, FRONTIER_EXTENSION_ABI_VERSION);
        dlclose(handle);
        return false;
    }

    /* Initialize */
    if (!do_init(&extension_api)) {
        log_error("Extension %s init failed", path);
        dlclose(handle);
        return false;
    }

    log_info("Loaded extension: %s v%s (%s)",
             info->name, info->version, info->description);

    /* Track for shutdown */
    register_loaded_extension(handle, info);
    return true;
}
```

### Comparison with Current EFP Pattern

| Aspect | Current EFP | Compiled Extension |
|---|---|---|
| **Location** | Compiled into Frontier binary | Separate `.dylib` file |
| **Registration** | `newfunctionprocessor()` + `langaddkeyword()` | Same, via `FrontierExtensionAPI` |
| **Verb callback** | `langvaluecallback` signature | Same signature |
| **Discovery** | Hardcoded in `initverbs()` | Scanned from extension directory |
| **Language** | C (same as Frontier) | Any language with C ABI |
| **Deployment** | Requires Frontier rebuild | Drop `.dylib` into extension dir |
| **Versioning** | Tied to Frontier version | Independent (ABI versioned) |

The compiled extension model is a superset of the EFP pattern. Existing EFPs could theoretically be moved into extensions without changing their internal implementation.

### Rust-Implemented Scripting Engines

An interesting middle ground: scripting engines written in Rust that embed in Frontier as extensions.

#### Boa (JavaScript in Rust)

[Boa](https://github.com/nickel-lang/boa) is a JavaScript engine written in Rust. It could be used as the JS engine instead of QuickJS:

- **Pros**: Rust safety guarantees, active development, ES2023+ support
- **Cons**: Slower than QuickJS (interpreter), less mature, Rust build dependency

#### Rhai (Rust-native scripting)

[Rhai](https://github.com/rhaiscript/rhai) is a scripting language designed for embedding in Rust applications:

- **Pros**: Tiny footprint, designed for embedding, no GC, safe sandboxing
- **Cons**: Custom language (not JS or Python), small community, learning curve

#### Assessment

These are interesting future options but not recommended for the initial polyglot effort. The primary value is in supporting mainstream languages (JS, Python) that developers already know. Rust-native engines could be added later as additional `FrontierLanguage` implementations if there's demand.

### Security Considerations

Compiled extensions run with full process privileges. Unlike sandboxed scripting, a malicious extension can:
- Access all memory in the Frontier process
- Call any system API
- Modify or corrupt ODB data
- Crash the process

**Mitigations**:
1. **Extension signing** — Future: require extensions to be signed by a trusted authority
2. **Extension directory permissions** — The extension directory should only be writable by the user
3. **ABI version check** — Prevents loading incompatible extensions
4. **User approval** — Frontier should log and optionally prompt before loading extensions

These are not required for the initial implementation but should be planned for.

## Open Questions

1. **Hot reload** — Can extensions be unloaded and reloaded without restarting Frontier? `dlclose()` is unreliable for libraries with global state. Practical?

2. **Extension discovery UX** — How does the user know what extensions are available? A `system.extensions` table in the ODB? A verb like `frontier.sys.listExtensions()`?

3. **Dependency management** — If Extension A depends on Extension B, how is load order determined? Explicit dependency declaration in `FrontierExtensionInfo`?

4. **Cross-platform build** — Should Frontier provide a build template (CMake, Cargo.toml) for extension authors? This would lower the barrier to entry.

5. **Extension marketplace** — Long-term: should there be a package registry for Frontier extensions (like crates.io or npm)? Far future but worth noting.

6. **Sandboxing** — Can extensions be loaded in a restricted mode (e.g., no file I/O, no network)? This would require OS-level sandboxing (seccomp on Linux, sandbox-exec on macOS).

## Next Steps

1. Define the `FrontierExtensionAPI` header file
2. Build a minimal C extension as a test (before Rust/Go)
3. Implement `dlopen`-based extension loading in the kernel
4. Port one existing EFP (e.g., math verbs) to an extension as validation
5. Build a Rust extension example with Cargo
6. Document the extension authoring workflow
