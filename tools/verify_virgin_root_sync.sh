#!/bin/bash
#
# verify_virgin_root_sync.sh — thin wrapper for verify_virgin_root_sync.py.
#
# The verifier itself is Python because the kernel emits non-UTF-8
# (MacRoman) bytes inside JSON, which is gnarly to decode in shell.
# This wrapper exists so the documented invocation matches the rest of
# tools/*.sh and so callers don't need to know the implementation language.
#
# See verify_virgin_root_sync.py for full docs and exit codes:
#   0  in sync
#   1  drift detected
#   2  infrastructure error (cli missing, root missing, protocol error)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec /usr/bin/env python3 "$SCRIPT_DIR/verify_virgin_root_sync.py" "$@"
