# REPL Tab Completion Implementation Plan

## Overview

Implement progressive tab completion for the Frontier CLI REPL, building from simple keyword completion to context-aware dotted path navigation.

## Architecture

### File Structure

```
frontier-cli/
├── completion.c          # NEW: Editline integration, top-level completion logic
├── completion.h          # NEW: Public completion API
├── repl.c               # MODIFY: Call completion init/cleanup
└── Makefile             # MODIFY: Add completion.c

Common/source/
└── langcompletion.c     # NEW: Language-level completion (hash table enumeration)

Common/headers/
└── langcompletion.h     # NEW: Completion engine API
```

### Data Structures

```c
// Match result structure
typedef struct completion_match {
    char name[256];              // Matched name
    tyvaluetype type;            // Value type (for display hints)
    boolean is_table;            // Can be navigated further
} completion_match;

// Match collection (max 100 matches to limit memory)
typedef struct completion_matches {
    completion_match *items;
    size_t count;
    size_t capacity;
    char common_prefix[256];     // Longest common prefix for auto-complete
} completion_matches;

// Completion context (what we're completing)
typedef struct completion_context {
    char *line;                  // Full line being edited
    int cursor_pos;              // Cursor position in line
    char *token;                 // Token being completed (extracted from line)
    char *prefix;                // Table path prefix (e.g., "system.verbs" for "system.verbs.str")
    boolean is_address;          // Starts with @
    boolean has_dot;             // Contains dot (dotted path)
} completion_context;
```

---

## Phase 1: Built-in Keywords (2-3 hours)

### Goal
Complete UserTalk language keywords with simple prefix matching.

### Keywords List
```c
static const char *usertalk_keywords[] = {
    // Control flow
    "if", "else", "while", "for", "loop", "break", "continue", "return",
    // Declarations
    "local", "on", "bundle", "case", "kernel", "syscall", "thread",
    // Operators/literals
    "and", "or", "not", "true", "false", "nil",
    // Types
    "boolean", "char", "short", "long", "date", "direction",
    "string", "filespec", "alias", "objspec", "address", "binary",
    "list", "record", "outline", "table", "script", "menubar", "wptext",
    NULL
};
```

### Implementation

1. **Extract token being completed** from editline buffer
2. **Match against keyword list** (case-insensitive prefix match)
3. **If single match**: Insert completion
4. **If multiple matches**: Show list or complete common prefix

### Editline Integration

```c
static unsigned char repl_complete(EditLine *el, int ch) {
    const LineInfo *li = el_line(el);

    // Extract token at cursor
    completion_context ctx;
    completion_parse_context(li->buffer, li->cursor - li->buffer, &ctx);

    // Get matches
    completion_matches matches;
    completion_init_matches(&matches, 100);

    // Phase 1: Keywords
    completion_add_keyword_matches(&matches, ctx.token);

    // Handle results
    if (matches.count == 1) {
        el_insertstr(el, matches.items[0].name + strlen(ctx.token));
        return CC_REFRESH;
    } else if (matches.count > 1) {
        // Complete common prefix or show list
        completion_show_matches(&matches);
        return CC_REDISPLAY;
    }

    return CC_ERROR;  // No matches
}
```

### Testing
- `loc<TAB>` → `local`
- `whi<TAB>` → `while`
- `re<TAB>` → shows `record`, `return` (multiple matches)

---

## Phase 2: Database Names (4-6 hours)

### Goal
Complete names from current scope hash tables.

### Tables to Search (in order)
1. **Local scope** (if in script context) - local variables
2. **roottable** - top-level database entries
3. **systemtable** - system.* entries
4. **builtinstable** - built-in function names

### Implementation

```c
// Callback for hashtablevisit
typedef struct {
    completion_matches *matches;
    const char *prefix;
    size_t prefix_len;
} completion_visitor_context;

static boolean completion_visit_callback(hdlhashnode node, ptrvoid refcon) {
    completion_visitor_context *ctx = (completion_visitor_context *)refcon;

    bigstring bs;
    gethashkey(node, bs);

    // Convert to C string
    char name[256];
    copystring(bs, name);

    // Check prefix match (case-insensitive)
    if (strncasecmp(name, ctx->prefix, ctx->prefix_len) == 0) {
        completion_add_match(ctx->matches, name, (**node).val.valuetype);
    }

    return true;  // Continue iteration
}

void completion_add_table_matches(completion_matches *matches,
                                   hdlhashtable table,
                                   const char *prefix) {
    completion_visitor_context ctx = {
        .matches = matches,
        .prefix = prefix,
        .prefix_len = strlen(prefix)
    };

    hashtablevisit(table, completion_visit_callback, &ctx);
}
```

### Search Strategy
```c
void completion_gather_all_matches(completion_matches *matches, const char *token) {
    // Phase 1: Keywords first
    completion_add_keyword_matches(matches, token);

    // Phase 2: Database tables
    if (roottable != nil) {
        completion_add_table_matches(matches, roottable, token);
    }
    if (systemtable != nil) {
        completion_add_table_matches(matches, systemtable, token);
    }
    if (builtinstable != nil) {
        completion_add_table_matches(matches, builtinstable, token);
    }
}
```

### Testing
- `sys<TAB>` → `system` (from roottable)
- `file<TAB>` → shows `file`, `filespec`, etc.
- User-defined entries complete too

---

## Phase 3: Dotted Paths (1-2 days)

### Goal
Navigate dotted paths like `system.verbs.string<TAB>`.

### Parsing Logic

```c
boolean completion_parse_dotted_path(const char *token,
                                      char *table_path,    // output: "system.verbs"
                                      char *leaf_prefix) { // output: "string"
    const char *last_dot = strrchr(token, '.');
    if (!last_dot) {
        table_path[0] = '\0';
        strcpy(leaf_prefix, token);
        return false;  // No dot
    }

    size_t path_len = last_dot - token;
    strncpy(table_path, token, path_len);
    table_path[path_len] = '\0';
    strcpy(leaf_prefix, last_dot + 1);
    return true;
}
```

### Table Navigation

```c
hdlhashtable completion_navigate_to_table(const char *path) {
    // Parse path: "system.verbs.string" → ["system", "verbs", "string"]
    // Navigate from roottable through each component

    hdlhashtable current = roottable;
    char *path_copy = strdup(path);
    char *component = strtok(path_copy, ".");

    while (component != NULL) {
        bigstring bs;
        copyctopstring(component, bs);

        tyvaluerecord val;
        hdlhashnode node;
        if (!hashtablelookup(current, bs, &val, &node)) {
            free(path_copy);
            return nil;  // Path not found
        }

        if (val.valuetype != tablevaluetype && val.valuetype != externalvaluetype) {
            free(path_copy);
            return nil;  // Not a table
        }

        // Get the table handle
        if (!langexternalvaltotable(val, &current, node)) {
            free(path_copy);
            return nil;
        }

        component = strtok(NULL, ".");
    }

    free(path_copy);
    return current;
}
```

### Integration

```c
void completion_handle_dotted_path(completion_matches *matches, const char *token) {
    char table_path[512];
    char leaf_prefix[256];

    if (!completion_parse_dotted_path(token, table_path, leaf_prefix)) {
        // No dot - use existing logic
        completion_gather_all_matches(matches, token);
        return;
    }

    // Navigate to parent table
    hdlhashtable parent = completion_navigate_to_table(table_path);
    if (parent == nil) {
        return;  // Invalid path
    }

    // Enumerate children with prefix
    completion_add_table_matches(matches, parent, leaf_prefix);
}
```

### Testing
- `system.<TAB>` → shows system table children
- `system.verbs.str<TAB>` → `system.verbs.string`
- `nonexistent.path.<TAB>` → no matches (graceful)

---

## Phase 4: Context-Aware Completion (1-2 days)

### Goal
Complete based on context (e.g., after `file.` show only file.* verbs).

### Context Detection

```c
typedef enum {
    COMPLETION_CONTEXT_GENERAL,      // Default
    COMPLETION_CONTEXT_VERB_CALL,    // After known verb processor (file., db., etc.)
    COMPLETION_CONTEXT_ADDRESS,      // After @ symbol
    COMPLETION_CONTEXT_STRING,       // Inside string literal (no completion)
} completion_context_type;

completion_context_type completion_detect_context(const char *line, int cursor_pos) {
    // Check if inside string
    int quote_count = 0;
    for (int i = 0; i < cursor_pos; i++) {
        if (line[i] == '"' && (i == 0 || line[i-1] != '\\')) {
            quote_count++;
        }
    }
    if (quote_count % 2 == 1) {
        return COMPLETION_CONTEXT_STRING;
    }

    // Check for @ prefix
    // Scan backwards from cursor to find token start
    int token_start = cursor_pos;
    while (token_start > 0 && (isalnum(line[token_start-1]) || line[token_start-1] == '.' || line[token_start-1] == '@')) {
        token_start--;
    }
    if (token_start < cursor_pos && line[token_start] == '@') {
        return COMPLETION_CONTEXT_ADDRESS;
    }

    // Check for verb processor prefix
    const char *verb_processors[] = {
        "file.", "db.", "string.", "table.", "op.", "sys.", "date.",
        "clock.", "math.", "dialog.", "xml.", "html.", "tcp.", "target.",
        NULL
    };
    for (int i = 0; verb_processors[i]; i++) {
        size_t len = strlen(verb_processors[i]);
        if (cursor_pos >= len && strncmp(line + token_start, verb_processors[i], len) == 0) {
            return COMPLETION_CONTEXT_VERB_CALL;
        }
    }

    return COMPLETION_CONTEXT_GENERAL;
}
```

### Verb Processor Completion

```c
void completion_add_verb_processor_matches(completion_matches *matches,
                                            const char *processor,
                                            const char *prefix) {
    // Look up processor in verbstable
    bigstring bs;
    copyctopstring(processor, bs);

    tyvaluerecord val;
    hdlhashnode node;
    if (!hashtablelookup(verbstable, bs, &val, &node)) {
        return;  // Unknown processor
    }

    hdlhashtable verbs;
    if (!langexternalvaltotable(val, &verbs, node)) {
        return;
    }

    // Enumerate verbs with prefix
    completion_add_table_matches(matches, verbs, prefix);
}
```

### Address (@) Completion

```c
void completion_add_address_matches(completion_matches *matches, const char *path) {
    // Skip @ prefix if present
    if (path[0] == '@') {
        path++;
    }

    // Use dotted path logic starting from roottable
    completion_handle_dotted_path(matches, path);
}
```

### Testing
- `file.wr<TAB>` → `file.write`
- `@system.ver<TAB>` → `@system.verbs`
- Inside `"string<TAB>"` → no completion

---

## Performance Considerations

### Limits
- **Max matches**: 100 (prevents memory bloat on large tables)
- **Max table depth**: 10 levels (prevents infinite recursion)
- **Timeout**: None needed if we limit matches

### Memory Management
- Use static buffers where possible
- Match array allocated once, reused per completion
- Free matches after display

### Lazy Loading
- Don't enumerate tables until tab is pressed
- Cache nothing between completions (tables can change)

---

## Testing Strategy

### Unit Tests
1. Token extraction from line buffer
2. Keyword prefix matching
3. Dotted path parsing
4. Table navigation

### Integration Tests
```yaml
tests:
  - name: "completion - keyword local"
    script: |
      # Test harness that simulates tab completion
      return completion_test("loc", "local")
    expected_success: true

  - name: "completion - dotted path"
    script: |
      return completion_test("system.ver", "system.verbs")
    expected_success: true
```

### Manual Testing Checklist
- [ ] Single keyword completion
- [ ] Multiple keyword matches (shows list)
- [ ] Database name completion
- [ ] Dotted path navigation
- [ ] Context-aware verb completion
- [ ] Address (@) completion
- [ ] No crash on invalid paths
- [ ] Performance with large tables

---

## Implementation Order

1. **Create file structure** (completion.c, completion.h, langcompletion.c, langcompletion.h)
2. **Phase 1**: Keywords - get basic completion working
3. **Test & commit Phase 1**
4. **Phase 2**: Database names - add hashtablevisit integration
5. **Test & commit Phase 2**
6. **Phase 3**: Dotted paths - add table navigation
7. **Test & commit Phase 3**
8. **Phase 4**: Context-aware - add context detection
9. **Final testing & PR**

---

## Risks & Mitigations

| Risk | Mitigation |
|------|------------|
| Large tables cause slow completion | Limit to 100 matches |
| Crash on nil table handles | Null checks everywhere |
| Memory leaks | Use Frontier handle conventions |
| Editline API complexity | Start simple, iterate |
| Thread safety | REPL is single-threaded, not a concern |

---

## Success Criteria

- [ ] Tab completion works for all UserTalk keywords
- [ ] Tab completion shows database entries
- [ ] Dotted paths navigate correctly
- [ ] Context-aware completion filters appropriately
- [ ] No crashes or memory leaks
- [ ] Performance < 100ms for typical completions
- [ ] Graceful handling of invalid input
