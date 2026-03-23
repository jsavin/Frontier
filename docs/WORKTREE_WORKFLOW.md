# Worktree Workflow Guide

Complete guide to using git worktrees for parallel feature development in Frontier.

## Quick Reference

See main CLAUDE.md for:
- Decision tree (when to use worktrees)
- Key stability principles
- Worktree naming conventions
- Critical discipline rules

This document provides detailed commands, examples, and troubleshooting.

---

## Creating a New Worktree

### Step-by-Step

```bash
# From main Frontier directory on develop
cd /Users/jake/dev/jsavin/Frontier
git worktree add ../Frontier-<feature-name> -b feature/<feature-name>
cd ../Frontier-<feature-name>

# Verify setup
pwd && git branch
# Should show: /Users/jake/dev/jsavin/Frontier-<feature-name>
#              * feature/<feature-name>

# Build and work here
make clean && make
./tools/run_headless_tests.sh
```

### Naming Examples

```bash
# For feature branches:
feature/table-verbs-headless  →  Frontier-table-verbs-headless

# For fix branches:
fix/database-corruption       →  Frontier-database-corruption

# Pattern: Strip prefix (feature/, fix/), prepend 'Frontier-'
```

---

## Cleaning Up After PR Merged

```bash
# After PR is merged to develop
cd /Users/jake/dev/jsavin
rm -rf Frontier-<feature-name>
cd Frontier
git worktree prune  # Clean up worktree metadata
git branch -d feature/<feature-name>  # Delete local branch (optional)
```

**Tip:** Use the `/tidy` command to automate this cleanup process with safety checks.

---

## Recommended Multi-Session Setup

### Session 1 (Feature Development)
```bash
cd /Users/jake/dev/jsavin/Frontier-build-fix  # worktree on feature branch
git branch -a  # verify you're on feature/*, not develop
# Do work, test locally with ./tools/run_headless_tests.sh
# Create PR when ready, let bot review
```

### Session 2 (Other Work)
```bash
cd /Users/jake/dev/jsavin/Frontier  # main directory on develop
git checkout develop  # verify you're on develop
# Work on separate feature branch, or research tasks that don't modify code
# Coordinate if you need to push to develop (ask Session 1 first)
```

### Parallel Development Rules
- Session 1 (worktree): Feature work on dedicated branch
- Session 2 (main dir): Only research, analysis, or separate feature work
- **Never both sessions push to develop simultaneously** - use PR workflow for visibility
- If Session 2 wants to commit to develop, check if Session 1 has open PRs first
- Session 1 should merge and clean up worktree before Session 2 does major develop work

---

## Pre-Work Checklist

Before starting major work in any session:
1. ✅ Verify which worktree/directory you're in: `pwd && git branch`
2. ✅ Check for uncommitted changes: `git status` (should show "working tree clean")
3. ✅ Sync with origin: `git fetch origin` (see if develop has changed)
4. ✅ If you're on develop, check recent commits: `git log -3`
5. ✅ Ask yourself: "Am I about to work on the right branch for this task?"

---

## Common Multi-Session Gotchas

### Gotcha 1: Building wrong binary
**Problem:** You're in Session 1's worktree, run tests, then switch to Session 2's directory. Session 2 has stale CLI binary from old build.

**Fix:** Each session rebuilds its own binary, or remove old one: `rm frontier-cli/frontier-cli`

### Gotcha 2: Database corruption from parallel test runs
**Problem:** Session 1 runs migration test, updates Frontier.root. Session 2 runs test at same time, expects old database state.

**Fix:** Don't run tests in parallel; use `git checkout` to reset databases between test runs

### Gotcha 3: Develop branch changes while working on feature
**Problem:** Session 1 is on feature branch, hasn't fetched in a while. Session 2 merges PR to develop. Session 1's PR conflicts because develop moved.

**Fix:** Session 1 runs `git fetch origin && git rebase origin/develop` before push

### Gotcha 4: Worktree gets "stuck" on merged branch
**Problem:** Feature branch was merged, worktree is still pointing to that branch. Attempting to push fails with "branch no longer exists"

**Fix:** Delete worktree when feature is merged: `git worktree remove feature-branch-name`

---

## When Something Goes Wrong

### Both sessions on same branch
**Problem:** Only one should push

**Solution:**
- Coordinate via chat/discussion
- One session rebases onto latest origin before pushing
- Other session pulls/rebases after first push succeeds

### Database state inconsistent
**Solution:**
- Restore: `git checkout databases/*.root`
- Rebuild: `make -C tests clean && make -C tests save_migration_tests`
- This resets to known-good state

### Worktree "detached" or in bad state
**Solution:**
- Delete and recreate: `git worktree remove <name> && git worktree add <name> origin/<branch>`

---

## Advanced: Rebasing in Worktrees

When your feature branch needs to incorporate changes from develop:

```bash
# In your worktree
cd /Users/jake/dev/jsavin/Frontier-<feature-name>

# Fetch latest develop
git fetch origin develop

# Rebase your feature branch
git rebase origin/develop

# If conflicts occur, resolve them:
# 1. Fix conflicts in files
# 2. git add <resolved-files>
# 3. git rebase --continue

# Force push to update PR (if already pushed)
git push --force-with-lease origin feature/<feature-name>
```

**Note:** Use `--force-with-lease` instead of `--force` - it's safer and prevents overwriting others' work.

---

## See Also

- Main CLAUDE.md - Decision tree and core principles
- `/tidy` command - Automated cleanup with safety checks
- `/doit` command - Full feature workflow including worktree setup
