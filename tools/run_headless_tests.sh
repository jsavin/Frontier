#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

echo "[headless-tests] rebuilding CLI..."
make -C frontier-cli

echo "[headless-tests] running test suite..."
make -C tests test

echo "[headless-tests] done."
