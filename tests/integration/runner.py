#!/usr/bin/env python3
"""
Frontier Integration Test Runner

Executes YAML test cases against frontier-cli and validates results.
"""

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, Optional, Tuple

try:
    import yaml
except ImportError:
    print("Error: PyYAML is required. Install with: pip3 install pyyaml", file=sys.stderr)
    sys.exit(1)


class TestResult:
    """Result of a single test execution."""

    def __init__(self, name: str, passed: bool, error: Optional[str] = None, details: Optional[str] = None):
        self.name = name
        self.passed = passed
        self.error = error
        self.details = details


class FrontierCLI:
    """Wrapper for executing scripts via frontier-cli."""

    def __init__(self, cli_path: str, system_root: Optional[str] = None):
        self.cli_path = cli_path
        self.system_root = system_root

        if not os.path.exists(cli_path):
            raise FileNotFoundError(f"frontier-cli not found: {cli_path}")

    def execute(self, script: str, timeout: int = 10) -> Dict:
        """Execute a UserTalk script and return JSON result."""
        cmd = [self.cli_path, '--output-json', '-e', script]

        if self.system_root:
            cmd.extend(['--system-root', self.system_root])

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=timeout
            )

            # Parse JSON from stdout
            try:
                output = json.loads(result.stdout)
                output['exit_code'] = result.returncode
                return output
            except json.JSONDecodeError as e:
                return {
                    'success': False,
                    'result': None,
                    'result_type': None,
                    'error': f'Invalid JSON output: {e}',
                    'error_type': 'json_parse_error',
                    'exit_code': result.returncode,
                    'stdout': result.stdout,
                    'stderr': result.stderr
                }

        except subprocess.TimeoutExpired:
            return {
                'success': False,
                'result': None,
                'result_type': None,
                'error': f'Script execution timed out ({timeout}s)',
                'error_type': 'timeout',
                'exit_code': -1
            }

        except Exception as e:
            return {
                'success': False,
                'result': None,
                'result_type': None,
                'error': str(e),
                'error_type': 'execution_error',
                'exit_code': -1
            }


class TestCase:
    """Represents a single test case from YAML."""

    def __init__(self, data: Dict):
        self.name = data.get('name', 'Unnamed Test')
        self.script = data.get('script', '')
        self.expected_success = data.get('expected_success', True)
        self.expected_result = data.get('expected_result')
        self.expected_error_type = data.get('expected_error_type')
        self.description = data.get('description', '')

    def validate(self, output: Dict) -> Tuple[bool, Optional[str]]:
        """Validate test output against expectations."""

        # Check success/failure status
        if output.get('success') != self.expected_success:
            return False, f"Expected success={self.expected_success}, got success={output.get('success')}"

        # If expecting success, check result
        if self.expected_success and self.expected_result is not None:
            actual_result = output.get('result')
            if str(actual_result) != str(self.expected_result):
                return False, f"Expected result={self.expected_result!r}, got {actual_result!r}"

        # If expecting failure, check error type
        if not self.expected_success and self.expected_error_type:
            actual_error_type = output.get('error_type')
            if actual_error_type != self.expected_error_type:
                return False, f"Expected error_type={self.expected_error_type}, got {actual_error_type}"

        return True, None


class TestRunner:
    """Main test runner that executes test cases."""

    def __init__(self, cli: FrontierCLI, verbose: bool = False):
        self.cli = cli
        self.verbose = verbose
        self.results: List[TestResult] = []

    def load_test_file(self, yaml_path: str) -> List[TestCase]:
        """Load test cases from YAML file."""
        with open(yaml_path, 'r') as f:
            data = yaml.safe_load(f)

        test_cases = []
        for test_data in data.get('tests', []):
            test_cases.append(TestCase(test_data))

        return test_cases

    def run_test(self, test: TestCase) -> TestResult:
        """Run a single test case."""
        if self.verbose:
            print(f"  Running: {test.name}")
            if test.description:
                print(f"    {test.description}")

        # Execute script
        output = self.cli.execute(test.script)

        # Validate result
        passed, error = test.validate(output)

        details = None
        if not passed and self.verbose:
            details = f"Output: {json.dumps(output, indent=2)}"

        return TestResult(test.name, passed, error, details)

    def run_test_file(self, yaml_path: str) -> List[TestResult]:
        """Run all tests in a YAML file."""
        test_file = Path(yaml_path).name
        print(f"\nRunning tests from: {test_file}")

        test_cases = self.load_test_file(yaml_path)
        print(f"  Found {len(test_cases)} test(s)")

        file_results = []
        for test in test_cases:
            result = self.run_test(test)
            file_results.append(result)
            self.results.append(result)

            # Print immediate feedback
            status = "✓ PASS" if result.passed else "✗ FAIL"
            print(f"    {status}: {result.name}")
            if not result.passed:
                print(f"      Error: {result.error}")
                if result.details:
                    print(f"      {result.details}")

        return file_results

    def print_summary(self):
        """Print test summary."""
        total = len(self.results)
        passed = sum(1 for r in self.results if r.passed)
        failed = total - passed

        print("\n" + "=" * 70)
        print("TEST SUMMARY")
        print("=" * 70)
        print(f"Total:  {total}")
        print(f"Passed: {passed}")
        print(f"Failed: {failed}")

        if failed > 0:
            print("\nFailed tests:")
            for result in self.results:
                if not result.passed:
                    print(f"  - {result.name}: {result.error}")

        print("=" * 70)

        return failed == 0


def main():
    parser = argparse.ArgumentParser(description="Run Frontier integration tests")
    parser.add_argument('test_files', nargs='+', help='YAML test files to run')
    parser.add_argument('--cli', default='./frontier-cli/frontier-cli',
                       help='Path to frontier-cli binary')
    parser.add_argument('--system-root', help='Path to system root database')
    parser.add_argument('--verbose', '-v', action='store_true',
                       help='Verbose output')

    args = parser.parse_args()

    # Initialize CLI wrapper
    try:
        cli = FrontierCLI(args.cli, args.system_root)
    except FileNotFoundError as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1

    # Initialize test runner
    runner = TestRunner(cli, verbose=args.verbose)

    # Run all test files
    for test_file in args.test_files:
        if not os.path.exists(test_file):
            print(f"Warning: Test file not found: {test_file}", file=sys.stderr)
            continue

        runner.run_test_file(test_file)

    # Print summary and exit with appropriate code
    all_passed = runner.print_summary()
    return 0 if all_passed else 1


if __name__ == '__main__':
    sys.exit(main())
