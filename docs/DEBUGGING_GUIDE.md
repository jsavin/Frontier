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

## Update History

- 2026-01-25: Initial version created during Issue #344 investigation
