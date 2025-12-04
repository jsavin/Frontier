# Kernel Verb Porting - Executive Summary

**Date:** 2025-12-03
**Document:** Technical Analysis Complete
**Primary Document:** `kernel_verb_porting_technical_guide.md`

---

## Quick Summary

Comprehensive technical analysis completed for porting Frontier kernel verbs from legacy Mac Carbon/UI runtime to headless portable runtime. Analysis focused on two failing verbs but establishes patterns for all future verb migrations.

---

## Key Findings

### 1. Most Code Is Already Portable

**Good News:**
- String path manipulation functions (`filefrompath`, `folderfrompath`, `lastword`, etc.) are **100% portable**
- These functions automatically work with Unix paths when `chpathseparator` is set to `/`
- Can be copied directly from legacy code with zero modification

### 2. Clear Dependency Categories

**UI/Carbon dependencies fall into 5 categories:**

1. **Window/Shell Dependencies** (e.g., `frontier.getFilePath`)
   - **Solution:** Use configuration globals instead of querying window state

2. **Dialog Box Dependencies** (e.g., `dialog.ask`)
   - **Solution:** Stub to stderr or return errors

3. **File System UI Dependencies** (e.g., Mac FSRef/FSSpec APIs)
   - **Solution:** Replace with POSIX `stat()`, string paths

4. **Event Loop Dependencies**
   - **Solution:** Keep threading/yield, stub UI event processing

5. **Resource Fork Dependencies**
   - **Solution:** Return errors (obsolete feature)

### 3. Specific Verb Analysis

#### `frontier.getFilePath` (Token 1)

**Legacy:** Queries current shell window for database path
**Problem:** No window system in headless
**Solution:** Store database path in global at startup, return that

**Implementation:**
```c
static tyfilespec g_headless_database_path;

case filepathfunc:
    return (setfilespecvalue(&g_headless_database_path, v));
```

#### `file.folderFromPath` (Token 110)

**Legacy:** Has two code paths - FSRef (Mac) and string manipulation
**Problem:** FSRef path uses Carbon APIs
**Solution:** Force through string path (already works!)

**Implementation:**
```c
// String functions are 100% portable:
cleanendoffilename(bs);   // Remove trailing /
folderfrompath(bs, bs);   // Extract folder portion
// Works identically with Unix paths!
```

---

## Code Reuse Matrix

| Category | Examples | Portability | Action |
|----------|----------|-------------|--------|
| String manipulation | `lastword()`, `filefrompath()` | 100% | Copy as-is |
| Path helpers | `cleanendoffilename()` | 100% | Copy as-is |
| String path verbs | `file.folderFromPath` string branch | 95% | Minor tweaks |
| Window state verbs | `frontier.getFilePath` | 0% | Use config global |
| Dialog verbs | `dialog.ask` | 0% | Stub to stderr |
| FSRef file ops | `file.getInfo` FSRef branch | 0% | Rewrite with POSIX |

---

## Implementation Patterns

### Pattern 1: Pure String Verb (No Changes Needed)

```c
// Works identically in headless!
case fv_folderFromPath:
    cleanendoffilename(bs);
    folderfrompath(bs, bs);
    return (setstringvalue(bs, v));
```

### Pattern 2: Window-Dependent Verb (Use Config)

```c
// Add global
static tyfilespec g_headless_database_path;

// Initialize at startup
headless_init_database_path(argv[1]);

// Return in verb
return (setfilespecvalue(&g_headless_database_path, v));
```

### Pattern 3: Dialog Verb (Stub to stderr)

```c
case alertdialogfunc:
    fprintf(stderr, "ALERT: %s\n", message);
    return (setbooleanvalue(true, v));
```

---

## Path Separator Magic

The key insight: **All string path functions automatically adapt to Unix paths!**

```c
// file.h
#if defined(FRONTIER_HEADLESS)
    #define chpathseparator '/'  // Unix paths
#else
    #define chpathseparator ':'  // Mac paths
#endif

// This single change makes ALL string functions work with Unix paths:
// - filefrompath()
// - folderfrompath()
// - lastword()
// - cleanendoffilename()
// - etc.
```

**Example:**
```
Mac path:   "Macintosh HD:Users:jake:file.txt"
Unix path:  "/Users/jake/file.txt"

Both work identically with filefrompath() thanks to chpathseparator!
```

---

## Critical Constraints

**ABSOLUTE REQUIREMENT:** No UI or Carbon dependencies in headless binary

**Verification:**
```bash
# Must show NO Carbon/Cocoa symbols
nm -u frontier-cli | grep -i carbon
nm -u frontier-cli | grep -i cocoa
# (should be empty)
```

---

## Implementation Roadmap

### Phase 1: String Path Verbs (EASIEST - 1-2 hours)
- [ ] Add `file.folderFromPath` (copy string branch)
- [ ] Add `file.fileFromPath` (copy string branch)
- [ ] Test with Unix paths

### Phase 2: System Verbs (2-3 hours)
- [ ] Add database path global
- [ ] Implement `frontier.getFilePath` using config
- [ ] Initialize from command-line arg

### Phase 3: File Metadata (4-6 hours)
- [ ] Port `file.exists()` using `stat()`
- [ ] Port `file.size()` using `stat()`
- [ ] Port `file.modified()` using `stat()`

### Phase 4: Dialog Stubs (1-2 hours)
- [ ] Stub alert/ask/notify dialogs
- [ ] Document limitations

---

## Testing Strategy

### Unit Tests
```c
void test_folderFromPath_unixPath(void) {
    bigstring input, result;
    copyctopstring("/home/user/docs/file.txt", input);
    folderfrompath(input, result);
    assert(equalstrings(result, "/home/user/docs/"));
}
```

### Integration Tests
```bash
# Test via CLI
result=$(./frontier-cli -e 'file.folderFromPath("/home/user/file.txt")')
[ "$result" = "/home/user/" ] || exit 1
```

---

## File Locations

**Main Technical Guide:**
`/Users/jake/dev/jsavin/Frontier/planning/phase3/kernel_verb_porting_technical_guide.md`

**Legacy Source (to analyze):**
- `/Users/jake/dev/jsavin/Frontier/Common/source/shellsysverbs.c` - System verbs
- `/Users/jake/dev/jsavin/Frontier/Common/source/fileverbs.c` - File verbs
- `/Users/jake/dev/jsavin/Frontier/Common/source/fileops.m` - Path helpers
- `/Users/jake/dev/jsavin/Frontier/Common/source/strings.c` - String functions

**Headless Targets (to implement):**
- `/Users/jake/dev/jsavin/Frontier/tests/headless_file_verbs.c` - Extend this
- `/Users/jake/dev/jsavin/Frontier/tests/headless_sys_verbs.c` - Create this

---

## Portable Functions Reference

**Already Portable (copy as-is):**
```c
boolean filefrompath(bigstring path, bigstring fname);
boolean folderfrompath(bigstring path, bigstring folder);
boolean cleanendoffilename(bigstring bs);
boolean endswithpathsep(bigstring bs);
boolean lastword(bigstring source, byte delim, bigstring dest);
boolean firstword(bigstring source, byte delim, bigstring dest);
```

**Need POSIX Replacement:**
```c
// Instead of: macgetfilespecparent()
// Use: folderfrompath() on string path

// Instead of: FSGetCatalogInfo()
// Use: stat() on Unix path
```

---

## Common Pitfalls to Avoid

1. **Don't mix Mac and Unix paths** - standardize on Unix absolute paths
2. **Don't call shell globals** - they're NULL/undefined in headless
3. **Don't assume dialogs work** - stub or error
4. **Don't use FSRef** - use string paths and POSIX APIs
5. **Don't skip parameter validation** - headless still needs error checking

---

## Success Metrics

**Definition of Done:**
- ✅ `frontier.getFilePath()` returns correct database path
- ✅ `file.folderFromPath()` works with Unix absolute paths
- ✅ `file.fileFromPath()` works with Unix absolute paths
- ✅ Unit tests pass for all ported verbs
- ✅ Integration tests via CLI pass
- ✅ No Carbon/Cocoa symbols in binary (`nm -u` clean)
- ✅ Scripts using these verbs execute successfully

---

## Next Actions

**For Developer:**
1. Read full technical guide: `kernel_verb_porting_technical_guide.md`
2. Start with Phase 1 (string path verbs) - easiest wins
3. Follow implementation patterns from guide
4. Write tests as you go
5. Verify no UI dependencies

**For System Architect:**
- Use findings to plan overall verb migration strategy
- Prioritize verbs based on actual script usage
- Consider auto-generating stub registry
- Plan verb coverage expansion roadmap

---

## Conclusion

**The core insight:** Frontier's string-based path manipulation is already portable! The majority of file path operations "just work" when you:

1. Use Unix paths (`/` separator)
2. Avoid FSRef/Carbon APIs
3. Replace window queries with configuration

**Estimated effort to fix failing verbs:**
- `file.folderFromPath`: **30 minutes** (just use string branch)
- `frontier.getFilePath`: **1 hour** (add config global)

**Total:** ~2 hours of focused work to unblock startup scripts.

---

**See full technical details in:** `kernel_verb_porting_technical_guide.md`
