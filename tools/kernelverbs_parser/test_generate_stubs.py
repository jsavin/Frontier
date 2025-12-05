#!/usr/bin/env python3
"""
test_generate_stubs.py - Unit tests for generate_processor_stubs.py

Tests C stub file generation, including:
- Token enum generation with correct format
- Switch statement structure with all verb cases
- Init function signatures
- ADD_VERB macro calls
- Prefix generation for various processor names
- Edge cases: single verb, many verbs, special characters
- Verb name extraction from RC content
"""

import unittest
import sys
import re
from pathlib import Path

# Add parent directory to path for imports
sys.path.insert(0, str(Path(__file__).parent))

from parse_kernelverbs import EFPProcessor
from generate_processor_stubs import (
    generate_processor_stub,
    extract_verb_names,
)


def validate_c_structure(c_code):
    """
    Structural validation of generated C code without compilation.

    Returns a list of errors found, or empty list if valid.
    """
    checks = [
        (r'#include "frontier\.h"', "Missing frontier.h include"),
        (r'#include "standard\.h"', "Missing standard.h include"),
        (r'enum\s*\{[^}]+\}', "Missing token enum"),
        (r'switch\s*\(token\)\s*\{', "Missing switch statement"),
        (r'boolean \w+_valueproc\(', "Missing valueproc function"),
        (r'boolean \w+initverbs\(void\)', "Missing init function"),
        (r'return true;', "Missing success return"),
        (r'return false;', "Missing failure return"),
    ]
    errors = []
    for pattern, msg in checks:
        if not re.search(pattern, c_code):
            errors.append(msg)
    return errors


class TestStubGeneration(unittest.TestCase):
    """Test C stub file generation."""

    def test_enum_generation(self):
        """Test that token enum is generated correctly with proper format."""
        proc = EFPProcessor("1099", "test", False, 3)
        verb_names = ["alpha", "beta", "gamma"]
        c_code = generate_processor_stub(proc, verb_names)

        # Should have proper enum format with processor prefix
        self.assertIn("enum {", c_code)
        self.assertIn("tesv_alpha = 0,", c_code)
        self.assertIn("tesv_beta = 1,", c_code)
        self.assertIn("tesv_gamma = 2", c_code)  # Last one without comma
        self.assertIn("};", c_code)

    def test_enum_no_trailing_comma(self):
        """Test that last enum entry doesn't have trailing comma."""
        proc = EFPProcessor("1099", "test", False, 1)
        c_code = generate_processor_stub(proc, ["only"])

        # Verify last enum entry has no trailing comma
        self.assertIn("tesv_only = 0\n};", c_code)
        # Make sure there's no trailing comma
        self.assertNotIn("tesv_only = 0,\n};", c_code)

    def test_switch_cases(self):
        """Test that switch statement has all verb cases."""
        proc = EFPProcessor("1099", "test", False, 2)
        verb_names = ["foo", "bar"]
        c_code = generate_processor_stub(proc, verb_names)

        self.assertIn("switch(token) {", c_code)
        self.assertIn("case tesv_foo:", c_code)
        self.assertIn("case tesv_bar:", c_code)
        self.assertIn("default:", c_code)
        self.assertIn("return false;", c_code)

    def test_switch_cases_have_comments(self):
        """Test that switch cases include TODO comments."""
        proc = EFPProcessor("1099", "test", False, 2)
        verb_names = ["get", "set"]
        c_code = generate_processor_stub(proc, verb_names)

        # Each case should have a TODO comment
        self.assertIn('/* TODO: Implement test.get */', c_code)
        self.assertIn('/* TODO: Implement test.set */', c_code)

    def test_init_function_signature(self):
        """Test init function has correct signature."""
        proc = EFPProcessor("1099", "myproc", False, 1)
        c_code = generate_processor_stub(proc, ["verb1"])

        self.assertIn("boolean myprocinitverbs(void) {", c_code)
        self.assertIn("return true;", c_code)

    def test_add_verb_macros(self):
        """Test ADD_VERB macro calls are generated correctly."""
        proc = EFPProcessor("1099", "test", False, 2)
        verb_names = ["get", "set"]
        c_code = generate_processor_stub(proc, verb_names)

        self.assertIn('ADD_VERB(BIGSTRING("\\pget"), tesv_get);', c_code)
        self.assertIn('ADD_VERB(BIGSTRING("\\pset"), tesv_set);', c_code)

    def test_single_verb_processor(self):
        """Test processor with single verb (like python)."""
        proc = EFPProcessor("1099", "python", False, 1)
        verb_names = ["doscript"]
        c_code = generate_processor_stub(proc, verb_names)

        # Verify enum
        self.assertIn("pytv_doscript = 0", c_code)
        # Should NOT have trailing comma on single-item enum
        self.assertIn("pytv_doscript = 0\n};", c_code)
        # Verify switch case
        self.assertIn("case pytv_doscript:", c_code)
        # Verify add_verb
        self.assertIn('ADD_VERB(BIGSTRING("\\pdoscript"), pytv_doscript);', c_code)

    def test_many_verb_processor(self):
        """Test processor with many verbs (like file with 86 verbs)."""
        proc = EFPProcessor("1007", "file", True, 10)
        verb_names = [f"verb{i}" for i in range(10)]
        c_code = generate_processor_stub(proc, verb_names)

        # Verify enum has all 10 verbs
        for i in range(10):
            self.assertIn(f"filv_verb{i} = {i}", c_code)

        # Verify switch has all cases
        for i in range(10):
            self.assertIn(f"case filv_verb{i}:", c_code)

    def test_prefix_generation(self):
        """Test 3-char prefix generation for various processor names."""
        test_cases = [
            ("string", "strv", "verb"),
            ("file", "filv", "verb"),
            ("db", "dbv", "verb"),  # 2-char name gets padded
            ("base64", "basv", "verb"),
            ("python", "pytv", "verb"),
            ("frontier", "frov", "verb"),
            ("op", "opv", "verb"),  # 2-char name
        ]
        for proc_name, expected_prefix, verb_name in test_cases:
            with self.subTest(proc_name=proc_name):
                proc = EFPProcessor("1099", proc_name, False, 1)
                c_code = generate_processor_stub(proc, [verb_name])
                self.assertIn(f"{expected_prefix}_{verb_name}", c_code,
                            f"Should use prefix '{expected_prefix}' for processor '{proc_name}'")

    def test_includes(self):
        """Test that generated stub includes required headers."""
        proc = EFPProcessor("1099", "test", False, 1)
        c_code = generate_processor_stub(proc, ["verb"])

        self.assertIn('#include "frontier.h"', c_code)
        self.assertIn('#include "standard.h"', c_code)
        self.assertIn('#include "memory.h"', c_code)
        self.assertIn('#include "strings.h"', c_code)
        self.assertIn('#include "lang.h"', c_code)
        self.assertIn('#include "langinternal.h"', c_code)
        self.assertIn('#include "tablestructure.h"', c_code)

    def test_c_structure_validation(self):
        """Test that generated code passes structural C validation."""
        proc = EFPProcessor("1099", "test", False, 3)
        verb_names = ["get", "set", "delete"]
        c_code = generate_processor_stub(proc, verb_names)

        errors = validate_c_structure(c_code)
        self.assertEqual(errors, [], f"Generated C code has structural issues: {errors}")

    def test_pushhashtable_pophashtable(self):
        """Test that hash table is properly pushed and popped."""
        proc = EFPProcessor("1099", "test", False, 1)
        c_code = generate_processor_stub(proc, ["verb"])

        self.assertIn("pushhashtable(htable);", c_code)
        self.assertIn("pophashtable();", c_code)
        # pophashtable should appear in the macro and at the end
        self.assertGreaterEqual(c_code.count("pophashtable()"), 2)

    def test_processor_name_in_string(self):
        """Test that processor name is properly used in copystring."""
        proc = EFPProcessor("1099", "myproc", False, 1)
        c_code = generate_processor_stub(proc, ["verb"])

        self.assertIn('copystring(BIGSTRING("\\pmyproc"), bsname);', c_code)


class TestVerbNameExtraction(unittest.TestCase):
    """Test verb name extraction from RC content.

    Note: Current implementation returns generic names {processor}_{verbN}.
    This is acceptable for Phase 1. The Opus-recommended improved extraction
    (with actual verb names from RC) is in the docs/verb-processor-status branch
    and can be integrated after merge.
    """

    def test_fallback_to_generic_names_empty_content(self):
        """Test that generic names are used when RC content is empty."""
        verb_names = extract_verb_names("", "test", 3)
        self.assertEqual(len(verb_names), 3)
        self.assertEqual(verb_names[0], "test_verb0")
        self.assertEqual(verb_names[1], "test_verb1")
        self.assertEqual(verb_names[2], "test_verb2")

    def test_fallback_to_generic_names_missing_processor(self):
        """Test that generic names are used when processor not found in RC."""
        rc_content = '"other\\0",'
        verb_names = extract_verb_names(rc_content, "notfound", 2)
        self.assertEqual(verb_names, ["notfound_verb0", "notfound_verb1"])

    def test_returns_correct_count(self):
        """Test that extraction returns correct number of verb names."""
        verb_names = extract_verb_names("", "date", 5)
        self.assertEqual(len(verb_names), 5)

    def test_processor_name_in_generic_names(self):
        """Test that processor name is included in generic verb names."""
        verb_names = extract_verb_names("", "file", 3)
        for i, name in enumerate(verb_names):
            self.assertTrue(name.startswith("file_"),
                          f"Verb name '{name}' should start with 'file_'")
            self.assertEqual(name, f"file_verb{i}")


class TestIntegrationStubGeneration(unittest.TestCase):
    """Integration tests for stub generation with realistic data."""

    def test_date_processor_stub(self):
        """Test generating stub for date processor with real verb count."""
        proc = EFPProcessor("1005", "date", False, 30)
        verb_names = [
            "get", "set", "abbrevstring", "dayofweek", "daysinmonth",
            "daystring", "firstofmonth", "lastofmonth", "longstring", "nextmonth",
            "nextweek", "nextyear", "prevmonth", "prevweek", "prevyear",
            "shortstring", "tomorrow", "weeksinmonth", "yesterday", "getcurrenttimezone",
            "netstandardstring", "monthtostring", "dayofweektostring", "versionlessthan",
            "day", "month", "year", "hour", "minute", "seconds"
        ]
        c_code = generate_processor_stub(proc, verb_names)

        # Verify structure
        errors = validate_c_structure(c_code)
        self.assertEqual(errors, [], f"Generated code has issues: {errors}")

        # Verify all verbs are present
        for verb in verb_names:
            self.assertIn(f"datv_{verb}", c_code)

        # Verify correct count
        case_count = len(re.findall(r'case datv_\w+:', c_code))
        self.assertEqual(case_count, 30)

    def test_op_processor_stub(self):
        """Test generating stub for op processor with large verb count."""
        proc = EFPProcessor("1000", "op", True, 45)
        verb_names = [f"verb{i}" for i in range(45)]
        c_code = generate_processor_stub(proc, verb_names)

        # Verify structure
        errors = validate_c_structure(c_code)
        self.assertEqual(errors, [])

        # Verify case count
        case_count = len(re.findall(r'case opv_verb\d+:', c_code))
        self.assertEqual(case_count, 45)


if __name__ == '__main__':
    unittest.main()
