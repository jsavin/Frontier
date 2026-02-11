#!/usr/bin/env bash
# 2026-02-10 Codex: Added guard to keep CLAUDE.md and AGENTS.md linked to shared AI guidelines.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SHARED_REF="docs/AI_SHARED_GUIDELINES.md"

missing=0
for file in "${ROOT_DIR}/AGENTS.md" "${ROOT_DIR}/CLAUDE.md"; do
    if ! rg -q "${SHARED_REF}" "${file}"; then
        echo "missing shared-guidelines reference: ${file}" >&2
        missing=1
    fi
done

if [ "${missing}" -ne 0 ]; then
    echo "check failed: both AGENTS.md and CLAUDE.md must reference ${SHARED_REF}" >&2
    exit 1
fi

echo "ok: AGENTS.md and CLAUDE.md both reference ${SHARED_REF}"
