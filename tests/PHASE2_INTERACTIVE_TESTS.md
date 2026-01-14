# Phase 2 Interactive Prompt Tests

**Status:** Test specifications complete, awaiting implementation
**Phase:** Phase 2 - Stdio-based interactive prompts
**Created:** 2026-01-13

---

## Overview

This document describes tests for Phase 2 headless interactive mode implementation. Phase 2 adds stdio-based interactive prompts for dialog and file dialog verbs when `isInteractiveMode()` returns true.

**Phase 1 (Merged):** CLI flags (`--batch`), TTY detection, `isInteractiveMode()`
**Phase 2 (In Progress):** Interactive stdio prompts with visual feedback and tab completion

---

## Test Files

### Automated Integration Tests

1. **`tests/integration/test_cases/dialog_verbs.yaml`** (30 tests)
   - Dialog prompt verbs: `msg()`, `dialog.ask()`, `dialog.getInt()`, `dialog.getString()`, `dialog.getPassword()`
   - Arrow key selection, defaults, input validation
   - Batch mode error handling (Phase 1 behavior preserved)

2. **`tests/integration/test_cases/file_dialog_verbs.yaml`** (28 tests)
   - File dialog verbs: `file.getFileDialog()`, `file.putFileDialog()`, `file.getFolderDialog()`, `file.getDiskDialog()`
   - Tab completion, navigation, hidden files visibility
   - Starting directory logic, full path returns
   - Batch mode error handling

**Total:** 58 automated test cases

---

## Running Tests

### Automated Tests (After Implementation)

```bash
# Run all integration tests
cd tests && make test-integration

# Run only dialog verb tests
cd tests && python3 integration/runner.py integration/test_cases/dialog_verbs.yaml

# Run only file dialog verb tests
cd tests && python3 integration/runner.py integration/test_cases/file_dialog_verbs.yaml
```

### Manual Interactive Tests

Some features require manual verification (arrow keys, visual display, Ctrl+C):

```bash
# Test interactive dialog prompts
./frontier-cli -e "dialog.ask('Continue?')"
# Expected: "Continue? Yes No" with Yes in inverted text
# Press Enter → returns true
# Press Right Arrow + Enter → returns false

./frontier-cli -e "dialog.getInt('How many?', 10)"
# Expected: "How many? [10]: "
# Press Enter → returns 10
# Type "42" + Enter → returns 42

./frontier-cli -e "dialog.getPassword('Enter key')"
# Expected: "Enter key: " then dots as you type
# Type "secret" + Enter → returns "secret" (dots shown during input)

# Test file dialog interactive menu
./frontier-cli -e "file.getFileDialog('Select file', @f); return f"
# Expected: Visual menu with file list, metadata, arrow key navigation
# Press Tab → shows/updates file list
# Arrow keys → select file
# Enter → confirms selection
```

---

## Test Categories

### 1. Dialog Prompt Tests

**Verbs Tested:**
- `msg()` - Output to stdout
- `dialog.ask()` - Yes/No prompt with arrow key selection
- `dialog.getInt()` - Integer input with default
- `dialog.getString()` - Text input with default
- `dialog.getPassword()` - Password input with dots

**Test Scenarios:**
- ✅ Accept default values with Enter
- ✅ Override defaults with typed input
- ✅ Arrow keys and Tab navigation
- ✅ Input validation and re-prompting
- ✅ Empty input handling
- ✅ Unicode input (CJK, emoji)
- ✅ Very long input (500+ chars)
- ✅ Special characters in passwords
- ✅ Batch mode error handling
- ✅ CI environment auto-detection

### 2. File Dialog Tests

**Verbs Tested:**
- `file.getFileDialog()` - Select existing file
- `file.putFileDialog()` - Browse + type new filename
- `file.getFolderDialog()` - Select directory only
- `file.getDiskDialog()` - Select volume

**Test Scenarios:**
- ✅ Tab completion and visual menu
- ✅ File metadata display (size, type, date)
- ✅ Directory navigation with Enter/Backspace
- ✅ Hidden files visibility (.gitignore, .env, etc.)
- ✅ Full absolute path returns (UserTalk requirement)
- ✅ Starting directory logic (output address vs cwd)
- ✅ Unicode filenames (CJK, emoji)
- ✅ Filenames with spaces
- ✅ Very long paths (256+ chars)
- ✅ Batch mode error handling
- ✅ CI environment auto-detection

### 3. Edge Cases

**Test Scenarios:**
- ✅ Very long input strings (500+ chars)
- ✅ Very long file paths (256+ chars)
- ✅ Unicode text and filenames (CJK, emoji)
- ✅ Special characters in input and filenames
- ✅ Empty input handling
- ✅ Invalid input re-prompting
- ✅ Large numbers (INT_MAX)
- ✅ Negative numbers
- ✅ Many files in directory (150+ for scrolling)
- ✅ Ctrl+C abort handling

---

## Manual Testing Procedures

### Test 1: dialog.ask() Visual Feedback

**Objective:** Verify inverted text selection and arrow key navigation

**Steps:**
1. Run: `./frontier-cli -e "dialog.ask('Proceed with backup?')"`
2. **Expected Output:**
   ```
   Proceed with backup? Yes No
                        ^^^
   ```
   (Yes should be in inverted text: white on black)
3. Press **Right Arrow** → "No" should become inverted
4. Press **Left Arrow** → "Yes" should become inverted again
5. Press **Tab** → should toggle between options
6. Press **Enter** → should confirm selection and return boolean

**Success Criteria:**
- ✅ Default option (Yes) is inverted initially
- ✅ Arrow keys switch selection with visual feedback
- ✅ Tab toggles between options
- ✅ Enter confirms and returns correct boolean

---

### Test 2: dialog.getPassword() Dot Display

**Objective:** Verify password masking with dots

**Steps:**
1. Run: `./frontier-cli -e "dialog.getPassword('Enter encryption key')"`
2. **Expected Output:**
   ```
   Enter encryption key: _
   ```
3. Type "secret123" (9 characters)
4. **Expected Display During Typing:**
   ```
   Enter encryption key: •••••••••
   ```
   (9 dots, one per character)
5. Press **Enter**
6. **Expected Result:** Script returns "secret123"

**Success Criteria:**
- ✅ Echo is disabled (characters not visible)
- ✅ Dots (U+2022) appear for each typed character
- ✅ Password returned correctly as string
- ✅ Backspace removes last dot

---

### Test 3: file.getFileDialog() Tab Completion Menu

**Objective:** Verify Zsh/Fish-style interactive menu with tab completion

**Steps:**
1. Create test files:
   ```bash
   mkdir -p /tmp/test_dialog
   touch /tmp/test_dialog/{config.txt,data.json,README.md,.gitignore}
   ```
2. Run: `./frontier-cli -e "file.getFileDialog('Select file', @f); return f"`
3. Type "tests/tmp/test" and press **Tab**
4. **Expected Output:**
   ```
   Select file: /tmp/test_dialog/

     .gitignore                   156 B   2024-01-13
   > config.txt                   4 KB    2024-01-14  ← inverted
     data.json                    2 KB    2024-01-13
     README.md                    1 KB    2024-01-13

   ↑↓ to select | Enter to confirm | Tab to complete | Esc to cancel
   ```
5. Press **Down Arrow** → data.json should become inverted
6. Press **Up Arrow** → config.txt should become inverted again
7. Press **Enter** → should confirm selection

**Success Criteria:**
- ✅ Tab shows visual menu with file list
- ✅ File metadata displayed (size, type, date)
- ✅ Hidden files (.gitignore) visible
- ✅ Arrow keys navigate with visual feedback (inverted text)
- ✅ Enter confirms selection and returns full path
- ✅ Returned path is absolute (starts with /)

---

### Test 4: file.putFileDialog() Type New Filename

**Objective:** Verify browsing directories then typing new filename

**Steps:**
1. Run: `./frontier-cli -e "file.putFileDialog('Save output as', @f); return f"`
2. Navigate to `/tmp/test_dialog/` using Tab/arrows
3. **Expected:** Menu shows existing files (config.txt, data.json, etc.)
4. Type "newfile.txt" at the end of the path
5. **Expected Input:**
   ```
   Save output as: /tmp/test_dialog/newfile.txt_
   ```
6. Press **Enter**

**Success Criteria:**
- ✅ Menu shows existing files (prevents overwrites)
- ✅ Can type new filename after directory path
- ✅ Returns full absolute path to new file
- ✅ File doesn't need to exist (creation allowed)

---

### Test 5: file.getFolderDialog() Directories Only

**Objective:** Verify only directories and symlinks are shown

**Steps:**
1. Create test structure:
   ```bash
   mkdir -p /tmp/test_folders/{dir1,dir2,.hidden}
   touch /tmp/test_folders/file.txt
   ```
2. Run: `./frontier-cli -e "file.getFolderDialog('Select output dir', @d); return d"`
3. Navigate to `/tmp/test_folders/` using Tab
4. **Expected Menu:**
   ```
   Select output dir: /tmp/test_folders/

   > .hidden/                    <dir>   2024-01-13  ← inverted
     dir1/                       <dir>   2024-01-14
     dir2/                       <dir>   2024-01-13

   ↑↓ to select | Enter to confirm
   ```
   (file.txt should NOT appear in menu)
5. Select dir1 and press **Enter**

**Success Criteria:**
- ✅ Only directories and symlinks shown
- ✅ Regular files (file.txt) not displayed
- ✅ Hidden directories (.hidden) visible
- ✅ Returns full absolute path to directory

---

### Test 6: Ctrl+C Abort Handling

**Objective:** Verify Ctrl+C kills UserTalk thread

**Steps:**
1. Run: `./frontier-cli -e "dialog.ask('Continue?')"`
2. Press **Ctrl+C** (SIGINT)
3. **Expected Behavior:**
   - Prompt should abort immediately
   - frontier-cli should exit
   - Exit code should be 130 (standard SIGINT exit code)

**Success Criteria:**
- ✅ Ctrl+C immediately aborts prompt
- ✅ UserTalk thread is killed
- ✅ frontier-cli exits cleanly
- ✅ No hanging processes

---

### Test 7: Batch Mode Error Handling

**Objective:** Verify Phase 1 error behavior preserved in batch mode

**Steps:**
1. Run with `--batch` flag:
   ```bash
   ./frontier-cli --batch -e "dialog.ask('Continue?')"
   ```
2. **Expected Output:**
   ```
   Error: Interactive prompts not available in batch mode
   ```
3. **Expected Exit Code:** 1 (failure)

**Repeat for:**
- `./frontier-cli --batch -e "dialog.getInt('Count', 10)"`
- `./frontier-cli --batch -e "dialog.getPassword('Pass')"`
- `./frontier-cli --batch -e "file.getFileDialog('Select', @f)"`

**Success Criteria:**
- ✅ All interactive verbs return errors in batch mode
- ✅ Error message is clear and helpful
- ✅ Exit code is 1 (failure)
- ✅ No prompts displayed

---

### Test 8: CI Environment Auto-Detection

**Objective:** Verify CI environment forces batch mode

**Steps:**
1. Run with `CI` environment variable:
   ```bash
   CI=true ./frontier-cli -e "dialog.ask('Continue?')"
   ```
2. **Expected Behavior:** Same as `--batch` flag (error, no prompt)

**Success Criteria:**
- ✅ CI environment auto-detects batch mode
- ✅ Interactive verbs return errors
- ✅ No prompts displayed

---

## Test Runner Enhancements Needed

The integration test runner (`tests/integration/runner.py`) needs enhancements to support Phase 2 tests:

### 1. Stdin Input Support

**Current:** No stdin input support
**Needed:** `stdin_input` field in YAML to simulate keyboard input

```python
# In runner.py, add stdin parameter to subprocess.run():
if 'stdin_input' in test:
    stdin_data = test['stdin_input'].encode('utf-8')
    result = subprocess.run(
        [cli_path, '-e', script],
        capture_output=True,
        stdin=subprocess.PIPE,
        input=stdin_data,
        timeout=30
    )
```

### 2. Batch Mode Flag Support

**Current:** No flag override support
**Needed:** `batch_mode: true` field to add `--batch` flag

```python
# In runner.py, check for batch_mode field:
cmd = [cli_path]
if test.get('batch_mode', False):
    cmd.append('--batch')
cmd.extend(['-e', script])
```

### 3. Environment Variable Support

**Current:** No environment override
**Needed:** `environment` field to set env vars like `CI=true`

```python
# In runner.py, merge environment:
env = os.environ.copy()
if 'environment' in test:
    env.update(test['environment'])

result = subprocess.run(cmd, env=env, ...)
```

### 4. Setup Script Support

**Current:** No pre-test setup
**Needed:** `setup_script` field to create test files before main script

```python
# In runner.py, run setup script first:
if 'setup_script' in test:
    setup_result = subprocess.run(
        [cli_path, '-e', test['setup_script']],
        capture_output=True,
        timeout=30
    )
    # Verify setup succeeded before running main test
```

---

## Implementation Checklist

### Phase 2A: Basic Dialog Prompts (1-2 days)

- [ ] **Terminal control utilities** (new file: `frontier-cli/terminal_control.c`)
  - [ ] ANSI escape code functions (clear line, move cursor, inverted text)
  - [ ] termios functions (disable echo, enable raw mode)
  - [ ] Cursor management (save/restore position)

- [ ] **dialog.ask() implementation**
  - [ ] Yes/No option rendering with inverted text
  - [ ] Arrow key and Tab detection
  - [ ] Enter to confirm selection
  - [ ] Return boolean based on selection

- [ ] **dialog.getInt() implementation**
  - [ ] Prompt with default in brackets
  - [ ] Accept Enter for default
  - [ ] Parse typed integer, validate
  - [ ] Re-prompt on invalid input

- [ ] **dialog.getString() implementation**
  - [ ] Prompt with default in brackets
  - [ ] Line editing via readline/libedit
  - [ ] Accept Enter for default
  - [ ] Return string

- [ ] **dialog.getPassword() implementation**
  - [ ] Disable echo via termios
  - [ ] Show dots (U+2022) for each character
  - [ ] Backspace removes last dot
  - [ ] Return password string

### Phase 2B: File Dialogs with Tab Completion (3-4 days)

- [ ] **Tab completion engine** (new file: `frontier-cli/tab_completion.c`)
  - [ ] Directory enumeration (readdir)
  - [ ] File metadata retrieval (stat)
  - [ ] Path auto-completion logic
  - [ ] Fuzzy matching (optional)

- [ ] **Visual menu rendering** (in `terminal_control.c`)
  - [ ] File list display with metadata
  - [ ] Scrolling for long lists (150+ files)
  - [ ] Inverted text for selected item
  - [ ] Footer with key hints

- [ ] **file.getFileDialog() implementation**
  - [ ] Tab completion shows file menu
  - [ ] Arrow key navigation
  - [ ] Enter on file confirms selection
  - [ ] Enter on directory descends
  - [ ] Backspace in empty path goes up directory
  - [ ] Only allows existing files
  - [ ] Returns full absolute path

- [ ] **file.putFileDialog() implementation**
  - [ ] Browse directories (shows all files)
  - [ ] Allow typing new filename after slash
  - [ ] Returns full absolute path (may not exist)

- [ ] **file.getFolderDialog() implementation**
  - [ ] Only shows directories and symlinks in menu
  - [ ] Navigate and select directory
  - [ ] Returns full absolute path to directory

- [ ] **file.getDiskDialog() implementation**
  - [ ] Enumerate mounted volumes (statfs/statvfs)
  - [ ] Display volume size and label
  - [ ] Returns mount point path

- [ ] **Starting directory logic**
  - [ ] Check output address for existing path
  - [ ] Fall back to cwd if no path hint
  - [ ] Matches legacy Frontier behavior

### Phase 2C: Testing & Integration (1-2 days)

- [ ] **Test runner enhancements**
  - [ ] Add stdin_input support
  - [ ] Add batch_mode flag support
  - [ ] Add environment variable support
  - [ ] Add setup_script support

- [ ] **Run automated tests**
  - [ ] All dialog verb tests (30 tests)
  - [ ] All file dialog verb tests (28 tests)
  - [ ] Verify batch mode tests still pass

- [ ] **Manual testing**
  - [ ] Visual verification of all 8 manual test procedures
  - [ ] Edge case testing (unicode, long paths, many files)
  - [ ] Ctrl+C handling verification

- [ ] **Documentation updates**
  - [ ] Update CLI_USAGE_GUIDE.md with interactive mode examples
  - [ ] Update HEADLESS_ADAPTATIONS.md with Phase 2 completion
  - [ ] Update processor_audits (dialog.md, file.md)

---

## Success Criteria

### Phase 2 Complete When:

✅ **All automated tests pass:**
- 30 dialog verb tests passing
- 28 file dialog verb tests passing
- Batch mode tests passing (Phase 1 behavior preserved)

✅ **All manual tests verified:**
- Visual feedback (inverted text, dots) working correctly
- Arrow keys and Tab navigation functional
- Tab completion menus displaying correctly
- Ctrl+C aborts cleanly

✅ **Edge cases handled:**
- Unicode filenames and input
- Very long paths and strings
- Many files (150+) with scrolling
- Special characters in input

✅ **Batch mode behavior:**
- `--batch` flag forces errors
- CI environment auto-detects batch mode
- Non-TTY contexts auto-detect batch mode
- Error messages clear and helpful

✅ **Documentation complete:**
- CLI usage guide updated
- Test procedures documented
- Implementation status updated

---

## Debugging Guide

### Common Issues and Solutions

**Issue:** Inverted text not displaying correctly
**Solution:** Check terminal ANSI support. Use `\x1b[7m` for invert, `\x1b[27m` for normal.

**Issue:** Arrow keys not detected
**Solution:** Enable raw mode with termios. Arrow keys send escape sequences (`\x1b[A`, `\x1b[B`, etc.)

**Issue:** Password echo not disabled
**Solution:** Use termios `ECHO` flag. Save/restore terminal settings around prompt.

**Issue:** Tab completion shows wrong files
**Solution:** Check readdir() enumeration. Verify stat() calls for metadata. Check hidden file logic (names starting with `.`).

**Issue:** Paths not absolute
**Solution:** Use `realpath()` to convert relative to absolute. Verify all file dialog returns start with `/`.

**Issue:** Tests hang waiting for input
**Solution:** Verify stdin_input provided in YAML. Check timeout in subprocess.run().

---

## References

- **Planning:** `planning/phase3/HEADLESS_INTERACTIVE_MODE.md` (Phase 2 section)
- **Phase 1 Tests:** `tests/HEADLESS_INTERACTIVE_MODE_TESTS.md`
- **Integration Runner:** `tests/integration/runner.py`
- **Dialog Audit:** `planning/phase3/processor_audits/dialog.md`
- **File Audit:** `planning/phase3/processor_audits/file.md`

---

**Last Updated:** 2026-01-13
**Status:** Tests complete, ready for Phase 2 implementation
