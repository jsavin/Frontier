# Project Reports

This directory contains generated reports and progress documentation for the Frontier project. Reports are organized by type and are automatically generated or updated as milestones are completed.

## Structure

### `progress/`
Historical progress reports documenting major accomplishments between pull requests or milestones.

- **Format**: `YYYY-MM-DD-<short-label>.md`
- **Purpose**: Provide a chronological ledger of work completed, suitable for external stakeholders or PR summaries
- **See also**: [`progress/README.md`](./progress/README.md)

### `coverage/`
Tool-generated coverage and implementation status reports.

#### `coverage/verb-binding/`
Automatic verb binding analyzer reports showing implementation status across all kernel verb processors.

- **Format**: `YYYY-MM-DD-NN.md` (with sequential numbering per day)
- **Frequency**: Generated on demand with `python3 tools/kernelverbs_parser/cli.py report`
- **Contents**:
  - Coverage metrics (implemented vs. stubbed verbs)
  - Per-processor implementation percentage
  - Detailed verb-by-verb status with source locations
  - Markdown tables for easy review
- **See also**: [`coverage/verb-binding/README.md`](./coverage/verb-binding/README.md)

### Integration Test OPML Files

**Hierarchical OPML structure** for browsing integration tests in outline editors like [Drummer](https://drummer.land/).

- **Files**: 26 total (1 manifest + 25 category files)
  - `integration_tests.opml` - Manifest with transclusion links
  - `integration_tests_{category}.opml` - Individual category files
- **Format**: OPML 2.0 with transclusion (`type="link"` attributes)
- **Tests**: 1,247 integration tests across 25 categories
- **Regenerate**: `python3 tools/export_tests_to_opml.py`

**Benefits**:
- No merge conflicts when adding tests to different categories
- Atomic changes (only affected category file changes)
- Scalable (category files stay ~450-3,600 lines)

**Opening**: Open `integration_tests.opml` in Drummer or any OPML 2.0-compatible editor. Category links load inline via transclusion.

## Adding New Reports

When adding a new report type:

1. Create a subdirectory under `reports/` (e.g., `reports/test-results/`)
2. Add a `README.md` explaining the report purpose and format
3. Use the pattern `YYYY-MM-DD-<type>.md` or `YYYY-MM-DD-NN.md` (with sequential numbering) for dated reports
4. Keep all reports in the repository (they're part of project history)

## Guidelines

- **Do not delete** old reports; they provide historical context
- **Update READMEs** when adding new report types
- **Link to relevant PRs, docs, or issues** within reports when helpful
- **Keep reports concise** but comprehensive enough for stakeholders
- **Use markdown tables** for structured data (coverage, metrics)
