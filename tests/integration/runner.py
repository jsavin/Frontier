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


# Type name aliases: Maps Frontier's internal type names to canonical test names
# Frontier uses shortened or internal names in JSON output that differ from
# the type names used in UserTalk and test expectations.
TYPE_ALIASES = {
    'addr': 'address',      # Address type
    'data': 'binary',       # Binary data type
    'fss ': 'filespec',     # Filespec type (note trailing space in Frontier output)
    'fss': 'filespec',      # Filespec type (without trailing space)
}


def normalize_type_name(type_name: Optional[str]) -> Optional[str]:
    """
    Normalize a Frontier type name to its canonical form.

    Args:
        type_name: Type name from Frontier JSON output

    Returns:
        Canonical type name, or original if no alias exists
    """
    if type_name is None:
        return None
    return TYPE_ALIASES.get(type_name, type_name)


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

    def execute(self, script: str, timeout: int = 10, stdin_input: Optional[str] = None,
                batch_mode: bool = False, env: Optional[Dict[str, str]] = None) -> Dict:
        """
        Execute a UserTalk script and return JSON result.

        Args:
            script: UserTalk script to execute
            timeout: Execution timeout in seconds
            stdin_input: Optional stdin input for interactive prompts
            batch_mode: If True, add --batch flag to disable interactive mode
            env: Optional environment variables to set
        """
        cmd = [self.cli_path, '--output-json', '-e', script]

        if self.system_root:
            cmd.extend(['--system-root', self.system_root])

        if batch_mode:
            cmd.append('--batch')

        # Merge environment variables with current environment
        process_env = os.environ.copy()
        if env:
            process_env.update(env)

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=timeout,
                input=stdin_input,  # Pass stdin input if provided
                env=process_env
            )

            # Parse JSON from stdout (stderr contains prompts and logs)
            # When dialog prompts are active, stderr contains prompt output
            # stdout contains the clean JSON result
            try:
                stdout_lines = result.stdout
                # Find last occurrence of '{\n  "success"' which marks start of JSON
                json_start = stdout_lines.rfind('{\n  "success"')
                if json_start == -1:
                    # Fallback: try to parse entire stdout as JSON
                    json_text = stdout_lines
                else:
                    json_text = stdout_lines[json_start:]

                output = json.loads(json_text)
                output['exit_code'] = result.returncode
                output['stderr'] = result.stderr  # Preserve stderr for prompts/logs
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
        self.expected_result = data.get('expected_result') or data.get('expected_output')  # Support both names
        self.expected_result_type = data.get('expected_result_type')
        self.expected_error_type = data.get('expected_error_type')
        self.expected_contains = data.get('expected_contains')  # List of strings that should be in result
        self.expected_pattern = data.get('expected_pattern')    # Regex pattern to match result
        self.expected_error_contains = data.get('expected_error_contains')  # String that should be in error
        self.description = data.get('description', '')
        self.timeout = data.get('timeout', 10)  # Default 10 seconds, configurable per test
        self.stdin_input = data.get('stdin_input')  # Optional stdin input for interactive tests
        self.batch_mode = data.get('batch_mode', False)  # Set true to test batch mode error behavior
        self.environment = data.get('environment', {})  # Optional environment variables

    def get_script_with_substitutions(self, test_root_dir: Optional[str] = None) -> str:
        """Get the script with path substitutions applied."""
        script = self.script

        # Substitute test directory paths
        if test_root_dir:
            test_tmp_dir = os.path.join(test_root_dir, 'tmp', 'integration')
            # Ensure tmp directory exists for tests
            os.makedirs(test_tmp_dir, exist_ok=True)
            script = script.replace('{FRONTIER_TEST_TMP_DIR}', test_tmp_dir)

        return script

    def get_stdin_with_substitutions(self, test_root_dir: Optional[str] = None) -> Optional[str]:
        """Get the stdin input with path substitutions applied."""
        if self.stdin_input is None:
            return None

        stdin_input = self.stdin_input

        # Substitute test directory paths
        if test_root_dir:
            test_tmp_dir = os.path.join(test_root_dir, 'tmp', 'integration')
            stdin_input = stdin_input.replace('{FRONTIER_TEST_TMP_DIR}', test_tmp_dir)

        return stdin_input

    def validate(self, output: Dict) -> Tuple[bool, Optional[str]]:
        """Validate test output against expectations."""
        import re

        # Check success/failure status
        if output.get('success') != self.expected_success:
            return False, f"Expected success={self.expected_success}, got success={output.get('success')}"

        # If expecting success, check result
        if self.expected_success and self.expected_result is not None:
            actual_result = output.get('result')
            if str(actual_result) != str(self.expected_result):
                return False, f"Expected result={self.expected_result!r}, got {actual_result!r}"

        # Check if result contains all expected strings
        if self.expected_success and self.expected_contains is not None:
            actual_result = str(output.get('result', ''))
            for expected_string in self.expected_contains:
                if expected_string not in actual_result:
                    return False, f"Expected result to contain {expected_string!r}, but got {actual_result!r}"

        # Check if result matches expected pattern (regex)
        if self.expected_success and self.expected_pattern is not None:
            actual_result = str(output.get('result', ''))
            if not re.search(self.expected_pattern, actual_result):
                return False, f"Expected result to match pattern {self.expected_pattern!r}, but got {actual_result!r}"

        # Check result type if specified
        if self.expected_success and self.expected_result_type is not None:
            actual_result_type = output.get('result_type')
            # Normalize both types for comparison (handles Frontier's internal type names)
            normalized_actual = normalize_type_name(actual_result_type)
            normalized_expected = normalize_type_name(self.expected_result_type)
            if normalized_actual != normalized_expected:
                return False, f"Expected result_type={self.expected_result_type}, got {actual_result_type}"

        # If expecting failure, check error type
        if not self.expected_success and self.expected_error_type:
            actual_error_type = output.get('error_type')
            if actual_error_type != self.expected_error_type:
                return False, f"Expected error_type={self.expected_error_type}, got {actual_error_type}"

        # If expecting failure, check error contains string
        if not self.expected_success and self.expected_error_contains:
            actual_error = str(output.get('error', ''))
            if self.expected_error_contains not in actual_error:
                return False, f"Expected error to contain {self.expected_error_contains!r}, but got {actual_error!r}"

        return True, None


class TestRunner:
    """Main test runner that executes test cases."""

    def __init__(self, cli: FrontierCLI, verbose: bool = False, test_root_dir: Optional[str] = None):
        self.cli = cli
        self.verbose = verbose
        self.test_root_dir = test_root_dir or str(Path.cwd())
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

        # Get script with path substitutions applied
        script = test.get_script_with_substitutions(self.test_root_dir)

        # Get stdin input with path substitutions applied
        stdin_input = test.get_stdin_with_substitutions(self.test_root_dir)

        # Prepare environment variables
        test_env = test.environment.copy()

        # If test provides stdin_input and isn't in batch mode, force interactive mode
        # This overrides TTY detection which fails when stdin is piped
        if stdin_input is not None and not test.batch_mode:
            test_env['FRONTIER_FORCE_INTERACTIVE'] = '1'

        # Execute script with test-specific timeout, stdin input, batch mode, and environment
        output = self.cli.execute(
            script,
            timeout=test.timeout,
            stdin_input=stdin_input,
            batch_mode=test.batch_mode,
            env=test_env
        )

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

    def cleanup_test_artifacts(self):
        """Clean up temporary test files and directories created during test execution."""
        import shutil
        test_tmp_dir = os.path.join(self.test_root_dir, 'tmp', 'integration')
        if os.path.exists(test_tmp_dir):
            try:
                shutil.rmtree(test_tmp_dir)
                if self.verbose:
                    print(f"\nCleaned up test artifacts: {test_tmp_dir}")
            except Exception as e:
                print(f"Warning: Failed to clean up test artifacts: {e}", file=sys.stderr)

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

    # Determine test root directory (project root)
    # This is used for path substitutions like {FRONTIER_TEST_TMP_DIR}
    test_root_dir = None
    if args.test_files:
        first_test_file = args.test_files[0]
        test_file_path = Path(first_test_file).resolve()

        # Find project root by walking up the directory tree
        # looking for marker files (.git, Makefile, etc.)
        current = test_file_path.parent
        while current != current.parent:
            if (current / '.git').exists() or (current / 'Makefile').exists():
                test_root_dir = str(current)
                break
            current = current.parent

        # Fallback to current working directory if no markers found
        if test_root_dir is None:
            test_root_dir = str(Path.cwd())

    # Initialize test runner
    runner = TestRunner(cli, verbose=args.verbose, test_root_dir=test_root_dir)

    # Run all test files
    for test_file in args.test_files:
        if not os.path.exists(test_file):
            print(f"Warning: Test file not found: {test_file}", file=sys.stderr)
            continue

        runner.run_test_file(test_file)

    # Clean up test artifacts
    runner.cleanup_test_artifacts()

    # Print summary and exit with appropriate code
    all_passed = runner.print_summary()
    return 0 if all_passed else 1


if __name__ == '__main__':
    sys.exit(main())
