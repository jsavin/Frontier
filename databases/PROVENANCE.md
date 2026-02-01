# Distribution Files Provenance

## Source

All files in this directory were copied from the official **Frontier 9.5** distribution:

- **Source**: `Frontier 9.5.dmg` disk image (UserLand Software, June 2005)
- **Mount path**: `/Volumes/Frontier 9.5/Frontier/`

## Database Files (.root)

The v6 database files are unmodified copies from the Frontier 9.5 distribution. MD5 checksums verified against the original disk image.

| File | MD5 |
|------|-----|
| Frontier.root | b993ad8fbe5dd7c1decd88adb08a1d66 |
| Guest Databases/apps/mainResponder.root | e14676c7aba69a950b8c3277a6afb1be |
| Guest Databases/apps/manila.root | b473eb5f9e82409c158bec228eb1ffc0 |
| Guest Databases/apps/Tools/radioCommunityServer.root | 24dd5b1e02ad5d1d8d125967c023f91f |
| Guest Databases/apps/Tools/serverMonitor.root | 9b41362c4dab1cce5974dcf42732e118 |
| Guest Databases/apps/Tools/TheXmlFiles.root | 199d47f168db2448fcf6cfaf49881a01 |
| Guest Databases/www/prefs.root | 8813305e5d4891f75811d866f517d312 |

## Static Files

- **License.rtf** - Original UserLand license
- **Manila User's Guide.pdf** - Original documentation
- **frontierStartupCommands.txt** - Default startup configuration

## Binary Files

These are historical artifacts preserved for compatibility:

- **DLLs/** - Windows DLL files (odbc.dll, regexcarbon.dll)
- **Extras/Altivec Support/** - PowerPC Altivec-optimized binary
- **Extras/keepFrontierRunning/** - Platform-specific launcher utilities
- **Extras/Shell/** - Shell integration scripts
- **Extras/ODBC/** - ODBC extension

## Icon Files (Appearance/Icons/)

These files use classic Mac OS resource forks to store icon data. Git shows them as 0 bytes because the data is in extended attributes (not the data fork). They are GUI assets for Manila outline node types and are not used by the headless CLI.

## Why Not Git LFS?

These files are static historical artifacts that will never change. Git LFS adds operational complexity (requires LFS setup, different clone behavior) for minimal benefit. The one-time ~30MB repository size increase is acceptable for build reproducibility without external dependencies.
