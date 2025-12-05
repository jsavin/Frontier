#!/usr/bin/env python3
"""
analyze_verbs.py - Analyze verb implementations across the codebase

Categorizes verbs by implementation status:
- Fully Implemented: Has real implementation
- Missing: Returns false/"not implemented"
- Platform-Specific: Has #ifdef guards
- GUI-Dependent: Uses FRONTIER_HEADLESS guards
- Unimplemented Stubs: New processors, all verbs return false

This script now uses programmatic extraction from kernelverbs.rc
instead of hardcoded verb mappings.
"""

import sys
import re
from pathlib import Path
from typing import Dict, List, Set, Tuple
from collections import defaultdict

# Import from kernelverbs_parser
sys.path.insert(0, str(Path(__file__).parent.parent / 'kernelverbs_parser'))
from parse_kernelverbs import parse_kernelverbs_rc, EFPProcessor
from generate_processor_stubs import extract_verb_names


def validate_repo_root(repo_root: Path) -> bool:
    """
    Validate that repo_root is actually the Frontier repository root.

    Checks for presence of key files/directories that should exist in the repo.
    Returns True if valid, False otherwise.
    """
    required_paths = [
        repo_root / "Common/resources/Win32/kernelverbs.rc",
        repo_root / "Common/headers",
        repo_root / "Common/source",
        repo_root / "tests",
        repo_root / "tools",
    ]

    missing = []
    for path in required_paths:
        if not path.exists():
            missing.append(str(path.relative_to(repo_root)))

    if missing:
        print(f"ERROR: Repository root validation failed.", file=sys.stderr)
        print(f"Expected directory: {repo_root}", file=sys.stderr)
        print(f"Missing required paths:", file=sys.stderr)
        for path in missing:
            print(f"  - {path}", file=sys.stderr)
        print(f"\nThis may indicate:", file=sys.stderr)
        print(f"  1. Script is running from wrong directory", file=sys.stderr)
        print(f"  2. Repository structure has changed", file=sys.stderr)
        print(f"  3. Repository is not fully checked out", file=sys.stderr)
        return False

    return True


class VerbAnalyzer:
    def __init__(self, repo_root: Path):
        self.repo_root = repo_root
        self.verbs_by_processor = {}  # processor -> [verb names]
        self.implementations = defaultdict(list)  # processor.verb -> status
        self.processors = []  # List[EFPProcessor]

    def extract_verbs_from_rc(self):
        """
        Extract verb names from kernelverbs.rc using programmatic parsing.

        This replaces the old hardcoded verb mappings with dynamic extraction
        using the same logic as the stub generator.
        """
        rc_file = self.repo_root / "Common/resources/Win32/kernelverbs.rc"

        if not rc_file.exists():
            raise FileNotFoundError(f"kernelverbs.rc not found at {rc_file}")

        # Parse RC file to discover all processors
        print(f"Parsing {rc_file}...", file=sys.stderr)
        self.processors, had_errors = parse_kernelverbs_rc(str(rc_file))

        if had_errors:
            print(f"Warning: Errors encountered while parsing RC file", file=sys.stderr)

        print(f"Discovered {len(self.processors)} processors", file=sys.stderr)

        # Read RC content for verb extraction
        with open(rc_file, 'r', encoding='utf-8', errors='replace') as f:
            rc_content = f.read()

        # Extract verb names for each processor
        for proc in self.processors:
            verb_names = extract_verb_names(rc_content, proc.name, proc.verb_count)
            self.verbs_by_processor[proc.name] = verb_names

            # Report if we got placeholder names (extraction failed)
            if verb_names and verb_names[0].startswith('verb'):
                print(f"Warning: {proc.name} has placeholder verb names (extraction may have failed)",
                      file=sys.stderr)

        print(f"Extracted verbs for {len(self.verbs_by_processor)} processors", file=sys.stderr)

    def analyze_implementations(self):
        """Analyze implementation status of each verb"""

        # Map of processors to their implementation files
        # This is somewhat manual but reflects actual file structure
        impl_files = {
            'file': 'tests/headless_file_verbs.c',
            'frontier': 'tests/headless_frontier_verbs.c',
            'string': 'Common/source/stringverbs.c',
            'table': 'Common/source/tableverbs.c',
            'menu': 'Common/source/menuverbs.c',
            'window': 'Common/source/shellwindowverbs.c',
            'dialog': 'Common/source/langverbs.c',
            'date': 'Common/source/langdate.c',
            'op': 'Common/source/opverbs.c',
            'math': 'Common/source/langmath.c',
            'xml': 'Common/source/langxml.c',
        }

        # Check each file for platform-specific code and missing impls
        for processor, filepath in impl_files.items():
            full_path = self.repo_root / filepath
            if full_path.exists():
                print(f"Analyzing {processor} in {filepath}...", file=sys.stderr)
                self._analyze_file(processor, full_path)
            else:
                print(f"Warning: {filepath} not found, marking {processor} as stub",
                      file=sys.stderr)

        # All unanalyzed processors are stubs
        analyzed_procs = set(impl_files.keys())
        stub_procs = set(self.verbs_by_processor.keys()) - analyzed_procs

        print(f"Marking {len(stub_procs)} processors as unimplemented stubs", file=sys.stderr)

        for proc in stub_procs:
            for verb in self.verbs_by_processor.get(proc, []):
                self.implementations[f'{proc}.{verb}'] = ['unimplemented_stub']

    def _analyze_file(self, processor: str, filepath: Path):
        """Analyze a single implementation file"""
        try:
            with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
                content = f.read()
        except Exception as e:
            print(f"Error reading {filepath}: {e}", file=sys.stderr)
            return

        # Look for platform-specific patterns
        has_platform_guards = bool(re.search(
            r'#ifdef.*WIN|#ifdef.*MAC|#if defined.*WIN|#if defined.*MAC',
            content
        ))
        has_headless_guards = bool(re.search(
            r'#ifdef FRONTIER_HEADLESS|#if defined\(FRONTIER_HEADLESS\)',
            content
        ))

        # Check for "not implemented" pattern in stubs
        has_not_implemented = bool(re.search(
            r'copystring.*not implemented|return false.*not implemented',
            content,
            re.IGNORECASE
        ))

        verbs = self.verbs_by_processor.get(processor, [])
        print(f"  Checking {len(verbs)} verbs for {processor}", file=sys.stderr)

        for verb in verbs:
            # Look for verb references (case statements, function names, etc.)
            # Use processor prefix to be more specific
            prefix = processor[:3] + 'v'  # e.g., "filv", "strv", "datv"
            case_pattern = rf'case\s+{prefix}_{verb}:|{prefix}_{verb}\s*='

            if re.search(case_pattern, content, re.IGNORECASE):
                status = []

                # Check if this specific verb returns "not implemented"
                # Look for the verb's case block and check if it has stub implementation
                verb_block_match = re.search(
                    rf'case\s+{prefix}_{verb}:.*?(?=case\s+\w+:|default:|\}})',
                    content,
                    re.IGNORECASE | re.DOTALL
                )

                if verb_block_match:
                    verb_block = verb_block_match.group(0)
                    if 'not implemented' in verb_block.lower():
                        status.append('unimplemented_stub')
                    elif has_platform_guards:
                        status.append('platform_specific')
                    elif has_headless_guards:
                        status.append('gui_dependent')
                    else:
                        status.append('implemented')
                else:
                    # Found verb enum but not implementation - likely missing
                    status.append('missing')

                self.implementations[f'{processor}.{verb}'] = status
            else:
                # Verb not found in file
                self.implementations[f'{processor}.{verb}'] = ['missing']


def generate_report(analyzer: VerbAnalyzer, output_file: Path):
    """Generate markdown report of verb status"""

    # Categorize verbs
    missing = []
    platform_specific = []
    gui_dependent = []
    unimplemented_stubs = []
    fully_implemented = []

    for verb_key, statuses in sorted(analyzer.implementations.items()):
        if 'unimplemented_stub' in statuses:
            unimplemented_stubs.append(verb_key)
        elif 'missing' in statuses:
            missing.append(verb_key)
        elif 'platform_specific' in statuses:
            platform_specific.append(verb_key)
        elif 'gui_dependent' in statuses:
            gui_dependent.append(verb_key)
        elif 'implemented' in statuses:
            fully_implemented.append(verb_key)

    total_verbs = sum(len(verbs) for verbs in analyzer.verbs_by_processor.values())

    # Generate markdown
    lines = [
        '# Frontier Verb Implementation Status Report',
        '',
        '**Generated**: Programmatic analysis from kernelverbs.rc',
        '',
        '## Executive Summary',
        '',
        f'- **Total Processors**: {len(analyzer.processors)}',
        f'- **Total Verbs**: {total_verbs}',
        f'- **Fully Implemented**: {len(fully_implemented)}',
        f'- **Platform-Specific**: {len(platform_specific)}',
        f'- **GUI-Dependent (Headless)**: {len(gui_dependent)}',
        f'- **Missing Implementations**: {len(missing)}',
        f'- **Unimplemented Stubs**: {len(unimplemented_stubs)}',
        '',
        '## Implementation Categories',
        '',
        '### Category 1: Unimplemented Stubs (Ready for Phase 2 Testing)',
        '',
        f'**Count**: {len(unimplemented_stubs)} verbs across multiple stub processors',
        '',
        'These processors have been generated but contain only stub implementations that return',
        '"not implemented". This is by design - they are ready for systematic testing to discover',
        'which verbs work in headless mode and which need special handling.',
        '',
        '**Processors**:',
        '',
    ]

    # Group unimplemented stubs by processor
    stubs_by_proc = defaultdict(list)
    for verb_key in unimplemented_stubs:
        proc, verb = verb_key.split('.', 1)
        stubs_by_proc[proc].append(verb)

    for proc in sorted(stubs_by_proc.keys()):
        verb_count = len(stubs_by_proc[proc])
        lines.append(f'- **{proc}**: {verb_count} verbs (stub)')

    lines.extend([
        '',
        '### Category 2: Platform-Specific Verbs',
        '',
        f'**Count**: {len(platform_specific)} verbs',
        '',
        'These verbs have conditional compilation guards (#ifdef WIN32, #ifdef MAC, etc.).',
        'They need platform-specific implementations for each supported OS.',
        '',
        '**Verbs**:',
        '',
    ])

    for verb_key in sorted(platform_specific):
        lines.append(f'- {verb_key}')

    lines.extend([
        '',
        '### Category 3: GUI-Dependent Verbs (Headless Incompatible)',
        '',
        f'**Count**: {len(gui_dependent)} verbs',
        '',
        'These verbs are guarded with #ifdef FRONTIER_HEADLESS or similar. They likely require',
        'UI features that are not available in headless mode and may not be implementable.',
        '',
        '**Verbs**:',
        '',
    ])

    for verb_key in sorted(gui_dependent):
        lines.append(f'- {verb_key}')

    lines.extend([
        '',
        '### Category 4: Missing Implementations',
        '',
        f'**Count**: {len(missing)} verbs',
        '',
        'These verbs are defined but not yet implemented (likely stubs or TODOs).',
        '',
        '**Verbs**:',
        '',
    ])

    for verb_key in sorted(missing):
        lines.append(f'- {verb_key}')

    lines.extend([
        '',
        '### Category 5: Fully Implemented Verbs',
        '',
        f'**Count**: {len(fully_implemented)} verbs',
        '',
        'These verbs have complete, working implementations.',
        '',
        '**Verbs**:',
        '',
    ])

    for verb_key in sorted(fully_implemented):
        lines.append(f'- {verb_key}')

    lines.extend([
        '',
        '## Verb Details by Processor',
        '',
        'Complete listing of all verbs organized by processor:',
        '',
    ])

    # Add detailed processor-by-processor breakdown
    for proc in sorted(analyzer.verbs_by_processor.keys()):
        verbs = analyzer.verbs_by_processor[proc]
        lines.append(f'### {proc} ({len(verbs)} verbs)')
        lines.append('')

        for verb in verbs:
            verb_key = f'{proc}.{verb}'
            status = analyzer.implementations.get(verb_key, ['unknown'])
            status_str = ', '.join(status)
            lines.append(f'- `{verb}` - {status_str}')

        lines.append('')

    lines.extend([
        '',
        '## Next Steps',
        '',
        '### Phase 2: Systematic Testing',
        '1. Create test harness to run all verbs',
        '2. Document which verbs work in headless mode',
        '3. Categorize failures into actionable buckets',
        '4. Build implementation roadmap based on real data',
        '',
        '### Phase 3: Targeted Implementation',
        '1. Start with high-impact processors (string, date, dialog)',
        '2. Implement platform-specific versions for macOS/Linux',
        '3. Document GUI-incompatible verbs and provide error messages',
        '4. Test incrementally with real UserTalk scripts',
        '',
        '---',
        '',
        '*This report was generated programmatically by analyzing kernelverbs.rc and*',
        '*implementation files. Verb names are extracted directly from the RC file*',
        '*rather than hardcoded mappings.*',
    ])

    with open(output_file, 'w') as f:
        f.write('\n'.join(lines))

    return output_file


if __name__ == '__main__':
    # Use relative path from script location to find repo root
    script_dir = Path(__file__).parent.resolve()  # tools/verb_analyzer/
    repo_root = script_dir.parent.parent  # Navigate up to repo root

    print(f"Script directory: {script_dir}", file=sys.stderr)
    print(f"Repository root: {repo_root}", file=sys.stderr)

    # Validate repo root before proceeding
    if not validate_repo_root(repo_root):
        print("\nFailed to validate repository root. Exiting.", file=sys.stderr)
        sys.exit(1)

    print("Repository root validated successfully.", file=sys.stderr)
    print("", file=sys.stderr)

    analyzer = VerbAnalyzer(repo_root)

    try:
        analyzer.extract_verbs_from_rc()
        analyzer.analyze_implementations()
    except Exception as e:
        print(f"\nError during analysis: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        sys.exit(1)

    output = repo_root / 'planning/VERB_IMPLEMENTATION_STATUS.md'
    output.parent.mkdir(parents=True, exist_ok=True)

    try:
        result = generate_report(analyzer, output)
        print(f"\n✓ Generated report: {result}", file=sys.stderr)
        print(f"\nSummary:", file=sys.stderr)
        print(f"  Total processors: {len(analyzer.processors)}", file=sys.stderr)
        print(f"  Total verbs: {sum(len(v) for v in analyzer.verbs_by_processor.values())}", file=sys.stderr)
        print(f"  Categorized: {len(analyzer.implementations)}", file=sys.stderr)
    except Exception as e:
        print(f"\nError generating report: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        sys.exit(1)
