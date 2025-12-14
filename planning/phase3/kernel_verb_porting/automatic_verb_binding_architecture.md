# Automatic Kernel Verb Binding Architecture

**Date:** 2025-12-13
**Status:** Design Complete - Ready for Implementation
**Owner:** System Architect + Explore Agent
**Purpose:** Architectural design for automatically creating bindings for all Frontier kernel verbs

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Current System Analysis](#current-system-analysis)
3. [The Gap](#the-gap)
4. [Recommended Architecture](#recommended-architecture)
5. [Implementation Specification](#implementation-specification)
6. [Alternative Architectures Considered](#alternative-architectures-considered)
7. [Implementation Roadmap](#implementation-roadmap)
8. [Success Criteria](#success-criteria)
9. [References](#references)

---

## Executive Summary

### Problem Statement

Frontier has ~700 kernel verbs across 51 processors that need to be ported from the GUI (Carbon/UI) runtime to the headless runtime. Currently, tracking which verbs are implemented vs. stubbed is a manual process using a hardcoded whitelist.

### Key Finding

**The codebase already has 80% of the automation infrastructure in place.** The existing system uses:
- Build-time code generation from `kernelverbs.rc`
- Python-based parser (`parse_kernelverbs.py`)
- Stub generator (`generate_processor_stubs.py`)
- Token-based dispatch with function pointers
- Auto-generated registration code

### Key Architectural Concept: UI Adapters

**Important**: Not all "UI-dependent" verbs are incompatible with headless mode. The existing kernel verb porting architecture defines **UI adapter patterns** that allow UI-centric verbs to work in headless mode through alternative implementations:

- **dialog.ask()** - Can use stdin/environment variables instead of GUI dialogs
- **dialog.alert()** - Can print to stderr instead of showing alerts
- **wp.*** and **op.*** verbs - Can operate on in-memory data structures without requiring windows
- **script.*** verbs - Can compile/execute scripts without UI

The automatic verb binding system must distinguish between:
1. **Carbon API dependencies** - Direct use of Carbon/QuickDraw APIs (incompatible with headless)
2. **UI adapter implementations** - Headless-compatible alternatives to UI operations

### The Gap

**Manual whitelist maintenance**: The system uses `HEADLESS_REGISTERED` set to track which processors are implemented. There's no automatic detection of:
- Which processors have real implementations vs. stubs
- Which individual verbs within a processor are implemented
- Whether implementations have UI dependencies
- Platform-specific code requirements

### Recommended Solution

**Static Analysis + Metadata Generation**: Extend the existing Python tooling with an implementation analyzer that automatically detects which verbs are implemented. This approach:
- Requires **zero C code changes**
- Works with existing codebase
- Provides verb-level granularity
- Auto-detects UI dependencies
- Scales to all 700 verbs without manual tracking
- Can be implemented in **1-2 weeks**

---

## Current System Analysis

### Overview

The Frontier kernel verb system uses a sophisticated two-tier dispatch architecture that's already highly automated.

### Source Definition: `kernelverbs.rc`

**Location:** `Common/resources/Win32/kernelverbs.rc`

This Windows resource file contains **51 EFP (External Function Processor) blocks** defining all kernel verbs.

**Example format:**
```rc
1007 /*idfileverbs*/ EFP DISCARDABLE
BEGIN
    1,                    // Number of "blocks"
    "file\0",            // Processor Name
    false,               // Window required
    86,                  // Verb count
    "created\0",
    "modified\0",
    "accessed\0",
    // ... 83 more verbs
END
```

**Coverage:**
- 51 processors across 4 major resource files
- Approximately 707 total verbs
- Spans file I/O, strings, dates, tables, database, networking, scripting, GUI controls

### Two-Tier Dispatch Architecture

**Tier 1: Processor Selection**
When a verb is called (e.g., `file.exists()`):
1. Looks up processor name ("file") in hash table
2. Finds associated callback function
3. Determines verb token (index) for that specific verb

**Tier 2: Verb Dispatch within Processor**
The processor's callback receives:
- `short token` - verb index (0, 1, 2, ... N-1)
- `hdltreenode hparam1` - parsed parameters
- `tyvaluerecord *vreturned` - output location
- `bigstring bserror` - error message output

**Implementation Pattern:**
```c
// 1. Token enum (auto-generated position in RC file)
enum {
    filv_created = 0,
    filv_modified = 1,
    filv_accessed = 2,
    // ... 83 more
};

// 2. Dispatch function
static boolean file_valueproc(short token,
                              hdltreenode hparam1,
                              tyvaluerecord *vreturned,
                              bigstring bserror) {
    switch(token) {
        case filv_created:
            return file_created_impl(hparam1, vreturned, bserror);
        case filv_modified:
            return file_modified_impl(hparam1, vreturned, bserror);
        case filv_accessed:
            // Stub - not yet implemented
            if (bserror)
                copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        // ... 83 more cases
    }
}

// 3. Init function (registers all verbs)
boolean fileinitverbs(void) {
    hdlhashtable htable = nil;

    if (!newfunctionprocessor(BIGSTRING("\pfile"),
                              &file_valueproc,
                              false,
                              &htable))
        return false;

    pushhashtable(htable);
    langaddkeyword(BIGSTRING("\pcreated"), filv_created);
    langaddkeyword(BIGSTRING("\pmodified"), filv_modified);
    langaddkeyword(BIGSTRING("\paccessed"), filv_accessed);
    // ... register all 86 verbs
    pophashtable();

    return true;
}
```

### Existing Automation Tools

#### Parser: `parse_kernelverbs.py`
**Location:** `tools/kernelverbs_parser/parse_kernelverbs.py`

**Purpose:** Extract processor definitions from RC file and generate initialization code

**Key Components:**
- `EFPProcessor` class - represents a single processor
- `parse_kernelverbs_rc()` - regex-based RC parser
- `generate_kernel_verbs_init_c()` - generates C initialization code
- `HEADLESS_REGISTERED` whitelist - **manually maintained list of implemented processors**

**Current Whitelist:**
```python
HEADLESS_REGISTERED: Set[str] = {
    # Already implemented in main codebase:
    'file', 'string', 'table', 'xml', 'html', 'window', 'db', 're',
    'sys', 'lang', 'crypt', 'math', 'sqlite', 'mysql',

    # New headless stubs:
    'frontier', 'op', 'opattributes', 'script', 'osa', 'menu', 'pict',
    'clock', 'date', 'dialog', 'kb', 'mouse', 'point', 'rectangle', 'rgb',
    'speaker', 'target', 'bit', 'semaphore', 'base64', 'tcp', 'dll',
    'python', 'htmlcontrol', 'statusbar', 'rez', 'search', 'filemenu',
    'editmenu', 'launch', 'clipboard', 'thread', 'mainwindow',
    'searchengine', 'mrcalendar', 'webserver', 'inetd'
}
```

#### Stub Generator: `generate_processor_stubs.py`
**Location:** `tools/kernelverbs_parser/generate_processor_stubs.py`

**Purpose:** Auto-generate skeleton `.c` files for unimplemented processors

**Features:**
- Extracts verb names from RC file
- Generates token enums
- Creates `{processor}initverbs()` function
- Creates `{processor}_valueproc()` dispatcher
- Properly formats Frontier string constants (pascal strings with `\p` prefix)

**Example Generated File:**
```c
/*
 * headless_clock_verbs.c - clock processor verbs (GENERATED FILE)
 * Auto-generated by tools/kernelverbs_parser/generate_processor_stubs.py
 * All verbs return false/"not implemented"
 */

#include "frontier.h"
#include "standard.h"

enum {
    clkv_now = 0,
    clkv_ticks = 1,
    clkv_waitSeconds = 2,
    // ... 4 more
};

static boolean clock_valueproc(short token, hdltreenode hparam1,
                               tyvaluerecord *vreturned,
                               bigstring bserror) {
    switch(token) {
        case clkv_now:
            /* Verb #0: clock.now - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        // ... more cases
    }
}

boolean clockinitverbs(void) {
    // ... registration code
}
```

### What Works Well

1. ✅ **Zero manual token management** - tokens auto-assigned from position in RC file
2. ✅ **Automatic processor registration** - generated init calls all processors
3. ✅ **Stub generation reduces boilerplate** - 90%+ code reduction for new processors
4. ✅ **Type-safe dispatch** - function pointer + switch statement pattern
5. ✅ **Clear separation** - headless code in `tests/headless_*_verbs.c`
6. ✅ **Build integration** - Make runs parser automatically

---

## The Gap

### Current Limitations

1. ❌ **Manual whitelist maintenance** - Must edit `HEADLESS_REGISTERED` set when implementing processors
2. ❌ **No stub vs implementation detection** - Can't distinguish real code from stubs automatically
3. ❌ **Processor-level granularity only** - Only tracks processor status, not individual verbs
4. ❌ **No implementation discovery** - Can't auto-detect which verbs are implemented within a processor
5. ❌ **Limited metadata** - No tracking of implementation status, platform support, UI dependencies

### Impact on Development

**When adding new verb implementations:**
1. Developer implements verb in C file
2. Developer must remember to add processor to `HEADLESS_REGISTERED`
3. No visibility into which verbs are stubbed vs implemented
4. No automatic detection of UI dependency violations
5. Manual tracking of implementation progress

**This creates:**
- Manual overhead for each new implementation
- Risk of forgetting to register processors
- No automated status reporting
- Difficult to track progress toward 700-verb goal

---

## Recommended Architecture

### Overview: Static Analysis + Metadata Generation

**Core Idea:** Extend existing Python tooling with an implementation analyzer that automatically detects which verbs are implemented by parsing C source files.

**Key Principle:** Zero C code changes - pure Python addition that works with existing codebase.

### Architecture Components

#### Component 1: Implementation Analyzer

**New File:** `tools/kernelverbs_parser/analyze_implementations.py`

**Purpose:** Parse C source files and classify each verb as implemented/stubbed

**Detection Strategy:**
```python
from pathlib import Path
from typing import Dict, Set, List
from dataclasses import dataclass
import re

@dataclass
class VerbImplementation:
    """Metadata for a single verb implementation."""
    processor: str
    verb_name: str
    token: int
    is_implemented: bool      # True if real code, False if stub
    impl_file: str            # Source file path
    impl_line: int            # Line number of implementation
    has_carbon_deps: bool     # True if uses Carbon/UI APIs directly
    uses_ui_adapter: bool     # True if uses UI adapter pattern (headless-compatible)
    platform_specific: bool   # True if has #ifdef POSIX/Windows
    complexity: int           # Lines of code in implementation

class VerbImplementationAnalyzer:
    """Analyze C source to detect real implementations vs stubs."""

    # Patterns that indicate a stub implementation
    STUB_PATTERNS = [
        r'copystring\(BIGSTRING\("\\pnot implemented"\)',
        r'return\s+false;\s*//.*not implemented',
        r'langerrormessage.*not.*implemented',
        r'getstringlist\(langerrorlist,\s*unimplementedverberror',
    ]

    # Patterns that indicate DIRECT Carbon/UI API usage (not adapter-based)
    # Note: These patterns detect Carbon API calls that would prevent
    # headless compilation. Verbs using UI adapter patterns (dialog.ask
    # via stdio, wp/op operations on in-memory data) should NOT be flagged.
    CARBON_API_PATTERNS = [
        r'\bWindowPtr\b', r'\bGrafPtr\b', r'\bMenuRef\b',
        r'\bDialogRef\b', r'\bControlRef\b', r'\bEventRecord\b',
        r'\bGetNewWindow\b', r'\bShowWindow\b', r'\bDrawMenuBar\b',
        r'#include\s*<Carbon', r'#include\s*<QuickDraw',
        r'#include\s*<Menus\.h>', r'#include\s*<Windows\.h>',
        r'\bshellwindow\b', r'\bwindowgetfspec\b',
    ]

    # Patterns that indicate UI adapter usage (headless-compatible)
    UI_ADAPTER_PATTERNS = [
        r'\badapter_alert\b', r'\badapter_ask\b', r'\badapter_notify\b',
        r'\bheadless_dialog_\w+\b',
        r'fprintf\s*\(\s*stderr.*ALERT',
        r'// UI adapter:',
        r'/\* UI adapter:',
    ]

    # Patterns that indicate platform-specific code
    PLATFORM_PATTERNS = [
        r'#ifdef\s+POSIX', r'#ifdef\s+WIN32',
        r'#ifdef\s+__APPLE__', r'#ifdef\s+_WIN32',
    ]

    def analyze_processor_file(self, filepath: Path) -> Dict[int, VerbImplementation]:
        """
        Analyze a headless_*_verbs.c file.

        Returns a dictionary mapping token number to VerbImplementation.
        """
        with open(filepath, 'r') as f:
            content = f.read()

        # Extract processor name from filename
        # headless_file_verbs.c -> "file"
        processor = self._extract_processor_name(filepath)

        # Parse token enum to build token -> verb_name mapping
        token_map = self._parse_token_enum(content)

        # Find switch statement and extract each case's code
        switch_cases = self._parse_switch_cases(content)

        # Analyze each case
        implementations = {}
        for token, verb_name in token_map.items():
            case_code = switch_cases.get(token, "")

            implementations[token] = VerbImplementation(
                processor=processor,
                verb_name=verb_name,
                token=token,
                is_implemented=not self._is_stub_implementation(case_code),
                impl_file=str(filepath),
                impl_line=self._find_line_number(content, token),
                has_carbon_deps=self._has_carbon_dependencies(case_code),
                uses_ui_adapter=self._uses_ui_adapter(case_code),
                platform_specific=self._detect_platform_specific(case_code),
                complexity=self._estimate_complexity(case_code)
            )

        return implementations

    def _is_stub_implementation(self, case_code: str) -> bool:
        """
        Detect if implementation is just a stub.

        A stub is code that just returns an error without doing real work.
        """
        # Check for explicit stub patterns
        for pattern in self.STUB_PATTERNS:
            if re.search(pattern, case_code, re.IGNORECASE):
                return True

        # Check for trivial implementation (just return false)
        lines = [l.strip() for l in case_code.split('\n')
                 if l.strip() and not l.strip().startswith('//')]
        if len(lines) <= 2 and 'return false' in case_code:
            return True

        return False

    def _has_carbon_dependencies(self, code: str) -> bool:
        """
        Detect DIRECT Carbon/UI API dependencies.

        Returns True only if code uses Carbon APIs directly,
        not if it uses UI adapter patterns (which are headless-compatible).
        """
        # First check if using UI adapter (headless-compatible)
        if self._uses_ui_adapter(code):
            return False

        # Then check for direct Carbon API usage
        for pattern in self.CARBON_API_PATTERNS:
            if re.search(pattern, code):
                return True
        return False

    def _uses_ui_adapter(self, code: str) -> bool:
        """
        Detect UI adapter pattern usage.

        UI adapters allow verbs to work in headless mode by providing
        alternative implementations (e.g., dialog.ask via stdio,
        wp/op operations on in-memory data without windows).
        """
        for pattern in self.UI_ADAPTER_PATTERNS:
            if re.search(pattern, code):
                return True
        return False

    def _detect_platform_specific(self, code: str) -> bool:
        """Detect platform-specific code."""
        for pattern in self.PLATFORM_PATTERNS:
            if re.search(pattern, code):
                return True
        return False

    def _estimate_complexity(self, case_code: str) -> int:
        """Estimate lines of code (excluding comments/blanks)."""
        lines = [l.strip() for l in case_code.split('\n')
                 if l.strip() and not l.strip().startswith('//')]
        return len(lines)

    def _parse_token_enum(self, content: str) -> Dict[int, str]:
        """
        Parse token enum to build mapping.

        Example:
            enum {
                filv_created = 0,
                filv_modified = 1,
            };

        Returns: {0: "created", 1: "modified"}
        """
        token_map = {}

        # Find enum block
        enum_pattern = r'enum\s*\{([^}]+)\}'
        match = re.search(enum_pattern, content, re.DOTALL)
        if not match:
            return token_map

        enum_body = match.group(1)

        # Parse each enum entry
        entry_pattern = r'(\w+)\s*=\s*(\d+)'
        for match in re.finditer(entry_pattern, enum_body):
            token_name = match.group(1)
            token_value = int(match.group(2))

            # Extract verb name from token name
            # filv_created -> "created"
            # clkv_now -> "now"
            if '_' in token_name:
                verb_name = token_name.split('_', 1)[1]
            else:
                verb_name = token_name

            token_map[token_value] = verb_name

        return token_map

    def _parse_switch_cases(self, content: str) -> Dict[int, str]:
        """
        Extract code for each switch case.

        Returns: {token: case_code}
        """
        cases = {}

        # Find switch statement
        switch_pattern = r'switch\s*\(\s*token\s*\)\s*\{(.*?)\n\s*\}'
        match = re.search(switch_pattern, content, re.DOTALL)
        if not match:
            return cases

        switch_body = match.group(1)

        # Split into cases
        # This is simplified - real implementation needs better parsing
        case_pattern = r'case\s+(\w+):(.*?)(?=case\s+\w+:|default:|$)'
        for match in re.finditer(case_pattern, switch_body, re.DOTALL):
            case_name = match.group(1)
            case_code = match.group(2)

            # Extract token value from case name (from enum)
            # This requires cross-referencing with token_map
            # Simplified here - real implementation uses token_map

            cases[case_name] = case_code

        return cases

    def _find_line_number(self, content: str, token: int) -> int:
        """Find line number where this case is defined."""
        lines = content.split('\n')
        for i, line in enumerate(lines):
            if f'case {token}:' in line or f'= {token}' in line:
                return i + 1
        return 0

    def _extract_processor_name(self, filepath: Path) -> str:
        """
        Extract processor name from filename.

        headless_file_verbs.c -> "file"
        """
        filename = filepath.stem  # headless_file_verbs
        if filename.startswith('headless_'):
            filename = filename[9:]  # file_verbs
        if filename.endswith('_verbs'):
            filename = filename[:-6]  # file
        return filename
```

#### Component 2: Status Report Generator

**Purpose:** Generate human-readable implementation status reports

**Implementation:**
```python
class StatusReportGenerator:
    """Generate implementation status reports."""

    def __init__(self, analyzer: VerbImplementationAnalyzer):
        self.analyzer = analyzer

    def generate_markdown_report(self, tests_dir: Path) -> str:
        """Generate comprehensive markdown status report."""

        # Analyze all processor files
        all_impls = {}
        for filepath in sorted(tests_dir.glob("headless_*_verbs.c")):
            processor_impls = self.analyzer.analyze_processor_file(filepath)
            all_impls.update(processor_impls)

        # Calculate statistics
        total = len(all_impls)
        implemented = sum(1 for v in all_impls.values() if v.is_implemented)
        stubbed = total - implemented
        carbon_flagged = sum(1 for v in all_impls.values()
                            if v.is_implemented and v.has_carbon_deps)
        ui_adapter_count = sum(1 for v in all_impls.values()
                              if v.is_implemented and v.uses_ui_adapter)

        # Group by processor
        by_processor = {}
        for impl in all_impls.values():
            if impl.processor not in by_processor:
                by_processor[impl.processor] = []
            by_processor[impl.processor].append(impl)

        # Generate report
        lines = [
            "# Kernel Verb Implementation Status\n",
            f"**Generated:** {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
            f"**Analyzed:** {len(by_processor)} processors, {total} verbs\n",
            "## Summary\n",
            f"- **Total verbs**: {total}",
            f"- **Implemented**: {implemented} ({implemented/total*100:.1f}%)",
            f"- **Stubbed**: {stubbed} ({stubbed/total*100:.1f}%)",
            f"- **Uses UI adapter**: {ui_adapter_count} ({ui_adapter_count/total*100:.1f}% - headless-compatible)",
            f"- **Has Carbon deps**: {carbon_flagged} ({carbon_flagged/total*100:.1f}% - ⚠️ needs review)\n",
            "## By Processor\n",
        ]

        # Detail each processor
        for processor, impls in sorted(by_processor.items()):
            impl_count = sum(1 for v in impls if v.is_implemented)
            total_count = len(impls)
            pct = impl_count / total_count * 100 if total_count > 0 else 0

            lines.append(f"### {processor} - {impl_count}/{total_count} ({pct:.1f}%)\n")

            # Show implementation status
            if impl_count == total_count:
                lines.append("✅ **All verbs implemented!**\n")
            elif impl_count == 0:
                lines.append("❌ **All verbs stubbed** (UI-only processor)\n")
            else:
                # List unimplemented verbs
                unimpl = [v for v in impls if not v.is_implemented]
                lines.append(f"**Not implemented** ({len(unimpl)} verbs):")
                for v in sorted(unimpl, key=lambda x: x.verb_name):
                    lines.append(f"- `{processor}.{v.verb_name}`")
                lines.append("")

                # List implemented verbs with UI adapters
                impl = [v for v in impls if v.is_implemented]
                ui_adapter_verbs = [v for v in impl if v.uses_ui_adapter]
                if ui_adapter_verbs:
                    lines.append("**✓ Implemented with UI adapter (headless-compatible):**")
                    for v in ui_adapter_verbs:
                        lines.append(f"- `{processor}.{v.verb_name}` "
                                   f"({v.impl_file}:{v.impl_line})")
                    lines.append("")

                # List implemented verbs with Carbon deps (needs review)
                carbon_verbs = [v for v in impl if v.has_carbon_deps]
                if carbon_verbs:
                    lines.append("**⚠️ Implemented with Carbon dependencies (needs review):**")
                    for v in carbon_verbs:
                        lines.append(f"- `{processor}.{v.verb_name}` "
                                   f"({v.impl_file}:{v.impl_line})")
                    lines.append("")

        return "\n".join(lines)

    def generate_json_metadata(self, tests_dir: Path) -> dict:
        """Generate machine-readable JSON metadata."""

        all_impls = {}
        for filepath in sorted(tests_dir.glob("headless_*_verbs.c")):
            processor_impls = self.analyzer.analyze_processor_file(filepath)
            all_impls.update(processor_impls)

        # Group by processor
        by_processor = {}
        for impl in all_impls.values():
            if impl.processor not in by_processor:
                by_processor[impl.processor] = {
                    'total': 0,
                    'implemented': 0,
                    'stubbed': 0,
                    'verbs': {}
                }

            proc_data = by_processor[impl.processor]
            proc_data['total'] += 1
            if impl.is_implemented:
                proc_data['implemented'] += 1
            else:
                proc_data['stubbed'] += 1

            proc_data['verbs'][impl.verb_name] = {
                'token': impl.token,
                'implemented': impl.is_implemented,
                'has_carbon_deps': impl.has_carbon_deps,
                'uses_ui_adapter': impl.uses_ui_adapter,
                'platform_specific': impl.platform_specific,
                'complexity': impl.complexity,
                'file': impl.impl_file,
                'line': impl.impl_line,
            }

        return {
            'generated_at': datetime.now().isoformat(),
            'total_processors': len(by_processor),
            'total_verbs': len(all_impls),
            'implemented_verbs': sum(1 for v in all_impls.values()
                                    if v.is_implemented),
            'processors': by_processor,
        }
```

#### Component 3: Enhanced Parser Integration

**Modify:** `tools/kernelverbs_parser/parse_kernelverbs.py`

**Changes:**
```python
from analyze_implementations import VerbImplementationAnalyzer

def generate_kernel_verbs_init_c(processors: List[EFPProcessor],
                                rc_path: str) -> str:
    """
    Generate init code using actual implementation data.

    NOW: Auto-detects which processors are implemented instead of
    using manual HEADLESS_REGISTERED whitelist.
    """

    # Initialize analyzer
    analyzer = VerbImplementationAnalyzer()
    tests_dir = Path(__file__).parent.parent.parent / "tests"

    # Auto-detect implemented processors
    implemented_procs = []
    for proc in processors:
        impl_file = tests_dir / f"headless_{proc.name}_verbs.c"
        if impl_file.exists():
            impls = analyzer.analyze_processor_file(impl_file)
            impl_count = sum(1 for v in impls.values() if v.is_implemented)

            # Include processor if ANY verb has real implementation
            if impl_count > 0:
                implemented_procs.append({
                    'processor': proc,
                    'impl_count': impl_count,
                    'total_count': len(impls),
                })

    # Generate init code with metadata
    lines = [
        "/**",
        " * headless_init_kernel_verbs - Initialize kernel verb processors",
        " *",
        f" * Implemented processors: {len(implemented_procs)} of {len(processors)} total",
        f" * Auto-detected by tools/kernelverbs_parser/analyze_implementations.py",
        " * To see detailed status: make verb-status",
        " */",
        "boolean headless_init_kernel_verbs(void) {",
    ]

    for proc_data in implemented_procs:
        proc = proc_data['processor']
        impl_count = proc_data['impl_count']
        total_count = proc_data['total_count']
        pct = impl_count / total_count * 100 if total_count > 0 else 0

        lines.append(f"    /* Initialize {proc.name} processor "
                    f"({impl_count}/{total_count} verbs = {pct:.0f}%) */")
        lines.append(f"    if (!{proc.init_func}())")
        lines.append(f"        return false;")
        lines.append("")

    lines.append("    return true;")
    lines.append("}")

    return "\n".join(lines)
```

#### Component 4: Build System Integration

**Add to:** `Makefile` (or `tests/Makefile`)

```makefile
# Generate verb implementation status report
.PHONY: verb-status
verb-status:
	@echo "Analyzing kernel verb implementations..."
	python3 tools/kernelverbs_parser/analyze_implementations.py \
		--tests-dir tests \
		--output-md planning/phase3/verb_implementation_status.md \
		--output-json generated/verb_status.json
	@echo "Report generated:"
	@echo "  - planning/phase3/verb_implementation_status.md"
	@echo "  - generated/verb_status.json"

# Auto-generate verb init code (now with auto-detection)
generated/kernel_verbs_init.c: Common/resources/Win32/kernelverbs.rc tools/kernelverbs_parser/*.py
	@echo "Generating kernel verb initialization code..."
	python3 tools/kernelverbs_parser/parse_kernelverbs.py \
		--input Common/resources/Win32/kernelverbs.rc \
		--output generated/kernel_verbs_init.c
	@echo "Generated with automatic implementation detection"
```

### Optional Enhancement: Annotation Support

For edge cases where heuristics fail, support optional annotations:

```c
// In headless_file_verbs.c
static boolean file_valueproc(short token, hdltreenode hparam1,
                              tyvaluerecord *vreturned,
                              bigstring bserror) {
    switch(token) {
        case filv_created:
            /* @IMPLEMENTED @PORTABLE */
            return file_created_impl(hparam1, vreturned, bserror);

        case filv_type:
            /* @STUB @OBSOLETE - Mac creator/type codes not supported */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;

        case filv_open:
            /* @IMPLEMENTED @PLATFORM_POSIX */
            return file_open_impl(hparam1, vreturned, bserror);
    }
}
```

**Annotation Tags:**
- `@IMPLEMENTED` - Override heuristic, force as implemented
- `@STUB` - Override heuristic, force as stub
- `@PORTABLE` - Fully portable across platforms
- `@PLATFORM_POSIX` - POSIX-specific implementation
- `@PLATFORM_WIN32` - Windows-specific implementation
- `@UI_ADAPTER` - Uses UI adapter pattern (headless-compatible)
- `@CARBON_DEPS` - Has direct Carbon API dependencies (needs review)
- `@OBSOLETE` - Legacy feature, intentionally not implemented

**Parser Support:**
```python
def parse_annotations(case_code: str) -> Set[str]:
    """Extract @TAGS from comments."""
    pattern = r'/\*\s*(@\w+(?:\s+@\w+)*)\s*(?:-[^*]*)?\*/'
    matches = re.findall(pattern, case_code)
    tags = set()
    for match in matches:
        tags.update(match.split())
    return tags

def analyze_verb_with_annotations(case_code: str) -> VerbImplementation:
    """Combine heuristic + annotation analysis."""
    annotations = parse_annotations(case_code)

    # Check annotations first
    if '@IMPLEMENTED' in annotations:
        is_implemented = True
    elif '@STUB' in annotations:
        is_implemented = False
    else:
        # Fall back to heuristics
        is_implemented = not self._is_stub_implementation(case_code)

    # Check for UI adapter vs Carbon dependencies
    if '@UI_ADAPTER' in annotations:
        uses_ui_adapter = True
        has_carbon_deps = False
    elif '@CARBON_DEPS' in annotations:
        uses_ui_adapter = False
        has_carbon_deps = True
    else:
        # Fall back to heuristics
        uses_ui_adapter = self._uses_ui_adapter(case_code)
        has_carbon_deps = self._has_carbon_dependencies(case_code)

    return VerbImplementation(
        is_implemented=is_implemented,
        has_carbon_deps=has_carbon_deps,
        uses_ui_adapter=uses_ui_adapter,
        # ...
    )
```

### Example Output

#### Generated Init Code
```c
/**
 * headless_init_kernel_verbs - Initialize kernel verb processors
 *
 * Implemented processors: 32 of 51 total
 * Implemented verbs: 423 of 707 total (59.8%)
 *
 * Auto-detected by tools/kernelverbs_parser/analyze_implementations.py
 * Generated: 2025-12-13 14:30:00
 * To see detailed status: make verb-status
 */
boolean headless_init_kernel_verbs(void) {
    /* Initialize file processor (72/86 verbs = 84%) */
    if (!fileinitverbs())
        return false;

    /* Initialize string processor (60/60 verbs = 100%) */
    if (!stringinitverbs())
        return false;

    /* Initialize frontier processor (8/14 verbs = 57%) */
    if (!frontierinitverbs())
        return false;

    /* Initialize clock processor (5/7 verbs = 71%) */
    if (!clockinitverbs())
        return false;

    // ... 28 more processors with real implementations
    // Note: 19 processors with 0% implementation are not registered

    return true;
}
```

#### Status Report (Markdown)
```markdown
# Kernel Verb Implementation Status

**Generated:** 2025-12-13 14:30:00
**Analyzed:** 51 processors, 707 verbs

## Summary

- **Total verbs**: 707
- **Implemented**: 423 (59.8%)
- **Stubbed**: 284 (40.2%)
- **Uses UI adapter**: 45 (6.4% - headless-compatible)
- **Has Carbon deps**: 3 (0.4% - ⚠️ needs review)

## By Processor

### file - 72/86 (83.7%)

**Not implemented** (14 verbs):
- `file.type` - Mac creator/type codes (obsolete)
- `file.creator` - Mac creator/type codes (obsolete)
- `file.getIconPos` - UI dependency (requires Finder)
- `file.setIconPos` - UI dependency (requires Finder)
- `file.label` - Mac label colors (obsolete)
- `file.setLabel` - Mac label colors (obsolete)
- `file.getComment` - Requires extended attributes
- `file.setComment` - Requires extended attributes
- `file.compareFiles` - Not yet implemented
- `file.getSpecialFolderPath` - Partial implementation
- `file.openDialog` - UI dependency (file picker)
- `file.saveDialog` - UI dependency (file picker)
- `file.folderDialog` - UI dependency (folder picker)
- `file.volumeDialog` - UI dependency (volume picker)

### string - 60/60 (100.0%)

✅ **All verbs implemented!**

### clock - 5/7 (71.4%)

**Not implemented** (2 verbs):
- `clock.waitSeconds` - Needs implementation
- `clock.sleepFor` - Needs implementation

### window - 0/31 (0.0%)

❌ **All verbs stubbed** (UI-only processor)

### op - 0/45 (0.0%)

❌ **All verbs stubbed** (UI-only processor)

### dialog - 3/19 (15.8%)

**Implemented** (3 verbs):
- `dialog.alert` ✓ (stderr adapter)
- `dialog.notify` ✓ (stderr adapter)
- `dialog.getNumber` ✓ (returns 0)

**✓ Implemented with UI adapter (headless-compatible)**:
- `dialog.alert` (tests/headless_dialog_verbs.c:45)
- `dialog.notify` (tests/headless_dialog_verbs.c:58)
- `dialog.getNumber` (tests/headless_dialog_verbs.c:71)

**Not implemented** (16 verbs):
- `dialog.ask` - Needs stdin implementation
- `dialog.password` - Needs stdin implementation
- `dialog.confirm` - Needs stdin implementation
- `dialog.twoWay` - Needs stdin implementation
- ... 12 more

### frontier - 8/14 (57.1%)

**Not implemented** (6 verbs):
- `frontier.bringToFront` - UI-only
- `frontier.isActive` - UI-only
- `frontier.showApplication` - UI-only
- `frontier.lockApplication` - UI-only
- `frontier.unlockApplication` - UI-only
- `frontier.getIdleSeconds` - Needs implementation
```

#### Machine-Readable Metadata (JSON)
```json
{
  "generated_at": "2025-12-13T14:30:00Z",
  "total_processors": 51,
  "total_verbs": 707,
  "implemented_verbs": 423,
  "stubbed_verbs": 284,
  "processors": {
    "file": {
      "total": 86,
      "implemented": 72,
      "stubbed": 14,
      "verbs": {
        "created": {
          "token": 0,
          "implemented": true,
          "has_carbon_deps": false,
          "uses_ui_adapter": false,
          "platform_specific": false,
          "complexity": 8,
          "file": "tests/headless_file_verbs.c",
          "line": 118
        },
        "modified": {
          "token": 1,
          "implemented": true,
          "has_carbon_deps": false,
          "uses_ui_adapter": false,
          "platform_specific": false,
          "complexity": 8,
          "file": "tests/headless_file_verbs.c",
          "line": 127
        },
        "type": {
          "token": 2,
          "implemented": false,
          "has_carbon_deps": false,
          "uses_ui_adapter": false,
          "platform_specific": false,
          "complexity": 2,
          "file": "tests/headless_file_verbs.c",
          "line": 136
        }
      }
    }
  }
}
```

---

## Alternative Architectures Considered

### Option 1: Macro-Based Registration (REJECTED)

**Approach:** Use C macros to auto-register implementations

```c
#define VERB_IMPL(name, token) \
    case token: { \
        extern boolean name(hdltreenode, tyvaluerecord*, bigstring); \
        return name(hparam1, vreturned, bserror); \
    }

#define VERB_STUB(token) \
    case token: \
        if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror); \
        return false;
```

**Why Rejected:**
- Doesn't solve detection problem (still need to track which macros are used)
- Adds C complexity for limited benefit
- Current pattern is already clear enough
- Would require rewriting all 51 processor files

### Option 2: Reflection-Like Tables (REJECTED)

**Approach:** Build function pointer tables at compile time

```c
typedef boolean (*verb_impl_func)(hdltreenode, tyvaluerecord*, bigstring);

typedef struct {
    const char *name;
    int token;
    verb_impl_func impl;
} verb_entry;

static verb_entry file_verbs[] = {
    {"created", 0, file_created_impl},
    {"modified", 1, file_modified_impl},
    {"type", 2, NULL},  // NULL = stub
    // ...
};
```

**Why Rejected:**
- Requires manual maintenance of tables
- Duplicates information already in switch statements
- Adds memory overhead (function pointer tables)
- Doesn't provide better detection than static analysis
- Current token-based dispatch is more efficient

### Option 3: Compile-Time Code Generation from Annotations (REJECTED)

**Approach:** Annotate C functions, generate dispatch code

```c
// @VERB file.created
// @TOKEN 0
boolean file_created_impl(hdltreenode hparam1, tyvaluerecord *v, bigstring err) {
    // implementation
}
```

**Why Rejected:**
- Requires significant changes to existing code
- Build system complexity
- Loses explicit switch statement clarity
- Harder to debug
- Not compatible with existing codebase structure

### Option 4: Runtime Registration (REJECTED)

**Approach:** Each implementation registers itself at startup

```c
// In file_created.c
static void __attribute__((constructor)) register_file_created(void) {
    register_verb("file", "created", 0, file_created_impl);
}
```

**Why Rejected:**
- Not portable across platforms
- Adds runtime overhead
- Unclear registration order
- Harder to audit what's registered
- Existing compile-time system is better

---

## Implementation Specification

### Phase 1: Basic Heuristic Analyzer (2-3 days)

**Deliverable:** `tools/kernelverbs_parser/analyze_implementations.py`

**Features:**
- Parse token enums from C files
- Extract switch statement cases
- Detect stub patterns (return false + error message)
- Classify each verb as implemented/stubbed
- Generate markdown status report

**Testing:**
```python
# tools/kernelverbs_parser/test_analyzer.py
def test_stub_detection():
    """Verify stub pattern detection."""
    stub_code = '''
        case filv_type:
            if (bserror) copystring(BIGSTRING("\\pnot implemented"), bserror);
            return false;
    '''
    analyzer = VerbImplementationAnalyzer()
    assert analyzer._is_stub_implementation(stub_code) == True

def test_real_implementation_detection():
    """Verify real implementation detection."""
    impl_code = '''
        case filv_created:
            struct stat st;
            bigstring path;
            if (!getpathvalue(hparam1, 1, path)) return false;
            if (stat(path, &st) != 0) return false;
            return setlongvalue(st.st_ctime, vreturned);
    '''
    analyzer = VerbImplementationAnalyzer()
    assert analyzer._is_stub_implementation(impl_code) == False

def test_ui_dependency_detection():
    """Verify UI dependency detection."""
    ui_code = '''
        case windowv_show:
            WindowPtr w;
            if (!getwindowptr(hparam1, 1, &w)) return false;
            ShowWindow(w);
            return setbooleanvalue(true, vreturned);
    '''
    analyzer = VerbImplementationAnalyzer()
    assert analyzer._has_ui_dependencies(ui_code) == True
```

**Integration:**
```bash
# Add to Makefile
make verb-status:
    python3 tools/kernelverbs_parser/analyze_implementations.py \
        --tests-dir tests \
        --output planning/phase3/verb_implementation_status.md
```

### Phase 2: Enhanced Detection (2-3 days)

**Add:**
- UI dependency detection (Carbon API patterns)
- Platform-specific code detection (#ifdef POSIX/Windows)
- Implementation complexity estimation (LOC in case)
- Cross-reference with kernelverbs.rc for completeness check

**New Features:**
```python
class VerbImplementationAnalyzer:
    def validate_completeness(self, rc_path: Path, tests_dir: Path):
        """
        Verify all verbs in RC file have corresponding C code.

        Returns list of missing verbs.
        """
        # Parse RC file to get expected verbs
        expected_verbs = self._parse_rc_file(rc_path)

        # Parse C files to get actual verbs
        actual_verbs = self._scan_all_processors(tests_dir)

        # Find discrepancies
        missing = expected_verbs - actual_verbs
        extra = actual_verbs - expected_verbs

        return {
            'missing': missing,  # In RC but not in C
            'extra': extra,      # In C but not in RC
        }
```

### Phase 3: Parser Integration (2-3 days)

**Modify:** `tools/kernelverbs_parser/parse_kernelverbs.py`

**Changes:**
1. Import analyzer module
2. Replace `HEADLESS_REGISTERED` whitelist with auto-detection
3. Generate init code with implementation statistics
4. Add comments showing implementation percentage

**Before:**
```python
HEADLESS_REGISTERED = {'file', 'string', 'table', ...}  # Manual
implemented_procs = [p for p in processors if p.name in HEADLESS_REGISTERED]
```

**After:**
```python
analyzer = VerbImplementationAnalyzer()
implemented_procs = []
for proc in processors:
    impl_file = tests_dir / f"headless_{proc.name}_verbs.c"
    if impl_file.exists():
        impls = analyzer.analyze_processor_file(impl_file)
        impl_count = sum(1 for v in impls.values() if v.is_implemented)
        if impl_count > 0:
            implemented_procs.append((proc, impl_count, len(impls)))
```

### Phase 4: Annotation Support (1 day)

**Add:**
- Parse `/* @TAG */` annotations from C code
- Use annotations to override heuristic detection
- Document annotation system

**Annotation Parser:**
```python
def parse_annotations(case_code: str) -> Set[str]:
    """Extract @TAGS from /* @TAG1 @TAG2 */ comments."""
    pattern = r'/\*\s*(@\w+(?:\s+@\w+)*)\s*(?:-[^*]*)?\*/'
    matches = re.findall(pattern, case_code)
    tags = set()
    for match in matches:
        tags.update(match.split())
    return tags
```

**Supported Tags:**
- `@IMPLEMENTED` - Force as implemented
- `@STUB` - Force as stub
- `@PORTABLE` - Fully portable
- `@PLATFORM_POSIX` - POSIX-specific
- `@PLATFORM_WIN32` - Windows-specific
- `@NO_UI` - No UI dependencies
- `@OBSOLETE` - Intentionally not implemented

### Phase 5: Testing & Validation (2 days)

**Unit Tests:**
- Test stub detection on 20+ sample cases
- Test UI dependency detection
- Test annotation parsing
- Test edge cases (empty switch, missing enum, etc.)

**Integration Tests:**
- Run analyzer on all 51 processor files
- Validate detection accuracy (manual spot-check 10% of verbs)
- Verify no false negatives for UI dependencies
- Test generated init code compiles

**Accuracy Targets:**
- 95%+ correct stub vs implementation classification
- 100% detection of all verbs in RC file
- 0% false negatives for UI dependencies (safety-critical)
- Acceptable false positives for UI dependencies (conservative)

### Quality Standards

**Detection Heuristics:**
```python
# Must detect these as stubs:
stub_examples = [
    'return false;  // not implemented',
    'copystring(BIGSTRING("\\pnot implemented"), bserror); return false;',
    'langerrormessage(BIGSTRING("\\pnot implemented")); return false;',
    'getstringlist(langerrorlist, unimplementedverberror, bserror); return false;',
]

# Must detect these as implemented:
impl_examples = [
    'if (!getparamvalue(...)) return false; return setstringvalue(...);',
    'struct stat st; if (stat(...) != 0) return false; return setlongvalue(...);',
    'return file_created_impl(hparam1, vreturned, bserror);',
]

# Must detect these as having UI dependencies:
ui_examples = [
    'WindowPtr w = getwindow();',
    'ShowWindow(w);',
    'MenuRef m = GetMenuHandle(128);',
    '#include <Carbon/Carbon.h>',
    'shellwindow',
]
```

### File Organization

```
Frontier/
├── tools/
│   └── kernelverbs_parser/
│       ├── parse_kernelverbs.py              # Existing (modified)
│       ├── generate_processor_stubs.py       # Existing
│       ├── analyze_implementations.py        # NEW: Core analyzer
│       ├── verb_metadata.py                  # NEW: Data classes
│       ├── status_report_generator.py        # NEW: Report generation
│       ├── test_analyzer.py                  # NEW: Unit tests
│       └── README.md                          # NEW: Tool documentation
├── tests/
│   ├── headless_file_verbs.c                 # Existing (no changes)
│   ├── headless_string_verbs.c               # Existing (no changes)
│   └── ...48 more processor files
├── generated/
│   ├── kernel_verbs_init.c                   # Enhanced with metadata
│   └── verb_status.json                      # NEW: Machine-readable status
├── planning/phase3/
│   ├── verb_implementation_status.md         # NEW: Auto-generated report
│   └── kernel_verb_porting/
│       └── automatic_verb_binding_architecture.md  # This document
└── Makefile
    # Add targets:
    # - make verb-status (generate reports)
    # - make verify-verbs (validate completeness)
```

---

## Implementation Roadmap

### Week 1: Foundation (5 days)

**Day 1-2: Implement Basic Analyzer**
- Create `analyze_implementations.py`
- Implement token enum parser
- Implement switch case parser
- Implement stub detection heuristics
- Unit tests for core parsing

**Day 3: Status Report Generator**
- Create `status_report_generator.py`
- Implement markdown report generation
- Implement JSON metadata generation
- Test on sample processor files

**Day 4-5: Integration Testing**
- Run analyzer on all 51 processor files
- Manual verification of 50+ verbs (10% sample)
- Fix parsing edge cases
- Refine stub detection patterns

**Deliverable:** Working analyzer that can classify verbs

### Week 2: Enhancement (5 days)

**Day 1-2: UI Dependency Detection**
- Add Carbon/UI API pattern matching
- Test on known UI-dependent code
- Ensure 0% false negatives (safety-critical)
- Generate UI dependency report

**Day 3: Platform-Specific Detection**
- Add #ifdef POSIX/Windows pattern matching
- Classify platform-specific implementations
- Add platform support to metadata

**Day 4: Completeness Validation**
- Cross-reference RC file with C implementations
- Detect missing verbs
- Detect extra verbs (not in RC)
- Generate discrepancy report

**Day 5: Testing & Refinement**
- Comprehensive testing on all processors
- Accuracy validation
- Edge case handling
- Performance optimization

**Deliverable:** Full-featured analyzer with safety checks

### Week 3: Integration (5 days)

**Day 1-2: Parser Integration**
- Modify `parse_kernelverbs.py`
- Replace manual whitelist with auto-detection
- Generate enhanced init code
- Test generated code compiles

**Day 3: Build System Integration**
- Add `make verb-status` target
- Add `make verify-verbs` target
- Integrate into CI/CD pipeline
- Test incremental builds

**Day 4: Annotation Support**
- Implement annotation parser
- Add override logic
- Document annotation system
- Test on edge cases

**Day 5: Testing & Rollout**
- End-to-end testing
- Performance benchmarks
- Documentation updates
- Deploy to development workflow

**Deliverable:** Fully integrated auto-detection system

### Week 4: Polish & Documentation (3-5 days)

**Day 1-2: Documentation**
- Update DEVELOPER_QUICKSTART_HEADLESS.md
- Create tool usage guide
- Add examples and tutorials
- Document annotation system

**Day 3: Report Enhancements**
- Add charts/graphs to reports
- Add historical tracking
- Add priority recommendations
- Improve formatting

**Day 4-5: Final Testing & Deployment**
- Full regression testing
- Performance validation
- User acceptance testing
- Production deployment

**Deliverable:** Production-ready automated system

---

## Dry-Run and Verification

### Dry-Run Capability

**Purpose**: Allow developers to preview generated code changes before committing.

**Implementation**:
```python
# tools/kernelverbs_parser/parse_kernelverbs.py

def generate_kernel_verbs_init_c(processors: List[EFPProcessor],
                                rc_path: str,
                                output_path: str = None,
                                dry_run: bool = False) -> str:
    """
    Generate kernel verbs init code.

    Args:
        processors: List of parsed processors
        rc_path: Path to kernelverbs.rc
        output_path: Where to write generated code (required if not dry_run)
        dry_run: If True, print instead of writing

    Returns:
        Generated code as string
    """
    analyzer = VerbImplementationAnalyzer()
    tests_dir = Path(__file__).parent.parent.parent / "tests"

    # Auto-detect implemented processors
    implemented_procs = []
    for proc in processors:
        impl_file = tests_dir / f"headless_{proc.name}_verbs.c"
        if impl_file.exists():
            impls = analyzer.analyze_processor_file(impl_file)
            impl_count = sum(1 for v in impls.values() if v.is_implemented)
            if impl_count > 0:
                implemented_procs.append({
                    'processor': proc,
                    'impl_count': impl_count,
                    'total_count': len(impls),
                })

    # Generate code
    generated_code = _format_init_code(implemented_procs)

    if dry_run:
        print("=" * 70)
        print("DRY RUN: Would generate the following code")
        print("=" * 70)
        print(generated_code)
        print("=" * 70)

        if output_path and os.path.exists(output_path):
            with open(output_path, 'r') as f:
                current_code = f.read()

            if current_code == generated_code:
                print("\n✓ Generated code matches current file (no changes)")
            else:
                print("\n⚠ Generated code differs from current file")
                print("\nDiff:")
                import difflib
                diff = difflib.unified_diff(
                    current_code.splitlines(keepends=True),
                    generated_code.splitlines(keepends=True),
                    fromfile='current',
                    tofile='generated',
                    lineterm=''
                )
                print(''.join(diff))

        return generated_code

    # Actually write file
    if not output_path:
        raise ValueError("output_path required when not in dry-run mode")

    with open(output_path, 'w') as f:
        f.write(generated_code)

    print(f"✓ Generated {output_path}")
    print(f"  {len(implemented_procs)} processors, "
          f"{sum(p['impl_count'] for p in implemented_procs)} verbs")

    return generated_code


# Add CLI flag support
if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser(description='Generate kernel verb initialization code')
    parser.add_argument('--input', default='Common/resources/Win32/kernelverbs.rc',
                       help='Path to kernelverbs.rc')
    parser.add_argument('--output', default='generated/kernel_verbs_init.c',
                       help='Output path for generated code')
    parser.add_argument('--dry-run', action='store_true',
                       help='Print generated code without writing')
    parser.add_argument('--verify', action='store_true',
                       help='Exit 1 if generated code differs from current file')

    args = parser.parse_args()

    # Parse RC file
    processors = parse_kernelverbs_rc(args.input)

    # Generate (with dry-run if requested)
    generated = generate_kernel_verbs_init_c(
        processors,
        args.input,
        args.output,
        dry_run=args.dry_run or args.verify
    )

    # Verify mode: exit 1 if differs
    if args.verify:
        if os.path.exists(args.output):
            with open(args.output, 'r') as f:
                current = f.read()
            if current != generated:
                print("\n❌ VERIFICATION FAILED: Generated code differs from committed version")
                print("Run: python3 tools/kernelverbs_parser/parse_kernelverbs.py")
                sys.exit(1)
        print("\n✓ VERIFICATION PASSED: Generated code matches committed version")
```

**Usage Examples**:
```bash
# Preview what would be generated
python3 tools/kernelverbs_parser/parse_kernelverbs.py --dry-run

# Verify current file is up to date (for CI)
python3 tools/kernelverbs_parser/parse_kernelverbs.py --verify

# Actually generate
python3 tools/kernelverbs_parser/parse_kernelverbs.py
```

### Test Infrastructure Integration

#### Unit Tests for Analyzer Logic

**File**: `tools/kernelverbs_parser/test_verb_analyzer.py`

**Coverage**:
```python
import pytest
from analyze_implementations import VerbImplementationAnalyzer

class TestStubDetection:
    """Test stub pattern detection."""

    def test_explicit_stub_pattern(self):
        """Detect explicit 'not implemented' stubs."""
        code = '''
            case filv_type:
                if (bserror) copystring(BIGSTRING("\\pnot implemented"), bserror);
                return false;
        '''
        analyzer = VerbImplementationAnalyzer()
        assert analyzer._is_stub_implementation(code) == True

    def test_trivial_return_false(self):
        """Detect trivial return false as stub."""
        code = '''
            case filv_creator:
                return false;
        '''
        analyzer = VerbImplementationAnalyzer()
        assert analyzer._is_stub_implementation(code) == True

    def test_real_implementation(self):
        """Don't flag real implementations as stubs."""
        code = '''
            case filv_created:
                struct stat st;
                bigstring path;
                if (!getpathvalue(hparam1, 1, path)) return false;
                if (stat(path, &st) != 0) return false;
                return setlongvalue(st.st_ctime, vreturned);
        '''
        analyzer = VerbImplementationAnalyzer()
        assert analyzer._is_stub_implementation(code) == False


class TestUIAdapterDetection:
    """Test UI adapter pattern detection."""

    def test_adapter_function_call(self):
        """Detect adapter_* function calls."""
        code = '''
            case dv_alert:
                bigstring message;
                DialogResult result;
                if (!getstringvalue(hparam1, 1, message)) return false;
                if (!adapter_alert(message, &result)) return false;
                return setlongvalue(result, vreturned);
        '''
        analyzer = VerbImplementationAnalyzer()
        assert analyzer._uses_ui_adapter(code) == True
        assert analyzer._has_carbon_dependencies(code) == False

    def test_stderr_adapter_pattern(self):
        """Detect fprintf(stderr, ...) adapter pattern."""
        code = '''
            case dv_notify:
                bigstring message;
                if (!getstringvalue(hparam1, 1, message)) return false;
                fprintf(stderr, "[ALERT] %s\\n", stringbaseaddress(message));
                return setbooleanvalue(true, vreturned);
        '''
        analyzer = VerbImplementationAnalyzer()
        assert analyzer._uses_ui_adapter(code) == True

    def test_adapter_comment_annotation(self):
        """Detect UI adapter comment annotations."""
        code = '''
            case dv_ask:
                // UI adapter: read from stdin instead of dialog
                bigstring prompt, result;
                if (!getstringvalue(hparam1, 1, prompt)) return false;
                printf("%s: ", stringbaseaddress(prompt));
                fgets(result, sizeof(result), stdin);
                return setstringvalue(result, vreturned);
        '''
        analyzer = VerbImplementationAnalyzer()
        assert analyzer._uses_ui_adapter(code) == True


class TestCarbonDependencyDetection:
    """Test Carbon API dependency detection."""

    def test_windowptr_usage(self):
        """Detect WindowPtr usage."""
        code = '''
            case windowv_show:
                WindowPtr w;
                if (!getwindowptr(hparam1, 1, &w)) return false;
                ShowWindow(w);
                return setbooleanvalue(true, vreturned);
        '''
        analyzer = VerbImplementationAnalyzer()
        assert analyzer._has_carbon_dependencies(code) == True
        assert analyzer._uses_ui_adapter(code) == False

    def test_carbon_include(self):
        """Detect Carbon framework includes."""
        code = '''
            #include <Carbon/Carbon.h>

            case menuv_new:
                MenuRef menu = NewMenu(128, "\\pFile");
                return setmenuvalue(menu, vreturned);
        '''
        analyzer = VerbImplementationAnalyzer()
        assert analyzer._has_carbon_dependencies(code) == True

    def test_portable_code_no_deps(self):
        """Don't flag portable code."""
        code = '''
            case strv_upper:
                bigstring bs;
                if (!getstringvalue(hparam1, 1, bs)) return false;
                allupper(bs);
                return setstringvalue(bs, vreturned);
        '''
        analyzer = VerbImplementationAnalyzer()
        assert analyzer._has_carbon_dependencies(code) == False
        assert analyzer._uses_ui_adapter(code) == False


class TestAnnotationParsing:
    """Test annotation parsing."""

    def test_parse_single_annotation(self):
        """Parse single annotation tag."""
        code = '''
            case filv_type:
                /* @STUB @OBSOLETE */
                if (bserror) copystring(BIGSTRING("\\pnot implemented"), bserror);
                return false;
        '''
        from analyze_implementations import parse_annotations
        annotations = parse_annotations(code)
        assert '@STUB' in annotations
        assert '@OBSOLETE' in annotations

    def test_parse_with_description(self):
        """Parse annotations with description text."""
        code = '''
            case filv_creator:
                /* @STUB @OBSOLETE - Mac creator/type codes not supported */
                return false;
        '''
        from analyze_implementations import parse_annotations
        annotations = parse_annotations(code)
        assert '@STUB' in annotations
        assert '@OBSOLETE' in annotations

    def test_ui_adapter_annotation(self):
        """Parse @UI_ADAPTER annotation."""
        code = '''
            case dv_alert:
                /* @IMPLEMENTED @UI_ADAPTER */
                return adapter_alert(message, &result);
        '''
        from analyze_implementations import parse_annotations
        annotations = parse_annotations(code)
        assert '@IMPLEMENTED' in annotations
        assert '@UI_ADAPTER' in annotations


# Run with: pytest tools/kernelverbs_parser/test_verb_analyzer.py
```

**Test Execution**:
```bash
# Run unit tests
pytest tools/kernelverbs_parser/test_verb_analyzer.py -v

# Run with coverage
pytest tools/kernelverbs_parser/test_verb_analyzer.py --cov=analyze_implementations --cov-report=html
```

#### Makefile Integration

**Add to**: `Makefile` (or `tests/Makefile`)

```makefile
# Verify kernel verb bindings are up to date
.PHONY: verify-verb-bindings
verify-verb-bindings:
	@echo "Verifying kernel verb bindings are up to date..."
	@python3 tools/kernelverbs_parser/parse_kernelverbs.py --verify

# Regenerate kernel verb bindings
.PHONY: regenerate-verb-bindings
regenerate-verb-bindings:
	@echo "Regenerating kernel verb bindings..."
	@python3 tools/kernelverbs_parser/parse_kernelverbs.py
	@echo "✓ Done. Review changes and commit if correct."

# Generate verb status report
.PHONY: verb-status
verb-status:
	@echo "Analyzing kernel verb implementations..."
	@python3 tools/kernelverbs_parser/analyze_implementations.py \
		--tests-dir tests \
		--output-md planning/phase3/verb_implementation_status.md \
		--output-json generated/verb_status.json
	@echo "✓ Reports generated:"
	@echo "  - planning/phase3/verb_implementation_status.md"
	@echo "  - generated/verb_status.json"

# Add verification to main test target
test: verify-verb-bindings
	@echo "Running test suite..."
	make -C tests test

# Integration test: regenerate, compile, test
.PHONY: test-verb-bindings
test-verb-bindings:
	@echo "Testing verb binding system..."
	@echo "1. Regenerating bindings..."
	python3 tools/kernelverbs_parser/parse_kernelverbs.py
	@echo "2. Rebuilding tests..."
	make -C tests clean
	make -C tests
	@echo "3. Running runtime tests..."
	make -C tests runtime_tests
	@echo "✓ Verb binding system tests passed"
```

**Usage**:
```bash
# Verify bindings are current (CI usage)
make verify-verb-bindings

# Regenerate bindings after adding new implementation
make regenerate-verb-bindings

# Generate status report
make verb-status

# Full integration test
make test-verb-bindings
```

#### CI Integration

**GitHub Actions Example**:
```yaml
# .github/workflows/ci.yml
name: CI

on: [push, pull_request]

jobs:
  verify-verb-bindings:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Set up Python
        uses: actions/setup-python@v4
        with:
          python-version: '3.10'

      - name: Verify kernel verb bindings are up to date
        run: |
          python3 tools/kernelverbs_parser/parse_kernelverbs.py --verify

      - name: Generate status report (for artifacts)
        run: |
          python3 tools/kernelverbs_parser/analyze_implementations.py \
            --tests-dir tests \
            --output-md /tmp/verb_status.md \
            --output-json /tmp/verb_status.json

      - name: Upload verb status report
        uses: actions/upload-artifact@v3
        with:
          name: verb-status-report
          path: |
            /tmp/verb_status.md
            /tmp/verb_status.json

  test:
    runs-on: macos-latest
    needs: verify-verb-bindings
    steps:
      - uses: actions/checkout@v3

      - name: Build and test
        run: |
          make -C tests
          make -C tests test
```

#### Pre-Commit Hook (Optional)

**File**: `.git/hooks/pre-commit` (or document in CONTRIBUTING.md)

```bash
#!/bin/bash
# Pre-commit hook: verify kernel verb bindings are up to date

echo "Checking kernel verb bindings..."

python3 tools/kernelverbs_parser/parse_kernelverbs.py --verify

if [ $? -ne 0 ]; then
    echo ""
    echo "❌ Kernel verb bindings are out of date!"
    echo ""
    echo "To fix, run:"
    echo "  make regenerate-verb-bindings"
    echo ""
    echo "To skip this check (not recommended):"
    echo "  git commit --no-verify"
    echo ""
    exit 1
fi

echo "✓ Kernel verb bindings are up to date"
```

**Installation Instructions** (in CONTRIBUTING.md):
```bash
# Install pre-commit hook (optional but recommended)
cp docs/pre-commit.sample .git/hooks/pre-commit
chmod +x .git/hooks/pre-commit
```

### Migration Verification Strategy

**Purpose**: Ensure transition from manual whitelist to auto-detection is safe.

**Phase 1: Baseline Capture**
```bash
# Before enabling auto-detection, capture current state
python3 tools/kernelverbs_parser/parse_kernelverbs.py > /tmp/baseline_init.c
cp generated/kernel_verbs_init.c generated/kernel_verbs_init.c.manual

# Generate status report with manual whitelist
python3 tools/kernelverbs_parser/analyze_implementations.py \
    --tests-dir tests \
    --output-json /tmp/baseline_status.json
```

**Phase 2: Enable Auto-Detection**
```python
# In parse_kernelverbs.py, add flag to switch modes
USE_AUTO_DETECTION = True  # Set to False to use manual whitelist

if USE_AUTO_DETECTION:
    implemented_procs = auto_detect_implementations(processors, tests_dir)
else:
    implemented_procs = [p for p in processors if p.name in HEADLESS_REGISTERED]
```

**Phase 3: Compare Results**
```bash
# Generate with auto-detection
USE_AUTO_DETECTION=1 python3 tools/kernelverbs_parser/parse_kernelverbs.py \
    > /tmp/autodetect_init.c

# Diff against baseline
diff -u /tmp/baseline_init.c /tmp/autodetect_init.c

# Expected: no differences (or only additions for newly implemented processors)
```

**Phase 4: Validation Tests**
```bash
# Compile with auto-detected bindings
make -C tests clean
make -C tests

# Run full test suite
make -C tests test

# Verify all tests pass
./tests/runtime_tests
./tests/cli_runtime_tests
```

**Phase 5: Commit Auto-Detection**
```bash
# If validation passes, commit the switch
git add tools/kernelverbs_parser/parse_kernelverbs.py
git commit -m "switch: enable automatic verb binding detection

Verified that auto-detection produces identical output to manual
whitelist. All tests pass.

Baseline comparison: /tmp/baseline_init.c vs /tmp/autodetect_init.c
No differences detected."
```

### Rollback Plan

**If auto-detection produces incorrect results:**

```bash
# Quick rollback: revert to manual whitelist
git revert HEAD

# Or: temporarily disable auto-detection
# In parse_kernelverbs.py:
USE_AUTO_DETECTION = False  # Back to manual whitelist

# Regenerate with manual whitelist
python3 tools/kernelverbs_parser/parse_kernelverbs.py

# File bug report with details
# Include: diff output, false positives/negatives, affected processors
```

### Quality Gates

**Before merging auto-detection implementation:**

- [ ] All unit tests pass (stub detection, UI adapter detection, Carbon detection)
- [ ] Dry-run mode works correctly
- [ ] Verify mode works correctly
- [ ] Manual whitelist → auto-detection produces identical output
- [ ] All existing tests pass with auto-generated bindings
- [ ] Status report generation works
- [ ] JSON metadata is valid and complete
- [ ] Documentation updated (README.md, CONTRIBUTING.md)
- [ ] Makefile targets work (`verify-verb-bindings`, `regenerate-verb-bindings`)
- [ ] CI integration tested

**Before removing manual whitelist:**

- [ ] Auto-detection has been stable for at least 2 weeks
- [ ] No false positives/negatives reported
- [ ] At least 3 new processor implementations successfully detected
- [ ] Team consensus that auto-detection is reliable

## Success Criteria

### Phase 1 Success Criteria

**Analyzer Works:**
- ✅ Correctly classifies 95%+ of verbs as implemented/stubbed
- ✅ Detects all verbs defined in kernelverbs.rc
- ✅ Zero false negatives for UI dependencies
- ✅ Unit tests cover all detection patterns
- ✅ Generates readable status reports

**Validation:**
```bash
# Run analyzer
make verb-status

# Manual spot-check 50 verbs across 10 processors
# Verify accuracy >= 95%

# Check UI dependency detection
# Verify all known UI-dependent verbs are flagged
```

### Phase 2 Success Criteria

**Auto-Detection Replaces Manual Whitelist:**
- ✅ `parse_kernelverbs.py` uses analyzer instead of `HEADLESS_REGISTERED`
- ✅ Generated init code includes implementation percentages
- ✅ Build process generates status report automatically
- ✅ Zero manual edits needed when adding new implementation

**Validation:**
```bash
# Remove HEADLESS_REGISTERED from parse_kernelverbs.py
# Run parser
python3 tools/kernelverbs_parser/parse_kernelverbs.py

# Verify generated code compiles
make clean && make

# Verify same processors are initialized as before
diff <(old_init_code) <(new_init_code)
```

### Final Success Criteria

**Zero Manual Overhead:**
- ✅ Adding new verb implementation requires ZERO tool changes
- ✅ Implementation status is always accurate
- ✅ Reports generate automatically on every build
- ✅ Developer can see implementation gaps at a glance
- ✅ UI dependencies auto-detected and flagged
- ✅ Build fails if UI dependencies leak into headless code

**Developer Experience:**
```bash
# Developer implements new verb in C file
vim tests/headless_clock_verbs.c

# Developer commits changes
git add tests/headless_clock_verbs.c
git commit -m "Implement clock.waitSeconds"

# Build automatically:
# 1. Detects new implementation
# 2. Updates init code
# 3. Generates new status report
# 4. Shows updated percentages
make

# Developer can check status
make verb-status
# Output:
# clock - 6/7 (85.7%)
# +1 verb since last build
```

### Metrics

**Accuracy Metrics:**
- Stub detection: >= 95% accuracy
- UI dependency detection: 100% recall (no false negatives)
- Platform detection: >= 90% accuracy
- Completeness validation: 100% (all verbs accounted for)

**Performance Metrics:**
- Analyzer runtime: < 5 seconds for all 51 processors
- Build time impact: < 2 seconds additional
- Report generation: < 3 seconds

**Maintenance Metrics:**
- False positive rate: < 10% (acceptable)
- False negative rate: 0% for UI deps, < 5% for stubs
- Manual overrides needed: < 3% of verbs

---

## Trade-off Analysis

| Aspect | Manual Whitelist (Current) | Static Analysis (Recommended) | Annotations Only | Full Macros |
|--------|---------------------------|-------------------------------|------------------|-------------|
| **Automatic detection** | ✗ | ✓ | ✗ | ✗ |
| **Accuracy** | 100% (manual) | 95%+ (heuristic) | 100% (explicit) | N/A |
| **Maintenance burden** | High | Low | Medium | Medium |
| **Verb-level granularity** | ✗ | ✓ | ✓ | ✗ |
| **C code changes** | None | None | Comments only | Significant |
| **Implementation effort** | 0 (done) | Medium (2-3 weeks) | Low (1 week) | High (4-6 weeks) |
| **Scales to 700 verbs** | Poor | Excellent | Good | Poor |
| **Report generation** | Manual | Automatic | Automatic | Manual |
| **UI dependency detection** | ✗ | ✓ | ✓ (with tags) | ✗ |
| **Build integration** | ✓ | ✓ | ✓ | Complex |
| **Debug experience** | Good | Good | Good | Difficult |

**Winner:** Static Analysis (best balance of automation, accuracy, and low overhead)

---

## Risk Assessment

### Low Risks

**False Positive Stub Detection**
- **Description:** Analyzer thinks something is stubbed when it's implemented
- **Impact:** Low - just means processor doesn't get auto-registered
- **Likelihood:** <5% of cases
- **Mitigation:** Conservative heuristics, annotations for edge cases
- **Consequence:** Build will fail fast with linker error, easy to fix

**Performance Overhead**
- **Description:** Analyzer adds time to build process
- **Impact:** Low - ~2 seconds per build
- **Likelihood:** 100%
- **Mitigation:** Optimize parser, cache results
- **Consequence:** Acceptable trade-off for automation

### Medium Risks

**False Negative Stub Detection**
- **Description:** Analyzer thinks stub is implemented
- **Impact:** Medium - runtime error when verb is called
- **Likelihood:** <5% of cases
- **Mitigation:** Comprehensive testing, integration tests
- **Consequence:** Runtime error, needs debugging

**Parsing Edge Cases**
- **Description:** Unusual C code confuses parser
- **Impact:** Medium - incorrect classification
- **Likelihood:** 5-10% of cases
- **Mitigation:** Annotation support for overrides
- **Consequence:** Manual annotation needed

### Critical Risks

**False Negative UI Dependency Detection**
- **Description:** Analyzer misses Carbon/UI code
- **Impact:** **CRITICAL** - UI code leaks into headless binary
- **Likelihood:** Target: 0%
- **Mitigation:**
  - Conservative patterns (err on false positive side)
  - Build-time link checks (`nm -u | grep Carbon`)
  - Integration testing
  - Code review for all new implementations
- **Consequence:** Binary won't link or will crash at runtime

**Mitigation Strategy:**
```makefile
# Add to build system
.PHONY: verify-no-ui-deps
verify-no-ui-deps: frontier-cli
	@echo "Verifying no UI dependencies in headless binary..."
	@if nm -u frontier-cli | grep -i carbon; then \
		echo "ERROR: Carbon symbols found!"; \
		exit 1; \
	fi
	@if nm -u frontier-cli | grep -i cocoa; then \
		echo "ERROR: Cocoa symbols found!"; \
		exit 1; \
	fi
	@echo "✓ No UI dependencies detected"

# Run after every build
all: frontier-cli verify-no-ui-deps
```

---

## References

### Planning Documents

- `planning/_CURRENT_STATUS.md` - Overall project status
- `planning/_CURRENT_TODO_LIST.md` - Active TODO items
- `planning/phase3/kernel_verb_porting/README.md` - Kernel verb porting overview
- `planning/phase3/kernel_verb_porting/kernel_verb_port_architecture.md` - Detailed architecture
- `planning/phase3/kernel_verb_porting/KERNEL_VERB_PORTING_SUMMARY.md` - Executive summary
- `planning/phase3/kernel_verb_porting/kernel_verb_porting_technical_guide.md` - Technical guide

### Source Files

**Resource Definitions:**
- `Common/resources/Win32/kernelverbs.rc` - Verb definitions
- `Common/headers/kernelverbdefs.h` - EFP ID constants

**Dispatch Implementation:**
- `Common/source/kernel_verbs_headless.c` - Registration code
- `Common/source/langverbs.c` - Legacy dispatcher

**Headless Implementations:**
- `tests/headless_*_verbs.c` - 51 processor files

**Automation Tools:**
- `tools/kernelverbs_parser/parse_kernelverbs.py` - Main parser
- `tools/kernelverbs_parser/generate_processor_stubs.py` - Stub generator
- `tools/verb_analyzer/analyze_verbs.py` - Status analyzer

### Key Insights from Exploration

**From Explore Agent:**
- 51 processors, 707 verbs total
- Token-based dispatch already highly automated
- Stub generator already eliminates 90%+ boilerplate
- Manual whitelist is the only remaining gap

**From System Architect:**
- Static analysis is best fit for C codebase
- Heuristic detection can achieve 95%+ accuracy
- Annotation support needed for edge cases
- Zero C code changes is critical requirement

---

## Conclusion

The recommended architecture (Static Analysis + Optional Annotations) provides:

1. **Immediate Value** - Auto-generates implementation status without manual tracking
2. **Zero Manual Overhead** - Works with existing codebase, no C changes needed
3. **Scalability** - Handles all 700 verbs automatically
4. **Safety** - Detects UI dependencies to prevent contamination
5. **Developer Experience** - Clear visibility into what's implemented vs stubbed
6. **Future-Proof** - Easy to extend with new metadata (platform support, complexity, etc.)

**The existing parser infrastructure is well-designed** and just needs this final piece - automatic implementation detection - to become fully automated. This is achievable in **2-3 weeks** with medium complexity Python code that leverages pattern matching and heuristics.

**Next Steps:**
1. Review and approve this architecture
2. Begin Phase 1 implementation (basic analyzer)
3. Validate accuracy on sample processors
4. Iterate and enhance based on findings
5. Deploy to production workflow

---

**Document Version:** 1.0
**Created:** 2025-12-13
**Status:** Ready for Implementation
**Estimated Effort:** 2-3 weeks
**Implementation Owner:** TBD
