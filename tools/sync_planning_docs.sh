#!/usr/bin/env bash
#
# 2025-11-26 Codex: Helper to push planning/*.md docs to origin/develop.
# Usage: tools/sync_planning_docs.sh ["commit message"]
# Captures planning/*.md diffs from the current branch, applies them onto
# develop, commits, pushes, then returns to the starting branch.

set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

msg="${1:-"docs: sync planning docs"}"
current_branch="$(git rev-parse --abbrev-ref HEAD)"

patch="$(mktemp)"
trap 'rm -f "$patch"' EXIT

# Capture planning/.md changes (tracked/untracked)
if ! git diff --binary -- planning/*.md > "$patch"; then
  echo "Failed to capture planning diffs" >&2
  exit 1
fi

# Also include untracked .md files under planning/
untracked=$(git ls-files --others --exclude-standard planning/*.md || true)
for f in $untracked; do
  git diff --binary -- /dev/null "$f" >> "$patch" || true
done

if ! grep -q "diff --git" "$patch"; then
  echo "No planning/.md changes to sync."
  exit 0
fi

echo "Syncing planning docs from $current_branch to develop..."
git checkout develop
git pull --ff-only

git apply "$patch"
git add planning/*.md

if git diff --cached --quiet; then
  echo "No staged planning changes after apply; aborting."
  git checkout "$current_branch"
  exit 0
fi

git commit -m "$msg"
git push origin develop

git checkout "$current_branch"
echo "Planning docs synced to origin/develop."
