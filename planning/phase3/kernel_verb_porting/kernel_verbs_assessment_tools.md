# Kernel Verb Assessment Tools

## Status
- State: Planning
- Phase: Headless Runtime Completion
- Created: 2025-12-03
- Related: kernel_verb_port_architecture.md

## Purpose

This document provides the tools and scripts needed to perform the systematic assessment described in the architecture document. These tools will generate the inventory, categorize verbs, and identify dependencies.

## Tool 1: Inventory Generator

### Script: tools/analyze_kernel_verbs.py

```python
#!/usr/bin/env python3
"""
Parse kernelverbs.rc and generate comprehensive inventory.

Output: planning/phase3/kernel_verbs_inventory.json
"""

import re
import json
import os
import subprocess
from pathlib import Path
from typing import List, Dict, Any, Optional

class VerbProcessor:
    def __init__(self, name: str, window_required: bool):
        self.name = name
        self.window_required = window_required
        self.verbs: List[Dict[str, Any]] = []

    def add_verb(self, verb_name: str, token: int):
        self.verbs.append({
            "name": verb_name,
            "token": token,
            "full_name": f"{self.name}.{verb_name}"
        })

def parse_rc_file(rc_path: str) -> List[VerbProcessor]:
    """Parse kernelverbs.rc and extract all processors and verbs."""

    with open(rc_path, 'r') as f:
        content = f.read()

    processors = []

    # Pattern to match each resource block
    # Matches: 1000 /*comment*/ EFP DISCARDABLE BEGIN ... END
    block_pattern = r'(\d+)\s+/\*[^*]*\*/\s+EFP\s+DISCARDABLE\s+BEGIN(.*?)END'

    for match in re.finditer(block_pattern, content, re.DOTALL):
        resource_id = int(match.group(1))
        block_content = match.group(2)

        # Parse block content
        lines = [l.strip() for l in block_content.split('\n') if l.strip() and not l.strip().startswith('//')]

        # First line should be number of processors
        if not lines:
            continue

        try:
            num_processors = int(lines[0].rstrip(','))
        except ValueError:
            continue

        # Parse each processor in the block
        i = 1
        while i < len(lines):
            # Processor name (in quotes)
            name_match = re.match(r'"([^"]+)"', lines[i])
            if not name_match:
                i += 1
                continue

            processor_name = name_match.group(1).replace('\\0', '')
            i += 1

            # Window required flag
            if i >= len(lines):
                break
            window_required = 'true' in lines[i].lower()
            i += 1

            # Verb count
            if i >= len(lines):
                break
            count_match = re.match(r'(\d+)', lines[i])
            if not count_match:
                i += 1
                continue
            verb_count = int(count_match.group(1))
            i += 1

            # Create processor
            processor = VerbProcessor(processor_name, window_required)

            # Extract verbs
            for token in range(verb_count):
                if i >= len(lines):
                    break
                verb_match = re.match(r'"([^"]+)"', lines[i])
                if verb_match:
                    verb_name = verb_match.group(1).replace('\\0', '')
                    processor.add_verb(verb_name, token)
                i += 1

            processors.append(processor)

    return processors

def find_implementation_file(processor_name: str, base_path: str) -> Optional[str]:
    """Search for the implementation file for a processor."""

    # Common patterns for implementation files
    search_patterns = [
        f"*{processor_name}verbs*.c",
        f"*{processor_name}*.c",
        f"lang*.c",  # Many processors in langverbs.c
        f"shell*.c", # Some in shellsysverbs.c
    ]

    search_dirs = [
        os.path.join(base_path, "Common/source"),
        os.path.join(base_path, "tests"),
    ]

    for search_dir in search_dirs:
        for pattern in search_patterns:
            try:
                result = subprocess.run(
                    ["find", search_dir, "-name", pattern, "-type", "f"],
                    capture_output=True,
                    text=True,
                    timeout=5
                )
                if result.returncode == 0 and result.stdout.strip():
                    files = result.stdout.strip().split('\n')

                    # Check if file contains processor reference
                    for file_path in files:
                        with open(file_path, 'r', errors='ignore') as f:
                            content = f.read()
                            # Look for function that might handle this processor
                            if processor_name in content or f"{processor_name}func" in content:
                                return file_path
            except Exception:
                continue

    return None

def categorize_verb(processor_name: str, verb_name: str, window_required: bool) -> str:
    """Categorize a verb based on processor and characteristics."""

    # Category A: Portable (pure computation)
    portable_processors = {
        "string", "clock", "date", "point", "rectangle", "rgb",
        "bit", "semaphore", "base64", "math", "crypt"
    }

    # Category B: Platform-specific (requires abstraction)
    platform_processors = {
        "file", "tcp", "sys", "launch", "thread"
    }

    # Category C: UI-dependent
    ui_processors = {
        "op", "table", "menu", "wp", "pict", "dialog", "kb",
        "mouse", "target", "window", "search", "filemenu",
        "editmenu", "mainwindow", "htmlcontrol", "statusbar"
    }

    # Category D: Optional
    optional_processors = {
        "python", "dll", "rez", "winregistry", "sqlite", "mysql", "re"
    }

    if processor_name in portable_processors:
        return "A_PORTABLE"
    elif processor_name in platform_processors:
        return "B_PLATFORM_SPECIFIC"
    elif processor_name in ui_processors or window_required:
        return "C_UI_DEPENDENT"
    elif processor_name in optional_processors:
        return "D_OPTIONAL"
    else:
        # Mixed processors like 'lang', 'frontier', 'html', 'xml', 'db'
        # Need manual review
        return "REVIEW_NEEDED"

def check_ui_dependencies(file_path: str) -> Dict[str, Any]:
    """Check a source file for UI/Carbon dependencies."""

    if not file_path or not os.path.exists(file_path):
        return {"has_deps": False, "calls": []}

    with open(file_path, 'r', errors='ignore') as f:
        content = f.read()

    # Patterns to search for
    ui_patterns = [
        r'#include\s*<Carbon',
        r'#include\s*<QuickDraw',
        r'#include\s*<Menus',
        r'#include\s*<Dialogs',
        r'#include\s*<Windows',
        r'WindowPtr',
        r'GrafPtr',
        r'MenuRef',
        r'DialogRef',
        r'ControlRef',
        r'EventRecord',
        r'GetNextEvent',
        r'DrawMenuBar',
        r'GetNewDialog',
        r'NewWindow',
        r'ShowWindow',
    ]

    found_deps = []
    for pattern in ui_patterns:
        matches = re.finditer(pattern, content)
        for match in matches:
            found_deps.append(match.group(0))

    return {
        "has_deps": len(found_deps) > 0,
        "calls": list(set(found_deps))  # Unique calls
    }

def determine_implementation_status(processor_name: str, verb_name: str,
                                     impl_file: Optional[str]) -> str:
    """Determine if verb is implemented, stubbed, or missing."""

    if impl_file is None:
        return "MISSING"

    # Check if it's a headless stub file
    if "headless" in impl_file:
        # Check if it's a real implementation or just a stub
        with open(impl_file, 'r', errors='ignore') as f:
            content = f.read()

        # Look for the verb name in switch/case statements
        if f'"{verb_name}"' in content or f"case.*{verb_name}" in content:
            # Check if it's just returning false/true without logic
            if "return false" in content or "return true" in content:
                # Simple heuristic: if file is very short, probably a stub
                if len(content) < 500:
                    return "STUBBED"
            return "IMPLEMENTED"
        return "STUBBED"

    # Non-headless file, likely implemented
    return "IMPLEMENTED"

def assign_tier(category: str, processor_name: str, verb_name: str,
                startup_critical: set) -> int:
    """Assign implementation tier to a verb."""

    full_name = f"{processor_name}.{verb_name}"

    # Tier 0: Startup-critical
    if full_name in startup_critical:
        return 0

    # Tier 4: UI-only
    if category == "C_UI_DEPENDENT":
        return 4

    # Tier 3: Optional
    if category == "D_OPTIONAL":
        return 3

    # Tier 1: Core functionality
    core_processors = {"file", "string", "clock", "date", "lang", "db"}
    if processor_name in core_processors and category in ["A_PORTABLE", "B_PLATFORM_SPECIFIC"]:
        return 1

    # Tier 2: Extended functionality
    if category in ["A_PORTABLE", "B_PLATFORM_SPECIFIC"]:
        return 2

    # Default to tier 2
    return 2

def generate_inventory(rc_path: str, base_path: str,
                       startup_critical: set) -> Dict[str, Any]:
    """Generate complete inventory."""

    print("Parsing kernelverbs.rc...")
    processors = parse_rc_file(rc_path)
    print(f"Found {len(processors)} processors")

    inventory = {
        "metadata": {
            "generated_at": "2025-12-03",
            "source_file": rc_path,
            "total_processors": len(processors),
            "total_verbs": sum(len(p.verbs) for p in processors)
        },
        "processors": []
    }

    for processor in processors:
        print(f"Processing {processor.name} ({len(processor.verbs)} verbs)...")

        # Find implementation file
        impl_file = find_implementation_file(processor.name, base_path)
        if impl_file:
            impl_file = os.path.relpath(impl_file, base_path)
            print(f"  Found implementation: {impl_file}")

        # Check UI dependencies
        ui_deps = check_ui_dependencies(
            os.path.join(base_path, impl_file) if impl_file else None
        )

        processor_data = {
            "name": processor.name,
            "window_required": processor.window_required,
            "impl_file": impl_file,
            "has_ui_deps": ui_deps["has_deps"],
            "ui_calls": ui_deps["calls"],
            "verb_count": len(processor.verbs),
            "verbs": []
        }

        for verb in processor.verbs:
            category = categorize_verb(processor.name, verb["name"],
                                      processor.window_required)
            status = determine_implementation_status(processor.name,
                                                    verb["name"], impl_file)
            tier = assign_tier(category, processor.name, verb["name"],
                              startup_critical)

            verb_data = {
                "name": verb["name"],
                "full_name": verb["full_name"],
                "token": verb["token"],
                "category": category,
                "status": status,
                "tier": tier
            }

            processor_data["verbs"].append(verb_data)

        inventory["processors"].append(processor_data)

    # Calculate statistics
    stats = calculate_statistics(inventory)
    inventory["statistics"] = stats

    return inventory

def calculate_statistics(inventory: Dict[str, Any]) -> Dict[str, Any]:
    """Calculate summary statistics."""

    total_verbs = 0
    by_category = {"A_PORTABLE": 0, "B_PLATFORM_SPECIFIC": 0,
                   "C_UI_DEPENDENT": 0, "D_OPTIONAL": 0, "REVIEW_NEEDED": 0}
    by_status = {"IMPLEMENTED": 0, "STUBBED": 0, "MISSING": 0}
    by_tier = {0: 0, 1: 0, 2: 0, 3: 0, 4: 0}

    for processor in inventory["processors"]:
        for verb in processor["verbs"]:
            total_verbs += 1
            by_category[verb["category"]] += 1
            by_status[verb["status"]] += 1
            by_tier[verb["tier"]] += 1

    return {
        "total_verbs": total_verbs,
        "by_category": by_category,
        "by_status": by_status,
        "by_tier": by_tier
    }

def main():
    """Main entry point."""

    # Get paths
    script_dir = Path(__file__).parent
    base_path = script_dir / "../.."
    rc_path = base_path / "Common/resources/Win32/kernelverbs.rc"
    output_path = script_dir / "kernel_verbs_inventory.json"

    # Startup-critical verbs (will be expanded during assessment)
    startup_critical = {
        "frontier.getFilePath",
        "frontier.getFilepath",  # Check for case variations
        "file.folderFromPath",
        "file.folderfrompath",
        "clock.now",
    }

    # Generate inventory
    print("Generating kernel verb inventory...")
    inventory = generate_inventory(str(rc_path), str(base_path), startup_critical)

    # Write output
    with open(output_path, 'w') as f:
        json.dump(inventory, f, indent=2)

    print(f"\nInventory written to: {output_path}")
    print("\nSummary:")
    print(f"  Total processors: {inventory['metadata']['total_processors']}")
    print(f"  Total verbs: {inventory['metadata']['total_verbs']}")
    print("\nBy category:")
    for cat, count in inventory['statistics']['by_category'].items():
        print(f"  {cat}: {count}")
    print("\nBy status:")
    for status, count in inventory['statistics']['by_status'].items():
        print(f"  {status}: {count}")
    print("\nBy tier:")
    for tier, count in sorted(inventory['statistics']['by_tier'].items()):
        tier_name = ["Startup-critical", "Core", "Extended", "Optional", "UI-only"][tier]
        print(f"  Tier {tier} ({tier_name}): {count}")

if __name__ == "__main__":
    main()
```

### Usage

```bash
cd /Users/jake/dev/jsavin/Frontier
python3 tools/analyze_kernel_verbs.py
```

This will generate: `planning/phase3/kernel_verbs_inventory.json`

## Tool 2: Dependency Checker

### Script: tools/check_headless_deps.sh

```bash
#!/bin/bash
# Check for Carbon/UI dependencies in headless code

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$BASE_DIR"

echo "Checking for UI dependencies in headless code..."
echo "================================================"

HEADLESS_FILES=$(find tests -name "headless_*.c" -o -name "headless_*.h")
PORTABLE_FILES=$(find Common/source -name "*_portable*.c" -o -name "*_adapter*.c")

ALL_FILES="$HEADLESS_FILES $PORTABLE_FILES"

ERRORS=0

# Check for Carbon includes
echo -e "\n[1/6] Checking for Carbon framework includes..."
if echo "$ALL_FILES" | xargs grep -l "#include.*<Carbon" 2>/dev/null; then
    echo "❌ ERROR: Carbon includes found in headless code"
    ERRORS=$((ERRORS + 1))
else
    echo "✓ No Carbon includes"
fi

# Check for QuickDraw includes
echo -e "\n[2/6] Checking for QuickDraw includes..."
if echo "$ALL_FILES" | xargs grep -l "#include.*<QuickDraw" 2>/dev/null; then
    echo "❌ ERROR: QuickDraw includes found"
    ERRORS=$((ERRORS + 1))
else
    echo "✓ No QuickDraw includes"
fi

# Check for UI types
echo -e "\n[3/6] Checking for UI type usage..."
UI_TYPES="WindowPtr\|GrafPtr\|MenuRef\|DialogRef\|ControlRef\|EventRecord"
if echo "$ALL_FILES" | xargs grep -l "$UI_TYPES" 2>/dev/null; then
    echo "❌ ERROR: UI types found in headless code"
    echo "$ALL_FILES" | xargs grep -n "$UI_TYPES" 2>/dev/null | head -10
    ERRORS=$((ERRORS + 1))
else
    echo "✓ No UI types"
fi

# Check for UI function calls
echo -e "\n[4/6] Checking for UI function calls..."
UI_FUNCS="GetNextEvent\|DrawMenuBar\|GetNewDialog\|NewWindow\|ShowWindow\|SetPort\|MoveTo\|LineTo"
if echo "$ALL_FILES" | xargs grep -l "$UI_FUNCS" 2>/dev/null; then
    echo "❌ ERROR: UI function calls found"
    echo "$ALL_FILES" | xargs grep -n "$UI_FUNCS" 2>/dev/null | head -10
    ERRORS=$((ERRORS + 1))
else
    echo "✓ No UI function calls"
fi

# Check for Window Manager calls
echo -e "\n[5/6] Checking for Window Manager calls..."
WIN_MGR="InitWindows\|GetNewWindow\|DisposeWindow\|SelectWindow\|HideWindow"
if echo "$ALL_FILES" | xargs grep -l "$WIN_MGR" 2>/dev/null; then
    echo "❌ ERROR: Window Manager calls found"
    ERRORS=$((ERRORS + 1))
else
    echo "✓ No Window Manager calls"
fi

# Check for Menu Manager calls
echo -e "\n[6/6] Checking for Menu Manager calls..."
MENU_MGR="NewMenu\|AppendMenu\|InsertMenu\|DeleteMenu\|DrawMenuBar"
if echo "$ALL_FILES" | xargs grep -l "$MENU_MGR" 2>/dev/null; then
    echo "❌ ERROR: Menu Manager calls found"
    ERRORS=$((ERRORS + 1))
else
    echo "✓ No Menu Manager calls"
fi

echo -e "\n================================================"
if [ $ERRORS -eq 0 ]; then
    echo "✓ All checks passed - no UI dependencies detected"
    exit 0
else
    echo "❌ Found $ERRORS categories of UI dependencies"
    exit 1
fi
```

### Usage

```bash
cd /Users/jake/dev/jsavin/Frontier
chmod +x tools/check_headless_deps.sh
./tools/check_headless_deps.sh
```

### Integration with Makefile

Add to Makefile:
```makefile
.PHONY: check-headless-deps
check-headless-deps:
	@./tools/check_headless_deps.sh
```

## Tool 3: Startup Script Analyzer

### Script: tools/analyze_startup_scripts.py

```python
#!/usr/bin/env python3
"""
Analyze system.startup scripts to identify verb dependencies.

Requires: frontier-cli to be built
"""

import re
import json
import subprocess
import sys
from pathlib import Path
from collections import defaultdict

def extract_verb_calls(script_text: str) -> list:
    """Extract all verb calls from UserTalk script."""

    # Pattern: processor.verb(...)
    # Matches: clock.now(), file.exists(), string.length(), etc.
    pattern = r'([a-z][a-z0-9]*)\s*\.\s*([a-zA-Z][a-zA-Z0-9]*)\s*\('

    matches = re.finditer(pattern, script_text, re.IGNORECASE)

    calls = []
    for match in matches:
        processor = match.group(1).lower()
        verb = match.group(2).lower()
        calls.append(f"{processor}.{verb}")

    return calls

def get_startup_scripts(db_path: str, cli_path: str) -> dict:
    """
    Extract startup scripts from database.

    This is a placeholder - actual implementation would need to:
    1. Use frontier-cli to query system.startup table
    2. Or directly read from database using the db reader
    """

    # For now, return known startup verbs based on error messages
    # This should be replaced with actual script extraction

    return {
        "system.startup.init": """
            frontier.getFilePath()
            file.folderFromPath(path)
            clock.now()
            string.length("test")
        """
    }

def analyze_dependencies(scripts: dict) -> dict:
    """Analyze all scripts and build dependency map."""

    verb_usage = defaultdict(int)
    script_deps = {}

    for script_name, script_text in scripts.items():
        calls = extract_verb_calls(script_text)
        script_deps[script_name] = calls

        for call in calls:
            verb_usage[call] += 1

    return {
        "verb_usage": dict(verb_usage),
        "script_dependencies": script_deps,
        "total_unique_verbs": len(verb_usage),
        "most_used_verbs": sorted(verb_usage.items(),
                                  key=lambda x: x[1],
                                  reverse=True)[:20]
    }

def main():
    """Main entry point."""

    if len(sys.argv) < 2:
        print("Usage: analyze_startup_scripts.py <path-to-database>")
        sys.exit(1)

    db_path = sys.argv[1]
    cli_path = "frontier-cli/frontier-cli"

    print(f"Analyzing startup scripts in: {db_path}")

    # Get scripts
    scripts = get_startup_scripts(db_path, cli_path)

    # Analyze
    analysis = analyze_dependencies(scripts)

    # Output
    output_path = Path("planning/phase3/startup_verb_dependencies.json")
    with open(output_path, 'w') as f:
        json.dump(analysis, f, indent=2)

    print(f"\nAnalysis written to: {output_path}")
    print(f"\nTotal unique verbs used: {analysis['total_unique_verbs']}")
    print("\nMost frequently used verbs:")
    for verb, count in analysis['most_used_verbs'][:10]:
        print(f"  {verb}: {count} calls")

if __name__ == "__main__":
    main()
```

## Tool 4: Implementation Progress Tracker

### Script: tools/track_verb_progress.py

```python
#!/usr/bin/env python3
"""
Track implementation progress of kernel verbs.
"""

import json
import sys
from pathlib import Path
from datetime import datetime

def load_inventory(path: str) -> dict:
    """Load verb inventory."""
    with open(path, 'r') as f:
        return json.load(f)

def generate_report(inventory: dict) -> str:
    """Generate markdown progress report."""

    stats = inventory['statistics']

    report = f"""# Kernel Verb Implementation Progress

**Generated**: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}

## Overall Statistics

- **Total Verbs**: {stats['total_verbs']}
- **Implemented**: {stats['by_status']['IMPLEMENTED']} ({stats['by_status']['IMPLEMENTED'] * 100 // stats['total_verbs']}%)
- **Stubbed**: {stats['by_status']['STUBBED']} ({stats['by_status']['STUBBED'] * 100 // stats['total_verbs']}%)
- **Missing**: {stats['by_status']['MISSING']} ({stats['by_status']['MISSING'] * 100 // stats['total_verbs']}%)

## Progress by Tier

| Tier | Description | Total | Implemented | Remaining |
|------|-------------|-------|-------------|-----------|
"""

    tier_names = ["Startup-critical", "Core", "Extended", "Optional", "UI-only"]
    for tier in range(5):
        name = tier_names[tier]
        total = stats['by_tier'][tier]

        # Calculate implemented in this tier
        implemented = 0
        for proc in inventory['processors']:
            for verb in proc['verbs']:
                if verb['tier'] == tier and verb['status'] == 'IMPLEMENTED':
                    implemented += 1

        remaining = total - implemented
        report += f"| {tier} | {name} | {total} | {implemented} | {remaining} |\n"

    report += "\n## Progress by Category\n\n"
    report += "| Category | Total | Implemented | Percentage |\n"
    report += "|----------|-------|-------------|------------|\n"

    for cat, total in stats['by_category'].items():
        implemented = 0
        for proc in inventory['processors']:
            for verb in proc['verbs']:
                if verb['category'] == cat and verb['status'] == 'IMPLEMENTED':
                    implemented += 1

        pct = (implemented * 100 // total) if total > 0 else 0
        report += f"| {cat} | {total} | {implemented} | {pct}% |\n"

    report += "\n## Processor Status\n\n"
    report += "| Processor | Total Verbs | Implemented | Status |\n"
    report += "|-----------|-------------|-------------|--------|\n"

    for proc in inventory['processors']:
        total = len(proc['verbs'])
        implemented = sum(1 for v in proc['verbs'] if v['status'] == 'IMPLEMENTED')
        pct = (implemented * 100 // total) if total > 0 else 0

        if pct == 100:
            status = "✓ Complete"
        elif pct > 50:
            status = "⚠ In Progress"
        elif pct > 0:
            status = "🔨 Started"
        else:
            status = "❌ Not Started"

        report += f"| {proc['name']} | {total} | {implemented} ({pct}%) | {status} |\n"

    return report

def main():
    """Main entry point."""

    inventory_path = Path("planning/phase3/kernel_verbs_inventory.json")

    if not inventory_path.exists():
        print(f"Error: {inventory_path} not found")
        print("Run tools/analyze_kernel_verbs.py first")
        sys.exit(1)

    inventory = load_inventory(str(inventory_path))
    report = generate_report(inventory)

    output_path = Path("planning/phase3/kernel_verbs_progress.md")
    with open(output_path, 'w') as f:
        f.write(report)

    print(f"Progress report written to: {output_path}")

    # Also print to console
    print("\n" + report)

if __name__ == "__main__":
    main()
```

## Quick Start Guide

### Step 1: Generate Inventory
```bash
cd /Users/jake/dev/jsavin/Frontier
python3 tools/analyze_kernel_verbs.py
```

This creates `planning/phase3/kernel_verbs_inventory.json` with complete verb categorization.

### Step 2: Check Dependencies
```bash
./tools/check_headless_deps.sh
```

Verifies no UI dependencies in headless code. Run before each commit.

### Step 3: Track Progress
```bash
python3 tools/track_verb_progress.py
```

Generates `planning/phase3/kernel_verbs_progress.md` showing implementation status.

### Step 4: Analyze Startup Dependencies (Manual)
```bash
# Extract startup scripts from database
./frontier-cli/frontier-cli --system-root databases/Frontier-v7.root \
    -e "db.getvalue('system.startup')"

# Manually review for verb calls
# Update startup_critical set in analyze_kernel_verbs.py
```

## Integration with Development Workflow

### Pre-commit Hook

Create `.git/hooks/pre-commit`:
```bash
#!/bin/bash
# Pre-commit hook to check for UI dependencies

if git diff --cached --name-only | grep -q "tests/headless_.*\.c\|.*_portable.*\.c"; then
    echo "Checking headless files for UI dependencies..."
    if ! ./tools/check_headless_deps.sh; then
        echo "❌ Pre-commit check failed"
        exit 1
    fi
fi

exit 0
```

### CI Integration (GitHub Actions)

```yaml
name: Headless Dependency Check

on: [push, pull_request]

jobs:
  check-deps:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Check UI Dependencies
        run: ./tools/check_headless_deps.sh
```

### Makefile Targets

Add to main Makefile:
```makefile
.PHONY: analyze-verbs check-deps progress-report

analyze-verbs:
	python3 tools/analyze_kernel_verbs.py

check-deps:
	./tools/check_headless_deps.sh

progress-report:
	python3 tools/track_verb_progress.py

verb-status: analyze-verbs progress-report
	@echo "Verb analysis complete"
```

## Expected Outputs

After running the analysis tools:

1. **kernel_verbs_inventory.json** (200-300 KB)
   - Complete list of all verbs
   - Categorization (A/B/C/D)
   - Implementation status
   - Tier assignments
   - Source file locations

2. **kernel_verbs_progress.md** (10-20 KB)
   - Summary statistics
   - Progress by tier
   - Processor status table
   - Visual progress indicators

3. **startup_verb_dependencies.json** (5-10 KB)
   - Verb usage frequency
   - Script-by-script dependencies
   - Priority ranking for implementation

These outputs guide the implementation phases described in the architecture document.

---

**Document Version**: 1.0
**Created**: 2025-12-03
**Status**: Ready for Use
