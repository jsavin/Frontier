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

### Test OPML Files (Build Artifacts — Not Tracked)

**Hierarchical OPML structure** for browsing tests in outline editors like [Drummer](https://drummer.land/).

These files are **regenerated on every test run** and are **not tracked in git** (issue #556) — anyone wanting a current snapshot regenerates them locally:

- **Integration tests** (`reports/integration_tests.opml` + `reports/integration_tests/*.opml`): regenerate with `python3 tools/export_tests_to_opml.py`. 1 manifest + per-category files (currently 69 categories, 2,243 tests). OPML 2.0 with transclusion (`type="link"`).
- **Unit tests** (`reports/unit_tests.opml` + `reports/unit_tests/*.opml`): regenerated as a side effect of `./tools/run_headless_tests.sh`.

**Why not tracked**: every regeneration updates embedded timestamps, producing a noisy diff for every PR that runs tests during validation. They're treated as build artifacts (like compiled binaries) and gitignored. CI publishes the artifact separately if needed.

> **Note for OPML subscribers**: raw GitHub URLs to these files (e.g. `raw.githubusercontent.com/.../reports/integration_tests.opml`) are no longer valid. Regenerate locally and serve from your own location, or subscribe to a CI-published artifact if/when one is set up.

**Opening**: Regenerate locally, then open `integration_tests.opml` (or `unit_tests.opml`) in Drummer or any OPML 2.0-compatible editor. Category links load inline via transclusion.

## Adding New Reports

When adding a new report type:

1. Create a subdirectory under `reports/` (e.g., `reports/test-results/`)
2. Add a `README.md` explaining the report purpose and format
3. Use the pattern `YYYY-MM-DD-<type>.md` or `YYYY-MM-DD-NN.md` (with sequential numbering) for dated reports
4. Keep all reports in the repository (they're part of project history), unless they're regenerated build artifacts like the test OPMLs above — those should be gitignored.

## Guidelines

- **Do not delete** old reports; they provide historical context (this applies to authored reports under `progress/`, `coverage/`, `static-analysis/`, etc. — not to regenerated build artifacts like the test OPMLs)
- **Update READMEs** when adding new report types
- **Link to relevant PRs, docs, or issues** within reports when helpful
- **Keep reports concise** but comprehensive enough for stakeholders
- **Use markdown tables** for structured data (coverage, metrics)
