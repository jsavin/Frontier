# Contributing Guidelines

This repository contains a large, legacy codebase being modernized.  
The goal is **clarity, traceability, and reversibility** — not speed.  
All changes should preserve build integrity and maintain a readable, bisectable history.

Before you start a change, skim the top-level `README.md` plus the documents in
`planning/` (especially `planning/INDEX.md` and `planning/DECISIONS.md`) to
understand current priorities and known gaps. Those files capture the context
that keeps changes aligned with the broader modernization effort.

---

## Branching and Merge Strategy

- Branch from the shared integration branch: `develop`.  
  Name branches `feature/...`, `fix/...`, `chore/...`, etc.
- Keep your branch up to date by rebasing or merging in `origin/develop`
  regularly, but avoid rewriting commits that are already under review.
- Open PRs against `develop`. Releases are cut separately by merging `develop`
  into `main`.
- **Do not squash merge.**  
  Merge via GitHub’s **“Create a merge commit”** (or `git merge --no-ff`) so the
  branch history remains inspectable.
- Force-pushing shared branches is discouraged. If history needs to be cleaned
  up, coordinate before rewriting.

---

## Commit Philosophy

- **Each commit should:**
  - Build and pass tests (or be an isolated commit that intentionally breaks, followed by a fixup).  
  - Represent a single conceptual change: refactor, fix, cleanup, or feature.  
  - Include a clear commit message describing *why*, not just *what*, in the
    imperative mood.  
    Example:
    ```
    parser: refactor token normalization logic
    - Replace recursive descent with iterative loop for clarity
    - Adds unit tests for empty token edge cases
    ```
- Use **`git commit --fixup`** and **`git rebase -i --autosquash`** to clean up history before merging.  
- Keep **mechanical changes (e.g., formatting, renames)** in separate commits
  and list them in `.git-blame-ignore-revs` (create the file if needed).

---

## Pre-Submit Checklist

Run the relevant build/test targets before opening or merging a PR:

```bash
# Core CLI build (universal binary)
make -C frontier-cli

# Test suite (known failures documented in README + planning/ISSUES.md)
make -C tests test
SANITIZE=1 make -C tests test     # optional but recommended before merging
```

Document any expected failures in your PR description and reference the
tracking issue or planning doc entry.

---

## Developer Tools Setup

For code analysis, static analysis, and understanding the codebase structure, install the analysis tooling:

### Quick Setup

```bash
# Install all development analysis tools at once
./scripts/install_dev_tools.sh
```

### Manual Installation

If you prefer to install individually:

```bash
brew install cflow universal-ctags clang-tools ripgrep tree bear
```

### What Gets Installed

- **cflow** — Call graph analysis to identify dead code
- **universal-ctags** — Symbol indexing and cross-reference analysis
- **clang-tools** — LLVM static analysis (clang-check, clang-tidy, clang-format)
- **ripgrep** — Fast pattern searching across codebase
- **tree** — Directory structure visualization
- **bear** — Compilation database generation for advanced analysis

### Using the Tools

See `DEVELOPER_SETUP.md` for comprehensive documentation including:
- Detailed usage examples for each tool
- Workflow examples for finding dead code
- Analyzing logging patterns
- Mapping module dependencies
- CI/CD integration

Quick examples:

```bash
# Identify potentially unused functions
cflow Common/source/*.c | grep "^[a-z_]*$"

# Find all logging statements
rg "fprintf\(stderr" --type c

# Find GUI-related code
rg "window|menu|dialog" --type c -l

# Generate call graph for specific file
cflow Common/source/db_format.c | head -50
```

---

## Configuration and Best Practices

Add this to your personal or repo `.gitconfig` to make rebasing safer and cleaner:
```ini
[pull]
  rebase = true
[rebase]
  autosquash = true
  autostash = true
[rerere]
  enabled = true
[merge]
  conflictstyle = zdiff3
[diff]
  renames = true
[commit]
  verbose = true
```

- Enable **rerere** — Git will remember conflict resolutions across rebases.  
- Use **zdiff3** conflict style for easier merges (shows ancestor context).  
- Configure blame to skip formatting-only commits:
  ```bash
git config blame.ignoreRevsFile .git-blame-ignore-revs
  ```

---

## Tagging and Milestones

Use **signed tags** for milestones during the modernization:
```bash
git tag -s v0.4-parser-rewrite -m "Parser module modernized to C99"
```
This anchors important stages in a long-running refactor.

---

## Code Review (even solo)

Even if working alone:
- Open a **draft PR** for significant changes to provide a diff view and narrative.  
- Use the PR description to summarize the scope, dependencies, and any follow-up TODOs.  
- Treat the PR as your externalized memory of intent — assume future-you will forget the context.
- Link to the relevant planning notes or ADRs where applicable so reviewers can
  jump straight to the rationale.

---

## Example Workflow

```bash
# Update branch and prep changes
git fetch origin
git rebase origin/develop    # or merge origin/develop if rebasing is risky

# Stage and commit logically
git add src/parser.c
git commit -m "parser: rewrite tokenizer for UTF-8 correctness"

# Optional fixup against a previous commit
git commit --fixup :/tokenizer

# Clean up series
git rebase -i --autosquash origin/develop    # optional, coordinate before rewriting shared history

# Review what changed since last base
git range-diff origin/develop...HEAD

# Push for review or merge
git push origin feature/parser-refactor
```

---

## Philosophy

> Preserve history like it’s archaeology.  
> Every commit should help a future reader understand how and why the code evolved — not just what changed.
