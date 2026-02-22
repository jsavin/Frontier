#!/usr/bin/env python3
r"""Migrate BIGSTRING("\NNNfoo") → PSTRING("\NNN", "foo") in headless verb files.

Only converts BIGSTRING calls that have a 3-digit octal length prefix (e.g.,
\006). Calls using fewer octal digits (e.g., \6) are not matched. Other
BIGSTRING usage without octal prefixes is left unchanged.

Scope: Intentionally targets only tests/headless_*_verbs.c files. BIGSTRING
calls in Common/ source files use the legacy format and are not migrated —
those files are shared with the classic Mac build which does not include the
PSTRING macro.

Usage:
    python3 tools/migrate_bigstring_to_pstr.py [--dry-run]
"""

import re
import glob
import sys

# Match BIGSTRING("\NNN...") where NNN is a 3-digit octal prefix
BIGSTRING_PATTERN = re.compile(r'BIGSTRING\("\\([0-7]{3})([^"]*)"\)')


def migrate_file(filepath, dry_run=False):
    with open(filepath, 'r', encoding='utf-8') as f:
        original = f.read()

    migrated = BIGSTRING_PATTERN.sub(r'PSTRING("\\\1", "\2")', original)

    if migrated == original:
        return 0

    count = len(BIGSTRING_PATTERN.findall(original))

    if not dry_run:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(migrated)

    return count


def main():
    dry_run = '--dry-run' in sys.argv

    files = sorted(glob.glob('tests/headless_*_verbs.c'))
    if not files:
        print("ERROR: No headless verb files found. Run from repository root.",
              file=sys.stderr)
        return 1

    total = 0
    changed_files = 0

    for filepath in files:
        count = migrate_file(filepath, dry_run)
        if count > 0:
            changed_files += 1
            total += count
            action = "Would migrate" if dry_run else "Migrated"
            print(f"  {action} {count} calls in {filepath}")

    mode = " (dry run)" if dry_run else ""
    print(f"\n{total} BIGSTRING -> PSTRING conversions across {changed_files} files{mode}")
    return 0


if __name__ == '__main__':
    sys.exit(main())
