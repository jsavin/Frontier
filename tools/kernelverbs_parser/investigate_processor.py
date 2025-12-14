#!/usr/bin/env python3
"""
Helper script to investigate processor verb name mappings.

Usage: python3 investigate_processor.py <processor_name>
"""

import sys
import re
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from parse_kernelverbs import parse_kernelverbs_rc
from analyzer import VerbImplementationAnalyzer


def investigate_processor(processor_name: str):
    """
    Investigate a single processor's verb name mappings.

    Shows:
    - RC verb names (source of truth)
    - C enum tokens (if found)
    - C case labels (if found)
    - Mapping between them
    """
    # Parse RC file
    project_root = Path(__file__).parent.parent.parent
    rc_path = project_root / "Common/resources/Win32/kernelverbs.rc"

    processors, _ = parse_kernelverbs_rc(str(rc_path))
    proc = next((p for p in processors if p.name == processor_name), None)

    if not proc:
        print(f"Error: Processor '{processor_name}' not found in RC file")
        return

    print(f"=" * 80)
    print(f"PROCESSOR: {processor_name}")
    print(f"=" * 80)
    print(f"Verb count: {proc.verb_count}")
    print(f"RC verb names: {len(proc.verb_names)}")
    print()

    # Find implementation file
    analyzer = VerbImplementationAnalyzer([proc])
    impl_file = analyzer.find_implementation_file(processor_name)

    if not impl_file:
        print("ERROR: No implementation file found")
        print()
        return

    print(f"Implementation file: {Path(impl_file).name}")
    print(f"Full path: {impl_file}")
    print()

    # Read source file
    with open(impl_file, 'r', encoding='utf-8', errors='replace') as f:
        source = f.read()

    # Extract enum tokens
    print("-" * 80)
    print("ENUM ANALYSIS")
    print("-" * 80)

    enum_patterns = [
        r'enum\s*\{([^}]+)\}',
        r'typedef\s+enum\s+\w*\s*\{([^}]+)\}',
    ]

    enum_body = None
    for pattern in enum_patterns:
        match = re.search(pattern, source, re.DOTALL)
        if match:
            enum_body = match.group(1)
            print(f"Found enum (pattern: {pattern[:30]}...)")
            break

    if not enum_body:
        print("NO ENUM FOUND")
        print()
    else:
        # Extract all tokens ending with 'func'
        func_tokens = re.findall(r'(\w+func)\s*[,=]', enum_body)
        print(f"Found {len(func_tokens)} tokens ending with 'func'")
        print()

        if func_tokens:
            print("First 10 enum tokens:")
            for i, token in enumerate(func_tokens[:10]):
                # Remove 'func' suffix to get base name
                base_name = token[:-4] if token.endswith('func') else token
                print(f"  {i}: {token:30s} -> {base_name}")
            print()

    # Check for case labels
    print("-" * 80)
    print("CASE LABEL ANALYSIS")
    print("-" * 80)

    # Find switch statement
    switch_match = re.search(r'switch\s*\([^)]+\)\s*\{', source)
    if switch_match:
        print("Found switch statement")

        # Extract case labels (first 10)
        case_pattern = r'case\s+(\w+)\s*:'
        cases = re.findall(case_pattern, source)
        print(f"Found {len(cases)} case labels")
        print()

        if cases:
            print("First 10 case labels:")
            for i, case in enumerate(cases[:10]):
                print(f"  {i}: {case}")
            print()
    else:
        print("NO SWITCH STATEMENT FOUND")
        print()

    # Create mapping table
    print("-" * 80)
    print("VERB NAME MAPPING TABLE")
    print("-" * 80)
    print(f"{'Index':<6} {'RC Verb':<25} {'Enum Token':<30} {'Match?'}")
    print("-" * 80)

    for i, rc_verb in enumerate(proc.verb_names[:15]):  # First 15 verbs
        # Try to find matching enum token
        enum_token = "???"
        match_status = "?"

        if enum_body:
            # Try various transformations
            candidates = [
                f"{processor_name}{rc_verb}func",  # filecreatedfunc
                f"{rc_verb}func",                   # movefunc
                f"{rc_verb.replace('get', '')}func" if rc_verb.startswith('get') else None,  # getlinetext -> linetextfunc
            ]

            for candidate in candidates:
                if candidate and candidate in enum_body:
                    enum_token = candidate
                    match_status = "✓"
                    break

            # If still not found, check if base name is in any token
            if match_status == "?":
                for token in func_tokens[:proc.verb_count] if 'func_tokens' in locals() else []:
                    if rc_verb in token.lower() or token.lower().replace('func', '') in rc_verb:
                        enum_token = f"{token}?"
                        match_status = "~"
                        break

        print(f"{i:<6} {rc_verb:<25} {enum_token:<30} {match_status}")

    if proc.verb_count > 15:
        print(f"... ({proc.verb_count - 15} more verbs)")

    print()
    print("Legend: ✓ = exact match, ~ = partial match, ? = no match found")
    print()


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python3 investigate_processor.py <processor_name>")
        print("\nExamples:")
        print("  python3 investigate_processor.py file")
        print("  python3 investigate_processor.py table")
        print("  python3 investigate_processor.py dialog")
        sys.exit(1)

    investigate_processor(sys.argv[1])
