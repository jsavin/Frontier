# Automatic Verb Binding - Project Plan

## Status
- State: Project Plan Complete
- Phase: 3
- Last Updated: 2025-12-21
- Notes: Overall project plan for verb binding implementation


**Created:** 2025-12-14
**Branch:** `feature/automatic-verb-binding` (open for future phases)
**Owner:** Codex
**Status:** ✅ MERGED (2025-12-14) - PR #104
**Maintainability:** Branch kept open for incremental improvements and Phase 2+ work

---

## Overview

Implement the automatic verb binding architecture designed in `automatic_verb_binding_architecture.md`. This eliminates manual whitelist maintenance for 707 kernel verbs across 51 processors.

**Goal:** Automatic detection of implemented verbs through static analysis, eliminating the need to manually update `HEADLESS_REGISTERED` whitelist.

---

## Project Phases

### Phase 1: Analyzer Core (Week 1, 3-4 days)
**Goal:** Build static analysis engine that detects verb implementations

#### Deliverables:
1. `tools/kernelverbs_parser/analyzer.py` (~400 lines)
   - VerbImplementationAnalyzer class
   - Pattern matching for Carbon APIs, UI adapters, stubs
   - File scanning and source code analysis

2. `tools/kernelverbs_parser/matchers.py` (~300 lines)
   - CARBON_API_PATTERNS (WindowPtr, MenuRef, Carbon.h, etc.)
   - UI_ADAPTER_PATTERNS (adapter_ask, fprintf stderr, @UI_ADAPTER, etc.)
   - STUB_INDICATORS ("not implemented", TODO, STUB, etc.)
   - Pattern matching functions

3. `tools/kernelverbs_parser/metadata_writer.py` (~100 lines)
   - VerbImplementation dataclass
   - JSON serialization
   - Whitelist generation
   - Status report generation

#### Tasks:
- [x] Create VerbImplementation dataclass with all fields
- [x] Implement file discovery (find C source files for each processor)
- [x] Implement pattern matching for stub detection
- [x] Implement pattern matching for Carbon API detection
- [x] Implement pattern matching for UI adapter detection
- [x] Implement annotation parsing (@UI_ADAPTER, @CARBON_DEPS)
- [x] Generate VerbImplementation records for all verbs
- [x] Test on existing file/frontier processors

#### Success Criteria:
- Correctly identifies all implemented verbs in file processor (86 verbs)
- Detects UI adapters in dialog processor
- No false positives on Carbon API patterns
- Generates valid VerbImplementation records

---

### Phase 2: Dry-Run & Verification (Week 1.5, 2-3 days)
**Goal:** Add safety mechanisms and reporting

#### Deliverables:
1. `tools/kernelverbs_parser/cli.py` (~200 lines)
   - Command-line interface with subcommands
   - `--dry-run` mode (show changes without applying)
   - `--verify` mode (check analyzer vs. generated code)
   - `--report` mode (generate coverage dashboard)

2. `docs/verb_metadata_schema.json`
   - JSON schema for VerbImplementation metadata
   - Validation rules
   - Example output

3. Updated `tools/kernelverbs_parser/parse_kernelverbs.py`
   - Integration with analyzer
   - Auto-update HEADLESS_REGISTERED from analyzer output
   - Deprecation warnings for manual whitelist

#### Tasks:
- [x] Create CLI argument parser with subcommands
- [x] Implement dry-run mode (print proposed changes)
- [x] Implement verify mode (check current state matches expected)
- [x] Generate JSON metadata file with all verb info
- [x] Create human-readable status report (markdown table)
- [x] Integrate analyzer into existing parse_kernelverbs.py workflow
- [x] Add deprecation warning for manual whitelist edits

#### Success Criteria:
- `python cli.py --dry-run` shows proposed whitelist changes
- `python cli.py --verify` exits 0 if state is consistent
- `python cli.py --report` generates coverage dashboard
- JSON metadata validates against schema

---

### Phase 3: Test Infrastructure (Week 2, 1-2 days)
**Goal:** Comprehensive test coverage for analyzer

#### Deliverables:
1. `tools/kernelverbs_parser/tests/test_analyzer.py` (20-30 tests)
   - Stub detection tests
   - UI adapter detection tests
   - Carbon API detection tests
   - Annotation parsing tests
   - Complex processor tests (all 51 processors)

2. `tools/kernelverbs_parser/tests/test_integration.py` (5-10 tests)
   - Full pipeline tests (RC → analyzer → whitelist → code gen)
   - Dry-run mode tests
   - Verify mode tests

3. Updated `frontier-cli/Makefile`
   - `make verify-verb-bindings` - Check analyzer vs. current state
   - `make regenerate-verb-bindings` - Run analyzer and update
   - `make verb-status` - Generate coverage report
   - `make test-verb-bindings` - Run unit/integration tests

4. `tools/kernelverbs_parser/README.md` updates
   - Document new analyzer workflow
   - Add usage examples
   - Document CI integration

#### Tasks:
- [x] Write stub detection tests (recognizes "not implemented", TODO, etc.)
- [x] Write UI adapter tests (finds adapter_ask, fprintf stderr, etc.)
- [x] Write Carbon API tests (detects WindowPtr, Carbon.h, etc.)
- [x] Write annotation parsing tests (@UI_ADAPTER, @CARBON_DEPS)
- [x] Write integration tests (full pipeline)
- [x] Add Makefile targets
- [x] Update README with new workflow
- [x] Document CI integration approach

#### Success Criteria:
- All 20-30 unit tests pass
- Integration tests cover full pipeline
- `make verify-verb-bindings` runs successfully
- README documents new workflow clearly

---

### Phase 4: Migration & Documentation (Week 2+, ongoing)
**Goal:** Apply analyzer to codebase and establish workflow

#### Deliverables:
1. `planning/phase3/kernel_verb_porting/verb_coverage_report.md`
   - Automatically generated coverage report
   - Per-processor status (implemented vs. stubbed)
   - UI adapter vs. Carbon dependency breakdown

2. Updated `tools/kernelverbs_parser/parse_kernelverbs.py`
   - HEADLESS_REGISTERED auto-generated (marked with comment)
   - Remove manual whitelist entries
   - Add pointer to analyzer as source of truth

3. CI integration (GitHub Actions or similar)
   - Run `make verify-verb-bindings` on every PR
   - Block PRs if whitelist is stale vs. analyzer output
   - Auto-generate coverage report

#### Tasks:
- [x] Run analyzer on full codebase
- [x] Generate initial verb_coverage_report.md (COVERAGE_REPORT-2025-12-14-01.md)
- [x] Update HEADLESS_REGISTERED from analyzer output
- [x] Add comments indicating auto-generated content
- [x] Test that make targets work end-to-end
- [ ] Set up CI integration (if CI is available)
- [x] Document maintenance workflow for future developers (in README.md)

#### Success Criteria:
- Analyzer detects all currently implemented verbs
- HEADLESS_REGISTERED matches analyzer output
- Coverage report shows accurate status
- No manual whitelist updates needed going forward

---

## File Structure

```
tools/kernelverbs_parser/
├── parse_kernelverbs.py          # Existing parser (extend)
├── generate_processor_stubs.py   # Existing stub generator
├── analyzer.py                   # NEW: Implementation analyzer
├── matchers.py                   # NEW: Pattern matching
├── metadata_writer.py            # NEW: JSON/report generation
├── cli.py                        # NEW: CLI interface
├── tests/
│   ├── test_analyzer.py          # NEW: Unit tests
│   └── test_integration.py       # NEW: Integration tests
└── README.md                     # Update with analyzer docs

planning/phase3/kernel_verb_porting/
├── automatic_verb_binding_architecture.md  # Existing design
├── automatic_verb_binding_project_plan.md  # This file
└── verb_coverage_report.md       # NEW: Auto-generated report

docs/
└── verb_metadata_schema.json     # NEW: JSON schema
```

---

## Daily Workflow

### Day 1: Analyzer Core Scaffolding
**Goal:** Basic analyzer infrastructure
- [x] Create analyzer.py, matchers.py, metadata_writer.py files
- [x] Define VerbImplementation dataclass
- [x] Implement basic file discovery
- [x] Test on file processor

### Day 2: Pattern Matching Implementation
**Goal:** Implement all pattern matchers
- [x] Implement CARBON_API_PATTERNS matching
- [x] Implement UI_ADAPTER_PATTERNS matching
- [x] Implement STUB_INDICATORS matching
- [x] Test on file, dialog, frontier processors

### Day 3: Metadata Generation
**Goal:** Generate VerbImplementation records
- [x] Scan all 51 processors
- [x] Generate metadata for all 707 verbs
- [x] Test accuracy on known-good processors

### Day 4: CLI & Dry-Run
**Goal:** Command-line interface
- [x] Create cli.py with argparse
- [x] Implement --dry-run mode
- [x] Implement --verify mode
- [x] Implement --report mode

### Day 5: Integration
**Goal:** Connect analyzer to existing workflow
- [x] Update parse_kernelverbs.py to use analyzer
- [x] Auto-generate HEADLESS_REGISTERED
- [x] Test end-to-end workflow

### Day 6: Testing
**Goal:** Comprehensive test coverage
- [x] Write 20-30 unit tests (31 total)
- [x] Write 5-10 integration tests
- [x] Ensure all tests pass

### Day 7: Makefile & Documentation
**Goal:** Polish and finalize
- [x] Add Makefile targets
- [x] Update README
- [x] Generate coverage report
- [x] Review and cleanup

---

## Risk Mitigation

### Risk 1: Pattern Matching False Positives/Negatives
**Mitigation:**
- Start conservative (fewer false positives)
- Use annotation overrides (@UI_ADAPTER, @CARBON_DEPS) for edge cases
- Run dry-run mode extensively before applying changes
- Manual spot-check sampling on first run

### Risk 2: Integration with Existing Parser
**Mitigation:**
- Keep parse_kernelverbs.py mostly unchanged
- Add analyzer as optional layer
- Preserve manual override capability initially
- Gradual migration path

### Risk 3: Performance (Scanning 51 Processors)
**Mitigation:**
- Use regex pre-compilation
- Cache file reads
- Profile if needed
- Expected runtime: <1 second for full scan

---

## Success Metrics

### Functional:
- ✅ Analyzer correctly identifies 100% of implemented verbs in file processor
- ✅ No false positives in Carbon API detection
- ✅ UI adapter patterns detected accurately
- ✅ Dry-run shows accurate diff vs. current whitelist
- ✅ All 31 unit tests pass (runtime: 0.076 seconds)

### Workflow:
- ✅ `python3 cli.py analyze` runs successfully
- ✅ `python3 cli.py report` generates markdown coverage reports with date tags
- ✅ `python3 cli.py verify` confirms consistency
- ✅ No manual whitelist updates needed after implementation

### Scale:
- ✅ Analyzer handles all 51 processors without error
- ✅ Generates metadata for all 707 verbs
- ✅ Achieves 27/51 processors detected (53%), 400/707 verbs (56%)
- ✅ Runtime <2 seconds for full analysis

### Coverage Achievement:
- ✅ Implemented Pattern A (standard) and Pattern B (simple) matching
- ✅ Implemented Pattern C (exception tables) for op, pict, frontier, sys
- ✅ Implemented Pattern D (multi-processor consolidation) for 10 processors
- ✅ Generated exception tables with 22 Pattern C + 35+ Pattern D mappings
- ✅ Correctly identifies GUI-dependent processors as intentionally stubbed

---

## Post-Implementation

### Immediate (Completed):
1. ✅ Implemented exception tables for clock/date (Pattern D)
2. ✅ Analyzer auto-detects Pattern D processors in langverbs.c
3. ✅ Generated coverage reports showing 27/51 processors (53%), 400/707 verbs (56%)
4. ✅ Investigated high-priority non-GUI processors (frontier, sys)

### Next Phase:
1. CI integration when available
2. Incrementally add more exception tables as new processors are implemented
3. Monthly coverage reports to track progress
4. Refinement of pattern matchers based on edge cases discovered
5. Consider extension to other processors (script, thread, tcp, etc.)

---

## Notes

- Keep analyzer simple; prefer false negatives (manual override) over false positives
- Document all pattern matching rules clearly
- Provide escape hatches (@UI_ADAPTER annotations) for edge cases
- Generate actionable reports (which verbs need implementation)
- Design for CI integration from day 1
