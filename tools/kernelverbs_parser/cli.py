#!/usr/bin/env python3
"""
Command-line interface for kernel verb analyzer.

Provides subcommands for analyzing verb implementations, generating reports,
and verifying whitelist consistency.
"""

import argparse
import sys
from pathlib import Path

# Add current directory to path for imports
sys.path.insert(0, str(Path(__file__).parent))

from parse_kernelverbs import parse_kernelverbs_rc
from analyzer import VerbImplementationAnalyzer
from metadata_writer import VerbMetadataWriter


def cmd_analyze(args):
    """
    Analyze all verb implementations and show results.
    """
    # Find project root
    script_dir = Path(__file__).parent
    project_root = script_dir.parent.parent
    rc_path = project_root / "Common/resources/Win32/kernelverbs.rc"

    if not rc_path.exists():
        print(f"ERROR: kernelverbs.rc not found at {rc_path}", file=sys.stderr)
        return 1

    # Parse RC file to get processor definitions
    print(f"Parsing {rc_path}...")
    processors, had_errors = parse_kernelverbs_rc(str(rc_path))

    if had_errors:
        print("WARNING: Some processors had errors during parsing", file=sys.stderr)

    print(f"Found {len(processors)} processors")

    # Run analyzer
    print("\nAnalyzing verb implementations...")
    analyzer = VerbImplementationAnalyzer(processors)
    implementations = analyzer.analyze_all_processors()

    # Create metadata writer
    writer = VerbMetadataWriter(implementations)

    # Get statistics
    stats = writer.get_statistics()

    print(f"\n=== Analysis Results ===")
    print(f"Total processors: {stats['total_processors']}")
    print(f"Total verbs analyzed: {stats['total_verbs']}")
    print(f"Detected as implemented: {stats['implemented_verbs']} ({100*stats['implemented_verbs']//stats['total_verbs']}%)")
    print(f"Detected as stubbed: {stats['stubbed_verbs']} ({100*stats['stubbed_verbs']//stats['total_verbs']}%)")
    print(f"UI adapters detected: {stats['ui_adapter_verbs']}")
    print(f"Carbon dependencies detected: {stats['carbon_dep_verbs']}")

    # Generate whitelist
    whitelist = writer.generate_whitelist()
    print(f"\nHeadless-compatible processors: {len(whitelist)}")
    print(f"Whitelist: {', '.join(sorted(whitelist))}")

    # Output JSON if requested
    if args.json:
        output_path = args.json
        writer.to_json(output_path)
        print(f"\nMetadata written to: {output_path}")

    return 0


def cmd_report(args):
    """
    Generate coverage report.
    """
    from datetime import datetime

    # Find project root
    script_dir = Path(__file__).parent
    project_root = script_dir.parent.parent
    rc_path = project_root / "Common/resources/Win32/kernelverbs.rc"

    if not rc_path.exists():
        print(f"ERROR: kernelverbs.rc not found at {rc_path}", file=sys.stderr)
        return 1

    # Parse RC file
    processors, had_errors = parse_kernelverbs_rc(str(rc_path))

    if had_errors:
        print("WARNING: Some processors had errors during parsing", file=sys.stderr)

    # Run analyzer
    analyzer = VerbImplementationAnalyzer(processors)
    implementations = analyzer.analyze_all_processors()

    # Generate report
    writer = VerbMetadataWriter(implementations)
    report = writer.generate_report()

    # Determine output filename
    if args.output:
        output_path = args.output
    else:
        # Generate date-tagged filename in reports/coverage/verb-binding/: YYYY-MM-DD-NN.md
        reports_dir = project_root / "reports/coverage/verb-binding"
        reports_dir.mkdir(parents=True, exist_ok=True)

        today = datetime.now().strftime("%Y-%m-%d")

        # Find next sequential number for today
        seq_num = 1
        while True:
            candidate = reports_dir / f"{today}-{seq_num:02d}.md"
            if not candidate.exists():
                output_path = str(candidate)
                break
            seq_num += 1
            if seq_num > 99:
                print(f"ERROR: Too many reports for {today}", file=sys.stderr)
                return 1

    # Output to file or stdout
    if output_path == '-':
        print(report)
    else:
        with open(output_path, 'w') as f:
            f.write(report)
        print(f"Report written to: {output_path}")

    return 0


def cmd_dry_run(args):
    """
    Show what would change in HEADLESS_REGISTERED without applying.
    """
    # Find project root
    script_dir = Path(__file__).parent
    project_root = script_dir.parent.parent
    rc_path = project_root / "Common/resources/Win32/kernelverbs.rc"

    if not rc_path.exists():
        print(f"ERROR: kernelverbs.rc not found at {rc_path}", file=sys.stderr)
        return 1

    # Parse RC file
    processors, had_errors = parse_kernelverbs_rc(str(rc_path))

    # Run analyzer
    analyzer = VerbImplementationAnalyzer(processors)
    implementations = analyzer.analyze_all_processors()

    # Generate new whitelist
    writer = VerbMetadataWriter(implementations)
    new_whitelist = set(writer.generate_whitelist())

    # Load current whitelist from parse_kernelverbs.py
    from parse_kernelverbs import HEADLESS_REGISTERED
    current_whitelist = set(HEADLESS_REGISTERED)

    # Calculate diff
    to_add = new_whitelist - current_whitelist
    to_remove = current_whitelist - new_whitelist
    unchanged = new_whitelist & current_whitelist

    print("=== Dry-Run Mode: Whitelist Changes ===\n")

    if not to_add and not to_remove:
        print("✓ No changes needed - whitelist is up to date")
        return 0

    if to_add:
        print(f"Processors to ADD ({len(to_add)}):")
        for proc in sorted(to_add):
            print(f"  + {proc}")
        print()

    if to_remove:
        print(f"Processors to REMOVE ({len(to_remove)}):")
        for proc in sorted(to_remove):
            print(f"  - {proc}")
        print()

    print(f"Unchanged: {len(unchanged)} processors")
    print(f"\nTotal whitelist size: {len(current_whitelist)} → {len(new_whitelist)}")

    return 0


def cmd_verify(args):
    """
    Verify current HEADLESS_REGISTERED matches analyzer output.
    """
    # Find project root
    script_dir = Path(__file__).parent
    project_root = script_dir.parent.parent
    rc_path = project_root / "Common/resources/Win32/kernelverbs.rc"

    if not rc_path.exists():
        print(f"ERROR: kernelverbs.rc not found at {rc_path}", file=sys.stderr)
        return 1

    # Parse RC file
    processors, had_errors = parse_kernelverbs_rc(str(rc_path))

    # Run analyzer
    analyzer = VerbImplementationAnalyzer(processors)
    implementations = analyzer.analyze_all_processors()

    # Generate expected whitelist
    writer = VerbMetadataWriter(implementations)
    expected_whitelist = set(writer.generate_whitelist())

    # Load current whitelist
    from parse_kernelverbs import HEADLESS_REGISTERED
    current_whitelist = set(HEADLESS_REGISTERED)

    # Check consistency
    if expected_whitelist == current_whitelist:
        print("✓ PASS: HEADLESS_REGISTERED matches analyzer output")
        return 0
    else:
        print("✗ FAIL: HEADLESS_REGISTERED is inconsistent with analyzer", file=sys.stderr)
        print("\nRun 'cli.py dry-run' to see differences", file=sys.stderr)

        # Show brief summary
        to_add = expected_whitelist - current_whitelist
        to_remove = current_whitelist - expected_whitelist

        if to_add:
            print(f"\nMissing {len(to_add)} processors: {', '.join(sorted(to_add))}", file=sys.stderr)
        if to_remove:
            print(f"\nExtra {len(to_remove)} processors: {', '.join(sorted(to_remove))}", file=sys.stderr)

        return 1


def main():
    parser = argparse.ArgumentParser(
        description="Kernel verb implementation analyzer",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Analyze all processors and show summary
  python3 cli.py analyze

  # Generate coverage report (auto-named with date)
  python3 cli.py report

  # Generate coverage report to specific file
  python3 cli.py report -o verb_coverage.md

  # Generate coverage report to stdout
  python3 cli.py report -o -

  # Check what would change (dry-run)
  python3 cli.py dry-run

  # Verify whitelist consistency
  python3 cli.py verify

  # Export metadata to JSON
  python3 cli.py analyze --json verb_metadata.json
"""
    )

    subparsers = parser.add_subparsers(dest='command', help='Subcommands')

    # analyze subcommand
    analyze_parser = subparsers.add_parser('analyze', help='Analyze verb implementations')
    analyze_parser.add_argument('--json', metavar='FILE', help='Export metadata to JSON file')
    analyze_parser.set_defaults(func=cmd_analyze)

    # report subcommand
    report_parser = subparsers.add_parser('report', help='Generate coverage report')
    report_parser.add_argument('-o', '--output', metavar='FILE', help='Output file (default: auto-generate COVERAGE_REPORT-YYYY-MM-DD-NN.md, or use "-" for stdout)')
    report_parser.set_defaults(func=cmd_report)

    # dry-run subcommand
    dryrun_parser = subparsers.add_parser('dry-run', help='Show whitelist changes without applying')
    dryrun_parser.set_defaults(func=cmd_dry_run)

    # verify subcommand
    verify_parser = subparsers.add_parser('verify', help='Verify whitelist consistency')
    verify_parser.set_defaults(func=cmd_verify)

    args = parser.parse_args()

    if not args.command:
        parser.print_help()
        return 1

    return args.func(args)


if __name__ == '__main__':
    sys.exit(main())
