# Missing Verb Implementations - Annotated with Processor Audits

This file lists the 102 verbs currently not detected as implemented
but are in processors that SHOULD have implementations.

**UPDATED WITH CONTEXT:** Based on processor audit findings from `planning/phase3/processor_audits/`


## Categorization Guide

For each verb, the audit context will help determine:

- **HAS_IMPL**: Verb has real C implementation but pattern matching missed it → ADD EXCEPTION
- **LEGIT_STUB**: Verb is legitimately stubbed (minimal/placeholder or GUI-context dependent) → LEAVE AS-IS
- **NEEDS_REVIEW**: Unclear - needs manual investigation → INVESTIGATE
- **MAC_LEGACY**: Mac Classic resource fork / type-creator / etc → LOW PRIORITY
- **DEFERRED**: Documented as deferred in audit (e.g., processhtmlmacros) → SKIP FOR NOW


## Summary by Processor

| Processor | Missing Count | Audit Status | Category | Notes |
|-----------|---------------|--------------|----------|-------|
| **clock** | 1 | 100% headless compatible | Core Infrastructure | See clock.md audit: all verbs implemented except possibly set |
| **date** | 4 | 100% headless compatible | Core Infrastructure | See date.md audit: missing verbs extracted via script |
| **db** | 2 | 100% headless compatible | Data Management | See db.md audit: all verbs implemented, headless stub |
| **dialog** | 5 | 100% headless compatible (stdio) | Text Processing | See dialog.md audit: can use stdio prompts instead of GUI |
| **file** | 3 | Partial (67-83/86 headless compatible) | Core + Script + Legacy | See file.md audit: Tier 1 has 37 core primitives |
| **html** | 2 | 100% headless compatible | Text Processing | See html.md audit: drawcalendar/parsehttpargs deferred |
| **inetd** | 1 | 100% headless compatible | Web Server & Search | See inetd.md audit: supervisor critical for network |
| **lang** | 15 | 100% headless compatible | Core Infrastructure | See lang.md audit: 15 missing legacy/platform-specific |
| **menu** | 2 | GUI-dependent | GUI-Dependent | Menu operations require window/UI context |
| **mysql** | 23 | 14% detected (4/27) | Optional Database | Optional external database, 23 likely stubbed |
| **op** | 1 | 100% headless compatible | Core Data Structures | See op.md audit: tabkeyreorg likely GUI-specific |
| **sqlite** | 15 | 12% detected (2/17) | Optional Database | Optional SQLite database, 15 likely stubbed |
| **statusbar** | 5 | 0% detected (0/5) | GUI-Dependent | GUI-dependent status bar, not headless-compatible |
| **string** | 6 | 100% headless compatible | Core Functionality | See string.md audit: Tier 1-2 implemented, Tier 4 deferred |
| **sys** | 3 | 100% headless compatible | System Operations | See sys.md audit: missing verbs platform-specific |
| **table** | 8 | 20/31 script-based headless | Data Management | See table.md audit: shared stubs for UI context |
| **window** | 5 | Partial (26/31 headless) | GUI-Dependent | Window management mixed, 5 likely GUI-context operations |
| **xml** | 1 | 100% headless compatible | Text Processing | See xml.md audit: frontiervaluetotaggedtext likely legacy |

---

## Detailed Verb Listing

### clock (6/7 detected, 86% coverage)

**Audit Status:** 100% headless compatible

**Context:** See clock.md audit: all verbs implemented except possibly set (platform-specific time adjustment)

| #   | Verb Name | Status                                                                           | Notes |
| --- | --------- | -------------------------------------------------------------------------------- | ----- |
| 1   | `set`     | [ ] HAS_IMPL / [ ] LEGIT_STUB / [x] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED |       |

### date (26/30 detected, 87% coverage)

**Audit Status:** 100% headless compatible

**Context:** See date.md audit: missing verbs (hour, minute, month, year) likely extracted via script from base date verbs

**Implementation Status:**
- All 4 verbs have C implementations in langverbs.c (datehourfunc, dateminutefunc, datemonthfunc, dateyearfunc)
- hour/minute/month/year: Lines 277/279/273/275 (enums), cases at 1986/1987/1985/1985
- minute, month, year have UserTalk wrappers in usertalk_scripts/system/verbs/builtins/date/
- hour has NO UserTalk wrapper - C implementation must be called directly via kernel()

| #   | Verb Name | Status                                                                           | Notes                                                           |
| --- | --------- | -------------------------------------------------------------------------------- | --------------------------------------------------------------- |
| 1   | `hour`    | [x] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | C: datehourfunc (langverbs.c:277), no UserTalk wrapper |
| 2   | `minute`  | [x] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | C: dateminutefunc (langverbs.c:279), UserTalk wrapper exists |
| 3   | `month`   | [x] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | C: datemonthfunc (langverbs.c:273), UserTalk wrapper exists |
| 4   | `year`    | [x] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | C: dateyearfunc (langverbs.c:275), UserTalk wrapper exists |

### db (11/13 detected, 85% coverage)

**Audit Status:** 100% headless compatible

**Context:** See db.md audit: all verbs implemented. Missing isTable/newTable have C implementations

**Implementation Status:**
- Both verbs have C implementations in dbverbs.c
- isTable: istablefunc (dbverbs.c:437, case at 970)
- newTable: newtablefunc (dbverbs.c:435, case at 967)
- Both have UserTalk wrappers calling kernel(db.*)

| #   | Verb Name  | Status                                                                           | Notes                                                  |
| --- | ---------- | -------------------------------------------------------------------------------- | ------------------------------------------------------ |
| 1   | `isTable`  | [x] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | C: istablefunc (dbverbs.c:437), UserTalk wrapper      |
| 2   | `newTable` | [x] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | C: newtablefunc (dbverbs.c:435), UserTalk wrapper     |

### dialog (14/19 detected, 74% coverage)

**Audit Status:** 100% headless compatible (stdio)

**Context:** See dialog.md audit: can use stdio prompts instead of GUI dialogs. Missing verbs likely GUI-only card operations

| #   | Verb Name             | Status                                                                           | Notes                                                                 |
| --- | --------------------- | -------------------------------------------------------------------------------- | --------------------------------------------------------------------- |
| 1   | `hideitem`            | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [x] MAC_LEGACY / [ ] DEFERRED | Legacy MacBird verb, should error with message saying not implemented |
| 2   | `ismodalcard`         | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [x] MAC_LEGACY / [ ] DEFERRED | Legacy MacBird verb, should error with message saying not implemented |
| 3   | `runcard`             | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [x] MAC_LEGACY / [ ] DEFERRED | Legacy MacBird verb, should error with message saying not implemented |
| 4   | `setmodalcardtimeout` | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [x] MAC_LEGACY / [ ] DEFERRED | Legacy MacBird verb, should error with message saying not implemented |
| 5   | `showitem`            | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [x] MAC_LEGACY / [ ] DEFERRED | Legacy MacBird verb, should error with message saying not implemented |

### file (83/86 detected, 97% coverage)

**Audit Status:** Partial (67-83/86 headless compatible)

**Context:** See file.md audit: Tier 1 has 37 core primitives. Missing 3 likely MAC LEGACY (getsystemfolderpath, getspecialfolderpath, mountservervolume)

| #   | Verb Name              | Status                                                                           | Notes                                                                |
| --- | ---------------------- | -------------------------------------------------------------------------------- | -------------------------------------------------------------------- |
| 1   | `getspecialfolderpath` | [ ] HAS_IMPL / [ ] LEGIT_STUB / [x] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | Returns full path to a given "special" folder (i.e. Documents)       |
| 2   | `getsystemfolderpath`  | [ ] HAS_IMPL / [ ] LEGIT_STUB / [x] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | Returns full path to the system folder (i.e. /System/ or C:\Win32/ ) |
| 3   | `mountservervolume`    | [ ] HAS_IMPL / [ ] LEGIT_STUB / [x] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | Mounts a remote server share                                         |

### html (21/23 detected, 91% coverage)

**Audit Status:** 100% headless compatible

**Context:** See html.md audit: drawcalendar and parsehttpargs have C implementations

**Implementation Status:**
- drawcalendar: htmlcalendardrawfunc (langhtml.c:277, case at 9951), UserTalk wrapper exists
- parsehttpargs: parseargsfunc in stringverbs.c (line 270, case at 1968), UserTalk wrapper in html processor

| #   | Verb Name       | Status                                                                           | Notes                                                            |
| --- | --------------- | -------------------------------------------------------------------------------- | ---------------------------------------------------------------- |
| 1   | `drawcalendar`  | [x] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | C: htmlcalendardrawfunc (langhtml.c:277), UserTalk wrapper      |
| 2   | `parsehttpargs` | [x] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | C: parseargsfunc (stringverbs.c:270), UserTalk wrapper          |

### inetd (0/1 detected, 0% coverage)

**Audit Status:** 100% headless compatible

**Context:** See inetd.md audit: supervisor has C implementation

**Implementation Status:**
- supervisor: inetdsupervisorfunc (langhtml.c:333, case at 10086), UserTalk wrapper exists

| #   | Verb Name    | Status                                                                            | Notes                                                                    |
| --- | ------------ | --------------------------------------------------------------------------------- | ---------------------------------------------------------------------- |
| 1   | `supervisor` | [x] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | C: inetdsupervisorfunc (langhtml.c:333), UserTalk wrapper              |

### lang (43/58 detected, 74% coverage)

**Audit Status:** 100% headless compatible

**Context:** See lang.md audit: 15 missing verbs likely legacy/platform-specific (DDEevent, callxcmd on Windows, Apple events)

| #   | Verb Name             | Status                                                                           | Notes                                                                |
| --- | --------------------- | -------------------------------------------------------------------------------- | -------------------------------------------------------------------- |
| 1   | `DDEevent`            | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED | Mac-only Apple event; trigger "not implemented" error in UserTalk   |
| 2   | `callxcmd`            | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED | Mac Classic XCMD interface, trigger "not implemented" error in UserTalk |
| 3   | `countapplelistitems` | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED | OSA integration, trigger "not implemented" error in UserTalk         |
| 4   | `delete`              | [x] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | UserTalk wrapper (kernel lang.delete), type coercion verb            |
| 5   | `edit`                | [ ] HAS_IMPL / [x] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | Requires GUI                                                         |
| 6   | `flushmemory`         | [x] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | C: flushmemfunc (langverbs.c:2620), simplify to noop returning true  |
| 7   | `getapplelistitem`    | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED | OSA integration, trigger "not implemented" error in UserTalk         |
| 8   | `geteventattribute`   | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED | AppleScript support, trigger "not implemented" error in UserTalk     |
| 9   | `putapplelistitem`    | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED | OSA integration, trigger "not implemented" error in UserTalk         |
| 10  | `rollbeachball`       | [ ] HAS_IMPL / [x] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | Requires GUI                                                         |
| 11  | `scripterror`         | [ ] HAS_IMPL / [x] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | Core runtime functionality for tripping UserTalk errors              |
| 12  | `seteventinteraction` | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED | AppleScript support, trigger "not implemented" error in UserTalk     |
| 13  | `short`               | [x] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | UserTalk wrapper (kernel lang.short), type coercion verb            |
| 14  | `string4`             | [x] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | UserTalk wrapper (kernel lang.string4), type coercion verb          |
| 15  | `transactionEvent`    | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED | AppleScript support, trigger "not implemented" error in UserTalk     |

### menu (12/14 detected, 86% coverage)

**Audit Status:** GUI-dependent

**Context:** Menu operations require window/UI context. getscript has C implementation

**Implementation Status:**
- getscript: getscriptfunc (menuverbs.c:82, case at 2001), UserTalk wrapper exists

| #   | Verb Name     | Status                                                                           | Notes                                                 |
| --- | ------------- | -------------------------------------------------------------------------------- | ----------------------------------------------------- |
| 1   | `getscript`   | [x] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | C: getscriptfunc (menuverbs.c:82), UserTalk wrap      |
| 2   | `isinstalled` | [ ] HAS_IMPL / [x] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | Requires GUI, should return false in headless for now |

### mysql (4/27 detected, 15% coverage) – DEFERRED

**Audit Status:** 14% detected (4/27)

**Context:** Optional external database. 23 missing likely all stubbed for headless (MySQL not typical)

| #   | Verb Name              | Status                                                                           | Notes |
| --- | ---------------------- | -------------------------------------------------------------------------------- | ----- |
| 1   | `clearQuery`           | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 2   | `compileQuery`         | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 3   | `escapeString`         | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 4   | `getAffectedRowCount`  | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 5   | `getClientInfo`        | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 6   | `getClientVersion`     | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 7   | `getColumnCount`       | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 8   | `getErrorMessage`      | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 9   | `getErrorNumber`       | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 10  | `getHostInfo`          | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 11  | `getProtocolInfo`      | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 12  | `getQueryInfo`         | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 13  | `getQueryWarningCount` | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 14  | `getRow`               | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 15  | `getSQLSTATE`          | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 16  | `getSelectedRowCount`  | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 17  | `getServerInfo`        | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 18  | `getServerStatus`      | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 19  | `getServerVersion`     | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 20  | `isThreadSafe`         | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 21  | `pingServer`           | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 22  | `seekRow`              | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 23  | `selectDatabase`       | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |

### op (44/45 detected, 98% coverage)

**Audit Status:** 100% headless compatible

**Context:** See op.md audit: core outline processor. tabkeyreorg has C implementation

**Implementation Status:**
- tabkeyreorg: tabkeyreorgfunc (opverbs.c:139, case at 3299), UserTalk wrapper exists

| #   | Verb Name     | Status                                                                           | Notes                                            |
| --- | ------------- | -------------------------------------------------------------------------------- | ------------------------------------------------ |
| 1   | `tabkeyreorg` | [x] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | C: tabkeyreorgfunc (opverbs.c:139), UserTalk wrap|

### sqlite (2/17 detected, 12% coverage) – DEFERRED

**Audit Status:** 12% detected (2/17)

**Context:** Optional SQLite database. 15 missing likely stubbed for headless (database not typical)

| #   | Verb Name            | Status                                                                           | Notes |
| --- | -------------------- | -------------------------------------------------------------------------------- | ----- |
| 1   | `clearQuery`         | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 2   | `compileQuery`       | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 3   | `getColumn`          | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 4   | `getColumnCount`     | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 5   | `getColumnDouble`    | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 6   | `getColumnInt`       | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 7   | `getColumnName`      | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 8   | `getColumnText`      | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 9   | `getColumnType`      | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 10  | `getErrorMessage`    | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 11  | `getLastInsertRowId` | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 12  | `getRow`             | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 13  | `resetQuery`         | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 14  | `setColumnBlob`      | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |
| 15  | `stepQuery`          | [ ] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED |       |

### statusbar (0/5 detected, 0% coverage)

**Audit Status:** 0% detected (0/5)

**Context:** GUI-dependent status bar. Enums defined in langverbs.c but no case statements (likely stubbed)

**Implementation Status:**
- msg: statusbarmsgfunc (langverbs.c:475) - enum only, NO case statement (stubbed)
- Other 4 verbs have enums but no implementations (stubbed for headless)

| #   | Verb Name       | Status                                                                           | Notes                                                              |
| --- | --------------- | -------------------------------------------------------------------------------- | ------------------------------------------------------------------ |
| 1   | `getmessage`    | [ ] HAS_IMPL / [x] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | C: statusbargetmessagefunc (langverbs.c:483), no case stmt (stub) |
| 2   | `getsectionone` | [ ] HAS_IMPL / [x] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | C: statusbargetsectiononefunc (langverbs.c:481), no case stmt     |
| 3   | `getsections`   | [ ] HAS_IMPL / [x] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | C: statusbargetsectionsfunc (langverbs.c:479), no case stmt       |
| 4   | `msg`           | [ ] HAS_IMPL / [x] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | C: statusbarmsgfunc (langverbs.c:475), no case stmt (stubbed)     |
| 5   | `setsections`   | [ ] HAS_IMPL / [x] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | C: statusbarsetsectionsfunc (langverbs.c:477), no case stmt       |

### string (54/60 detected, 90% coverage)

**Audit Status:** 100% headless compatible

**Context:** See string.md audit: All 6 missing verbs have C implementations in stringverbs.c

**Implementation Status:**
- hashMD5: hashmd5func (line 294, case at 2074), UserTalk wrapper exists
- innerCaseName: innercasefunc (line 290, case at 2050), UserTalk wrapper exists
- lower: lowercasefunc (line 246, case at 1765), UserTalk wrapper exists
- parseHttpArgs: parseargsfunc (line 270, case at 1968), UserTalk wrapper exists
- processhtmlmacros: processmacrosfunc (line 264, case at 1959), UserTalk wrapper exists
- upper: uppercasefunc (line 244, case at 1748), UserTalk wrapper exists

| #   | Verb Name           | Status                                                                           | Notes                                                           |
| --- | ------------------- | -------------------------------------------------------------------------------- | --------------------------------------------------------------- |
| 1   | `hashMD5`           | [x] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | C: hashmd5func (stringverbs.c:294), UserTalk wrapper           |
| 2   | `innercasename`     | [x] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | C: innercasefunc (stringverbs.c:290), UserTalk wrapper         |
| 3   | `lower`             | [x] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | C: lowercasefunc (stringverbs.c:246), UserTalk wrapper         |
| 4   | `parsehttpargs`     | [x] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | C: parseargsfunc (stringverbs.c:270), UserTalk wrapper         |
| 5   | `processhtmlmacros` | [x] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | C: processmacrosfunc (stringverbs.c:264), UserTalk wrapper     |
| 6   | `upper`             | [x] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | C: uppercasefunc (stringverbs.c:244), UserTalk wrapper         |

### sys (13/16 detected, 81% coverage)

**Audit Status:** 100% headless compatible

**Context:** See sys.md audit: missing verbs have C enums but no case statements (stubbed)

**Implementation Status:**
- getenvironmentvariable: getenvironmentvariablefunc (shellsysverbs.c:105) - enum only, NO case statement
- setenvironmentvariable: setenvironmentvariablefunc (shellsysverbs.c:107) - enum only, NO case statement
- winshellcommand: winshellcommandfunc (shellsysverbs.c:111) - enum only, NO case statement (or Windows-only case)

| #   | Verb Name                | Status                                                                           | Notes                                                      |
| --- | ------------------------ | -------------------------------------------------------------------------------- | ---------------------------------------------------------- |
| 1   | `getenvironmentvariable` | [ ] HAS_IMPL / [x] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | C: getenvironmentvariablefunc (shellsysverbs.c:105), stubbed |
| 2   | `setenvironmentvariable` | [ ] HAS_IMPL / [x] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | C: setenvironmentvariablefunc (shellsysverbs.c:107), stubbed |
| 3   | `winshellcommand`        | [ ] HAS_IMPL / [x] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | C: winshellcommandfunc (shellsysverbs.c:111), Windows only |

### table (10/18 detected, 56% coverage)

**Audit Status:** 20/31 script-based headless

**Context:** See table.md audit: HAS_IMPL verbs have C implementations in tableverbs.c

**Implementation Status:**
- getCursor: getcursorfunc (tableverbs.c:72, case at 818), UserTalk wrapper exists
- go: gofunc (tableverbs.c:76, case at 888), UserTalk wrapper exists
- goto: gotofunc (tableverbs.c:78, case at 846), UserTalk wrapper exists
- gotoName: gotonamefunc (tableverbs.c:80, case at 869), UserTalk wrapper exists
- sortBy: sortbyfunc (tableverbs.c:70, case at 766), UserTalk wrapper exists
- getdisplaysettings/getselection/setdisplaysettings: legitimately stubbed for GUI

| #   | Verb Name            | Status                                                                            | Notes                                                |
| --- | -------------------- | --------------------------------------------------------------------------------- | ---------------------------------------------------- |
| 1   | `getcursor`          | [x] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED  | C: getcursorfunc (tableverbs.c:72), UserTalk wrapper |
| 2   | `getdisplaysettings` | [ ] HAS_IMPL / [x] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED  | Requires GUI                                         |
| 3   | `getselection`       | [ ] HAS_IMPL / [x] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED  | Requires GUI                                         |
| 4   | `go`                 | [x] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED  | C: gofunc (tableverbs.c:76), UserTalk wrapper        |
| 5   | `goto`               | [x] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED  | C: gotofunc (tableverbs.c:78), UserTalk wrapper      |
| 6   | `gotoname`           | [x] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED  | C: gotonamefunc (tableverbs.c:80), UserTalk wrapper  |
| 7   | `setdisplaysettings` | [ ] HAS_IMPL / [x] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x ] DEFERRED | Requires GUI                                         |
| 8   | `sortby`             | [x] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED  | C: sortbyfunc (tableverbs.c:70), UserTalk wrapper    |

### window (26/31 detected, 84% coverage)

**Audit Status:** Partial (26/31 headless)

**Context:** Window management mixed. ismodified/setmodified have UserTalk wrappers but no C enum found

**Implementation Status:**
- ismodified: UserTalk wrapper exists, NO C enum found in shellwindowverbs.c (may be in another file)
- setmodified: UserTalk wrapper exists, NO C enum found in shellwindowverbs.c (may be in another file)
- getposition/setposition/dbstats: legitimately stubbed for GUI

| #   | Verb Name     | Status                                                                           | Notes                                                             |
| --- | ------------- | -------------------------------------------------------------------------------- | ----------------------------------------------------------------- |
| 1   | `dbstats`     | [ ] HAS_IMPL / [ ] LEGIT_STUB / [x] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED |                                                                   |
| 2   | `getposition` | [ ] HAS_IMPL / [x] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [x] DEFERRED | Requires GUI                                                      |
| 3   | `ismodified`  | [ ] HAS_IMPL / [ ] LEGIT_STUB / [x] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | UserTalk wrapper exists, no C enum found (may be in another file) |
| 4   | `setmodified` | [ ] HAS_IMPL / [ ] LEGIT_STUB / [x] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | UserTalk wrapper exists, no C enum found (may be in another file) |
| 5   | `setposition` | [ ] HAS_IMPL / [x] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED |                                                                   |

### xml (13/14 detected, 93% coverage)

**Audit Status:** 100% headless compatible

**Context:** See xml.md audit: frontiervaluetotaggedtext is a UserTalk script implementation

**Implementation Status:**
- frontiervaluetotaggedtext: UserTalk script in langxml.c (pure UserTalk implementation, no C enum)

| #   | Verb Name                   | Status                                                                           | Notes                                                        |
| --- | --------------------------- | -------------------------------------------------------------------------------- | ------------------------------------------------------------ |
| 1   | `frontiervaluetotaggedtext` | [x] HAS_IMPL / [ ] LEGIT_STUB / [ ] NEEDS_REVIEW / [ ] MAC_LEGACY / [ ] DEFERRED | UserTalk script in langxml.c (no C kernel implementation)    |

---

## Phase 3 Stub Implementation Strategy

Based on comprehensive investigation of all NEEDS_REVIEW and DEFERRED verbs, the following strategy has been established for stubbing unimplemented verbs in headless mode:

### Category 1: Trigger "Not Implemented" Errors

**OSA/AppleScript Verbs** - These require Mac OS inter-application scripting capabilities with no headless equivalent:
- `lang.DDEevent` - Mac-only Apple event communication
- `lang.callxcmd` - Mac Classic XCMD external command interface
- `lang.countapplelistitems` - Apple list manipulation
- `lang.getapplelistitem` - Apple list access
- `lang.putapplelistitem` - Apple list modification
- `lang.geteventattribute` - Apple event attribute access
- `lang.seteventinteraction` - Apple event interaction control
- `lang.transactionEvent` - Apple transaction event handling

**GUI-Only Verbs** - These require graphical windows/dialogs with no headless equivalent:
- `window.dbstats` - Database statistics visualization window
- `dialog.*` (hideitem, ismodalcard, runcard, setmodalcardtimeout, showitem) - Modal dialog operations
- `statusbar.*` (all verbs) - Status bar operations
- `window.getposition`, `window.setposition` - Window positioning

**Admin-Required Operations** - These require elevated privileges unavailable in normal execution:
- `clock.set` - Requires admin/root to set system time
- `file.mountservervolume` - Requires admin to mount network volumes programmatically

**Platform-Specific (Unavailable)** - These are for platforms not yet supported:
- `sys.winshellcommand` - Windows-specific shell command execution

**Optional Database Systems** - External databases not included in standard distribution:
- All `mysql.*` verbs (23 total) - MySQL driver not available
- All `sqlite.*` verbs (15 total) - SQLite driver not available

### Category 2: Return Silent Success (Noop with True)

**Non-Critical Display Operations** - These don't affect script logic; safe to succeed silently:
- `table.getdisplaysettings` - Returns true (no actual display settings in headless)
- `table.setdisplaysettings` - Returns true (display settings irrelevant in headless)

**Memory Management Legacy Code** - Modern GC makes these unnecessary:
- `lang.flushmemory` - Returns true (memory is automatically managed on modern systems)

### Category 3: Delegate to Object State

**Window Modification Tracking** - Modern architecture delegates to object's dirty flag:
- `window.isModified(adr)` - Finds the object and returns its `fldirty` flag value
- `window.setModified(adr, modified)` - Finds the object and updates its `fldirty` flag

This maintains backward compatibility while using the proper object-based dirty tracking model.

### Category 4: Keep Hybrid UserTalk/C Implementation

**Modern Multi-Platform Paths** - These use platform detection in UserTalk with C kernel calls:
- `file.getspecialfolderpath` - UserTalk logic for platform selection, C kernel for actual paths
- `file.getsystemfolderpath` - UserTalk logic for platform selection, C kernel for actual paths

This approach allows flexibility to update modern path strategies without changing C code.

### Category 5: Already Implemented

**Unix Shell Commands** - Already works on Unix-like systems:
- `sys.unixshellcommand` - Already has full C implementation (calls unixshellcall)

---

## Implementation Notes

- **Error Messages:** All "not implemented" errors should be raised at the UserTalk level with clear, user-friendly messages explaining why the operation is unavailable in headless mode.
- **Script Continuity:** Where possible (Categories 2 and 3), verbs should succeed silently or with sensible defaults to allow legacy scripts to continue without modification.
- **Future Expansion:** As support for Windows, optional databases, or GUI modes is added, these categories can be updated with actual implementations.
- **Testing:** Each stub implementation should be tested to ensure it doesn't break existing scripts that might call these verbs.
