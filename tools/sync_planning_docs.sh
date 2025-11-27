#!/usr/bin/env bash
#
# 2025-11-26 Codex: Helper to push planning/*.md docs to origin/develop.
# 2025-11-27 Codex: Auto-stashes local work, checks out develop, copies planning docs from the source branch, commits, pushes, and restores your work.
# Usage: tools/sync_planning_docs.sh ["commit message"]

set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

msg="${1:-"docs: sync planning docs"}"
source_branch="$(git rev-parse --abbrev-ref HEAD)"
stash_name="sync-planning-docs-$(date +%s)"
stash_created=0

cleanup() {
  # Return to the original branch if needed.
  if git rev-parse --abbrev-ref HEAD >/dev/null 2>&1; then
    if [ "$(git rev-parse --abbrev-ref HEAD)" != "$source_branch" ]; then
      git checkout "$source_branch" >/dev/null 2>&1 || true
    fi
  fi
  # Restore stash if still pending.
  if [ "$stash_created" -eq 1 ]; then
    git stash pop >/dev/null 2>&1 || true
  fi
}
trap cleanup EXIT

# Stash everything (including untracked) to avoid clobbering local work.
git stash push --include-untracked -m "$stash_name" >/dev/null 2>&1 && stash_created=1 || true

echo "Syncing planning docs from $source_branch to develop..."
git checkout develop
git pull --ff-only

# Copy all planning/*.md from the source branch onto develop.
while IFS= read -r file; do
  dir="$(dirname "$file")"
  mkdir -p "$dir"
  git show "$source_branch:$file" > "$file"
done < <(git ls-tree -r --name-only "$source_branch" planning | grep '\.md$')

git add planning/*.md

if git diff --cached --quiet; then
  echo "No staged planning changes after copy; aborting." >&2
  exit 0
fi

git commit -m "$msg"
git push origin develop

git checkout "$source_branch"
if [ "$stash_created" -eq 1 ]; then
  git stash pop >/dev/null 2>&1 || echo "Warning: stash pop had conflicts; please resolve manually."
  stash_created=0
fi

echo "Planning docs synced to origin/develop."
