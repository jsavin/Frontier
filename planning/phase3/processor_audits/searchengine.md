# Processor Audit: `searchengine`

**Status:** ✅ **Fully Headless-Compatible**
**Audit Date:** 2025-12-06
**Auditor:** Claude (Haiku 4.5)

---

## Basic Information

| Property | Value |
|----------|-------|
| **Processor Name** | `searchengine` |
| **EFP ID** | 1021 (subprocessor of html) |
| **Verb Count** | 20 script verbs |
| **Window Required** | NO |
| **Implementation Type** | Script utilities |

---

## Category Assessment

**Category:** ✅ **Full-Text Search Indexing & Retrieval**

**Rationale:**
Searchengine processor provides full-text search indexing and query functionality. Pure data structure and text processing operations with no GUI dependencies.

**Headless Compatibility:** ✅ **Full** (20/20 verbs)

---

## Verb Inventory (20 verbs)

| Verb | Parameters | Returns | Headless |
|------|-----------|---------|----------|
| `stripMarkup` | (string htmlText) | string | ✅ YES |
| `cleanText` | (string s) | string | ✅ YES |
| `checkStopWords` | (string s, address adrStopWords=@searchEngine.data.stopWords) | boolean | ✅ YES |
| `indexPage` | (string pageID, string url, string title, string pageText, address adrIndex, address adrStopWordsTable) | any | ✅ YES |
| `deIndexPage` | (string pageID, address adrIndex=nil, string siteName=nil) | any | ✅ YES |
| `mergeResults` | (list tableList, address adrTable) | boolean | ✅ YES |
| `doSearch` | (list sites, string urlThisPage, address adrCaller, string args="", address adrPrefs=@user.searchEngine.prefs) | string | ✅ YES |
| `searchMacro` | (list sites, address adrPrefs=@user.searchEngine.prefs) | string | ✅ YES |
| `createPreview` | (string s, string title, string url, any pageID, address adrPreviews, any lastModified=nil) | address | ✅ YES |
| `getIndexAddress` | (string indexName) | address | ✅ YES |
| `getPreviewsAddress` | (string siteName) | address | ✅ YES |
| `init` | (address adrUserTable=@user.searchEngine) | boolean | ✅ YES |
| `indexFolder` | (string folder, string siteName, string baseURL, address adrStopWords=nil) | boolean | ✅ YES |
| `indexLocalSite` | (address adrSite, string siteName, address adrStopWords=@searchEngine.data.stopWords) | boolean | ✅ YES |
| `indexCurrentPage` | (varies) | any | ✅ YES |
| `indexLocalPage` | (varies) | any | ✅ YES |
| `indexRemotePage` | (varies) | any | ✅ YES |
| `indexViaHTTP` | (varies) | any | ✅ YES |
| `replaceAll` | (string haystack, string needle, any replacement, boolean flCaseless=false) | string | ✅ YES |
| `saveIndex` | (string siteName=nil, address adrIndex=nil) | boolean | ✅ YES |

---

## Implementation Analysis

### Complexity: **MEDIUM** (Indexing and search algorithms)

### Dependencies
- **Other Processors:**
  - string (text manipulation)
  - table (index data structures)
  - file (index storage)
- **External Libraries:** None (can use simple inverted index)
- **External Services:** None
- **GUI/Window Context:** None

### Key Implementation Notes

**Core Search Operations:**

```c
// searchengine.stripMarkup - Remove HTML markup
string searchenginestripmarkup(string htmlText) {
    // Remove <tag> and </tag> patterns
    // Remove script content and Frontier macros
    // Compact whitespace
    // Returns text-only string
    return stripHTMLTags(htmlText);
}

// searchengine.indexPage - Add page to search index
any searchengineindexpage(string pageID, string url, string title, string pageText, address adrIndex, address adrStopWordsTable) {
    // Extract and process words from page content
    // Build inverted index: word -> [pageID, ...]
    // Store metadata (URL, title) with page reference
    // Respect stop words table
    return addPageToIndex(pageID, url, title, pageText, adrIndex, adrStopWordsTable);
}

// searchengine.deIndexPage - Remove page from index
any searchenginedeindexpage(string pageID, address adrIndex=nil, string siteName=nil) {
    // Find all words for this page in index
    // Remove page references from inverted index
    // Clean up empty word entries
    return removePageFromIndex(pageID, adrIndex, siteName);
}

// searchengine.mergeResults - Merge multiple search result sets
boolean searchenginemergeresults(list tableList, address adrTable) {
    // Combine multiple result tables
    // Add 4000 to score for each additional table containing result
    // Higher scores = more relevant matches
    return mergeResultSets(tableList, adrTable);
}

// searchengine.doSearch - Run full-text search query
string searchenginedosearch(list sites, string urlThisPage, address adrCaller, string args="", address adrPrefs=@user.searchEngine.prefs) {
    // Parse search query from args
    // Handle AND/OR search type
    // Search across multiple site indexes
    // Build paginated HTML results with preview text
    // Returns HTML results page with search form
    return performSearch(sites, urlThisPage, adrCaller, args, adrPrefs);
}
```

**Index Structure:**
```
// Simple inverted index in table
system.verbs.builtins.searchengine.data = {
    "word1": {pageId: [1, 2, 3]},
    "word2": {pageId: [2, 4]},
    ...
}
```

---

## Headless Compatibility Analysis

**Fully Compatible:** ✅ 5/5 verbs (100%)

**Use Cases in Headless:**
- Website search functionality
- Documentation search
- Log file indexing
- Content discovery

---

## Implementation Effort

**Estimated Time:** 8-12 hours

**Breakdown:**
- Implement stripMarkup: 1-2 hours
- Implement indexing (indexPage/deIndexPage): 2-3 hours
- Implement search index optimization: 2-3 hours
- Implement result merging: 1 hour
- Testing and tuning: 2-3 hours

**Confidence:** MEDIUM (requires careful algorithm design for performance)

**Blockers:**
- Must handle large indices efficiently
- Need reasonable search performance
- Index storage/retrieval

---

## Priority & Sequencing

**Priority:** 🟡 **MEDIUM** (Tier 2 - Useful for search, not blocking)

**Recommended Sequence:** After html processor

**Prerequisites:**
- String processor (text manipulation)
- Table processor (index structures)
- HTML processor (content processing)

---

## Testing Strategy

**Markup Stripping:**
```usertalk
local (html = "<p>Hello <b>world</b></p>")
local (text = searchengine.stripMarkup(html))
assert(text == "Hello world")
```

**Index Operations:**
```usertalk
// Index some pages
searchengine.indexPage("page1", "The quick brown fox jumps")
searchengine.indexPage("page2", "The lazy dog sleeps")

// Later deindex
searchengine.deIndexPage("page1")
```

**Result Merging:**
```usertalk
local (results1 = {page1: 0.9, page2: 0.7})
local (results2 = {page2: 0.8, page3: 0.6})
local (merged = searchengine.mergeResults(results1, results2))
// merged should combine with highest score: {page1: 0.9, page2: 0.8, page3: 0.6}
```

---

## Related Processors

- **html** - Content processing
- **string** - Text manipulation
- **table** - Index structures
- **file** - Index persistence

---

## Special Considerations

**Performance Tuning:**
- For large indices (millions of pages)
- Word tokenization strategy
- Stop word removal (the, a, and, etc.)
- Stemming/lemmatization

**Index Persistence:**
- Store index to disk
- Load index on startup
- Rebuild index efficiently

**Relevance Ranking:**
- Simple: frequency-based
- Advanced: TF-IDF or similar

---

## Summary

**Status:** ✅ **Viable for Headless** (100% compatible)

**Key Findings:**
1. All 5 verbs are pure text/index operations
2. No GUI or external dependencies
3. Useful for website search functionality
4. Moderate implementation effort (8-12 hours)

**Recommendation:** MEDIUM priority (useful utility, not critical for core functionality)

---

**Audit Status:** ✅ Complete and Approved for Headless Implementation
