# Static Analysis Reports

This directory contains static analysis reports for code quality, dead code identification, and logging pattern analysis.

## Structure

### `dead-code/`
Call graph and dead code analysis reports.

- **Tools**: cflow, ctags, ripgrep
- **Format**: `YYYY-MM-DD-<analysis-type>.txt` or `.md`
- **Purpose**: Identify potentially unused functions, unreachable code, and legacy code candidates
- **Contents**:
  - Call graph analysis (cflow output)
  - Uncalled function lists with source locations
  - Category-based dead code candidates (GUI-only, legacy format, platform stubs, etc.)

### `logging/`
Logging pattern and noise analysis reports.

- **Tools**: ripgrep, custom scripts
- **Format**: `YYYY-MM-DD-<analysis-type>.txt` or `.md`
- **Purpose**: Identify logging patterns, noise sources, and candidates for logging infrastructure redesign
- **Contents**:
  - fprintf(stderr) statement counts and locations
  - Debug/diagnostic pattern distribution
  - Per-component logging density

## Usage

Reports in this directory are generated on-demand using the developer tools installed via `./scripts/install_dev_tools.sh`.

See `DEVELOPER_SETUP.md` for tool usage examples.

## Guidelines

- Keep raw tool output (`.txt`) for reproducibility
- Create summary reports (`.md`) for human review
- Date all reports with `YYYY-MM-DD` prefix
- Link to planning docs or issues when relevant
