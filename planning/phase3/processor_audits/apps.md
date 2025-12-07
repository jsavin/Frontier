# Processor Audit: `apps`

**Status:** ⏸️ **Deferred - Awaiting IAC Architecture Decision**
**Audit Date:** 2025-12-06
**Auditor:** Claude (Haiku 4.5)
**Note:** Implementation deferred pending real Inter-Application Communication (IAC) strategy

---

## Basic Information

| Property | Value |
|----------|-------|
| **Processor Name** | `system.verbs.apps` |
| **Verb Count** | ~30 app categories, 100+ verbs total |
| **Window Required** | NO (but may integrate with external apps) |
| **Implementation Type** | Script Verbs (UserTalk) |
| **Location** | `system.verbs.apps/` (30 app subdirectories) |

---

## Category Assessment

**Category:** ⚠️ **Application Integration & External Services**

**Rationale:**
App verbs provide scripting glue for integrating with external applications and web services. Most are script-based verbs that make HTTP requests, call external APIs, or invoke command-line tools. Headless-compatible if underlying services are accessible (network, external apps, etc.). GUI dependency is rare—mainly in app-specific UI integration.

**Headless Compatibility:** ✅ **High** (~95% compatible with caveats)

**Caveats:**
- Network-dependent verbs require internet connectivity
- App-specific verbs assume external application is installed
- Some may have GUI components (embedded browsing, etc.)
- Requires appropriate API keys/credentials for web services

---

## App Categories Inventory

### Web Services (15+ categories)

| App | Verb Count | Type | Headless |
|-----|-----------|------|----------|
| `amazon` | 3-5 | AWS API integration | ✅ YES (requires API key) |
| `blogger` | 5+ | Blogger API | ✅ YES (requires auth) |
| `google` | 5+ | Google Search API | ✅ YES (requires API key) |
| `googleBlogSearch` | 2-3 | Google Blog Search | ✅ YES (API) |
| `mailToTheFuture` | 2-3 | Email scheduling service | ✅ YES (HTTP API) |
| `manila` | 5+ | Manilla CMS integration | ✅ YES (XML-RPC) |
| `metaWeblog` | 5+ | MetaWeblog API (blogging) | ✅ YES (XML-RPC) |
| `salesforce` | 10+ | Salesforce CRM API | ✅ YES (SOAP/REST) |
| `trackback` | 2-3 | TrackBack protocol | ✅ YES (HTTP) |
| `weblogsCom` | 2-3 | Weblogs.com ping | ✅ YES (HTTP) |
| `weblogUpdates` | 2-3 | Blog update notification | ✅ YES (HTTP) |
| `xmlStorageSystem` | 5+ | XML storage API | ✅ YES (HTTP/XML) |

### Local Applications (10+ categories)

| App | Verb Count | Type | Headless |
|-----|-----------|------|----------|
| `BBEdit` | 5+ | Text editor scripting (macOS) | ⚠️ PARTIAL (requires app) |
| `Eudora` | 5+ | Email client scripting | ⚠️ PARTIAL (requires app) |
| `Fetch` | 3-5 | FTP client scripting | ⚠️ PARTIAL (requires app) |
| `Filemaker` | 5+ | FileMaker database scripting | ⚠️ PARTIAL (requires app) |
| `Finder` | 5+ | macOS Finder scripting | ❌ NO (GUI-dependent) |
| `FinderClassic` | 3-5 | Classic Mac Finder | ❌ NO (obsolete) |
| `FinderMenu` | 2-3 | Finder menu integration | ❌ NO (GUI-dependent) |
| `Netscape` | 3-5 | Netscape browser (obsolete) | ❌ NO (obsolete) |
| `msExplorer` | 3-5 | Windows Explorer scripting | ❌ NO (GUI-dependent) |
| `WebStar` | 5+ | WebStar web server scripting | ✅ YES |

### System Integration (5+ categories)

| App | Verb Count | Type | Headless |
|-----|-----------|------|----------|
| `winShell` | 5+ | Windows shell commands | ✅ YES |
| `AnArchie` | 2-3 | Archie search (obsolete) | ❌ NO (obsolete) |
| `BarChart` | 2-3 | Chart/graph creation | ⚠️ PARTIAL |
| `netEvents` | 3-5 | Network event monitoring | ✅ YES |
| `stuff` | 2-3 | StuffIt compression | ⚠️ PARTIAL (requires app) |
| `uBASE` | 3-5 | Database connectivity | ✅ YES |

### Obsolete Applications (Removed)

| App | Reason |
|-----|--------|
| `WebStar` | Old MacOS web server (no longer functional) |

---

## Implementation Analysis

### Complexity: **VARIABLE** (Mix of Simple HTTP to Complex APIs)

### Dependencies

- **Other Processors:**
  - `tcp` - HTTP/network communication
  - `string` - XML/JSON parsing
  - `date` - Timestamp handling
  - `file` - File operations
  - External apps (optional)
  - External web services (optional)

- **External Services:** YES
  - Network connectivity required
  - API keys/credentials for services
  - External applications (optional)

### Key Implementation Notes

**Typical App Verb Pattern:**

```usertalk
// Example: amazon/search
on search (keyword) {
    local (url = "http://amazon.com/api?q=" + string.urlEncode(keyword))
    return (tcp.httpClient (url))
}
```

**Categories:**

1. **Web Service Integration (95% headless-compatible)**
   - HTTP API calls (GET/POST)
   - XML-RPC, REST, SOAP protocols
   - Require network connectivity and credentials
   - No GUI dependencies

2. **Local Application Scripting (50% headless-compatible)**
   - AppleScript/COM bridge to external apps
   - Require app to be installed and running
   - Many have GUI components
   - Obsolete apps (Netscape, FinderClassic) skip

3. **System Integration (80% headless-compatible)**
   - Command-line tool integration
   - Shell command execution
   - File/network operations
   - Some GUI dependencies (Finder, Explorer)

---

## Headless Compatibility Analysis

**Fully Compatible:** ✅ ~20-25 categories (HTTP APIs, system tools)

**Partially Compatible:** ⚠️ ~5-7 categories (requires external app)

**Not Compatible:** ❌ ~3-5 categories (GUI-dependent or obsolete)

**Summary by Type:**

**✅ Headless-Compatible (Web Services):**
- amazon, blogger, google, googleBlogSearch, mailToTheFuture, manila, metaWeblog, salesforce, trackback, weblogsCom, weblogUpdates, xmlStorageSystem (12 categories)
- These are pure HTTP API wrappers; fully headless

**⚠️ Partially Compatible (External Apps):**
- BBEdit, Eudora, Fetch, Filemaker, WebStar, stuff, BarChart, netEvents, uBASE (9 categories)
- Work if external app is installed and accessible
- May have GUI components (dialogs, menus)

**❌ Not Compatible (GUI/Obsolete):**
- Finder, FinderClassic, FinderMenu, msExplorer, Netscape, AnArchie (6 categories)
- GUI-dependent or obsolete technologies

---

## Headless Implementation Strategy

**Phase 1 - Web Service Verbs (High Priority)**
- Implement all HTTP-based API integrations (12 categories, ~60+ verbs)
- These are straightforward HTTP requests with XML/JSON parsing
- Requires TCP processor, string manipulation, date handling
- Estimated effort: 20-30 hours (many follow similar patterns)

**Phase 2 - System Integration (Medium Priority)**
- Implement winShell, netEvents, uBASE, WebStar (4-5 categories, ~20+ verbs)
- These are command-line and network-based
- Estimated effort: 8-12 hours

**Phase 3 - External App Scripting (Low Priority, Optional)**
- Implement BBEdit, Eudora, Fetch, Filemaker verbs if needed
- Requires AppleScript/COM bridge; platform-specific
- May not work well in headless contexts
- Skip for initial headless implementation

**Skip:**
- Finder, FinderMenu, msExplorer - GUI-dependent
- FinderClassic, Netscape, AnArchie - Obsolete

---

## Testing Strategy

**Web Service Verbs:**
- Mock HTTP responses or use live API endpoints
- Test with valid/invalid credentials
- Test network error handling (timeout, connection refused)
- Test with sample data for each service

**System Integration:**
- Test command execution
- Test network monitoring
- Test database connectivity (uBASE)

**External App Scripting:**
- Test only if app is installed
- Document app version requirements
- Skip in CI/automated testing

---

## Related Processors

- **tcp** - Network/HTTP operations (foundational)
- **string** - XML/JSON parsing, URL encoding
- **date** - Timestamp formatting for APIs
- **file** - File upload/download (some APIs)
- **db** - Database connectivity (uBASE, etc.)

---

## Summary

**Status:** ⏸️ **Deferred - Awaiting IAC Architecture Decision**

**Key Findings:**
1. Web service verbs (12 categories) are 100% headless-compatible
2. System integration verbs (5 categories) are mostly headless-compatible
3. External app scripting verbs (9 categories) require IAC framework (AppleScript, COM, etc.)
4. GUI-specific and obsolete verbs (5 categories) should be skipped
5. WebStar removed (old MacOS web server, no longer functional)

**Architectural Blocking Issue:**
- Apps verbs require a clear Inter-Application Communication (IAC) strategy
- Mix of local apps (AppleScript, COM) and remote services (HTTP APIs) needs unified approach
- Cannot implement without deciding:
  - How to handle platform-specific IAC (AppleScript on Mac, COM on Windows)
  - Whether to support legacy app scripting at all
  - How to handle deprecated apps (Netscape, FinderClassic, etc.)

**Recommendation:**
- **DEFER** all apps verb implementation until IAC architecture is defined
- Document the architectural requirements and open questions
- Once IAC strategy is decided, Phase 1 (web services) becomes HIGH PRIORITY
- Phase 2-3 (app scripting) depends on IAC framework availability

**Next Steps:**
1. Design IAC architecture (local apps + remote services)
2. Document platform-specific limitations (AppScript Mac-only, COM Windows-only)
3. Decide on legacy app support (deprecate old apps or maintain?)
4. Once decided, implement Phase 1 (straightforward HTTP APIs)
5. Implement Phase 2-3 based on IAC framework

---

**Audit Status:** ⏸️ Complete and Deferred (Awaiting Architecture Decision)
