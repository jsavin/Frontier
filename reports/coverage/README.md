# Coverage Reports

Generated reports showing implementation status and coverage metrics across various project subsystems.

## Structure

### `verb-binding/`
Automatic verb binding analyzer reports for kernel verb processor implementations.

- **Tool**: `tools/kernelverbs_parser/cli.py`
- **Format**: Date-tagged with sequential numbering (e.g., `2025-12-14-01.md`, `2025-12-14-02.md`)
- **Generation**: On-demand with `python3 tools/kernelverbs_parser/cli.py report`
- **Metrics**: 
  - Processors detected vs. total (e.g., 27/51 = 53%)
  - Verbs implemented vs. total (e.g., 400/707 = 56%)
  - GUI-dependent processors (correctly stubbed for headless)
  - Implementation status by processor with markdown tables
- **See also**: [`verb-binding/README.md`](./verb-binding/README.md)

## Adding New Coverage Reports

1. Create a subdirectory (e.g., `test-results/`, `performance-benchmarks/`)
2. Add a `README.md` explaining the report purpose and frequency
3. Use consistent naming: `YYYY-MM-DD-<type>.md` or `YYYY-MM-DD-NN.md`
4. Keep all historical reports (they track project progress)

## Guidelines

- Reports are automatically generated artifacts—don't edit them manually
- Link to relevant source files or tools in the report README
- Keep READMEs updated when adding new report types
