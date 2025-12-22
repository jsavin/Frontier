# Dead Code Removal Implementation Guide

**Created**: 2025-12-21
**Purpose**: Step-by-step guide for removing explicit dead code markers
**Target Audience**: Entry-level C engineers (Haiku) with code cleanup experience

---

## Overview

**Total Effort**: 2 weeks
**Phases**: 2 phases (explicit markers, obsolete platforms)
**Complexity**: ⭐ Low (Haiku-level work)
**Risk**: Very low - code explicitly marked as dead

This is straightforward mechanical work suitable for junior engineers.

---

## Phase 1: Remove "xxx"-Prefixed Blocks

**Timeline**: Week 1, Days 1-3
**Complexity**: ⭐ Very Low (Haiku)
**Risk**: NONE - "xxx" prefix explicitly marks disabled code

### Background

The Frontier codebase uses an "xxx" prefix convention to mark disabled code. Any `#ifdef xxxSOMETHING` indicates code that has been intentionally disabled and should be removed.

**Examples**:
- `#ifdef xxxWIN95VERSION` - Disabled Windows 95 code
- `#ifdef xxxPIKE` - Disabled PIKE product variant code
- `#ifdef xxxfldebug` - Disabled debugging code

### Task 1.1: Inventory xxx-Prefixed Blocks

**Find all blocks**:
```bash
cd /Users/jake/dev/jsavin/Frontier
rg '#ifdef xxx' Common/source --files-with-matches
```

**Create inventory**:
Create file `planning/phase3/implementation/xxx_blocks_inventory.txt`:

```
Common/source/strings.c:1339         #ifdef xxxWIN95VERSION      ~15 lines
Common/source/shellwindow.c:59       #ifdef xxxWIN95VERSION      ~12 lines
Common/source/langpack.c:868         #ifdef xxxWIN95VERSION      ~8 lines
Common/source/shellwindowmenu.c:82   #ifdef xxxPIKE              ~5 lines
Common/source/claybrowserexpand.c:504 #ifdef xxxfldebug          ~10 lines
... (continue for all found)
```

**Deliverable**: Complete inventory with line counts

---

### Task 1.2: Remove xxx-Prefixed Blocks (Mechanical)

**For each file in inventory**:

1. **Open the file**
2. **Find the block** (use line number from inventory)
3. **Verify it's disabled**:
   - Confirm `#ifdef xxxSOMETHING`
   - Confirm no matching `#define xxxSOMETHING` exists
4. **Delete the block**:
   - Delete from `#ifdef xxxSOMETHING`
   - Delete through matching `#endif`
   - Delete all lines in between
5. **Save the file**
6. **Test compilation**:
   ```bash
   make -C frontier-cli clean && make -C frontier-cli
   ```
7. **Commit**:
   ```bash
   git add Common/source/<filename>
   git commit -m "cleanup: Remove xxxSOMETHING dead code block from <filename>

   Removed xxx-prefixed block that was explicitly disabled.
   - Location: <filename>:<line>
   - Block: #ifdef xxxSOMETHING
   - Lines removed: ~<N> lines"
   ```

**Example - strings.c:1339**:

Before:
```c
void somefunction(void) {
    // ... active code ...

#ifdef xxxWIN95VERSION
    if (stringlength (bs) > 16) {
        RECT r;
        r.top = 0;
        r.bottom = 50;
        // ... Windows 95 text measurement code ...
    }
#endif

    // ... more active code ...
}
```

After:
```c
void somefunction(void) {
    // ... active code ...

    // ... more active code ...
}
```

**Process for each file**:
- [ ] Open file
- [ ] Locate #ifdef xxx block
- [ ] Verify no #define xxx
- [ ] Delete entire block
- [ ] Test build
- [ ] Commit with descriptive message

**Critical**: Test build after EACH file modification. Don't batch.

---

### Task 1.3: Verify All Removals

**After all blocks removed**:

1. **Search for remaining xxx blocks**:
   ```bash
   rg '#ifdef xxx' Common/source
   # Should return no results
   ```

2. **Full clean build**:
   ```bash
   make -C frontier-cli clean
   make -C frontier-cli
   # Should succeed with no errors
   ```

3. **Run tests**:
   ```bash
   ./tools/run_headless_tests.sh
   # All tests should pass
   ```

4. **Create summary**:
   Create `planning/phase3/implementation/phase1_completion_summary.txt`:
   ```
   Phase 1: xxx-Prefixed Block Removal - COMPLETE

   Total blocks removed: <N>
   Total lines removed: ~<N>
   Files modified: <N>
   Commits created: <N>

   All tests passing: YES
   Clean build: YES
   No remaining xxx blocks: YES
   ```

---

## Phase 1B: Remove OBSOLETE and NEVER Markers

**Timeline**: Week 1, Days 4-5
**Complexity**: ⭐ Very Low (Haiku)
**Risk**: NONE - explicitly marked dead

### Task 1B.1: Remove OBSOLETE Block (whirlpool.c)

**File**: `Common/source/whirlpool.c:618`
**Size**: ~1000+ lines (large lookup table)

**Locate the block**:
```bash
cd Common/source
grep -n "ifdef OBSOLETE" whirlpool.c
```

**Verify it's obsolete**:
```bash
# Search for any #define OBSOLETE
rg '#define OBSOLETE' .
# Should find nothing
```

**Remove the block**:
1. Open `Common/source/whirlpool.c`
2. Go to line 618 (or wherever `#ifdef OBSOLETE` is)
3. Select from `#ifdef OBSOLETE` through matching `#endif`
4. Delete selection
5. Save file

**Test**:
```bash
make -C frontier-cli clean && make -C frontier-cli
```

**Commit**:
```bash
git add Common/source/whirlpool.c
git commit -m "cleanup: Remove OBSOLETE crypto lookup table from whirlpool.c

Removed 1000+ line obsolete Whirlpool crypto lookup table that was
wrapped in #ifdef OBSOLETE and never used.

- Lines removed: ~1200 lines
- Tables removed: C0-C7 lookup tables"
```

---

### Task 1B.2: Remove NEVER Block (langevaluate.c)

**File**: `Common/source/langevaluate.c:897`
**Size**: ~20 lines (error formatting code)

**Same process**:
1. Find `#ifdef NEVER` block
2. Verify no `#define NEVER`
3. Delete block
4. Test build
5. Commit with message:
   ```
   cleanup: Remove NEVER dead code block from langevaluate.c

   Removed error formatting code wrapped in #ifdef NEVER.
   - Location: langevaluate.c:897
   - Lines removed: ~20 lines
   ```

---

### Task 1B.3: Handle NeverDefine_For_Reference (KEEP IT)

**File**: `Common/source/WinSockNetEvents.c:50`

**IMPORTANT**: This is NOT dead code - it's a **defensive guard**.

**Purpose**: The block contains reference documentation that should only compile if the symbol is accidentally defined.

**Action**: DO NOT REMOVE THIS BLOCK

**Verification**:
```bash
# Verify the block is still there
grep -A 5 "ifdef NeverDefine_For_Reference" Common/source/WinSockNetEvents.c
```

**Document**:
Add to `planning/phase3/implementation/phase1_completion_summary.txt`:
```
Note: NeverDefine_For_Reference in WinSockNetEvents.c:50 was kept.
This is a defensive guard, not dead code.
```

---

## Phase 2: Remove Obsolete Platform Code

**Timeline**: Week 2, Days 1-3
**Complexity**: ⭐ Low (Haiku)
**Risk**: LOW - platforms no longer supported

### Task 2.1: Remove oldMACVERSION Blocks

**Background**: oldMACVERSION was for Mac OS Classic (pre-OSX). Modern Frontier doesn't support this platform.

**Find blocks**:
```bash
rg '#ifdef oldMACVERSION' Common/source
```

**Expected locations**:
- `Common/source/langhash.c` - 3 blocks for Mac alias handling

**For each block**:

1. **Verify it's oldMACVERSION** (not just MACVERSION)
2. **Check v7 format doesn't use this code**:
   - oldMACVERSION code handled Mac aliases
   - v7 format doesn't use aliases
   - Safe to remove
3. **Delete block**
4. **Test build**
5. **Commit**:
   ```bash
   git commit -m "cleanup: Remove oldMACVERSION block from langhash.c

   Removed legacy Mac OS Classic alias handling code.
   v7 format doesn't use Mac aliases, making this code obsolete.

   - Lines removed: ~50 lines
   - Block: Mac alias serialization"
   ```

**Example - langhash.c**:

Before:
```c
#ifdef oldMACVERSION
    case filespecvaluetype: {
        // Mac alias handling
        register hdlfilespec x = val.data.filespecvalue;
        tyfilespec fs = **x;
        AliasHandle halias = nil;
        // ... alias serialization code ...
    }
#endif
```

After:
```c
// (block removed - v7 doesn't use Mac aliases)
```

**Acceptance criteria**:
- [ ] All oldMACVERSION blocks removed
- [ ] Build succeeds
- [ ] Migration test passes (v6→v7 still works)

---

### Task 2.2: Remove Commented WIN95VERSION Blocks

**Background**: Some WIN95VERSION blocks are already commented out. Remove these.

**Find blocks**:
```bash
rg '//.*#ifdef WIN95VERSION' Common/source
# OR
rg '/\*.*#ifdef WIN95VERSION' Common/source
```

**Expected locations**:
- `Common/source/langtrace.c:59` - Trace file logging
- `Common/source/frontierwindows.c:128` - Window display

**For each block**:
1. **Verify block is commented** (has `//` or `/* */`)
2. **Delete entire commented block**
3. **Test build**
4. **Commit**

**Example - langtrace.c**:

Before:
```c
void sometrace(void) {
    // ... code ...

    /*
    #ifdef WIN95VERSION
        // Windows 95 trace file logging
        FILE *f = fopen("trace.log", "a");
        fprintf(f, "trace: %s\n", msg);
        fclose(f);
    #endif
    */

    // ... more code ...
}
```

After:
```c
void sometrace(void) {
    // ... code ...

    // ... more code ...
}
```

**Commit message**:
```
cleanup: Remove commented WIN95VERSION block from langtrace.c

Removed already-commented Windows 95 trace logging code.
- Lines removed: ~15 lines
```

---

### Task 2.3: Keep Active WIN95VERSION Block

**File**: `Common/source/shellsysverbs.c:610`

**IMPORTANT**: This block is NOT commented - it's active.

**Code**:
```c
#ifdef WIN95VERSION
    // Environment variable setting for Windows
#endif
```

**Action**: **KEEP THIS BLOCK**

**Why**: Windows builds still need this for environment variable setting.

**Verification**:
```bash
# Verify block is still there and active (not commented)
grep -A 3 "ifdef WIN95VERSION" Common/source/shellsysverbs.c
```

**Document**:
Add to completion summary:
```
Note: Active WIN95VERSION block in shellsysverbs.c:610 was kept.
This is needed for Windows builds (environment variable setting).
```

---

### Task 2.4: Verify Removals

**After all removals**:

1. **Check for remaining oldMACVERSION**:
   ```bash
   rg '#ifdef oldMACVERSION' Common/source
   # Should return no results
   ```

2. **Check for commented WIN95VERSION**:
   ```bash
   rg '//.*#ifdef WIN95VERSION|/\*.*#ifdef WIN95VERSION' Common/source
   # Should return no results
   ```

3. **Verify active WIN95VERSION still exists**:
   ```bash
   rg '#ifdef WIN95VERSION' Common/source/shellsysverbs.c
   # Should still show the active block
   ```

4. **Test migration**:
   ```bash
   make -C tests clean && make -C tests save_migration_tests
   ./tests/save_migration_tests
   # Should succeed - v6→v7 migration still works
   ```

---

## Testing Protocol

**After each file modification**:
```bash
# 1. Clean build
make -C frontier-cli clean
make -C frontier-cli
# Should succeed with no errors

# 2. Quick test
./frontier-cli/frontier-cli -e "1+1"
# Should print: 2
```

**After each phase**:
```bash
# 1. Full test suite
./tools/run_headless_tests.sh
# All tests should pass

# 2. Migration test
make -C tests clean && make -C tests save_migration_tests
./tests/save_migration_tests
# Should succeed

# 3. Verb bindings check
cd tools/kernelverbs_parser
python3 cli.py analyze
cd ../..
# Should show same verb count as before
```

**Before final commit**:
```bash
# Run all tests on both architectures (if possible)
arch -arm64 ./tools/run_headless_tests.sh  # Apple Silicon
arch -x86_64 ./tools/run_headless_tests.sh # Intel

# Both should pass
```

---

## Git Workflow

### Branching Strategy

```bash
# Create feature branch
git checkout develop
git pull origin develop
git checkout -b cleanup/remove-dead-code-phase1

# Work on phase 1
# ... commits ...

# Push and create PR
git push origin cleanup/remove-dead-code-phase1
gh pr create --title "Dead Code Removal Phase 1: Explicit Markers" \
  --body "$(cat <<'EOF'
## Summary
- Removed xxx-prefixed disabled code blocks (15 blocks)
- Removed OBSOLETE marker block (1200 lines)
- Removed NEVER marker block (20 lines)

## Testing
- ✅ All headless tests pass
- ✅ Migration test passes
- ✅ Clean build on both architectures

## Impact
- ~1,370 lines removed
- 16 blocks eliminated
- ZERO risk (all explicitly marked dead)

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"

# After PR approved and merged, create branch for phase 2
git checkout develop
git pull origin develop
git checkout -b cleanup/remove-dead-code-phase2
```

---

## Commit Message Template

```
cleanup: <Brief description>

<Detailed explanation of what was removed and why>

<Optional context about the code's history>

- Location: <file>:<line>
- Lines removed: ~<N> lines
- Block type: <xxx-prefixed|OBSOLETE|NEVER|platform>
- Risk: ZERO (explicitly marked dead)

🤖 Generated with [Claude Code](https://claude.com/claude-code)

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>
```

**Example**:
```
cleanup: Remove xxxWIN95VERSION dead code from strings.c

Removed Windows 95 text measurement code that was wrapped in
#ifdef xxxWIN95VERSION. The xxx prefix indicates this code was
intentionally disabled and is safe to remove.

- Location: Common/source/strings.c:1339
- Lines removed: ~15 lines
- Block type: xxx-prefixed
- Risk: ZERO (explicitly marked dead)

🤖 Generated with [Claude Code](https://claude.com/claude-code)

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>
```

---

## Progress Tracking

Create file: `planning/phase3/implementation/PROGRESS.md`

Update after each block removed:

```markdown
# Dead Code Removal Progress

## Phase 1: xxx-Prefixed Blocks

### strings.c:1339 - xxxWIN95VERSION
- [x] Located block
- [x] Verified no #define
- [x] Removed code
- [x] Build tested
- [x] Committed
- Commit: abc1234

### shellwindow.c:59 - xxxWIN95VERSION
- [x] Located block
- [x] Verified no #define
- [x] Removed code
- [x] Build tested
- [x] Committed
- Commit: def5678

... (continue for all blocks)

## Summary
Total blocks removed: 16/16
Total commits: 16
Tests passing: YES
Ready for PR: YES
```

---

## Troubleshooting

### Build Fails After Removal

**Problem**: Removed code that was actually used

**Solution**:
1. **Don't panic** - use git to undo
   ```bash
   git diff  # See what you removed
   git checkout -- <filename>  # Restore file
   ```
2. **Investigate** why code was being used:
   - Was there a #define somewhere you missed?
   - Is there a related ifdef you should check?
3. **Escalate** to Sonnet-level engineer

**Prevention**: Always verify no matching #define exists

---

### Tests Fail After Removal

**Problem**: Removed code affected test behavior

**Solution**:
1. **Restore the file**: `git checkout -- <filename>`
2. **Run tests again** to confirm restoration fixes it
3. **Escalate** - this is unexpected for explicitly dead code

---

### Merge Conflict

**Problem**: Someone else modified the same files

**Solution**:
```bash
git checkout develop
git pull origin develop
git checkout cleanup/remove-dead-code-phase1
git merge develop
# Resolve conflicts manually
git add .
git commit
```

If conflicts are complex, escalate to Sonnet-level engineer.

---

## Completion Checklist

### Phase 1 Complete When:
- [ ] All xxx-prefixed blocks removed
- [ ] OBSOLETE block removed
- [ ] NEVER block removed
- [ ] NeverDefine_For_Reference confirmed kept
- [ ] All commits have descriptive messages
- [ ] Build succeeds
- [ ] All tests pass
- [ ] Migration test passes
- [ ] No compiler warnings
- [ ] PR created and approved
- [ ] Branch merged to develop

### Phase 2 Complete When:
- [ ] All oldMACVERSION blocks removed
- [ ] All commented WIN95VERSION blocks removed
- [ ] Active WIN95VERSION block confirmed kept
- [ ] All commits have descriptive messages
- [ ] Build succeeds
- [ ] All tests pass
- [ ] Migration test passes
- [ ] PR created and approved
- [ ] Branch merged to develop

---

## Estimated Timeline

**Phase 1**: 3-4 days
- Day 1: Inventory + remove first 5 xxx blocks
- Day 2: Remove remaining xxx blocks
- Day 3: Remove OBSOLETE and NEVER blocks
- Day 4: Final testing + PR

**Phase 2**: 2-3 days
- Day 1: Remove oldMACVERSION blocks
- Day 2: Remove commented WIN95VERSION blocks
- Day 3: Final testing + PR

**Total**: 5-7 days (with buffer for testing)

---

## Tips for Success

1. **Work methodically**: One block at a time, commit after each
2. **Test frequently**: Build after every file change
3. **Use git**: Commit often, can always undo
4. **Ask for help**: Don't spend >2 hours stuck
5. **Take breaks**: Code cleanup is repetitive, stay fresh
6. **Document**: Update progress tracking regularly

---

*Implementation guide created: 2025-12-21*
*Good luck with the cleanup!*
