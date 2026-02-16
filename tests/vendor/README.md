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

## Update Process

To update vendored packages:
1. `pip3 install --upgrade pexpect`
2. Copy updated packages: `cp -r $(python3 -c "import pexpect; import os; print(os.path.dirname(pexpect.__file__))") tests/vendor/pexpect`
3. Copy ptyprocess: `cp -r $(python3 -c "import ptyprocess; import os; print(os.path.dirname(ptyprocess.__file__))") tests/vendor/ptyprocess`
4. Remove \_\_pycache\_\_: `find tests/vendor -name "__pycache__" -type d -exec rm -rf {} +`
5. Update version numbers in this README
