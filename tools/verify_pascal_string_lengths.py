#!/usr/bin/env python3
r"""Verify Pascal string length bytes in headless verb registration files.

Checks all ADD_VERB(BIGSTRING()) macros in tests/headless_*_verbs.c files to
ensure the octal length byte matches the actual string length. Also checks for
broken \p prefixes that don't work in modern compilers.

Usage:
    python3 tools/verify_pascal_string_lengths.py

Exit code 0 if all lengths are correct, 1 if any errors found.
"""

import re
import glob
import sys


def check_file(filepath):
    errors = []
    with open(filepath, 'r') as f:
        for lineno, line in enumerate(f, 1):
            # Check for broken \p prefixes in ADD_VERB lines
            for m in re.finditer(r'ADD_VERB\(BIGSTRING\("\\p([^"]+)"\)', line):
                errors.append((filepath, lineno, 'broken_prefix',
                    f'\\p prefix (not valid in modern C): "\\p{m.group(1)}"'))

            # Check octal length bytes in ADD_VERB lines only
            # (skip non-verb BIGSTRING usage like shell escape sequences)
            for m in re.finditer(r'ADD_VERB\(BIGSTRING\("\\(\d{3})([^"]+)"\)', line):
                octal_str = m.group(1)
                verb_name = m.group(2)
                declared_len = int(octal_str, 8)
                actual_len = len(verb_name)
                if declared_len != actual_len:
                    errors.append((filepath, lineno, 'wrong_length',
                        f'\\{octal_str} ({declared_len}) != "{verb_name}" ({actual_len})'))
    return errors


def main():
    files = sorted(glob.glob('tests/headless_*_verbs.c'))
    if not files:
        print("ERROR: No headless verb files found. Run from repository root.", file=sys.stderr)
        return 1

    all_errors = []
    total_checked = 0

    for filepath in files:
        with open(filepath, 'r') as f:
            content = f.read()
        count = len(re.findall(r'ADD_VERB\(BIGSTRING\("\\', content))
        total_checked += count
        errors = check_file(filepath)
        all_errors.extend(errors)

    if all_errors:
        print(f"FAILED: {len(all_errors)} error(s) found in {len(files)} files:")
        for filepath, lineno, kind, msg in all_errors:
            print(f"  {filepath}:{lineno}: [{kind}] {msg}")
        return 1
    else:
        print(f"OK: All {total_checked} Pascal string lengths correct across {len(files)} files.")
        return 0


if __name__ == '__main__':
    sys.exit(main())
