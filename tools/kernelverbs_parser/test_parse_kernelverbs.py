#!/usr/bin/env python3
"""
test_parse_kernelverbs.py - Unit tests for parse_kernelverbs.py

Tests the kernel verbs parser with various RC content formats,
whitelist filtering behavior, and error handling.
"""

import unittest
import tempfile
import os
import sys
from pathlib import Path
from io import StringIO

# Add parent directory to path for imports
sys.path.insert(0, str(Path(__file__).parent))

from parse_kernelverbs import (
    parse_kernelverbs_rc,
    generate_kernel_verbs_init_c,
    EFPProcessor,
    HEADLESS_IMPLEMENTED,
)


class TestEFPProcessor(unittest.TestCase):
    """Test the EFPProcessor class"""

    def test_processor_creation(self):
        """Test creating an EFPProcessor instance"""
        proc = EFPProcessor("1007", "file", True, 86)
        self.assertEqual(proc.efp_id, "1007")
        self.assertEqual(proc.name, "file")
        self.assertEqual(proc.window_required, True)
        self.assertEqual(proc.verb_count, 86)
        self.assertEqual(proc.init_func, "fileinitverbs")

    def test_processor_repr(self):
        """Test processor string representation"""
        proc = EFPProcessor("1016", "frontier", False, 14)
        self.assertEqual(repr(proc), "EFP(1016, frontier, 14 verbs)")


class TestParserBasics(unittest.TestCase):
    """Test basic parser functionality with mock RC content"""

    def create_temp_rc(self, content):
        """Helper to create a temporary RC file"""
        with tempfile.NamedTemporaryFile(
            mode='w', suffix='.rc', delete=False, encoding='utf-8'
        ) as f:
            f.write(content)
            return f.name

    def tearDown(self):
        """Clean up temporary files"""
        if hasattr(self, 'temp_file') and os.path.exists(self.temp_file):
            os.unlink(self.temp_file)

    def test_parse_simple_processor(self):
        """Test parsing a simple processor definition"""
        rc_content = '''1007 /*idfileverbs*/ EFP DISCARDABLE
BEGIN
1,

"file\\0",
	true,
		86
END
'''
        self.temp_file = self.create_temp_rc(rc_content)
        processors = parse_kernelverbs_rc(self.temp_file)

        self.assertEqual(len(processors), 1)
        self.assertEqual(processors[0].name, "file")
        self.assertEqual(processors[0].verb_count, 86)
        self.assertEqual(processors[0].window_required, True)

    def test_parse_multiple_processors(self):
        """Test parsing multiple processors in one RC file"""
        rc_content = '''1007 /*idfileverbs*/ EFP DISCARDABLE
BEGIN
2,

"file\\0",
	true,
		86,

"database\\0",
	false,
		42
END

1016 /*idfrontierverbs*/ EFP DISCARDABLE
BEGIN
1,

"frontier\\0",
	true,
		14
END
'''
        self.temp_file = self.create_temp_rc(rc_content)
        processors = parse_kernelverbs_rc(self.temp_file)

        self.assertEqual(len(processors), 3)
        names = {p.name for p in processors}
        self.assertEqual(names, {"file", "database", "frontier"})

    def test_parse_with_inline_comments(self):
        """Test parsing processors with inline comments"""
        rc_content = '''1007 /*idtest*/ EFP DISCARDABLE
BEGIN
2,

"file\\0",
	true,
		86, // File operations processor

"text\\0",
	false,
		28 // Text handling processor
END
'''
        self.temp_file = self.create_temp_rc(rc_content)
        processors = parse_kernelverbs_rc(self.temp_file)

        self.assertEqual(len(processors), 2)
        file_proc = next(p for p in processors if p.name == "file")
        text_proc = next(p for p in processors if p.name == "text")
        self.assertEqual(file_proc.verb_count, 86)
        self.assertEqual(text_proc.verb_count, 28)

    def test_parse_boolean_flags(self):
        """Test parsing both true and false for window_required flag"""
        rc_content = '''1007 /*idtest*/ EFP DISCARDABLE
BEGIN
2,

"requires_window\\0",
	true,
		10,

"no_window\\0",
	false,
		20
END
'''
        self.temp_file = self.create_temp_rc(rc_content)
        processors = parse_kernelverbs_rc(self.temp_file)

        self.assertEqual(len(processors), 2)
        requires_win = next((p for p in processors if p.name == "requires_window"), None)
        no_win = next((p for p in processors if p.name == "no_window"), None)

        self.assertIsNotNone(requires_win)
        self.assertIsNotNone(no_win)
        self.assertTrue(requires_win.window_required)
        self.assertFalse(no_win.window_required)

    def test_parse_empty_file(self):
        """Test parsing an empty RC file"""
        rc_content = ""
        self.temp_file = self.create_temp_rc(rc_content)
        processors = parse_kernelverbs_rc(self.temp_file)

        self.assertEqual(len(processors), 0)

    def test_parse_file_without_efp_blocks(self):
        """Test parsing file with no EFP blocks"""
        rc_content = '''// Some comment
#include "resource.h"

// No actual EFP blocks here
'''
        self.temp_file = self.create_temp_rc(rc_content)
        processors = parse_kernelverbs_rc(self.temp_file)

        self.assertEqual(len(processors), 0)


class TestErrorHandling(unittest.TestCase):
    """Test error handling and validation"""

    def create_temp_rc(self, content):
        """Helper to create a temporary RC file"""
        with tempfile.NamedTemporaryFile(
            mode='w', suffix='.rc', delete=False, encoding='utf-8'
        ) as f:
            f.write(content)
            return f.name

    def tearDown(self):
        """Clean up temporary files"""
        if hasattr(self, 'temp_file') and os.path.exists(self.temp_file):
            os.unlink(self.temp_file)

    def test_invalid_identifier_skipped(self):
        """Test that invalid C identifiers are skipped with warning"""
        rc_content = '''1007 /*idtest*/ EFP DISCARDABLE
BEGIN
3,

"invalid-name\\0",
	true,
		10,

"valid_name\\0",
	true,
		20,

"123invalid\\0",
	true,
		30
END
'''
        self.temp_file = self.create_temp_rc(rc_content)

        # Capture stderr to verify warnings
        old_stderr = sys.stderr
        sys.stderr = StringIO()

        processors = parse_kernelverbs_rc(self.temp_file)

        stderr_output = sys.stderr.getvalue()
        sys.stderr = old_stderr

        # Should only parse the valid identifier
        self.assertEqual(len(processors), 1)
        self.assertEqual(processors[0].name, "valid_name")

        # Should have warned about invalid names
        self.assertIn("invalid-name", stderr_output)
        self.assertIn("123invalid", stderr_output)

    def test_duplicate_processor_skipped(self):
        """Test that duplicate processor names are skipped"""
        rc_content = '''1007 /*idtest*/ EFP DISCARDABLE
BEGIN
3,

"file\\0",
	true,
		86,

"file\\0",
	false,
		50,

"other\\0",
	true,
		10
END
'''
        self.temp_file = self.create_temp_rc(rc_content)

        # Capture stderr to verify warnings
        old_stderr = sys.stderr
        sys.stderr = StringIO()

        processors = parse_kernelverbs_rc(self.temp_file)

        stderr_output = sys.stderr.getvalue()
        sys.stderr = old_stderr

        # Should keep first occurrence, skip duplicate
        file_procs = [p for p in processors if p.name == "file"]
        self.assertEqual(len(file_procs), 1)
        self.assertEqual(file_procs[0].verb_count, 86)

        # Should have warned about duplicate
        self.assertIn("duplicate", stderr_output.lower())

    def test_nonexistent_file(self):
        """Test handling of nonexistent input file"""
        with self.assertRaises(FileNotFoundError):
            parse_kernelverbs_rc("/nonexistent/path/to/file.rc")


class TestWhitelistFiltering(unittest.TestCase):
    """Test whitelist-based processor filtering"""

    def create_temp_rc(self, content):
        """Helper to create a temporary RC file"""
        with tempfile.NamedTemporaryFile(
            mode='w', suffix='.rc', delete=False, encoding='utf-8'
        ) as f:
            f.write(content)
            return f.name

    def tearDown(self):
        """Clean up temporary files"""
        if hasattr(self, 'temp_file') and os.path.exists(self.temp_file):
            os.unlink(self.temp_file)

    def test_whitelist_filtering(self):
        """Test that only whitelisted processors are included in generated code"""
        # Create RC with both implemented and unimplemented processors
        rc_content = '''1007 /*idmixed*/ EFP DISCARDABLE
BEGIN
4,

"file\\0",
	true,
		86,

"database\\0",
	false,
		42,

"frontier\\0",
	true,
		14,

"xml\\0",
	false,
		28
END
'''
        self.temp_file = self.create_temp_rc(rc_content)
        processors = parse_kernelverbs_rc(self.temp_file)

        # Generate code with current whitelist
        c_code = generate_kernel_verbs_init_c(processors, self.temp_file)

        # Should include only whitelisted processors in forward declarations
        self.assertIn("fileinitverbs", c_code)
        self.assertIn("frontierinitverbs", c_code)

        # Should NOT include unwhitelisted processors
        self.assertNotIn("databaseinitverbs", c_code)
        self.assertNotIn("xmlinitverbs", c_code)

    def test_generated_code_structure(self):
        """Test that generated C code has proper structure"""
        rc_content = '''1007 /*idtest*/ EFP DISCARDABLE
BEGIN
2,

"file\\0",
	true,
		86,

"frontier\\0",
	true,
		14
END
'''
        self.temp_file = self.create_temp_rc(rc_content)
        processors = parse_kernelverbs_rc(self.temp_file)
        c_code = generate_kernel_verbs_init_c(processors, self.temp_file)

        # Check for required elements
        self.assertIn("#include \"frontier.h\"", c_code)
        self.assertIn("boolean headless_init_kernel_verbs(void)", c_code)
        self.assertIn("extern boolean fileinitverbs(void)", c_code)
        self.assertIn("extern boolean frontierinitverbs(void)", c_code)
        self.assertIn("if (!fileinitverbs())", c_code)
        self.assertIn("if (!frontierinitverbs())", c_code)
        self.assertIn("return true;", c_code)

    def test_generated_code_verb_count_summary(self):
        """Test that generated code includes verb count summaries"""
        rc_content = '''1007 /*idtest*/ EFP DISCARDABLE
BEGIN
3,

"file\\0",
	true,
		86,

"frontier\\0",
	true,
		14,

"database\\0",
	false,
		42
END
'''
        self.temp_file = self.create_temp_rc(rc_content)
        processors = parse_kernelverbs_rc(self.temp_file)
        c_code = generate_kernel_verbs_init_c(processors, self.temp_file)

        # Should have summary comments
        self.assertIn("Implemented processors: 2 of 3", c_code)
        self.assertIn("Implemented verbs: 100 of 142", c_code)


class TestRegressions(unittest.TestCase):
    """Regression tests for specific bugs"""

    def create_temp_rc(self, content):
        """Helper to create a temporary RC file"""
        with tempfile.NamedTemporaryFile(
            mode='w', suffix='.rc', delete=False, encoding='utf-8'
        ) as f:
            f.write(content)
            return f.name

    def tearDown(self):
        """Clean up temporary files"""
        if hasattr(self, 'temp_file') and os.path.exists(self.temp_file):
            os.unlink(self.temp_file)

    def test_all_51_processors_parseable(self):
        """
        Regression test: Parser should handle real kernelverbs.rc
        with all 51 processor definitions
        """
        # This test would ideally use the actual kernelverbs.rc file
        # For now, simulate the key characteristics with correct RC format
        rc_content = '''1007 /*idfileverbs*/ EFP DISCARDABLE
BEGIN
1,

"file\\0",
	true,
		86
END

1008 /*idmenuverbs*/ EFP DISCARDABLE
BEGIN
1,

"menu\\0",
	true,
		20
END

1009 /*idwindowverbs*/ EFP DISCARDABLE
BEGIN
1,

"window\\0",
	true,
		35
END

1016 /*idfrontierverbs*/ EFP DISCARDABLE
BEGIN
1,

"frontier\\0",
	true,
		14
END
'''
        self.temp_file = self.create_temp_rc(rc_content)
        processors = parse_kernelverbs_rc(self.temp_file)

        # Parser should handle multiple processors
        self.assertGreaterEqual(len(processors), 4)
        self.assertEqual(len(processors), 4)

    def test_no_double_init_calls(self):
        """
        Regression test: Generated code should not have duplicate
        initialization calls for the same processor
        """
        rc_content = '''1007 /*idtest*/ EFP DISCARDABLE
BEGIN
2,

"file\\0",
	true,
		86,

"frontier\\0",
	true,
		14
END
'''
        self.temp_file = self.create_temp_rc(rc_content)
        processors = parse_kernelverbs_rc(self.temp_file)
        c_code = generate_kernel_verbs_init_c(processors, self.temp_file)

        # Count occurrences of each init call
        file_init_count = c_code.count("fileinitverbs()")
        frontier_init_count = c_code.count("frontierinitverbs()")

        # Should only appear once in actual calls (not counting comments)
        self.assertEqual(file_init_count, 1)
        self.assertEqual(frontier_init_count, 1)


if __name__ == '__main__':
    unittest.main()
