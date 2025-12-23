# Complete #ifdef Inventory

Analysis Date: 2025-12-20

## Summary

Total unique ifdef patterns: **116**  
Total ifdef blocks in codebase: **~350+**

---

## Category 1: Debug/Diagnostic Logging (76 blocks)

**Purpose**: Conditional debug output and diagnostics

| Pattern | Count | Notes |
|---------|-------|-------|
| `fldebug` | 55 | Core debug logging throughout |
| `DATABASE_DEBUG` | 10 | Database-specific debug |
| `DEBUG_SERIALIZER` | 9 | Serialization debugging |
| `PARSER_TRACE` | 2 | Parser tracing |
| `TRACE_INTERMEDIATE_VALUES` | 5 | Intermediate value tracing |
| `PURIFY` | 1 | Purify profiler integration |
| `flprofile` | 4 | Profiling instrumentation |

**Total**: ~76 blocks  
**Status**: Redundant with fprintf output; best handled by logging infrastructure redesign

---

## Category 2: Headless/GUI Mode Control (47 blocks)

**Purpose**: Code that only runs in headless mode or GUI mode

| Pattern | Count | Notes |
|---------|-------|-------|
| `FRONTIER_HEADLESS` | 47 | Headless-specific code |

**Status**: Essential for headless-only runtime; likely to stay

---

## Category 3: External Library Integration (44 blocks)

**Purpose**: Feature flags for optional/external systems

| Pattern | Count | Notes |
|---------|-------|-------|
| `PIKE` | 29 | Pike scripting integration |
| `FRONTIER_MYSQL` | 3 | MySQL support |
| `FRONTIER_PYTHON` | 2 | Python support |
| `FRONTIER_SQLITE` | 2 | SQLite support (appears 3x total) |
| `FRONTIER_GUSI_2` | 6 | GUSI 2 networking |
| `appletinclude` | 5 | AppleScript/OSA support |
| `claydialoginclude` | 5 | Dialog system |
| `landinclude` | 7 | Language system |
| `iowaRuntime` | 1 | Iowa runtime |

**Total**: ~60 blocks  
**Status**: Feature toggles; could be compile-time only, runtime control, or removed

---

## Category 4: Platform/Version Targeting (40 blocks)

**Purpose**: Platform-specific or version-specific code

| Pattern | Count | Notes |
|---------|-------|-------|
| `xxxWIN95VERSION` | 11 | Windows 95 (obsolete) |
| `WIN95VERSION` | 3 | Windows 95 variant |
| `WINDOWS_PLATFORM` | 2 | Generic Windows |
| `MACVERSION` | 2 | Mac version check |
| `oldMACVERSION` | 2 | Old Mac version |
| `__MACH__` | 1 | Mach kernel check |
| `__LITTLE_ENDIAN__` / `__BIG_ENDIAN__` | 2 | Byte order |
| `SWAP_BYTE_ORDER` | 6 | Byte swapping |
| `SWAP_REZ_BYTE_ORDER` | 1 | Resource byte order |
| `L_ENDIAN` | 1 | Little-endian |
| `__MWERKS__` | 1 | Metrowerks compiler |
| `FRONTIER_PORTABLE` | 7 | Portable mode |
| `FRONTIER_USE_PORTABLE_HANDLES` | 1 | Portable handles |

**Total**: ~40 blocks  
**Status**: Many obsolete (Windows 95!); byte order flags critical for v6→v7 migration

---

## Category 5: UI/Display Appearance (15 blocks)

**Purpose**: GUI appearance and behavior

| Pattern | Count | Notes |
|---------|-------|-------|
| `gray3Dlook` | 15 | 3D gray appearance |

**Total**: 15 blocks  
**Status**: GUI-only; dead code for headless

---

## Category 6: Compilation Control (11 blocks)

**Purpose**: Build-time control flags

| Pattern | Count | Notes |
|---------|-------|-------|
| `compileall` | 11 | Compile all vs. selective |

**Total**: 11 blocks  
**Status**: Build system control; context-dependent

---

## Category 7: XML Support (10 blocks)

**Purpose**: XML feature support

| Pattern | Count | Notes |
|---------|-------|-------|
| `xmlfeature` | 10 | XML features |

**Total**: 10 blocks  
**Status**: Optional feature; used by some modules

---

## Category 8: Threading/Networking (14 blocks)

**Purpose**: Threading and network operation modes

| Pattern | Count | Notes |
|---------|-------|-------|
| `ACCEPT_CONN_WITHOUT_GLOBALS` | 8 | Connection handling |
| `ACCEPT_IN_SEPARATE_THREAD` | 6 | Threading model |

**Total**: 14 blocks  
**Status**: Runtime behavior; may be compile-time or feature-gated

---

## Category 9: Interface/DLL Control (12 blocks)

**Purpose**: DLL and interface variants

| Pattern | Count | Notes |
|---------|-------|-------|
| `NEW_DLL_INTERFACE` | 6 | DLL interface version |
| `SUPPORT_NAMED_PARAMS_IN_FRONTIER_COM` | 2 | COM support |
| `YYPARSE_PARAM` / `YYLEX_PARAM` / `YYERROR_VERBOSE` | 4 | Yacc/Lex parser control |

**Total**: 12 blocks  
**Status**: Build-time interface control; not runtime

---

## Category 10: Database/Smart Opening (7 blocks)

**Purpose**: Database optimization and control

| Pattern | Count | Notes |
|---------|-------|-------|
| `SMART_DB_OPENING` | 7 | Smart database opening |

**Total**: 7 blocks  
**Status**: Performance optimization; unclear if actively used

---

## Category 11: Obscure/Single-Use Flags (50+ blocks)

**Purpose**: Various one-off or unclear purposes

| Pattern | Count | Notes |
|---------|-------|-------|
| `xxxPIKE`, `PIKExxx` | 3 | Pike variants (typos?) |
| `newparam`, `new_ba`, `NeverDefine_For_Reference` | 3 | Parameter variants |
| `OBSOLETE`, `NEVER` | 2 | Explicit dead markers |
| `flwpcolor`, `flbuttoncolor` | 2 | UI color flags |
| `tablehorizline`, `openrecentmenu` | 2 | Feature toggles |
| And 40+ more single-occurrence flags | 1 each | Typos, experiments, legacy |

**Status**: Need investigation; many appear to be typos or abandoned experiments

---

## Distribution by File

Files with most ifdefs:
```
20+ blocks: lang.c, db.c, oppack_v7.c
10+ blocks: tablepack.c, langvalue.c, opops.c, langexternal.c
5-10 blocks: 15+ other files
```

---

## Distribution by Type

**By Category Count**:
1. Debug/Diagnostic: 76 (21%)
2. Feature Integration: 60 (17%)
3. Platform/Version: 40 (11%)
4. Headless Mode: 47 (13%)
5. Other: 127 (36%)

**Total mapped**: 350 blocks

---

## Files with Complete ifdef Inventory

Run this to see all ifdefs in a specific file:
```bash
rg "^\s*#ifdef" Common/source/lang.c
```

---

## Next Steps (Sonnet Analysis)

### Questions for Sonnet:
1. **Which ifdefs can be removed entirely?** (e.g., Windows 95, obsolete platforms)
2. **Which should become compile-time?** (e.g., PIKE, FRONTIER_SQLITE)
3. **Which should become runtime?** (e.g., fldebug → logging framework)
4. **Which are critical?** (e.g., SWAP_BYTE_ORDER for migration)
5. **Architecture**: Should we introduce a feature flag system, or just clean up case-by-case?

### High-Priority Analysis Needed:
- Platform-specific code (11 xxxWIN95VERSION blocks - why still there?)
- Typos and single-use flags (50+ unclear purpose)
- Database optimization flags (SMART_DB_OPENING)
- Parser/Yacc directives (generated code?)

