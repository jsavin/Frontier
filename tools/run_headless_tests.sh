#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

echo "[headless-tests] rebuilding CLI..."
make -C frontier-cli

echo "[headless-tests] migrating v6 database to v7..."
# Restore v6 database from git if it's been migrated
if [ -f databases/Frontier-v6.root ]; then
    # Check if it's already v7 (first 2 bytes are 0007 in big-endian)
    version_bytes=$(xxd -l 2 -p databases/Frontier-v6.root)
    if [ "$version_bytes" = "0007" ]; then
        echo "[headless-tests] Frontier-v6.root has been migrated, restoring from git..."
        git checkout databases/Frontier-v6.root
    fi
fi
# Create v7 migrated database
if [ ! -f databases/Frontier-v6-v7.root ] || [ databases/Frontier-v6.root -nt databases/Frontier-v6-v7.root ]; then
    # Copy v6 to v7 and migrate the copy
    cp databases/Frontier-v6.root databases/Frontier-v6-v7.root
    FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli --system-root databases/Frontier-v6-v7.root -e "1" > /dev/null 2>&1 || true
fi

echo "[headless-tests] running test suite..."
make -C tests test

echo "[headless-tests] done."
