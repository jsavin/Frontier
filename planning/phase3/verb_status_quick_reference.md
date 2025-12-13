# Frontier Verb Processor Status - Quick Reference

> 2025-12-08 Codex: This quick reference assumes the bulk auto-registration from PR #60. That path was rolled back; headless currently uses the curated `kernel_verbs_headless.c` registration list and a small set of linked stubs. Treat the counts below as planning guidance until a stable registration pass is re-landed.

**As of**: December 4, 2025 - Phase 1 Complete

## TL;DR

✅ **All 51 verb processors initialized and compiling successfully**

- **707 total verbs** across all processors
- **300 verbs** with real implementations (working)
- **407 verbs** with stubs (ready for testing)
- **Ready for Phase 2**: Systematic testing and prioritization

---

## By The Numbers

```
Processors:  51 total
├── 14 with real implementations
└── 37 with stub implementations

Verbs:       707 total
├── ~300 working (string, lang, file, etc.)
├── ~170 GUI-incompatible (dialogs, windows, menus)
├── ~100 platform-specific (need OS-specific code)
├── ~100 need bug fixes
└── ~37 unimplemented stubs

Estimated Headless Coverage: 60-70%
```

---

## What Works Today (14 Processors)

**Core Language & I/O**:
- ✅ `string` (60 verbs) - All string operations
- ✅ `file` (86 verbs, 12 implemented) - Basic file I/O
- ✅ `lang` (58 verbs) - Core runtime
- ✅ `db` (13 verbs) - Database operations

**Data Processing**:
- ✅ `xml` (14 verbs) - XML parsing
- ✅ `html` (23 verbs) - HTML processing
- ✅ `re` (10 verbs) - Regular expressions
- ✅ `math` (3 verbs) - Math operations

**System & Infrastructure**:
- ✅ `sys` (16 verbs) - System operations
- ✅ `window` (31 verbs) - Window management
- ✅ `crypt` (5 verbs) - Encryption/hashing
- ✅ `sqlite` (17 verbs) - SQLite databases
- ✅ `mysql` (27 verbs) - MySQL databases
- ✅ `table` (18 verbs) - Outline/table operations

---

## What Needs Work (37 Processors)

### High Priority (Most Useful)

| Processor | Verbs | Status | Effort |
|-----------|-------|--------|--------|
| **date** | 30 | Pure logic, no GUI | 🟢 Low |
| **op** | 45 | ~70% doable, ~30% GUI | 🟡 Medium |
| **script** | 13 | Needs implementation | 🟡 Medium |
| **clipboard** | 2 | System access | 🟢 Low |

### Medium Priority (Specialized)

| Processor | Verbs | Status | Effort |
|-----------|-------|--------|--------|
| **dialog** | 19 | 100% GUI-dependent | 🔴 High |
| **menu** | 14 | 100% GUI-dependent | 🔴 High |
| **kb/mouse** | 6 | Input handling | 🟡 Medium |
| **bit** | 8 | Pure logic | 🟢 Low |
| **tcp** | 23 | Networking | 🟡 Medium |
| **thread** | 17 | Thread mgmt | 🟡 Medium |

### Lower Priority (Rarely Used)

- `pict` (4) - Image rendering (GUI)
- `rez` (15) - Resource forks (legacy Mac)
- `dll` (4) - Windows-specific
- `htmlcontrol` (8) - UI controls
- `statusbar` (5) - UI elements
- `searchengine` (5) - Integration
- `mrcalendar` (11) - Calendar control
- `webserver` (7) - Web services
- And 10 others with specialized use

---

## GUI-Incompatible (Can't Fix Without GUI Framework)

**~170 verbs that require a window system**:

- `dialog.*` (19) - Alert boxes, dialogs
- `menu.*` (14) - Menu operations
- `window.*` (31) - Window management
- `filemenu.*` (10) - File menu
- `editmenu.*` (16) - Edit menu
- `htmlcontrol.*` (8) - HTML UI controls
- `statusbar.*` (5) - Status bar
- `mainwindow.*` (7) - Main window
- `search.*` (6) - Search UI
- `pict.*` (4) - Picture rendering
- `target.*` (3) - Target window

**Strategy**: Return helpful error messages ("Feature not available in headless mode") rather than crashing.

---

## Platform-Specific (~50-100 verbs)

These need different implementations for macOS, Linux, and Windows:

- File operations (metadata, locking, volumes)
- System operations (process management, environment)
- Networking (socket APIs differ)
- Path handling (separators, conventions)

**Strategy**: Use portable library wrappers or conditional compilation.

---

## Next Steps

### Immediate (This Week)
- [ ] Create test harness for 707 verbs
- [ ] Run automated tests
- [ ] Categorize results

### Short Term (Next 2 Weeks)
- [ ] Analyze test results
- [ ] Build prioritized backlog
- [ ] Identify quick wins

### Medium Term (Weeks 3-4)
- [ ] Implement high-impact verbs (date, script, clipboard)
- [ ] Complete file verbs (74 remaining)
- [ ] Create platform-specific wrappers

### Long Term (Month 2+)
- [ ] Dialog/menu stubs with error handling
- [ ] Platform-specific implementations
- [ ] Integration testing with real UserTalk scripts

---

## Key Metrics to Track

```
Phase 2 (Testing):
- Verbs tested: ___/707
- Verbs working: ___/707
- Verbs GUI-incompatible: ___/170
- Verbs needing fixes: ___/???

Phase 3 (Implementation):
- Verbs implemented: ___/???
- Test coverage: ___%
- Common errors documented: Yes/No
```

---

## Communication Points for Stakeholders

**Progress to Date**:
- ✅ All verb processors loadable
- ✅ Build system clean and automated
- ✅ Ready for systematic testing

**Current Status**:
- 🔄 Phase 2: Testing and categorization
- ⏳ Phase 3: Implementation roadmap

**Expected Outcome**:
- 🎯 60-70% of verbs headless-compatible
- 🎯 Most common operations working (strings, files, dates)
- 🎯 Clear documentation of limitations
- 🎯 Graceful error handling for GUI-only features

---

## File Locations

**Documentation**:
- Full Status Report: `planning/phase3/verb_implementation_status.md`
- This Quick Reference: `planning/phase3/verb_status_quick_reference.md`
- Implementation Plan: `planning/phase3/verb_processor_implementation_plan.md`

**Code**:
- Stub Generator: `tools/kernelverbs_parser/generate_processor_stubs.py`
- Whitelist: `tools/kernelverbs_parser/parse_kernelverbs.py` (line 27)
- Init Code: `generated/kernel_verbs_init.c` (auto-generated)

**Processors**:
- Real Implementations: `Common/source/*verbs.c`
- Stub Implementations: `tests/headless_*_verbs.c`

---

*Last Updated: December 4, 2025*
*Document Owner: Implementation Team*
*Status: Phase 1 ✅ Complete | Phase 2 ⏳ Starting*
