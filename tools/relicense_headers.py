#!/usr/bin/env python3
"""
relicense_headers.py - Replace GPLv2 file headers with MIT license headers

Sweeps all source files in the project (excluding third_party/, vendored
dependencies, and tooling that is independently licensed) and rewrites GPL
boilerplate at the top of each file with an MIT license header.

Two GPL boilerplate variants are recognized:

  Variant A (long form, most C/C++/headers/.r/.rc files):
      "UserLand Frontier(tm) -- ..." block followed by GNU GPL v2 paragraphs
      and a Boston FSF address line. May be wrapped in a /* ... */ comment
      with leading "$Id$" line.

  Variant B (short form, frontier-cli/* and portable/* files):
      Brief "Copyright (C) 1992-2004 UserLand Software, Inc." followed by
      "This program is free software; ..." paragraph. Embedded in a comment
      block that also contains a file-specific description above the GPL text.

Per the project's RELICENSING.md, the rights holder authorized relicensing
to MIT. This script preserves any file-specific descriptive comments that
appear before the copyright line; only the GPL grant itself is replaced.

Usage:
    python3 tools/relicense_headers.py            # apply changes
    python3 tools/relicense_headers.py --dry-run  # preview changes
    python3 tools/relicense_headers.py --check    # exit 1 if any file still has GPL header
"""

import argparse
import re
import sys
from pathlib import Path
from typing import List, Optional, Tuple

# MIT license body, suitable for embedding in /* ... */ comments.
MIT_LICENSE_BODY = """SPDX-License-Identifier: MIT

Copyright (c) 1992-2004 UserLand Software, Inc.
Copyright (c) 2025-2026 Frontier contributors

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE."""


# Markers that uniquely identify the GPL header region.
GPL_TRIGGER_PHRASES = (
    "GNU General Public License",
    "This program is free software",
)

# Trailing GPL anchors used to find the end of the boilerplate.
GPL_END_PHRASES = (
    "Boston, MA",
    "59 Temple Place",
    "Free Software Foundation, Inc.",
    "(at your option) any later version",
)


# Files / directories that must not be touched.
SKIP_DIRS = (
    "third_party/",
    "planning/",
    "docs/",
    "reports/",
    "node_modules/",
    ".git/",
    ".claude/",
    "dist/",
    "tests/tmp/",
    "frontier-cli/build/",
)

SKIP_FILES = (
    # The MySQL stub file has its own generator-controlled banner; the script
    # generator will inject the MIT header when regenerating, but the existing
    # checked-in file does not contain GPL boilerplate.
    "tests/headless_mysql_verbs.c",
)

# File extensions where C-style /* ... */ comments are valid.
C_STYLE_EXTS = {
    ".c", ".h", ".cpp", ".hpp", ".cc", ".m", ".mm",
    ".r", ".rc", ".y", ".l",  # Rez, Win32 resource, yacc/bison, lex
}


def find_gpl_header_block(text: str) -> Optional[Tuple[int, int]]:
    """Locate the byte range covering the file's GPL header comment.

    Returns (start, end) where text[start:end] is the comment block to
    replace, or None if no GPL header is present.

    Walks forward through the leading C-style comments in the file (within
    the first 16 KB) until it finds one that contains a GPL trigger phrase.
    This handles files that begin with a small "$Id$" comment followed by
    the real GPL banner block.
    """
    head_window = text[:16384]
    cursor = 0
    while cursor < len(head_window):
        open_idx = text.find("/*", cursor)
        if open_idx < 0 or open_idx >= len(head_window):
            return None

        close_idx = text.find("*/", open_idx + 2)
        if close_idx < 0:
            return None
        block_end = close_idx + 2

        block = text[open_idx:block_end]
        if any(phrase in block for phrase in GPL_TRIGGER_PHRASES):
            return (open_idx, block_end)

        # Not a GPL block — skip past it. Only allow whitespace/blank lines
        # between the previous comment and the next one; if we hit real
        # source code first, give up so we never modify code that has no
        # GPL header.
        between = text[block_end:].lstrip()
        if not between.startswith("/*"):
            return None

        # Resume searching from the next "/*".
        cursor = block_end

    return None


def extract_descriptive_preamble(block: str) -> str:
    """Pull out any file-specific descriptive lines that appeared above the
    Copyright line in the original GPL block, so they can be preserved
    above the new MIT header.

    Returns the preamble text (without surrounding /* */), normalized to
    one description paragraph. Empty string if there is nothing to keep.
    """
    # Strip the /* */ wrappers.
    inner = block
    if inner.startswith("/*"):
        inner = inner[2:]
    if inner.endswith("*/"):
        inner = inner[:-2]

    lines = inner.splitlines()
    # Drop a leading "$Id$" line if present.
    while lines and ("$Id$" in lines[0] or lines[0].strip() in ("", "*")):
        lines.pop(0)

    # Find where the GPL section starts (UserLand banner OR Copyright line).
    gpl_start = None
    for i, line in enumerate(lines):
        stripped = line.strip(" *\t")
        if (
            "UserLand Frontier" in stripped
            or stripped.startswith("Copyright (C)")
            or stripped.startswith("Copyright (c)")
            or "This program is free software" in stripped
        ):
            gpl_start = i
            break

    if gpl_start is None or gpl_start == 0:
        return ""

    preamble_lines = lines[:gpl_start]
    # Strip leading "* " comment markers from each preamble line, and filter
    # out decorative banner lines (rows of "*", "=", "-", or "/").
    cleaned: List[str] = []
    for line in preamble_lines:
        s = line.rstrip()
        m = re.match(r"^\s*\*\s?(.*)$", s)
        if m:
            content = m.group(1).rstrip()
        else:
            content = s
        # Banner-only line (all decorative chars) -> drop.
        stripped = content.strip()
        if stripped and set(stripped) <= set("*=-/_ "):
            continue
        cleaned.append(content)

    # Collapse leading/trailing blank lines and runs of blanks.
    while cleaned and cleaned[0].strip() == "":
        cleaned.pop(0)
    while cleaned and cleaned[-1].strip() == "":
        cleaned.pop()

    return "\n".join(cleaned).strip()


def build_mit_header(preamble: str) -> str:
    """Return a /* ... */ comment block containing the MIT license, with an
    optional descriptive preamble preserved above it."""
    body_lines = MIT_LICENSE_BODY.splitlines()
    indented = "\n".join(f"    {line}".rstrip() for line in body_lines)

    if preamble:
        # Indent each preamble line under the same comment style.
        preamble_lines = preamble.splitlines()
        preamble_block = "\n".join(
            f"    {line}".rstrip() for line in preamble_lines
        )
        return f"/*\n{preamble_block}\n\n{indented}\n*/"

    return f"/*\n{indented}\n*/"


def relicense_file(path: Path, text: str, encoding: str, dry_run: bool) -> bool:
    """Replace the GPL header in `path` with an MIT header.

    `text` must be the file's already-loaded contents (the caller has typically
    just read it to detect a GPL block). `encoding` is the encoding the caller
    successfully decoded the file with — we write back using the same encoding
    so we never silently re-encode (e.g. promoting Latin-1 0xA9 © to UTF-8
    0xC2 0xA9, which would shift hardcoded BIGSTRING length bytes).

    Returns True if the file was modified (or would be modified, in dry-run).
    """
    block_range = find_gpl_header_block(text)
    if block_range is None:
        return False

    start, end = block_range
    block = text[start:end]
    preamble = extract_descriptive_preamble(block)
    new_header = build_mit_header(preamble)

    new_text = text[:start] + new_header + text[end:]
    if new_text == text:
        return False

    if not dry_run:
        path.write_text(new_text, encoding=encoding)
    return True


def is_skipped(path: Path, repo_root: Path) -> bool:
    rel = str(path.relative_to(repo_root))
    if rel in SKIP_FILES:
        return True
    for prefix in SKIP_DIRS:
        if rel.startswith(prefix):
            return True
    return False


def collect_candidate_files(repo_root: Path) -> List[Path]:
    candidates: List[Path] = []
    for path in repo_root.rglob("*"):
        if not path.is_file():
            continue
        if path.suffix.lower() not in C_STYLE_EXTS:
            continue
        if is_skipped(path, repo_root):
            continue
        candidates.append(path)
    return candidates


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show which files would change without writing them.",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Exit 1 if any in-scope file still has a GPL header.",
    )
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parent.parent,
        help="Repository root (default: tools/.. relative to this script).",
    )
    args = parser.parse_args()

    repo_root = args.root.resolve()
    candidates = collect_candidate_files(repo_root)

    changed: List[Path] = []
    still_gpl: List[Path] = []
    for path in candidates:
        try:
            text = path.read_text(encoding="utf-8")
            file_encoding = "utf-8"
        except UnicodeDecodeError:
            # Some .r/.h files are Latin-1; fall back permissively so we
            # never silently skip a file that has GPL text. Track the
            # encoding so relicense_file() can write back in the same
            # encoding instead of promoting the file to UTF-8.
            text = path.read_text(encoding="latin-1")
            file_encoding = "latin-1"

        if find_gpl_header_block(text) is None:
            continue

        if args.check:
            still_gpl.append(path)
            continue

        if relicense_file(path, text, file_encoding, dry_run=args.dry_run):
            changed.append(path)

    if args.check:
        if still_gpl:
            print(f"FAIL: {len(still_gpl)} files still have GPL headers:")
            for p in still_gpl:
                print(f"  {p.relative_to(repo_root)}")
            return 1
        print("OK: no GPL headers detected in in-scope files.")
        return 0

    verb = "Would update" if args.dry_run else "Updated"
    print(f"{verb} {len(changed)} file(s).")
    if args.dry_run:
        for p in changed:
            print(f"  {p.relative_to(repo_root)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
