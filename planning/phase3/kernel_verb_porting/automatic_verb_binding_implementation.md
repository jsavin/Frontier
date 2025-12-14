# Automatic Verb Binding - Implementation Checklist

**Created:** 2025-12-14
**Branch:** `feature/automatic-verb-binding`
**Status:** In Progress
**Last Updated:** 2025-12-14

---

## Implementation Progress

**Overall Status:** 0% complete (0/8 phases)

- [ ] Phase 1: Analyzer Core
- [ ] Phase 2: Dry-Run & Verification
- [ ] Phase 3: Test Infrastructure
- [ ] Phase 4: Documentation
- [ ] Phase 5: Integration
- [ ] Phase 6: Makefile Targets
- [ ] Phase 7: Coverage Reporting
- [ ] Phase 8: Final Validation

---

## Phase 1: Analyzer Core (~400 lines, Day 1-3)

### 1.1 VerbImplementation Data Structure
- [ ] Create `tools/kernelverbs_parser/metadata_writer.py`
- [ ] Define VerbImplementation dataclass with fields:
  - [ ] `processor: str` - Processor name (e.g., "file")
  - [ ] `verb_name: str` - Verb name (e.g., "exists")
  - [ ] `token: int` - Token index in processor
  - [ ] `is_implemented: bool` - True if real code, False if stub
  - [ ] `impl_file: str` - Source file path
  - [ ] `impl_line: int` - Line number of implementation
  - [ ] `has_carbon_deps: bool` - Uses Carbon/QuickDraw APIs
  - [ ] `uses_ui_adapter: bool` - Uses UI adapter pattern
  - [ ] `platform_specific: bool` - Platform-specific code
  - [ ] `complexity: int` - Estimated complexity (1-5)
- [ ] Add JSON serialization methods (`to_dict()`, `from_dict()`)
- [ ] Add __str__ method for debugging
- [ ] Test dataclass creation and serialization

### 1.2 Pattern Matching Infrastructure
- [ ] Create `tools/kernelverbs_parser/matchers.py`
- [ ] Define CARBON_API_PATTERNS (list of regex patterns):
  - [ ] `r'\bWindowPtr\b'`
  - [ ] `r'\bGrafPtr\b'`
  - [ ] `r'\bMenuRef\b'`
  - [ ] `r'\bGetNewWindow\b'`
  - [ ] `r'\bShowWindow\b'`
  - [ ] `r'#include\s*<Carbon'`
  - [ ] `r'#include\s*<QuickDraw'`
  - [ ] `r'\bDialogRef\b'`
  - [ ] `r'\bControlRef\b'`
- [ ] Define UI_ADAPTER_PATTERNS:
  - [ ] `r'\badapter_alert\b'`
  - [ ] `r'\badapter_ask\b'`
  - [ ] `r'fprintf\s*\(\s*stderr.*ALERT'`
  - [ ] `r'// UI adapter:'`
  - [ ] `r'/\* UI adapter:'`
  - [ ] `r'@UI_ADAPTER'`
- [ ] Define STUB_INDICATORS:
  - [ ] `r'(not )?implemented'`
  - [ ] `r'\bTODO\b'`
  - [ ] `r'\bSTUB\b'`
  - [ ] `r'\bplaceholder\b'`
  - [ ] `r'\bunimplemented\b'`
  - [ ] `r'return false; /\* stub'`
- [ ] Implement `detect_carbon_apis(source: str) -> bool`
- [ ] Implement `detect_ui_adapters(source: str) -> bool`
- [ ] Implement `detect_stub_verb(source: str) -> bool`
- [ ] Test pattern matching functions with sample code

### 1.3 File Discovery
- [ ] Add `find_implementation_file(processor_name: str) -> Optional[str]`
  - [ ] Search patterns: `tests/headless_{processor}_verbs.c`
  - [ ] Search patterns: `Common/source/{processor}verbs.c`
  - [ ] Search patterns: `Common/source/lang{processor}.c`
  - [ ] Handle special cases (frontier → frontierverbs.c)
- [ ] Add `read_source_file(file_path: str) -> str`
- [ ] Add error handling for missing files
- [ ] Test file discovery on known processors (file, frontier, string)

### 1.4 Verb Implementation Analyzer
- [ ] Create `tools/kernelverbs_parser/analyzer.py`
- [ ] Create VerbImplementationAnalyzer class
- [ ] Implement `__init__(self, rc_parser_output)`
  - [ ] Accept processor definitions from parse_kernelverbs.py
  - [ ] Initialize empty verb implementation list
- [ ] Implement `analyze_processor(self, processor_name: str) -> List[VerbImplementation]`
  - [ ] Find implementation file
  - [ ] Read source file
  - [ ] For each verb in processor:
    - [ ] Search for verb implementation function
    - [ ] Detect if stub using STUB_INDICATORS
    - [ ] Detect Carbon APIs if implemented
    - [ ] Detect UI adapters if implemented
    - [ ] Extract line number
    - [ ] Create VerbImplementation record
  - [ ] Return list of VerbImplementation records
- [ ] Implement `analyze_all_processors(self) -> List[VerbImplementation]`
  - [ ] Iterate through all 51 processors
  - [ ] Call analyze_processor for each
  - [ ] Collect all VerbImplementation records
  - [ ] Return complete list
- [ ] Test analyzer on file processor (86 verbs)
- [ ] Test analyzer on frontier processor (14 verbs)
- [ ] Verify accuracy of stub detection
- [ ] Verify accuracy of Carbon API detection

### 1.5 Annotation Support
- [ ] Add `parse_annotations(source: str) -> Dict[str, bool]`
  - [ ] Detect `@UI_ADAPTER` annotation
  - [ ] Detect `@CARBON_DEPS` annotation
  - [ ] Parse annotation and override heuristics
- [ ] Integrate annotation parsing into analyzer
- [ ] Test annotation override mechanism

---

## Phase 2: Dry-Run & Verification (~200 lines, Day 4-5)

### 2.1 CLI Infrastructure
- [ ] Create `tools/kernelverbs_parser/cli.py`
- [ ] Set up argparse with subcommands:
  - [ ] `analyze` - Run analyzer and show results
  - [ ] `--dry-run` - Show changes without applying
  - [ ] `--verify` - Check current state vs. expected
  - [ ] `--report` - Generate coverage report
- [ ] Add `--verbose` flag for detailed output
- [ ] Add `--json` flag to output JSON instead of text
- [ ] Test CLI argument parsing

### 2.2 Dry-Run Mode
- [ ] Implement `dry_run_mode()`
  - [ ] Run analyzer to get current implementation state
  - [ ] Load existing HEADLESS_REGISTERED from parse_kernelverbs.py
  - [ ] Compare analyzer output vs. current whitelist
  - [ ] Print processors to add (in analyzer but not in whitelist)
  - [ ] Print processors to remove (in whitelist but not implemented)
  - [ ] Print summary statistics
  - [ ] Exit without making changes
- [ ] Test dry-run on current codebase
- [ ] Verify diff is accurate

### 2.3 Verify Mode
- [ ] Implement `verify_mode()`
  - [ ] Run analyzer
  - [ ] Load current HEADLESS_REGISTERED
  - [ ] Check if they match
  - [ ] Exit 0 if consistent, exit 1 if drift detected
  - [ ] Print detailed error if inconsistent
- [ ] Test verify mode (should fail initially)
- [ ] Test verify mode after applying changes (should pass)

### 2.4 Report Mode
- [ ] Implement `generate_coverage_report() -> str`
  - [ ] Run analyzer
  - [ ] Calculate statistics:
    - [ ] Total verbs: 707
    - [ ] Implemented verbs: X
    - [ ] Stubbed verbs: Y
    - [ ] UI adapter verbs: Z
    - [ ] Carbon-dependent verbs: W
  - [ ] Generate markdown table by processor
  - [ ] Include implementation percentage
  - [ ] Include verb details (implemented vs. stubbed)
- [ ] Save report to `planning/phase3/kernel_verb_porting/verb_coverage_report.md`
- [ ] Test report generation
- [ ] Review report for accuracy

### 2.5 JSON Schema
- [ ] Create `docs/verb_metadata_schema.json`
- [ ] Define JSON schema for VerbImplementation
- [ ] Add validation examples
- [ ] Document schema in README

---

## Phase 3: Test Infrastructure (~300 lines, Day 6)

### 3.1 Unit Tests - Stub Detection
- [ ] Create `tools/kernelverbs_parser/tests/test_analyzer.py`
- [ ] Test stub detection with "not implemented" text
- [ ] Test stub detection with "TODO" comment
- [ ] Test stub detection with "STUB" marker
- [ ] Test stub detection with placeholder function
- [ ] Test that real implementations are not flagged as stubs

### 3.2 Unit Tests - UI Adapter Detection
- [ ] Test adapter_alert detection
- [ ] Test adapter_ask detection
- [ ] Test fprintf stderr pattern
- [ ] Test comment-based UI adapter markers
- [ ] Test @UI_ADAPTER annotation

### 3.3 Unit Tests - Carbon API Detection
- [ ] Test WindowPtr detection
- [ ] Test MenuRef detection
- [ ] Test Carbon.h include detection
- [ ] Test QuickDraw.h include detection
- [ ] Test that portable code is not flagged

### 3.4 Unit Tests - Annotation Parsing
- [ ] Test @UI_ADAPTER annotation override
- [ ] Test @CARBON_DEPS annotation override
- [ ] Test annotations override heuristics correctly

### 3.5 Unit Tests - Complex Processors
- [ ] Test file processor (86 verbs) - verify all detected correctly
- [ ] Test frontier processor (14 verbs)
- [ ] Test dialog processor (UI adapter patterns)
- [ ] Test string processor (no UI dependencies)
- [ ] Test all 51 processors parse without error

### 3.6 Integration Tests
- [ ] Create `tools/kernelverbs_parser/tests/test_integration.py`
- [ ] Test full pipeline: RC → analyzer → whitelist → code gen
- [ ] Test dry-run mode end-to-end
- [ ] Test verify mode (consistent state)
- [ ] Test verify mode (inconsistent state)
- [ ] Test report generation

### 3.7 Test Execution
- [ ] Create `tools/kernelverbs_parser/run_tests.sh`
- [ ] Run all unit tests
- [ ] Run all integration tests
- [ ] Ensure 100% pass rate
- [ ] Document test coverage

---

## Phase 4: Documentation (~100 lines, Day 7)

### 4.1 README Updates
- [ ] Update `tools/kernelverbs_parser/README.md`
- [ ] Add "Automatic Verb Binding" section
- [ ] Document analyzer workflow
- [ ] Add usage examples:
  - [ ] `python cli.py analyze --dry-run`
  - [ ] `python cli.py analyze --verify`
  - [ ] `python cli.py analyze --report`
- [ ] Document pattern matching rules
- [ ] Document annotation override mechanism
- [ ] Add troubleshooting section

### 4.2 Architecture Documentation
- [ ] Add implementation notes to `automatic_verb_binding_architecture.md`
- [ ] Document actual file structure
- [ ] Document any deviations from original design
- [ ] Add examples of analyzer output

---

## Phase 5: Integration with Existing Parser (~100 lines, Day 7)

### 5.1 Parser Integration
- [ ] Update `tools/kernelverbs_parser/parse_kernelverbs.py`
- [ ] Import analyzer module
- [ ] Add `run_analyzer()` function
  - [ ] Call analyzer on all processors
  - [ ] Get list of implemented processors
  - [ ] Update HEADLESS_REGISTERED automatically
- [ ] Add comment indicating HEADLESS_REGISTERED is auto-generated
- [ ] Add timestamp to auto-generated section
- [ ] Preserve manual override capability (opt-out flag)

### 5.2 Backward Compatibility
- [ ] Add `--skip-analyzer` flag to preserve old behavior
- [ ] Test that existing workflow still works
- [ ] Document migration path from manual to automatic

---

## Phase 6: Makefile Targets (Day 7)

### 6.1 Makefile Integration
- [ ] Add to `frontier-cli/Makefile`:
  - [ ] `verify-verb-bindings` target
    - [ ] Run `python tools/kernelverbs_parser/cli.py analyze --verify`
    - [ ] Exit with error code if drift detected
  - [ ] `regenerate-verb-bindings` target
    - [ ] Run analyzer
    - [ ] Update parse_kernelverbs.py with new whitelist
    - [ ] Regenerate kernel_verbs_init.c
  - [ ] `verb-status` target
    - [ ] Generate coverage report
    - [ ] Print to stdout or save to file
  - [ ] `test-verb-bindings` target
    - [ ] Run all analyzer unit tests
    - [ ] Run integration tests
- [ ] Test all Makefile targets
- [ ] Document targets in Makefile comments

---

## Phase 7: Coverage Reporting (Day 7)

### 7.1 Initial Coverage Report
- [ ] Run `make verb-status` to generate report
- [ ] Review `verb_coverage_report.md`
- [ ] Verify processor breakdown is accurate
- [ ] Verify implementation counts are correct
- [ ] Identify top priority verbs to implement next

### 7.2 CI Integration Preparation
- [ ] Document CI integration strategy in README
- [ ] Add example GitHub Actions workflow (commented out)
- [ ] Document how to use verify-verb-bindings in CI
- [ ] Add quality gates to documentation

---

## Phase 8: Final Validation (Day 7)

### 8.1 End-to-End Testing
- [ ] Run dry-run mode: `make verb-status --dry-run`
- [ ] Review proposed changes
- [ ] Apply changes: `make regenerate-verb-bindings`
- [ ] Run verify mode: `make verify-verb-bindings`
- [ ] Ensure verify passes (exit 0)

### 8.2 Regression Testing
- [ ] Run existing tests: `make -C tests test`
- [ ] Ensure no regressions
- [ ] Run `make -C frontier-cli`
- [ ] Ensure CLI still builds
- [ ] Run `make -C tests runtime_tests`
- [ ] Ensure runtime tests pass

### 8.3 Code Review Preparation
- [ ] Review all new code for style/quality
- [ ] Ensure all functions have docstrings
- [ ] Ensure all files have header comments
- [ ] Run any linters/formatters
- [ ] Create comprehensive commit message
- [ ] Prepare PR description

### 8.4 Documentation Review
- [ ] Review all documentation for accuracy
- [ ] Ensure examples are correct
- [ ] Check for typos/formatting issues
- [ ] Verify all links work
- [ ] Update project plan with actual timeline

---

## Proof of Concept (Optional: Day 8)

### POC: Implement Clock Verbs
- [ ] Implement `clock.now` using standard verb pattern
- [ ] Implement `clock.ticks`
- [ ] Implement `clock.milliseconds`
- [ ] Run analyzer
- [ ] Verify analyzer auto-detects new implementations
- [ ] Verify HEADLESS_REGISTERED updates automatically
- [ ] Generate updated coverage report
- [ ] Document proof of concept in planning docs

---

## Completion Criteria

### Must Have (Blocking):
- [ ] Analyzer detects 100% of file processor verbs correctly
- [ ] No false positives in Carbon API detection
- [ ] Dry-run mode shows accurate diff
- [ ] Verify mode exits 0 when consistent
- [ ] All 20-30 unit tests pass
- [ ] Integration tests pass
- [ ] Documentation complete and accurate

### Should Have (Nice to Have):
- [ ] Coverage report generated automatically
- [ ] Makefile targets all work
- [ ] CI integration documented
- [ ] Proof of concept with clock verbs

### Could Have (Future):
- [ ] Performance optimization (caching)
- [ ] Advanced reporting (HTML output)
- [ ] Annotation IDE support
- [ ] Auto-fix mode (apply changes automatically)

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

## Next Steps

**Immediate:** Begin Phase 1.1 - Create VerbImplementation dataclass in `metadata_writer.py`
**After Phase 1:** Proceed to Phase 2 - CLI and dry-run implementation
**Final:** Complete Phase 8 validation and prepare for PR

---

## Related Documents

- `automatic_verb_binding_architecture.md` - Architecture design
- `automatic_verb_binding_project_plan.md` - High-level project plan
- `tools/kernelverbs_parser/README.md` - Parser documentation
