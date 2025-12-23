# Dead Code Analysis: Category Overview

Analysis Date: 2025-12-20  
Model: Haiku (broad strokes analysis)

## Summary

Frontier codebase analysis reveals **5 major categories** of dead code candidates:

| Category | Count | Risk | Priority |
|----------|-------|------|----------|
| GUI-only code | 28 files | Medium | High |
| Logging noise | 352 fprintf calls | Low | Medium |
| Legacy/obsolete markers | 25 files | Medium | Medium |
| Conditional feature code | ~40 ifdef blocks | Medium | Medium |
| Stub/unreachable functions | 10+ functions | Low | Low |

---

## 1. GUI-Only Code (28 files)

**Files identified**: All menu, window, dialog, display management code  
**Cross-references**: 15+ core GUI files have incoming references  
**Removal strategy**: Most need stubbing rather than deletion; some dialog/menu verbs may be used by scripts

**Key files by dependency**:
- `menu.c` (55 refs) - HIGH PRIORITY
- `frontierwindows.c` (44 refs) - HIGH PRIORITY  
- `dialogs.c` (38 refs) - HIGH PRIORITY
- `shellmenu.c` (18 refs) - MEDIUM

**See also**: `2025-12-20-gui-only-candidates.md` for full list

---

## 2. Logging Noise (352 fprintf statements)

**Distribution**:
- `langhash.c`: 58 statements
- `db.c`: 47 statements
- `db_format.c`: 46 statements
- `tableexternal_common.c`: 25 statements
- `tablepack.c`: 19 statements
- _20 more files with 5-17 statements each_

**Pattern**: Mix of debug, error reporting, and diagnostic output  
**Removal risk**: LOW (easy to stub or redirect)  
**Action**: Consolidate into structured logging infrastructure with per-component control

---

## 3. Legacy/Obsolete Code Markers (25 files)

**Pattern**: Files containing keywords: "legacy", "obsolete", "deprecated", "old_format"  
**Examples**:
- v6 reader/packer code  
- Pre-Paige WPText format handlers
- Obsolete database version handlers

**Status**: Mostly intentional (v6 readers stay, v6 writers can be removed)

---

## 4. Conditional Feature Code

**Key ifdef patterns found**:
- `FRONTIER_HEADLESS` (31 blocks) - Headless mode conditionals
- `fldebug` (40+ blocks) - Debug mode code
- `PIKE` (21 blocks) - Pike integration
- `gray3Dlook` (15 blocks) - UI appearance
- `xmlfeatures` (6 blocks) - XML support
- `NEW_DLL_INTERFACE` (6 blocks) - DLL interface
- `SMART_DB_OPENING` (7 blocks) - Database opening optimization

**Dead code risk**: Code inside false conditionals needs audit

---

## 5. Stub/Unreachable Functions

**Count**: ~10 functions that are obvious stubs (immediate `return false/0/NULL`)  
**Examples**: Platform-specific functions that do nothing in headless mode  
**Removal risk**: LOW (safe to remove/stub)

---

## Next Steps for Deeper Analysis

### For GUI Code (needs Sonnet)
- Trace call graph from non-GUI to GUI functions
- Identify which functions can be stubbed vs. fully deleted
- Check for UserTalk verb exports that scripts depend on

### For Logging Infrastructure (Haiku can handle)
- Categorize log types (debug, error, diagnostic, performance)
- Propose per-component log level control
- Design structured logging interface

### For Legacy Code (needs Sonnet + planning docs)
- Audit which v6-related code can be removed
- Identify deprecated formats that are still in use
- Plan removal/stubbing strategy

### For Feature Conditionals (Haiku + grep)
- Identify which ifdef blocks are always true/false in headless
- Propose compile-time vs. runtime feature control

---

## Files Analyzed

**Total C files scanned**: 200+  
**Analysis tools used**: ripgrep, grep, pattern matching  
**False positive risk**: MEDIUM (dynamic dispatch and verb tables hide some calls)

## Recommendations

1. **Start safe**: Stub/remove logging first (352 low-risk fprintf calls)
2. **Then medium**: Remove obvious stubs and unused feature code  
3. **Then complex**: Tackle GUI code with proper dependency mapping
4. **Last**: Legacy format cleanup (well-understood, low risk)

