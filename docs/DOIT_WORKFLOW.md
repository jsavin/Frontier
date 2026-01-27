# /doit Workflow - Frontier-Specific Guide

**Reference document for /doit agent selection and parallel patterns.**

For the full /doit workflow (phases, decision gates, error handling), see global instructions in `~/.claude/CLAUDE.md`.

This document provides **Frontier-specific** guidance on which agents to use and when to parallelize.

---

## Agent Selection by Phase

| Phase | Primary Agent(s) | When to Use in Parallel | Notes |
|-------|------------------|-------------------------|-------|
| **Phase 2: Research** | Explore | Always use for multi-file analysis | Fast codebase exploration and architectural context |
| **Phase 3: Planning** | Plan, system-architect | Use system-architect if architecture/design decisions needed | Plan for implementation steps, system-architect for tech decisions |
| **Phase 4: Implementation** | system-architect (C code, architecture)<br>usertalk-engineer (UserTalk scripts)<br>odb-database-expert (DB format)<br>logging-expert (logging) | Parallelize independent work:<br>- C implementation + tests<br>- Multiple independent verbs<br>- UserTalk + C components | Choose based on domain:<br>- C runtime → system-architect<br>- UserTalk → usertalk-engineer<br>- Database → odb-database-expert<br>- Logging → logging-expert |
| **Phase 5: Testing** | frontier-sdet | Can parallel with code-review-bar-raiser | Write tests (unit + integration), ensure all pass |
| **Phase 6: PR** | pull-request<br>code-review-bar-raiser<br>security-reviewer | **Always parallel**: All three agents | PR creation + quality review + security review |

---

## Common Parallel Agent Patterns

### New Kernel Verb

1. Explore (find existing verb patterns) + Plan (design approach) - sequential
2. system-architect (C implementation) + frontier-sdet (test design) - **parallel**
3. code-review-bar-raiser + security-reviewer - **parallel**

### Database Format Change

1. Explore (affected code) + odb-database-expert (format analysis) - sequential
2. odb-database-expert (implementation) + frontier-sdet (migration tests) - **parallel**
3. code-review-bar-raiser + security-reviewer - **parallel**

### UserTalk Feature

1. usertalk-engineer (implementation) + frontier-sdet (integration tests) - **parallel**
2. code-review-bar-raiser (review UserTalk patterns) - sequential

### Refactoring Work

1. refactoring-consultant (design refactoring approach) - sequential
2. system-architect (implement refactoring) + frontier-sdet (update tests) - **parallel**
3. code-review-bar-raiser (verify no regressions) - sequential

---

## Agent Working Directory Context - CRITICAL ⚠️⚠️⚠️

**MANDATORY REQUIREMENT**: When launching sub-agents during /doit workflow (or any workflow involving feature branches/worktrees), you MUST explicitly pass the working directory path in the agent's task prompt.

**Why This Matters**:
- Agents don't automatically inherit the correct working directory
- Without explicit path, agents may work in the main Frontier directory on develop
- This introduces breaking changes to develop branch instead of feature branch
- Creates redundant work when changes need to be moved/redone
- Violates the fundamental worktree workflow discipline

**CORRECT Pattern** ✅:
```markdown
When launching agent in /doit workflow Phase 4 or Phase 6:

Task Prompt MUST include:
"WORKING DIRECTORY: /Users/jake/dev/jsavin/Frontier-<feature-name>
BRANCH: feature/<feature-name>

You MUST execute all commands and file operations in this working directory.
Verify your location with 'pwd && git branch' before making any changes.

[Rest of task description...]"
```

**WRONG Pattern** ❌:
```markdown
# Missing working directory context
"Implement the new verb dispatch logic for system.verbs.table.assign..."
# Agent may work in wrong directory!
```

**Verification Steps**:
1. Before launching ANY agent during /doit: Store worktree path from Phase 1
2. Include explicit working directory path in EVERY agent prompt
3. Instruct agent to verify location before starting work
4. After agent completes: Verify changes are in correct worktree

**Code Review Checkpoint**:
When reviewing /doit workflow execution, check:
- ✅ Did all sub-agents receive explicit working directory path?
- ✅ Are changes in the feature branch worktree, not main directory?
- ✅ Does `git status` show changes on feature/* branch, not develop?

**Enforcement**: This is a BLOCKING requirement. Never proceed with agent launch during /doit without explicit working directory context.

---

## Investigation Templates

**For bug investigations**, use these templates in `planning/investigations/`:

### Verb Resolution Bug Template

```markdown
## Verb Resolution Investigation

### Test Case
- Expression: `_______________`
- Expected: `_______________`
- Actual: `_______________`

### Theory
_______________

### Verification Method
- [ ] LLDB trace
- [ ] Git bisect
- [ ] Logging at key points

### LLDB Breakpoints (if applicable)
- langgetdotparams
- langgethandlercode
- langfindsymbol
- langsearchpathvisit
- langdirecttablelookup / langtablelookup
- [error location]

### Results
_______________

### Root Cause
_______________
```

**See also**: [`docs/VERB_RESOLUTION_ARCHITECTURE.md`](VERB_RESOLUTION_ARCHITECTURE.md) and [`docs/DEBUGGING_GUIDE.md`](DEBUGGING_GUIDE.md)
