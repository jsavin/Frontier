# Automatic Verb Binding Coverage Reports

Status and implementation metrics for the 51 kernel verb processors and 707 total verbs.

## Purpose

These reports track the progress of implementing and analyzing kernel verb bindings across Frontier's codebase. They show:
- Which processors have implementations vs. stubs
- Implementation percentage by processor
- Per-verb status with source file locations
- Infrastructure for automatic verb binding detection

## Report Format

- **Filename**: `YYYY-MM-DD-NN.md` (date with sequential numbering per day)
- **Contents**:
  - Summary statistics (X/Y processors detected, A/B verbs implemented)
  - Processor breakdown (by detection percentage)
  - Detailed per-processor tables showing:
    - Verb count and implementation percentage
    - Individual verb status (✅ Implemented or ⬜ Stub)
    - Source file and line number for implementations

## Generation

Generate a new report with:

```bash
cd tools/kernelverbs_parser
python3 cli.py report
```

This automatically:
1. Analyzes all 51 processors
2. Detects verb implementations using pattern matching
3. Creates a date-tagged report (e.g., `2025-12-14-03.md`)
4. Moves the report to this directory

## Key Metrics

**Current Coverage** (as of latest report):
- Processors detected: 27/51 (53%)
- Verbs implemented: 400/707 (56%)
- GUI-dependent processors (correctly stubbed): 9
- Non-GUI processors needing investigation: 15

**Processor Categories**:
- **Fully detected (100%)**: frontier, kb, math, mouse, pict, point, rectangle, rgb, speaker
- **Mostly detected (70-99%)**: op, xml, html, string, date, menu, clock, db, lang, dialog, file
- **GUI-dependent (intentionally stubbed)**: window, editmenu, filemenu, statusbar, and 5 more
- **Genuinely stubbed (headless only)**: script, thread, tcp, base64, bit, dll, and 12+ more

## Recent Reports

- [`2025-12-14-02.md`](./2025-12-14-02.md) - Detailed verb tables with high-priority processor investigation
- [`2025-12-14-01.md`](./2025-12-14-01.md) - Initial coverage analysis with processor categorization

## How to Use

1. **Track progress**: Compare sequential reports to see coverage improvements
2. **Identify next targets**: Look for processors with low detection (0% or <50%)
3. **Verify implementations**: Check source locations for newly implemented verbs
4. **Find stubbed verbs**: See which verbs need implementation work
5. **Plan next phases**: Use coverage data to prioritize processor implementation

## Analysis Patterns

The analyzer detects verbs using four distinct patterns:

- **Pattern A**: Standard `{processor}{verb}func` mapping (e.g., `filecreatedfunc`)
- **Pattern B**: Simple `{verb}func` mapping (e.g., `movefunc`)
- **Pattern C**: Irregular naming requiring exception tables (op, pict, frontier, sys)
- **Pattern D**: Multi-processor consolidation in langverbs.c (10 processors)

Exception tables in `tools/kernelverbs_parser/verb_exceptions.py` handle special cases.

## See Also

- [`tools/kernelverbs_parser/README.md`](../../../../tools/kernelverbs_parser/README.md) - Analyzer documentation
- [`tools/kernelverbs_parser/cli.py`](../../../../tools/kernelverbs_parser/cli.py) - Report generation tool
- [`planning/phase3/kernel_verb_porting/`](../../../../planning/phase3/kernel_verb_porting/) - Project planning docs
