#!/usr/bin/env python3
"""
Pattern matching for kernel verb implementation analysis.

This module provides pattern matching to detect:
- Carbon API dependencies (incompatible with headless)
- UI adapter patterns (headless-compatible UI abstraction)
- Stub indicators (unimplemented verbs)
"""

import re
from typing import List


# Carbon/QuickDraw API patterns (headless-incompatible)
CARBON_API_PATTERNS = [
    r'\bWindowPtr\b',
    r'\bGrafPtr\b',
    r'\bMenuRef\b',
    r'\bDialogRef\b',
    r'\bControlRef\b',
    r'\bGetNewWindow\b',
    r'\bShowWindow\b',
    r'\bHideWindow\b',
    r'\bDisposeWindow\b',
    r'\bSetPort\b',
    r'\bGetPort\b',
    r'\bMoveTo\b',
    r'\bLineTo\b',
    r'\bDrawString\b',
    r'\bGetFNum\b',
    r'\bTextFont\b',
    r'\bTextSize\b',
    r'\bTextFace\b',
    r'#include\s*<Carbon\.h>',
    r'#include\s*<Carbon/',
    r'#include\s*<QuickDraw\.h>',
    r'#include\s*<QuickDraw/',
    r'\bEventRecord\b',
    r'\bModalDialog\b',
    r'\bAlert\b',
    r'\bStopAlert\b',
    r'\bCautionAlert\b',
    r'\bNoteAlert\b',
]

# UI adapter patterns (headless-compatible abstraction)
UI_ADAPTER_PATTERNS = [
    r'\badapter_alert\b',
    r'\badapter_ask\b',
    r'\badapter_confirm\b',
    r'fprintf\s*\(\s*stderr.*ALERT',
    r'fprintf\s*\(\s*stderr.*WARNING',
    r'//\s*UI adapter:',
    r'/\*\s*UI adapter:',
    r'@UI_ADAPTER',
]

# Stub indicators (not yet implemented)
STUB_INDICATORS = [
    r'not implemented',
    r'Not implemented',
    r'NOT IMPLEMENTED',
    r'\bTODO\b',
    r'\bFIXME\b',
    r'\bSTUB\b',
    r'\bplaceholder\b',
    r'\bunimplemented\b',
    r'return false;\s*/\*\s*stub',
    r'return 0;\s*/\*\s*stub',
    r'return NULL;\s*/\*\s*stub',
    r'return;\s*/\*\s*stub',
]

# Compile patterns once for efficiency with error handling
def _compile_patterns(patterns: List[str], name: str) -> List:
    """
    Compile regex patterns with error handling.

    Args:
        patterns: List of regex pattern strings
        name: Name of pattern group (for error messages)

    Returns:
        List of compiled regex patterns

    Raises:
        ValueError: If any pattern is malformed
    """
    compiled = []
    for i, pattern in enumerate(patterns):
        try:
            compiled.append(re.compile(pattern, re.IGNORECASE))
        except re.error as e:
            raise ValueError(f"{name}[{i}]: Invalid regex pattern: {pattern!r}\n{e}")
    return compiled

try:
    _carbon_patterns = _compile_patterns(CARBON_API_PATTERNS, "CARBON_API_PATTERNS")
    _ui_adapter_patterns = _compile_patterns(UI_ADAPTER_PATTERNS, "UI_ADAPTER_PATTERNS")
    _stub_patterns = _compile_patterns(STUB_INDICATORS, "STUB_INDICATORS")
except ValueError as e:
    # Fail loudly at module load time if patterns are malformed
    raise ImportError(f"Failed to compile matchers patterns:\n{e}")


def detect_carbon_apis(source: str) -> bool:
    """
    Detect if source code uses Carbon/QuickDraw APIs.

    Args:
        source: Source code to analyze

    Returns:
        True if Carbon APIs detected, False otherwise
    """
    for pattern in _carbon_patterns:
        if pattern.search(source):
            return True
    return False


def detect_ui_adapters(source: str) -> bool:
    """
    Detect if source code uses UI adapter patterns.

    Args:
        source: Source code to analyze

    Returns:
        True if UI adapters detected, False otherwise
    """
    for pattern in _ui_adapter_patterns:
        if pattern.search(source):
            return True
    return False


def detect_stub_verb(source: str) -> bool:
    """
    Detect if verb implementation is a stub.

    Args:
        source: Source code to analyze

    Returns:
        True if stub detected, False otherwise
    """
    for pattern in _stub_patterns:
        if pattern.search(source):
            return True
    return False


def parse_annotations(source: str) -> dict:
    """
    Parse source annotations that override heuristics.

    Supported annotations:
        @UI_ADAPTER - Force UI adapter detection
        @CARBON_DEPS - Force Carbon dependency detection
        @PLATFORM_SPECIFIC - Mark as platform-specific

    Args:
        source: Source code to analyze

    Returns:
        Dictionary of annotation flags
    """
    annotations = {
        'ui_adapter': False,
        'carbon_deps': False,
        'platform_specific': False,
    }

    if re.search(r'@UI_ADAPTER', source, re.IGNORECASE):
        annotations['ui_adapter'] = True

    if re.search(r'@CARBON_DEPS', source, re.IGNORECASE):
        annotations['carbon_deps'] = True

    if re.search(r'@PLATFORM_SPECIFIC', source, re.IGNORECASE):
        annotations['platform_specific'] = True

    return annotations


def estimate_complexity(source: str) -> int:
    """
    Estimate implementation complexity (1-5 scale).

    Heuristics:
    - Line count
    - Branching (if/switch)
    - Loop count
    - Function calls

    Args:
        source: Source code to analyze

    Returns:
        Complexity score 1-5 (1=trivial, 5=complex)
    """
    lines = source.split('\n')
    non_empty = [l for l in lines if l.strip() and not l.strip().startswith('//')]
    line_count = len(non_empty)

    # Count branches and loops
    branch_count = len(re.findall(r'\b(if|switch|case)\b', source))
    loop_count = len(re.findall(r'\b(for|while|do)\b', source))
    call_count = len(re.findall(r'\w+\s*\(', source))

    # Simple heuristic scoring
    score = 1

    if line_count > 10:
        score += 1
    if line_count > 30:
        score += 1

    if branch_count > 2:
        score += 1

    if loop_count > 0 or call_count > 5:
        score += 1

    return min(score, 5)


def find_function_definition(source: str, func_name: str) -> tuple:
    """
    Find function definition in source code.

    Args:
        source: Source code to search
        func_name: Function name to find

    Returns:
        Tuple of (line_number, function_source) or (0, "") if not found
    """
    lines = source.split('\n')

    # Pattern for function definition
    # Matches: bool func_name(...) or static bool func_name(...)
    pattern = re.compile(
        rf'^\s*(static\s+)?(bool|int|void|short|long|double|char\s*\*)\s+{re.escape(func_name)}\s*\(',
        re.MULTILINE
    )

    match = pattern.search(source)
    if not match:
        return (0, "")

    # Find line number
    start_pos = match.start()
    line_num = source[:start_pos].count('\n') + 1

    # Extract function body (simplified - assumes brace on next line or same line)
    func_start = start_pos
    brace_count = 0
    in_function = False
    func_end = start_pos

    for i in range(func_start, len(source)):
        if source[i] == '{':
            brace_count += 1
            in_function = True
        elif source[i] == '}':
            brace_count -= 1
            if in_function and brace_count == 0:
                func_end = i + 1
                break

    func_source = source[func_start:func_end]
    return (line_num, func_source)


if __name__ == '__main__':
    # Test patterns
    carbon_code = """
    WindowPtr window = GetNewWindow(128, NULL, (WindowPtr)-1);
    ShowWindow(window);
    """

    ui_adapter_code = """
    bool result = adapter_ask("Continue?", &answer);
    if (!result) return false;
    """

    stub_code = """
    bool myverb() {
        // TODO: implement this
        return false; /* stub */
    }
    """

    implemented_code = """
    bool fileexistsverb() {
        tyvaluerecord val;
        bigstring path;
        if (!getpathparam(1, &val, path))
            return false;
        return setbooleanvalue(fileexists(path), &val);
    }
    """

    print("Carbon API detection:")
    print(f"  Carbon code: {detect_carbon_apis(carbon_code)}")
    print(f"  UI adapter code: {detect_carbon_apis(ui_adapter_code)}")
    print(f"  Stub code: {detect_carbon_apis(stub_code)}")
    print()

    print("UI adapter detection:")
    print(f"  Carbon code: {detect_ui_adapters(carbon_code)}")
    print(f"  UI adapter code: {detect_ui_adapters(ui_adapter_code)}")
    print(f"  Stub code: {detect_ui_adapters(stub_code)}")
    print()

    print("Stub detection:")
    print(f"  Carbon code: {detect_stub_verb(carbon_code)}")
    print(f"  UI adapter code: {detect_stub_verb(ui_adapter_code)}")
    print(f"  Stub code: {detect_stub_verb(stub_code)}")
    print(f"  Implemented code: {detect_stub_verb(implemented_code)}")
    print()

    print("Complexity estimation:")
    print(f"  Stub code: {estimate_complexity(stub_code)}")
    print(f"  Implemented code: {estimate_complexity(implemented_code)}")
