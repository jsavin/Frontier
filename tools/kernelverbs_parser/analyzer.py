#!/usr/bin/env python3
"""
Verb implementation analyzer.

Analyzes C source files to determine which kernel verbs are implemented
vs. stubbed, and detects Carbon dependencies and UI adapters.
"""

import os
import re
from typing import List, Optional, Dict
from pathlib import Path

from metadata_writer import VerbImplementation
from matchers import (
    detect_carbon_apis,
    detect_ui_adapters,
    detect_stub_verb,
    parse_annotations,
    estimate_complexity
)


class VerbImplementationAnalyzer:
    """
    Analyzes verb implementations across all processors.
    """

    def __init__(self, processors: List):
        """
        Initialize analyzer with processor definitions.

        Args:
            processors: List of EFPProcessor objects from parse_kernelverbs.py
        """
        self.processors = processors
        self.implementations = []

    def find_implementation_file(self, processor_name: str) -> Optional[str]:
        """
        Find the C source file containing verb implementations for a processor.

        Search order:
        1. tests/headless_{processor}_verbs.c
        2. Common/source/{processor}verbs.c
        3. Common/source/lang{processor}.c
        4. Special cases (frontier → frontierverbs.c, etc.)

        Args:
            processor_name: Name of the processor (e.g., "file", "frontier")

        Returns:
            Absolute path to implementation file, or None if not found
        """
        # Find project root (two levels up from tools/kernelverbs_parser)
        script_dir = Path(__file__).parent
        project_root = script_dir.parent.parent

        search_patterns = [
            project_root / f"tests/headless_{processor_name}_verbs.c",
            project_root / f"Common/source/{processor_name}verbs.c",
            project_root / f"Common/source/lang{processor_name}.c",
        ]

        # Special cases
        special_cases = {
            'frontier': [
                project_root / 'Common/source/frontierverbs.c',
                project_root / 'Common/source/langstartup.c'
            ],
            'sys': [project_root / 'Common/source/shellsysverbs.c'],
            'window': [project_root / 'Common/source/shellwindowverbs.c'],
            'opattributes': [project_root / 'Common/source/opverbs.c'],  # Often in same file as op
        }

        if processor_name in special_cases:
            search_patterns.extend(special_cases[processor_name])

        # Try each pattern
        for pattern in search_patterns:
            if pattern.exists():
                return str(pattern.absolute())

        return None

    def read_source_file(self, file_path: str) -> str:
        """
        Read source file contents.

        Args:
            file_path: Path to source file

        Returns:
            File contents as string
        """
        try:
            with open(file_path, 'r', encoding='utf-8', errors='replace') as f:
                return f.read()
        except Exception as e:
            print(f"Error reading {file_path}: {e}")
            return ""

    def extract_case_implementation(self, source: str, case_label: str) -> str:
        """
        Extract code for a specific case in a switch statement.

        Args:
            source: Source code containing switch statement
            case_label: Case label to find (e.g., "filv_exists")

        Returns:
            Code for that case, or empty string if not found
        """
        # Find the case statement
        pattern = rf'case\s+{re.escape(case_label)}\s*:'
        match = re.search(pattern, source)

        if not match:
            return ""

        # Extract from case to next break/case/default/}
        start = match.end()

        # Find the end (next case, default, or closing brace)
        end_pattern = r'(^\s*case\s+\w+\s*:|^\s*default\s*:|^\s*\})'
        remaining = source[start:]
        end_match = re.search(end_pattern, remaining, re.MULTILINE)

        if end_match:
            end = start + end_match.start()
        else:
            # Take until end of file (shouldn't happen)
            end = len(source)

        return source[start:end]

    def analyze_verb_implementation(self, source: str, processor_name: str,
                                   verb_name: str, token: int,
                                   impl_file: str) -> VerbImplementation:
        """
        Analyze a single verb implementation.

        Args:
            source: Source code containing verb implementation
            processor_name: Processor name
            verb_name: Verb name
            token: Token index
            impl_file: Source file path

        Returns:
            VerbImplementation record
        """
        # Parse annotations first (they override heuristics)
        annotations = parse_annotations(source)

        # Detect implementation vs. stub
        is_stub = detect_stub_verb(source)
        is_implemented = not is_stub

        # Detect Carbon dependencies (only if implemented)
        has_carbon = False
        if is_implemented and not annotations.get('carbon_deps', False):
            has_carbon = detect_carbon_apis(source)
        elif annotations.get('carbon_deps', False):
            has_carbon = True

        # Detect UI adapters (only if implemented)
        uses_ui_adapter = False
        if is_implemented and not annotations.get('ui_adapter', False):
            uses_ui_adapter = detect_ui_adapters(source)
        elif annotations.get('ui_adapter', False):
            uses_ui_adapter = True

        # Platform-specific flag
        platform_specific = annotations.get('platform_specific', False)

        # Estimate complexity
        complexity = estimate_complexity(source) if is_implemented else 1

        # Find line number (approximate - just find the case statement)
        case_label = f"{processor_name[0:3]}v_{verb_name}"  # e.g., filv_exists
        pattern = rf'case\s+{re.escape(case_label)}\s*:'
        match = re.search(pattern, source)
        line_num = source[:match.start()].count('\n') + 1 if match else 0

        return VerbImplementation(
            processor=processor_name,
            verb_name=verb_name,
            token=token,
            is_implemented=is_implemented,
            impl_file=impl_file,
            impl_line=line_num,
            has_carbon_deps=has_carbon,
            uses_ui_adapter=uses_ui_adapter,
            platform_specific=platform_specific,
            complexity=complexity
        )

    def extract_verb_names_from_enum(self, source: str, processor_name: str) -> List[str]:
        """
        Extract verb names from the enum definition in the source file.

        The enum format is:
            enum {
                filv_created = 0,
                filv_modified = 1,
                ...
            };

        Args:
            source: Source code
            processor_name: Processor name (e.g., "file")

        Returns:
            List of verb names (without prefix)
        """
        # Find enum block
        enum_pattern = r'enum\s*\{([^}]+)\}'
        match = re.search(enum_pattern, source, re.DOTALL)

        if not match:
            return []

        enum_body = match.group(1)

        # Extract verb tokens (e.g., filv_created, filv_modified)
        # Pattern: processor_prefix + v_ + verb_name
        prefix = processor_name[0:3] + "v_"  # e.g., "filv_"

        # Find all enum entries
        pattern = rf'{re.escape(prefix)}(\w+)\s*='
        matches = re.findall(pattern, enum_body)

        return matches

    def analyze_processor(self, processor_name: str, verb_count: int) -> List[VerbImplementation]:
        """
        Analyze all verbs in a processor.

        Args:
            processor_name: Name of the processor
            verb_count: Number of verbs in this processor

        Returns:
            List of VerbImplementation records
        """
        # Find implementation file
        impl_file = self.find_implementation_file(processor_name)

        if not impl_file:
            print(f"Warning: No implementation file found for processor '{processor_name}'")
            # Return stub records for all verbs
            return [
                VerbImplementation(
                    processor=processor_name,
                    verb_name=f"verb{i}",
                    token=i,
                    is_implemented=False,
                    impl_file="",
                    impl_line=0,
                    has_carbon_deps=False,
                    uses_ui_adapter=False,
                    platform_specific=False,
                    complexity=1
                )
                for i in range(verb_count)
            ]

        # Read source file
        source = self.read_source_file(impl_file)

        if not source:
            print(f"Warning: Could not read source file: {impl_file}")
            return []

        # Extract verb names from enum
        verb_names = self.extract_verb_names_from_enum(source, processor_name)

        if not verb_names:
            print(f"Warning: Could not extract verb names for {processor_name} (expected enum not found)")
            # Fall back to generic names
            verb_names = [f"verb{i}" for i in range(verb_count)]

        if len(verb_names) != verb_count:
            print(f"Warning: Verb count mismatch for {processor_name}: enum has {len(verb_names)}, RC has {verb_count}")
            # Pad or truncate to match
            if len(verb_names) < verb_count:
                verb_names.extend([f"verb{i}" for i in range(len(verb_names), verb_count)])
            else:
                verb_names = verb_names[:verb_count]

        implementations = []

        # Analyze each verb
        for i, verb_name in enumerate(verb_names):
            # Extract case implementation (if exists)
            case_label = f"{processor_name[0:3]}v_{verb_name}"
            case_source = self.extract_case_implementation(source, case_label)

            if case_source:
                impl = self.analyze_verb_implementation(
                    case_source, processor_name, verb_name, i, impl_file
                )
            else:
                # No case found - assume stub
                impl = VerbImplementation(
                    processor=processor_name,
                    verb_name=verb_name,
                    token=i,
                    is_implemented=False,
                    impl_file=impl_file,
                    impl_line=0,
                    has_carbon_deps=False,
                    uses_ui_adapter=False,
                    platform_specific=False,
                    complexity=1
                )

            implementations.append(impl)

        return implementations

    def analyze_all_processors(self) -> List[VerbImplementation]:
        """
        Analyze all processors and generate implementation records.

        Returns:
            List of all VerbImplementation records
        """
        all_implementations = []

        for processor in self.processors:
            print(f"Analyzing processor: {processor.name} ({processor.verb_count} verbs)")

            impls = self.analyze_processor(processor.name, processor.verb_count)
            all_implementations.extend(impls)

        return all_implementations


if __name__ == '__main__':
    # Simple test
    from parse_kernelverbs import EFPProcessor

    # Create test processors
    test_processors = [
        EFPProcessor("1000", "file", True, 86),
        EFPProcessor("1001", "frontier", False, 14),
    ]

    analyzer = VerbImplementationAnalyzer(test_processors)

    # Test file discovery
    print("File discovery test:")
    for proc in test_processors:
        file_path = analyzer.find_implementation_file(proc.name)
        print(f"  {proc.name}: {file_path}")

    print("\nAnalyzing processors...")
    implementations = analyzer.analyze_all_processors()

    print(f"\nTotal implementations found: {len(implementations)}")
    print(f"Implemented: {sum(1 for i in implementations if i.is_implemented)}")
    print(f"Stubbed: {sum(1 for i in implementations if not i.is_implemented)}")
