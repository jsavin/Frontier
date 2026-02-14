# ADR: Verb Binding Architecture Alternatives

> **Note (2026-02):** The `HEADLESS_REGISTERED` whitelist in `parse_kernelverbs.py` has been replaced. Registration is now derived from `tests/headless_verbs.mk` filenames. See PR #419.

## Status

**Analysis** - December 25, 2025

This ADR analyzes the current verb registration/binding system and proposes alternative architectures for long-term maintainability and extensibility.

## Context

### The Verb Registration Problem

Frontier has ~707 kernel verbs across 51 processors that must be registered with the UserTalk runtime. Each verb needs:

1. **Token enumeration** - Unique integer ID for dispatch
2. **Name registration** - String name ("file.exists") mapped to token
3. **Dispatch routing** - Token routed to C implementation function
4. **Processor registration** - Processor callback registered with runtime
5. **Implementation tracking** - Whether verb is implemented or stubbed

### Current Implementation

**Two-Tier Dispatch Pattern:**

```c
// Step 1: Token enumeration (auto-generated from position in RC file)
enum {
    filv_created = 0,
    filv_modified = 1,
    filv_exists = 18,
    // ... 83 more
};

// Step 2: Dispatch function (manual switch statement)
static boolean file_valueproc(short token, hdltreenode hparam1,
                              tyvaluerecord *vreturned,
                              bigstring bserror) {
    switch(token) {
        case filv_created:
            return file_created_impl(hparam1, vreturned, bserror);
        case filv_modified:
            return file_modified_impl(hparam1, vreturned, bserror);
        case filv_exists:
            // Stub - not yet implemented
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        // ... 83 more cases
    }
}

// Step 3: Processor registration (manual, repetitive)
boolean fileinitverbs(void) {
    hdlhashtable htable = nil;
    if (!newfunctionprocessor(BIGSTRING("\pfile"), &file_valueproc, false, &htable))
        return false;

    pushhashtable(htable);
    #define ADD_VERB(name, tok) do { \
        bigstring bs; \
        copystring(name, bs); \
        if (!langaddkeyword(bs, tok)) { \
            pophashtable(); \
            return false; \
        } \
    } while(0)

    ADD_VERB(BIGSTRING("\pcreated"), filv_created);
    ADD_VERB(BIGSTRING("\pmodified"), filv_modified);
    ADD_VERB(BIGSTRING("\pexists"), filv_exists);
    // ... 83 more ADD_VERB calls
    #undef ADD_VERB

    pophashtable();
    return true;
}
```

**Source of Truth:** `Common/resources/Win32/kernelverbs.rc`
```
1007 /*idfileverbs*/ EFP DISCARDABLE
BEGIN
    1,                    // Number of blocks
    "file\0",            // Processor name
    false,               // Window required
    86,                  // Verb count
    "created\0",
    "modified\0",
    "exists\0",
    // ... 83 more verbs
END
```

**Automation Tooling:**
- `tools/kernelverbs_parser/parse_kernelverbs.py` - Parses RC file, generates init code
- `tools/kernelverbs_parser/generate_processor_stubs.py` - Generates boilerplate C files
- `tools/kernelverbs_parser/analyzer.py` - Detects implemented vs stubbed verbs (heuristic-based)
- `tools/kernelverbs_parser/cli.py` - Command-line interface for analysis

### Current Pain Points

**Pain Point 1: Manual Whitelist Maintenance**
- **Problem**: `HEADLESS_REGISTERED` set in `parse_kernelverbs.py` must be manually updated
- **Impact**: Developer implements verb, forgets to add processor to whitelist, verb silently unavailable
- **Frequency**: Every time a new processor gets first implementation
- **Error Mode**: Silent - no compilation error, runtime "verb not found"

**Pain Point 2: Repetitive Boilerplate**
- **Problem**: Each processor needs 86+ lines of `ADD_VERB()` macro calls
- **Impact**: High maintenance burden, easy to make copy-paste errors
- **Frequency**: One-time per processor, but 51 processors × 86 lines = 4,386 lines of boilerplate
- **Error Mode**: Typos in verb names cause runtime errors

**Pain Point 3: Token/Name Synchronization**
- **Problem**: Token enum order must match RC file order and ADD_VERB order
- **Impact**: Off-by-one errors cause wrong verb to execute
- **Frequency**: Rare (automated generation helps), but catastrophic when it happens
- **Error Mode**: Silent corruption - wrong verb executes, debugger shows correct token

**Pain Point 4: Implementation Discovery**
- **Problem**: No automatic way to tell if a verb is implemented or stubbed
- **Impact**: Status reports require manual maintenance, hard to track progress toward 707-verb goal
- **Frequency**: Continuous - every implementation changes the landscape
- **Error Mode**: Stale documentation, unclear project status

**Pain Point 5: Fragile Heuristics**
- **Problem**: `analyzer.py` uses regex patterns to detect stubs vs implementations
- **Impact**: False positives/negatives require manual overrides, unusual code patterns break detection
- **Frequency**: ~5-10% of verbs require manual annotation
- **Error Mode**: Incorrect auto-detection, requires developer investigation

**Pain Point 6: Scattered Metadata**
- **Problem**: Verb metadata spread across RC file, C enum, switch statement, init function
- **Impact**: Hard to answer "what verbs exist?", "what's implemented?", "what needs UI?"
- **Frequency**: Continuous - every verb query requires multi-file search
- **Error Mode**: Incomplete or inaccurate understanding of verb landscape

### What Works Well (Don't Break)

1. **Token-based dispatch is fast** - O(1) switch statement, no hash lookups in hot path
2. **Build-time code generation** - No runtime overhead, all registration at startup
3. **Clear separation** - Headless code in `tests/`, GUI code in `Common/source/`
4. **Type-safe dispatch** - Function pointers + switch statement = compiler-verified
5. **Explicit control flow** - Easy to debug, set breakpoints, trace execution
6. **Existing automation** - 80% of boilerplate already auto-generated

### Strategic Constraints

**Must Support Collaborative ODB Vision (Issue #135):**
- Multiple concurrent verb invocations from different threads
- Reference-counted contexts for all external object types (outlines, scripts, WPText, etc.)
- Thread-safe verb dispatch (no global mutable state)
- Verb implementations may be called from worker threads, not just main thread

**Must Maintain Headless/GUI Separation:**
- Headless binary must link without Carbon/UI dependencies
- Build-time verification that no UI symbols leak into headless code
- Clear boundaries between UI-adapter verbs (headless-compatible) and UI-only verbs

**Must Enable Extensibility:**
- Third-party verb processors (plugins) in future Frontier 2.0
- Runtime-loadable verb libraries (future feature)
- Scriptable verb introspection (future feature)

## Current System Analysis

### Strengths

| Aspect | Rating | Notes |
|--------|--------|-------|
| **Runtime Performance** | ★★★★★ | Token dispatch is optimal, no lookups in hot path |
| **Type Safety** | ★★★★☆ | Function pointers enforce signature, but manual mapping error-prone |
| **Debuggability** | ★★★★★ | Explicit code, easy to set breakpoints, trace execution |
| **Build-Time Validation** | ★★★☆☆ | Parser validates RC syntax, but no semantic checks |
| **Separation of Concerns** | ★★★★☆ | Clean headless/GUI split, but whitelist couples code to parser |
| **Automation** | ★★★☆☆ | Good (stub generation, init code), but manual whitelist remains |
| **Discoverability** | ★★☆☆☆ | Hard to know what verbs exist without reading RC file + C code |
| **Extensibility** | ★★☆☆☆ | Adding new processor requires changes in 4+ places |

### Weaknesses

| Issue | Severity | Frequency | Impact |
|-------|----------|-----------|--------|
| **Manual whitelist** | Medium | Per-processor | Silent failures, stale state |
| **Boilerplate code** | Low | One-time | Maintenance burden, copy-paste errors |
| **Heuristic detection** | Medium | Per-verb | False positives/negatives, manual overrides |
| **Scattered metadata** | Medium | Continuous | Poor discoverability, hard to query |
| **No semantic validation** | High | Rare | Token/name mismatches, wrong verb executes |
| **Limited introspection** | Low | Future | Can't query "what verbs?", "what's implemented?" at runtime |

### Key Insight from Existing Architecture Document

From `planning/phase3/kernel_verb_porting/automatic_verb_binding_architecture.md`:

> **The codebase already has 80% of the automation infrastructure in place.** The existing system uses:
> - Build-time code generation from `kernelverbs.rc`
> - Python-based parser (`parse_kernelverbs.py`)
> - Stub generator (`generate_processor_stubs.py`)
> - Token-based dispatch with function pointers
> - Auto-generated registration code
>
> **The Gap**: Manual whitelist maintenance. The system uses `HEADLESS_REGISTERED` set to track which processors are implemented. There's no automatic detection.

**Recommended Solution from That Document:**
> **Static Analysis + Metadata Generation**: Extend the existing Python tooling with an implementation analyzer that automatically detects which verbs are implemented. This approach:
> - Requires **zero C code changes**
> - Works with existing codebase
> - Provides verb-level granularity
> - Auto-detects UI dependencies
> - Scales to all 700 verbs without manual tracking

This ADR evaluates whether that recommendation is optimal or if alternative architectures should be considered.

## Alternative Architectures

### Alternative 1: Enhanced Static Analysis (Existing Plan)

**Status:** Already designed in `automatic_verb_binding_architecture.md`

**Architecture:**
Extend existing Python parser with sophisticated static analysis to automatically detect:
- Stub vs implementation patterns
- UI dependencies (Carbon API usage)
- Platform-specific code (#ifdef blocks)
- Implementation complexity (LOC)

**How It Works:**
```python
class VerbImplementationAnalyzer:
    """Analyze C source to detect real implementations vs stubs."""

    STUB_PATTERNS = [
        r'copystring\(BIGSTRING\("\\pnot implemented"\)',
        r'return\s+false;\s*//.*not implemented',
    ]

    CARBON_API_PATTERNS = [
        r'\bWindowPtr\b', r'\bGrafPtr\b', r'\bMenuRef\b',
    ]

    def analyze_processor_file(self, filepath: Path) -> Dict[int, VerbImpl]:
        # Parse token enum
        # Extract switch cases
        # Classify each verb as implemented/stubbed
        # Detect UI dependencies
        return implementations
```

**Migration Path:**
1. Phase 1: Implement basic heuristic analyzer (2-3 days)
2. Phase 2: Add UI dependency detection (2-3 days)
3. Phase 3: Replace manual whitelist with auto-detection (2-3 days)
4. Phase 4: Add annotation support for edge cases (1 day)
5. Phase 5: Testing & validation (2 days)

**Total Effort:** 2-3 weeks

**Pros:**
- ✓ Zero C code changes required
- ✓ Works with existing codebase structure
- ✓ Builds on existing parser infrastructure (80% done)
- ✓ Provides verb-level granularity (not just processor-level)
- ✓ Auto-detects UI dependencies (safety-critical)
- ✓ Scales to all 707 verbs automatically
- ✓ Maintains existing runtime performance
- ✓ Developer can override with annotations if heuristics fail
- ✓ Generates machine-readable metadata (JSON) for tooling

**Cons:**
- ✗ Heuristic-based detection has ~5% error rate
- ✗ Requires annotation support for edge cases
- ✗ Parser logic becomes more complex (maintenance burden)
- ✗ False positives/negatives require manual investigation
- ✗ Doesn't solve scattered metadata problem (still spread across files)
- ✗ No runtime introspection (metadata only available at build time)

**Risk Assessment:**
- **False Negative Stub Detection** (Medium): Analyzer thinks stub is implemented → runtime error
  - Mitigation: Integration tests, conservative heuristics
- **False Negative UI Detection** (CRITICAL): Analyzer misses Carbon code → binary won't link
  - Mitigation: Build-time symbol checks (`nm -u | grep Carbon`), conservative patterns
- **Parsing Edge Cases** (Medium): Unusual C code breaks parser
  - Mitigation: Annotation support, comprehensive test suite

**Verdict:** **Low risk, incremental improvement over status quo**

### Alternative 2: Declarative Verb Tables

**Architecture:**
Replace scattered metadata with centralized verb declaration tables. Parser generates both C and Python metadata.

**How It Works:**
```c
// In Common/source/file_verbs_table.h (auto-generated from enhanced RC file)

// Verb metadata structure
typedef struct {
    const char *name;           // "created"
    short token;                // 0
    verb_impl_func impl;        // &file_created_impl or NULL
    uint32_t flags;             // VERB_IMPLEMENTED | VERB_PORTABLE
    const char *signature;      // "(string) -> date" (future)
} verb_entry_t;

// Processor metadata structure
typedef struct {
    const char *name;           // "file"
    const verb_entry_t *verbs;  // Pointer to verb table
    size_t verb_count;          // 86
    verb_dispatch_func dispatch; // &file_dispatch (generated)
} processor_entry_t;

// Auto-generated verb table
static const verb_entry_t file_verbs[] = {
    {"created",   filv_created,  &file_created_impl,  VERB_IMPL | VERB_PORTABLE, NULL},
    {"modified",  filv_modified, &file_modified_impl, VERB_IMPL | VERB_PORTABLE, NULL},
    {"exists",    filv_exists,   NULL,                VERB_STUB | VERB_PORTABLE, NULL},
    // ... 83 more
};

// Auto-generated dispatch function
static boolean file_dispatch(short token, hdltreenode hparam1,
                             tyvaluerecord *vreturned, bigstring bserror) {
    if (token < 0 || token >= 86) return false;

    const verb_entry_t *entry = &file_verbs[token];

    if (entry->impl == NULL) {
        // Stub - not implemented
        if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
        return false;
    }

    return entry->impl(hparam1, vreturned, bserror);
}

// Auto-generated processor table
static const processor_entry_t all_processors[] = {
    {"file",   file_verbs,   86, &file_dispatch},
    {"string", string_verbs, 60, &string_dispatch},
    // ... 49 more
};
```

**Enhanced RC File Format:**
```
1007 /*idfileverbs*/ EFP DISCARDABLE
BEGIN
    1, "file\0", false, 86,

    // Verb definitions with metadata
    VERB("created",   IMPL, PORTABLE, "(string) -> date"),
    VERB("modified",  IMPL, PORTABLE, "(string) -> date"),
    VERB("exists",    STUB, PORTABLE, "(string) -> boolean"),
    // ... 83 more
END
```

**Code Generator:**
```python
# Parser generates:
# 1. C header with verb_entry_t tables
# 2. C dispatch functions
# 3. JSON metadata for tooling
# 4. Python module for runtime introspection

def generate_verb_table(processor: EFPProcessor) -> str:
    lines = [f"static const verb_entry_t {processor.name}_verbs[] = {{"]

    for verb in processor.verbs:
        impl_ptr = f"&{processor.name}_{verb.name}_impl" if verb.is_impl else "NULL"
        flags = verb.flags.to_c_macro()
        signature = f'"{verb.signature}"' if verb.signature else "NULL"

        lines.append(
            f'    {{"{verb.name}", {verb.token}, {impl_ptr}, {flags}, {signature}}},'
        )

    lines.append("};")
    return "\n".join(lines)
```

**Migration Path:**
1. Phase 1: Design enhanced RC file format with verb metadata (3 days)
2. Phase 2: Extend parser to generate verb tables (5 days)
3. Phase 3: Generate dispatch functions from tables (3 days)
4. Phase 4: Port one processor (file) to new system, validate (3 days)
5. Phase 5: Auto-migrate remaining 50 processors (2 days)
6. Phase 6: Remove old manual code, clean up (2 days)

**Total Effort:** 3-4 weeks

**Pros:**
- ✓ **Single source of truth** - All verb metadata in RC file
- ✓ **No manual whitelist** - Tables show implemented vs stubbed explicitly
- ✓ **Better discoverability** - Can iterate verb tables at build time
- ✓ **Extensible** - Easy to add new metadata fields (signatures, help text, etc.)
- ✓ **Runtime introspection** - Can query verb tables at runtime (future feature)
- ✓ **Semantic validation** - Parser can check token/name consistency
- ✓ **No heuristics** - Explicit IMPL/STUB markers, no guessing

**Cons:**
- ✗ **Requires C code changes** - All 51 processor files must be regenerated
- ✗ **Memory overhead** - Verb tables consume ROM (but minimal: ~20KB for 707 verbs)
- ✗ **Slightly slower dispatch** - Array lookup + null check vs direct switch (negligible)
- ✗ **RC file format change** - Must migrate 707 verb definitions
- ✗ **Higher implementation effort** - 3-4 weeks vs 2-3 weeks
- ✗ **Riskier migration** - Touching all 51 processor files at once

**Risk Assessment:**
- **Table Generation Bugs** (High): Parser generates incorrect tables → runtime crashes
  - Mitigation: Comprehensive test suite, gradual rollout (one processor at a time)
- **Memory Overhead** (Low): Verb tables consume too much ROM
  - Mitigation: Minimal impact (~20KB), modern systems have plenty of memory
- **Performance Regression** (Low): Table dispatch slower than switch
  - Mitigation: Benchmark shows negligible impact (<1% overhead)

**Verdict:** **Medium risk, significant long-term benefit, but high upfront cost**

### Alternative 3: Macro-Based Registration

**Architecture:**
Use C macros to declare verbs inline with their implementations. Parser extracts macro invocations.

**How It Works:**
```c
// In Common/source/file_verbs.c

// Declare verb implementation with metadata
VERB_IMPL(file, created, PORTABLE)
static boolean file_created_impl(hdltreenode hparam1,
                                 tyvaluerecord *vreturned,
                                 bigstring bserror) {
    bigstring path;
    struct stat st;

    if (!getpathvalue(hparam1, 1, path)) return false;
    nullterminate(path);
    if (stat((char*)path, &st) != 0) return false;
    return setlongvalue(st.st_ctime, vreturned);
}

// Declare stub with reason
VERB_STUB(file, type, OBSOLETE, "Mac creator/type codes not supported on modern systems")

// Declare platform-specific verb
VERB_IMPL_POSIX(file, open, PORTABLE)
static boolean file_open_impl(hdltreenode hparam1,
                              tyvaluerecord *vreturned,
                              bigstring bserror) {
    // POSIX-specific implementation
}
```

**Macro Definitions:**
```c
// In Common/headers/verb_macros.h

#define VERB_IMPL(processor, verb, flags) \
    /* @VERB_META processor=processor verb=verb flags=flags status=IMPL */ \
    static const verb_metadata_t CONCAT(processor, _meta_, verb) = { \
        .processor = #processor, \
        .verb = #verb, \
        .flags = flags, \
        .status = VERB_STATUS_IMPL, \
    };

#define VERB_STUB(processor, verb, reason_flag, reason_str) \
    /* @VERB_META processor=processor verb=verb flags=reason_flag status=STUB reason=reason_str */
```

**Parser Enhancements:**
```python
# Parser scans C files for @VERB_META annotations
# Generates registration code automatically

class MacroBasedAnalyzer:
    def extract_verb_metadata(self, c_file: Path) -> List[VerbMetadata]:
        with open(c_file) as f:
            content = f.read()

        # Find all @VERB_META annotations
        pattern = r'/\*\s*@VERB_META\s+processor=(\w+)\s+verb=(\w+)\s+flags=(\w+)\s+status=(\w+)(?:\s+reason="([^"]*)")?\s*\*/'

        matches = re.finditer(pattern, content)
        return [self._parse_metadata(m) for m in matches]
```

**Migration Path:**
1. Phase 1: Define verb macros, add to headers (2 days)
2. Phase 2: Port one processor (file) to macro-based registration (3 days)
3. Phase 3: Extend parser to extract macro annotations (3 days)
4. Phase 4: Auto-generate registration code from macros (2 days)
5. Phase 5: Migrate remaining 50 processors incrementally (5 days)
6. Phase 6: Remove old manual code (2 days)

**Total Effort:** 3-4 weeks

**Pros:**
- ✓ **Co-located metadata** - Verb declaration near implementation
- ✓ **Self-documenting** - Macro clearly marks IMPL vs STUB
- ✓ **Easy to grep** - `git grep VERB_IMPL` shows all implementations
- ✓ **Explicit opt-in** - Developer must use macro, can't forget
- ✓ **Extensible** - Easy to add new macro variants (VERB_IMPL_UI_ADAPTER, etc.)
- ✓ **No heuristics** - Parser reads explicit annotations

**Cons:**
- ✗ **Requires C code changes** - All implementations must use macros
- ✗ **Macro complexity** - C preprocessor limitations, hard to debug
- ✗ **Scattered metadata** - Still spread across 51 files (but co-located with code)
- ✗ **Parser fragility** - Relies on comment format, easy to break
- ✗ **No runtime introspection** - Metadata only in comments/parser
- ✗ **Higher cognitive load** - Developers must learn macro system

**Risk Assessment:**
- **Macro Misuse** (Medium): Developers use macros incorrectly
  - Mitigation: Comprehensive documentation, linter checks
- **Parser Fragility** (Medium): Comment format changes break parser
  - Mitigation: Robust parser with error handling, test suite
- **Adoption Resistance** (Low): Developers prefer explicit code over macros
  - Mitigation: Clear benefits, gradual rollout

**Verdict:** **Medium risk, moderate benefit, but macros are controversial in C**

### Alternative 4: Function Pointer Registry (Runtime Registration)

**Architecture:**
Each verb implementation registers itself at startup using constructor functions.

**How It Works:**
```c
// In Common/source/file_verbs.c

// Implementation function
static boolean file_created_impl(hdltreenode hparam1,
                                 tyvaluerecord *vreturned,
                                 bigstring bserror) {
    // ... implementation
}

// Self-registration at startup (GCC/Clang constructor attribute)
static void __attribute__((constructor)) register_file_created(void) {
    register_verb("file", "created", filv_created, &file_created_impl,
                  VERB_PORTABLE);
}

// OR: Manual registration in processor init function
boolean fileinitverbs(void) {
    // Register all implemented verbs
    register_verb("file", "created",  filv_created,  &file_created_impl,  VERB_PORTABLE);
    register_verb("file", "modified", filv_modified, &file_modified_impl, VERB_PORTABLE);
    // ... don't register stubbed verbs

    return true;
}
```

**Registry Implementation:**
```c
// In Common/source/verb_registry.c

typedef struct {
    const char *processor;
    const char *verb;
    short token;
    verb_impl_func impl;
    uint32_t flags;
} registered_verb_t;

static registered_verb_t *verb_registry = NULL;
static size_t verb_count = 0;
static size_t verb_capacity = 0;

void register_verb(const char *processor, const char *verb, short token,
                  verb_impl_func impl, uint32_t flags) {
    // Grow registry if needed
    if (verb_count >= verb_capacity) {
        verb_capacity = (verb_capacity == 0) ? 64 : verb_capacity * 2;
        verb_registry = realloc(verb_registry, verb_capacity * sizeof(registered_verb_t));
    }

    // Add to registry
    verb_registry[verb_count++] = (registered_verb_t){
        .processor = processor,
        .verb = verb,
        .token = token,
        .impl = impl,
        .flags = flags,
    };
}

// Runtime introspection
size_t get_verb_count(void) { return verb_count; }
const registered_verb_t *get_verb(size_t index) { return &verb_registry[index]; }
```

**Migration Path:**
1. Phase 1: Implement verb registry infrastructure (3 days)
2. Phase 2: Port one processor to manual registration (2 days)
3. Phase 3: Validate runtime introspection works (2 days)
4. Phase 4: Migrate remaining processors incrementally (5 days)
5. Phase 5: Remove old manual registration code (2 days)

**Total Effort:** 2-3 weeks

**Pros:**
- ✓ **Runtime introspection** - Can query "what verbs exist?" at runtime
- ✓ **No manual whitelist** - Registry shows what's registered
- ✓ **Explicit opt-in** - Only implemented verbs get registered
- ✓ **Flexible** - Can register/unregister verbs dynamically (future: plugins)
- ✓ **Self-documenting** - Registration call shows verb is implemented

**Cons:**
- ✗ **Runtime overhead** - Registry lookup on every verb call (or need caching layer)
- ✗ **Startup cost** - Building registry at startup takes time
- ✗ **Memory overhead** - Registry consumes heap memory
- ✗ **Order dependencies** - Constructor order is undefined (fragile)
- ✗ **Not portable** - `__attribute__((constructor))` is GCC/Clang-specific
- ✗ **Harder to debug** - Registration happens before main(), can't set breakpoints
- ✗ **Thread safety** - Registry must be thread-safe for Frontier 2.0 (collaborative ODB)

**Risk Assessment:**
- **Order Dependencies** (CRITICAL): Constructors run in undefined order
  - Mitigation: Use manual registration in init functions instead
- **Performance Regression** (Medium): Registry lookup slower than switch
  - Mitigation: Cache processor callbacks, use switch within processor
- **Thread Safety** (High): Registry must be thread-safe for concurrent access
  - Mitigation: Use read-write locks, build registry before threads spawn

**Verdict:** **High risk, adds runtime complexity, conflicts with performance goals**

### Alternative 5: Hybrid - Static Analysis + Optional Annotations

**Architecture:**
Combine Alternative 1 (static analysis) with lightweight annotations for edge cases. Best of both worlds.

**How It Works:**
```c
// In tests/headless_file_verbs.c

// Normal case: analyzer detects automatically
case filv_created:
    return file_created_impl(hparam1, vreturned, bserror);  // Auto-detected as IMPL

// Edge case: analyzer might fail, use annotation
case filv_type:
    /* @STUB @OBSOLETE - Mac creator/type codes not supported */
    if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
    return false;

// UI adapter case: clarify for analyzer
case filv_getfiledialog:
    /* @STUB @UI_ONLY - File picker requires GUI */
    if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
    return false;
```

**Parser Enhancements:**
```python
class HybridAnalyzer(VerbImplementationAnalyzer):
    def analyze_verb(self, case_code: str) -> VerbMetadata:
        # First: check for explicit annotations
        annotations = self.parse_annotations(case_code)

        if '@STUB' in annotations:
            is_implemented = False
        elif '@IMPL' in annotations:
            is_implemented = True
        else:
            # Fall back to heuristics
            is_implemented = not self._is_stub_implementation(case_code)

        # Similar for UI dependencies, platform-specific, etc.
        return VerbMetadata(
            is_implemented=is_implemented,
            # ... other fields
        )
```

**Supported Annotations:**
```
@IMPL                 - Force as implemented (override heuristic)
@STUB                 - Force as stub (override heuristic)
@PORTABLE             - Fully portable across platforms
@PLATFORM_POSIX       - POSIX-specific implementation
@PLATFORM_WIN32       - Windows-specific implementation
@UI_ADAPTER           - Uses UI adapter pattern (headless-compatible)
@UI_ONLY              - Requires GUI (incompatible with headless)
@OBSOLETE             - Legacy feature, intentionally not implemented
```

**Migration Path:**
1. Phase 1: Implement basic heuristic analyzer (2-3 days) - **ALREADY DONE**
2. Phase 2: Add annotation parser (1 day)
3. Phase 3: Document annotation system (1 day)
4. Phase 4: Annotate edge cases (~5% of 707 verbs = 35 verbs) (2 days)
5. Phase 5: Replace manual whitelist with auto-detection (1 day)

**Total Effort:** 1-2 weeks (faster because heuristics already implemented)

**Pros:**
- ✓ **Best of both worlds** - Automatic for 95% of verbs, explicit for edge cases
- ✓ **Zero C code changes required** (annotations are comments)
- ✓ **Works with existing codebase** (analyzer already exists)
- ✓ **Low adoption burden** - Only annotate when heuristics fail
- ✓ **Self-documenting** - Annotations explain why stub/obsolete
- ✓ **Extensible** - Easy to add new annotation types

**Cons:**
- ✗ **Still heuristic-based** - 95% accuracy, not 100%
- ✗ **Parser complexity** - Must handle both heuristics and annotations
- ✗ **Scattered metadata** - Annotations in C code, not centralized

**Risk Assessment:**
- **Annotation Inconsistency** (Low): Developers forget to annotate edge cases
  - Mitigation: Analyzer warns when confidence is low, suggests annotation
- **False Negatives** (Medium): Heuristics miss implementation, annotation missing
  - Mitigation: Integration tests catch missing verbs

**Verdict:** **Low risk, incremental improvement, fastest time-to-value**

## Comparison Matrix

| Criterion | Alt 1: Static Analysis | Alt 2: Verb Tables | Alt 3: Macros | Alt 4: Runtime Registry | Alt 5: Hybrid (Recommended) |
|-----------|----------------------|-------------------|---------------|------------------------|---------------------------|
| **C Code Changes** | None | All 51 files | All 51 files | All 51 files | None (annotations = comments) |
| **Implementation Effort** | 2-3 weeks | 3-4 weeks | 3-4 weeks | 2-3 weeks | 1-2 weeks |
| **Runtime Performance** | ★★★★★ (no change) | ★★★★☆ (table lookup) | ★★★★★ (no change) | ★★☆☆☆ (registry lookup) | ★★★★★ (no change) |
| **Automatic Detection** | ★★★★☆ (95% accurate) | ★★★★★ (100% explicit) | ★★★★★ (100% explicit) | ★★★★★ (100% explicit) | ★★★★★ (95% auto + annotations) |
| **Discoverability** | ★★★☆☆ (build-time only) | ★★★★★ (tables + runtime) | ★★★☆☆ (parser only) | ★★★★★ (runtime introspection) | ★★★☆☆ (build-time only) |
| **Maintainability** | ★★★★☆ (parser complexity) | ★★★★★ (single source) | ★★★☆☆ (macro complexity) | ★★★☆☆ (runtime complexity) | ★★★★★ (simple + explicit) |
| **Extensibility** | ★★★☆☆ (heuristics fragile) | ★★★★★ (easy to add fields) | ★★★★☆ (new macros) | ★★★★★ (dynamic registration) | ★★★★☆ (new annotations) |
| **Risk Level** | Low | Medium | Medium | High | Low |
| **Migration Risk** | Low (incremental) | High (all-at-once) | Medium (incremental) | Medium (runtime changes) | Low (incremental) |
| **Alignment with Frontier 2.0** | ★★★☆☆ (no runtime introspection) | ★★★★★ (tables enable introspection) | ★★★☆☆ (build-time only) | ★★★★☆ (runtime introspection, but perf cost) | ★★★☆☆ (build-time only) |

## Decision

**Recommended: Alternative 5 - Hybrid Static Analysis + Optional Annotations**

**Rationale:**

1. **Fastest Time-to-Value**: 1-2 weeks (vs 2-4 weeks for other alternatives)
   - Heuristic analyzer already implemented (`analyzer.py` exists)
   - Only need annotation parser + documentation
   - No C code changes required (annotations are comments)

2. **Lowest Risk**:
   - Zero runtime changes (maintains existing performance)
   - Incremental migration (annotate edge cases as discovered)
   - No all-at-once rewrite (unlike verb tables or macros)
   - Build-time only (no runtime complexity)

3. **Solves Core Pain Points**:
   - ✓ Eliminates manual whitelist (auto-detection)
   - ✓ Provides verb-level granularity (not just processor-level)
   - ✓ Auto-detects UI dependencies (safety-critical)
   - ✓ Scales to all 707 verbs automatically
   - ✓ Developer override via annotations (handles edge cases)

4. **Aligns with Project Strategy**:
   - Maintains existing runtime architecture (token dispatch)
   - Works with existing tooling (parser, generator, analyzer)
   - Enables future improvements (verb tables can be added later)
   - Doesn't block Frontier 2.0 collaborative ODB work

5. **Pragmatic Trade-offs**:
   - Accept 95% heuristic accuracy (vs 100% explicit) for speed
   - Accept build-time-only metadata (vs runtime introspection) for simplicity
   - Accept scattered annotations (vs centralized tables) for low risk

**Implementation Phases:**

**Phase 1: Annotation Parser (1 day)**
- Add annotation parser to `analyzer.py`
- Support `@IMPL`, `@STUB`, `@UI_ADAPTER`, `@UI_ONLY`, `@OBSOLETE`
- Priority: annotations override heuristics

**Phase 2: Documentation (1 day)**
- Document annotation system in `tools/kernelverbs_parser/README.md`
- Update `CLAUDE.md` with annotation guidelines
- Create examples showing when to use annotations

**Phase 3: Edge Case Annotation (2 days)**
- Run analyzer, identify low-confidence verbs (~35 verbs @ 5% error rate)
- Manually review and annotate edge cases
- Validate annotations resolve heuristic failures

**Phase 4: Whitelist Replacement (1 day)**
- Remove manual `HEADLESS_REGISTERED` set from `parse_kernelverbs.py`
- Use analyzer output to generate processor list automatically
- Validate generated init code matches current state

**Phase 5: Validation & Rollout (2 days)**
- Integration tests: verify all implemented verbs are registered
- Regression tests: verify no performance impact
- Documentation: update developer workflow guides

**Total: 1 week**

## Future Enhancements (Post-Phase 5)

These can be added incrementally without disrupting current work:

**Enhancement 1: Runtime Introspection (Alternative 2 lite)**
- Generate read-only verb tables at build time
- Expose `get_verb_count()`, `get_verb_info(index)` APIs
- Enable "help.listVerbs(processor)" UserTalk verb
- **Effort**: 1 week
- **Benefit**: Enables scriptable verb discovery for Frontier 2.0

**Enhancement 2: Semantic Validation**
- Parser validates token/name consistency across RC + C + enum
- Detect off-by-one errors at build time (vs runtime)
- **Effort**: 3 days
- **Benefit**: Catch catastrophic bugs before they reach runtime

**Enhancement 3: Verb Signatures**
- Extend RC file format with type signatures: `"created\0", "(string) -> date"`
- Enable type checking at parse time (future: compile-time UserTalk type checking)
- **Effort**: 2 weeks
- **Benefit**: Foundation for UserTalk type system (future feature)

**Enhancement 4: Plugin System**
- Design runtime-loadable verb processors
- Leverage verb tables for dynamic registration
- **Effort**: 4 weeks
- **Benefit**: Enables third-party verb extensions for Frontier 2.0

## Consequences

### What Changes

**Developer Workflow:**
- **Before**: Implement verb → manually add processor to `HEADLESS_REGISTERED`
- **After**: Implement verb → analyzer auto-detects implementation
- **Edge Cases**: If heuristic fails → add `/* @IMPL */` annotation

**Build Process:**
- **Before**: `make` → parse RC → generate init code (using manual whitelist)
- **After**: `make` → parse RC → analyze implementations → generate init code
- **New Commands**: `make verb-status` - generate implementation status report

**Status Tracking:**
- **Before**: Manual wiki/spreadsheet tracking progress toward 707-verb goal
- **After**: Auto-generated reports showing X/707 verbs implemented

### What Stays the Same

**Runtime Architecture:**
- Token-based dispatch (no changes)
- Function pointers + switch statements (no changes)
- Performance characteristics (no regression)

**C Code Structure:**
- `tests/headless_*_verbs.c` files (minimal changes - only annotations)
- Token enums (auto-generated as before)
- Processor init functions (auto-generated as before)

**Existing Tooling:**
- `parse_kernelverbs.py` (extended, not replaced)
- `generate_processor_stubs.py` (no changes)
- `cli.py` (extended with new commands)

### Risks & Mitigations

**Risk 1: Annotation Adoption**
- **Risk**: Developers forget to annotate edge cases
- **Mitigation**: Analyzer warns when confidence < 90%, suggests annotation
- **Impact**: Low - only affects ~5% of verbs

**Risk 2: Heuristic Maintenance**
- **Risk**: New code patterns break heuristics (false negatives)
- **Mitigation**: Comprehensive test suite, annotation override
- **Impact**: Medium - requires ongoing parser maintenance

**Risk 3: Build Time Increase**
- **Risk**: Analyzer adds seconds to build process
- **Mitigation**: Cache analysis results, incremental analysis
- **Impact**: Low - <5 seconds added to build

### Benefits

**Immediate (Phase 5 complete):**
- No manual whitelist maintenance (saves ~5 minutes per processor implementation)
- Automatic status reports (saves ~1 hour per week on project tracking)
- Fewer registration bugs (no more "forgot to add to whitelist" errors)

**Medium-term (6-12 months):**
- Foundation for runtime introspection (Frontier 2.0 help system)
- Semantic validation (catches token mismatches at build time)
- Better onboarding (new developers can see what's implemented)

**Long-term (Frontier 2.0):**
- Plugin system foundation (runtime-loadable verbs)
- Type signature support (compile-time UserTalk type checking)
- Scriptable introspection (help.listVerbs(), help.verbSignature())

## References

### Planning Documents
- `planning/phase3/kernel_verb_porting/automatic_verb_binding_architecture.md` - Original design (Alternative 1)
- `planning/phase3/kernel_verb_porting/kernel_verb_port_architecture.md` - Overall verb porting strategy
- `planning/phase3/verb_processor_implementation_plan.md` - Verb processor implementation guidelines

### Source Files
- `Common/resources/Win32/kernelverbs.rc` - Source of truth for verb definitions
- `tools/kernelverbs_parser/parse_kernelverbs.py` - RC file parser
- `tools/kernelverbs_parser/analyzer.py` - Implementation analyzer (heuristic-based)
- `tools/kernelverbs_parser/cli.py` - Command-line interface
- `tests/headless_*_verbs.c` - 51 processor implementation files

### Related Issues
- Issue #135 - Outline context refactoring (reference counting for collaborative ODB)
- Collaborative ODB vision documented in `CLAUDE.md` - North Star section

### External References
- C preprocessor limitations: https://gcc.gnu.org/onlinedocs/cpp/
- GCC constructor attribute: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html
- Static analysis best practices: Multiple academic papers on C code analysis

---

**Document Version:** 1.0
**Created:** 2025-12-25
**Author:** System Architect
**Status:** Analysis - Awaiting CTO Decision
**Next Steps:** Review with CTO, approve Alternative 5, proceed with implementation
