#!/usr/bin/env python3
"""
Export Frontier integration tests to OPML format.

This script reads all YAML test files from tests/integration/test_cases/
and generates an OPML file suitable for viewing in outline editors.

UserTalk scripts are converted to outline format by:
- Removing semicolons from line endings
- Removing curly braces (structure is represented by outline hierarchy)
- Using indentation to create nested outline elements
"""

import json
import yaml
import sys
import os
from datetime import datetime, timezone
from pathlib import Path
from xml.etree.ElementTree import Element, SubElement, ElementTree, tostring
from xml.dom import minidom


def prettify_category_name(filename):
    """Convert 'string_verbs.yaml' to 'String Verbs (string_verbs)'"""
    base = filename.replace('.yaml', '')
    # Convert underscores to spaces and title case
    pretty = base.replace('_', ' ').title()
    return f"{pretty} ({base})"


def extract_category_key(filename):
    """
    Extract category key from test filename.

    Args:
        filename: Test YAML filename (e.g., 'db_verbs.yaml')

    Returns:
        Category key (e.g., 'db_verbs')
    """
    return filename.replace('.yaml', '')


def count_test_stats(tests):
    """
    Count pass/skip/fail statistics for a list of tests.

    Args:
        tests: List of test dictionaries from YAML

    Returns:
        dict: {'total': int, 'pass': int, 'skip': int, 'fail': int}
    """
    total = len(tests)
    skip_count = 0
    fail_count = 0

    for test in tests:
        if test.get('skip', False):
            skip_count += 1
        elif test.get('expected_success', True) is False:
            # Tests with expected_success=false are expected error tests
            # They pass when the error occurs, so count as expected passes
            pass

    pass_count = total - skip_count - fail_count

    return {
        'total': total,
        'pass': pass_count,
        'skip': skip_count,
        'fail': fail_count
    }


def format_stats_summary(stats):
    """
    Format statistics as a summary string.

    Args:
        stats: dict with 'total', 'pass', 'skip', 'fail' keys

    Returns:
        str: e.g., "45 tests: 40 pass (89%), 3 skip (7%), 2 fail (4%)"
    """
    total = stats['total']
    if total == 0:
        return "0 tests"

    parts = [f"{total} tests:"]

    # Pass count
    pass_count = stats['pass']
    pass_pct = (pass_count * 100) // total
    parts.append(f"{pass_count} pass ({pass_pct}%)")

    # Skip count (only if non-zero)
    skip_count = stats['skip']
    if skip_count > 0:
        skip_pct = (skip_count * 100) // total
        parts.append(f"{skip_count} skip ({skip_pct}%)")

    # Fail count (only if non-zero)
    fail_count = stats['fail']
    if fail_count > 0:
        fail_pct = (fail_count * 100) // total
        parts.append(f"{fail_count} fail ({fail_pct}%)")

    return ' '.join(parts)


def group_tests_by_category(test_dir):
    """
    Group test files by category.

    Args:
        test_dir: Path to tests/integration/test_cases/

    Returns:
        dict: {category_key: {
            'filename': Path,
            'pretty_name': str,
            'tests': list,
            'test_count': int,
            'stats': dict  # pass/skip/fail counts
        }}
    """
    test_files = sorted(Path(test_dir).glob('*.yaml'))
    categories = {}

    for test_file in test_files:
        category_key = extract_category_key(test_file.name)
        pretty_name = prettify_category_name(test_file.name)

        # Load YAML
        with open(test_file, 'r') as f:
            data = yaml.safe_load(f)

        if not data or 'tests' not in data:
            print(f"Warning: No tests found in {test_file}", file=sys.stderr)
            continue

        tests = data['tests']
        stats = count_test_stats(tests)

        categories[category_key] = {
            'filename': test_file,
            'pretty_name': pretty_name,
            'tests': tests,
            'test_count': len(tests),
            'stats': stats
        }

    return categories


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

    # Filter out invalid XML control characters
    result = []
    for char in text:
        ord_val = ord(char)
        # Allow normal characters and the 3 allowed control characters
        if ord_val >= 0x20 or ord_val in (0x09, 0x0A, 0x0D):
            result.append(char)
        else:
            # Replace invalid control character with description
            result.append(f'[0x{ord_val:02x}]')

    return ''.join(result)


def parse_script_to_outline(script_text):
    """
    Parse UserTalk script into outline structure.

    Removes semicolons and curly braces, preserves indentation hierarchy.
    Returns list of (indent_level, text) tuples.
    """
    lines = script_text.split('\n')
    result = []

    for line in lines:
        if not line.strip():
            continue

        # Calculate indentation level (spaces or tabs)
        stripped = line.lstrip()
        indent_level = len(line) - len(stripped)

        # Remove semicolons from end
        text = stripped.rstrip(';').rstrip()

        # Skip lines that are just closing braces
        if text == '}':
            continue

        # Remove opening curly braces
        if text.endswith('{'):
            text = text[:-1].rstrip()

        if text:
            result.append((indent_level, text))

    return result


def build_outline_hierarchy(parsed_lines):
    """
    Convert flat list of (indent, text) into nested structure.
    Returns list of dicts with 'text' and optional 'children' keys.
    """
    if not parsed_lines:
        return []

    # Normalize indent levels to 0, 1, 2, etc.
    indent_levels = sorted(set(indent for indent, _ in parsed_lines))
    indent_map = {level: i for i, level in enumerate(indent_levels)}

    normalized = [(indent_map[indent], text) for indent, text in parsed_lines]

    # Build hierarchy
    root_items = []
    stack = [(-1, root_items)]  # (level, children_list)

    for level, text in normalized:
        item = {'text': text}

        # Pop stack until we find the parent level
        while stack and stack[-1][0] >= level:
            stack.pop()

        # Add to parent's children
        parent_level, parent_children = stack[-1]
        parent_children.append(item)

        # Push current item onto stack for potential children
        if 'children' not in item:
            item['children'] = []
        stack.append((level, item['children']))

    return root_items


def add_outline_elements(parent_elem, items):
    """Recursively add outline elements to XML tree."""
    for item in items:
        outline = SubElement(parent_elem, 'outline')
        outline.set('text', sanitize_xml_text(item['text']))

        if item.get('children'):
            add_outline_elements(outline, item['children'])


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
    # Generate new XML string
    xml_str = minidom.parseString(
        tostring(opml_element, encoding='unicode')
    ).toprettyxml(indent='  ')

    # Remove extra blank lines that minidom adds
    xml_lines = [line for line in xml_str.split('\n') if line.strip()]
    new_content = '\n'.join(xml_lines) + '\n'

    # Check if file exists
    if os.path.exists(output_file):
        with open(output_file, 'r') as f:
            existing_content = f.read()

        # Compare without dateCreated lines
        new_without_date = '\n'.join([
            line for line in new_content.split('\n')
            if '<dateCreated>' not in line
        ])
        existing_without_date = '\n'.join([
            line for line in existing_content.split('\n')
            if '<dateCreated>' not in line
        ])

        if new_without_date == existing_without_date:
            # Content unchanged - don't write file
            return False

    # Content changed or file doesn't exist - write it
    with open(output_file, 'w') as f:
        f.write(new_content)
    return True


def generate_category_opml(category_key, category_data, output_file):
    """
    Generate standalone OPML for a single test category.

    Args:
        category_key: Base category name (e.g., 'db_verbs')
        category_data: Dict with 'pretty_name', 'tests', 'stats', etc.
        output_file: Path to output OPML file
    """
    # Create OPML structure
    opml = Element('opml')
    opml.set('version', '2.0')

    # Head section
    head = SubElement(opml, 'head')
    title = SubElement(head, 'title')
    # Include stats in title: "String Verbs (string_verbs) - 45 tests: 40 pass (89%), 3 skip (7%)"
    stats_summary = format_stats_summary(category_data['stats'])
    title.text = f"{category_data['pretty_name']} - {stats_summary}"
    date_created = SubElement(head, 'dateCreated')
    date_created.text = datetime.now(timezone.utc).strftime('%a, %d %b %Y %H:%M:%S GMT')

    # Body section
    body = SubElement(opml, 'body')

    # Process each test in category (tests at top level)
    for test in category_data['tests']:
        # Build test name with description
        test_name = test.get('name', 'Unnamed Test')
        test_desc = test.get('description', '')
        if test_desc:
            test_text = f"{test_name} - {test_desc}"
        else:
            test_text = test_name

        test_outline = SubElement(body, 'outline')
        test_outline.set('text', sanitize_xml_text(test_text))

        # Metadata section (only non-default fields)
        metadata_fields = []
        for key, value in test.items():
            if key in ['name', 'description', 'script']:
                continue  # Skip these, handled separately

            # Skip default values
            if key == 'expected_success' and value is True:
                continue
            if key == 'timeout' and value == 10:
                continue

            metadata_fields.append((key, value))

        if metadata_fields:
            metadata_outline = SubElement(test_outline, 'outline')
            metadata_outline.set('text', 'Metadata')

            for key, value in metadata_fields:
                field_outline = SubElement(metadata_outline, 'outline')
                field_outline.set('text', sanitize_xml_text(f"{key}: {value}"))

        # Script section
        script_text = test.get('script', '')
        if script_text:
            script_outline = SubElement(test_outline, 'outline')
            script_outline.set('text', 'Script')

            # Parse script into outline format
            parsed_lines = parse_script_to_outline(script_text)
            hierarchy = build_outline_hierarchy(parsed_lines)
            add_outline_elements(script_outline, hierarchy)

    # Write OPML to file only if content changed
    write_opml_if_changed(opml, output_file)


def load_last_run_results(project_root):
    """
    Load the last integration test run results from JSON.

    Args:
        project_root: Path to the project root directory

    Returns:
        Parsed dict from last_run.json, or None if the file doesn't exist
    """
    json_path = Path(project_root) / 'tests' / 'tmp' / 'integration' / 'last_run.json'
    if not json_path.exists():
        return None
    try:
        with open(json_path, 'r') as f:
            return json.load(f)
    except (json.JSONDecodeError, OSError) as e:
        print(f"Warning: Could not read {json_path}: {e}", file=sys.stderr)
        return None


def generate_manifest_opml(categories, output_file, last_run=None, total_stats=None):
    """
    Generate top-level manifest OPML with transclusion links.

    Args:
        categories: Dict mapping category_key -> category_data
        output_file: Path to output manifest OPML file
        last_run: Optional dict from last_run.json with test run results
        total_stats: Optional dict with YAML-defined test counts
    """
    # Calculate total stats across all categories (use passed-in value if available)
    if total_stats is None:
        total_stats = {'total': 0, 'pass': 0, 'skip': 0, 'fail': 0}
        for category_data in categories.values():
            stats = category_data['stats']
            total_stats['total'] += stats['total']
            total_stats['pass'] += stats['pass']
            total_stats['skip'] += stats['skip']
            total_stats['fail'] += stats['fail']

    # Prefer actual last-run results in the title (matches body) and fall
    # back to YAML-defined stats only if no run data is available. This keeps
    # the title and body internally consistent.
    if last_run is not None and last_run.get('total', 0) > 0:
        title_stats = {
            'total': last_run.get('total', 0),
            'pass': last_run.get('passed', 0),
            'skip': last_run.get('skipped', 0),
            'fail': last_run.get('failed', 0),
        }
    else:
        title_stats = total_stats
    total_summary = format_stats_summary(title_stats)

    # Create OPML structure
    opml = Element('opml')
    opml.set('version', '2.0')

    # Head section
    head = SubElement(opml, 'head')
    title = SubElement(head, 'title')
    title.text = f'Frontier Integration Tests - {total_summary}'
    date_created = SubElement(head, 'dateCreated')
    date_created.text = datetime.now(timezone.utc).strftime('%a, %d %b %Y %H:%M:%S GMT')

    # Body section
    body = SubElement(opml, 'body')

    # Add "Last Test Run" header section
    header = SubElement(body, 'outline')
    header.set('text', 'Last Test Run')

    if last_run is not None:
        # Parse timestamp
        try:
            ts = datetime.fromisoformat(last_run['timestamp'].replace('Z', '+00:00'))
            run_time = ts.strftime('%Y-%m-%d at %H:%M UTC')
        except (KeyError, ValueError):
            run_time = 'unknown'

        run_line = SubElement(header, 'outline')
        run_line.set('text', f'Run: {run_time}')

        # Results line with counts and percentages
        lr_total = last_run.get('total', 0)
        lr_passed = last_run.get('passed', 0)
        lr_skipped = last_run.get('skipped', 0)
        lr_failed = last_run.get('failed', 0)
        if lr_total > 0:
            pass_pct = round(lr_passed * 100 / lr_total)
            skip_pct = round(lr_skipped * 100 / lr_total)
            fail_pct = round(lr_failed * 100 / lr_total)
            results_text = (
                f'Results: {lr_passed} passed ({pass_pct}%), '
                f'{lr_skipped} skipped ({skip_pct}%), '
                f'{lr_failed} failed ({fail_pct}%)'
            )
        else:
            results_text = 'Results: no tests ran'
        results_line = SubElement(header, 'outline')
        results_line.set('text', results_text)

        # Duration line
        duration = last_run.get('duration_seconds', 0)
        workers = last_run.get('workers', 1)
        batch_mode = last_run.get('batch_mode', False)
        mode_str = 'batch mode' if batch_mode else 'sequential mode'
        duration_line = SubElement(header, 'outline')
        duration_line.set('text', f'Duration: {duration}s ({workers} workers, {mode_str})')

        # YAML-defined stats line
        if total_stats is not None:
            yaml_total = total_stats['total']
            yaml_active = total_stats['pass']
            yaml_skip = total_stats['skip']
            num_categories = len(categories)
            yaml_line = SubElement(header, 'outline')
            yaml_line.set('text',
                          f'YAML-defined: {yaml_total} tests ({yaml_active} active, '
                          f'{yaml_skip} marked skip) across {num_categories} categories')
    else:
        # No run data available
        no_data_line = SubElement(header, 'outline')
        no_data_line.set('text', 'No run data available. Run: cd tests && make test-integration')

        # Still show YAML-defined stats if available
        if total_stats is not None:
            yaml_total = total_stats['total']
            yaml_active = total_stats['pass']
            yaml_skip = total_stats['skip']
            num_categories = len(categories)
            yaml_line = SubElement(header, 'outline')
            yaml_line.set('text',
                          f'YAML-defined: {yaml_total} tests ({yaml_active} active, '
                          f'{yaml_skip} marked skip) across {num_categories} categories')

    # Add category links at top level (sorted by key for deterministic ordering)
    for category_key in sorted(categories.keys()):
        category_data = categories[category_key]

        # Create outline element with transclusion link (absolute GitHub URL for Drummer)
        # Category files are in integration_tests/ subdirectory
        # Include stats summary: "String Verbs (string_verbs) - 45 tests: 40 pass (89%), 3 skip (7%)"
        stats_summary = format_stats_summary(category_data['stats'])
        display_text = f"{category_data['pretty_name']} - {stats_summary}"

        category_outline = SubElement(body, 'outline')
        category_outline.set('text', sanitize_xml_text(display_text))
        category_outline.set('type', 'link')
        category_outline.set('url', f'https://raw.githubusercontent.com/jsavin/Frontier/develop/reports/integration_tests/{category_key}.opml')

    # Write OPML to file only if content changed
    write_opml_if_changed(opml, output_file)


def export_hierarchical_opml(test_dir, output_dir):
    """
    Generate hierarchical OPML structure.

    Creates one manifest file in output_dir and category files in
    output_dir/integration_tests/ subdirectory.

    Args:
        test_dir: Path to tests/integration/test_cases/
        output_dir: Path to reports/ directory

    Returns:
        List of generated file paths
    """
    # Group tests by category
    categories = group_tests_by_category(test_dir)

    if not categories:
        print("Warning: No test categories found", file=sys.stderr)
        return []

    generated_files = []

    # Create subdirectory for category files
    category_dir = output_dir / 'integration_tests'
    category_dir.mkdir(parents=True, exist_ok=True)

    # Generate each category file in subdirectory
    for category_key, category_data in categories.items():
        category_file = category_dir / f'{category_key}.opml'
        generate_category_opml(category_key, category_data, category_file)
        generated_files.append(category_file)
        stats = category_data['stats']
        print(f"Generated: integration_tests/{category_file.name} ({stats['total']} tests: {stats['pass']} pass, {stats['skip']} skip)")

    # Load last run results and compute total YAML-defined stats
    project_root = output_dir.parent
    last_run = load_last_run_results(project_root)

    total_stats = {'total': 0, 'pass': 0, 'skip': 0, 'fail': 0}
    for category_data in categories.values():
        stats = category_data['stats']
        total_stats['total'] += stats['total']
        total_stats['pass'] += stats['pass']
        total_stats['skip'] += stats['skip']
        total_stats['fail'] += stats['fail']

    # Generate manifest file in output_dir (not subdirectory)
    manifest_file = output_dir / 'integration_tests.opml'
    generate_manifest_opml(categories, manifest_file, last_run=last_run, total_stats=total_stats)
    generated_files.append(manifest_file)
    print(f"Generated: {manifest_file.name} (manifest with {len(categories)} categories)")

    # Validate file count
    expected_file_count = len(categories) + 1  # categories + manifest
    if len(generated_files) != expected_file_count:
        print(f"Warning: Expected {expected_file_count} files, generated {len(generated_files)}",
              file=sys.stderr)

    return generated_files


def export_tests_to_opml(test_dir, output_file):
    """
    Read all YAML test files and generate OPML.

    Args:
        test_dir: Path to tests/integration/test_cases/
        output_file: Path to output OPML file
    """
    # Create OPML structure
    opml = Element('opml')
    opml.set('version', '2.0')

    # Head section
    head = SubElement(opml, 'head')
    title = SubElement(head, 'title')
    title.text = 'Frontier Integration Tests'
    date_created = SubElement(head, 'dateCreated')
    date_created.text = datetime.now().strftime('%a, %d %b %Y %H:%M:%S GMT')

    # Body section
    body = SubElement(opml, 'body')

    # Find all YAML test files
    test_files = sorted(Path(test_dir).glob('*.yaml'))

    if not test_files:
        print(f"Warning: No test files found in {test_dir}", file=sys.stderr)
        return

    # Process each test file (category)
    for test_file in test_files:
        category_name = prettify_category_name(test_file.name)

        # Load YAML
        with open(test_file, 'r') as f:
            data = yaml.safe_load(f)

        if not data or 'tests' not in data:
            print(f"Warning: No tests found in {test_file}", file=sys.stderr)
            continue

        # Create category outline
        category_outline = SubElement(body, 'outline')
        category_outline.set('text', sanitize_xml_text(category_name))

        # Process each test in category
        for test in data['tests']:
            # Build test name with description
            test_name = test.get('name', 'Unnamed Test')
            test_desc = test.get('description', '')
            if test_desc:
                test_text = f"{test_name} - {test_desc}"
            else:
                test_text = test_name

            test_outline = SubElement(category_outline, 'outline')
            test_outline.set('text', sanitize_xml_text(test_text))

            # Metadata section (only non-default fields)
            metadata_fields = []
            for key, value in test.items():
                if key in ['name', 'description', 'script']:
                    continue  # Skip these, handled separately

                # Skip default values
                if key == 'expected_success' and value is True:
                    continue
                if key == 'timeout' and value == 10:
                    continue

                metadata_fields.append((key, value))

            if metadata_fields:
                metadata_outline = SubElement(test_outline, 'outline')
                metadata_outline.set('text', 'Metadata')

                for key, value in metadata_fields:
                    field_outline = SubElement(metadata_outline, 'outline')
                    field_outline.set('text', sanitize_xml_text(f"{key}: {value}"))

            # Script section
            script_text = test.get('script', '')
            if script_text:
                script_outline = SubElement(test_outline, 'outline')
                script_outline.set('text', 'Script')

                # Parse script into outline format
                parsed_lines = parse_script_to_outline(script_text)
                hierarchy = build_outline_hierarchy(parsed_lines)
                add_outline_elements(script_outline, hierarchy)

    # Write OPML to file only if content changed
    write_opml_if_changed(opml, output_file)

    print(f"Successfully exported {len(test_files)} test categories to {output_file}")

    # Count total tests (load each file once)
    total_tests = 0
    for tf in test_files:
        with open(tf, 'r') as f:
            data = yaml.safe_load(f)
        if data and 'tests' in data:
            total_tests += len(data['tests'])
    print(f"Total tests exported: {total_tests}")


def main():
    # Determine project root (assuming script is in tools/)
    script_dir = Path(__file__).parent
    project_root = script_dir.parent

    test_dir = project_root / 'tests' / 'integration' / 'test_cases'
    output_dir = project_root / 'reports'

    # Create output directory if needed
    output_dir.mkdir(parents=True, exist_ok=True)

    if not test_dir.exists():
        print(f"Error: Test directory not found: {test_dir}", file=sys.stderr)
        sys.exit(1)

    # Generate hierarchical OPML structure
    print("Generating hierarchical OPML structure...")
    generated_files = export_hierarchical_opml(test_dir, output_dir)

    if generated_files:
        # Calculate total tests
        categories = group_tests_by_category(test_dir)
        total_tests = sum(cat['test_count'] for cat in categories.values())

        print(f"\nSuccessfully generated hierarchical OPML:")
        print(f"  Manifest: reports/integration_tests.opml")
        print(f"  Category files: reports/integration_tests/ ({len(categories)} files)")
        print(f"  Total tests: {total_tests}")
    else:
        print("Error: No files were generated", file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
