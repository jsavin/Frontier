#!/usr/bin/env python3
"""
parse_kernelverbs.py - Generate kernel_verbs_init.c from kernelverbs.rc

Parses the Windows resource file containing EFP (External Function Processor)
definitions and generates C initialization code that calls all verb processor
init functions.

The registered processor set is derived from headless_verbs.mk (single source
of truth). Filenames matching headless_{name}_verbs.c are extracted, filtered
against EXCLUDED_PROCESSORS, and cross-referenced with kernelverbs.rc.

Usage:
    python3 parse_kernelverbs.py <input.rc> <output.c> --mk-path <headless_verbs.mk>

Example:
    python3 parse_kernelverbs.py Common/resources/Win32/kernelverbs.rc generated/kernel_verbs_init.c --mk-path tests/headless_verbs.mk
"""

import sys
import re
from typing import List, Set, Tuple
from pathlib import Path

# Try to import analyzer for reporting (optional, not used for registration)
try:
    from analyzer import VerbImplementationAnalyzer
    from metadata_writer import VerbMetadataWriter
    ANALYZER_AVAILABLE = True
except ImportError:
    ANALYZER_AVAILABLE = False


# Processors excluded from registration even if present in headless_verbs.mk.
# Each entry should have a comment explaining why.
EXCLUDED_PROCESSORS: Set[str] = {
    'webserver',  # Script-implemented; EFP stub shadows builtins.webserver
}

# Processors whose initverbs() is defined outside tests/headless_*_verbs.c stubs.
# These are real implementations linked into both test and CLI builds. The init
# function is forward-declared in the generated kernel_verbs_init.c and called
# from headless_init_kernel_verbs(); the linker resolves the symbol from
# whichever source file actually defines it.
#
# Source locations vary:
#   - Common/source/<name>.c for core runtime processors (math, crypt, menu)
#   - frontier-cli/headless_<name>_verbs.c for CLI-resident processors (thread)
# Both are pulled into the test and CLI Makefiles' source lists, so adding a
# name here is sufficient to wire the processor without a tests/ stub.
CORE_IMPLEMENTED_PROCESSORS: Set[str] = {
    'math',    # langmath.c - mathinitverbs()
    'crypt',   # langcrypt.c - cryptinitverbs()
    'menu',    # menuverbs_headless.c - menuinitverbs() (consolidated per issue #585)
    'thread',  # frontier-cli/headless_thread_verbs.c - threadinitverbs() (issue #614)
}


def validate_input_paths(input_path: str, output_path: str) -> bool:
    """
    Validate that input/output paths are reasonable and directory structure exists.

    Returns True if valid, False otherwise.
    Prints detailed error messages to stderr on validation failure.
    """
    input_file = Path(input_path)
    output_file = Path(output_path)

    # Check if input file exists
    if not input_file.exists():
        print(f"ERROR: Input file not found: {input_path}", file=sys.stderr)
        print(f"Expected: kernelverbs.rc file", file=sys.stderr)
        return False

    # Validate it's the expected filename
    if input_file.name != 'kernelverbs.rc':
        print(f"WARNING: Input file is not named 'kernelverbs.rc': {input_file.name}", file=sys.stderr)
        print(f"This may work but is unexpected.", file=sys.stderr)

    # Check that input is under a reasonable path structure
    # Should be under Common/resources/Win32/ or similar
    try:
        parts = input_file.parts
        if 'Common' not in parts or 'resources' not in parts:
            print(f"WARNING: Input file not in expected Common/resources/ path", file=sys.stderr)
            print(f"  Path: {input_path}", file=sys.stderr)
            print(f"  Expected something like: Common/resources/Win32/kernelverbs.rc", file=sys.stderr)
    except:
        pass  # Path analysis failed, continue anyway

    # Ensure output directory exists or can be created
    output_dir = output_file.parent
    if not output_dir.exists():
        try:
            print(f"Creating output directory: {output_dir}", file=sys.stderr)
            output_dir.mkdir(parents=True, exist_ok=True)
        except Exception as e:
            print(f"ERROR: Cannot create output directory: {output_dir}", file=sys.stderr)
            print(f"  Reason: {e}", file=sys.stderr)
            return False

    # Check output path is reasonable
    if output_file.suffix != '.c':
        print(f"WARNING: Output file extension is not '.c': {output_file.suffix}", file=sys.stderr)

    return True


# Regex pattern constants with documentation
EFP_BLOCK_PATTERN = r'(\d+)\s+/\*[^*]*\*/\s+EFP\s+DISCARDABLE'
BEGIN_PATTERN = r'BEGIN'
END_PATTERN = r'\bEND\b'

# PROCESSOR_PATTERN matches EFP processor definitions in kernelverbs.rc format
#
# Expected RC format (within BEGIN...END blocks):
#   "processorname\0", ...additional_fields..., true/false, <verb_count>
#
# Pattern breakdown:
#   "([^"]+)\\0"           - Captures processor name: matches quoted string ending with \0
#   [^,]*,                 - Skips intermediate fields until first comma
#   \s*(?://[^\n]*)?       - Skips optional inline comment with optional // and trailing content
#   \s*(true|false)        - Captures window_required boolean (group 2)
#   \s*,                   - Skips to next comma
#   \s*(?://[^\n]*)?       - Skips optional inline comment
#   \s*(\d+)               - Captures verb count as integer (group 3)
#
# Groups:
#   1: Processor name (e.g., "file", "frontier", "database")
#   2: Window required flag: "true" or "false"
#   3: Verb count: number of verbs this processor implements
#
# Example matches:
#   "file\0", 0, true, 86           -> name="file", window_required=true, verb_count=86
#   "frontier\0", 0, false, 14      -> name="frontier", window_required=false, verb_count=14
#   "xml\0", 0, true, 28 // comment -> name="xml", window_required=true, verb_count=28
#
PROCESSOR_PATTERN = r'"([^"]+)\\0"[^,]*,\s*(?://[^\n]*)?\s*(true|false)\s*,\s*(?://[^\n]*)?\s*(\d+)'
IDENTIFIER_PATTERN = r'^[a-zA-Z_][a-zA-Z0-9_]*$'


class EFPProcessor:
    """Represents an External Function Processor from kernelverbs.rc"""

    def __init__(self, efp_id: str, name: str, window_required: bool, verb_count: int, verb_names: List[str] = None):
        self.efp_id = efp_id
        self.name = name
        self.window_required = window_required
        self.verb_count = verb_count
        self.verb_names = verb_names if verb_names is not None else []
        self.init_func = f"{name}initverbs"

    def __repr__(self):
        return f"EFP({self.efp_id}, {self.name}, {self.verb_count} verbs)"


def extract_verb_names(block_content: str, start_pos: int, expected_count: int) -> List[str]:
    """
    Extract verb names from the RC block content after a processor definition.

    Format in RC file:
        "processorname\0",
            true/false,
                count,
                "verb1\0",
                "verb2\0",
                ...

    Args:
        block_content: The content between BEGIN...END
        start_pos: Position after the processor definition line
        expected_count: Expected number of verbs

    Returns:
        List of verb names (without \0 terminators)
    """
    verb_names = []

    # Pattern to match quoted strings with \0 terminator
    # Matches: "verbname\0"
    verb_pattern = r'"([^"]+)\\0"'

    # Search from start_pos onward for verb names
    remaining = block_content[start_pos:]

    for match in re.finditer(verb_pattern, remaining):
        verb_name = match.group(1)

        # Stop if we hit the next processor definition (which also has \0)
        # This is a heuristic: if the "verb" looks like a processor name, we've gone too far
        # Processor names are typically lowercase single words without special chars
        # Actual verbs are also lowercase, so we use count as the primary limiter

        # Normalize to lowercase for consistent matching in analyzer
        # RC file may have camelCase (getCursor, gotoName) but C code uses lowercase
        # ASSUMPTION: All C case labels use lowercase naming (e.g., getcursorfunc, gotonamefunc)
        # If this assumption is violated, patterns will fail to match and verbs will be missed
        verb_names.append(verb_name.lower())

        # Stop when we've found the expected number
        if len(verb_names) >= expected_count:
            break

    return verb_names


def strip_preprocessor_conditionals(content: str) -> str:
    """
    Strip #ifdef/#else/#endif directives from RC content for regex-based parsing.

    Strategy:
    - If an #ifdef has an #else: keep only the #else branch
      (e.g., wp's #ifdef flvariables: keep the 27-verb count, not the 36)
    - If an #ifdef has no #else: keep the #ifdef branch content
      (e.g., #ifdef flregexpverbs wraps the entire re processor - we want it)

    This is a simple, non-nested approach sufficient for kernelverbs.rc.

    Args:
        content: Raw RC file content

    Returns:
        Content with preprocessor directives removed
    """
    lines = content.split('\n')
    result = []
    ifdef_lines = []  # accumulate lines in #ifdef branch
    in_ifdef = False
    in_else = False
    has_else = False

    for line in lines:
        stripped = line.strip()
        if stripped.startswith('#ifdef') or stripped.startswith('#ifndef'):
            if in_ifdef:
                print(f"ERROR: Nested #ifdef detected in RC file (not supported): {stripped}",
                      file=sys.stderr)
                sys.exit(1)
            in_ifdef = True
            in_else = False
            has_else = False
            ifdef_lines = []
            continue
        elif stripped.startswith('#else') and in_ifdef:
            in_else = True
            has_else = True
            continue
        elif stripped.startswith('#endif') and in_ifdef:
            if not has_else:
                # No #else branch: keep the #ifdef content
                result.extend(ifdef_lines)
            in_ifdef = False
            in_else = False
            has_else = False
            ifdef_lines = []
            continue

        if in_ifdef and not in_else:
            # In #ifdef branch (before any #else)
            ifdef_lines.append(line)
        elif in_ifdef and in_else:
            # In #else branch: always keep
            result.append(line)
        else:
            # Not inside any conditional
            result.append(line)

    return '\n'.join(result)


def parse_kernelverbs_rc(rc_path: str) -> Tuple[List[EFPProcessor], bool]:
    """
    Parse kernelverbs.rc and extract all EFP processor definitions.

    Returns a tuple of (processor list, had_errors flag).

    The had_errors flag indicates whether any processors were skipped due to
    validation errors (as opposed to just being unimplemented).

    Args:
        rc_path: Path to the kernelverbs.rc file

    Returns:
        Tuple of (list of EFPProcessor objects, bool indicating if errors occurred)
    """
    with open(rc_path, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()

    # Strip preprocessor conditionals so regex can parse verb counts
    # (e.g., wp processor has #ifdef flvariables around its verb count)
    content = strip_preprocessor_conditionals(content)

    processors = []
    seen_names: Set[str] = set()
    had_errors = False

    # Find all EFP blocks: <number> /*comment*/ EFP DISCARDABLE
    for match in re.finditer(EFP_BLOCK_PATTERN, content):
        efp_id = match.group(1)
        block_start = match.end()

        # Find the BEGIN...END block (limit search to reasonable distance)
        begin_match = re.search(BEGIN_PATTERN, content[block_start:block_start + 100])
        if not begin_match:
            continue

        block_content_start = block_start + begin_match.end()

        # Find matching END
        end_match = re.search(END_PATTERN, content[block_content_start:])
        if not end_match:
            continue

        block_content = content[block_content_start:block_content_start + end_match.start()]

        # Parse processors in block with validation
        for proc_match in re.finditer(PROCESSOR_PATTERN, block_content):
            processor_name = proc_match.group(1)

            # Validate processor name is a valid C identifier
            if not re.match(IDENTIFIER_PATTERN, processor_name):
                print(f"Error: Skipping processor '{processor_name}' (EFP {efp_id}): "
                      f"not a valid C identifier", file=sys.stderr)
                had_errors = True
                continue

            # Check for duplicates
            if processor_name in seen_names:
                print(f"Error: Skipping duplicate processor '{processor_name}' (EFP {efp_id})",
                      file=sys.stderr)
                had_errors = True
                continue

            seen_names.add(processor_name)

            window_required = proc_match.group(2) == 'true'
            verb_count = int(proc_match.group(3))

            # Extract verb names from the RC block
            # The verbs start after the processor definition line
            verb_names = extract_verb_names(block_content, proc_match.end(), verb_count)

            if len(verb_names) != verb_count:
                print(f"Warning: Processor '{processor_name}' expected {verb_count} verbs, "
                      f"found {len(verb_names)}", file=sys.stderr)

            processor = EFPProcessor(efp_id, processor_name, window_required, verb_count, verb_names)
            processors.append(processor)

    return processors, had_errors


def parse_headless_verbs_mk(mk_path: str) -> Set[str]:
    """
    Extract processor names from headless_verbs.mk.

    Reads the .mk file, joins backslash-continuation lines, then extracts
    processor names from filenames matching the pattern headless_{name}_verbs.c.
    Non-matching files (headless_odb_stubs.c, headless_threadglobals.c, etc.)
    are silently skipped.

    Args:
        mk_path: Path to headless_verbs.mk

    Returns:
        Set of processor names (e.g., {'file', 'table', 'op', ...})
    """
    mk_file = Path(mk_path)
    if not mk_file.exists():
        print(f"ERROR: headless_verbs.mk not found: {mk_path}", file=sys.stderr)
        sys.exit(1)

    with open(mk_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Join continuation lines (backslash + newline)
    content = content.replace('\\\n', ' ')

    # Extract processor names from headless_{name}_verbs.c pattern
    processor_names: Set[str] = set()
    for match in re.finditer(r'headless_(\w+)_verbs\.c', content):
        processor_names.add(match.group(1))

    return processor_names


def generate_kernel_verbs_init_c(processors: List[EFPProcessor], rc_path: str, whitelist: Set[str]) -> str:
    """
    Generate the C source code for kernel_verbs_init.c

    Args:
        processors: List of EFPProcessor objects
        rc_path: Path to the source kernelverbs.rc (for comments)
        whitelist: Set of processor names to include

    Returns:
        String containing the complete C source file
    """
    # Filter to only implemented processors using the provided whitelist
    implemented_procs = [p for p in processors if p.name in whitelist]
    unimplemented_procs = [p for p in processors if p.name not in whitelist]

    lines = [
        "/* Auto-generated from kernelverbs.rc - DO NOT EDIT BY HAND */",
        f"/* Generated from: {rc_path} */",
        "/*",
        " * This file is automatically generated by tools/kernelverbs_parser/parse_kernelverbs.py",
        " * To regenerate, run: make in frontier-cli directory",
        " *",
        " * Registered processors come from two sources:",
        " *   - tests/headless_verbs.mk (most processors): the headless_<name>_verbs.c",
        " *     pattern for processors with their own headless stub.",
        " *   - CORE_IMPLEMENTED_PROCESSORS in parse_kernelverbs.py: processors whose",
        " *     initverbs() lives in either Common/source/ (e.g., menu, math, crypt)",
        " *     or frontier-cli/ (e.g., thread) and is already linked into both test",
        " *     and CLI builds.",
        " *",
        " * To add a new processor:",
        " *   1. Either (a) create tests/headless_<name>_verbs.c with <name>initverbs()",
        " *      and add it to tests/headless_verbs.mk, OR (b) add <name> to",
        " *      CORE_IMPLEMENTED_PROCESSORS in parse_kernelverbs.py if the impl",
        " *      lives in Common/source/ or frontier-cli/.",
        " *   2. Run make to regenerate.",
        " */",
        "",
        "#include \"frontier.h\"",
        "#include \"standard.h\"",
        "#include \"langinternal.h\"",
        "",
        "/* Forward declarations for IMPLEMENTED verb processor initialization functions */",
    ]

    # Add forward declarations for implemented processors only
    for proc in implemented_procs:
        lines.append(f"extern boolean {proc.init_func}(void);  /* EFP {proc.efp_id}: {proc.name} ({proc.verb_count} verbs) */")

    total_implemented_verbs = sum(p.verb_count for p in implemented_procs)
    total_all_verbs = sum(p.verb_count for p in processors)

    lines.extend([
        "",
        "/**",
        " * headless_init_kernel_verbs - Initialize all kernel verb processors",
        " *",
        " * This function calls the initialization function for each IMPLEMENTED verb processor",
        " * that has a headless implementation (in tests/headless_*_verbs.c).",
        " *",
        f" * Implemented processors: {len(implemented_procs)} of {len(processors)} total",
        f" * Implemented verbs: {total_implemented_verbs} of {total_all_verbs} total",
        " *",
        " * To add more processors: see the file-header comment at the top — choose",
        " * either the tests/headless_<name>_verbs.c stub path or the",
        " * CORE_IMPLEMENTED_PROCESSORS path (for impls in Common/source/ or frontier-cli/).",
        " *",
        " * Returns: true if all processors initialized successfully, false otherwise",
        " */",
        "boolean headless_init_kernel_verbs(void) {",
    ])

    # Add initialization calls for implemented processors only
    for proc in implemented_procs:
        lines.extend([
            f"    /* Initialize {proc.name} processor (EFP {proc.efp_id}, {proc.verb_count} verbs) */",
            f"    if (!{proc.init_func}())",
            f"        return false;",
            ""
        ])

    lines.extend([
        "    return true;",
        "}",
        ""
    ])

    return "\n".join(lines)


def generate_whitelist_from_analyzer(processors: List[EFPProcessor]) -> Set[str]:
    """
    Automatically generate whitelist by analyzing implementations (reporting only).

    Uses the VerbImplementationAnalyzer to detect which processors have real
    implementations vs. stubs, and returns the whitelist accordingly.

    Args:
        processors: List of EFPProcessor objects from RC file

    Returns:
        Set of processor names that have implementations
    """
    if not ANALYZER_AVAILABLE:
        print("Error: Analyzer not available for automatic whitelist generation", file=sys.stderr)
        return set()

    try:
        print("Running automatic verb binding analyzer...", file=sys.stderr)
        analyzer = VerbImplementationAnalyzer(processors)
        implementations = analyzer.analyze_all_processors()

        # Generate whitelist from analyzer results
        writer = VerbMetadataWriter(implementations)
        whitelist = writer.generate_whitelist()

        # Return as set
        return set(whitelist)
    except Exception as e:
        print(f"Error running analyzer: {e}", file=sys.stderr)
        return set()


def main() -> None:
    """
    Main entry point.

    Exits with:
      0: Success
      1: Fatal errors (missing input, no processors found, I/O errors)
      2: Parsing errors (invalid identifiers, duplicates in RC file)

    Exit code 2 is specifically for CI to detect accidental breakage in kernelverbs.rc.
    """
    # Parse command-line arguments
    import argparse
    parser = argparse.ArgumentParser(
        description='Generate kernel_verbs_init.c from kernelverbs.rc',
        epilog='Example: python3 parse_kernelverbs.py Common/resources/Win32/kernelverbs.rc generated/kernel_verbs_init.c --mk-path tests/headless_verbs.mk'
    )
    parser.add_argument('input_rc', help='Path to kernelverbs.rc')
    parser.add_argument('output_c', help='Path to kernel_verbs_init.c output')
    parser.add_argument('--mk-path', required=True,
                        help='Path to headless_verbs.mk (single source of truth for registered processors)')
    parser.add_argument('--analyze', action='store_true',
                        help='Run analyzer for reporting (does NOT influence registration)')

    args = parser.parse_args()

    input_path = args.input_rc
    output_path = args.output_c

    # Validate paths before proceeding
    if not validate_input_paths(input_path, output_path):
        print("\nValidation failed. Please check paths and try again.", file=sys.stderr)
        sys.exit(1)

    # Parse the RC file
    print(f"Parsing {input_path}...", file=sys.stderr)
    processors, had_parsing_errors = parse_kernelverbs_rc(input_path)

    if not processors:
        print("Warning: No EFP processors found in input file", file=sys.stderr)
        sys.exit(1)

    # Derive whitelist from headless_verbs.mk + core-implemented processors
    mk_processors = parse_headless_verbs_mk(args.mk_path)
    rc_processor_names = {p.name for p in processors}

    # Whitelist = ((mk processors | core implemented) - excluded) intersected with RC
    all_candidates = mk_processors | CORE_IMPLEMENTED_PROCESSORS
    whitelist = (all_candidates - EXCLUDED_PROCESSORS) & rc_processor_names

    # Warn about mk processors not found in RC
    mk_only = mk_processors - EXCLUDED_PROCESSORS - rc_processor_names
    if mk_only:
        print(f"Warning: Processors in .mk but not in RC (skipped): {sorted(mk_only)}", file=sys.stderr)

    # Report excluded processors found in mk
    excluded_found = mk_processors & EXCLUDED_PROCESSORS
    if excluded_found:
        print(f"Excluded processors (in .mk but blocklisted): {sorted(excluded_found)}", file=sys.stderr)

    # Run analyzer for reporting if requested (does NOT influence registration)
    if args.analyze:
        if ANALYZER_AVAILABLE:
            analyzer_whitelist = generate_whitelist_from_analyzer(processors)
            diff = whitelist.symmetric_difference(analyzer_whitelist)
            if diff:
                print(f"Note: Analyzer differs from .mk-derived whitelist on: {sorted(diff)}", file=sys.stderr)
        else:
            print("Warning: --analyze requested but analyzer not available", file=sys.stderr)

    # Use the determined whitelist to categorize processors
    implemented = [p for p in processors if p.name in whitelist]
    unimplemented = [p for p in processors if p.name not in whitelist]

    print(f"Found {len(processors)} verb processors:", file=sys.stderr)
    print(f"\nImplemented in headless mode ({len(implemented)}):", file=sys.stderr)
    for proc in implemented:
        print(f"  + {proc.name:20s} (EFP {proc.efp_id:4s}, {proc.verb_count:3d} verbs)", file=sys.stderr)

    print(f"\nNot yet implemented ({len(unimplemented)}):", file=sys.stderr)
    for proc in unimplemented:
        print(f"  - {proc.name:20s} (EFP {proc.efp_id:4s}, {proc.verb_count:3d} verbs)", file=sys.stderr)

    # Generate the C code
    print(f"\nGenerating {output_path}...", file=sys.stderr)
    c_code = generate_kernel_verbs_init_c(processors, input_path, whitelist)

    # Ensure output directory exists
    output_dir = Path(output_path).parent
    output_dir.mkdir(parents=True, exist_ok=True)

    # Write output file
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(c_code)

    impl_verbs = sum(p.verb_count for p in implemented)
    total_verbs = sum(p.verb_count for p in processors)

    print(f"+ Generated {output_path}", file=sys.stderr)
    print(f"  Implemented processors: {len(implemented)} of {len(processors)}", file=sys.stderr)
    print(f"  Implemented verbs: {impl_verbs} of {total_verbs}", file=sys.stderr)
    print(file=sys.stderr)
    print("To add more processors:", file=sys.stderr)
    print("  1. Create tests/headless_<name>_verbs.c with <name>initverbs()", file=sys.stderr)
    print("  2. Add headless_<name>_verbs.c to tests/headless_verbs.mk", file=sys.stderr)
    print("  3. Run make to regenerate", file=sys.stderr)

    # Exit with appropriate code: 2 if parsing errors, 0 on success
    if had_parsing_errors:
        print("\nNote: Parser encountered errors while processing kernelverbs.rc", file=sys.stderr)
        print("      (See messages above for details)", file=sys.stderr)
        sys.exit(2)


if __name__ == '__main__':
    main()
