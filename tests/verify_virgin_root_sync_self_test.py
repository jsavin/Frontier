#!/usr/bin/env python3
"""
Self-tests for tools/verify_virgin_root_sync.py — the literal-aware comment
marker substitution and the kernel-body normalization pipeline.

These are pure-function tests: they exercise _substitute_comment_markers and
normalize_kernel_body without spawning frontier-cli or touching any database.

Background: UserTalk's legacy comment markers are 0xC7 (chcomment, START,
equivalent to //) and 0xC8 (chendcomment, END). After MacRoman decoding these
become the curly guillemets "<<" / ">>" (rendered here as the real chars). The
exporter must rewrite a marker ONLY when it acts as a comment delimiter, never
when it appears as literal data inside a string ("...", curly-quoted "...",
or '...' char literal). Mirrors langscan.c per-line literal tracking. A
regression that stopped tracking literals would corrupt char/string constants
that contain these bytes -- the assertions below are the regression anchors,
including three real corpus lines that previously round-tripped wrong.

Run with:
    python3 -m unittest tests.verify_virgin_root_sync_self_test -v
"""

import importlib.util
import os
import unittest
from pathlib import Path


# Load tools/verify_virgin_root_sync.py as a module. It is not a package and
# lives outside tests/, so import it by file path.
_HERE = Path(os.path.dirname(os.path.abspath(__file__)))
_VERIFIER_PATH = _HERE.parent / "tools" / "verify_virgin_root_sync.py"

_spec = importlib.util.spec_from_file_location("vvrs", _VERIFIER_PATH)
vvrs = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(vvrs)

# The legacy comment markers as decoded curly guillemets.
COMMENT_START = "«"  # « (0xC7 chcomment)
COMMENT_END = "»"    # » (0xC8 chendcomment)
OPEN_CURLY = "“"     # left double quote
CLOSE_CURLY = "”"    # right double quote


class SubstituteCommentMarkersTest(unittest.TestCase):
    """_substitute_comment_markers: rewrite markers only outside literals."""

    def sub(self, text):
        return vvrs._substitute_comment_markers(text)

    def test_no_markers_returns_input_unchanged(self):
        # Fast-path short-circuit: text with neither marker is returned as-is.
        s = 'local (x = 1);\ntry {bundle {x = 2}}'
        self.assertEqual(self.sub(s), s)

    def test_marker_in_code_position_becomes_comment(self):
        # A bare START marker in code position is the legacy // comment.
        self.assertEqual(self.sub(COMMENT_START + " a note"), "// a note")

    def test_end_marker_in_code_position_is_dropped(self):
        # END marker in code position is dropped (// comments run to EOL).
        self.assertEqual(
            self.sub(COMMENT_START + "note" + COMMENT_END), "//note")

    def test_marker_inside_double_quote_string_preserved(self):
        # `replace ("»", ">>")` must keep the search string intact.
        s = 'replace ("' + COMMENT_END + '", ">>")'
        self.assertEqual(self.sub(s), s)

    def test_marker_inside_char_literal_preserved(self):
        # `nthChar (line, 1) != '»'` must keep its char literal.
        s = "if c == '" + COMMENT_END + "' {return}"
        self.assertEqual(self.sub(s), s)

    def test_marker_inside_curly_quote_string_preserved(self):
        s = OPEN_CURLY + "x" + COMMENT_START + "y" + CLOSE_CURLY
        self.assertEqual(self.sub(s), s)

    def test_markers_across_two_lines_each_handled(self):
        # Literal state resets at the line break; both START markers in code
        # position on their own line become //.
        s = COMMENT_START + "first\n" + COMMENT_START + "second"
        self.assertEqual(self.sub(s), "//first\n//second")

    def test_unterminated_string_does_not_swallow_next_line_marker(self):
        # An unterminated " on line 1 must NOT keep line 2's marker literal:
        # literals never span lines, so the line-2 START is a real comment.
        s = 'x = "oops\n' + COMMENT_START + " real comment"
        self.assertEqual(self.sub(s), 'x = "oops\n// real comment')

    def test_escaped_quote_keeps_string_open(self):
        # An escaped quote inside "..." does not close the string, so a marker
        # after it is still literal data.
        s = 'a = "x\\"' + COMMENT_END + '"'
        self.assertEqual(self.sub(s), s)

    def test_marker_after_close_quote_is_code(self):
        # Once the string closes, a following marker is a real comment.
        s = 'a = "x" ' + COMMENT_START + " trailing"
        self.assertEqual(self.sub(s), 'a = "x" // trailing')


class NormalizeKernelBodyTest(unittest.TestCase):
    """normalize_kernel_body: JSON-unescape -> MacRoman->UTF-8 -> markers."""

    def test_json_escapes_decoded(self):
        # \r and \t are JSON-unescaped; a later normalization step folds CR->LF,
        # so the canonical output carries LF (not CR) plus the literal tab.
        raw = b'local (x);\\r\\ttry {x = 1}'
        out = vvrs.normalize_kernel_body(raw)
        self.assertIn(b"\n", out)
        self.assertNotIn(b"\r", out)
        self.assertIn(b"\t", out)

    def test_macroman_marker_byte_becomes_comment_in_code(self):
        # 0xC7 is MacRoman « (chcomment). In code position it normalizes to //.
        raw = b"\xc7 a note"
        out = vvrs.normalize_kernel_body(raw)
        self.assertTrue(out.startswith(b"//"))
        self.assertNotIn("«".encode("utf-8"), out)

    def test_macroman_marker_byte_inside_string_preserved_as_utf8(self):
        # 0xC8 is MacRoman » (chendcomment). Inside a "..." string it survives
        # as the UTF-8 encoding of », not rewritten to //.
        raw = b'replace ("\xc8", ">>")'
        out = vvrs.normalize_kernel_body(raw)
        self.assertIn("»".encode("utf-8"), out)
        self.assertNotIn(b"//", out)


if __name__ == "__main__":
    unittest.main()
