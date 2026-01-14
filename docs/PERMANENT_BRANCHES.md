# Permanent Branches

## Overview

The following branches exist independently of the main development flow and should **never be merged** to develop. These are archive and documentation branches preserved for historical reference.

---

## archive/codex-sessions

**Purpose:** Session recordings and transcripts from Claude Code interactions

**Status:** Archive/documentation branch

**Size:** ~950k lines (session data)

**Created:** October 2025

**Description:** Contains historical record of AI-assisted development sessions. This data is for reference and documentation purposes only. Provides insight into development process, problem-solving approaches, and architectural decisions made during Claude Code-assisted sessions.

**Git Notes:** This branch has a git note attached with additional metadata.

View with:
```bash
git notes show refs/heads/archive/codex-sessions
```

---

## archive/portable-refactoring

**Purpose:** Stash snapshot from portable refactoring cleanup work

**Status:** Archive/experimental branch

**Size:** 348 files changed, 98k lines modified

**Created:** November 2025

**Description:** Historical snapshot of portable handle refactoring experiments. Preserved for reference but superseded by subsequent refactoring work. Contains intermediate refactoring states that may be useful for understanding evolution of portable handle architecture.

**Git Notes:** This branch has a git note attached with additional metadata.

View with:
```bash
git notes show refs/heads/archive/portable-refactoring
```

---

## archive/carbon-migration

**Purpose:** Legacy planning phase documentation archival

**Status:** Archive/documentation branch

**Size:** 85 files changed, 228 lines modified

**Created:** October 2025

**Description:** Historical snapshot of planning documentation reorganization. Preserved for reference. Contains early planning documents and architectural decision records that were later consolidated or superseded.

**Git Notes:** This branch has a git note attached with additional metadata.

View with:
```bash
git notes show refs/heads/archive/carbon-migration
```

---

## Viewing All Git Notes

To view git notes for all archived branches at once:

```bash
git notes show refs/heads/archive/codex-sessions
git notes show refs/heads/archive/portable-refactoring
git notes show refs/heads/archive/carbon-migration
```

---

## Important: Never Merge These Branches

These branches are intentionally kept separate from the main development flow:

- ❌ **Never run `git merge archive/<branch-name>`** into develop
- ❌ **Never create PRs from these branches** to develop
- ✅ **Reference them for historical context** when needed
- ✅ **Cherry-pick specific commits** if absolutely necessary (rare)

---

## Why Keep Archive Branches?

1. **Historical record** - Documents development evolution and decision-making
2. **Learning resource** - Future developers can understand why certain approaches were taken
3. **Reference material** - Contains intermediate states that may explain current architecture
4. **Documentation** - Session transcripts provide context for complex implementations

---

## Listing Archive Branches

To see all archive branches:

```bash
git branch -a | grep archive
```

Expected output:
```
  archive/carbon-migration
  archive/codex-sessions
  archive/portable-refactoring
```

---

## Adding New Archive Branches

If you need to create a new permanent archive branch:

1. Create the branch: `git checkout -b archive/<descriptive-name>`
2. Add content (documentation, snapshots, etc.)
3. Push to origin: `git push origin archive/<descriptive-name>`
4. Add git note with metadata: `git notes add -m "Purpose: ... Created: ... Size: ..."`
5. Update this document with branch details
6. Notify team that this branch should never be merged

---

## See Also

- `.git/info/exclude` - Local git exclude patterns
- `planning/INDEX.md` - Active planning documentation
- `planning/archive/` - Archived planning documents (in develop branch)
