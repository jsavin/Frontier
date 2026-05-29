# Vendored Test Dependencies

These are vendored copies of Python packages used by the integration test runner.
They serve as fallbacks when packages aren't available via pip.

## Packages

### pexpect (version 4.9.0)
- Source: https://github.com/pexpect/pexpect
- License: ISC
- Purpose: PTY-based interactive dialog testing

### ptyprocess (version 0.7.0)
- Source: https://github.com/pexpect/ptyprocess
- License: ISC
- Purpose: Required by pexpect for PTY process management

### pyte (version 0.8.2)
- Source: https://pypi.org/project/pyte/0.8.2/ (sdist)
- License: LGPL-3.0
- Purpose: VT100/xterm screen emulation for L4 palette test harness
  — interprets ANSI escape sequences emitted by frontier-cli's palette
  and REPL modes, producing a deterministic in-memory `Screen.display`
  that can be diffed against golden text fixtures.

### wcwidth (version 0.2.13)
- Source: https://pypi.org/project/wcwidth/0.2.13/ (sdist)
- License: MIT
- Purpose: Required by pyte for measuring the column width of Unicode
  characters when laying out the emulated screen.

## Update Process

To update vendored packages:
1. `pip3 install --upgrade pexpect pyte wcwidth`
2. Copy updated packages, e.g.:
   `cp -r $(python3 -c "import pexpect; import os; print(os.path.dirname(pexpect.__file__))") tests/vendor/pexpect`
3. Copy ptyprocess, pyte, wcwidth the same way.
4. Remove \_\_pycache\_\_: `find tests/vendor -name "__pycache__" -type d -exec rm -rf {} +`
5. Update version numbers in this README

## Smoke Tests

- `test_pyte_import.py` — verifies the vendored pyte + wcwidth pair imports
  and that feeding bytes through `pyte.Stream` populates `Screen.display`.
  Run with: `python3 -m unittest tests.vendor.test_pyte_import -v`
