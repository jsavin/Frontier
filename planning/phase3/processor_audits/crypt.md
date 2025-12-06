# Processor Audit: `crypt`

**Status:** ✅ Ready for Implementation (Core Already Exists!)
**Audit Date:** 2025-12-05
**Auditor:** Claude (Sonnet 4.5)

---

## Basic Information

| Property | Value |
|----------|-------|
| **Processor Name** | `crypt` |
| **EFP ID** | Unknown (lang block) |
| **Verb Count** | 5 |
| **Window Required** | NO |
| **Documentation** | None found (legacy processor) |
| **Stub Implementation** | [headless_crypt_verbs.c](../../../tests/headless_crypt_verbs.c) |
| **Existing Implementation** | [langcrypt.c](../../../Common/source/langcrypt.c) ✅ |

---

## Category Assessment

**Category:** ✅ **Core Functionality**

**Rationale:**
Cryptographic hashing and HMAC operations are essential security utilities with no GUI dependencies. Provides MD5, SHA1, and Whirlpool hashing plus HMAC variants. All operations use pure algorithms with no OS dependencies beyond basic memory operations.

**Headless Compatibility:** ✅ **Full**

**Blocking Verbs:** None

---

## Verb Inventory

| # | Verb Name | Signature | Description |
|---|-----------|-----------|-------------|
| 1 | `MD5` | `crypt.MD5(data, flTranslate=true) -> string/binary` | MD5 hash (128-bit) |
| 2 | `SHA1` | `crypt.SHA1(data, flTranslate=true) -> string/binary` | SHA1 hash (160-bit) |
| 3 | `whirlpool` | `crypt.whirlpool(data, flTranslate=true) -> string/binary` | Whirlpool hash (512-bit) |
| 4 | `hmacMD5` | `crypt.hmacMD5(data, key, flTranslate=true) -> string/binary` | HMAC-MD5 |
| 5 | `hmacSHA1` | `crypt.hmacSHA1(data, key, flTranslate=true) -> string/binary` | HMAC-SHA1 |

**Parameters:**
- `data` - String or binary data to hash
- `key` - Secret key for HMAC operations
- `flTranslate` - Optional boolean (default=true):
  - `true` → Return hex-encoded string (e.g., "5d41402abc4b2a76b9719d911017c592")
  - `false` → Return raw binary digest

---

## Implementation Analysis

### Complexity: **VERY LOW** ⭐

### Dependencies

- **Other Processors:** None
- **External Services:** None
- **OS-Specific Functionality:** NO (pure algorithm implementations)
- **GUI/Window Context:** NO
- **Existing Code:** ✅ **Complete implementation already exists in langcrypt.c**

### Key Implementation Notes

**Existing Implementations (langcrypt.c):**
```c
// MD5 - lines 194-251
case md5func:
    MD5Init(&hashcontext);
    MD5Update(&hashcontext, data, size);
    MD5Final(checksum, &hashcontext);
    // Convert to hex string or return binary

// SHA1 - lines 253-310
case sha1func:
    SHA1_Init(&hashcontext);
    SHA1_Update(&hashcontext, data, size);
    SHA1_Final(digest, &hashcontext);
    // Convert to hex string or return binary

// Whirlpool - lines 76-132
case whirlpoolfunc:
    NESSIEinit(&w);
    NESSIEadd(data, 8 * size, &w);
    NESSIEfinalize(&w, digest);
    // Convert to hex string or return binary

// HMAC-MD5 - lines 134-192
case hmacmd5func:
    hmacmd5(data, datasize, key, keysize, digest);
    // Convert to hex string or return binary

// HMAC-SHA1 - lines 312-370
case hmacsha1func:
    hmacsha1(data, datasize, key, keysize, digest);
    // Convert to hex string or return binary
```

**Supporting Files Already in Codebase:**
- `Common/source/md5.c` - MD5 implementation (OpenSSL-derived)
- `Common/source/sha1dgst.c` - SHA1 implementation (OpenSSL-derived)
- `Common/source/whirlpool.c` - Whirlpool implementation (NESSIE)
- `Common/headers/md5.h` - MD5 interface
- `Common/headers/sha.h` - SHA interface

**Implementation Task:**
The core crypto code already exists! We only need to:
1. Expose the existing `cryptfunctionvalue()` function via kernel verb system
2. Update function signatures to match new verb registration pattern
3. Wire up verb tokens to existing switch cases
4. Test that it works in headless mode

**Hash Output Formats:**
```c
// flTranslate=true (default) - Hex string
MD5:       "5d41402abc4b2a76b9719d911017c592" (32 hex chars)
SHA1:      "aaf4c61ddcc5e8a2dabede0f3b482cd9aea9434d" (40 hex chars)
Whirlpool: "19fa...0af7" (128 hex chars)

// flTranslate=false - Binary digest
MD5:       16 bytes
SHA1:      20 bytes
Whirlpool: 64 bytes
HMAC-MD5:  16 bytes
HMAC-SHA1: 20 bytes
```

**Hash Algorithm Details:**

**MD5 (Message Digest 5):**
- Output: 128-bit (16 bytes)
- Status: **BROKEN** - Not secure for new applications (collision attacks exist)
- Use case: Legacy systems, non-security checksums
- Speed: Very fast

**SHA1 (Secure Hash Algorithm 1):**
- Output: 160-bit (20 bytes)
- Status: **DEPRECATED** - Weaknesses found, not recommended for new security applications
- Use case: Legacy systems, Git commits (non-adversarial)
- Speed: Fast

**Whirlpool:**
- Output: 512-bit (64 bytes)
- Status: **SECURE** - No known practical attacks
- Use case: High-security hashing, passwords
- Speed: Slower than MD5/SHA1

**HMAC (Hash-based Message Authentication Code):**
- Provides authentication + integrity
- Uses underlying hash (MD5 or SHA1)
- Secure even if hash has collision weaknesses (for HMAC purpose)
- Use case: API authentication, message verification

**Security Considerations:**
- **MD5**: DO NOT use for password hashing, digital signatures, or certificate validation
- **SHA1**: Deprecated for security-critical uses (SSL/TLS, code signing)
- **Whirlpool**: Safe for all use cases
- **HMAC**: Safe when used with adequate key length (≥128 bits)

**Modern Alternatives (Not in Frontier):**
- SHA-256, SHA-512 (SHA-2 family) - Recommended for new applications
- SHA-3 - Latest standard
- BLAKE2 - Modern, fast, secure
- Argon2 - For password hashing

**Type Coercion:**
- Input: String or binary handle
- Output: String (hex) or binary handle
- flTranslate parameter controls format

**Edge Cases:**
- Empty input → Valid hash (hash of zero bytes)
- Very large inputs → Streaming via Update() calls
- Invalid hex conversion → Should not occur (fixed alphabet)
- HMAC with empty key → Valid but not secure

---

## UserTalk Documentation Notes

No official documentation found, but inferred from implementation:

**crypt.MD5(data [, flTranslate])**
- Compute MD5 hash of data
- Returns 32-character hex string (default) or 16-byte binary
- Example: `crypt.MD5("hello")` → "5d41402abc4b2a76b9719d911017c592"

**crypt.SHA1(data [, flTranslate])**
- Compute SHA1 hash of data
- Returns 40-character hex string (default) or 20-byte binary
- Example: `crypt.SHA1("hello")` → "aaf4c61ddcc5e8a2dabede0f3b482cd9aea9434d"

**crypt.whirlpool(data [, flTranslate])**
- Compute Whirlpool hash of data
- Returns 128-character hex string (default) or 64-byte binary
- More secure than MD5/SHA1

**crypt.hmacMD5(data, key [, flTranslate])**
- Compute HMAC-MD5 for message authentication
- Requires secret key
- Returns 32-character hex string (default) or 16-byte binary

**crypt.hmacSHA1(data, key [, flTranslate])**
- Compute HMAC-SHA1 for message authentication
- Requires secret key
- Returns 40-character hex string (default) or 20-byte binary

**Common Use Cases:**
```usertalk
// File integrity verification
local (hash = crypt.SHA1(file.readWholeFile("data.txt")))

// API request signing
local (signature = crypt.hmacSHA1(requestData, apiSecret))

// Password verification (NOT RECOMMENDED - use Argon2/bcrypt)
local (storedHash = crypt.whirlpool(password + salt))

// Content fingerprinting
local (etag = crypt.MD5(httpResponse))
```

---

## Testing Requirements

**Minimum Test Cases Per Verb:**
- Known test vectors (standard hash inputs/outputs)
- Empty input
- Large input (>1MB)
- Binary data (non-text)
- flTranslate=true vs false

**Test Scenarios:**
```usertalk
// crypt.MD5() - Known test vectors
crypt.MD5("")                         → "d41d8cd98f00b204e9800998ecf8427e"
crypt.MD5("hello")                    → "5d41402abc4b2a76b9719d911017c592"
crypt.MD5("The quick brown fox...")   → "9e107d9d372bb6826bd81d3542a419d6"

// crypt.SHA1() - Known test vectors
crypt.SHA1("")                        → "da39a3ee5e6b4b0d3255bfef95601890afd80709"
crypt.SHA1("hello")                   → "aaf4c61ddcc5e8a2dabede0f3b482cd9aea9434d"

// crypt.whirlpool() - Known test vector
crypt.whirlpool("")                   → "19fa61d75522a466...af7c6cfe6c3fd4e"

// crypt.hmacMD5() - With known key
crypt.hmacMD5("message", "key")       → (verify against known HMAC-MD5)

// crypt.hmacSHA1() - With known key
crypt.hmacSHA1("message", "key")      → (verify against known HMAC-SHA1)

// Binary output
local (bin = crypt.MD5("test", false))
sizeof(bin)                           → 16 bytes

// Large input
local (big = string.replicate("x", 1000000))
crypt.SHA1(big)                       → (should not crash, returns hash)
```

**Test Vectors:**
Use standard test vectors from:
- RFC 1321 (MD5)
- RFC 3174 (SHA-1)
- NESSIE test vectors (Whirlpool)
- RFC 2104 (HMAC)

**Edge Case Tests:**
- Empty string hash
- Single character hash
- Unicode/UTF-8 data
- Binary data with null bytes
- Very large data (memory handling)
- HMAC with empty key
- HMAC with very long key

**Platform Differences:**
- None expected (pure algorithm implementations)
- Byte order handled correctly
- Should produce identical results on all platforms

---

## Implementation Effort

**Estimated Time:** 2-3 hours ⚡

**Breakdown:**
- Code refactoring: 1-1.5 hours (expose existing code via new verb system)
- Testing: 0.5-1 hour (verify against test vectors)
- Documentation: 0.5 hours

**Confidence:** VERY HIGH - Core code already exists and works!

**Work Required:**
1. Copy/adapt `cryptfunctionvalue()` from langcrypt.c
2. Update verb registration to use kernel verb system
3. Ensure headless compatibility (no GUI dependencies)
4. Add test vectors to verify correctness
5. Document security considerations

---

## Priority & Sequencing

**Priority:** 🏆 **QUICK WIN** (Tier 1)

**Recommended Implementation Order:** 14 (after date)

**Blockers/Prerequisites:** None

**Implementation Sequence:**
1. Review existing langcrypt.c implementation
2. Create thin wrapper for kernel verb system
3. Wire up 5 verb tokens to existing switch cases
4. Test with known MD5/SHA1 test vectors
5. Verify HMAC implementations
6. Test binary vs hex output modes
7. Document security status of each algorithm

---

## Quick Win Justification

**Why This Is an EXTREME Quick Win:**
1. **Code Already Exists:** 100% of crypto logic implemented in langcrypt.c!
2. **Well-Tested Algorithms:** Using established OpenSSL-derived implementations
3. **No Dependencies:** Pure algorithm code, no OS/GUI dependencies
4. **High Value:** Essential for API authentication, data integrity
5. **Simple Integration:** Just wire existing code to new verb system
6. **Easy Testing:** Standard test vectors available

**Value Proposition:**
- Required for HMAC-based API authentication
- Enables data integrity verification (checksums)
- Supports legacy systems using MD5/SHA1
- Provides modern Whirlpool for high-security needs
- Foundation for security features

**Why This Should Be Next:**
- Probably the easiest processor to port (code exists!)
- Small number of verbs (5)
- No complex logic needed
- High utility for web/API operations

---

## Related Processors

- **string** - String operations often combined with hashing
- **file** - File integrity checking
- **tcp** / **http** - API authentication

---

## Special Considerations

**Security Status (2025):**

**⚠️ MD5 - BROKEN:**
- **DO NOT USE** for:
  - Password hashing
  - Digital signatures
  - Certificate validation
  - Any security-critical application
- **OK for:**
  - Non-security checksums
  - Legacy system compatibility
  - Content-addressable storage (non-adversarial)

**⚠️ SHA1 - DEPRECATED:**
- **DO NOT USE** for:
  - SSL/TLS certificates
  - Code signing
  - New security applications
- **OK for:**
  - Git commits (non-adversarial)
  - Legacy system compatibility
  - HMAC (still secure for authentication)

**✅ Whirlpool - SECURE:**
- Safe for all applications
- No known practical attacks
- Suitable for high-security needs

**✅ HMAC - SECURE:**
- HMAC-MD5 and HMAC-SHA1 are still secure for authentication
- Even though MD5/SHA1 have collision weaknesses, HMAC use is safe
- Key must be ≥128 bits for security

**Implementation Notes:**
- Document security status in verb comments
- Warn users about MD5/SHA1 deprecation
- Recommend Whirlpool for new applications
- Consider adding SHA-256 in future (not in scope now)

**Existing Code Quality:**
- OpenSSL-derived implementations (well-tested)
- NESSIE-standard Whirlpool
- RFC-compliant HMAC
- Should work correctly as-is

**Binary vs Hex Output:**
- Default (flTranslate=true) returns hex string
- Binary mode (flTranslate=false) returns raw bytes
- Hex encoding algorithm already implemented
- Both modes should be thoroughly tested

**Memory Handling:**
- Existing code uses Handle-based operations
- Should work correctly with large inputs
- No known memory leaks
- Verify cleanup in error paths

**Thread Safety:**
- Hash functions use stack-allocated context structures
- Should be thread-safe (no shared state)
- HMAC implementations should also be safe
- Verify no global variables

---

## References

**Implementation:**
- Current: `Common/source/langcrypt.c` (complete implementation!)
- Supporting: `Common/source/md5.c`, `sha1dgst.c`, `whirlpool.c`
- Headers: `Common/headers/md5.h`, `sha.h`
- Stub: `tests/headless_crypt_verbs.c`

**Standards:**
- RFC 1321: MD5 Message-Digest Algorithm
- RFC 3174: US Secure Hash Algorithm 1 (SHA1)
- RFC 2104: HMAC: Keyed-Hashing for Message Authentication
- ISO/IEC 10118-3: Whirlpool hash function
- NESSIE: New European Schemes for Signatures, Integrity and Encryption

**Test Vectors:**
- MD5: https://www.ietf.org/rfc/rfc1321.txt (Appendix A.5)
- SHA1: https://www.ietf.org/rfc/rfc3174.txt (Section 7)
- HMAC: https://www.ietf.org/rfc/rfc2104.txt (Section 2)

**Security:**
- MD5 Status: https://www.kb.cert.org/vuls/id/836068
- SHA1 Status: https://shattered.io/
- NIST Recommendations: https://csrc.nist.gov/publications/

---

## Next Steps

1. ✅ Audit complete - ready for implementation
2. ⏳ Review langcrypt.c implementation (already done!)
3. ⏳ Create kernel verb wrapper in `tests/headless_crypt_verbs.c`
4. ⏳ Wire up 5 verbs to existing crypto functions
5. ⏳ Test with RFC test vectors (MD5, SHA1, HMAC)
6. ⏳ Verify binary vs hex output modes
7. ⏳ Document security status and recommendations
8. ⏳ Update implementation status

---

**Audit Status:** ✅ Complete and Approved for Implementation

**Special Note:** This is the easiest processor to implement - the entire cryptographic code already exists in langcrypt.c and just needs to be wired up to the kernel verb system!
