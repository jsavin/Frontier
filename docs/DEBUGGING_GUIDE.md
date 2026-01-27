# Debugging Guide

**LIVING DOCUMENT**: Add new debugging patterns as we discover them.

## When to Use LLDB vs Logging

### ✅ Use LLDB First For

- Control flow questions ("which path is executing?")
- Unexpected behavior ("why isn't this called?")
- Value inspection ("what's in this variable?")
- Call chain tracing ("how did we get here?")
- **ANY execution flow bug** - debugger shows evidence, not assumptions

### Use Logging For

- Pattern analysis across many executions
- Production debugging (can't attach debugger)
- After LLDB identifies the area to monitor
- Performance profiling

## LLDB Investigation Template

### 1. Create Breakpoint Script

```bash
cat > /tmp/lldb_script.txt << 'EOF'
# Breakpoint at suspected entry point
b function_name
commands
  p variable_name
  bt 5
  c
end

# Breakpoint at error location
b error_function
commands
  p error_context
  bt 10
  frame
end

# Run program
r --system-root databases/Frontier.root7 -e "test expression"
EOF
```

### 2. Run with LLDB

```bash
lldb -s /tmp/lldb_script.txt ./frontier-cli/frontier-cli
```

### 3. Analyze Output

- Which breakpoints hit?
- Call chain matches expectation?
- Variable values as expected?

### 4. Refine Theory

Based on evidence from LLDB, update theory and add more breakpoints if needed.

## Git Bisect for Regressions

### 1. Create Test Script

**CRITICAL**: Script must handle project-specific state (database migration, etc.)

```bash
#!/bin/bash
cd "$(dirname "$0")"

# Rebuild
make -C frontier-cli clean > /dev/null 2>&1
make -C frontier-cli > /dev/null 2>&1

if [ ! -f frontier-cli/frontier-cli ]; then
    echo "Build failed"
    exit 125  # Skip this commit
fi

# PROJECT-SPECIFIC: Force fresh database migration
rm -f databases/Frontier.root7

# Test the behavior
OUTPUT=$(./frontier-cli/frontier-cli --system-root databases/Frontier.root -e "test" 2>&1)

# Check for expected vs broken behavior
if echo "$OUTPUT" | grep -q "error_pattern"; then
    echo "BAD: Feature broken"
    exit 1  # Bad commit
elif echo "$OUTPUT" | grep -q "success_pattern"; then
    echo "GOOD: Feature works"
    exit 0  # Good commit
else
    echo "UNCLEAR: $OUTPUT"
    exit 125  # Skip this commit
fi
```

### 2. Validate Test Script

**MANDATORY before running bisect:**

```bash
# Test at known-good commit
git checkout <good-commit>
./test_script.sh
# MUST exit 0

# Test at known-bad commit
git checkout <bad-commit>
./test_script.sh
# MUST exit 1

# Test at skip scenario if applicable
git checkout <skip-commit>
./test_script.sh
# MUST exit 125
```

### 3. Run Bisect

```bash
git bisect start
git bisect bad <bad-commit>
git bisect good <good-commit>
git bisect run ./test_script.sh
```

### 4. Analyze Breaking Commit

```bash
# Show what changed
git show <breaking-commit>

# Files changed
git diff <breaking-commit>^ <breaking-commit>
```

## Common Debugging Patterns

### Database Migration Issues

**Symptom**: Works with pre-migrated database, fails with fresh migration

**Solution**: Always delete `.root7` and re-migrate during testing

```bash
rm -f databases/Frontier.root7
./frontier-cli/frontier-cli --system-root databases/Frontier.root -e "test"
```

### External Script Loading

**Symptom**: "missing valueroutine" error

**Cause**: Script stored as external, not loaded into memory

**Debug Steps**:
1. Check if `flinmemory=0` in value record
2. Verify external loading happened
3. Trace through `langexternalgettable()` or similar

### Verb Resolution Failures

**Symptom**: "Can't call X" or verb not found

**Debug With LLDB**:
```bash
# Set breakpoints at key resolution points
b langgetdotparams
b langgethandlercode
b langfindsymbol
b langsearchpathvisit
b langdirecttablelookup
```

**Reference**: See [`docs/VERB_RESOLUTION_ARCHITECTURE.md`](VERB_RESOLUTION_ARCHITECTURE.md)

## Investigation Checklist

Before implementing a fix:

- [ ] Stated theory about root cause explicitly
- [ ] Verified theory with evidence (LLDB trace, bisect, etc.)
- [ ] Answered all 5 verification questions (see global CLAUDE.md)
- [ ] Identified potential side effects
- [ ] Confirmed fix addresses root cause, not symptoms

---

## Debugging Lessons: Common Incorrect Assumptions

**Source**: These lessons come from real debugging sessions. They capture patterns where initial assumptions led investigations astray.

### Lesson 1: Check ALL Functions in the Call Chain

**Case Study**: PR #352 - Introspection bugs (`parentOf`, `typeOf` returning wrong values)

**Initial assumption**: "The bug must be in the orchestrator function (`langgetdotparams`)"
- Thought the search order in the top-level function was wrong
- Tried changing the order of operations in `langgetdotparams`
- Result: Stack overflow / infinite recursion

**What was actually wrong**: A function **called by** the orchestrator was doing extra work
- `langexternalgettable()` had explicit EFP search that shouldn't have been there
- The orchestrator's search order was correct
- The bug was in a helper function adding unwanted behavior

**Lesson**: When debugging search order or resolution issues:
1. Trace the ENTIRE call chain, not just the top function
2. Check what each called function does, not just the orchestrator
3. Bug may be extra work in a helper, not wrong order in the caller
4. Compare ALL functions in the call chain between legacy and current code

### Lesson 2: Name Resolution vs Verb Dispatch Are Different

**Case Study**: PR #352 - Removing EFP search from `langexternalgettable`

**Initial concern**: "Removing EFP search will break verb dispatch"
- Worried that kernel verbs like `string.mid()` wouldn't be found
- Seemed like EFP search was needed for verbs to work

**What was actually true**: Two separate code paths with different search orders
- **Name resolution** (`langexternalgettable`) - used for introspection, should find database tables
- **Verb dispatch** (`langhandlercall`) - used for actually calling verbs, has its own EFP search
- EFP search in name resolution was redundant AND broke introspection

**Lesson**: Understand the architecture before making assumptions
1. Name resolution and verb dispatch use different code paths
2. EFP stubs are implementation details for dispatch, not canonical locations for introspection
3. Don't assume a function is the only place something happens - check for parallel code paths
4. Read `docs/VERB_RESOLUTION_ARCHITECTURE.md` before modifying verb lookup code

### Lesson 3: Search Order Bugs May Be Addition, Not Reordering

**Case Study**: PR #352 - EFP search prioritized over database tables

**Initial diagnosis approach**: "Need to change the order of searches"
- Focused on reordering steps in the orchestrator function
- Tried moving database search before external search
- Didn't recognize the problem was an extra search that shouldn't exist

**What actually fixed it**: Removing code, not reordering it
- The ~60 lines of explicit EFP search in `langexternalgettable()` were the entire problem
- No reordering needed - the search order was already correct
- The fix was deleting code that shouldn't have been there

**Lesson**: When debugging search order:
1. Consider that the bug might be an EXTRA search, not wrong order
2. Ask "should this search be here at all?" before asking "should it be earlier/later?"
3. Compare with legacy code to find additions, not just differences in order
4. Sometimes the right fix is deletion, not rearrangement

### Lesson 4: Legacy Comparison Requires Complete Call Chain Analysis

**Case Study**: PR #352 - Comparing search behavior with legacy Frontier

**Initial assumption**: "Legacy code must do something different in the orchestrator"
- Only compared `langgetdotparams()` between legacy and current
- Assumed differences would be in the top-level function

**What we discovered**: Orchestrator was identical, but helper function was different
- `langgetdotparams()` was the SAME in both codebases
- `langexternalgettable()` had extra EFP search in headless code that legacy didn't have
- The addition was ~60 lines deeper in the call chain

**Lesson**: When comparing with legacy code:
1. Compare ALL functions in the call chain, not just the entry point
2. Look for additions in helper functions, not just top-level changes
3. Pay attention to conditional compilation or headless-specific code
4. The difference may be subtle (extra code in one function) even if behavior is very different

### Lesson 5: Symptoms vs Root Cause

**Case Study**: PR #352 - Multiple symptoms, single root cause

**Observed symptoms**:
- `parentOf(string.mid)` returned wrong path
- `typeOf(op.outlineToXml)` returned wrong type
- `defined()` couldn't find some verbs
- Multiple different introspection operations were broken

**Initial thought**: "These might be separate bugs"
- Seemed like multiple unrelated problems
- Each symptom appeared in different code

**Actual root cause**: Single search order issue affecting all introspection
- All symptoms traced to same EFP search priority problem
- All introspection operations use `langexternalgettable()`
- Single fix (removing EFP search) solved all symptoms

**Lesson**: When seeing multiple related symptoms:
1. Look for a common code path that all symptoms share
2. Don't assume multiple symptoms = multiple bugs
3. Architectural bugs often have wide-ranging symptoms
4. Fix the architecture, not individual symptoms

---

## Update History

- 2026-01-25: Initial version created during Issue #344 investigation
- 2026-01-27: Added "Debugging Lessons" section with PR #352 case studies
