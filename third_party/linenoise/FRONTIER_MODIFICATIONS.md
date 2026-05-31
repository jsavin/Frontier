# Frontier Modifications to Linenoise

This document tracks customizations made to the linenoise library for Frontier-specific functionality.

## Overview

Linenoise is a lightweight readline alternative used by frontier-cli for REPL line editing. We've enhanced it with UserTalk-aware features while maintaining compatibility with upstream.

---

## Modifications

### 1. Word Navigation with UserTalk-Aware Boundaries

**Added in**: PR #356 (2026-01-27)
**Files Modified**: `linenoise.c`

#### New Functions

- **`isWordChar(char c)`** (line ~1216)
  - Determines if a character is part of a word for navigation purposes
  - Word characters: `a-z A-Z 0-9 _ - . @ ^ [ ]`
  - UserTalk-specific operators included: `@` (address-of), `^` (dereference), `[` `]` (array indexing)

- **`linenoiseEditMoveWordLeft()`** (line ~1244)
  - Move cursor to start of previous word
  - UTF-8 aware character-by-character navigation
  - Handles UserTalk expressions like `system.verbs.string.countFields` as single word

- **`linenoiseEditMoveWordRight()`** (line ~1256)
  - Move cursor to start of next word
  - Three-phase algorithm: skip non-word chars → skip word chars → skip trailing non-word chars
  - Allows natural navigation through expressions like `msg("hello")` as three words: `msg` → `(` → `"hello"`

#### Modified Functions

- **`linenoiseEditDeletePrevWord()`** (line ~1228)
  - Updated to use `isWordChar()` for consistent word boundary detection
  - Previously used space-only boundaries; now uses UserTalk-aware boundaries

#### Escape Sequence Handling

- **Emacs-style sequences** (line ~1441)
  - Added handling for `ESC b` (Option+Left on macOS Terminal.app)
  - Added handling for `ESC f` (Option+Right on macOS Terminal.app)
  - Single-byte sequences checked before reading second byte to avoid consuming input

- **Modified arrow key sequences** (line ~1446)
  - Support for `ESC[1;3C` (Option+Right on other terminals)
  - Support for `ESC[1;3D` (Option+Left on other terminals)

#### Keycode Debugging

- **`linenoisePrintKeyCodes()`** (line ~1593)
  - Enhanced to exit on ESC key (previously only 'quit' command)
  - Used by frontier-cli's `/keycodes` command for terminal debugging
  - PR #674 (2026-05-30): both exit branches (ESC and "quit") now emit
    explicit `\r\n` before breaking out of the raw-mode loop. The bare
    `\n` from the original code left the cursor at its current column
    after termios was restored, causing the next REPL prompt to print
    mid-row instead of column 1 — visible from both the slash command
    and the new REPL menu's "Key codes" item.

#### Exported Internals

- **`linenoiseEditInsert`** (linenoise.c line ~1099)
  - PR #674 (2026-05-30): exported via linenoise.h. Originally
    intra-translation-unit but had external linkage; frontier-cli's
    slash-menu disambiguator in `repl.c` needs to push a follow-up
    byte back into the edit buffer after consuming it for `//`
    fast-path detection. The function echoes the byte to the user's
    terminal and updates the linenoise state's buf/len/pos, so
    linenoise sees the byte on the next `linenoiseEditFeed` cycle
    as if the user typed it normally.

---

## Rationale for UserTalk-Specific Word Boundaries

Standard editor word boundaries (alphanumeric + underscore) don't work well for UserTalk expressions. Our customizations allow:

- **Dotted paths**: `system.verbs.apps` navigates as one word
- **Address expressions**: `@workspace.settings` navigates as one word
- **Dereference operators**: `adrtable^.foo` navigates as one word
- **Array indexing**: `mylist[n]` navigates as one word
- **Hyphenated identifiers**: `my-function-name` navigates as one word

This matches how UserTalk developers think about code structure.

---

## Upstream Compatibility

These modifications are additive and don't change existing linenoise behavior:

- ✅ All existing key bindings work unchanged (arrows, Home/End, Ctrl+A/E, etc.)
- ✅ Existing edit functions unmodified (except word deletion to use new boundaries)
- ✅ No changes to core line editing logic
- ✅ No changes to history, completion, or rendering

### Merge Considerations

When updating linenoise from upstream:
1. Check for conflicts in escape sequence handler (line ~1428-1500)
2. Verify `linenoiseEditDeletePrevWord()` still uses our `isWordChar()` helper
3. Test word navigation with UserTalk expressions after merge
4. Review any new edit functions for word boundary usage

---

## Testing

Word navigation is tested manually due to integration test framework limitations (no escape sequence simulation support).

**Test coverage tracked in**: Issue #357

**Manual test cases**:
- `system.verbs.string.countFields("hello world", " ")` - verify dotted path as one word
- `@someobject.subobject` - verify address-of operator included
- `adrtable^.foo` - verify dereference operator included
- `mylist[n]` - verify array indexing included
- `msg("hello")` - verify punctuation as separate words

---

## References

- **PR #356**: Word navigation implementation
- **Issue #357**: Future automated test coverage
- **Upstream linenoise**: https://github.com/antirez/linenoise
- **Frontier docs**: `docs/CLI_USAGE_GUIDE.md` - REPL key bindings

---

## Version Information

**Linenoise Version**: 1.0 (as of vendoring)
**Last Frontier Modification**: 2026-05-30
**Modified Lines**: ~100 lines added to ~1500 line file (~6.6% modification)
