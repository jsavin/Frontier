# Frontier Development Guide

## Shared Guidelines (Cross-Agent Source of Truth)

- Read and follow `docs/AI_SHARED_GUIDELINES.md` before starting work.
- `docs/AI_SHARED_GUIDELINES.md` is authoritative for cross-agent policy (branch/worktree discipline, test requirements, logging policy, UserTalk test constraints, issue hygiene, and shared priority usage).
- If this file conflicts with `docs/AI_SHARED_GUIDELINES.md` on a cross-agent rule, follow `docs/AI_SHARED_GUIDELINES.md`.

## Technical Decision-Making Principles

**Default to proper, maintainable, long-term solutions.** Quick fixes accumulate as technical debt that becomes costly to unwind. Unless the user explicitly requests a quick fix for time constraints, recommend the approach that solves the problem correctly rather than suppressing symptoms.

**When in doubt:** Apply the proper fix, not temporary workarounds.

---

## Quick Reference

### Essential Commands

```bash
# Build the CLI (universal binary for arm64 + x86_64)
make -C frontier-cli

# Run full test suite (unit tests)
./tools/run_headless_tests.sh

# Run integration tests (Python/YAML-based verb tests)
cd tests && make test-integration

# Run all tests (unit + integration)
cd tests && make test-all

# Database migration (v6 → v7)
rm -f databases/Frontier.root7
./frontier-cli/frontier-cli --system-root databases/Frontier.root -e "1"

# Verb coverage analysis
cd tools/kernelverbs_parser && python3 cli.py report

# Export integration tests to OPML
python3 tools/export_tests_to_opml.py
# Output: reports/integration_tests.opml

# Install git hooks
./tools/install_git_hooks.sh

# Create PR (after pushing branch)
gh pr create --title "..." --body "..." --base develop
# Start background monitor (auto-backgrounds itself):
./tools/monitor_pr_review.sh <PR_NUMBER>
# Watch with: tail -f tests/tmp/pr_monitor_<PR_NUMBER>.log
```

**Test Output Directory Structure**:
- `tests/tmp/unit/` - C unit test outputs
- `tests/tmp/integration/` - Integration test outputs
- `tests/tmp/migration/` - Migration test artifacts
- `tests/tmp/results/` - Test logs and CLI runtime artifacts

For test scratch files, `/tmp` or `tests/tmp/` both work.

### Documentation Quick Links

**Getting Started**:
- **[Getting Started Guide](docs/GETTING_STARTED.md)** - Complete newcomer guide (build, run, test)
- **[CLI Usage Guide](docs/CLI_USAGE_GUIDE.md)** - Complete frontier-cli reference

**Implementation & Testing**:
- **[Verb Implementation Guide](docs/VERB_IMPLEMENTATION_GUIDE.md)** - Implementing kernel verbs in C
- **[Testing Guide](docs/TESTING_GUIDE.md)** - CLI usage, testing patterns, database migration
- **[Logging Standards](docs/LOGGING_STANDARDS.md)** - Structured logging requirements

**Debugging & Architecture**:
- **[Verb Resolution Architecture](docs/VERB_RESOLUTION_ARCHITECTURE.md)** - How verb lookup works
- **[Debugging Guide](docs/DEBUGGING_GUIDE.md)** - LLDB, git bisect, investigation patterns
- **[Architectural Anti-Patterns](docs/ARCHITECTURAL_ANTIPATTERNS.md)** - Known pitfalls to avoid

**Workflow**:
- **[/doit Workflow Guide](docs/DOIT_WORKFLOW.md)** - Agent selection and parallel patterns
- **[Worktree Workflow](docs/WORKTREE_WORKFLOW.md)** - Feature branch workflow

**Reference**:
- **[UserTalk Documentation](docs/usertalk/docserver/)** - DocServer verb reference (75+ categories)

---

## Pre-Work Verification

Cross-agent pre-work checks and branch/worktree policy now live in `docs/AI_SHARED_GUIDELINES.md`.
Follow that file before starting non-trivial work.

---

## Communication Standards

### Privacy & Entity References

- **NEVER mention specific people or entities** (partnerships, companies, individuals) unless the user explicitly asks
- This includes in commit messages, PR descriptions, code comments, and documentation
- Keep communications focused on technical details, not partnerships or strategic relationships

---

## Permanent Branches (Never Merge to Develop)

- `archive/codex-sessions` - AI session transcripts (~950k lines)
- `archive/portable-refactoring` - Refactoring experiments (348 files, 98k lines)
- `archive/carbon-migration` - Legacy documentation (85 files)

**Full details:** See [`docs/PERMANENT_BRANCHES.md`](docs/PERMANENT_BRANCHES.md)

---

## Strategic Roadmap

**Planning Documentation** (source of truth for strategic direction):
- `planning/INDEX.md` - Navigation for active and archived workstreams
- `planning/phase_overview.md` - Overview of all phases
- `planning/phase6/CRDT_FOUNDATION_ROADMAP.md` - Collaborative ODB foundation roadmap

**Key Project Context**:
- Frontier has "guest databases" - any databases opened that aren't system root
- Current "target" is generally a window (database or editor window)
- Legacy Frontier source: `/Users/jake/dev/tedchoward/Frontier`
- v7 database format should NOT contain font/style info (except within stored RTF objects)
- **Threading model**: GIL (Global Interpreter Lock) — real POSIX threads serialized by a single mutex. Only the GIL holder can access C globals. Yield points at `langbackgroundtask()` and `thread.sleepTicks()`. See ADR-014 for details.

**PR Workflow Requirements** ⚠️:
1. Create feature branch, commit work
2. **BEFORE FIRST PUSH**: Run full test suite (unit + integration)
3. Push feature branch (NEVER push to origin/develop directly)
4. Use pull-request agent to create PR
5. Start PR monitoring: `./tools/monitor_pr_review.sh <PR_NUMBER>`
6. **ALWAYS discuss bot feedback with user before addressing**
7. **NEVER merge PRs without explicit user approval**

---

## Working with Agents

### CRITICAL: ALWAYS Use Sub-Agents ⚠️⚠️⚠️

**DEFAULT BEHAVIOR**: Use sub-agents for ALL non-trivial work unless the user explicitly asks you not to.

- **ALWAYS use agents** for multi-file exploration, implementation, testing, code review
- **ALWAYS parallelize** independent agent work (single message with multiple Task tool calls)
- **Only work directly** if: trivial single-file change OR user explicitly requests visibility

**Why**: Agents have specialized expertise, conserve main conversation context, and can work in parallel.

### Available Agents

| Agent | Use When |
|-------|----------|
| **Explore** | Multi-file codebase exploration, understanding architecture |
| **Plan** | Designing implementation plans, architectural decisions |
| **system-architect** | C domain work, runtime architecture, memory management |
| **usertalk-engineer** | UserTalk scripting, verb implementations in UserTalk domain |
| **odb-database-expert** | Database format, corruption issues, migration |
| **logging-expert** | Logging infrastructure, standards compliance |
| **code-review-bar-raiser** | Pre-merge quality review of significant implementations |
| **refactoring-consultant** | Planning and executing refactoring work |
| **pull-request** | Creating PR summaries, pushing to origin |
| **frontier-sdet** | Test infrastructure, test strategy |
| **claude-code-guide** | Questions about Claude Code, SDK, or API |

### Error Type Delegation

- **UserTalk syntax errors** → `usertalk-engineer` agent
- **Integration test framework issues** → `frontier-sdet` agent
- **Architecture/design decisions** → `system-architect` agent
- **Test content verification** → `frontier-sdet` or `usertalk-engineer` agent

### Agent Verification Requirements ⚠️

Agents must verify fixes work end-to-end, not just fix one piece. Test the complete chain: name resolution → dispatch → implementation.

**📖 Read `docs/DOIT_WORKFLOW.md` for**: Agent selection by phase, parallel patterns, working directory requirements.

---

## /doit Workflow

**Basics**: Use /doit for ALL non-trivial feature work and bug fixes. See global `~/.claude/CLAUDE.md` for full workflow.

**📖 Read `docs/DOIT_WORKFLOW.md` when:**
- Selecting which agent to use for a specific phase
- Parallelizing agent work
- Working on kernel verbs, database changes, or UserTalk features

---

## UserTalk Critical Facts

Cross-agent UserTalk invariants and integration-test parser constraints now live in `docs/AI_SHARED_GUIDELINES.md`.
Use those rules as mandatory baseline guidance.

---

## Critical Testing Constraints

Cross-agent test requirements (including integration test expectations for verb changes) now live in `docs/AI_SHARED_GUIDELINES.md`.
For Frontier-specific test patterns and command details, also see `docs/TESTING_GUIDE.md`.

---

## Architectural Anti-Patterns

**Quick Reference** (1-line summaries):
- **Name resolution vs verb dispatch**: Don't add EFP searches to `langexternalgettable()` - only in `langhandlercall()`
- **Hash table lookup**: Use `hashtablelookupnode()` when you only need the node
- **Global mutable state**: Eliminate before launch, use thread-local or explicit context
- **Address values**: Always use `setexemptaddressvalue()`, never modify handle memory directly
- **Mode stack**: Use context guards, don't rely on push/pop being restored
- **Context guard completeness**: Guards must save/restore ALL globals the guarded operation clears
- **Tmp stack ownership**: Call `exemptfromtmpstack()` after storing heap values in persistent tables

**📖 Read `docs/ARCHITECTURAL_ANTIPATTERNS.md` when:**
- Debugging crashes in hash table, database, or verb resolution code
- Modifying `langexternalgettable()`, `langgetdotparams()`, or `langhandlercall()`
- Working on migration code (v6→v7)
- Encountering "mode stack" or "context guard" patterns
- Adding or modifying global state
- Modifying or creating context guards (`odb_context_guard`, `db_context_guard`)
- Storing heap-allocated values in hash tables (tmp stack ownership)
- Debugging use-after-free crashes (garbage pointers with ASCII content)
- Any significant refactor
- Any time you need to deep-dive on data structures

---

## Debugging & Investigation

**📖 Read `docs/DEBUGGING_GUIDE.md` when:**
- Using LLDB or git bisect
- Investigating verb resolution bugs
- Seeing "loadfromhandle fail" errors (false alarms - format detection logging)
- Debugging database-related crashes

**📖 Read `docs/VERB_RESOLUTION_ARCHITECTURE.md` when:**
- Debugging verb lookup issues
- Understanding how `parentOf()`, `typeOf()`, `defined()` work
- Modifying search order or path resolution

---

## Logging Standards

Cross-agent logging policy is defined in `docs/AI_SHARED_GUIDELINES.md`.
Implementation-level logging details remain in `docs/LOGGING_STANDARDS.md`.

---

## Knowledge Capture

Whenever you discover something significant about this project:
- Architectural insights → `planning/architectural_decision_records/`
- Implementation details → `planning/phase3/` or appropriate phase
- Development guides → `docs/`
- Anti-patterns → `docs/ARCHITECTURAL_ANTIPATTERNS.md`

**Rationale**: Document learning to prevent it from being lost.
