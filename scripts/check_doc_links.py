#!/usr/bin/env python3
"""
check_doc_links.py - Verify local links and references in Markdown docs.

- Scans Markdown files under specified roots (default: planning, tests, frontier-cli)
- Validates relative Markdown/image links [text](path[#anchor]) exist
- Validates inline backticked paths `path/to/file.md` exist (best-effort)
- Optionally validates anchors (#section) against headings in the target doc
- Skips external links (http/https/mailto) and codex_sessions/, build_* dirs
- Suggests replacements for known moved docs

Exit code is nonzero if any errors are found.

Usage:
  python3 scripts/check_doc_links.py [--roots planning tests frontier-cli] [--no-anchors]
"""
from __future__ import annotations

import argparse
import os
import re
import sys
from typing import Dict, List, Optional, Set, Tuple


DEFAULT_ROOTS = ["planning", "tests", "frontier-cli"]
SKIP_DIR_PREFIXES = {".git", "build_", "codex_sessions"}

# Known moves to suggest replacements
MOVED_MAP: Dict[str, str] = {
    "planning/0.5.24_carbon_dependency_audit.md": "planning/ui_abstraction/phase2/carbon_dependency_audit.md",
    "planning/0.5.22_quicktime_retirement.md": "planning/ui_abstraction/phase2/quicktime_retirement.md",
    "planning/0.5.26_langsystem7_headless_refactor.md": "planning/ui_abstraction/phase2/langsystem7_headless_refactor.md",
}


MD_LINK_RE = re.compile(r"\[(?P<text>[^\]]+)\]\((?P<href>[^)]+)\)")
CODE_RE = re.compile(r"`([^`]+)`")


def is_external(href: str) -> bool:
    href = href.strip()
    return href.startswith(("http://", "https://", "mailto:"))


def should_skip_dir(dirpath: str) -> bool:
    base = os.path.basename(dirpath)
    if base in SKIP_DIR_PREFIXES:
        return True
    for p in SKIP_DIR_PREFIXES:
        if base.startswith(p):
            return True
    return False


def slugify_anchor(text: str) -> str:
    # GitHub-like slug: lowercase, keep alnum and spaces/hyphens, collapse spaces to '-'
    s = text.strip().lower()
    # remove markdown inline code backticks
    s = s.replace("`", "")
    # remove punctuation except hyphen and space
    s = re.sub(r"[^a-z0-9\-\s]", "", s)
    s = re.sub(r"\s+", "-", s)
    s = re.sub(r"-+", "-", s).strip("-")
    return s


def collect_anchors(md_path: str) -> Set[str]:
    anchors: Set[str] = set()
    try:
        with open(md_path, "r", errors="ignore") as fh:
            for line in fh:
                if line.lstrip().startswith("#"):
                    # ATX-style heading
                    # Remove leading #'s and one space
                    txt = line.lstrip("#").strip()
                    if txt:
                        anchors.add(slugify_anchor(txt))
    except Exception:
        pass
    return anchors


def parse_args() -> argparse.Namespace:
    ap = argparse.ArgumentParser()
    ap.add_argument("--roots", nargs="*", default=DEFAULT_ROOTS, help="Directories to scan")
    ap.add_argument("--no-anchors", action="store_true", help="Do not validate #anchors")
    return ap.parse_args()


def build_file_index() -> Set[str]:
    files: Set[str] = set()
    for root, dirs, filenames in os.walk("."):
        # Skip unwanted dirs
        if should_skip_dir(root):
            dirs[:] = []
            continue
        for d in list(dirs):
            if should_skip_dir(os.path.join(root, d)):
                dirs.remove(d)
        for fn in filenames:
            files.add(os.path.normpath(os.path.join(root, fn)).lstrip("./"))
    return files


def check_file(md_path: str, all_files: Set[str], validate_anchors: bool) -> List[str]:
    issues: List[str] = []
    try:
        with open(md_path, "r", errors="ignore") as fh:
            content = fh.read()
    except Exception as e:
        return [f"{md_path}:0: error: cannot read file: {e}"]

    # Check Markdown links
    for m in MD_LINK_RE.finditer(content):
        href = m.group("href").strip()
        # Strip title part "path" "title"
        if "\"" in href:
            href = href.split("\"")[0].strip()
        if is_external(href):
            continue
        if href.startswith("#"):
            # anchor within same file
            if validate_anchors:
                anchor = href[1:]
                target_anchors = collect_anchors(md_path)
                if slugify_anchor(anchor) not in target_anchors:
                    issues.append(f"{md_path}: link to missing anchor #{anchor}")
            continue
        # Remove optional angle brackets <path>
        href = href.strip("<>")
        path, anchor = (href.split("#", 1) + [None])[:2]
        norm = os.path.normpath(os.path.join(os.path.dirname(md_path), path))
        rel = norm.lstrip("./")
        if rel not in all_files:
            suggestion = MOVED_MAP.get(rel)
            if suggestion:
                issues.append(f"{md_path}: missing {path} (moved → {suggestion})")
            else:
                issues.append(f"{md_path}: missing {path}")
            continue
        if validate_anchors and anchor:
            target_anchors = collect_anchors(rel)
            if slugify_anchor(anchor) not in target_anchors:
                issues.append(f"{md_path}: {path} missing anchor #{anchor}")

    # Check inline backticked paths (best-effort)
    for m in CODE_RE.finditer(content):
        code = m.group(1).strip()
        if "/" not in code:
            continue
        # Skip code blocks that look like commands (start with $, ./, make, etc.)
        if code.startswith(("$ ", "./", "make ", "python", "bash")):
            continue
        # Verify if it looks like a file path relative to repo root
        candidate = code.strip("<>")
        if candidate.startswith("/"):
            continue
        rel = os.path.normpath(candidate)
        if not os.path.exists(rel):
            suggestion = MOVED_MAP.get(rel)
            if suggestion:
                issues.append(f"{md_path}: backticked path missing {code} (moved → {suggestion})")
            else:
                # Only warn for likely docs (markdown/images)
                if any(rel.endswith(ext) for ext in (".md", ".png", ".jpg", ".jpeg", ".gif", ".svg")):
                    issues.append(f"{md_path}: backticked path missing {code}")

    return issues


def main() -> int:
    args = parse_args()
    roots = args.roots or DEFAULT_ROOTS
    validate_anchors = not args.no_anchors

    # Build index of files for existence checks
    all_files = build_file_index()

    issues: List[str] = []
    scanned: List[str] = []

    for root in roots:
        if not os.path.isdir(root):
            continue
        for dirpath, dirs, filenames in os.walk(root):
            if should_skip_dir(dirpath):
                dirs[:] = []
                continue
            for d in list(dirs):
                if should_skip_dir(os.path.join(dirpath, d)):
                    dirs.remove(d)
            for fn in filenames:
                if fn.lower().endswith(".md"):
                    md_path = os.path.join(dirpath, fn)
                    scanned.append(md_path)
                    issues.extend(check_file(md_path, all_files, validate_anchors))

    if issues:
        print("Doc link check found issues:")
        for i in issues:
            print("-", i)
        print(f"\nScanned {len(scanned)} Markdown files under: {', '.join(roots)}")
        return 1
    else:
        print(f"All links OK. Scanned {len(scanned)} Markdown files under: {', '.join(roots)}")
        return 0


if __name__ == "__main__":
    sys.exit(main())
