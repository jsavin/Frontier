#!/usr/bin/env python3
"""
Metadata writer for kernel verb implementation analysis.

This module provides data structures and serialization for tracking
the implementation status of kernel verbs across all processors.
"""

from dataclasses import dataclass, asdict
from typing import List, Dict, Optional
import json


@dataclass
class VerbImplementation:
    """
    Metadata about a single kernel verb implementation.

    Fields:
        processor: Processor name (e.g., "file", "frontier")
        verb_name: Verb name (e.g., "exists", "compile")
        token: Token index in the processor's verb table
        is_implemented: True if real implementation, False if stub
        impl_file: Source file path containing the implementation
        impl_line: Line number where implementation starts
        has_carbon_deps: True if uses Carbon/QuickDraw APIs
        uses_ui_adapter: True if uses UI adapter pattern
        platform_specific: True if contains platform-specific code
        complexity: Estimated complexity (1-5 scale)
    """
    processor: str
    verb_name: str
    token: int
    is_implemented: bool
    impl_file: str
    impl_line: int
    has_carbon_deps: bool
    uses_ui_adapter: bool
    platform_specific: bool
    complexity: int

    def to_dict(self) -> Dict:
        """Convert to dictionary for JSON serialization."""
        return asdict(self)

    @classmethod
    def from_dict(cls, data: Dict) -> 'VerbImplementation':
        """Create VerbImplementation from dictionary."""
        return cls(**data)

    def __str__(self) -> str:
        """Human-readable representation."""
        status = "IMPL" if self.is_implemented else "STUB"
        flags = []
        if self.has_carbon_deps:
            flags.append("Carbon")
        if self.uses_ui_adapter:
            flags.append("UIAdapter")
        if self.platform_specific:
            flags.append("Platform")

        flag_str = f" [{', '.join(flags)}]" if flags else ""
        return f"{self.processor}.{self.verb_name} [{status}]{flag_str} @ {self.impl_file}:{self.impl_line}"


class VerbMetadataWriter:
    """
    Writes verb implementation metadata to various formats.
    """

    def __init__(self, verbs: List[VerbImplementation]):
        """
        Initialize with a list of verb implementations.

        Args:
            verbs: List of VerbImplementation records
        """
        self.verbs = verbs

    def to_json(self, filepath: str, indent: int = 2):
        """
        Write metadata to JSON file.

        Args:
            filepath: Output JSON file path
            indent: JSON indentation (default: 2)
        """
        data = {
            "version": "1.0",
            "total_verbs": len(self.verbs),
            "verbs": [v.to_dict() for v in self.verbs]
        }

        with open(filepath, 'w') as f:
            json.dump(data, f, indent=indent)

    @classmethod
    def from_json(cls, filepath: str) -> 'VerbMetadataWriter':
        """
        Load metadata from JSON file.

        Args:
            filepath: Input JSON file path

        Returns:
            VerbMetadataWriter instance
        """
        with open(filepath, 'r') as f:
            data = json.load(f)

        verbs = [VerbImplementation.from_dict(v) for v in data['verbs']]
        return cls(verbs)

    def generate_whitelist(self) -> List[str]:
        """
        Generate HEADLESS_REGISTERED whitelist from implemented verbs.

        Returns:
            List of processor names that have at least one C-implemented verb
            and are headless-compatible (no Carbon dependencies).

        Note:
            Processors where ALL verbs are script-implemented (impl_file contains
            "<UserTalk script>") are EXCLUDED. Such processors have no C code to
            dispatch to - the database table (builtins.*) already has the full
            implementation. Registering an EFP for them would shadow the database
            table, causing defined(processor.item) to fail for items not in the
            EFP stub.

            Example: webserver has 7 verbs, all script-implemented. Without this
            exclusion, defined(webserver.init) fails because the EFP stub (7 items)
            shadows builtins.webserver (21 items including 'init').
        """
        # Group verbs by processor
        by_processor = {}
        for verb in self.verbs:
            if verb.processor not in by_processor:
                by_processor[verb.processor] = []
            by_processor[verb.processor].append(verb)

        whitelist = []
        for processor, verbs in sorted(by_processor.items()):
            # Check if processor has any C-implemented verbs (not just script-implemented)
            # A verb is C-implemented if is_implemented=True AND impl_file is NOT "<UserTalk script>"
            has_c_impl = any(
                v.is_implemented and v.impl_file != "<UserTalk script>"
                for v in verbs
            )

            # Check if processor has Carbon dependencies
            has_carbon = any(v.has_carbon_deps for v in verbs if v.is_implemented)

            # Include only if has C implementation and no Carbon dependencies
            # Processors with ONLY script-implemented verbs should NOT be registered
            # (they use the database table builtins.* instead of EFP)
            if has_c_impl and not has_carbon:
                whitelist.append(processor)

        return whitelist

    def generate_report(self) -> str:
        """
        Generate human-readable markdown status report.

        Returns:
            Markdown-formatted status report
        """
        # Group by processor
        by_processor = {}
        for verb in self.verbs:
            if verb.processor not in by_processor:
                by_processor[verb.processor] = []
            by_processor[verb.processor].append(verb)

        lines = ["# Automatic Verb Binding - Coverage Analysis", ""]
        lines.append("*This report analyzes the implementation status of Frontier's 707 kernel verbs")
        lines.append("across 51 processors. It detects which verbs are implemented vs. stubbed.*")
        lines.append("")

        # Summary statistics
        total = len(self.verbs)
        detected = sum(1 for v in self.verbs if v.is_implemented)
        stubbed = total - detected
        ui_adapter = sum(1 for v in self.verbs if v.uses_ui_adapter)
        carbon_deps = sum(1 for v in self.verbs if v.has_carbon_deps)

        lines.append("## Summary")
        lines.append("")
        lines.append(f"- **Total verbs analyzed:** {total}")
        lines.append(f"- **Detected as implemented:** {detected} ({100*detected//total}%)")
        lines.append(f"- **Detected as stubbed:** {stubbed} ({100*stubbed//total}%)")
        lines.append(f"- **UI adapters detected:** {ui_adapter}")
        lines.append(f"- **Carbon API dependencies detected:** {carbon_deps}")
        lines.append("")

        # Per-processor summary table
        lines.append("## Processor Coverage Summary")
        lines.append("")
        lines.append("| Processor | Total Verbs | Detected (%) | Stubbed (%) | Missing (%) |")
        lines.append("|-----------|-------------|--------------|-------------|-------------|")

        for processor in sorted(by_processor.keys()):
            verbs = by_processor[processor]
            proc_total = len(verbs)
            proc_detected = sum(1 for v in verbs if v.is_implemented)
            proc_stubbed = sum(1 for v in verbs if not v.is_implemented)
            # Note: For now, "missing" is 0 since we detect all verbs from RC file
            # This placeholder allows future enhancement if needed
            proc_missing = 0

            detected_pct = 100 * proc_detected // proc_total if proc_total > 0 else 0
            stubbed_pct = 100 * proc_stubbed // proc_total if proc_total > 0 else 0
            missing_pct = 100 * proc_missing // proc_total if proc_total > 0 else 0

            lines.append(f"| {processor} | {proc_total} | {detected_pct}% ({proc_detected}) | {stubbed_pct}% ({proc_stubbed}) | {missing_pct}% ({proc_missing}) |")

        lines.append("")
        lines.append("## Detailed Processor Coverage")
        lines.append("")

        # Detailed per-processor breakdown
        lines.append("| Processor | Total | Detected | Stubbed | UI Adapter | Carbon | Status |")
        lines.append("|-----------|-------|----------|---------|------------|--------|--------|")

        for processor in sorted(by_processor.keys()):
            verbs = by_processor[processor]
            proc_total = len(verbs)
            proc_detected = sum(1 for v in verbs if v.is_implemented)
            proc_stubbed = proc_total - proc_detected
            proc_ui = sum(1 for v in verbs if v.uses_ui_adapter)
            proc_carbon = sum(1 for v in verbs if v.has_carbon_deps)

            if proc_detected == 0:
                status = "Not Started"
            elif proc_stubbed == 0:
                status = "Complete"
            else:
                status = f"{100*proc_detected//proc_total}%"

            lines.append(f"| {processor} | {proc_total} | {proc_detected} | {proc_stubbed} | {proc_ui} | {proc_carbon} | {status} |")

        lines.append("")

        # Add section listing specific stubbed verbs per processor
        lines.append("## Stubbed Verbs by Processor")
        lines.append("")
        lines.append("This section lists the specific verbs that are currently stubbed (not implemented) in each processor.")
        lines.append("")

        has_stubbed_verbs = False
        for processor in sorted(by_processor.keys()):
            verbs = by_processor[processor]
            stubbed_verbs = [v for v in verbs if not v.is_implemented]

            if stubbed_verbs:
                has_stubbed_verbs = True
                lines.append(f"### {processor} ({len(stubbed_verbs)} stubbed)")
                lines.append("")
                for verb in sorted(stubbed_verbs, key=lambda v: v.verb_name):
                    # Include additional context if available
                    details = []
                    if verb.uses_ui_adapter:
                        details.append("UI-dependent")
                    if verb.has_carbon_deps:
                        details.append("Carbon API")

                    detail_str = f" [{', '.join(details)}]" if details else ""
                    lines.append(f"- `{processor}.{verb.verb_name}`{detail_str}")
                lines.append("")

        if not has_stubbed_verbs:
            lines.append("*No stubbed verbs found - all processors are fully implemented!*")
            lines.append("")

        return "\n".join(lines)

    def get_statistics(self) -> Dict:
        """
        Get implementation statistics.

        Returns:
            Dictionary with various statistics
        """
        by_processor = {}
        for verb in self.verbs:
            if verb.processor not in by_processor:
                by_processor[verb.processor] = {
                    'total': 0,
                    'implemented': 0,
                    'stubbed': 0,
                    'ui_adapter': 0,
                    'carbon_deps': 0
                }

            stats = by_processor[verb.processor]
            stats['total'] += 1
            if verb.is_implemented:
                stats['implemented'] += 1
            else:
                stats['stubbed'] += 1
            if verb.uses_ui_adapter:
                stats['ui_adapter'] += 1
            if verb.has_carbon_deps:
                stats['carbon_deps'] += 1

        return {
            'total_verbs': len(self.verbs),
            'total_processors': len(by_processor),
            'implemented_verbs': sum(1 for v in self.verbs if v.is_implemented),
            'stubbed_verbs': sum(1 for v in self.verbs if not v.is_implemented),
            'ui_adapter_verbs': sum(1 for v in self.verbs if v.uses_ui_adapter),
            'carbon_dep_verbs': sum(1 for v in self.verbs if v.has_carbon_deps),
            'by_processor': by_processor
        }


if __name__ == '__main__':
    # Example usage
    example_verbs = [
        VerbImplementation(
            processor="file",
            verb_name="exists",
            token=0,
            is_implemented=True,
            impl_file="tests/headless_file_verbs.c",
            impl_line=42,
            has_carbon_deps=False,
            uses_ui_adapter=False,
            platform_specific=False,
            complexity=2
        ),
        VerbImplementation(
            processor="dialog",
            verb_name="ask",
            token=1,
            is_implemented=True,
            impl_file="Common/source/dialogverbs.c",
            impl_line=156,
            has_carbon_deps=False,
            uses_ui_adapter=True,
            platform_specific=False,
            complexity=3
        )
    ]

    writer = VerbMetadataWriter(example_verbs)
    print(writer.generate_report())
    print("\nWhitelist:", writer.generate_whitelist())
