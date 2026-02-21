#!/usr/bin/env python3
r"""Verify Pascal string length bytes in headless verb registration files.

Checks all PSTRING() and BIGSTRING() macros in tests/headless_*_verbs.c files to
ensure the octal length byte matches the actual string length. Also checks for
broken \p prefixes that don't work in modern compilers.

Usage:
    python3 tools/verify_pascal_string_lengths.py

Exit code 0 if all lengths are correct, 1 if any errors found.
"""

import re
import glob
import sys


# Matches a single C character (escape sequence or literal) in a string
_C_CHAR_RE = re.compile(r'\\(?:[abfnrtv\\\'"?]|[0-7]{1,3}|x[0-9a-fA-F]+)|.')


def c_strlen(s):
    """Compute the length of a C string literal (handling escape sequences)."""
    return len(_C_CHAR_RE.findall(s))


def check_file(filepath):
    errors = []
    with open(filepath, 'r') as f:
        for lineno, line in enumerate(f, 1):
            # Check for broken \p prefixes
            for m in re.finditer(r'(?:ADD_VERB|copystring)\(BIGSTRING\("\\p([^"]+)"\)', line):
                errors.append((filepath, lineno, 'broken_prefix',
                    f'\\p prefix (not valid in modern C): "\\p{m.group(1)}"'))

            # Check PSTRING("\NNN", "name") format (new validated format)
            for m in re.finditer(r'PSTRING\("\\([0-7]{3})",\s*"([^"]*)"\)', line):
                octal_str = m.group(1)
                verb_name = m.group(2)
                declared_len = int(octal_str, 8)
                actual_len = c_strlen(verb_name)
                if declared_len != actual_len:
                    errors.append((filepath, lineno, 'wrong_length',
                        f'PSTRING \\{octal_str} ({declared_len}) != "{verb_name}" ({actual_len})'))

            # Check legacy BIGSTRING("\NNNname") format
            for m in re.finditer(r'BIGSTRING\("\\([0-7]{3})([^"]+)"\)', line):
                octal_str = m.group(1)
                verb_name = m.group(2)
                declared_len = int(octal_str, 8)
                actual_len = c_strlen(verb_name)
                if declared_len != actual_len:
                    errors.append((filepath, lineno, 'wrong_length',
                        f'BIGSTRING \\{octal_str} ({declared_len}) != "{verb_name}" ({actual_len})'))
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
        # Count both PSTR and BIGSTRING with octal prefixes
        pstr_count = len(re.findall(r'PSTRING\("\\[0-7]{3}",\s*"', content))
        bigstring_count = len(re.findall(r'BIGSTRING\("\\[0-7]{3}', content))
        total_checked += pstr_count + bigstring_count
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
