# Verb Resolution Architecture

**LIVING DOCUMENT**: Update as we learn new implementation details.

## Overview

Frontier resolves verbs (function names) through multiple mechanisms depending on the expression type.

## Resolution Paths

### Path 1: Bare Identifiers (e.g., `string(789)`)

**Call Chain**:
1. `langgetdotparams()` - Checks if identifier has dots
2. Returns nil table for bare identifiers
3. `langgethandlercode()` - Sees nil table
4. `langfindsymbol()` - Searches local scope chain
5. **INVESTIGATE**: Path search for bare identifiers may not be implemented

**Callbacks Used**: None (direct scope chain walking)

**Current Status**: ⚠️ Path searching for bare identifiers appears broken (Issue #344)

### Path 2: Dotted Paths (e.g., `string(webserver.init)`)

**Call Chain**:
1. `langgetdotparams()` - Detects dots in identifier
2. `langsearchpathvisit()` - Iterates through `system.paths` tables
3. For each path, calls `langdirecttablelookup()` callback
4. Callback returns parent table if name found
5. Caller looks up name in parent table to get actual value

**Callbacks Used**:
- `langdirecttablelookup()` - MUST return PARENT table + name (fixed 2026-01-25)
- Rejects non-table intermediate values for multi-component paths

**Current Status**: ✅ Fixed in commit [TBD] (PR #344)

### Path 3: Builtin/EFP Resolution

**Call Chain**:
1. `langexternalgettable()` - Checks local scope and legacy paths
2. `langhandlercall()` - For verb dispatch, checks efptable AFTER system.paths

**Fixed (PR #352)**: The explicit EFP table search was removed from `langexternalgettable()`.
Previously, EFP tables were searched BEFORE `system.paths`, causing introspection bugs:
- `parentOf(string.mid)` returned `system.compiler.["kernel"].string` instead of `system.verbs.builtins.string`
- `typeOf(op.outlineToXml)` returned `tokn` instead of `scpt`

Verb dispatch still works because `langhandlercall()` has its own search order that checks
efptable AFTER system.paths (see langvalue.c lines ~8737-8760).

**Fixed (PR #469)**: A second instance of the same anti-pattern was found in `langgethandlercode()`.
A headless EFP fast-path (added Oct 2025 as a "temporary shim") checked efptable BEFORE
system.paths for dotted verbs. When a verb existed as a UserTalk script in
`system.verbs.builtins.<efp>.<verb>` but NOT as a kernel verb, the fast-path returned success
with `hnode=nil`, blocking the database fallback. This broke `inetd.startOne` and other UserTalk
scripts under EFP-named tables. The fast-path was removed entirely since database hydration now
loads system tables at startup.

## Function Reference

### `langdirecttablelookup(htable, bsname, *hresult)`
- **Purpose**: Callback for dotted path resolution
- **MUST Return**: Parent table (htable parameter) when name found
- **Used For**: Multi-component dotted paths
- **NOT Used For**: Bare identifiers
- **File**: `Common/source/langvalue.c:3759-3795`

### `langtablelookup(intable, bsname, *htable)`
- **Purpose**: Callback for `system.paths` search
- **MUST Return**: Parent table (intable) when name found
- **Used For**: **INVESTIGATE** - when is this actually called?
- **File**: `Common/source/langvalue.c:4145-4166`

### `langfindsymbol(bs, *htable, *hnode)`
- **Purpose**: Search local scope chain
- **Does NOT**: Search `system.paths`
- **Used For**: Local variables, with statements, etc.
- **File**: `Common/source/langops.c:339-470`

### `langsearchpathvisit(visit, bsname, *htable)`
- **Purpose**: Iterate through `system.paths` and call callback for each
- **Used For**: Dotted path resolution
- **Current Issue**: May not be called for bare identifiers
- **File**: `Common/source/langvalue.c:3798-3831`

## Common Gotchas

1. **Different callbacks for different paths** - Don't assume all resolution uses same callback
2. **Parent table vs child value** - Callbacks return PARENT, caller extracts child
3. **Bare vs dotted** - Completely different code paths
4. **EFP vs system.paths** - Priority matters for builtin vs user verbs

## Known Issues

- ⚠️ **Bare identifier path search not implemented** (Issue #344)
- ⚠️ `langtablelookup` may not be called in current code
- ✅ `langdirecttablelookup` fixed to return parent table (2026-01-25, PR #344)
- ✅ EFP introspection bugs fixed (2026-01-26, PR #352) - `parentOf()` and `typeOf()` now return correct database paths

## Related Files

- `Common/source/langvalue.c` - Main resolution logic
- `Common/source/langops.c` - `langfindsymbol` implementation
- `Common/source/langexternal.c` - External/builtin resolution
- `planning/phase4/verb-resolution/` - Investigation reports

## Update History

- 2026-01-26: Updated Path 3 (EFP Resolution) to reflect PR #352 fix
- 2026-01-25: Initial version created during Issue #344 investigation
