# Filespec & Alias Manager Call-Site Map
**Date:** October 31, 2025  
**Owner:** Codex  

Tracks every reference to the classic Alias Manager bridge (`tyfilespec`, `filespectoalias`, `aliastofilespec`, `langpackfileval`). Source derived from `rg "filespectoalias|aliastofilespec|langpackfileval"`.

## Summary
- **Total references:** 26 (code + headers + stubs).  
- **Core runtime impact:** `langhash.c` and `langpack.c` rely on alias serialization to compare file values and packages.  
- **Desktop-only:** `langsystem7.c` implements the Mac alias glue; most other platforms fall back to stubs.  
- **Headless gap:** No portable replacement for alias/bookmark serialization; must design a new path abstraction.

## Occurrence Table
| Symbol | Path | Count | Area | Notes | Status |
| --- | --- | --- | --- | --- | --- |
| `filespectoalias` | `Common/source/langsystypes.c` | 4 | Legacy bridge | Wrapper used by language verbs to package file values. | Replace with portable bookmark serializer |
| `filespectoalias` | `Common/source/langsystem7.c` | 4 | Classic Mac | Actual Alias Manager implementation. | Retire after portable path abstraction |
| `filespectoalias` | `Common/source/langhash.c` | 1 | Core runtime | Hash comparisons for file values. | Needs new portable helper |
| `filespectoalias` | `Common/headers/langsystem7.h` | 1 | Header | Prototype exposed to language layer. | Update when new API defined |
| `filespectoalias` | `portable/runtime_stubs_system.c` | 1 | Headless stub | Returns nil; temporary. | Remove after replacement |
| `filespectoalias` | `portable/runtime_stubs_lang.c` | 1 | Headless stub | Same stub entry. | Remove after replacement |
| `aliastofilespec` | `Common/source/langsystypes.c` | 3 | Legacy bridge | Used when unpacking alias handles. | Replace with path parser |
| `aliastofilespec` | `Common/source/langsystem7.c` | 4 | Classic Mac | Alias Manager implementation. | Retire |
| `aliastofilespec` | `Common/source/langhash.c` | 2 | Core runtime | File value coercion. | Needs new helper |
| `aliastofilespec` | `Common/source/langvalue.c` | 1 | Core runtime | Coerces alias values during evaluation. | Needs replacement |
| `aliastofilespec` | `portable/runtime_stubs.c` | 1 | Headless stub | Returns `NULL`. | Remove |
| `aliastofilespec` | `portable/runtime_stubs_system.c` | 1 | Headless stub | Same stub. | Remove |
| `aliastofilespec` | `portable/runtime_stubs.h` | 1 | Header | Stub declaration. | Replace |
| `aliastofilespec` | `Common/headers/langsystem7.h` | 1 | Header | Prototype. | Update |
| `langpackfileval` | `Common/source/langsystypes.c` | 1 | Legacy bridge | Packages file values for storage. | Redesign using new abstraction |
| `langpackfileval` | `Common/source/langpack.c` | 1 | Core runtime | Persists file values; depends on alias handles. | Needs new serializer |
| `langpackfileval` | `Common/headers/langsystem7.h` | 1 | Header | Prototype. | Update |

## Notes
- `tyfilespec` struct definitions live in `langsystem7.h`; replacing with a POSIX path struct will cascade through `langhash.c`, `langsystypes.c`, and file verbs.
- The Windows port already stores plain path strings (`shell.win.h`, `langwinipc.c`) — treat that behaviour as the baseline for the new abstraction.
- Headless currently stubs every API here, leading to runtime failures when code expects real alias resolution. Replacement work must precede headless feature parity.
