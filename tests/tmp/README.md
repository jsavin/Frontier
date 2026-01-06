# Test Output Directory Structure

This directory contains all test-generated artifacts organized by test type.

## Subdirectories

- **unit/** - C unit test outputs (file verbs, parameter state tests)
- **integration/** - YAML-based integration test outputs
- **migration/** - Database migration test artifacts
- **results/** - Test logs and CLI runtime test artifacts

## Usage

All test outputs are consolidated under `tests/tmp/` to:
1. Keep project root clean
2. Provide clear separation by test type
3. Enable easy cleanup with `rm -rf tests/tmp/`
4. Work correctly regardless of working directory

Test frameworks automatically create these directories as needed.

## .gitignore

All subdirectories are git-ignored. Only this README is tracked.
