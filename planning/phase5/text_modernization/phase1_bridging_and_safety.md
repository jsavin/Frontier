# Phase 1: Bridging & Safety

**Status**: Partially Complete
**Risk**: Low
**Breakage**: None
**Depends On**: Nothing
**Can Parallel With**: Phase 3 (Hashtable Modernization)

---

## Goal

Establish a safe, consistent bridge between Pascal strings and C strings. Stop the bleeding: no new Pascal string usage in modern code, and eliminate unsafe casts in existing code. This phase involves zero behavioral changes — it's pure safety and hygiene.

---

## Completed Work

- `bs_from_c(const char *cstr, bigstring out)` — C string to Pascal string conversion
- `c_from_bs(const bigstring in, char *out, unsigned long out_sz)` — Pascal string to C string conversion (buffer-safe)
- Both defined in `Common/source/strings_extras.c`, declared in `Common/headers/strings.h`
- Used in init code (constants/keywords/builtins) and table operations

---

## Remaining Work

### 1. Sweep Unsafe `(ptrstring)"literal"` Casts

**Problem**: Code casts C string literals directly to `ptrstring`, bypassing the length-byte requirement. These are latent bugs — they produce invalid Pascal strings where the first byte of the ASCII text is misinterpreted as a length byte.

**Action**: Find all `(ptrstring)"..."` casts and replace with either:
- `BIGSTRING("\xNN""text")` for compile-time Pascal string literals (where `\xNN` is the hex length)
- `bs_from_c("text", bsTemp)` for runtime conversion into a local bigstring

**Search pattern**: `grep -rn "(ptrstring)\"" Common/source/ tests/`

### 2. Enforce C Strings in New Code

**Policy** (to document and enforce in code review):
- All new functions should accept `const char *` + length, not `bigstring`
- Pascal strings are permitted only at legacy API boundaries
- New string literals should be plain C strings, converted at the call site when interfacing with legacy code

### 3. Audit `BIGSTRING()` Macro Usage

The `BIGSTRING("\xNN""text")` pattern is fragile — the hex length must be manually maintained. Catalog all usages and verify lengths are correct. Consider a build-time validation tool that checks `\xNN` matches the actual string length.

**Current scale**: ~6,550 BIGSTRING usages across 348 files.

---

## Key Files

| File | Role |
|------|------|
| `Common/headers/strings.h` | String API declarations, `bs_from_c` / `c_from_bs` prototypes |
| `Common/source/strings.c` | Core Pascal string operations |
| `Common/source/strings_extras.c` | Modern bridging helpers |
| `Common/SystemHeaders/standard.h` | `bigstring` typedef, `BIGSTRING` macro, helper macros |

---

## Verification

- Full test suite passes: `./tools/run_headless_tests.sh`
- Integration tests pass: `cd tests && make test-integration`
- `grep -rn "(ptrstring)\"" Common/source/` returns zero results after sweep
- No new `bigstring` parameters in any newly-added function signatures (code review check)

---

## Risks

**Low**. This phase is mechanical and additive. No behavioral changes, no format changes, no API signature changes. Each cast replacement can be verified individually.

The main risk is the BIGSTRING length-byte audit — an incorrect `\xNN` value silently produces a truncated or over-read string. A build-time validator would eliminate this class of bug permanently.
