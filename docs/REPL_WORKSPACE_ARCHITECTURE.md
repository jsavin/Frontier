# REPL Workspace Architecture

## Overview

The Frontier CLI REPL (Read-Eval-Print Loop) provides an interactive environment similar to browser JavaScript consoles. This document explains how REPL variables are stored, managed, and cleared.

**Key Principle**: REPL variables are **ephemeral** (not persisted to database), while system globals and `root.workspace.*` remain accessible and persistent.

---

## Architecture Design

### Storage Location: Thread-Local Hash Table

REPL variables are stored in `currenthashtable`, which points to the thread's local hash table stored in `tythreadglobals.htable`.

**Why Thread-Local?**
- ✅ **Ephemeral by design**: Lives in memory only, never serialized to database
- ✅ **Standard Frontier pattern**: Local script scopes use the same mechanism
- ✅ **Automatic cleanup**: Disposed when REPL exits
- ✅ **Isolated**: No parent scope (`prevhashtable = nil`)

**Structure**:
```c
typedef struct repl_workspace_t {
    hdlhashtable workspace_table;  // Points to currenthashtable (thread-local)
    boolean initialized;
} repl_workspace;

// On initialization:
newhashtable(&hnew);
(**hnew).fllocaltable = true;      // Mark as ephemeral
(**hnew).prevhashtable = nil;      // No parent scope
currenthashtable = hnew;            // Set as active scope
```

---

## Variable Resolution

### Name Resolution Order

When user types `x` in REPL:

1. **Check `currenthashtable`** (REPL workspace) ← **Found here** if `x` is REPL variable
2. **Check parent scopes**: `prevhashtable` chain (none for REPL, because `prevhashtable = nil`)
3. **Check `roottable`**: System globals (`system.*`), persistent data (`root.workspace.*`)

### Variable Assignment Examples

```usertalk
# Ephemeral REPL variable (stored in currenthashtable)
> x = 42
> x
42

# Persistent database variable (stored in root.workspace)
> root.workspace.note = "Keep this"
> root.workspace.note
"Keep this"

# Shorthand for root.workspace (when system.paths is loaded)
> workspace.mynote = "Also persistent"
> workspace.mynote
"Also persistent"
```

**Critical Distinction**:
- **Simple names** (`x`, `y`, `z`) → REPL workspace (ephemeral)
- **Dotted paths** (`root.workspace.x`, `workspace.x`) → Database tables (persistent)

---

## `/clear` Command

### What It Does

Empties the REPL workspace hash table, removing all ephemeral variables.

**Implementation**:
```c
boolean repl_workspace_clear(repl_workspace *ws) {
    hdlhashtable ht = ws->workspace_table;

    // Safety check: refuse to clear non-local tables
    if (!(**ht).fllocaltable) {
        log_error(LOG_COMP_GENERAL, "Refusing to clear non-local table");
        return false;
    }

    // Empty the hash table (disposes all entries)
    return emptyhashtable(ht, false);
}
```

### Effects of `/clear`

**What Gets Cleared** ✅:
- All REPL-local variables (`x`, `y`, `z`, etc.)
- Memory allocated for REPL variable values

**What Remains Intact** ❌:
- `system.*` globals (system.paths, system.compiler, etc.)
- `root.workspace.*` persistent data
- Any other database tables
- System state and name resolution

**User Experience** (matches browser console behavior):
```usertalk
> x = 42
> y = "hello"
> root.workspace.note = "persistent"

> /clear
Workspace cleared

> x
Error: Can't find x

> root.workspace.note
"persistent"

> sizeOf(system)
# Still works - system globals intact
```

---

## Safety Mechanisms

### 1. Prevent Accidental Database Writes

**Risk**: User types `workspace.x = 42` expecting REPL workspace, but actually writes to `root.workspace.x` (persisted).

**Mitigation**:
- REPL workspace has **no name in roottable** (not linked as `root.repl` or `system.compiler.repl`)
- Dotted paths like `workspace.x` **always** resolve via `roottable` → finds `root.workspace`
- Users must use simple names (`x`, not `workspace.x`) for REPL variables

**Documentation in CLI help**:
```
REPL Variables:
  x = 42              ← Ephemeral (REPL workspace, cleared by /clear)
  root.workspace.x    ← Persistent (saved to database)
  workspace.x         ← Persistent (shorthand for root.workspace.x)

Use simple names (x, y, z) for temporary REPL variables.
Use dotted paths (root.workspace.*) for persistent data.
```

### 2. Prevent Clearing Persisted Data

**Risk**: Bug in `/clear` implementation accidentally empties `root.workspace`.

**Mitigation**:
```c
// Safety check before emptying
if (!(**ht).fllocaltable) {
    log_error(LOG_COMP_GENERAL, "Refusing to clear non-local table");
    return false;
}
```

**Test Case**: Verify `/clear` refuses to clear `root.workspace` even if REPL context corrupted.

### 3. Memory Leaks on REPL Exit

**Risk**: REPL workspace not disposed on exit → memory leak.

**Mitigation**:
- `repl_workspace_cleanup()` explicitly calls `disposehashtable()`
- Hash table disposed when REPL exits

---

## Design Alternatives Considered

### Alternative 1: Spawn Thread on `/clear` (User's Initial Idea)

**Description**: Create UserTalk thread for REPL, `/clear` = kill thread + spawn new one.

**Why Rejected**:
- ❌ **Overkill**: Full thread lifecycle management for simple variable reset
- ❌ **Complexity**: Thread management adds 200+ lines for 3-line solution
- ❌ **No benefit**: `emptyhashtable()` achieves same effect
- ❌ **System state**: Thread context includes error hooks, process stack - irrelevant to REPL

**Verdict**: Clever idea, but overengineered. Thread-local hash table is simpler.

### Alternative 2: Dedicated `system.compiler.repl` Table

**Description**: Create ephemeral table at `system.compiler.repl` for REPL variables.

**Why Rejected**:
- ❌ **Name collision risk**: `system.compiler` is global namespace
- ❌ **Persists across sessions**: If user saves database, gets serialized (undesired)
- ❌ **Name resolution complexity**: Would need custom lookup chain
- ✓ **One upside**: Easy to inspect (`sizeOf(system.compiler.repl)`)

**Verdict**: Not worth complexity when thread-local table works perfectly.

### Alternative 3: Temporary Table Not Linked to System

**Description**: Create standalone hash table not linked to any parent.

**Why Rejected**:
- ❌ **Reinvents thread-local storage**: Manual management instead of `tythreadglobals`
- ❌ **Not idiomatic Frontier**: Thread-local hash tables are standard pattern

**Verdict**: Use existing infrastructure, don't reinvent.

---

## Comparison to Legacy QuickScript Window

**QuickScript**: One-shot execution window in classic Frontier GUI
- Variables did NOT persist across executions
- No `/clear` command (window close = reset)
- Primarily for quick tests, not interactive sessions

**REPL**: Interactive console (like browser DevTools)
- Variables persist within session
- `/clear` resets to clean slate
- Designed for exploratory programming and debugging

**Key Difference**: REPL maintains state across evaluations until explicit `/clear`, making it more like modern browser consoles than QuickScript.

---

## Future Enhancements (Out of Scope for Phase 1)

### Phase 2: REPL History Persistence

Store command history (not variable values) in `system.compiler.repl.history`:
```usertalk
# On each REPL input:
lang.new(listType, @system.compiler.repl.history)
list.add(@system.compiler.repl.history, user_input)
```

### Phase 3: REPL Inspection Commands

```
/vars               List all REPL variables
/sizeof <var>       Show size of REPL variable
/type <var>         Show type of REPL variable
```

### Phase 4: REPL Workspace Snapshots

```
/save <name>        Save REPL workspace to root.workspace.snapshots.<name>
/load <name>        Load REPL workspace from snapshot
```

---

## References

- **Implementation**: `frontier-cli/repl_eval.c`
- **Command Handler**: `frontier-cli/repl_commands.c`
- **Thread Globals**: `Common/headers/processinternal.h:118-168`
- **Design Doc**: `planning/phase4/REPL_INTERACTIVE_MODE_DESIGN.md`
- **Testing Guide**: `tests/integration/REPL_TESTING_GUIDE.md`

---

## Summary

**Architecture**: Thread-local hash table (`tythreadglobals.htable`)

**Key Principles**:
1. **Ephemeral by default**: REPL variables don't persist
2. **Matches user expectations**: Behaves like browser console
3. **Safety first**: `fllocaltable` check prevents data loss
4. **Simple is better**: Leverage existing Frontier patterns

**User Experience**: Clean, intuitive REPL matching modern console expectations while preserving access to Frontier's powerful database and scripting capabilities.
