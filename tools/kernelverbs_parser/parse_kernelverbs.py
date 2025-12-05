#!/usr/bin/env python3
"""
parse_kernelverbs.py - Generate kernel_verbs_init.c from kernelverbs.rc

Parses the Windows resource file containing EFP (External Function Processor)
definitions and generates C initialization code that calls all verb processor
init functions.

Usage:
    python3 parse_kernelverbs.py <input.rc> <output.c>

Example:
    python3 parse_kernelverbs.py Common/resources/Win32/kernelverbs.rc generated/kernel_verbs_init.c
"""

import sys
import re
from typing import List, Set, Tuple
from pathlib import Path

# Whitelist of processors that have headless implementations
# Only these processors will be included in the generated init function.
# To add a new processor:
# 1. Implement tests/headless_<processor>_verbs.c with <processor>initverbs()
# 2. Add the processor name to this whitelist
# 3. Run make to regenerate kernel_verbs_init.c
HEADLESS_REGISTERED: Set[str] = {
    # Already implemented in main codebase:
    'file',              # fileverbs.c
    'string',            # stringverbs.c
    'table',             # tableverbs.c
    'xml',               # langxml.c
    'html',              # langhtml.c
    'window',            # shellwindowverbs.c (windowinitverbs)
    'db',                # dbverbs.c
    're',                # langregexp.c
    'sys',               # shellsysverbs.c
    'lang',              # langstartup.c
    'crypt',             # langcrypt.c
    'math',              # langmath.c
    'sqlite',            # langsqlite.c
    'mysql',             # langmysql.c

    # New headless stubs (not yet implemented):
    'frontier',          # tests/headless_frontier_verbs.c
    'op',                # tests/headless_op_verbs.c
    'opattributes',      # tests/headless_opattributes_verbs.c
    'script',            # tests/headless_script_verbs.c
    'osa',               # tests/headless_osa_verbs.c
    'menu',              # tests/headless_menu_verbs.c
    'pict',              # tests/headless_pict_verbs.c
    'clock',             # tests/headless_clock_verbs.c
    'date',              # tests/headless_date_verbs.c
    'dialog',            # tests/headless_dialog_verbs.c
    'kb',                # tests/headless_kb_verbs.c
    'mouse',             # tests/headless_mouse_verbs.c
    'point',             # tests/headless_point_verbs.c
    'rectangle',         # tests/headless_rectangle_verbs.c
    'rgb',               # tests/headless_rgb_verbs.c
    'speaker',           # tests/headless_speaker_verbs.c
    'target',            # tests/headless_target_verbs.c
    'bit',               # tests/headless_bit_verbs.c
    'semaphore',         # tests/headless_semaphore_verbs.c
    'base64',            # tests/headless_base64_verbs.c
    'tcp',               # tests/headless_tcp_verbs.c
    'dll',               # tests/headless_dll_verbs.c
    'python',            # tests/headless_python_verbs.c
    'htmlcontrol',       # tests/headless_htmlcontrol_verbs.c
    'statusbar',         # tests/headless_statusbar_verbs.c
    'rez',               # tests/headless_rez_verbs.c
    'search',            # tests/headless_search_verbs.c
    'filemenu',          # tests/headless_filemenu_verbs.c
    'editmenu',          # tests/headless_editmenu_verbs.c
    'launch',            # tests/headless_launch_verbs.c
    'clipboard',         # tests/headless_clipboard_verbs.c
    'thread',            # tests/headless_thread_verbs.c
    'mainwindow',        # tests/headless_mainwindow_verbs.c
    'searchengine',      # tests/headless_searchengine_verbs.c
    'mrcalendar',        # tests/headless_mrcalendar_verbs.c
    'webserver',         # tests/headless_webserver_verbs.c
    'inetd',             # tests/headless_inetd_verbs.c
}

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

    def __init__(self, efp_id: str, name: str, window_required: bool, verb_count: int):
        self.efp_id = efp_id
        self.name = name
        self.window_required = window_required
        self.verb_count = verb_count
        self.init_func = f"{name}initverbs"

    def __repr__(self):
        return f"EFP({self.efp_id}, {self.name}, {self.verb_count} verbs)"


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

            processor = EFPProcessor(efp_id, processor_name, window_required, verb_count)
            processors.append(processor)

    return processors, had_errors


def generate_kernel_verbs_init_c(processors: List[EFPProcessor], rc_path: str) -> str:
    """
    Generate the C source code for kernel_verbs_init.c

    Args:
        processors: List of EFPProcessor objects
        rc_path: Path to the source kernelverbs.rc (for comments)

    Returns:
        String containing the complete C source file

    Note:
        Uses the module-level HEADLESS_REGISTERED whitelist to filter which
        processors get initialization calls in the generated code.
    """
    # Filter to only implemented processors using module-level whitelist
    implemented_procs = [p for p in processors if p.name in HEADLESS_REGISTERED]
    unimplemented_procs = [p for p in processors if p.name not in HEADLESS_REGISTERED]

    lines = [
        "/* Auto-generated from kernelverbs.rc - DO NOT EDIT BY HAND */",
        f"/* Generated from: {rc_path} */",
        "/*",
        " * This file is automatically generated by tools/kernelverbs_parser/parse_kernelverbs.py",
        " * To regenerate, run: make in frontier-cli directory",
        " *",
        " * NOTE: Only processors with headless implementations are included.",
        " * See HEADLESS_REGISTERED whitelist in parse_kernelverbs.py.",
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
        " * To add more processors, implement them in tests/headless_*_verbs.c",
        " * and add to HEADLESS_REGISTERED whitelist in parse_kernelverbs.py.",
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


def main() -> None:
    """
    Main entry point.

    Exits with:
      0: Success
      1: Fatal errors (missing input, no processors found, I/O errors)
      2: Parsing errors (invalid identifiers, duplicates in RC file)

    Exit code 2 is specifically for CI to detect accidental breakage in kernelverbs.rc.
    """
    if len(sys.argv) != 3:
        print("Usage: parse_kernelverbs.py <input.rc> <output.c>", file=sys.stderr)
        print(file=sys.stderr)
        print("Example:", file=sys.stderr)
        print("  python3 parse_kernelverbs.py Common/resources/Win32/kernelverbs.rc generated/kernel_verbs_init.c", file=sys.stderr)
        sys.exit(1)

    input_path = sys.argv[1]
    output_path = sys.argv[2]

    # Validate input file exists
    if not Path(input_path).exists():
        print(f"Error: Input file not found: {input_path}", file=sys.stderr)
        sys.exit(1)

    # Parse the RC file
    print(f"Parsing {input_path}...", file=sys.stderr)
    processors, had_parsing_errors = parse_kernelverbs_rc(input_path)

    if not processors:
        print("Warning: No EFP processors found in input file", file=sys.stderr)
        sys.exit(1)

    # Use the module-level whitelist to categorize processors
    implemented = [p for p in processors if p.name in HEADLESS_REGISTERED]
    unimplemented = [p for p in processors if p.name not in HEADLESS_REGISTERED]

    print(f"Found {len(processors)} verb processors:", file=sys.stderr)
    print(f"\nImplemented in headless mode ({len(implemented)}):", file=sys.stderr)
    for proc in implemented:
        print(f"  ✓ {proc.name:20s} (EFP {proc.efp_id:4s}, {proc.verb_count:3d} verbs)", file=sys.stderr)

    print(f"\nNot yet implemented ({len(unimplemented)}):", file=sys.stderr)
    for proc in unimplemented:
        print(f"  - {proc.name:20s} (EFP {proc.efp_id:4s}, {proc.verb_count:3d} verbs)", file=sys.stderr)

    # Generate the C code
    print(f"\nGenerating {output_path}...", file=sys.stderr)
    c_code = generate_kernel_verbs_init_c(processors, input_path)

    # Ensure output directory exists
    output_dir = Path(output_path).parent
    output_dir.mkdir(parents=True, exist_ok=True)

    # Write output file
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(c_code)

    impl_verbs = sum(p.verb_count for p in implemented)
    total_verbs = sum(p.verb_count for p in processors)

    print(f"✓ Generated {output_path}", file=sys.stderr)
    print(f"  Implemented processors: {len(implemented)} of {len(processors)}", file=sys.stderr)
    print(f"  Implemented verbs: {impl_verbs} of {total_verbs}", file=sys.stderr)
    print(file=sys.stderr)
    print("To add more processors:", file=sys.stderr)
    print("  1. Implement tests/headless_<processor>_verbs.c with <processor>initverbs()", file=sys.stderr)
    print("  2. Add processor name to HEADLESS_REGISTERED in parse_kernelverbs.py", file=sys.stderr)
    print("  3. Run make to regenerate", file=sys.stderr)

    # Exit with appropriate code: 2 if parsing errors, 0 on success
    if had_parsing_errors:
        print("\nNote: Parser encountered errors while processing kernelverbs.rc", file=sys.stderr)
        print("      (See messages above for details)", file=sys.stderr)
        sys.exit(2)


if __name__ == '__main__':
    main()
