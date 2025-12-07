# Processor Audits Directory

This directory contains detailed audits for Frontier's processor/verb system. Processors are either kernel verbs (defined in `kernelverbs.rc`) or script-based verbs (in `system.verbs.builtins/`, `system.callbacks/`, `system.agents/`, `system.verbs.globals/`, etc.).

## All Processors (32 total across kernelverbs.rc + non-kernel)

### Kernel Verbs from kernelverbs.rc (1000-1027)

#### Completed Audits

- ✅ [bit.md](bit.md) - EFP 1005 - 8 verbs - Core bitwise operations
- ✅ [clock.md](clock.md) - EFP 1005 - 7 verbs - Time/clock operations
- ✅ [crypt.md](crypt.md) - EFP 1025 - 5 verbs - Cryptographic operations (MD5, SHA1, Whirlpool, HMAC)
- ✅ [date.md](date.md) - EFP 1005 - 30 verbs - Date/time manipulation
- ✅ [db.md](db.md) - EFP 1019 - 13 verbs - Database CRUD operations
- ✅ [file.md](file.md) - EFP 1007 - 86 verbs - File system operations (updated: 67/86 headless)
- ✅ [inetd.md](inetd.md) - EFP 1021 - 1 kernel verb - Network daemon supervisor
- ✅ [launch.md](launch.md) - EFP 1014 - 5 verbs - Application launching (updated: 3/5 headless)
- ✅ [op.md](op.md) - EFP 1000 - 45 verbs + 5 attributes - Outline processor (100% headless)
- ✅ [script.md](script.md) - EFP 1000 - 13 verbs - Script compilation & debugging (updated: 100% headless)
- ✅ [string.md](string.md) - EFP 1006 - 60 verbs - String manipulation (100% headless)
- ✅ [sys.md](sys.md) - EFP 1013 - 16 verbs - System operations (updated: 15/16 headless)
- ✅ [table.md](table.md) - EFP 1001 - 18 verbs + 13 scripts - Table/hash operations (20/31 headless)
- ✅ [tcp.md](tcp.md) - EFP 1005 - 23 verbs - Network I/O operations
- ✅ [thread.md](thread.md) - EFP 1018 - 17 verbs - Thread management (100% headless)
- ✅ [webserver.md](webserver.md) - EFP 1021 - 7 verbs + 66 scripts - HTTP server

#### Not Yet Audited (kernelverbs.rc)

- ⏳ menu (EFP 1002) - 14 verbs - Menu bar operations
- ⏳ wp (EFP 1003) - 27 verbs - WPText formatted text operations (**Note: headless-compatible via in-memory objects**)
- ⏳ pict (EFP 1004) - 4 verbs - Picture operations
- ⏳ rez (EFP 1008) - 15 verbs - Resource operations
- ⏳ window (EFP 1009) - 31 verbs - Window management
- ⏳ search (EFP 1010) - 6 verbs - Search/replace operations
- ⏳ filemenu (EFP 1011) - 10 verbs - File menu operations
- ⏳ editmenu (EFP 1012) - 16 verbs - Edit menu operations
- ⏳ clipboard (EFP 1015) - 2 verbs - Clipboard operations
- ⏳ frontier (EFP 1016) - 14 verbs - Frontier application control
- ⏳ mainwindow (EFP 1017) - 7 verbs - Main window control
- ⏳ xml (EFP 1020) - 14 verbs - XML operations
- ⏳ html (EFP 1021) - 23 verbs - HTML processing
- ⏳ searchengine (EFP 1021) - 5 verbs - Search engine utilities
- ⏳ mrcalendar (EFP 1021) - 11 verbs - Calendar operations
- ⏳ re (EFP 1023) - 10 verbs - Regular expressions (conditional)
- ⏳ math (EFP 1024) - 3 verbs - Math operations (min, max, sqrt)
- ⏳ sqlite (EFP 1026) - 17 verbs - SQLite database
- ⏳ mysql (EFP 1027) - 27 verbs - MySQL database
- ⏳ base64 (EFP 1005) - 2 verbs - Base64 encoding/decoding
- ⏳ dialog (EFP 1005) - 19 verbs - Dialog boxes (GUI-dependent)
- ⏳ kb (EFP 1005) - 4 verbs - Keyboard state
- ⏳ mouse (EFP 1005) - 2 verbs - Mouse state
- ⏳ point (EFP 1005) - 2 verbs - Point operations
- ⏳ rectangle (EFP 1005) - 2 verbs - Rectangle operations
- ⏳ rgb (EFP 1005) - 2 verbs - Color operations
- ⏳ speaker (EFP 1005) - 3 verbs - Sound operations
- ⏳ target (EFP 1005) - 3 verbs - Target (window/editor context)
- ⏳ semaphore (EFP 1005) - 2 verbs - Mutex/lock operations
- ⏳ python (EFP 1005) - 1 verb - Python script execution
- ⏳ dll (EFP 1005) - 4 verbs - DLL calling (Windows-specific)
- ⏳ htmlcontrol (EFP 1005) - 8 verbs - HTML browser control
- ⏳ statusbar (EFP 1005) - 5 verbs - Status bar operations
- ⏳ winregistry (EFP 1005) - 4 verbs - Windows registry access
- ⏳ osa (EFP 1000) - 2 verbs - Open Scripting Architecture

### Non-Kernel Processors (4)

- ✅ [globals.md](globals.md) - 41 type constructors & utilities (100% headless)
- ✅ [callbacks.md](callbacks.md) - 21 system event handlers (100% headless)
- ✅ [agents.md](agents.md) - 6 background task agents (5/6 headless; scheduler exists in UserTalk)
- ⏳ [apps.md](apps.md) - 30+ app categories, 100+ verbs (deferred - awaiting IAC architecture)

---

## Current Status

**Completed Audits:** 19 out of 32 processors (59%)

**Headless-Ready:** 16+ processors (100% compatible)

**Partially Headless:** 3 processors (file, sys, table)

**Deferred:** 1 processor (apps - architecture decision needed)

**Not Yet Audited:** 12 processors (mostly GUI-dependent or low priority)

---

## Implementation Priority

Based on headless operation, the recommended implementation order is:

1. **Critical Infrastructure** (already audited):
   - thread (runtime threading foundation)
   - db (database operations)
   - op (outline data structures)
   - table (hash table operations)

2. **Essential Verbs** (already audited):
   - string (text manipulation)
   - file (file I/O)
   - tcp (network I/O)
   - date/clock (time operations)

3. **High Priority** (not yet audited but headless-compatible):
   - **wp** (WPText objects - in-memory, no GUI required)
   - xml (XML processing)
   - html (HTML utilities)
   - target (context management)

4. **Medium Priority** (some headless compatibility):
   - bit, rgb, point, rectangle (data utilities)
   - crypt, base64 (encoding/crypto)
   - dialog (can be stdio-based)
   - launch (process spawning)

5. **Low Priority / Platform-Specific**:
   - menu, window, search (GUI-dependent)
   - dialog, kb, mouse (input-dependent)
   - dll, python, htmlcontrol (platform-specific)
   - resa, rez, pict, speaker (resource/multimedia)

6. **Deferred**:
   - apps (awaiting IAC architecture)
   - sqlite, mysql (databases - consider later)

---

## Audit Template

Each audit file contains:
- Basic information (processor name, verb count, kernel ID)
- Headless compatibility assessment
- Verb inventory with categories
- Implementation analysis
- Blocking dependencies
- Testing strategy
- Related processors

---

**Total Processors:** 32
**Total Verbs:** 700+ (across all processors)
**Overall Headless Compatibility:** ~95% of audited processors
