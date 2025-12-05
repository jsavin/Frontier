#!/usr/bin/env python3
"""
generate_processor_stubs.py - Generate stub processor files for all unimplemented verbs

This script parses kernelverbs.rc and generates skeleton C files for all processors
that don't yet have headless implementations.

Usage:
    python3 generate_processor_stubs.py <input.rc> <output_dir> [--implemented processor1,processor2,...]

Example:
    python3 generate_processor_stubs.py \
        Common/resources/Win32/kernelverbs.rc \
        tests \
        --implemented file,frontier
"""

import sys
import re
from typing import List, Set, Tuple
from pathlib import Path

# Import from parse_kernelverbs to reuse processor discovery
sys.path.insert(0, str(Path(__file__).parent))
from parse_kernelverbs import (
    parse_kernelverbs_rc,
    EFPProcessor,
    HEADLESS_IMPLEMENTED
)


def generate_processor_stub(processor: EFPProcessor, verb_names: List[str]) -> str:
    """
    Generate a complete C stub file for a processor.

    Args:
        processor: EFPProcessor object with name and verb count
        verb_names: List of verb names (should match verb_count length)

    Returns:
        String containing complete C source file
    """
    proc_name = processor.name
    verb_count = processor.verb_count

    # Ensure we have the right number of verbs
    if len(verb_names) < verb_count:
        # Pad with generic names if we don't have all verb names
        verb_names = verb_names + [f"verb{i}" for i in range(len(verb_names), verb_count)]

    lines = [
        '#include "frontier.h"',
        '#include "standard.h"',
        '',
        '#include "memory.h"',
        '#include "strings.h"',
        '#include "lang.h"',
        '#include "langinternal.h"',
        '#include "tablestructure.h"',
        '',
        f'/* Token enum for all verbs in the {proc_name} processor */',
        'enum {',
    ]

    # Generate enum with all tokens
    for i in range(verb_count):
        prefix = f"{proc_name[0:3]}v"  # e.g., "strv" for string processor
        token_name = f"{prefix}_{verb_names[i]}"
        if i < verb_count - 1:
            lines.append(f"    {token_name} = {i},")
        else:
            lines.append(f"    {token_name} = {i}")

    lines.extend([
        '};',
        '',
        f'static boolean {proc_name}_valueproc(short token, hdltreenode hparam1,',
        '                                     tyvaluerecord *vreturned,',
        '                                     bigstring bserror) {',
        '    switch(token) {',
    ])

    # Generate switch cases for all verbs (all returning false for now)
    for i in range(verb_count):
        prefix = f"{proc_name[0:3]}v"
        token_name = f"{prefix}_{verb_names[i]}"
        lines.extend([
            f"        case {token_name}:",
            f"            /* Verb #{i} - not yet implemented */",
            f"            if (bserror) copystring(BIGSTRING(\"\\pnot implemented\"), bserror);",
            f"            return false;",
        ])

    lines.extend([
        '        default:',
        '            return false;',
        '    }',
        '}',
        '',
        f'boolean {proc_name}initverbs(void) {{',
        '    hdlhashtable htable = nil;',
        '    bigstring bsname;',
        '',
        f'    copystring(BIGSTRING("\\p{proc_name}"), bsname);',
        '',
        f'    if (!newfunctionprocessor(bsname, &{proc_name}_valueproc, false, &htable))',
        '        return false;',
        '',
        '    pushhashtable(htable);',
        '',
        '    #define ADD_VERB(name, tok) do { \\',
        '        bigstring bs; \\',
        '        copystring(name, bs); \\',
        '        if (!langaddkeyword(bs, tok)) { \\',
        '            pophashtable(); \\',
        '            return false; \\',
        '        } \\',
        '    } while(0)',
        '',
    ])

    # Generate ADD_VERB calls for all verbs
    for i, verb_name in enumerate(verb_names[:verb_count]):
        lines.append(f'    ADD_VERB(BIGSTRING("\\p{verb_name}"), {prefix}_{verb_name});')

    lines.extend([
        '',
        '    #undef ADD_VERB',
        '',
        '    pophashtable();',
        '    return true;',
        '}',
        '',
    ])

    return '\n'.join(lines)


def extract_verb_names(rc_content: str, processor_name: str, verb_count: int) -> List[str]:
    """
    Try to extract actual verb names from RC file for a processor.

    Falls back to generic names if not found.

    Args:
        rc_content: Raw RC file content
        processor_name: Name of processor to find verbs for
        verb_count: Expected number of verbs

    Returns:
        List of verb names
    """
    # This is a simplified extraction - in reality verb names are scattered
    # throughout the RC file in various formats. For now, return generic names.
    # TODO: Parse RC file more carefully to extract actual verb names
    return [f"{processor_name}_verb{i}" for i in range(verb_count)]


def main() -> None:
    """Main entry point."""
    if len(sys.argv) < 3:
        print("Usage: generate_processor_stubs.py <input.rc> <output_dir> [--implemented proc1,proc2,...]",
              file=sys.stderr)
        sys.exit(1)

    input_path = sys.argv[1]
    output_dir = sys.argv[2]

    # Parse optional --implemented flag
    implemented = set(HEADLESS_IMPLEMENTED)
    if len(sys.argv) > 3 and sys.argv[3] == '--implemented':
        implemented = set(sys.argv[4].split(','))

    # Validate input file exists
    if not Path(input_path).exists():
        print(f"Error: Input file not found: {input_path}", file=sys.stderr)
        sys.exit(1)

    # Parse RC file
    print(f"Parsing {input_path}...", file=sys.stderr)
    processors, had_errors = parse_kernelverbs_rc(input_path)

    if not processors:
        print("Error: No EFP processors found in input file", file=sys.stderr)
        sys.exit(1)

    # Separate implemented vs unimplemented
    unimplemented = [p for p in processors if p.name not in implemented]

    print(f"Found {len(unimplemented)} unimplemented processors to generate", file=sys.stderr)

    # Create output directory
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)

    # Read RC file for verb name extraction (optional)
    with open(input_path, 'r', encoding='utf-8', errors='replace') as f:
        rc_content = f.read()

    # Generate stub file for each unimplemented processor
    generated_count = 0
    for proc in unimplemented:
        # Extract verb names (fallback to generic if not found)
        verb_names = extract_verb_names(rc_content, proc.name, proc.verb_count)

        # Generate C code
        c_code = generate_processor_stub(proc, verb_names)

        # Write to file
        output_file = output_path / f"headless_{proc.name}_verbs.c"
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write(c_code)

        print(f"✓ Generated {output_file.name}", file=sys.stderr)
        generated_count += 1

    print(f"\nGenerated {generated_count} processor stub files", file=sys.stderr)
    print(f"Output directory: {output_path}", file=sys.stderr)


if __name__ == '__main__':
    main()
