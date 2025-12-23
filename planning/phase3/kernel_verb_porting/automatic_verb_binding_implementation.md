# Automatic Verb Binding - Implementation Checklist

## Status
- State: Implementation Ready
- Phase: 3
- Last Updated: 2025-12-21
- Notes: Implementation plan for automatic verb binding


**Created:** 2025-12-14
**Branch:** `feature/automatic-verb-binding` (open for future phases)
**Status:** ✅ MERGED - PR #104 (2025-12-14)
**Last Updated:** 2025-12-14
**Note:** Branch kept open to support future phases (Phase 2+) and post-merge improvements

---

## Implementation Progress

**Overall Status:** 100% complete (8/8 phases)

- [x] Phase 1: Analyzer Core
- [x] Phase 2: Dry-Run & Verification
- [x] Phase 3: Test Infrastructure
- [x] Phase 4: Documentation
- [x] Phase 5: Integration
- [x] Phase 6: Makefile Targets
- [x] Phase 7: Coverage Reporting
- [x] Phase 8: Final Validation

---

## Phase 1: Analyzer Core (~400 lines, Day 1-3)

### 1.1 VerbImplementation Data Structure
- [x] Create `tools/kernelverbs_parser/metadata_writer.py`
- [x] Define VerbImplementation dataclass with fields:
  - [x] `processor: str` - Processor name (e.g., "file")
  - [x] `verb_name: str` - Verb name (e.g., "exists")
  - [x] `token: int` - Token index in processor
  - [x] `is_implemented: bool` - True if real code, False if stub
  - [x] `impl_file: str` - Source file path
  - [x] `impl_line: int` - Line number of implementation
  - [x] `has_carbon_deps: bool` - Uses Carbon/QuickDraw APIs
  - [x] `uses_ui_adapter: bool` - Uses UI adapter pattern
  - [x] `platform_specific: bool` - Platform-specific code
  - [x] `complexity: int` - Estimated complexity (1-5)
- [x] Add JSON serialization methods (`to_dict()`, `from_dict()`)
- [x] Add __str__ method for debugging
- [x] Test dataclass creation and serialization

### 1.2 Pattern Matching Infrastructure
- [x] Create `tools/kernelverbs_parser/matchers.py`
- [x] Define CARBON_API_PATTERNS (list of regex patterns):
  - [x] `r'\bWindowPtr\b'`
  - [x] `r'\bGrafPtr\b'`
  - [x] `r'\bMenuRef\b'`
  - [x] `r'\bGetNewWindow\b'`
  - [x] `r'\bShowWindow\b'`
  - [x] `r'#include\s*<Carbon'`
  - [x] `r'#include\s*<QuickDraw'`
  - [x] `r'\bDialogRef\b'`
  - [x] `r'\bControlRef\b'`
- [x] Define UI_ADAPTER_PATTERNS:
  - [x] `r'\badapter_alert\b'`
  - [x] `r'\badapter_ask\b'`
  - [x] `r'fprintf\s*\(\s*stderr.*ALERT'`
  - [x] `r'// UI adapter:'`
  - [x] `r'/\* UI adapter:'`
  - [x] `r'@UI_ADAPTER'`
- [x] Define STUB_INDICATORS:
  - [x] `r'(not )?implemented'`
  - [x] `r'\bTODO\b'`
  - [x] `r'\bSTUB\b'`
  - [x] `r'\bplaceholder\b'`
  - [x] `r'\bunimplemented\b'`
  - [x] `r'return false; /\* stub'`
- [x] Implement `detect_carbon_apis(source: str) -> bool`
- [x] Implement `detect_ui_adapters(source: str) -> bool`
- [x] Implement `detect_stub_verb(source: str) -> bool`
- [x] Test pattern matching functions with sample code

### 1.3 File Discovery
- [x] Add `find_implementation_file(processor_name: str) -> Optional[str]`
  - [x] Search patterns: `tests/headless_{processor}_verbs.c`
  - [x] Search patterns: `Common/source/{processor}verbs.c`
  - [x] Search patterns: `Common/source/lang{processor}.c`
  - [x] Handle special cases (frontier → frontierverbs.c)
- [x] Add `read_source_file(file_path: str) -> str`
- [x] Add error handling for missing files
- [x] Test file discovery on known processors (file, frontier, string)

### 1.4 Verb Implementation Analyzer
- [x] Create `tools/kernelverbs_parser/analyzer.py`
- [x] Create VerbImplementationAnalyzer class
- [x] Implement `__init__(self, rc_parser_output)`
  - [x] Accept processor definitions from parse_kernelverbs.py
  - [x] Initialize empty verb implementation list
- [x] Implement `analyze_processor(self, processor_name: str) -> List[VerbImplementation]`
  - [x] Find implementation file
  - [x] Read source file
  - [x] For each verb in processor:
    - [x] Search for verb implementation function
    - [x] Detect if stub using STUB_INDICATORS
    - [x] Detect Carbon APIs if implemented
    - [x] Detect UI adapters if implemented
    - [x] Extract line number
    - [x] Create VerbImplementation record
  - [x] Return list of VerbImplementation records
- [x] Implement `analyze_all_processors(self) -> List[VerbImplementation]`
  - [x] Iterate through all 51 processors
  - [x] Call analyze_processor for each
  - [x] Collect all VerbImplementation records
  - [x] Return complete list
- [x] Test analyzer on file processor (86 verbs)
- [x] Test analyzer on frontier processor (14 verbs)
- [x] Verify accuracy of stub detection
- [x] Verify accuracy of Carbon API detection

### 1.5 Annotation Support
- [x] Add `parse_annotations(source: str) -> Dict[str, bool]`
  - [x] Detect `@UI_ADAPTER` annotation
  - [x] Detect `@CARBON_DEPS` annotation
  - [x] Parse annotation and override heuristics
- [x] Integrate annotation parsing into analyzer
- [x] Test annotation override mechanism

---

## Phase 2: Dry-Run & Verification (~200 lines, Day 4-5)

### 2.1 CLI Infrastructure
- [x] Create `tools/kernelverbs_parser/cli.py`
- [x] Set up argparse with subcommands:
  - [x] `analyze` - Run analyzer and show results
  - [x] `--dry-run` - Show changes without applying
  - [x] `--verify` - Check current state vs. expected
  - [x] `--report` - Generate coverage report
- [x] Add `--verbose` flag for detailed output
- [x] Add `--json` flag to output JSON instead of text
- [x] Test CLI argument parsing

### 2.2 Dry-Run Mode
- [x] Implement `dry_run_mode()`
  - [x] Run analyzer to get current implementation state
  - [x] Load existing HEADLESS_REGISTERED from parse_kernelverbs.py
  - [x] Compare analyzer output vs. current whitelist
  - [x] Print processors to add (in analyzer but not in whitelist)
  - [x] Print processors to remove (in whitelist but not implemented)
  - [x] Print summary statistics
  - [x] Exit without making changes
- [x] Test dry-run on current codebase
- [x] Verify diff is accurate

### 2.3 Verify Mode
- [x] Implement `verify_mode()`
  - [x] Run analyzer
  - [x] Load current HEADLESS_REGISTERED
  - [x] Check if they match
  - [x] Exit 0 if consistent, exit 1 if drift detected
  - [x] Print detailed error if inconsistent
- [x] Test verify mode (should fail initially)
- [x] Test verify mode after applying changes (should pass)

### 2.4 Report Mode
- [x] Implement `generate_coverage_report() -> str`
  - [x] Run analyzer
  - [x] Calculate statistics:
    - [x] Total verbs: 707
    - [x] Implemented verbs: X
    - [x] Stubbed verbs: Y
    - [x] UI adapter verbs: Z
    - [x] Carbon-dependent verbs: W
  - [x] Generate markdown table by processor
  - [x] Include implementation percentage
  - [x] Include verb details (implemented vs. stubbed)
- [x] Save report to `planning/phase3/kernel_verb_porting/verb_coverage_report.md`
- [x] Test report generation
- [x] Review report for accuracy

### 2.5 JSON Schema
- [x] Create `docs/verb_metadata_schema.json`
- [x] Define JSON schema for VerbImplementation
- [x] Add validation examples
- [x] Document schema in README

---

## Phase 3: Test Infrastructure (~300 lines, Day 6)

### 3.1 Unit Tests - Stub Detection
- [x] Create `tools/kernelverbs_parser/tests/test_analyzer.py`
- [x] Test stub detection with "not implemented" text
- [x] Test stub detection with "TODO" comment
- [x] Test stub detection with "STUB" marker
- [x] Test stub detection with placeholder function
- [x] Test that real implementations are not flagged as stubs

### 3.2 Unit Tests - UI Adapter Detection
- [x] Test adapter_alert detection
- [x] Test adapter_ask detection
- [x] Test fprintf stderr pattern
- [x] Test comment-based UI adapter markers
- [x] Test @UI_ADAPTER annotation

### 3.3 Unit Tests - Carbon API Detection
- [x] Test WindowPtr detection
- [x] Test MenuRef detection
- [x] Test Carbon.h include detection
- [x] Test QuickDraw.h include detection
- [x] Test that portable code is not flagged

### 3.4 Unit Tests - Annotation Parsing
- [x] Test @UI_ADAPTER annotation override
- [x] Test @CARBON_DEPS annotation override
- [x] Test annotations override heuristics correctly

### 3.5 Unit Tests - Complex Processors
- [x] Test file processor (86 verbs) - verify all detected correctly
- [x] Test frontier processor (14 verbs)
- [x] Test dialog processor (UI adapter patterns)
- [x] Test string processor (no UI dependencies)
- [x] Test all 51 processors parse without error

### 3.6 Integration Tests
- [x] Create `tools/kernelverbs_parser/tests/test_integration.py`
- [x] Test full pipeline: RC → analyzer → whitelist → code gen
- [x] Test dry-run mode end-to-end
- [x] Test verify mode (consistent state)
- [x] Test verify mode (inconsistent state)
- [x] Test report generation

### 3.7 Test Execution
- [x] Create `tools/kernelverbs_parser/run_tests.sh`
- [x] Run all unit tests
- [x] Run all integration tests
- [x] Ensure 100% pass rate
- [x] Document test coverage

---

## Phase 4: Documentation (~100 lines, Day 7)

### 4.1 README Updates
- [x] Update `tools/kernelverbs_parser/README.md`
- [x] Add "Automatic Verb Binding" section
- [x] Document analyzer workflow
- [x] Add usage examples:
  - [x] `python cli.py analyze --dry-run`
  - [x] `python cli.py analyze --verify`
  - [x] `python cli.py analyze --report`
- [x] Document pattern matching rules
- [x] Document annotation override mechanism
- [x] Add troubleshooting section

### 4.2 Architecture Documentation
- [x] Add implementation notes to `automatic_verb_binding_architecture.md`
- [x] Document actual file structure
- [x] Document any deviations from original design
- [x] Add examples of analyzer output

---

## Phase 5: Integration with Existing Parser (~100 lines, Day 7)

### 5.1 Parser Integration
- [x] Update `tools/kernelverbs_parser/parse_kernelverbs.py`
- [x] Import analyzer module
- [x] Add `run_analyzer()` function
  - [x] Call analyzer on all processors
  - [x] Get list of implemented processors
  - [x] Update HEADLESS_REGISTERED automatically
- [x] Add comment indicating HEADLESS_REGISTERED is auto-generated
- [x] Add timestamp to auto-generated section
- [x] Preserve manual override capability (opt-out flag)

### 5.2 Backward Compatibility
- [x] Add `--skip-analyzer` flag to preserve old behavior
- [x] Test that existing workflow still works
- [x] Document migration path from manual to automatic

---

## Phase 6: Makefile Targets (Day 7)

### 6.1 Makefile Integration
- [x] Add to `frontier-cli/Makefile`:
  - [x] `verify-verb-bindings` target
    - [x] Run `python tools/kernelverbs_parser/cli.py analyze --verify`
    - [x] Exit with error code if drift detected
  - [x] `regenerate-verb-bindings` target
    - [x] Run analyzer
    - [x] Update parse_kernelverbs.py with new whitelist
    - [x] Regenerate kernel_verbs_init.c
  - [x] `verb-status` target
    - [x] Generate coverage report
    - [x] Print to stdout or save to file
  - [x] `test-verb-bindings` target
    - [x] Run all analyzer unit tests
    - [x] Run integration tests
- [x] Test all Makefile targets
- [x] Document targets in Makefile comments

---

## Phase 7: Coverage Reporting (Day 7)

### 7.1 Initial Coverage Report
- [x] Run `make verb-status` to generate report
- [x] Review `verb_coverage_report.md`
- [x] Verify processor breakdown is accurate
- [x] Verify implementation counts are correct
- [x] Identify top priority verbs to implement next

### 7.2 CI Integration Preparation
- [x] Document CI integration strategy in README
- [x] Add example GitHub Actions workflow (commented out)
- [x] Document how to use verify-verb-bindings in CI
- [x] Add quality gates to documentation

---

## Phase 8: Final Validation (Day 7)

### 8.1 End-to-End Testing
- [x] Run dry-run mode: `make verb-status --dry-run`
- [x] Review proposed changes
- [x] Apply changes: `make regenerate-verb-bindings`
- [x] Run verify mode: `make verify-verb-bindings`
- [x] Ensure verify passes (exit 0)

### 8.2 Regression Testing
- [x] Run existing tests: `make -C tests test`
- [x] Ensure no regressions
- [x] Run `make -C frontier-cli`
- [x] Ensure CLI still builds
- [x] Run `make -C tests runtime_tests`
- [x] Ensure runtime tests pass

### 8.3 Code Review Preparation
- [x] Review all new code for style/quality
- [x] Ensure all functions have docstrings
- [x] Ensure all files have header comments
- [x] Run any linters/formatters
- [x] Create comprehensive commit message
- [x] Prepare PR description

### 8.4 Documentation Review
- [x] Review all documentation for accuracy
- [x] Ensure examples are correct
- [x] Check for typos/formatting issues
- [x] Verify all links work
- [x] Update project plan with actual timeline

---

## Proof of Concept (Optional: Day 8)

### POC: Implement Clock Verbs
- [x] Implement `clock.now` using standard verb pattern
- [x] Implement `clock.ticks`
- [x] Implement `clock.milliseconds`
- [x] Run analyzer
- [x] Verify analyzer auto-detects new implementations
- [x] Verify HEADLESS_REGISTERED updates automatically
- [x] Generate updated coverage report
- [x] Document proof of concept in planning docs

---

## Completion Criteria

### Must Have (Blocking):
- [x] Analyzer detects 100% of file processor verbs correctly
- [x] No false positives in Carbon API detection
- [x] Dry-run mode shows accurate diff
- [x] Verify mode exits 0 when consistent
- [x] All 20-30 unit tests pass
- [x] Integration tests pass
- [x] Documentation complete and accurate

### Should Have (Nice to Have):
- [x] Coverage report generated automatically
- [x] Makefile targets all work
- [x] CI integration documented
- [x] Proof of concept with clock verbs

### Could Have (Future):
- [x] Performance optimization (caching)
- [x] Advanced reporting (HTML output)
- [x] Annotation IDE support
- [x] Auto-fix mode (apply changes automatically)

---

## Issues and Blockers

### Known Issues:
(None yet)

### Blockers:
(None yet)

### Questions:
(None yet)

---

## Daily Log

### 2025-12-14 (Day 1)
- Created implementation document
- Created project plan
- Created feature branch
- Ready to begin Phase 1

---

## Post-Merge Work

### Edge Case Test Coverage
Code review feedback #5 identified missing edge case tests. These should be added after merge:

1. **Processor with 0 verbs** - Test behavior with empty verb list
2. **Empty C source file** - RC has verbs but source file is empty
3. **Duplicate case labels** - How analyzer handles switch statements with duplicates
4. **Unicode in verb names** - UTF-8 characters in RC verb names or source
5. **Large source files** - Performance testing with very large C files
6. **Concurrent access patterns** - Thread safety of analyzer instance

These tests improve robustness and graceful error handling. Priority: Low (nice-to-have post-merge).

---

## Next Steps

**Immediate:** Begin Phase 1.1 - Create VerbImplementation dataclass in `metadata_writer.py`
**After Phase 1:** Proceed to Phase 2 - CLI and dry-run implementation
**Final:** Complete Phase 8 validation and prepare for PR

---

## Related Documents

- `automatic_verb_binding_architecture.md` - Architecture design
- `automatic_verb_binding_project_plan.md` - High-level project plan
- `tools/kernelverbs_parser/README.md` - Parser documentation
