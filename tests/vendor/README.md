# Vendored Test Dependencies

These are vendored copies of Python packages used by the integration test runner.
They serve as fallbacks when packages aren't available via pip.

All vendored packages here are MIT or ISC licensed, per Frontier's MIT-only
vendoring policy (see `RELICENSING.md`). GPL/LGPL-family packages must NOT be
vendored — they are added to `tests/requirements.txt` and pip-installed on
demand by the runner instead.

## Packages

### pexpect (version 4.9.0)
- Source: https://github.com/pexpect/pexpect
- License: ISC
- Purpose: PTY-based interactive dialog testing

### ptyprocess (version 0.7.0)
- Source: https://github.com/pexpect/ptyprocess
- License: ISC
- Purpose: Required by pexpect for PTY process management

## Non-Vendored Test Dependencies

### pyte (version >= 0.8.2)
- Source: https://pypi.org/project/pyte/
- License: LGPL-3.0 (NOT MIT-compatible — see RELICENSING.md)
- Purpose: VT100/xterm screen emulation for L4 palette test harness
- Install: handled automatically by `tests/integration/runner.py`
  `_ensure_pyte()` on first palette_mode test invocation; also listed in
  `tests/requirements.txt`. Pulls `wcwidth` (MIT) as a transitive dep.

## Update Process

To update vendored packages:
1. `pip3 install --upgrade pexpect`
2. Copy updated packages, e.g.:
   `cp -r $(python3 -c "import pexpect; import os; print(os.path.dirname(pexpect.__file__))") tests/vendor/pexpect`
3. Copy ptyprocess the same way.
4. Remove `__pycache__`: `find tests/vendor -name "__pycache__" -type d -exec rm -rf {} +`
5. Update version numbers in this README
