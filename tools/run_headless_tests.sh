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

echo "[headless-tests] migrating v6 database to v7..."
# Restore v6 database from git if it's been migrated
if [ -f databases/Frontier.root ]; then
    # Check if it's already v7 (first 2 bytes are 0007 in big-endian)
    version_bytes=$(xxd -l 2 -p databases/Frontier.root)
    if [ "$version_bytes" = "0007" ]; then
        echo "[headless-tests] WARNING: Frontier.root has been corrupted (v7 format), restoring from git..."
        chmod 644 databases/Frontier.root  # Make writable for git checkout
        if ! git checkout databases/Frontier.root; then
            echo "[headless-tests] ERROR: Failed to restore Frontier.root from git"
            exit 1
        fi
        chmod 444 databases/Frontier.root  # Protect from future writes
        echo "[headless-tests] Restored and protected Frontier.root"
    fi
fi
# Ensure v6 database is read-only to prevent accidental modification
chmod 444 databases/Frontier.root 2>/dev/null || true

# Create v7 migrated database
if [ ! -f databases/Frontier.root7 ] || [ databases/Frontier.root -nt databases/Frontier.root7 ]; then
    # Run CLI with v6 database - creates v7 output file automatically
    # Pattern: Frontier.root → Frontier.root7 (.root7 extension appended)
    # Note: Migration may exit with non-zero code (startup script errors) but still succeed
    ./frontier-cli/frontier-cli --system-root databases/Frontier.root -e "1" > /dev/null 2>&1

    # Check if migration succeeded by verifying output file exists and has v7 header
    if [ ! -f databases/Frontier.root7 ]; then
        echo "[headless-tests] ERROR: Database migration failed - no output file created"
        echo "[headless-tests] Try running manually: ./frontier-cli/frontier-cli --system-root databases/Frontier.root -e \"1\""
        exit 1
    fi

    v7_version=$(xxd -l 2 -p databases/Frontier.root7)
    if [ "$v7_version" != "0007" ]; then
        echo "[headless-tests] ERROR: Migration created invalid output (expected v7, got: $v7_version)"
        exit 1
    fi

    echo "[headless-tests] Database migration completed successfully"
fi

# Verify v6 database wasn't modified during migration
version_bytes=$(xxd -l 2 -p databases/Frontier.root)
if [ "$version_bytes" != "0006" ]; then
    echo "[headless-tests] ERROR: Frontier.root was corrupted during migration!"
    echo "[headless-tests] Expected v6 (0006), found: $version_bytes"
    exit 1
fi

echo "[headless-tests] running test suite..."
make -C tests test

echo "[headless-tests] generating unit test OPML report..."
python3 tools/export_unit_tests_to_opml.py || echo "[headless-tests] WARNING: OPML export failed (non-fatal)"

echo "[headless-tests] done."
