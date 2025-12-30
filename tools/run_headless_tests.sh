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

echo "[headless-tests] migrating v6 database to v7..."
# Restore v6 database from git if it's been migrated
if [ -f databases/Frontier-v6.root ]; then
    # Check if it's already v7 (first 2 bytes are 0007 in big-endian)
    version_bytes=$(xxd -l 2 -p databases/Frontier-v6.root)
    if [ "$version_bytes" = "0007" ]; then
        echo "[headless-tests] WARNING: Frontier-v6.root has been corrupted (v7 format), restoring from git..."
        chmod 644 databases/Frontier-v6.root  # Make writable for git checkout
        if ! git checkout databases/Frontier-v6.root; then
            echo "[headless-tests] ERROR: Failed to restore Frontier-v6.root from git"
            exit 1
        fi
        chmod 444 databases/Frontier-v6.root  # Protect from future writes
        echo "[headless-tests] Restored and protected Frontier-v6.root"
    fi
fi
# Ensure v6 database is read-only to prevent accidental modification
chmod 444 databases/Frontier-v6.root 2>/dev/null || true

# Create v7 migrated database
if [ ! -f databases/Frontier-v6-v7.root ] || [ databases/Frontier-v6.root -nt databases/Frontier-v6-v7.root ]; then
    # Run CLI with v6 database - creates v7 output file automatically (INPUT.root → INPUT-v7.root)
    if ! FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli --system-root databases/Frontier-v6.root -e "1" > /dev/null 2>&1; then
        echo "[headless-tests] ERROR: Database migration failed"
        echo "[headless-tests] Try running manually: ./frontier-cli/frontier-cli --system-root databases/Frontier-v6.root -e \"1\""
        exit 1
    fi
    echo "[headless-tests] Database migration completed successfully"
fi

# Verify v6 database wasn't modified during migration
version_bytes=$(xxd -l 2 -p databases/Frontier-v6.root)
if [ "$version_bytes" != "0006" ]; then
    echo "[headless-tests] ERROR: Frontier-v6.root was corrupted during migration!"
    echo "[headless-tests] Expected v6 (0006), found: $version_bytes"
    exit 1
fi

echo "[headless-tests] running test suite..."
make -C tests test

echo "[headless-tests] done."
