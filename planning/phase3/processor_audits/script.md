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
Script processor provides operations for compiling, decompiling, and managing UserTalk scripts. Operations include compile/uncompile (headless-compatible), but debugging/breakpoint features require editor context (GUI-dependent). Core compilation is fully headless-compatible.

**Headless Compatibility:** ⚠️ **PARTIAL** (~70% compatible, ~30% GUI-dependent)

**GUI-Only Verbs:**
- getBreakpoint, setBreakpoint, clearBreakpoint (3 debugging ops)
- startProfile, stopProfile (2 profiling ops - may need GUI context)
- Total GUI-dependent: ~5 of 13 (38%)

**Headless-Compatible Verbs:**
- compile, uncompile, getCode, getLanguage, setLanguage (5 core)
- makeComment, uncomment, isComment (3 formatting)
- Total Headless: ~8 of 13 (62%)

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
| **Debugging** | getBreakpoint | ❌ GUI-Only | Get breakpoint state (editor context) |
| | setBreakpoint | ❌ GUI-Only | Set breakpoint (editor context) |
| | clearBreakpoint | ❌ GUI-Only | Clear breakpoint (editor context) |
| **Profiling** | startProfile | ❌ GUI-Only | Start execution profiler |
| | stopProfile | ❌ GUI-Only | Stop profiler and show results |

### Utility Scripts (2 UserTalk implementations)

| Script | Headless | Description |
|--------|----------|-------------|
| newScriptObject | ✅ YES | Create new script object at address |
| scriptToOutline | ⚠️ PARTIAL | Convert script to outline (may need editor) |
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

3. **Debugging** - Editor integration only
   - getBreakpoint(adrScript, lineNum) - Query breakpoint status
   - setBreakpoint(adrScript, lineNum) - Enable breakpoint
   - clearBreakpoint(adrScript, lineNum) - Disable breakpoint
   - Requires editor window to display and manage breakpoints

4. **Profiling** - Performance measurement
   - startProfile() - Begin execution timing
   - stopProfile() - End timing and generate report
   - Likely needs GUI window to show results

**Headless Strategy:**
For headless operation:
- Use compile/uncompile for all script operations
- Use getCode/setLanguage for script manipulation
- Use comment verbs for code formatting
- Skip debugging (breakpoints, profiling) entirely

---

## Implementation Status

**Current State:**
- ✅ 13 kernel verbs defined in kernelverbs.rc
- ✅ 3 UserTalk utility scripts
- ⏳ Compiler implementation (complex)
- ⏳ Debugging infrastructure (editor-dependent)
- ⏳ Profiling engine (performance measurement)

**What Needs Implementation:**

**Tier 1 - Core (Headless-Compatible):**
1. **compile()** - UserTalk compiler + bytecode generation
2. **uncompile()** - Decompiler + bytecode interpretation
3. **getCode()** - Read script source from object
4. **getLanguage()** - Language detection/retrieval
5. **setLanguage()** - Language switching

**Tier 2 - Support (Headless-Compatible):**
1. **makeComment/uncomment** - Text processing
2. **isComment** - Line parsing
3. **newScriptObject** - Create scripts

**Tier 3 - GUI-Dependent (Skip for Headless):**
1. **getBreakpoint/setBreakpoint/clearBreakpoint** - Debugging UI
2. **startProfile/stopProfile** - Profiling UI

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

**Fully Compatible:** ⚠️ PARTIAL (8/13 verbs = 62%)

**Headless-Compatible Verbs (8):**
- Compilation: compile, uncompile, getCode, getLanguage, setLanguage (5)
- Comments: makeComment, uncomment, isComment (3)

**GUI-Only Verbs (5):**
- Debugging: getBreakpoint, setBreakpoint, clearBreakpoint (3)
- Profiling: startProfile, stopProfile (2)

**Headless Strategy:**
✅ **VIABLE FOR HEADLESS** (use 8 compatible verbs)
- Implement core compilation (Tier 1)
- Implement comment operations (Tier 2)
- Skip debugging/profiling entirely

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

**Status:** ⚠️ **VIABLE FOR HEADLESS** (62% compatible)

**Key Findings:**
1. **Core functionality is headless-compatible** (compile/uncompile/getCode)
2. **Debugging features require GUI** (breakpoints, profiling)
3. **Comment operations are text-based** (fully headless)
4. **Compiler is complex but essential** (UserTalk execution foundation)

**Headless Implementation Strategy:**
- Implement 8 compatible verbs (62% of processor)
- Skip 5 GUI-dependent verbs (debugging/profiling)
- Result: Fully functional script compilation for headless use

**Recommendation:** HIGH PRIORITY but COMPLEX (compiler implementation)

---

## References

**Kernel Definitions:**
- `Common/resources/Win32/kernelverbs.rc` (lines 96-111)
- ID 1000 (shared): script (13 verbs)

**Scripts:**
- `system.verbs.builtins.script/` (15 scripts)

---
