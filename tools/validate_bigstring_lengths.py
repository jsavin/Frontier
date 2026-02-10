#!/usr/bin/env python3
r"""
Validate BIGSTRING length prefixes in Frontier C/H source files.

BIGSTRING("\NNNstring") creates a Pascal string where the first byte
is the length prefix. This script verifies that the octal or hex
escape prefix matches the actual string length.

Patterns handled:
  - Octal prefix:  BIGSTRING("\015innercasename")     -- \015 = 13, string is 13 chars
  - Hex prefix:    BIGSTRING("\x1e" "Can't do ...")    -- \x1e = 30, string is 30 chars
  - Concatenated:  BIGSTRING("\x08" "flindent")        -- \x08 = 8, string is 8 chars
  - Empty string:  BIGSTRING("\x00")                   -- \x00 = 0, empty string

Patterns skipped (correct by construction):
  - Pascal prefix: BIGSTRING("\psome string")          -- compiler handles \p automatically
  - No prefix:     BIGSTRING("literal")                -- no length byte to validate

Exit codes:
  0 - All BIGSTRING length prefixes are valid
  1 - One or more mismatches found
  2 - Script error (bad arguments, etc.)
"""

import os
import re
import sys
import argparse


# Match BIGSTRING("...") including possible string concatenation.
# We capture the full content inside the parentheses.
# This handles both single-string and concatenated forms:
#   BIGSTRING("\015innercasename")
#   BIGSTRING("\x1e" "Can't do fileloop: bad path")
BIGSTRING_PATTERN = re.compile(
    r'BIGSTRING\(\s*("(?:[^"\\]|\\.)*"(?:\s*"(?:[^"\\]|\\.)*")*)\s*\)'
)


def parse_escape_prefix(s):
    """Parse the leading escape sequence from a C string literal content.

    Given the content inside quotes (after the opening quote), determine
    if it starts with an octal or hex escape that serves as a length prefix.

    Returns:
        (prefix_value, rest_of_string) if a length prefix is found
        None if no length prefix (e.g., \\p prefix or no escape)
    """
    if not s:
        return None

    if s[0] != '\\':
        return None

    if len(s) < 2:
        return None

    # Skip \p (Pascal string - compiler handles it)
    if s[1] == 'p':
        return None

    # Hex escape: \xNN
    if s[1] == 'x':
        hex_match = re.match(r'\\x([0-9a-fA-F]{1,2})', s)
        if hex_match:
            value = int(hex_match.group(1), 16)
            rest = s[len(hex_match.group(0)):]
            return (value, rest)
        return None

    # Octal escape: \N, \NN, or \NNN (1-3 octal digits)
    oct_match = re.match(r'\\([0-7]{1,3})', s)
    if oct_match:
        value = int(oct_match.group(1), 8)
        rest = s[len(oct_match.group(0)):]
        return (value, rest)

    return None


def combine_string_literals(raw_content):
    """Combine C string literal concatenation into a single content string.

    Input: '"\\x1e" "Can\\'t do fileloop: bad path"'
    Output: '\\x1eCan\\'t do fileloop: bad path'

    We extract the content of each quoted segment and concatenate them.
    """
    parts = []
    # Find each quoted string segment
    for m in re.finditer(r'"((?:[^"\\]|\\.)*)"', raw_content):
        parts.append(m.group(1))
    return ''.join(parts)


def compute_string_length(rest):
    """Compute the actual length of the string portion after the prefix.

    The string may contain escape sequences that represent single characters.
    In practice, the strings after the length prefix in BIGSTRING are plain
    ASCII text, but we handle common escapes for correctness.
    """
    length = 0
    i = 0
    while i < len(rest):
        if rest[i] == '\\':
            if i + 1 < len(rest):
                next_char = rest[i + 1]
                if next_char == 'x':
                    # Hex escape: \xNN - consumes up to 2 hex digits
                    hex_match = re.match(r'\\x[0-9a-fA-F]{1,2}', rest[i:])
                    if hex_match:
                        i += len(hex_match.group(0))
                        length += 1
                        continue
                elif next_char in '01234567':
                    # Octal escape: \NNN - consumes up to 3 octal digits
                    oct_match = re.match(r'\\[0-7]{1,3}', rest[i:])
                    if oct_match:
                        i += len(oct_match.group(0))
                        length += 1
                        continue
                else:
                    # Simple escape: \n, \t, \\, \", etc. = 1 character
                    i += 2
                    length += 1
                    continue
            # Trailing backslash (shouldn't happen in valid C)
            i += 1
            length += 1
        else:
            i += 1
            length += 1
    return length


def validate_file(filepath, verbose=False):
    """Validate all BIGSTRING occurrences in a single file.

    Returns a list of (filepath, line_number, message) tuples for mismatches.
    """
    errors = []
    checked = 0

    try:
        with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
            content = f.read()
    except (IOError, OSError) as e:
        return errors, 0, [(filepath, 0, f"Could not read file: {e}")]

    # Process line by line for accurate line numbers, but handle multi-line
    # by working on the full content and mapping positions to lines
    line_offsets = [0]
    for i, ch in enumerate(content):
        if ch == '\n':
            line_offsets.append(i + 1)

    def offset_to_line(offset):
        """Convert a character offset to a 1-based line number."""
        lo, hi = 0, len(line_offsets) - 1
        while lo < hi:
            mid = (lo + hi + 1) // 2
            if line_offsets[mid] <= offset:
                lo = mid
            else:
                hi = mid - 1
        return lo + 1  # 1-based

    for match in BIGSTRING_PATTERN.finditer(content):
        raw = match.group(1)
        line_num = offset_to_line(match.start())

        # Combine any concatenated string literals
        combined = combine_string_literals(raw)

        # Try to parse a length prefix
        result = parse_escape_prefix(combined)
        if result is None:
            # No length prefix to validate (e.g., \p or no escape)
            if verbose:
                print(f"  SKIP {filepath}:{line_num} - no explicit length prefix")
            continue

        prefix_value, rest = result
        actual_length = compute_string_length(rest)
        checked += 1

        if prefix_value != actual_length:
            errors.append((
                filepath,
                line_num,
                f"Length mismatch: prefix={prefix_value} (0x{prefix_value:02x}), "
                f"actual string length={actual_length}, "
                f"string content: \"{rest[:50]}{'...' if len(rest) > 50 else ''}\""
            ))
        elif verbose:
            print(f"  OK   {filepath}:{line_num} - prefix={prefix_value}, len={actual_length}")

    return errors, checked, []


def find_source_files(root_dir, extensions=('.c', '.h')):
    """Find all C/H source files under root_dir, skipping common non-source dirs."""
    skip_dirs = {'.git', 'build', 'build-headless', 'build-headless-debug',
                 'cmake-install', 'node_modules', '__pycache__', 'dist'}
    source_files = []

    for dirpath, dirnames, filenames in os.walk(root_dir):
        # Skip non-source directories
        dirnames[:] = [d for d in dirnames if d not in skip_dirs]

        for filename in filenames:
            if any(filename.endswith(ext) for ext in extensions):
                source_files.append(os.path.join(dirpath, filename))

    return sorted(source_files)


def main():
    parser = argparse.ArgumentParser(
        description='Validate BIGSTRING length prefixes in C/H source files.'
    )
    parser.add_argument(
        'root_dir',
        nargs='?',
        default=None,
        help='Root directory to scan (default: repository root)'
    )
    parser.add_argument(
        '-v', '--verbose',
        action='store_true',
        help='Show all checked strings, not just errors'
    )
    parser.add_argument(
        '--files-only',
        action='store_true',
        help='Only scan .c and .h files (default behavior)'
    )

    args = parser.parse_args()

    # Determine root directory
    if args.root_dir:
        root_dir = os.path.abspath(args.root_dir)
    else:
        # Default to repository root (parent of tools/)
        script_dir = os.path.dirname(os.path.abspath(__file__))
        root_dir = os.path.dirname(script_dir)

    if not os.path.isdir(root_dir):
        print(f"Error: {root_dir} is not a directory", file=sys.stderr)
        sys.exit(2)

    print(f"Scanning for BIGSTRING length mismatches in: {root_dir}")
    print()

    source_files = find_source_files(root_dir)
    total_errors = []
    total_warnings = []
    total_checked = 0
    files_with_bigstrings = 0

    for filepath in source_files:
        errors, checked, warnings = validate_file(filepath, verbose=args.verbose)
        if checked > 0 or errors:
            files_with_bigstrings += 1
        total_checked += checked
        total_errors.extend(errors)
        total_warnings.extend(warnings)

    # Report warnings
    for filepath, line, msg in total_warnings:
        print(f"WARNING: {filepath}:{line}: {msg}")

    # Report errors
    if total_errors:
        print("ERRORS FOUND:")
        print()
        for filepath, line, msg in total_errors:
            print(f"  {filepath}:{line}: {msg}")
        print()
        print(f"Summary: {len(total_errors)} mismatch(es) found in {total_checked} "
              f"BIGSTRING(s) across {files_with_bigstrings} file(s)")
        sys.exit(1)
    else:
        print(f"All {total_checked} BIGSTRING length prefix(es) validated successfully "
              f"across {files_with_bigstrings} file(s).")
        sys.exit(0)


if __name__ == '__main__':
    main()
