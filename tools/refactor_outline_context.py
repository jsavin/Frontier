#!/usr/bin/env python3
"""
Automated refactoring script for ADR-006 Phase 3: Outline Context Accessor Migration

This script migrates outline context macro calls to thread-safe accessor functions:
  - outlinedata → op_get_outlinedata() / op_set_outlinedata()
  - topoutlinestack → op_get_topoutlinestack() / op_set_topoutlinestack()
  - outlinestack[N] → op_get_outlinestack(N) / op_set_outlinestack(N, value)

Usage:
  # Dry run (show changes without applying):
  python3 tools/refactor_outline_context.py --dry-run

  # Apply changes:
  python3 tools/refactor_outline_context.py --apply

  # Process specific files:
  python3 tools/refactor_outline_context.py --dry-run Common/source/op.c

  # Show statistics:
  python3 tools/refactor_outline_context.py --stats
"""

import argparse
import re
import sys
from pathlib import Path
from typing import List, Tuple, Dict
from dataclasses import dataclass


@dataclass
class Replacement:
    """Represents a single replacement operation."""
    file_path: Path
    line_num: int
    original: str
    replacement: str
    context: str  # Surrounding lines for review


class OutlineContextRefactorer:
    """Refactors outline context macros to accessor functions."""

    def __init__(self, dry_run: bool = True):
        self.dry_run = dry_run
        self.replacements: List[Replacement] = []
        self.stats = {
            'files_processed': 0,
            'files_modified': 0,
            'outlinedata_reads': 0,
            'outlinedata_writes': 0,
            'topoutlinestack_reads': 0,
            'topoutlinestack_writes': 0,
            'outlinestack_reads': 0,
            'outlinestack_writes': 0,
            'address_taking': 0,
        }

    def find_source_files(self, paths: List[str] = None) -> List[Path]:
        """Find all C source files to process."""
        if paths:
            # Process specified files
            return [Path(p) for p in paths if Path(p).exists()]

        # Process all C source files (excluding certain directories)
        base = Path('.')
        exclude_dirs = {'.git', 'build', 'third_party', 'generated', 'build_Xcode_modern'}

        files = []
        for pattern in ['**/*.c', '**/*.h']:
            for file in base.glob(pattern):
                # Skip if file is in excluded directory
                if any(excluded in file.parts for excluded in exclude_dirs):
                    continue
                files.append(file)

        return sorted(files)

    def is_write_context(self, line: str, macro_name: str) -> bool:
        """Determine if macro usage is in a write context (assignment)."""
        # Find the macro in the line
        macro_match = re.search(rf'\b{re.escape(macro_name)}\b', line)
        if not macro_match:
            return False

        macro_pos = macro_match.end()
        after_macro = line[macro_pos:].lstrip()

        # Check for direct assignment: outlinedata = ...
        if after_macro.startswith('=') and not after_macro.startswith('=='):
            return True

        # Check for compound assignment: outlinedata += ...
        if re.match(r'^[+\-*/%&|^]=', after_macro):
            return True

        # Check for increment/decrement: outlinedata++, ++outlinedata
        before_macro = line[:macro_match.start()].rstrip()
        if after_macro.startswith('++') or after_macro.startswith('--'):
            return True
        if before_macro.endswith('++') or before_macro.endswith('--'):
            return True

        return False

    def is_address_taking(self, line: str, macro_name: str) -> bool:
        """Detect if we're taking the address of the macro (&outlinedata)."""
        pattern = rf'&\s*{re.escape(macro_name)}\b'
        return bool(re.search(pattern, line))

    def refactor_outlinedata(self, line: str, line_num: int, file_path: Path) -> str:
        """Refactor outlinedata macro usage."""
        original_line = line

        # Handle address-taking (&outlinedata) - requires unsafe accessor
        if self.is_address_taking(line, 'outlinedata'):
            self.stats['address_taking'] += 1
            line = re.sub(
                r'&\s*outlinedata\b',
                'op_get_outlinedata_ptr_unsafe()',
                line
            )
            if line != original_line:
                self.replacements.append(Replacement(
                    file_path, line_num, original_line.rstrip(), line.rstrip(),
                    f"Address-taking detected (unsafe accessor required)"
                ))
            return line

        # Handle assignment (write): outlinedata = value
        if self.is_write_context(line, 'outlinedata'):
            self.stats['outlinedata_writes'] += 1
            # Replace: outlinedata = value  →  op_set_outlinedata(value)
            line = re.sub(
                r'\boutlinedata\s*=\s*([^;]+)',
                r'op_set_outlinedata(\1)',
                line
            )
        else:
            # Handle read context: value = outlinedata
            if 'outlinedata' in line:
                self.stats['outlinedata_reads'] += 1
                # Replace: outlinedata  →  op_get_outlinedata()
                line = re.sub(
                    r'\boutlinedata\b',
                    'op_get_outlinedata()',
                    line
                )

        if line != original_line:
            self.replacements.append(Replacement(
                file_path, line_num, original_line.rstrip(), line.rstrip(),
                f"outlinedata refactored"
            ))

        return line

    def refactor_topoutlinestack(self, line: str, line_num: int, file_path: Path) -> str:
        """Refactor topoutlinestack macro usage."""
        original_line = line

        # Handle assignment (write): topoutlinestack = value
        if self.is_write_context(line, 'topoutlinestack'):
            self.stats['topoutlinestack_writes'] += 1
            # Replace: topoutlinestack = value  →  op_set_topoutlinestack(value)
            line = re.sub(
                r'\btopoutlinestack\s*=\s*([^;]+)',
                r'op_set_topoutlinestack(\1)',
                line
            )
        else:
            # Handle read context: value = topoutlinestack
            if 'topoutlinestack' in line:
                self.stats['topoutlinestack_reads'] += 1
                # Replace: topoutlinestack  →  op_get_topoutlinestack()
                line = re.sub(
                    r'\btopoutlinestack\b',
                    'op_get_topoutlinestack()',
                    line
                )

        if line != original_line:
            self.replacements.append(Replacement(
                file_path, line_num, original_line.rstrip(), line.rstrip(),
                f"topoutlinestack refactored"
            ))

        return line

    def refactor_outlinestack(self, line: str, line_num: int, file_path: Path) -> str:
        """Refactor outlinestack[N] array access."""
        original_line = line

        # Handle assignment (write): outlinestack[N] = value
        write_match = re.search(r'\boutlinestack\s*\[\s*([^\]]+)\s*\]\s*=\s*([^;]+)', line)
        if write_match:
            self.stats['outlinestack_writes'] += 1
            index = write_match.group(1)
            value = write_match.group(2)
            # Replace: outlinestack[N] = value  →  op_set_outlinestack(N, value)
            line = re.sub(
                r'\boutlinestack\s*\[\s*' + re.escape(index) + r'\s*\]\s*=\s*' + re.escape(value),
                f'op_set_outlinestack({index}, {value})',
                line
            )
        else:
            # Handle read context: value = outlinestack[N]
            read_match = re.search(r'\boutlinestack\s*\[\s*([^\]]+)\s*\]', line)
            if read_match:
                self.stats['outlinestack_reads'] += 1
                index = read_match.group(1)
                # Replace: outlinestack[N]  →  op_get_outlinestack(N)
                line = re.sub(
                    r'\boutlinestack\s*\[\s*' + re.escape(index) + r'\s*\]',
                    f'op_get_outlinestack({index})',
                    line
                )

        if line != original_line:
            self.replacements.append(Replacement(
                file_path, line_num, original_line.rstrip(), line.rstrip(),
                f"outlinestack array access refactored"
            ))

        return line

    def should_skip_line(self, line: str, file_path: Path, line_num: int) -> bool:
        """Determine if a line should be skipped (comments, strings, struct defs)."""
        stripped = line.strip()

        # Skip comments (// and /* ... */)
        if stripped.startswith('//') or stripped.startswith('/*') or stripped.startswith('*'):
            return True

        # Skip struct member definitions (looking for pattern: type membername;)
        if re.match(r'^\s*(short|hdloutlinerecord)\s+(outlinedata|topoutlinestack|outlinestack)\s*[;\[]', stripped):
            return True

        # Skip typedef definitions
        if 'typedef' in line:
            return True

        # Skip preprocessor directives (#define, #ifdef, etc.)
        if stripped.startswith('#'):
            return True

        # Skip string literals containing the macro names
        # (This is a simplified check - a full parser would be better)
        if '"' in line:
            # Check if macro appears inside quotes
            in_string = False
            for i, char in enumerate(line):
                if char == '"' and (i == 0 or line[i-1] != '\\'):
                    in_string = not in_string
                if in_string and any(macro in line[i:] for macro in ['outlinedata', 'topoutlinestack', 'outlinestack']):
                    return True

        return False

    def process_file(self, file_path: Path) -> bool:
        """Process a single file. Returns True if file was modified."""
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                lines = f.readlines()
        except Exception as e:
            print(f"Error reading {file_path}: {e}", file=sys.stderr)
            return False

        modified = False
        new_lines = []

        for line_num, line in enumerate(lines, start=1):
            original_line = line

            # Skip if line is in processinternal.h macro definitions (lines 367-376)
            if file_path.name == 'processinternal.h' and 367 <= line_num <= 376:
                new_lines.append(line)
                continue

            # Skip if line is in accessor function definitions (lines 297-360)
            if file_path.name == 'processinternal.h' and 297 <= line_num <= 360:
                new_lines.append(line)
                continue

            # Skip if line is in tythreadglobals struct definition (lines 145-149)
            if file_path.name == 'processinternal.h' and 145 <= line_num <= 149:
                new_lines.append(line)
                continue

            # Skip comments, struct definitions, strings, etc.
            if self.should_skip_line(line, file_path, line_num):
                new_lines.append(line)
                continue

            # Apply refactorings in order
            if 'outlinedata' in line:
                line = self.refactor_outlinedata(line, line_num, file_path)

            if 'topoutlinestack' in line:
                line = self.refactor_topoutlinestack(line, line_num, file_path)

            if 'outlinestack' in line:
                line = self.refactor_outlinestack(line, line_num, file_path)

            new_lines.append(line)

            if line != original_line:
                modified = True

        # Write changes if not dry run
        if modified and not self.dry_run:
            try:
                with open(file_path, 'w', encoding='utf-8') as f:
                    f.writelines(new_lines)
            except Exception as e:
                print(f"Error writing {file_path}: {e}", file=sys.stderr)
                return False

        return modified

    def run(self, file_paths: List[str] = None):
        """Run the refactoring process."""
        files = self.find_source_files(file_paths)

        print(f"{'DRY RUN - ' if self.dry_run else ''}Processing {len(files)} files...\n")

        for file_path in files:
            self.stats['files_processed'] += 1

            if self.process_file(file_path):
                self.stats['files_modified'] += 1
                print(f"{'Would modify' if self.dry_run else 'Modified'}: {file_path}")

        print(f"\n{'='*70}")
        print(f"{'DRY RUN ' if self.dry_run else ''}SUMMARY")
        print(f"{'='*70}")
        print(f"Files processed: {self.stats['files_processed']}")
        print(f"Files modified: {self.stats['files_modified']}")
        print(f"\nRefactorings:")
        print(f"  outlinedata reads:      {self.stats['outlinedata_reads']}")
        print(f"  outlinedata writes:     {self.stats['outlinedata_writes']}")
        print(f"  topoutlinestack reads:  {self.stats['topoutlinestack_reads']}")
        print(f"  topoutlinestack writes: {self.stats['topoutlinestack_writes']}")
        print(f"  outlinestack reads:     {self.stats['outlinestack_reads']}")
        print(f"  outlinestack writes:    {self.stats['outlinestack_writes']}")
        print(f"  address-taking (unsafe): {self.stats['address_taking']}")
        print(f"\nTotal replacements: {len(self.replacements)}")

        if self.dry_run and self.replacements:
            print(f"\n{'='*70}")
            print(f"SAMPLE CHANGES (first 20):")
            print(f"{'='*70}")
            for i, repl in enumerate(self.replacements[:20], 1):
                print(f"\n{i}. {repl.file_path}:{repl.line_num}")
                print(f"   - {repl.original}")
                print(f"   + {repl.replacement}")

    def print_stats(self):
        """Print statistics only."""
        files = self.find_source_files()

        # Quick scan for occurrences
        outlinedata_count = 0
        topoutlinestack_count = 0
        outlinestack_count = 0

        for file_path in files:
            try:
                with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()
                    outlinedata_count += len(re.findall(r'\boutlinedata\b', content))
                    topoutlinestack_count += len(re.findall(r'\btopoutlinestack\b', content))
                    outlinestack_count += len(re.findall(r'\boutlinestack\[', content))
            except:
                pass

        print(f"{'='*70}")
        print(f"OUTLINE CONTEXT USAGE STATISTICS")
        print(f"{'='*70}")
        print(f"Total source files: {len(files)}")
        print(f"\nMacro occurrences:")
        print(f"  outlinedata:     {outlinedata_count}")
        print(f"  topoutlinestack: {topoutlinestack_count}")
        print(f"  outlinestack[]:  {outlinestack_count}")
        print(f"\nTotal:             {outlinedata_count + topoutlinestack_count + outlinestack_count}")


def main():
    parser = argparse.ArgumentParser(
        description='Refactor outline context macros to accessor functions (ADR-006 Phase 3)'
    )
    parser.add_argument(
        '--dry-run',
        action='store_true',
        help='Show what would be changed without modifying files (default)'
    )
    parser.add_argument(
        '--apply',
        action='store_true',
        help='Apply changes to files (WARNING: modifies source code)'
    )
    parser.add_argument(
        '--stats',
        action='store_true',
        help='Show usage statistics only'
    )
    parser.add_argument(
        'files',
        nargs='*',
        help='Specific files to process (default: all C/H files)'
    )

    args = parser.parse_args()

    # Default to dry-run unless --apply is specified
    dry_run = not args.apply

    refactorer = OutlineContextRefactorer(dry_run=dry_run)

    if args.stats:
        refactorer.print_stats()
    else:
        refactorer.run(args.files if args.files else None)

        if dry_run:
            print(f"\n{'='*70}")
            print(f"This was a DRY RUN. No files were modified.")
            print(f"To apply changes, run with --apply flag:")
            print(f"  python3 tools/refactor_outline_context.py --apply")
            print(f"{'='*70}")


if __name__ == '__main__':
    main()
