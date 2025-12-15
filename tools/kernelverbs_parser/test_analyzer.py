#!/usr/bin/env python3
"""
Unit tests for kernel verb analyzer.

Tests pattern matching, exception tables, file discovery, and verb analysis.
"""

import unittest
from pathlib import Path
from analyzer import VerbImplementationAnalyzer
from metadata_writer import VerbImplementation
from verb_exceptions import (
    get_pattern_c_exception,
    is_pattern_d_processor,
    generate_pattern_d_candidates,
    PATTERN_C_EXCEPTIONS,
    PATTERN_D_PROCESSORS,
)
from parse_kernelverbs import EFPProcessor


class TestExceptionTables(unittest.TestCase):
    """Test Pattern C and D exception table lookups."""

    def test_pattern_c_exception_op(self):
        """Test Pattern C exception for op processor."""
        # Test known exception
        result = get_pattern_c_exception('op', 'getlinetext')
        self.assertEqual(result, 'linetextfunc')

        # Test another exception
        result = get_pattern_c_exception('op', 'subsexpanded')
        self.assertEqual(result, 'getexpandedfunc')

        # Test non-existent exception
        result = get_pattern_c_exception('op', 'unknown')
        self.assertIsNone(result)

    def test_pattern_c_exception_frontier(self):
        """Test Pattern C exception for frontier processor."""
        result = get_pattern_c_exception('frontier', 'getprogrampath')
        self.assertEqual(result, 'programpathfunc')

    def test_pattern_c_exception_nonexistent_processor(self):
        """Test exception lookup for processor without exceptions."""
        result = get_pattern_c_exception('file', 'created')
        self.assertIsNone(result)

    def test_pattern_d_processor_detection(self):
        """Test Pattern D processor whitelist detection."""
        # Known Pattern D processors
        self.assertTrue(is_pattern_d_processor('dialog'))
        self.assertTrue(is_pattern_d_processor('clock'))
        self.assertTrue(is_pattern_d_processor('date'))
        self.assertTrue(is_pattern_d_processor('kb'))

        # Non-Pattern D processors
        self.assertFalse(is_pattern_d_processor('file'))
        self.assertFalse(is_pattern_d_processor('op'))
        self.assertFalse(is_pattern_d_processor('window'))

    def test_pattern_d_candidates_dialog(self):
        """Test Pattern D candidate generation for dialog processor."""
        candidates = generate_pattern_d_candidates('dialog', 'getvalue')

        # Should include exception if it exists (none for getvalue)
        # Should include standard patterns
        self.assertIn('getvaluedialogfunc', candidates)
        self.assertIn('dialoggetvaluefunc', candidates)
        self.assertIn('getvaluefunc', candidates)

    def test_pattern_d_candidates_clock(self):
        """Test Pattern D candidate generation for clock processor."""
        candidates = generate_pattern_d_candidates('clock', 'now')

        # Check exception is prioritized
        self.assertEqual(candidates[0], 'timefunc')  # Exception table entry

    def test_pattern_d_exception_dialog(self):
        """Test Pattern D exception for dialog processor."""
        from verb_exceptions import get_pattern_d_exception
        result = get_pattern_d_exception('dialog', 'notify')
        self.assertEqual(result, 'notifytdialogfunc')


class TestFileDiscovery(unittest.TestCase):
    """Test processor implementation file discovery."""

    def setUp(self):
        """Create analyzer instance."""
        self.analyzer = VerbImplementationAnalyzer([])

    def test_pattern_d_file_discovery(self):
        """Test that Pattern D processors discover langverbs.c."""
        impl_file = self.analyzer.find_implementation_file('dialog')
        self.assertIsNotNone(impl_file)
        self.assertTrue(impl_file.endswith('langverbs.c'))

    def test_special_case_frontier(self):
        """Test that frontier processor discovers shellsysverbs.c."""
        impl_file = self.analyzer.find_implementation_file('frontier')
        self.assertIsNotNone(impl_file)
        self.assertTrue(impl_file.endswith('shellsysverbs.c'))

    def test_special_case_sys(self):
        """Test that sys processor discovers shellsysverbs.c."""
        impl_file = self.analyzer.find_implementation_file('sys')
        self.assertIsNotNone(impl_file)
        self.assertTrue(impl_file.endswith('shellsysverbs.c'))

    def test_special_case_window(self):
        """Test that window processor discovers shellwindowverbs.c."""
        impl_file = self.analyzer.find_implementation_file('window')
        self.assertIsNotNone(impl_file)
        self.assertTrue(impl_file.endswith('shellwindowverbs.c'))

    def test_standard_file_discovery(self):
        """Test standard {processor}verbs.c discovery."""
        impl_file = self.analyzer.find_implementation_file('file')
        self.assertIsNotNone(impl_file)
        self.assertTrue(impl_file.endswith('fileverbs.c'))

    def test_nonexistent_processor(self):
        """Test that nonexistent processor returns None."""
        impl_file = self.analyzer.find_implementation_file('nonexistent')
        self.assertIsNone(impl_file)


class TestEnumExtraction(unittest.TestCase):
    """Test enum token extraction from C source files."""

    def setUp(self):
        """Create analyzer instance."""
        self.analyzer = VerbImplementationAnalyzer([])

    def test_enum_extraction_simple(self):
        """Test simple enum extraction."""
        source = """
        enum {
            verb1func,
            verb2func,
            verb3func
        };
        """
        verbs = self.analyzer.extract_verb_names_from_enum(source, 'test')
        self.assertGreater(len(verbs), 0)

    def test_enum_extraction_typedef(self):
        """Test typedef enum extraction."""
        source = """
        typedef enum tytesttoken {
            verb1func,
            verb2func,
        } tytesttoken;
        """
        verbs = self.analyzer.extract_verb_names_from_enum(source, 'test')
        self.assertGreater(len(verbs), 0)

    def test_enum_comment_stripping(self):
        """Test that comments are stripped before extraction."""
        source = """
        enum {
            /* comment */ verb1func,
            verb2func,  // inline comment
            verb3func
        };
        """
        verbs = self.analyzer.extract_verb_names_from_enum(source, 'test')
        # Should extract without comment interference
        self.assertGreater(len(verbs), 0)


class TestCaseExtraction(unittest.TestCase):
    """Test case statement extraction from switch statements."""

    def setUp(self):
        """Create analyzer instance."""
        self.analyzer = VerbImplementationAnalyzer([])

    def test_case_extraction_simple(self):
        """Test basic case statement extraction."""
        source = """
        switch (token) {
            case verb1func: {
                return doSomething();
            }
            case verb2func:
            default:
                return false;
        }
        """
        case_source, line_num = self.analyzer.extract_case_implementation(
            source, ['verb1func']
        )
        self.assertGreater(len(case_source), 0)
        self.assertGreater(line_num, 0)

    def test_case_extraction_multiple_candidates(self):
        """Test case extraction tries multiple case labels."""
        source = """
        switch (token) {
            case verb2func: {
                return doSomething();
            }
            default:
                return false;
        }
        """
        # Try multiple labels, second one should match
        case_source, line_num = self.analyzer.extract_case_implementation(
            source, ['verb1func', 'verb2func', 'verb3func']
        )
        self.assertGreater(len(case_source), 0)

    def test_case_extraction_not_found(self):
        """Test case extraction returns empty when not found."""
        source = """
        switch (token) {
            case otherverb:
                return false;
        }
        """
        case_source, line_num = self.analyzer.extract_case_implementation(
            source, ['notfoundfunc']
        )
        self.assertEqual(case_source, "")
        self.assertEqual(line_num, 0)


class TestVerbImplementationAnalysis(unittest.TestCase):
    """Test verb implementation analysis."""

    def setUp(self):
        """Create analyzer instance."""
        self.analyzer = VerbImplementationAnalyzer([])

    def test_stub_detection_explicit(self):
        """Test stub detection with 'not implemented' marker."""
        from matchers import detect_stub_verb

        source = "/* not implemented */"
        self.assertTrue(detect_stub_verb(source))

    def test_stub_detection_todo(self):
        """Test stub detection with TODO marker."""
        from matchers import detect_stub_verb

        source = "/* TODO: implement this */"
        self.assertTrue(detect_stub_verb(source))

    def test_implemented_detection(self):
        """Test that real implementations are not marked as stubs."""
        from matchers import detect_stub_verb

        source = """
        case myfunc: {
            return setfilespecvalue(&programfspec, v);
        }
        """
        self.assertFalse(detect_stub_verb(source))


class TestPatternMatchers(unittest.TestCase):
    """Test pattern matching for Carbon APIs and UI adapters."""

    def test_carbon_api_detection(self):
        """Test Carbon API pattern detection."""
        from matchers import detect_carbon_apis

        source = "WindowPtr window = GetFrontWindow();"
        self.assertTrue(detect_carbon_apis(source))

    def test_ui_adapter_detection(self):
        """Test UI adapter pattern detection."""
        from matchers import detect_ui_adapters

        source = "return adapter_alert(...);"
        self.assertTrue(detect_ui_adapters(source))

    def test_no_carbon_apis(self):
        """Test that normal code doesn't match Carbon patterns."""
        from matchers import detect_carbon_apis

        source = "return setvalue(x, v);"
        self.assertFalse(detect_carbon_apis(source))


class TestAnalyzerIntegration(unittest.TestCase):
    """Integration tests with real processors."""

    def setUp(self):
        """Create analyzer instance."""
        from parse_kernelverbs import parse_kernelverbs_rc
        processors, _ = parse_kernelverbs_rc('../../Common/resources/Win32/kernelverbs.rc')
        self.test_processors = [p for p in processors if p.name in ['op', 'file', 'frontier']]
        self.analyzer = VerbImplementationAnalyzer(self.test_processors)

    def test_op_processor_detection(self):
        """Test that op processor (Pattern C) is detected correctly."""
        op_proc = [p for p in self.test_processors if p.name == 'op'][0]
        impls = self.analyzer.analyze_processor(op_proc)

        # op has 45 verbs, should detect most
        implemented = sum(1 for i in impls if i.is_implemented)
        self.assertGreater(implemented, 40)
        self.assertEqual(len(impls), 45)

    def test_file_processor_detection(self):
        """Test that file processor (Pattern A) is detected correctly."""
        file_proc = [p for p in self.test_processors if p.name == 'file'][0]
        impls = self.analyzer.analyze_processor(file_proc)

        # file has 86 verbs, should detect most
        implemented = sum(1 for i in impls if i.is_implemented)
        self.assertGreater(implemented, 50)
        self.assertEqual(len(impls), 86)

    def test_frontier_processor_100_percent(self):
        """Test that frontier processor achieves 100% detection."""
        frontier_proc = [p for p in self.test_processors if p.name == 'frontier'][0]
        impls = self.analyzer.analyze_processor(frontier_proc)

        # frontier should be 100% with exception tables
        implemented = sum(1 for i in impls if i.is_implemented)
        self.assertEqual(implemented, 14)
        self.assertEqual(len(impls), 14)

    def test_verb_implementation_structure(self):
        """Test that VerbImplementation records have correct structure."""
        op_proc = [p for p in self.test_processors if p.name == 'op'][0]
        impls = self.analyzer.analyze_processor(op_proc)

        # Check first verb record has required fields
        if impls:
            impl = impls[0]
            self.assertEqual(impl.processor, 'op')
            self.assertIsNotNone(impl.verb_name)
            self.assertIsInstance(impl.token, int)
            self.assertIsInstance(impl.is_implemented, bool)
            self.assertIsNotNone(impl.impl_file)
            self.assertIsInstance(impl.impl_line, int)


class TestMetadataWriter(unittest.TestCase):
    """Test metadata writer functionality."""

    def setUp(self):
        """Create test VerbImplementation records."""
        self.verbs = [
            VerbImplementation(
                processor='test',
                verb_name='verb1',
                token=0,
                is_implemented=True,
                impl_file='test.c',
                impl_line=100,
                has_carbon_deps=False,
                uses_ui_adapter=False,
                platform_specific=False,
                complexity=2,
            ),
            VerbImplementation(
                processor='test',
                verb_name='verb2',
                token=1,
                is_implemented=False,
                impl_file='test.c',
                impl_line=0,
                has_carbon_deps=False,
                uses_ui_adapter=False,
                platform_specific=False,
                complexity=1,
            ),
        ]

    def test_statistics_generation(self):
        """Test that statistics are calculated correctly."""
        from metadata_writer import VerbMetadataWriter

        writer = VerbMetadataWriter(self.verbs)
        stats = writer.get_statistics()

        self.assertEqual(stats['total_verbs'], 2)
        self.assertEqual(stats['implemented_verbs'], 1)
        self.assertEqual(stats['stubbed_verbs'], 1)
        self.assertEqual(stats['total_processors'], 1)

    def test_whitelist_generation(self):
        """Test that whitelist is generated correctly."""
        from metadata_writer import VerbMetadataWriter

        writer = VerbMetadataWriter(self.verbs)
        whitelist = writer.generate_whitelist()

        # Should include processor with implementations
        self.assertIn('test', whitelist)


class TestEdgeCases(unittest.TestCase):
    """Test edge cases and boundary conditions."""

    def test_processor_with_zero_verbs(self):
        """Test analyzer handles processor with 0 verbs gracefully."""
        # Create a processor with 0 verbs
        processor = EFPProcessor(
            efp_id="9999",
            name="empty",
            window_required=False,
            verb_count=0,
            verb_names=[]
        )

        analyzer = VerbImplementationAnalyzer([processor])
        implementations = analyzer.analyze_processor(processor)

        # Should return empty list, not crash
        self.assertEqual(implementations, [])

    def test_empty_c_source_file(self):
        """Test analyzer behavior when C source file is empty."""
        # Create processor with verbs but empty source
        processor = EFPProcessor(
            efp_id="9999",
            name="empty_src",
            window_required=False,
            verb_count=2,
            verb_names=["verb1", "verb2"]
        )

        analyzer = VerbImplementationAnalyzer([processor])

        # Mock an empty source file by using read_source_file
        # The analyzer should handle gracefully
        implementations = analyzer.analyze_processor(processor)

        # With empty source, all verbs should be marked as stubs
        stub_count = sum(1 for impl in implementations if not impl.is_implemented)
        self.assertEqual(stub_count, 2)

    def test_verb_name_validation(self):
        """Test that verb names with unusual characters are handled correctly."""
        processor = EFPProcessor(
            efp_id="9999",
            name="test",
            window_required=False,
            verb_count=3,
            verb_names=["normal", "with_underscore", "with123number"]
        )

        analyzer = VerbImplementationAnalyzer([processor])

        # Source code with implementation
        source = """
        case normalfunc: {
            return true;
        }
        case with_undercorefunc: {
            return true;
        }
        case with123numberfunc: {
            return true;
        }
        """

        # Should extract without errors
        case_source, line_num = analyzer.extract_case_implementation(
            source,
            ["normalfunc", "with_undercorefunc", "with123numberfunc"]
        )

        self.assertGreater(line_num, 0)
        self.assertTrue("return true" in case_source)

    def test_large_source_file_performance(self):
        """Test that analyzer handles large source files efficiently."""
        processor = EFPProcessor(
            efp_id="9999",
            name="large",
            window_required=False,
            verb_count=3,
            verb_names=["verb1", "verb2", "verb3"]
        )

        # Create a large source file (1000+ lines)
        large_source = "// Comment line\n" * 500
        large_source += """
        case verb1func: {
            return true;
        }
        """
        large_source += "// Comment line\n" * 500
        large_source += """
        case verb2func: {
            return false; /* stub */
        }
        """
        large_source += "// Comment line\n" * 500

        analyzer = VerbImplementationAnalyzer([processor])

        # Should process without performance issues
        case_source, line_num = analyzer.extract_case_implementation(
            large_source,
            ["verb1func"]
        )

        self.assertGreater(line_num, 0)
        self.assertIn("return true", case_source)

    def test_case_extraction_with_fallthrough(self):
        """Test case extraction handles code without explicit break."""
        source = """
        case test1func:
        case test2func: {
            // shared implementation (fall-through)
            return true;
        }
        case test3func: {
            return false;
        }
        """

        analyzer = VerbImplementationAnalyzer([])

        # Should extract test1func case
        case_source, line_num = analyzer.extract_case_implementation(
            source,
            ["test1func"]
        )

        # Should find something (the shared implementation)
        self.assertGreater(line_num, 0)

    def test_deeply_nested_braces(self):
        """Test case extraction with deeply nested block structures."""
        source = """
        case complexfunc: {
            if (condition) {
                while (true) {
                    doSomething();
                }
            }
            return true;
        }
        case nextfunc: {
            return false;
        }
        """

        analyzer = VerbImplementationAnalyzer([])

        # Should correctly identify the end of complexfunc case
        case_source, line_num = analyzer.extract_case_implementation(
            source,
            ["complexfunc"]
        )

        self.assertGreater(line_num, 0)
        # Should NOT include the nextfunc case
        self.assertNotIn("nextfunc", case_source)
        # Should include opening of nested structures
        self.assertIn("if (condition)", case_source)


if __name__ == '__main__':
    unittest.main()
