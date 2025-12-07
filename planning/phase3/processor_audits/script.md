# Processor Audit: `script`

**Status:** ⚠️ **MIXED** (13 Kernel Verbs + 2 Scripts, ~60% Headless-Compatible)
**Audit Date:** 2025-12-06
**Auditor:** Claude (Haiku 4.5)

---

## Basic Information

| Property | Value |
|----------|-------|
| **Processor Name** | `script` |
| **EFP ID** | 1000 (shared resource block) |
| **Verb Count** | 13 (kernel) + 2 (utility scripts) = 15 total |
| **Window Required** | YES (but core functionality can be headless) |
| **Documentation** | [script/](../../../docs/usertalk/docserver.userland.com/script/index.html) |
| **Script Implementation** | `system.verbs.builtins.script` (Frontier.root) |

---

## Category Assessment

**Category:** ⚠️ **Script Compilation & Debugging** (Mostly Headless, Some GUI)

**Rationale:**
Script processor provides operations for compiling, decompiling, and managing UserTalk scripts. All core operations are headless-compatible: compilation/decompilation, source access, breakpoint metadata management, and profiling. Breakpoints and profiling are NOT editor-dependent; they're metadata storage and performance analysis tools essential for headless automation and web server profiling.

**Headless Compatibility:** ✅ **HIGH** (~92% compatible, ~8% optional)

**Core Headless-Compatible Verbs:**
- compile, uncompile, getCode, getLanguage, setLanguage (5 core compilation)
- makeComment, uncomment, isComment (3 formatting)
- getBreakpoint, setBreakpoint, clearBreakpoint (3 breakpoint metadata)
- startProfile, stopProfile (2 profiling tools - essential for server-side performance analysis)
- scriptToOutline (1 data transformation)
- Total Headless: ~13 of 13 (100%)

**Optional/Enhancement Verbs:**
- None - all core verbs are headless-compatible

---

## Verb Inventory

### Kernel Verbs (13 from kernelverbs.rc)

| Category | Verb | Status | Description |
|----------|------|--------|-------------|
| **Compilation** | compile | ✅ Headless | Compile script source to bytecode |
| | uncompile | ✅ Headless | Decompile bytecode to source |
| **Access** | getCode | ✅ Headless | Get script source code |
| | getLanguage | ✅ Headless | Get script language (UserTalk, AppleScript, etc.) |
| | setLanguage | ✅ Headless | Change script language |
| **Comments** | makeComment | ✅ Headless | Convert lines to comments |
| | uncomment | ✅ Headless | Remove comment markers |
| | isComment | ✅ Headless | Check if line is comment |
| **Debugging** | getBreakpoint | ✅ Headless | Get breakpoint metadata on script-outline headlines |
| | setBreakpoint | ✅ Headless | Set breakpoint metadata on script-outline headlines |
| | clearBreakpoint | ✅ Headless | Clear breakpoint metadata on script-outline headlines |
| **Profiling** | startProfile | ✅ Headless | Start execution profiler (essential for web server profiling) |
| | stopProfile | ✅ Headless | Stop profiler and return performance statistics |
| **Data Xform** | scriptToOutline | ✅ Headless | Convert script text to outline structure |

### Utility Scripts (2 UserTalk implementations)

| Script | Headless | Description |
|--------|----------|-------------|
| newScriptObject | ✅ YES | Create new script object at address |
| scriptToOutline | ✅ YES | Convert script text to outline structure (pure data transformation) |
| removeSource | ✅ YES | Remove script source (keep compiled) |

---

## Implementation Analysis

### Complexity: **MEDIUM** (Compiler + Editor Integration)

### Dependencies

- **Other Processors:** NONE (foundational for UserTalk)
- **Compilation Engine:** UserTalk compiler (C implementation)
- **GUI/Window Context:** YES (for breakpoints/profiling, but not required for core)

### Key Implementation Notes

**Architecture:**

Script objects in Frontier contain:
1. **Source Code** - Human-readable UserTalk text
2. **Compiled Bytecode** - Intermediate representation for runtime
3. **Language** - UserTalk, AppleScript, or binary
4. **Metadata** - Breakpoints, comments, language info

**Core Operations:**

1. **Compilation** - Parse UserTalk source, generate bytecode
   - compile(adrScript) - Compile source code
   - uncompile(adrScript) - Decompile bytecode back to source
   - getCode(adrScript) - Read source text
   - setLanguage(adrScript, lang) - Change script language

2. **Comment Management** - Text manipulation
   - makeComment(adrScript, lineRange) - Add // to lines
   - uncomment(adrScript, lineRange) - Remove // from lines
   - isComment(adrScript, lineNum) - Check if line is comment

3. **Debugging** - Breakpoint metadata management
   - getBreakpoint(adrScript, lineNum) - Query breakpoint status on script-outline headlines
   - setBreakpoint(adrScript, lineNum) - Set breakpoint metadata on script-outline headlines
   - clearBreakpoint(adrScript, lineNum) - Clear breakpoint metadata on script-outline headlines
   - Pure metadata operations; no GUI required

4. **Profiling** - Performance analysis and reporting
   - startProfile() - Begin execution timing for all verb calls
   - stopProfile() - End timing and return verb call statistics
   - Essential for server-side performance analysis (e.g., web server profiling via /profile/ path)

5. **Data Transformation** - Script-to-structure conversion
   - scriptToOutline() - Convert script text to outline structure
   - Pure data transformation, no GUI required

**Headless Strategy:**
For headless operation, ALL verbs are supported:
- Use compile/uncompile for script operations
- Use getCode/setLanguage for script manipulation
- Use comment verbs for code formatting
- Use breakpoint verbs for debugging metadata (stored on script-outline headlines)
- Use profiling verbs for server-side performance analysis
- Use scriptToOutline for script-to-outline transformations

---

## Implementation Status

**Current State:**
- ✅ 13 kernel verbs defined in kernelverbs.rc
- ✅ 3 UserTalk utility scripts
- ⏳ Compiler implementation (complex)
- ⏳ Debugging infrastructure (breakpoint metadata on script-outline headlines)
- ⏳ Profiling engine (verb call execution timing and statistics)

**What Needs Implementation:**

**Tier 1 - Core Compilation (Headless-Compatible):**
1. **compile()** - UserTalk compiler + bytecode generation
2. **uncompile()** - Decompiler + bytecode interpretation
3. **getCode()** - Read script source from object
4. **getLanguage()** - Language detection/retrieval
5. **setLanguage()** - Language switching

**Tier 2 - Support Operations (Headless-Compatible):**
1. **makeComment/uncomment** - Text processing
2. **isComment** - Line parsing
3. **newScriptObject** - Create scripts

**Tier 3 - Debugging & Profiling (Headless-Compatible):**
1. **getBreakpoint/setBreakpoint/clearBreakpoint** - Breakpoint metadata on script-outline headlines
2. **startProfile/stopProfile** - Performance profiling for all verb calls

**Tier 4 - Data Transformation (Headless-Compatible):**
1. **scriptToOutline()** - Convert script text to outline structure

---

## Testing Requirements

**Basic Compilation (Headless):**
```usertalk
// Create script
new (scriptType, @myScript)
script.setLanguage (@myScript, "UserTalk")

// Compile
script.compile (@myScript)  // bytecode generated

// Decompile
script.uncompile (@myScript)  // source restored

// Read code
local (code = script.getCode (@myScript))
```

**Comment Operations:**
```usertalk
script.makeComment (@myScript, 1, 5)    // Comment lines 1-5
script.uncomment (@myScript, 1, 5)      // Uncomment lines 1-5
script.isComment (@myScript, 1)         // true
```

**Edge Cases:**
- Compile invalid syntax (error handling)
- Decompile without source
- Large scripts (1000+ lines)
- Nested function definitions
- Comment edge cases (already commented, etc.)

---

## Priority & Sequencing

**Priority:** 🟢 **HIGH** (Tier 1 - Script Execution Foundation)

**Recommended Implementation Order:** Early (after compiler framework)

**Prerequisites:**
- UserTalk compiler (the complex part)
- Script object types
- Runtime engine for bytecode execution

---

## Headless Compatibility Analysis

**Fully Compatible:** ✅ ALL VERBS (13/13 kernel + 3 utility = 16 total = 100%)

**Core Headless-Compatible Verbs (13):**
- Compilation: compile, uncompile, getCode, getLanguage, setLanguage (5)
- Comments: makeComment, uncomment, isComment (3)
- Debugging: getBreakpoint, setBreakpoint, clearBreakpoint (3) - metadata on script-outline headlines
- Profiling: startProfile, stopProfile (2) - verb call timing and statistics

**Utility Scripts (3):**
- newScriptObject, scriptToOutline, removeSource (3) - all headless-compatible

**Headless Strategy:**
✅ **FULLY VIABLE FOR HEADLESS** (all verbs compatible)
- Implement core compilation (Tier 1)
- Implement comment operations (Tier 2)
- Implement debugging metadata management (Tier 3)
- Implement profiling for performance analysis (Tier 3)
- Implement data transformations (Tier 4)

---

## Related Processors

- **thread** - Execute scripts in threads (requires compilation)
- **op** - Outline processor (scripts can be stored in outlines)
- **table** - Scripts stored in tables (requires manipulation)
- **window** - Script editor window (for debugging only)

---

## Special Considerations

**Compiler Complexity:**
- UserTalk is a full programming language
- Requires lexer, parser, semantic analyzer, code generator
- Type system support
- Error reporting with line numbers

**Language Support:**
- UserTalk (primary)
- AppleScript (if Mac support needed)
- Binary (pre-compiled scripts)

**Performance:**
- Compilation is CPU-intensive
- Caching compiled bytecode (storage)
- Lazy compilation (compile-on-demand)

---

## Summary

**Status:** ✅ **FULLY VIABLE FOR HEADLESS** (100% compatible)

**Key Findings:**
1. **All core functionality is headless-compatible** (compile/uncompile/getCode/comments)
2. **Debugging features are metadata-only** (breakpoints stored on script-outline headlines, no GUI required)
3. **Profiling is essential for server operation** (verb call timing for performance analysis on web servers)
4. **Data transformation is pure** (scriptToOutline is text-to-structure conversion, no GUI required)
5. **Compiler is complex but essential** (UserTalk execution foundation)

**Headless Implementation Strategy:**
- Implement all 13 kernel verbs + 3 utility scripts (100% of processor)
- All verbs are headless-compatible
- Result: Fully functional script compilation, debugging, and profiling for headless use

**Recommendation:** HIGH PRIORITY but COMPLEX (compiler implementation is fundamental)

---

## References

**Kernel Definitions:**
- `Common/resources/Win32/kernelverbs.rc` (lines 96-111)
- ID 1000 (shared): script (13 verbs)

**Scripts:**
- `system.verbs.builtins.script/` (15 scripts)

---
