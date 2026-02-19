#!/usr/bin/env python3
"""
Export Frontier unit test results to OPML format.

This script reads JSON result files from tests/tmp/unit/ (one per executable)
and generates an OPML report suitable for viewing in outline editors.

Input format (per-executable JSON):
{
  "executable": "runtime_tests",
  "timestamp": "2026-02-18T12:00:00Z",
  "tests": [
    {"name": "run_basic_script", "passed": true},
    {"name": "run_constants_smoke", "passed": false}
  ],
  "total": 2,
  "passed": 1,
  "failed": 1
}

Output:
- reports/unit_tests.opml          — manifest with links to per-executable files
- reports/unit_tests/<name>.opml   — one per executable
"""

import json
import os
import sys
from datetime import datetime, timezone
from pathlib import Path
from xml.dom import minidom
from xml.etree.ElementTree import Element, SubElement, tostring


# ---------------------------------------------------------------------------
# Utility functions (mirrored from export_tests_to_opml.py)
# ---------------------------------------------------------------------------

def sanitize_xml_text(text):
    """
    Sanitize text for XML by removing invalid control characters.

    XML 1.0 only allows specific control characters:
    - Tab (0x09)
    - Newline (0x0A)
    - Carriage return (0x0D)
    All other characters in range 0x00-0x1F are invalid.
    """
    if not isinstance(text, str):
        return str(text)

    result = []
    for char in text:
        ord_val = ord(char)
        if ord_val >= 0x20 or ord_val in (0x09, 0x0A, 0x0D):
            result.append(char)
        else:
            result.append(f'[0x{ord_val:02x}]')

    return ''.join(result)


def format_stats_summary(stats):
    """
    Format statistics as a summary string.

    Unit tests have no "skip" concept — only pass/fail.

    Args:
        stats: dict with 'total', 'pass', 'fail' keys

    Returns:
        str: e.g., "45 tests: 40 pass (89%), 5 fail (11%)"
    """
    total = stats['total']
    if total == 0:
        return "0 tests"

    parts = [f"{total} tests:"]

    pass_count = stats['pass']
    pass_pct = (pass_count * 100) // total
    parts.append(f"{pass_count} pass ({pass_pct}%)")

    fail_count = stats['fail']
    if fail_count > 0:
        fail_pct = (fail_count * 100) // total
        parts.append(f"{fail_count} fail ({fail_pct}%)")

    return ' '.join(parts)


def write_opml_if_changed(opml_element, output_file):
    """
    Write OPML to file only if content (excluding timestamp) has changed.

    This prevents unnecessary git commits when only timestamps differ.

    Args:
        opml_element: XML Element tree representing OPML
        output_file: Path to output file

    Returns:
        True if file was written (content changed), False otherwise
    """
    xml_str = minidom.parseString(
        tostring(opml_element, encoding='unicode')
    ).toprettyxml(indent='  ')

    # Remove extra blank lines that minidom adds
    xml_lines = [line for line in xml_str.split('\n') if line.strip()]
    new_content = '\n'.join(xml_lines) + '\n'

    if os.path.exists(output_file):
        with open(output_file, 'r') as f:
            existing_content = f.read()

        new_without_date = '\n'.join([
            line for line in new_content.split('\n')
            if '<dateCreated>' not in line
        ])
        existing_without_date = '\n'.join([
            line for line in existing_content.split('\n')
            if '<dateCreated>' not in line
        ])

        if new_without_date == existing_without_date:
            return False

    with open(output_file, 'w') as f:
        f.write(new_content)
    return True


def prettify_category_name(executable_name):
    """Convert 'runtime_tests' to 'Runtime Tests (runtime_tests)'"""
    pretty = executable_name.replace('_', ' ').title()
    return f"{pretty} ({executable_name})"


# ---------------------------------------------------------------------------
# JSON loading
# ---------------------------------------------------------------------------

def load_unit_test_results(unit_json_dir):
    """
    Load all per-executable JSON result files from tests/tmp/unit/.

    Args:
        unit_json_dir: Path to tests/tmp/unit/

    Returns:
        dict: {executable_name: {
            'executable': str,
            'timestamp': str,
            'tests': list of {'name': str, 'passed': bool},
            'total': int,
            'passed': int,
            'failed': int,
            'stats': {'total': int, 'pass': int, 'fail': int}
        }}
    """
    json_files = sorted(Path(unit_json_dir).glob('*.json'))
    executables = {}

    for json_file in json_files:
        # Skip the aggregate file
        if json_file.name == 'last_run.json':
            continue
        try:
            with open(json_file, 'r') as f:
                data = json.load(f)
        except (json.JSONDecodeError, OSError) as e:
            print(f"Warning: Could not read {json_file}: {e}", file=sys.stderr)
            continue

        exe_name = data.get('executable', json_file.stem)
        tests = data.get('tests', [])
        total = data.get('total', len(tests))
        passed = data.get('passed', sum(1 for t in tests if t.get('passed', False)))
        failed = data.get('failed', total - passed)

        executables[exe_name] = {
            'executable': exe_name,
            'timestamp': data.get('timestamp', ''),
            'tests': tests,
            'total': total,
            'passed': passed,
            'failed': failed,
            'stats': {
                'total': total,
                'pass': passed,
                'fail': failed,
            }
        }

    return executables


# ---------------------------------------------------------------------------
# OPML generation — per-executable files
# ---------------------------------------------------------------------------

def generate_executable_opml(exe_name, exe_data, output_file):
    """
    Generate standalone OPML for a single test executable.

    Args:
        exe_name: Executable name (e.g., 'runtime_tests')
        exe_data: Dict with tests, stats, etc.
        output_file: Path to output OPML file
    """
    opml = Element('opml')
    opml.set('version', '2.0')

    # Head
    head = SubElement(opml, 'head')
    title = SubElement(head, 'title')
    pretty_name = prettify_category_name(exe_name)
    stats_summary = format_stats_summary(exe_data['stats'])
    title.text = f"{pretty_name} - {stats_summary}"
    date_created = SubElement(head, 'dateCreated')
    date_created.text = datetime.now(timezone.utc).strftime('%a, %d %b %Y %H:%M:%S GMT')

    # Body — one outline node per test function
    body = SubElement(opml, 'body')

    for test in exe_data['tests']:
        test_name = test.get('name', 'Unnamed Test')
        passed = test.get('passed', False)
        indicator = '\u2713' if passed else '\u2717'  # checkmark or X
        test_text = f"{indicator} {test_name}"

        test_outline = SubElement(body, 'outline')
        test_outline.set('text', sanitize_xml_text(test_text))

    write_opml_if_changed(opml, output_file)


# ---------------------------------------------------------------------------
# OPML generation — manifest
# ---------------------------------------------------------------------------

def generate_manifest_opml(executables, output_file):
    """
    Generate top-level manifest OPML with transclusion links to per-executable files.

    Args:
        executables: Dict mapping executable_name -> exe_data
        output_file: Path to output manifest OPML file
    """
    # Aggregate stats
    total_stats = {'total': 0, 'pass': 0, 'fail': 0}
    for exe_data in executables.values():
        stats = exe_data['stats']
        total_stats['total'] += stats['total']
        total_stats['pass'] += stats['pass']
        total_stats['fail'] += stats['fail']

    total_summary = format_stats_summary(total_stats)

    # Build OPML
    opml = Element('opml')
    opml.set('version', '2.0')

    head = SubElement(opml, 'head')
    title = SubElement(head, 'title')
    title.text = f'Frontier Unit Tests - {total_summary}'
    date_created = SubElement(head, 'dateCreated')
    date_created.text = datetime.now(timezone.utc).strftime('%a, %d %b %Y %H:%M:%S GMT')

    body = SubElement(opml, 'body')

    # "Last Test Run" section
    header = SubElement(body, 'outline')
    header.set('text', 'Last Test Run')

    # Use the most recent timestamp across all executables
    timestamps = [
        exe_data['timestamp']
        for exe_data in executables.values()
        if exe_data.get('timestamp')
    ]
    if timestamps:
        try:
            latest_ts = max(timestamps)
            ts = datetime.fromisoformat(latest_ts.replace('Z', '+00:00'))
            run_time = ts.strftime('%Y-%m-%d at %H:%M UTC')
        except (ValueError, TypeError):
            run_time = 'unknown'
    else:
        run_time = 'unknown'

    run_line = SubElement(header, 'outline')
    run_line.set('text', f'Run: {run_time}')

    # Pass/fail summary
    if total_stats['total'] > 0:
        pass_pct = round(total_stats['pass'] * 100 / total_stats['total'])
        fail_pct = round(total_stats['fail'] * 100 / total_stats['total'])
        results_text = (
            f"Results: {total_stats['pass']} passed ({pass_pct}%), "
            f"{total_stats['fail']} failed ({fail_pct}%)"
        )
    else:
        results_text = 'Results: no tests ran'
    results_line = SubElement(header, 'outline')
    results_line.set('text', results_text)

    # Executable count
    exe_count_line = SubElement(header, 'outline')
    exe_count_line.set('text',
                       f"Executables: {len(executables)} "
                       f"({total_stats['total']} tests total)")

    # Category links (one per executable, sorted for deterministic output)
    for exe_name in sorted(executables.keys()):
        exe_data = executables[exe_name]
        pretty_name = prettify_category_name(exe_name)
        stats_summary = format_stats_summary(exe_data['stats'])
        display_text = f"{pretty_name} - {stats_summary}"

        cat_outline = SubElement(body, 'outline')
        cat_outline.set('text', sanitize_xml_text(display_text))
        cat_outline.set('type', 'link')
        cat_outline.set('url',
                        f'https://raw.githubusercontent.com/jsavin/Frontier/'
                        f'develop/reports/unit_tests/{exe_name}.opml')

    write_opml_if_changed(opml, output_file)


# ---------------------------------------------------------------------------
# Main entry point
# ---------------------------------------------------------------------------

def main():
    # Determine project root (script lives in tools/)
    script_dir = Path(__file__).parent
    project_root = script_dir.parent

    unit_json_dir = project_root / 'tests' / 'tmp' / 'unit'
    output_dir = project_root / 'reports'

    # Gracefully handle missing input directory
    if not unit_json_dir.exists():
        print(f"Warning: Unit test results directory not found: {unit_json_dir}",
              file=sys.stderr)
        print("Run the unit test suite first to generate JSON result files.",
              file=sys.stderr)
        sys.exit(0)

    # Load all per-executable JSON results
    executables = load_unit_test_results(unit_json_dir)

    if not executables:
        print(f"Warning: No JSON result files found in {unit_json_dir}",
              file=sys.stderr)
        print("Run the unit test suite first to generate JSON result files.",
              file=sys.stderr)
        sys.exit(0)

    # Ensure output directories exist
    output_dir.mkdir(parents=True, exist_ok=True)
    category_dir = output_dir / 'unit_tests'
    category_dir.mkdir(parents=True, exist_ok=True)

    print("Generating unit test OPML reports...")

    # Generate per-executable OPML files
    for exe_name, exe_data in sorted(executables.items()):
        exe_file = category_dir / f'{exe_name}.opml'
        generate_executable_opml(exe_name, exe_data, exe_file)
        stats = exe_data['stats']
        print(f"  Generated: unit_tests/{exe_file.name} "
              f"({stats['total']} tests: {stats['pass']} pass, {stats['fail']} fail)")

    # Generate manifest
    manifest_file = output_dir / 'unit_tests.opml'
    generate_manifest_opml(executables, manifest_file)
    print(f"  Generated: {manifest_file.name} (manifest with {len(executables)} executables)")

    # Summary
    total_tests = sum(e['total'] for e in executables.values())
    total_passed = sum(e['passed'] for e in executables.values())
    total_failed = sum(e['failed'] for e in executables.values())
    print(f"\nUnit test OPML export complete:")
    print(f"  Manifest: reports/unit_tests.opml")
    print(f"  Executable files: reports/unit_tests/ ({len(executables)} files)")
    print(f"  Total: {total_tests} tests, {total_passed} passed, {total_failed} failed")


if __name__ == '__main__':
    main()
