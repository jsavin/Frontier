# Headless Interactive Mode: Batch Flag and TTY Detection

**Status:** Planning → Implementation
**Phase:** Phase 3 (Headless Bring-Up)
**Created:** 2026-01-01
**Owner:** Verb binding workstream

---

## Overview

Frontier's headless mode supports two execution contexts:

1. **Interactive Mode** - Running from a terminal (TTY), allows stdio prompts for user input
2. **Batch Mode** - Running in CI/CD, scripts, daemons, or with `--batch` flag, disallows prompts

This document defines the behavior, detection logic, and implementation strategy for handling user interaction verbs in headless mode.

---

## Table of Contents

1. [Problem Statement](#problem-statement)
2. [Design Decisions](#design-decisions)
3. [Prior Art](#prior-art)
4. [Detection Logic](#detection-logic)
5. [Affected Verbs](#affected-verbs)
6. [Implementation Plan](#implementation-plan)
7. [Testing Strategy](#testing-strategy)
8. [References](#references)

---

## Problem Statement

### Background

Many Frontier verbs require user interaction:
- **Dialog verbs** (`dialog.alert`, `dialog.ask`, `dialog.getInt`, etc.)
- **File dialog verbs** (`file.getFileDialog`, `file.putFileDialog`, etc.)
- **Password prompts** (`dialog.getPassword`)

In GUI mode, these display modal dialogs. In headless mode, we need to decide:

**Question 1:** Should these verbs work in headless mode?
- **Dave Winer's Recommendation:** No - these should cause runtime errors
- **Rationale:** Daemon/server processes shouldn't prompt for input (would hang)

**Question 2:** What about terminal-based interactive use cases?
- **Use Case:** CLI tools, SSH sessions, interactive scripts
- **User Requirement:** Support stdio prompts when running from terminal

**Question 3:** How do we reconcile both needs?
- **Solution:** Auto-detect TTY + explicit `--batch` override

---

## Design Decisions

### Decision 1: Default Behavior Based on TTY Detection

**Default:** Auto-detect interactive vs batch mode using `isatty()`

```c
bool isInteractiveMode() {
    // Force batch mode if --batch flag OR CI environment
    if (fl_batch_mode || getenv("CI")) {
        return false;
    }

    // Auto-detect: interactive if stdin AND stdout are TTYs
    return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
}
```

**Behavior:**
- **Terminal (TTY detected):** Interactive prompts allowed (stdio-based)
- **Pipe/Redirect/Daemon (no TTY):** Error immediately, don't hang
- **CI Environment (`CI=true`):** Force batch mode
- **`--batch` flag:** Force batch mode

### Decision 2: Explicit `--batch` Flag Override

**Flag:** `-b, --batch`

**Purpose:** Force non-interactive mode even when running from a terminal

**Use Cases:**
- Automated testing from terminal
- Scripts that should never prompt
- Reproducible builds
- CI/CD local testing

**Example:**
```bash
# Interactive (TTY detected, prompts allowed)
./frontier-cli -e "dialog.ask('Continue?')"

# Batch mode (TTY detected, but flag forces error)
./frontier-cli --batch -e "dialog.ask('Continue?')"
```

### Decision 3: Phased Implementation

**Phase 1 (Current):** Error-only behavior
- All interactive verbs return `unimplementedverberror` in headless mode
- Add `--batch` flag infrastructure
- Implement `isInteractiveMode()` detection
- Document planned stdio behavior

**Phase 2 (Future):** Stdio prompt implementation
- Implement stdio prompts for `dialog.*` verbs
- Implement stdio prompts for `file.get*Dialog()` verbs
- Add readline-style path completion (optional enhancement)

---

## Prior Art

### Unix Tools with Similar Patterns

| Tool | Auto-Detect TTY? | Non-Interactive Flag | Notes |
|------|------------------|---------------------|-------|
| **GPG** | Yes | `--batch` | "Never ask, do not allow interactive commands" |
| **git** | Yes | `GIT_TERMINAL_PROMPT=0` | Env var to disable prompts |
| **apt** | Yes | `-y, --assumeyes` | Assume "yes" to all prompts |
| **npm** | Yes | `--non-interactive` | Explicit non-interactive mode |
| **pacman** | Yes | `--noconfirm` | Bypass confirmations |
| **ansible** | Yes | `--non-interactive` | Force non-interactive mode |

**Standard Pattern:**
- ✅ Auto-detect TTY (most tools)
- ✅ Flag to force batch mode (common: `--batch`, `--non-interactive`, `-y`)
- ✅ Environment variable for CI (`CI=true`)

**Frontier's Choice:** `--batch` (follows GPG precedent, shorter than `--non-interactive`)

**References:**
- [GPG Batch Mode](https://www.gnupg.org/documentation/manuals/gnupg/Unattended-GPG-key-generation.html)
- [Command Line Interface Guidelines](https://clig.dev/)
- [GitHub: Disable interactive mode discussion](https://github.com/cli/cli/issues/1739)

---

## Detection Logic

### Implementation

**Location:** `frontier-cli/cli_utils.c` (or similar CLI infrastructure file)

```c
#include <unistd.h>
#include <stdlib.h>

/* Global flag set by CLI argument parser */
boolean fl_batch_mode = false;

/**
 * isInteractiveMode - Determine if interactive prompts are allowed
 *
 * Returns true if:
 *   - NOT in batch mode (--batch flag not set)
 *   - CI environment variable not set
 *   - stdin and stdout are both TTYs
 *
 * This function is called by dialog verbs and file dialog verbs to decide
 * whether to prompt via stdio or return unimplementedverberror.
 */
boolean isInteractiveMode(void) {
    /* Force batch mode if --batch flag set */
    if (fl_batch_mode) {
        return false;
    }

    /* Force batch mode if running in CI environment */
    if (getenv("CI")) {
        return false;
    }

    /* Auto-detect: interactive if stdin AND stdout are TTYs */
    return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
}
```

**CLI Argument Parser:**
```c
/* In main.c or cli_parser.c */
static struct option long_options[] = {
    {"batch",         no_argument, 0, 'b'},
    {"non-interactive", no_argument, 0, 'b'},  /* Alias for --batch */
    // ... other options ...
};

/* In argument parsing loop */
case 'b':
    fl_batch_mode = true;
    break;
```

---

## Affected Verbs

### File Processor Dialog Verbs

**Affected:** 4 verbs in `file.*` processor

| Verb | Token | GUI Behavior | Headless Phase 1 | Headless Phase 2 |
|------|-------|--------------|------------------|------------------|
| `getFileDialog` | `sfgetfilefunc` | Native file picker | ❌ Error | ✅ stdio prompt |
| `putFileDialog` | `sfputfilefunc` | Save file picker | ❌ Error | ✅ stdio prompt |
| `getFolderDialog` | `sfgetfolderfunc` | Folder picker | ❌ Error | ✅ stdio prompt |
| `getDiskDialog` | `sfgetdiskfunc` | Volume picker | ❌ Error | ✅ stdio prompt |

**Implementation Location:** `Common/source/fileverbs.c`

**Error Code:** `unimplementedverberror` (#117)

### Dialog Processor Verbs

**Affected:** 19 verbs in `dialog.*` processor

| Category | Verbs | Phase 1 | Phase 2 |
|----------|-------|---------|---------|
| **Prompts** | `ask`, `getInt`, `getUserInfo`, `getPassword` | ❌ Error | ✅ stdio |
| **Alerts** | `alert`, `notify` | ❌ Error | ✅ stdio |
| **Complex Dialogs** | `run`, `runModeless`, `runCard`, etc. | ❌ Error | ❌ Error |
| **Dialog Control** | `getValue`, `setValue`, `setItemEnable`, etc. | ❌ Error | ❌ No-op |

**See:** `planning/phase3/processor_audits/dialog.md` for full analysis

---

## Implementation Plan

### Phase 1: Error-Only Behavior (Current PR)

**Goal:** Make file verbs build successfully, dialog verbs error correctly

**Tasks:**

1. **Add CLI flag infrastructure**
   - [ ] Add `--batch` / `-b` flag to CLI parser
   - [ ] Add `--non-interactive` as alias
   - [ ] Set global `fl_batch_mode` boolean
   - [ ] Test flag parsing

2. **Implement detection logic**
   - [ ] Add `isInteractiveMode()` function to CLI utils
   - [ ] Check `isatty(STDIN_FILENO)` and `isatty(STDOUT_FILENO)`
   - [ ] Check `fl_batch_mode` global
   - [ ] Check `CI` environment variable
   - [ ] Export to verb processors (extern declaration)

3. **Wrap file dialog verbs**
   - [ ] Add conditional compilation to `fileverbs.c`
   - [ ] Wrap `sfgetfilefunc`, `sfputfilefunc`, `sfgetfolderfunc`, `sfgetdiskfunc`
   - [ ] Return `unimplementedverberror` in headless mode
   - [ ] Add TODO comments for Phase 2 stdio implementation

4. **Documentation**
   - [x] Create this planning document
   - [ ] Update `docs/HEADLESS_ADAPTATIONS.md` with link
   - [ ] Update `planning/phase3/processor_audits/file.md` with link
   - [ ] Update `docs/CLI_USAGE_GUIDE.md` with `--batch` flag

5. **Testing**
   - [ ] Test `--batch` flag sets `fl_batch_mode`
   - [ ] Test `isInteractiveMode()` returns false when `--batch` set
   - [ ] Test `isInteractiveMode()` returns false when not TTY
   - [ ] Test file dialog verbs return error in headless mode
   - [ ] Test error message is clear and helpful

**Success Criteria:**
- ✅ `make` builds successfully (file verbs compile)
- ✅ File dialog verbs return `unimplementedverberror` in headless mode
- ✅ Error message indicates `--batch` mode or non-TTY context
- ✅ `./frontier-cli --batch -e "1"` works
- ✅ Tests pass

### Phase 2: Stdio Prompt Implementation (Future PR)

**Goal:** Enable interactive prompts when running from terminal

**Status:** Ready for implementation (spec complete)

---

## Interactive Dialog UX Specification

### Design Principles

1. **Interactive Selection** - Use arrow keys/tab to navigate options (no typing ambiguous text)
2. **Visual Feedback** - Inverted text for selected option, dots for password input
3. **Defaults Visible** - Always show default value when provided
4. **Full Path Returns** - All file dialogs must return absolute paths (UserTalk requirement)
5. **Developer-Focused** - Show hidden files, assume technical users

### Dialog Verb Specifications

#### `msg()` - Output (Already Works)
```usertalk
msg("Starting backup process")
```
**Terminal Output:**
```
Starting backup process
```
**Note:** Uses stdout, already implemented. Verify behavior in Phase 2.

#### `dialog.ask()` - Yes/No Prompt
```usertalk
local(confirmed = dialog.ask("Proceed with backup?"))
```
**Terminal Output:**
```
Proceed with backup? Yes No
                     ^^^
                     (inverted text on selected option)
```
**Behavior:**
- Default option shown in inverted text (white on black)
- Arrow keys or Tab/Shift-Tab to switch selection
- Enter to confirm
- Returns boolean (true/false)

#### `dialog.getInt()` - Integer Input with Default
```usertalk
local(count = dialog.getInt("How many iterations?", 10))
```
**Terminal Output:**
```
How many iterations? [10]: _
```
**Behavior:**
- Shows default in brackets
- Enter with no input accepts default
- Type number + Enter to override
- Returns long integer

#### `dialog.getString()` - Text Input with Default
```usertalk
local(name = dialog.getString("Enter project name", "MyProject"))
```
**Terminal Output:**
```
Enter project name [MyProject]: _
```
**Behavior:**
- Shows default in brackets
- Enter with no input accepts default
- Type text + Enter to override
- Basic line editing (backspace, arrow keys, Ctrl+A/E via readline/libedit)
- Returns string

#### `dialog.getPassword()` - Password Input
```usertalk
local(password = dialog.getPassword("Enter encryption key"))
```
**Terminal Output:**
```
Enter encryption key: ••••••••••
```
**Behavior:**
- Echo disabled via termios
- Shows dots (U+2022) for visual feedback
- Enter to confirm
- Ctrl+C kills UserTalk thread
- Returns string

---

### File Dialog Specifications

**Common Behavior (All File Dialogs):**
- **Interactive menu** - Zsh/Fish-style auto-complete with visual selection
- **Arrow key navigation** - Up/down to select files/folders
- **Tab completion** - Shows menu of matches, auto-completes unambiguous paths
- **Enter on folder** - Descends into directory, shows contents
- **Enter on file** - Confirms selection
- **Backspace in empty path** - Goes up one directory level
- **Shows metadata** - File size, type (file/<dir>), modification date
- **Shows hidden files** - Files starting with `.` always visible (developer tool)
- **Returns full path** - Always absolute path (UserTalk requirement)
- **Starting directory:**
  - If output address contains a path → start from that directory
  - Otherwise → start from current working directory (cwd)
  - (Matches legacy Frontier behavior)

#### `file.getFileDialog()` - Select Existing File
```usertalk
file.getFileDialog("Select config file", @result)
```
**Terminal Output:**
```
Select config file: /Users/jake/project/[TAB]

  .gitignore                   156 B   2024-01-13
> config.txt                   4 KB    2024-01-14  ← inverted
  data/                        <dir>   2024-01-12
  logs/                        <dir>   2024-01-13
  README.md                    2 KB    2024-01-13

↑↓ to select | Enter to confirm | Tab to complete | Esc to cancel
```
**Behavior:**
- Lists all files and directories (including hidden)
- Only allows selecting files that exist
- Descending into directories updates menu
- Returns filespec with full absolute path

#### `file.putFileDialog()` - Save File (Create or Overwrite)
```usertalk
file.putFileDialog("Save output as", @result)
```
**Terminal Output:**
```
Save output as: /Users/jake/project/[TAB]

  .gitignore                   156 B   2024-01-13
  config.txt                   4 KB    2024-01-14
> data/                        <dir>   2024-01-12  ← inverted
  logs/                        <dir>   2024-01-13
  README.md                    2 KB    2024-01-13

↑↓ to select | Enter to confirm | Tab to complete | Type filename
```
**Behavior:**
- Browse directories like `getFolderDialog()` but showing all files
- Showing existing files prevents accidental overwrites (user sees what exists)
- After selecting directory, allow typing filename after trailing slash:
  ```
  Save output as: /Users/jake/project/output.txt_
  ```
- Can select existing file to overwrite
- Returns filespec with full absolute path

#### `file.getFolderDialog()` - Select Directory
```usertalk
file.getFolderDialog("Select output directory", @result)
```
**Terminal Output:**
```
Select output directory: /Users/jake/[TAB]

> .config/                    <dir>   2024-01-10  ← inverted
  Desktop/                    <dir>   2024-01-14
  Documents/                  <dir>   2024-01-13
  Downloads/                  <dir>   2024-01-14
  project/                    <dir>   2024-01-12

↑↓ to select | Enter to confirm | Tab to complete
```
**Behavior:**
- Only lists directories and symlinks (no regular files)
- Enter on directory either:
  - Descends if browsing deeper
  - Confirms if this is final selection
- Returns filespec with full absolute path to directory

#### `file.getDiskDialog()` - Select Volume
```usertalk
file.getDiskDialog("Select backup volume", @result)
```
**Terminal Output:**
```
Select backup volume:

> /                            931 GB  macOS System     ← inverted
  /Volumes/Backup              2 TB    External Drive
  /Volumes/TimeMachine         1 TB    External Drive
  /System/Volumes/Data         <mount>

↑↓ to select | Enter to confirm
```
**Behavior:**
- Lists mounted volumes/filesystems
- Shows total size and label/description
- Returns full path to volume mount point

---

### Implementation Details

**Phase 2A: Basic Dialog Prompts** (1-2 days)
- [ ] Verify `msg()` output behavior
- [ ] `dialog.ask()` with arrow key selection (inverted text rendering)
- [ ] `dialog.getInt()` with default in brackets
- [ ] `dialog.getString()` with readline/libedit line editing
- [ ] `dialog.getPassword()` with termios echo disable + dot rendering

**Phase 2B: File Dialogs with Tab Completion** (3-4 days)
- [ ] Tab completion engine (POSIX only: `readdir()`, `stat()`, termios, ANSI codes)
- [ ] Visual menu rendering (file metadata, scrolling for long lists)
- [ ] `file.getFileDialog()` - select existing files only
- [ ] `file.putFileDialog()` - browse + type filename
- [ ] `file.getFolderDialog()` - directories and symlinks only
- [ ] `file.getDiskDialog()` - volume enumeration
- [ ] Starting directory logic (output address path vs cwd)

**Phase 2C: Integration & Testing** (1-2 days)
- [ ] Replace error stubs with `isInteractiveMode()` checks
- [ ] Batch mode forces errors (unchanged from Phase 1)
- [ ] Integration tests with scripted input
- [ ] Test defaults (Enter accepts default)
- [ ] Test Ctrl+C behavior (kills UserTalk thread)
- [ ] Edge cases: very long paths, many files, unicode filenames
- [ ] Documentation updates

**Implementation Notes:**
- **No external dependencies** - Use POSIX APIs only (termios, readdir, stat)
- **Terminal control** - ANSI escape codes for cursor movement and inverted text
- **Readline/libedit** - Use standard library for line editing (available on macOS/Linux)
- **Thread safety** - Prompts use stdin/stdout, safe in single-threaded CLI context
- **Fuzzy matching** - Deferred (nice-to-have, doesn't work with putFileDialog)

**Success Criteria:**
- ✅ `dialog.ask()` shows inverted text selection
- ✅ `dialog.getInt()` accepts defaults on Enter
- ✅ `dialog.getPassword()` shows dots, no echo
- ✅ `file.getFileDialog()` shows interactive menu with tab completion
- ✅ `file.putFileDialog()` allows typing new filename
- ✅ All file dialogs return full absolute paths
- ✅ All verbs error gracefully in batch mode
- ✅ CI environment auto-detects batch mode

---

## Testing Strategy

### Phase 1 Tests

**CLI Flag Parsing:**
```bash
# Test --batch flag sets mode
./frontier-cli --batch -e "sys.version()" 2>&1 | grep -v "Error"

# Test --non-interactive alias
./frontier-cli --non-interactive -e "sys.version()"

# Test short form
./frontier-cli -b -e "sys.version()"
```

**TTY Detection:**
```bash
# Interactive (TTY): should detect as interactive
./frontier-cli -e "1"

# Pipe (no TTY): should detect as batch
echo "1" | ./frontier-cli -e -

# Batch flag overrides TTY
./frontier-cli --batch -e "1"
```

**File Dialog Verbs (Phase 1 - Should Error):**
```yaml
# tests/integration/test_cases/file_verbs.yaml

- name: "file.getFileDialog - not implemented in headless"
  script: |
    file.getFileDialog("Select a file", @result)
  expected_success: false
  expected_error: "not implemented"
```

### Phase 2 Tests

**Interactive Dialog Tests:**

**Test 1: dialog.ask() with default selection**
```bash
# Simulate Enter key (accepts default "Yes")
echo -e "\n" | ./frontier-cli -e "dialog.ask('Continue?')"
# Expected: true (Yes is default)
```

**Test 2: dialog.ask() with arrow key selection**
```bash
# Simulate Right Arrow + Enter (selects "No")
printf '\x1b[C\n' | ./frontier-cli -e "dialog.ask('Continue?')"
# Expected: false
```

**Test 3: dialog.getInt() with default**
```bash
# Simulate Enter key (accepts default)
echo -e "\n" | ./frontier-cli -e "dialog.getInt('How many?', 10)"
# Expected: 10
```

**Test 4: dialog.getInt() override default**
```bash
# Type new value
echo "42" | ./frontier-cli -e "dialog.getInt('How many?', 10)"
# Expected: 42
```

**Test 5: dialog.getPassword()**
```bash
# Type password + Enter
echo "secret123" | ./frontier-cli -e "dialog.getPassword('Password')"
# Expected: "secret123" (dots shown during input)
```

**File Dialog Tests:**

**Test 6: file.getFileDialog() tab completion**
```bash
# Simulate typing partial path + Tab + Arrow + Enter
# (Complex - requires scripting terminal input)
# Expected: Full path to selected file
```

**Test 7: file.putFileDialog() type new filename**
```bash
# Navigate to directory + type new filename
# Expected: Full path to new file (may not exist yet)
```

**Test 8: file.getFolderDialog() directory selection**
```bash
# Navigate and select directory
# Expected: Full path to directory
```

**Batch Mode Tests (Phase 1 behavior maintained):**

**Test 9: Batch mode forces errors**
```bash
# Force batch mode, expect error
./frontier-cli --batch -e "dialog.ask('Continue?')" 2>&1 | grep "not implemented"
# Expected: Error message, exit code 1

./frontier-cli --batch -e "file.getFileDialog('Select', @f)" 2>&1 | grep "not implemented"
# Expected: Error message, exit code 1
```

**Test 10: CI environment auto-detection**
```bash
# CI env forces batch mode
CI=true ./frontier-cli -e "dialog.ask('Continue?')" 2>&1 | grep "not implemented"
# Expected: Error message (batch mode forced by CI env)
```

**Edge Case Tests:**

**Test 11: Very long file paths**
```bash
# Test path with 256+ characters
# Expected: Handles gracefully, scrolls horizontally
```

**Test 12: Unicode filenames**
```bash
# Test files with emoji, CJK characters, etc.
# Expected: Displays correctly in menu
```

**Test 13: Ctrl+C during prompt**
```bash
# Simulate Ctrl+C (SIGINT)
# Expected: Kills UserTalk thread, exits frontier-cli
```

**Test 14: Hidden files visibility**
```bash
# Directory with .gitignore, .env, etc.
# Expected: All hidden files visible in menu
```

**Test 15: Starting directory logic**
```yaml
# Integration test
- name: "file.getFileDialog - respects output address path"
  script: |
    local(pathvar = "/Users/jake/Documents/");
    file.getFileDialog("Select file", @pathvar)
  expected_behavior: "Starts in /Users/jake/Documents/"

- name: "file.getFileDialog - defaults to cwd"
  script: |
    file.getFileDialog("Select file", @result)
  expected_behavior: "Starts in current working directory"
```

---

## References

### Planning Documents
- [Dialog Processor Audit](processor_audits/dialog.md) - Full stdio implementation plan
- [File Processor Audit](processor_audits/file.md) - File verb categorization

### Implementation Guides
- [Headless Adaptations](../../docs/HEADLESS_ADAPTATIONS.md) - General headless patterns
- [CLI Usage Guide](../../docs/CLI_USAGE_GUIDE.md) - CLI flags and options

### External References
- [GPG Batch Mode](https://www.gnupg.org/documentation/manuals/gnupg/Unattended-GPG-key-generation.html)
- [Command Line Interface Guidelines](https://clig.dev/)
- [GitHub: cli/cli #1739 - Disable interactive mode](https://github.com/cli/cli/issues/1739)
- [Baeldung: Scripting Yes During Install](https://www.baeldung.com/linux/scripting-yes-during-install)

---

## Decision Log

| Date | Decision | Rationale |
|------|----------|-----------|
| 2026-01-01 | Use `--batch` flag (not `-i`) | Auto-detect TTY by default, override with batch flag (follows GPG) |
| 2026-01-01 | Check `CI` environment variable | Standard practice for CI/CD detection |
| 2026-01-01 | Phase 1: error-only | Get file verbs working immediately, defer stdio complexity |
| 2026-01-01 | `isatty()` on stdin AND stdout | Both must be TTY for interactive mode (safety) |

---

**Last Updated:** 2026-01-01
**Status:** ✅ Approved - Ready for Implementation (Phase 1)
