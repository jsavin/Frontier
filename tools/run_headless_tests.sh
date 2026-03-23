#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

# Check for xxd dependency
if ! command -v xxd &> /dev/null; then
    echo "[headless-tests] ERROR: xxd command not found"
    echo "[headless-tests] xxd is required for database version verification"
    echo "[headless-tests] Install via: brew install vim (macOS) or apt-get install vim-common (Linux)"
    exit 1
fi

echo "[headless-tests] rebuilding CLI..."
make -C frontier-cli

echo "[headless-tests] checking datetime type usage..."
if ! ./tools/check_datetime_types.sh; then
    echo "[headless-tests] datetime type check FAILED"
    exit 1
fi

# Verify source database is v7 format
echo "[headless-tests] verifying v7 source database..."
version_bytes=$(xxd -l 2 -p databases/Frontier.root)
if [ "$version_bytes" != "0007" ]; then
    echo "[headless-tests] ERROR: databases/Frontier.root is not v7 format (got: $version_bytes)"
    echo "[headless-tests] Source databases should be v7. See docs for migration instructions."
    exit 1
fi

echo "[headless-tests] running test suite..."
make -C tests test

echo "[headless-tests] generating unit test OPML report..."
python3 tools/export_unit_tests_to_opml.py || echo "[headless-tests] WARNING: OPML export failed (non-fatal)"

echo "[headless-tests] done."
