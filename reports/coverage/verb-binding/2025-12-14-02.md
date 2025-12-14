# Automatic Verb Binding Coverage Report

**Generated:** 2025-12-14 14:35:53
**Total Processors:** 51
**Total Verbs:** 707

## Executive Summary

- **Processors detected:** 27/51
- **Verbs implemented:** 400/707 (56%)

## Detailed Processor Coverage

### base64

**Status:** 0/2 verbs (0% implemented, 2 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `encode` | ⬜ Stub | — |
| 1 | `decode` | ⬜ Stub | — |

### bit

**Status:** 0/8 verbs (0% implemented, 8 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `get` | ⬜ Stub | — |
| 1 | `set` | ⬜ Stub | — |
| 2 | `clear` | ⬜ Stub | — |
| 3 | `logicaland` | ⬜ Stub | — |
| 4 | `logicalor` | ⬜ Stub | — |
| 5 | `logicalxor` | ⬜ Stub | — |
| 6 | `shiftleft` | ⬜ Stub | — |
| 7 | `shiftright` | ⬜ Stub | — |

### clipboard 🖥️ GUI

**Status:** 0/2 verbs (0% implemented, 2 stubbed)

*All verbs are GUI-dependent stubs (headless)*

### clock

**Status:** 6/7 verbs (85% implemented, 1 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `now` | ✅ Implemented | langverbs.c:1905 |
| 1 | `set` | ⬜ Stub | — |
| 2 | `sleepfor` | ✅ Implemented | langverbs.c:2927 |
| 3 | `ticks` | ✅ Implemented | langverbs.c:2899 |
| 4 | `milliseconds` | ✅ Implemented | langverbs.c:2905 |
| 5 | `waitseconds` | ✅ Implemented | langverbs.c:2912 |
| 6 | `waitsixtieths` | ✅ Implemented | langverbs.c:2947 |

### crypt

**Status:** 1/5 verbs (20% implemented, 4 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `whirlpool` | ✅ Implemented | langcrypt.c:76 |
| 1 | `hmacMD5` | ⬜ Stub | — |
| 2 | `MD5` | ⬜ Stub | — |
| 3 | `SHA1` | ⬜ Stub | — |
| 4 | `hmacSHA1` | ⬜ Stub | — |

### date

**Status:** 26/30 verbs (86% implemented, 4 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `get` | ✅ Implemented | langverbs.c:1948 |
| 1 | `set` | ✅ Implemented | langverbs.c:1919 |
| 2 | `abbrevstring` | ✅ Implemented | langverbs.c:2024 |
| 3 | `dayofweek` | ✅ Implemented | langverbs.c:2037 |
| 4 | `daysinmonth` | ✅ Implemented | langverbs.c:2051 |
| 5 | `daystring` | ✅ Implemented | langverbs.c:2067 |
| 6 | `firstofmonth` | ✅ Implemented | langverbs.c:2083 |
| 7 | `lastofmonth` | ✅ Implemented | langverbs.c:2096 |
| 8 | `longstring` | ✅ Implemented | langverbs.c:2109 |
| 9 | `nextmonth` | ✅ Implemented | langverbs.c:2122 |
| 10 | `nextweek` | ✅ Implemented | langverbs.c:2135 |
| 11 | `nextyear` | ✅ Implemented | langverbs.c:2148 |
| 12 | `prevmonth` | ✅ Implemented | langverbs.c:2161 |
| 13 | `prevweek` | ✅ Implemented | langverbs.c:2174 |
| 14 | `prevyear` | ✅ Implemented | langverbs.c:2187 |
| 15 | `shortstring` | ✅ Implemented | langverbs.c:2200 |
| 16 | `tomorrow` | ✅ Implemented | langverbs.c:2213 |
| 17 | `weeksinmonth` | ✅ Implemented | langverbs.c:2226 |
| 18 | `yesterday` | ✅ Implemented | langverbs.c:2247 |
| 19 | `getcurrenttimezone` | ✅ Implemented | langverbs.c:2260 |
| 20 | `netstandardstring` | ✅ Implemented | langverbs.c:2267 |
| 21 | `monthtostring` | ✅ Implemented | langverbs.c:2278 |
| 22 | `dayofweektostring` | ✅ Implemented | langverbs.c:2289 |
| 23 | `versionlessthan` | ✅ Implemented | langverbs.c:2300 |
| 24 | `day` | ✅ Implemented | langverbs.c:1983 |
| 25 | `month` | ⬜ Stub | — |
| 26 | `year` | ⬜ Stub | — |
| 27 | `hour` | ⬜ Stub | — |
| 28 | `minute` | ⬜ Stub | — |
| 29 | `seconds` | ✅ Implemented | langverbs.c:1988 |

### db

**Status:** 11/13 verbs (84% implemented, 2 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `new` | ✅ Implemented | dbverbs.c:943 |
| 1 | `open` | ✅ Implemented | dbverbs.c:946 |
| 2 | `save` | ✅ Implemented | dbverbs.c:949 |
| 3 | `close` | ✅ Implemented | dbverbs.c:952 |
| 4 | `defined` | ✅ Implemented | dbverbs.c:955 |
| 5 | `getvalue` | ✅ Implemented | dbverbs.c:958 |
| 6 | `setvalue` | ✅ Implemented | dbverbs.c:961 |
| 7 | `delete` | ✅ Implemented | dbverbs.c:964 |
| 8 | `newTable` | ⬜ Stub | — |
| 9 | `isTable` | ⬜ Stub | — |
| 10 | `countitems` | ✅ Implemented | dbverbs.c:973 |
| 11 | `getnthitem` | ✅ Implemented | dbverbs.c:976 |
| 12 | `getmoddate` | ✅ Implemented | dbverbs.c:979 |

### dialog 🖥️ GUI

**Status:** 14/19 verbs (73% implemented, 5 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `alert` | ✅ Implemented | langverbs.c:2783 |
| 1 | `run` | ✅ Implemented | langverbs.c:2793 |
| 2 | `runmodeless` | ✅ Implemented | langverbs.c:1803 |
| 3 | `runcard` | ⬜ Stub | — |
| 4 | `runmodalcard` | ✅ Implemented | langverbs.c:1806 |
| 5 | `ismodalcard` | ⬜ Stub | — |
| 6 | `setmodalcardtimeout` | ⬜ Stub | — |
| 7 | `getvalue` | ✅ Implemented | langverbs.c:2802 |
| 8 | `setvalue` | ✅ Implemented | langverbs.c:2805 |
| 9 | `setitemenable` | ✅ Implemented | langverbs.c:2808 |
| 10 | `showitem` | ⬜ Stub | — |
| 11 | `hideitem` | ⬜ Stub | — |
| 12 | `twoway` | ✅ Implemented | langverbs.c:2817 |
| 13 | `threeway` | ✅ Implemented | langverbs.c:2820 |
| 14 | `ask` | ✅ Implemented | langverbs.c:2823 |
| 15 | `getint` | ✅ Implemented | langverbs.c:2829 |
| 16 | `notify` | ✅ Implemented | langverbs.c:2832 |
| 17 | `getuserinfo` | ✅ Implemented | langverbs.c:2857 |
| 18 | `getpassword` | ✅ Implemented | langverbs.c:2826 |

### dll

**Status:** 0/4 verbs (0% implemented, 4 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `call` | ⬜ Stub | — |
| 1 | `load` | ⬜ Stub | — |
| 2 | `unload` | ⬜ Stub | — |
| 3 | `isloaded` | ⬜ Stub | — |

### editmenu 🖥️ GUI

**Status:** 0/16 verbs (0% implemented, 16 stubbed)

*All verbs are GUI-dependent stubs (headless)*

### file

**Status:** 60/86 verbs (69% implemented, 26 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `created` | ✅ Implemented | fileverbs.c:2630 |
| 1 | `modified` | ✅ Implemented | fileverbs.c:2653 |
| 2 | `type` | ✅ Implemented | fileverbs.c:2669 |
| 3 | `creator` | ✅ Implemented | fileverbs.c:2685 |
| 4 | `setcreated` | ⬜ Stub | — |
| 5 | `setmodified` | ⬜ Stub | — |
| 6 | `settype` | ⬜ Stub | — |
| 7 | `setcreator` | ⬜ Stub | — |
| 8 | `isfolder` | ✅ Implemented | fileverbs.c:2741 |
| 9 | `isvolume` | ✅ Implemented | fileverbs.c:2755 |
| 10 | `islocked` | ✅ Implemented | fileverbs.c:2768 |
| 11 | `lock` | ✅ Implemented | fileverbs.c:2911 |
| 12 | `unlock` | ✅ Implemented | fileverbs.c:2935 |
| 13 | `copy` | ✅ Implemented | fileverbs.c:2972 |
| 14 | `copydatafork` | ✅ Implemented | fileverbs.c:2975 |
| 15 | `copyresourcefork` | ✅ Implemented | fileverbs.c:2978 |
| 16 | `delete` | ✅ Implemented | fileverbs.c:2981 |
| 17 | `rename` | ✅ Implemented | fileverbs.c:3077 |
| 18 | `exists` | ✅ Implemented | fileverbs.c:2984 |
| 19 | `size` | ✅ Implemented | fileverbs.c:2896 |
| 20 | `fullpath` | ✅ Implemented | fileverbs.c:2959 |
| 21 | `getpath` | ✅ Implemented | fileverbs.c:3006 |
| 22 | `setpath` | ✅ Implemented | fileverbs.c:3018 |
| 23 | `filefrompath` | ✅ Implemented | fileverbs.c:3003 |
| 24 | `folderfrompath` | ✅ Implemented | fileverbs.c:3470 |
| 25 | `getsystemfolderpath` | ⬜ Stub | — |
| 26 | `getspecialfolderpath` | ⬜ Stub | — |
| 27 | `new` | ✅ Implemented | fileverbs.c:3036 |
| 28 | `newfolder` | ✅ Implemented | fileverbs.c:3061 |
| 29 | `newalias` | ✅ Implemented | fileverbs.c:3366 |
| 30 | `getfiledialog` | ⬜ Stub | — |
| 31 | `putfiledialog` | ⬜ Stub | — |
| 32 | `getfolderdialog` | ⬜ Stub | — |
| 33 | `getdiskdialog` | ⬜ Stub | — |
| 34 | `geticonpos` | ✅ Implemented | fileverbs.c:3372 |
| 35 | `seticonpos` | ✅ Implemented | fileverbs.c:3375 |
| 36 | `getversion` | ⬜ Stub | — |
| 37 | `setversion` | ⬜ Stub | — |
| 38 | `getfullversion` | ⬜ Stub | — |
| 39 | `setfullversion` | ⬜ Stub | — |
| 40 | `getcomment` | ✅ Implemented | fileverbs.c:3357 |
| 41 | `setcomment` | ✅ Implemented | fileverbs.c:3384 |
| 42 | `getlabel` | ✅ Implemented | fileverbs.c:3387 |
| 43 | `setlabel` | ✅ Implemented | fileverbs.c:3390 |
| 44 | `findapplication` | ⬜ Stub | — |
| 45 | `isbusy` | ✅ Implemented | fileverbs.c:2790 |
| 46 | `hasbundle` | ✅ Implemented | fileverbs.c:2804 |
| 47 | `setbundle` | ✅ Implemented | fileverbs.c:2820 |
| 48 | `isalias` | ✅ Implemented | fileverbs.c:2847 |
| 49 | `isvisible` | ✅ Implemented | fileverbs.c:2862 |
| 50 | `setvisible` | ✅ Implemented | fileverbs.c:2876 |
| 51 | `followalias` | ✅ Implemented | fileverbs.c:3369 |
| 52 | `move` | ✅ Implemented | fileverbs.c:3103 |
| 53 | `eject` | ⬜ Stub | — |
| 54 | `isejectable` | ⬜ Stub | — |
| 55 | `freespaceonvolume` | ⬜ Stub | — |
| 56 | `volumesize` | ✅ Implemented | fileverbs.c:3188 |
| 57 | `volumeblocksize` | ✅ Implemented | fileverbs.c:3240 |
| 58 | `filesonvolume` | ✅ Implemented | fileverbs.c:3257 |
| 59 | `foldersonvolume` | ✅ Implemented | fileverbs.c:3274 |
| 60 | `unmountvolume` | ✅ Implemented | fileverbs.c:3393 |
| 61 | `mountservervolume` | ⬜ Stub | — |
| 62 | `findinfile` | ✅ Implemented | fileverbs.c:3298 |
| 63 | `countlines` | ✅ Implemented | fileverbs.c:3301 |
| 64 | `open` | ⬜ Stub | — |
| 65 | `close` | ⬜ Stub | — |
| 66 | `endoffile` | ✅ Implemented | fileverbs.c:3310 |
| 67 | `setendoffile` | ✅ Implemented | fileverbs.c:3313 |
| 68 | `getendoffile` | ✅ Implemented | fileverbs.c:3316 |
| 69 | `setposition` | ✅ Implemented | fileverbs.c:3319 |
| 70 | `getposition` | ✅ Implemented | fileverbs.c:3322 |
| 71 | `readline` | ✅ Implemented | fileverbs.c:3325 |
| 72 | `writeline` | ✅ Implemented | fileverbs.c:3328 |
| 73 | `read` | ✅ Implemented | fileverbs.c:3331 |
| 74 | `write` | ✅ Implemented | fileverbs.c:3334 |
| 75 | `compare` | ✅ Implemented | fileverbs.c:3337 |
| 76 | `writewholefile` | ✅ Implemented | fileverbs.c:3342 |
| 77 | `getpathchar` | ✅ Implemented | fileverbs.c:3345 |
| 78 | `freespaceonvolumedouble` | ⬜ Stub | — |
| 79 | `volumesizedouble` | ✅ Implemented | fileverbs.c:3222 |
| 80 | `getmp3info` | ✅ Implemented | fileverbs.c:3533 |
| 81 | `readwholefile` | ✅ Implemented | fileverbs.c:3576 |
| 82 | `getLabelIndex` | ⬜ Stub | — |
| 83 | `setLabelIndex` | ⬜ Stub | — |
| 84 | `getLabelNames` | ⬜ Stub | — |
| 85 | `getPosixPath` | ⬜ Stub | — |

### filemenu 🖥️ GUI

**Status:** 0/10 verbs (0% implemented, 10 stubbed)

*All verbs are GUI-dependent stubs (headless)*

### frontier

**Status:** 14/14 verbs (100% implemented, 0 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `getprogrampath` | ✅ Implemented | shellsysverbs.c:712 |
| 1 | `getfilepath` | ✅ Implemented | shellsysverbs.c:721 |
| 2 | `enableagents` | ✅ Implemented | shellsysverbs.c:739 |
| 3 | `requesttofront` | ✅ Implemented | shellsysverbs.c:700 |
| 4 | `isruntime` | ✅ Implemented | shellsysverbs.c:767 |
| 5 | `countthreads` | ✅ Implemented | shellsysverbs.c:775 |
| 6 | `ispowerpc` | ✅ Implemented | shellsysverbs.c:782 |
| 7 | `reclaimmemory` | ✅ Implemented | shellsysverbs.c:792 |
| 8 | `version` | ✅ Implemented | shellsysverbs.c:803 |
| 9 | `hashstats` | ✅ Implemented | shellsysverbs.c:806 |
| 10 | `gethashloopcount` | ✅ Implemented | shellsysverbs.c:813 |
| 11 | `hideapplication` | ✅ Implemented | shellsysverbs.c:826 |
| 12 | `isvalidserialnumber` | ✅ Implemented | shellsysverbs.c:832 |
| 13 | `showapplication` | ✅ Implemented | shellsysverbs.c:845 |

### html

**Status:** 21/23 verbs (91% implemented, 2 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `processmacros` | ✅ Implemented | langhtml.c:9833 |
| 1 | `urldecode` | ✅ Implemented | langhtml.c:9836 |
| 2 | `urlencode` | ✅ Implemented | langhtml.c:9839 |
| 3 | `parsehttpargs` | ⬜ Stub | — |
| 4 | `iso8859encode` | ✅ Implemented | langhtml.c:9845 |
| 5 | `getgifheightwidth` | ✅ Implemented | langhtml.c:9848 |
| 6 | `getjpegheightwidth` | ✅ Implemented | langhtml.c:9851 |
| 7 | `buildpagetable` | ✅ Implemented | langhtml.c:9854 |
| 8 | `refglossary` | ✅ Implemented | langhtml.c:9857 |
| 9 | `getpref` | ✅ Implemented | langhtml.c:9860 |
| 10 | `getonedirective` | ✅ Implemented | langhtml.c:9863 |
| 11 | `rundirective` | ✅ Implemented | langhtml.c:9866 |
| 12 | `rundirectives` | ✅ Implemented | langhtml.c:9869 |
| 13 | `runoutlinedirectives` | ✅ Implemented | langhtml.c:9872 |
| 14 | `cleanforexport` | ✅ Implemented | langhtml.c:9875 |
| 15 | `normalizename` | ✅ Implemented | langhtml.c:9878 |
| 16 | `glossarypatcher` | ✅ Implemented | langhtml.c:9881 |
| 17 | `expandurls` | ✅ Implemented | langhtml.c:9884 |
| 18 | `traversalskip` | ✅ Implemented | langhtml.c:9887 |
| 19 | `getpagetableaddress` | ✅ Implemented | langhtml.c:9890 |
| 20 | `neutermacros` | ✅ Implemented | langhtml.c:9893 |
| 21 | `neutertags` | ✅ Implemented | langhtml.c:9896 |
| 22 | `drawcalendar` | ⬜ Stub | — |

### htmlcontrol 🖥️ GUI

**Status:** 0/8 verbs (0% implemented, 8 stubbed)

*All verbs are GUI-dependent stubs (headless)*

### inetd

**Status:** 0/1 verbs (0% implemented, 1 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `supervisor` | ⬜ Stub | — |

### kb

**Status:** 4/4 verbs (100% implemented, 0 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `optionkey` | ✅ Implemented | langverbs.c:579 |
| 1 | `cmdkey` | ✅ Implemented | langverbs.c:582 |
| 2 | `shiftkey` | ✅ Implemented | langverbs.c:585 |
| 3 | `controlkey` | ✅ Implemented | langverbs.c:588 |

### lang

**Status:** 43/58 verbs (74% implemented, 15 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `scripterror` | ⬜ Stub | — |
| 1 | `new` | ✅ Implemented | langverbs.c:1857 |
| 2 | `delete` | ⬜ Stub | — |
| 3 | `edit` | ⬜ Stub | — |
| 4 | `close` | ✅ Implemented | langverbs.c:1867 |
| 5 | `timecreated` | ✅ Implemented | langverbs.c:3016 |
| 6 | `timemodified` | ✅ Implemented | langverbs.c:3025 |
| 7 | `settimecreated` | ✅ Implemented | langverbs.c:3034 |
| 8 | `settimemodified` | ✅ Implemented | langverbs.c:3037 |
| 9 | `boolean` | ✅ Implemented | langverbs.c:1885 |
| 10 | `char` | ✅ Implemented | langverbs.c:1890 |
| 11 | `short` | ⬜ Stub | — |
| 12 | `long` | ✅ Implemented | langverbs.c:1900 |
| 13 | `date` | ✅ Implemented | langverbs.c:1914 |
| 14 | `direction` | ✅ Implemented | langverbs.c:2319 |
| 15 | `string4` | ⬜ Stub | — |
| 16 | `string` | ✅ Implemented | langverbs.c:2329 |
| 17 | `displaystring` | ✅ Implemented | langverbs.c:2340 |
| 18 | `address` | ✅ Implemented | langverbs.c:2357 |
| 19 | `binary` | ✅ Implemented | langverbs.c:2362 |
| 20 | `getbinarytype` | ✅ Implemented | langverbs.c:2367 |
| 21 | `setbinarytype` | ✅ Implemented | langverbs.c:2386 |
| 22 | `point` | ✅ Implemented | langverbs.c:2416 |
| 23 | `rect` | ✅ Implemented | langverbs.c:2456 |
| 24 | `rgb` | ✅ Implemented | langverbs.c:2515 |
| 25 | `pattern` | ✅ Implemented | langverbs.c:2560 |
| 26 | `fixed` | ✅ Implemented | langverbs.c:2565 |
| 27 | `single` | ✅ Implemented | langverbs.c:2570 |
| 28 | `double` | ✅ Implemented | langverbs.c:2575 |
| 29 | `filespec` | ✅ Implemented | langverbs.c:2580 |
| 30 | `alias` | ✅ Implemented | langverbs.c:2585 |
| 31 | `list` | ✅ Implemented | langverbs.c:2590 |
| 32 | `record` | ✅ Implemented | langverbs.c:2598 |
| 33 | `enum` | ✅ Implemented | langverbs.c:2606 |
| 34 | `memavail` | ✅ Implemented | langverbs.c:2614 |
| 35 | `flushmemory` | ⬜ Stub | — |
| 36 | `random` | ✅ Implemented | langverbs.c:2637 |
| 37 | `evaluate` | ✅ Implemented | langverbs.c:2745 |
| 38 | `evaluatethread` | ✅ Implemented | langverbs.c:2767 |
| 39 | `rollbeachball` | ⬜ Stub | — |
| 40 | `abs` | ✅ Implemented | langverbs.c:2721 |
| 41 | `seteventtimeout` | ✅ Implemented | langverbs.c:2977 |
| 42 | `seteventtransactionid` | ✅ Implemented | langverbs.c:2980 |
| 43 | `seteventinteraction` | ⬜ Stub | — |
| 44 | `geteventattribute` | ⬜ Stub | — |
| 45 | `coerceappleitem` | ✅ Implemented | langverbs.c:2989 |
| 46 | `getapplelistitem` | ⬜ Stub | — |
| 47 | `putapplelistitem` | ⬜ Stub | — |
| 48 | `countapplelistitems` | ⬜ Stub | — |
| 49 | `systemevent` | ✅ Implemented | langverbs.c:3006 |
| 50 | `DDEevent` | ⬜ Stub | — |
| 51 | `transactionEvent` | ⬜ Stub | — |
| 52 | `msg` | ✅ Implemented | langverbs.c:1802 |
| 53 | `callxcmd` | ⬜ Stub | — |
| 54 | `calldll` | ✅ Implemented | langverbs.c:3048 |
| 55 | `packwindow` | ✅ Implemented | langverbs.c:3060 |
| 56 | `unpackwindow` | ✅ Implemented | langverbs.c:3063 |
| 57 | `callscript` | ✅ Implemented | langverbs.c:3044 |

### launch 🖥️ GUI

**Status:** 0/5 verbs (0% implemented, 5 stubbed)

*All verbs are GUI-dependent stubs (headless)*

### mainwindow 🖥️ GUI

**Status:** 0/7 verbs (0% implemented, 7 stubbed)

*All verbs are GUI-dependent stubs (headless)*

### math

**Status:** 3/3 verbs (100% implemented, 0 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `min` | ✅ Implemented | langmath.c:74 |
| 1 | `max` | ✅ Implemented | langmath.c:178 |
| 2 | `sqrt` | ✅ Implemented | langmath.c:282 |

### menu

**Status:** 12/14 verbs (85% implemented, 2 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `zoomscript` | ✅ Implemented | menuverbs.c:1954 |
| 1 | `buildmenubar` | ✅ Implemented | menuverbs.c:1876 |
| 2 | `clearmenubar` | ✅ Implemented | menuverbs.c:1884 |
| 3 | `isinstalled` | ⬜ Stub | — |
| 4 | `install` | ✅ Implemented | menuverbs.c:1898 |
| 5 | `remove` | ✅ Implemented | menuverbs.c:1904 |
| 6 | `getscript` | ⬜ Stub | — |
| 7 | `setscript` | ✅ Implemented | menuverbs.c:1862 |
| 8 | `addmenucommand` | ✅ Implemented | menuverbs.c:1910 |
| 9 | `deletemenucommand` | ✅ Implemented | menuverbs.c:1916 |
| 10 | `addsubmenu` | ✅ Implemented | menuverbs.c:1922 |
| 11 | `deletesubmenu` | ✅ Implemented | menuverbs.c:1928 |
| 12 | `getcommandkey` | ✅ Implemented | menuverbs.c:2011 |
| 13 | `setcommandkey` | ✅ Implemented | menuverbs.c:2016 |

### mouse 🖥️ GUI

**Status:** 2/2 verbs (100% implemented, 0 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `button` | ✅ Implemented | langverbs.c:2670 |
| 1 | `location` | ✅ Implemented | langverbs.c:2676 |

### mrcalendar 🖥️ GUI

**Status:** 0/11 verbs (0% implemented, 11 stubbed)

*All verbs are GUI-dependent stubs (headless)*

### mysql

**Status:** 4/27 verbs (14% implemented, 23 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `init` | ✅ Implemented | langmysql.c:113 |
| 1 | `end` | ✅ Implemented | langmysql.c:117 |
| 2 | `connect` | ✅ Implemented | langmysql.c:121 |
| 3 | `compileQuery` | ⬜ Stub | — |
| 4 | `clearQuery` | ⬜ Stub | — |
| 5 | `getRow` | ⬜ Stub | — |
| 6 | `getErrorNumber` | ⬜ Stub | — |
| 7 | `getErrorMessage` | ⬜ Stub | — |
| 8 | `getClientInfo` | ⬜ Stub | — |
| 9 | `getClientVersion` | ⬜ Stub | — |
| 10 | `getHostInfo` | ⬜ Stub | — |
| 11 | `getServerVersion` | ⬜ Stub | — |
| 12 | `getProtocolInfo` | ⬜ Stub | — |
| 13 | `getServerInfo` | ⬜ Stub | — |
| 14 | `getQueryInfo` | ⬜ Stub | — |
| 15 | `getAffectedRowCount` | ⬜ Stub | — |
| 16 | `getSelectedRowCount` | ⬜ Stub | — |
| 17 | `getColumnCount` | ⬜ Stub | — |
| 18 | `getServerStatus` | ⬜ Stub | — |
| 19 | `getQueryWarningCount` | ⬜ Stub | — |
| 20 | `pingServer` | ⬜ Stub | — |
| 21 | `seekRow` | ⬜ Stub | — |
| 22 | `selectDatabase` | ⬜ Stub | — |
| 23 | `getSQLSTATE` | ⬜ Stub | — |
| 24 | `escapeString` | ⬜ Stub | — |
| 25 | `isThreadSafe` | ⬜ Stub | — |
| 26 | `close` | ✅ Implemented | langmysql.c:217 |

### op

**Status:** 44/45 verbs (97% implemented, 1 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `getlinetext` | ✅ Implemented | opverbs.c:3429 |
| 1 | `level` | ✅ Implemented | opverbs.c:3445 |
| 2 | `countsubs` | ✅ Implemented | opverbs.c:3453 |
| 3 | `countsummits` | ✅ Implemented | opverbs.c:3466 |
| 4 | `go` | ✅ Implemented | opverbs.c:3474 |
| 5 | `firstsummit` | ✅ Implemented | opverbs.c:3495 |
| 6 | `expand` | ✅ Implemented | opverbs.c:3507 |
| 7 | `collapse` | ✅ Implemented | opverbs.c:3528 |
| 8 | `subsexpanded` | ✅ Implemented | opverbs.c:3544 |
| 9 | `insert` | ✅ Implemented | opverbs.c:3554 |
| 10 | `find` | ✅ Implemented | opverbs.c:3575 |
| 11 | `sort` | ✅ Implemented | opverbs.c:3580 |
| 12 | `setlinetext` | ✅ Implemented | opverbs.c:3590 |
| 13 | `reorg` | ✅ Implemented | opverbs.c:3605 |
| 14 | `promote` | ✅ Implemented | opverbs.c:3646 |
| 15 | `demote` | ✅ Implemented | opverbs.c:3656 |
| 16 | `hoist` | ✅ Implemented | opverbs.c:3666 |
| 17 | `dehoist` | ✅ Implemented | opverbs.c:3677 |
| 18 | `deletesubs` | ✅ Implemented | opverbs.c:3624 |
| 19 | `deleteline` | ✅ Implemented | opverbs.c:3634 |
| 20 | `tabkeyreorg` | ⬜ Stub | — |
| 21 | `flatcursorkeys` | ✅ Implemented | opverbs.c:3310 |
| 22 | `getdisplay` | ✅ Implemented | opverbs.c:3749 |
| 23 | `setdisplay` | ✅ Implemented | opverbs.c:3759 |
| 24 | `getcursor` | ✅ Implemented | opverbs.c:3774 |
| 25 | `setcursor` | ✅ Implemented | opverbs.c:3782 |
| 26 | `getrefcon` | ✅ Implemented | opverbs.c:3797 |
| 27 | `setrefcon` | ✅ Implemented | opverbs.c:3808 |
| 28 | `getexpansionstate` | ✅ Implemented | opverbs.c:3819 |
| 29 | `setexpansionstate` | ✅ Implemented | opverbs.c:3827 |
| 30 | `getscrollstate` | ✅ Implemented | opverbs.c:3843 |
| 31 | `setscrollstate` | ✅ Implemented | opverbs.c:3851 |
| 32 | `getsuboutline` | ✅ Implemented | opverbs.c:3864 |
| 33 | `insertoutline` | ✅ Implemented | opverbs.c:3881 |
| 34 | `setmodified` | ✅ Implemented | opverbs.c:3949 |
| 35 | `getselection` | ✅ Implemented | opverbs.c:3976 |
| 36 | `getheadnumber` | ✅ Implemented | opverbs.c:3983 |
| 37 | `visitall` | ✅ Implemented | opverbs.c:3383 |
| 38 | `getselectedsuboutlines` | ✅ Implemented | opverbs.c:3995 |
| 39 | `xmltooutline` | ✅ Implemented | opverbs.c:3391 |
| 40 | `outlinetoxml` | ✅ Implemented | opverbs.c:3399 |
| 41 | `sethtmlformatting` | ✅ Implemented | opverbs.c:4012 |
| 42 | `gethtmlformatting` | ✅ Implemented | opverbs.c:4061 |
| 43 | `setdynamic` | ✅ Implemented | opverbs.c:4075 |
| 44 | `getdynamic` | ✅ Implemented | opverbs.c:4091 |

### opattributes

**Status:** 5/5 verbs (100% implemented, 0 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `addgroup` | ✅ Implemented | opverbs.c:4102 |
| 1 | `getall` | ✅ Implemented | opverbs.c:4109 |
| 2 | `getone` | ✅ Implemented | opverbs.c:4137 |
| 3 | `makeempty` | ✅ Implemented | opverbs.c:4168 |
| 4 | `setone` | ✅ Implemented | opverbs.c:4174 |

### osa

**Status:** 0/2 verbs (0% implemented, 2 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `compile` | ⬜ Stub | — |
| 1 | `getsource` | ⬜ Stub | — |

### pict

**Status:** 4/4 verbs (100% implemented, 0 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `scheduleupdate` | ✅ Implemented | pictverbs.c:826 |
| 1 | `expressions` | ✅ Implemented | pictverbs.c:838 |
| 2 | `getpicture` | ✅ Implemented | pictverbs.c:843 |
| 3 | `setpicture` | ✅ Implemented | pictverbs.c:848 |

### point

**Status:** 2/2 verbs (100% implemented, 0 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `get` | ✅ Implemented | langverbs.c:2437 |
| 1 | `set` | ✅ Implemented | langverbs.c:2421 |

### python

**Status:** 0/1 verbs (0% implemented, 1 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `doscript` | ⬜ Stub | — |

### re

**Status:** 0/10 verbs (0% implemented, 10 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `compile` | ⬜ Stub | — |
| 1 | `match` | ⬜ Stub | — |
| 2 | `replace` | ⬜ Stub | — |
| 3 | `extract` | ⬜ Stub | — |
| 4 | `split` | ⬜ Stub | — |
| 5 | `join` | ⬜ Stub | — |
| 6 | `visit` | ⬜ Stub | — |
| 7 | `grep` | ⬜ Stub | — |
| 8 | `getpatterninfo` | ⬜ Stub | — |
| 9 | `expand` | ⬜ Stub | — |

### rectangle

**Status:** 2/2 verbs (100% implemented, 0 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `get` | ✅ Implemented | langverbs.c:2489 |
| 1 | `set` | ✅ Implemented | langverbs.c:2461 |

### rez

**Status:** 0/15 verbs (0% implemented, 15 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `getresource` | ⬜ Stub | — |
| 1 | `putresource` | ⬜ Stub | — |
| 2 | `getnamedresource` | ⬜ Stub | — |
| 3 | `putnamedresource` | ⬜ Stub | — |
| 4 | `countrestypes` | ⬜ Stub | — |
| 5 | `getnthrestype` | ⬜ Stub | — |
| 6 | `countresources` | ⬜ Stub | — |
| 7 | `getnthresource` | ⬜ Stub | — |
| 8 | `getnthresinfo` | ⬜ Stub | — |
| 9 | `resourceexists` | ⬜ Stub | — |
| 10 | `namedresourceexists` | ⬜ Stub | — |
| 11 | `deleteresource` | ⬜ Stub | — |
| 12 | `deletenamedresource` | ⬜ Stub | — |
| 13 | `getresourceattributes` | ⬜ Stub | — |
| 14 | `setresourceattributes` | ⬜ Stub | — |

### rgb

**Status:** 2/2 verbs (100% implemented, 0 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `get` | ✅ Implemented | langverbs.c:2537 |
| 1 | `set` | ✅ Implemented | langverbs.c:2520 |

### script

**Status:** 0/13 verbs (0% implemented, 13 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `compile` | ⬜ Stub | — |
| 1 | `uncompile` | ⬜ Stub | — |
| 2 | `getcode` | ⬜ Stub | — |
| 3 | `getlanguage` | ⬜ Stub | — |
| 4 | `setlanguage` | ⬜ Stub | — |
| 5 | `makecomment` | ⬜ Stub | — |
| 6 | `uncomment` | ⬜ Stub | — |
| 7 | `iscomment` | ⬜ Stub | — |
| 8 | `getbreakpoint` | ⬜ Stub | — |
| 9 | `setbreakpoint` | ⬜ Stub | — |
| 10 | `clearbreakpoint` | ⬜ Stub | — |
| 11 | `startprofile` | ⬜ Stub | — |
| 12 | `stopprofile` | ⬜ Stub | — |

### search

**Status:** 0/6 verbs (0% implemented, 6 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `reset` | ⬜ Stub | — |
| 1 | `findnext` | ⬜ Stub | — |
| 2 | `replace` | ⬜ Stub | — |
| 3 | `replaceall` | ⬜ Stub | — |
| 4 | `findtextdialog` | ⬜ Stub | — |
| 5 | `replacetextdialog` | ⬜ Stub | — |

### searchengine

**Status:** 0/5 verbs (0% implemented, 5 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `stripmarkup` | ⬜ Stub | — |
| 1 | `deindexpage` | ⬜ Stub | — |
| 2 | `indexpage` | ⬜ Stub | — |
| 3 | `cleanindex` | ⬜ Stub | — |
| 4 | `mergeresults` | ⬜ Stub | — |

### semaphore

**Status:** 0/2 verbs (0% implemented, 2 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `lock` | ⬜ Stub | — |
| 1 | `unlock` | ⬜ Stub | — |

### speaker

**Status:** 3/3 verbs (100% implemented, 0 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `beep` | ✅ Implemented | langverbs.c:2889 |
| 1 | `sound` | ✅ Implemented | langverbs.c:2860 |
| 2 | `playnamedsound` | ✅ Implemented | langverbs.c:2881 |

### sqlite

**Status:** 2/17 verbs (11% implemented, 15 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `open` | ✅ Implemented | langsqlite.c:128 |
| 1 | `compileQuery` | ⬜ Stub | — |
| 2 | `clearQuery` | ⬜ Stub | — |
| 3 | `resetQuery` | ⬜ Stub | — |
| 4 | `stepQuery` | ⬜ Stub | — |
| 5 | `getColumnCount` | ⬜ Stub | — |
| 6 | `getColumnType` | ⬜ Stub | — |
| 7 | `getColumnInt` | ⬜ Stub | — |
| 8 | `getColumnDouble` | ⬜ Stub | — |
| 9 | `getColumnText` | ⬜ Stub | — |
| 10 | `getColumnName` | ⬜ Stub | — |
| 11 | `getColumn` | ⬜ Stub | — |
| 12 | `getRow` | ⬜ Stub | — |
| 13 | `getErrorMessage` | ⬜ Stub | — |
| 14 | `close` | ✅ Implemented | langsqlite.c:198 |
| 15 | `setColumnBlob` | ⬜ Stub | — |
| 16 | `getLastInsertRowId` | ⬜ Stub | — |

### statusbar 🖥️ GUI

**Status:** 0/5 verbs (0% implemented, 5 stubbed)

*All verbs are GUI-dependent stubs (headless)*

### string

**Status:** 54/60 verbs (90% implemented, 6 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `delete` | ✅ Implemented | stringverbs.c:1393 |
| 1 | `insert` | ✅ Implemented | stringverbs.c:1446 |
| 2 | `popleading` | ✅ Implemented | stringverbs.c:1484 |
| 3 | `poptrailing` | ✅ Implemented | stringverbs.c:1501 |
| 4 | `trimwhitespace` | ✅ Implemented | stringverbs.c:1518 |
| 5 | `popsuffix` | ✅ Implemented | stringverbs.c:1531 |
| 6 | `hassuffix` | ✅ Implemented | stringverbs.c:1551 |
| 7 | `mid` | ✅ Implemented | stringverbs.c:1565 |
| 8 | `nthchar` | ✅ Implemented | stringverbs.c:1604 |
| 9 | `nthfield` | ✅ Implemented | stringverbs.c:1625 |
| 10 | `countfields` | ✅ Implemented | stringverbs.c:1654 |
| 11 | `setwordchar` | ✅ Implemented | stringverbs.c:1377 |
| 12 | `getwordchar` | ✅ Implemented | stringverbs.c:1387 |
| 13 | `firstword` | ✅ Implemented | stringverbs.c:1674 |
| 14 | `lastword` | ✅ Implemented | stringverbs.c:1688 |
| 15 | `nthword` | ✅ Implemented | stringverbs.c:1701 |
| 16 | `countwords` | ✅ Implemented | stringverbs.c:1719 |
| 17 | `commentdelete` | ✅ Implemented | stringverbs.c:1735 |
| 18 | `firstsentence` | ✅ Implemented | stringverbs.c:1738 |
| 19 | `patternmatch` | ✅ Implemented | stringverbs.c:1807 |
| 20 | `hex` | ✅ Implemented | stringverbs.c:1838 |
| 21 | `timestring` | ✅ Implemented | stringverbs.c:1851 |
| 22 | `datestring` | ✅ Implemented | stringverbs.c:1872 |
| 23 | `upper` | ⬜ Stub | — |
| 24 | `lower` | ⬜ Stub | — |
| 25 | `filledstring` | ✅ Implemented | stringverbs.c:1785 |
| 26 | `addcommas` | ✅ Implemented | stringverbs.c:1893 |
| 27 | `replace` | ✅ Implemented | stringverbs.c:1903 |
| 28 | `replaceall` | ✅ Implemented | stringverbs.c:1906 |
| 29 | `length` | ✅ Implemented | stringverbs.c:1909 |
| 30 | `isalpha` | ✅ Implemented | stringverbs.c:1920 |
| 31 | `isnumeric` | ✅ Implemented | stringverbs.c:1933 |
| 32 | `ispunctuation` | ✅ Implemented | stringverbs.c:1946 |
| 33 | `processhtmlmacros` | ⬜ Stub | — |
| 34 | `urldecode` | ✅ Implemented | stringverbs.c:1962 |
| 35 | `urlencode` | ✅ Implemented | stringverbs.c:1965 |
| 36 | `parsehttpargs` | ⬜ Stub | — |
| 37 | `iso8859encode` | ✅ Implemented | stringverbs.c:1971 |
| 38 | `getgifheightwidth` | ✅ Implemented | stringverbs.c:1974 |
| 39 | `getjpegheightwidth` | ✅ Implemented | stringverbs.c:1977 |
| 40 | `wrap` | ✅ Implemented | stringverbs.c:1980 |
| 41 | `davenetmassager` | ✅ Implemented | stringverbs.c:1996 |
| 42 | `parseaddress` | ✅ Implemented | stringverbs.c:2019 |
| 43 | `dropnonalphas` | ✅ Implemented | stringverbs.c:2022 |
| 44 | `padwithzeros` | ✅ Implemented | stringverbs.c:2025 |
| 45 | `ellipsize` | ✅ Implemented | stringverbs.c:2028 |
| 46 | `innercasename` | ⬜ Stub | — |
| 47 | `urlsplit` | ✅ Implemented | stringverbs.c:2063 |
| 48 | `hashMD5` | ⬜ Stub | — |
| 49 | `latintomac` | ✅ Implemented | stringverbs.c:2141 |
| 50 | `mactolatin` | ✅ Implemented | stringverbs.c:2155 |
| 51 | `utf16toansi` | ✅ Implemented | stringverbs.c:2169 |
| 52 | `utf8toansi` | ✅ Implemented | stringverbs.c:2188 |
| 53 | `ansitoutf8` | ✅ Implemented | stringverbs.c:2207 |
| 54 | `ansitoutf16` | ✅ Implemented | stringverbs.c:2226 |
| 55 | `multiplereplaceall` | ✅ Implemented | stringverbs.c:2245 |
| 56 | `macromantoutf8` | ✅ Implemented | stringverbs.c:2250 |
| 57 | `utf8tomacroman` | ✅ Implemented | stringverbs.c:2267 |
| 58 | `convertcharset` | ✅ Implemented | stringverbs.c:2284 |
| 59 | `ischarsetavailable` | ✅ Implemented | stringverbs.c:2312 |

### sys

**Status:** 13/16 verbs (81% implemented, 3 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `osversion` | ✅ Implemented | shellsysverbs.c:391 |
| 1 | `systemtask` | ✅ Implemented | shellsysverbs.c:402 |
| 2 | `browsenetwork` | ✅ Implemented | shellsysverbs.c:420 |
| 3 | `appisrunning` | ✅ Implemented | shellsysverbs.c:427 |
| 4 | `frontmostapp` | ✅ Implemented | shellsysverbs.c:465 |
| 5 | `bringapptofront` | ✅ Implemented | shellsysverbs.c:477 |
| 6 | `countapps` | ✅ Implemented | shellsysverbs.c:490 |
| 7 | `getnthapp` | ✅ Implemented | shellsysverbs.c:496 |
| 8 | `getapppath` | ✅ Implemented | shellsysverbs.c:509 |
| 9 | `memavail` | ✅ Implemented | shellsysverbs.c:524 |
| 10 | `machine` | ✅ Implemented | shellsysverbs.c:537 |
| 11 | `os` | ✅ Implemented | shellsysverbs.c:549 |
| 12 | `getenvironmentvariable` | ⬜ Stub | — |
| 13 | `setenvironmentvariable` | ⬜ Stub | — |
| 14 | `unixshellcommand` | ✅ Implemented | shellsysverbs.c:555 |
| 15 | `winshellcommand` | ⬜ Stub | — |

### table

**Status:** 9/18 verbs (50% implemented, 9 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `move` | ✅ Implemented | tableverbs.c:686 |
| 1 | `copy` | ✅ Implemented | tableverbs.c:689 |
| 2 | `rename` | ✅ Implemented | tableverbs.c:692 |
| 3 | `moveandrename` | ✅ Implemented | tableverbs.c:695 |
| 4 | `assign` | ✅ Implemented | tableverbs.c:703 |
| 5 | `validate` | ✅ Implemented | tableverbs.c:683 |
| 6 | `sortby` | ⬜ Stub | — |
| 7 | `getcursor` | ⬜ Stub | — |
| 8 | `getselection` | ⬜ Stub | — |
| 9 | `go` | ⬜ Stub | — |
| 10 | `goto` | ⬜ Stub | — |
| 11 | `gotoname` | ⬜ Stub | — |
| 12 | `jettison` | ✅ Implemented | tableverbs.c:712 |
| 13 | `packtable` | ✅ Implemented | tableverbs.c:706 |
| 14 | `emptytable` | ✅ Implemented | tableverbs.c:709 |
| 15 | `getdisplaysettings` | ⬜ Stub | — |
| 16 | `setdisplaysettings` | ⬜ Stub | — |
| 17 | `getsortorder` | ⬜ Stub | — |

### target

**Status:** 0/3 verbs (0% implemented, 3 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `get` | ⬜ Stub | — |
| 1 | `set` | ⬜ Stub | — |
| 2 | `clear` | ⬜ Stub | — |

### tcp

**Status:** 0/23 verbs (0% implemented, 23 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `addressdecode` | ⬜ Stub | — |
| 1 | `addressencode` | ⬜ Stub | — |
| 2 | `addresstoname` | ⬜ Stub | — |
| 3 | `nametoaddress` | ⬜ Stub | — |
| 4 | `myaddress` | ⬜ Stub | — |
| 5 | `abortstream` | ⬜ Stub | — |
| 6 | `closestream` | ⬜ Stub | — |
| 7 | `closelisten` | ⬜ Stub | — |
| 8 | `openaddrstream` | ⬜ Stub | — |
| 9 | `opennamestream` | ⬜ Stub | — |
| 10 | `readstream` | ⬜ Stub | — |
| 11 | `writestream` | ⬜ Stub | — |
| 12 | `listenstream` | ⬜ Stub | — |
| 13 | `statusstream` | ⬜ Stub | — |
| 14 | `getpeeraddress` | ⬜ Stub | — |
| 15 | `getpeerport` | ⬜ Stub | — |
| 16 | `writestringtostream` | ⬜ Stub | — |
| 17 | `writefiletostream` | ⬜ Stub | — |
| 18 | `readstreamuntil` | ⬜ Stub | — |
| 19 | `readstreambytes` | ⬜ Stub | — |
| 20 | `readstreamuntilclosed` | ⬜ Stub | — |
| 21 | `getstats` | ⬜ Stub | — |
| 22 | `countconnections` | ⬜ Stub | — |

### thread

**Status:** 0/17 verbs (0% implemented, 17 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `exists` | ⬜ Stub | — |
| 1 | `evaluate` | ⬜ Stub | — |
| 2 | `callscript` | ⬜ Stub | — |
| 3 | `getcurrentid` | ⬜ Stub | — |
| 4 | `getcount` | ⬜ Stub | — |
| 5 | `getnthid` | ⬜ Stub | — |
| 6 | `sleep` | ⬜ Stub | — |
| 7 | `sleepfor` | ⬜ Stub | — |
| 8 | `sleepticks` | ⬜ Stub | — |
| 9 | `issleeping` | ⬜ Stub | — |
| 10 | `wake` | ⬜ Stub | — |
| 11 | `kill` | ⬜ Stub | — |
| 12 | `gettimeslice` | ⬜ Stub | — |
| 13 | `settimeslice` | ⬜ Stub | — |
| 14 | `getdefaulttimeslice` | ⬜ Stub | — |
| 15 | `setdefaulttimeslice` | ⬜ Stub | — |
| 16 | `getstats` | ⬜ Stub | — |

### webserver

**Status:** 0/7 verbs (0% implemented, 7 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `server` | ⬜ Stub | — |
| 1 | `dispatch` | ⬜ Stub | — |
| 2 | `parseheaders` | ⬜ Stub | — |
| 3 | `parsecookies` | ⬜ Stub | — |
| 4 | `buildresponse` | ⬜ Stub | — |
| 5 | `builderrorpage` | ⬜ Stub | — |
| 6 | `getserverstring` | ⬜ Stub | — |

### window 🖥️ GUI

**Status:** 26/31 verbs (83% implemented, 5 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `isopen` | ✅ Implemented | shellwindowverbs.c:1100 |
| 1 | `open` | ✅ Implemented | shellwindowverbs.c:1103 |
| 2 | `isfront` | ✅ Implemented | shellwindowverbs.c:1106 |
| 3 | `bringtofront` | ✅ Implemented | shellwindowverbs.c:1109 |
| 4 | `sendtoback` | ✅ Implemented | shellwindowverbs.c:1112 |
| 5 | `frontmost` | ✅ Implemented | shellwindowverbs.c:1115 |
| 6 | `next` | ✅ Implemented | shellwindowverbs.c:1118 |
| 7 | `isvisible` | ✅ Implemented | shellwindowverbs.c:1121 |
| 8 | `show` | ✅ Implemented | shellwindowverbs.c:1124 |
| 9 | `hide` | ✅ Implemented | shellwindowverbs.c:1127 |
| 10 | `close` | ✅ Implemented | shellwindowverbs.c:1130 |
| 11 | `update` | ✅ Implemented | shellwindowverbs.c:1133 |
| 12 | `ismenuscript` | ✅ Implemented | shellwindowverbs.c:1136 |
| 13 | `getposition` | ⬜ Stub | — |
| 14 | `setposition` | ⬜ Stub | — |
| 15 | `getsize` | ✅ Implemented | shellwindowverbs.c:1145 |
| 16 | `setsize` | ✅ Implemented | shellwindowverbs.c:1148 |
| 17 | `zoom` | ✅ Implemented | shellwindowverbs.c:1151 |
| 18 | `runselection` | ✅ Implemented | shellwindowverbs.c:1154 |
| 19 | `scroll` | ✅ Implemented | shellwindowverbs.c:1167 |
| 20 | `msg` | ✅ Implemented | shellwindowverbs.c:1191 |
| 21 | `dbstats` | ⬜ Stub | — |
| 22 | `quickscript` | ✅ Implemented | shellwindowverbs.c:1228 |
| 23 | `ismodified` | ⬜ Stub | — |
| 24 | `setmodified` | ⬜ Stub | — |
| 25 | `gettitle` | ✅ Implemented | shellwindowverbs.c:1242 |
| 26 | `settitle` | ✅ Implemented | shellwindowverbs.c:1245 |
| 27 | `about` | ✅ Implemented | shellwindowverbs.c:1248 |
| 28 | `getfile` | ✅ Implemented | shellwindowverbs.c:1266 |
| 29 | `isreadonly` | ✅ Implemented | shellwindowverbs.c:1269 |
| 30 | `setquickscript` | ✅ Implemented | shellwindowverbs.c:1272 |

### xml

**Status:** 13/14 verbs (92% implemented, 1 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `addtable` | ✅ Implemented | langxml.c:3578 |
| 1 | `addvalue` | ✅ Implemented | langxml.c:3581 |
| 2 | `compile` | ✅ Implemented | langxml.c:3572 |
| 3 | `decompile` | ✅ Implemented | langxml.c:3575 |
| 4 | `getaddress` | ✅ Implemented | langxml.c:3587 |
| 5 | `getaddresslist` | ✅ Implemented | langxml.c:3590 |
| 6 | `getattribute` | ✅ Implemented | langxml.c:3593 |
| 7 | `getattributevalue` | ✅ Implemented | langxml.c:3596 |
| 8 | `getvalue` | ✅ Implemented | langxml.c:3584 |
| 9 | `valtostring` | ✅ Implemented | langxml.c:3599 |
| 10 | `frontiervaluetotaggedtext` | ⬜ Stub | — |
| 11 | `structtofrontiervalue` | ✅ Implemented | langxml.c:3605 |
| 12 | `getpathaddress` | ✅ Implemented | langxml.c:3608 |
| 13 | `converttodisplayname` | ✅ Implemented | langxml.c:3640 |
