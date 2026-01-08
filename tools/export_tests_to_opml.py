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

import yaml
import sys
import os
from datetime import datetime
from pathlib import Path
from xml.etree.ElementTree import Element, SubElement, ElementTree, tostring
from xml.dom import minidom


def prettify_category_name(filename):
    """Convert 'string_verbs.yaml' to 'String Verbs (string_verbs)'"""
    base = filename.replace('.yaml', '')
    # Convert underscores to spaces and title case
    pretty = base.replace('_', ' ').title()
    return f"{pretty} ({base})"


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
        outline.set('text', item['text'])

        if item.get('children'):
            add_outline_elements(outline, item['children'])


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
        category_outline.set('text', category_name)

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
            test_outline.set('text', test_text)

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
                    field_outline.set('text', f"{key}: {value}")

            # Script section
            script_text = test.get('script', '')
            if script_text:
                script_outline = SubElement(test_outline, 'outline')
                script_outline.set('text', 'Script')

                # Parse script into outline format
                parsed_lines = parse_script_to_outline(script_text)
                hierarchy = build_outline_hierarchy(parsed_lines)
                add_outline_elements(script_outline, hierarchy)

    # Write OPML to file with pretty formatting
    tree = ElementTree(opml)
    xml_str = minidom.parseString(
        tostring(opml, encoding='unicode')
    ).toprettyxml(indent='  ')

    # Remove extra blank lines that minidom adds
    xml_lines = [line for line in xml_str.split('\n') if line.strip()]
    xml_str = '\n'.join(xml_lines) + '\n'

    with open(output_file, 'w') as f:
        f.write(xml_str)

    print(f"Successfully exported {len(test_files)} test categories to {output_file}")

    # Count total tests
    total_tests = sum(len(yaml.safe_load(open(tf))['tests'])
                     for tf in test_files
                     if yaml.safe_load(open(tf)).get('tests'))
    print(f"Total tests exported: {total_tests}")


def main():
    # Determine project root (assuming script is in tools/)
    script_dir = Path(__file__).parent
    project_root = script_dir.parent

    test_dir = project_root / 'tests' / 'integration' / 'test_cases'
    output_file = project_root / 'tests' / 'tmp' / 'integration_tests.opml'

    # Create output directory if needed
    output_file.parent.mkdir(parents=True, exist_ok=True)

    if not test_dir.exists():
        print(f"Error: Test directory not found: {test_dir}", file=sys.stderr)
        sys.exit(1)

    export_tests_to_opml(test_dir, output_file)


if __name__ == '__main__':
    main()
