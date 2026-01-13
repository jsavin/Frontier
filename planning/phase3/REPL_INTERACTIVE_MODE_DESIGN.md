# REPL Interactive Mode Design

**Document Status**: Design Document
**Version**: 1.0
**Date**: 2026-01-12
**Author**: System Architect (Claude Sonnet 4.5)

---

## Executive Summary

This document describes the design and implementation of a comprehensive REPL (Read-Eval-Print Loop) interactive mode for `frontier-cli`. The REPL provides an interactive UserTalk development environment that serves as:

1. **Developer productivity tool** - Fast experimentation with UserTalk without file creation
2. **Testing and debugging interface** - Inspect runtime state, test expressions, explore ODB
3. **Foundation for server mode** - Architecture designed to support future network protocol abstraction

The REPL design prioritizes simplicity, predictable behavior, and extensibility. It maintains backward compatibility with existing batch execution modes while adding powerful interactive capabilities.

**Key Design Principles**:
- **Simplicity over cleverness**: Explicit multi-line continuation (`\`) rather than complex parsing
- **Persistent workspace**: Variables live across REPL evaluations in a dedicated table scope
- **Standard conventions**: `/command` syntax familiar from modern developer tools
- **Stateless evaluation**: Each input is compiled and executed independently (except for workspace persistence)
- **Server-ready architecture**: Clear separation between UI layer (terminal I/O) and evaluation engine

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Implementation Phases](#implementation-phases)
3. [Technical Design](#technical-design)
4. [Command Line Interface](#command-line-interface)
5. [Special Commands](#special-commands)
6. [Testing Strategy](#testing-strategy)
7. [Future Considerations](#future-considerations)
8. [Migration Path](#migration-path)

---

## Architecture Overview

### High-Level Components

```
┌─────────────────────────────────────────────────────────────┐
│                     frontier-cli                             │
├─────────────────────────────────────────────────────────────┤
│  Main Entry Point (main.c)                                   │
│    ├─ CLI Parser (cli_parser.c)                             │
│    ├─ Mode Detector (determine interactive vs batch)        │
│    └─ Mode Router                                            │
│         ├─ Batch Executor (cli_executor.c) [existing]       │
│         └─ REPL Loop (repl.c) [new]                         │
├─────────────────────────────────────────────────────────────┤
│  REPL Components (Phase 1-4)                                 │
│    ├─ Input Handler (repl_input.c)                          │
│    │    ├─ Line reading (readline/libedit integration)      │
│    │    ├─ Multi-line continuation handling                 │
│    │    ├─ History management                                │
│    │    └─ Special command detection                         │
│    ├─ Evaluator (repl_eval.c)                               │
│    │    ├─ Workspace table management                        │
│    │    ├─ UserTalk compilation                              │
│    │    ├─ UserTalk execution                                │
│    │    └─ Error capture and formatting                      │
│    ├─ Output Formatter (repl_output.c)                      │
│    │    ├─ Value-to-string conversion (coercetostring)      │
│    │    ├─ Table summary formatting                          │
│    │    ├─ Error display                                     │
│    │    └─ Prompt rendering                                  │
│    └─ Command Processor (repl_commands.c)                   │
│         ├─ /exit, /help, /clear, /vars                      │
│         └─ Future: /load, /save, /cd                        │
├─────────────────────────────────────────────────────────────┤
│  Frontier Runtime (existing)                                 │
│    ├─ Language Engine (langcompiletext, langrunscriptcode)  │
│    ├─ Hash Table System (hashtable operations)              │
│    ├─ Value System (tyvaluerecord, coercetostring)          │
│    └─ Database System (system root, workspace persistence)  │
└─────────────────────────────────────────────────────────────┘
```

### Separation of Concerns

**UI Layer** (`repl_input.c`, `repl_output.c`):
- Terminal I/O (readline, printf, ANSI codes)
- Line editing, history, tab completion (future)
- Prompt rendering
- **Future abstraction point**: Replace terminal I/O with network protocol for server mode

**Evaluation Layer** (`repl_eval.c`):
- UserTalk compilation and execution
- Workspace table lifecycle
- Error capture
- **Stateless**: No UI dependencies, testable without terminal

**Command Layer** (`repl_commands.c`):
- Special command parsing (`/exit`, `/help`, etc.)
- Metadata operations (show workspace, clear state)
- **Extensible**: Easy to add new commands without touching evaluator

**Core Loop** (`repl.c`):
- Orchestrates input → eval → output cycle
- Manages REPL state machine (running, exit requested, error recovery)
- Handles signal interrupts (Ctrl-C)
- Initializes and cleans up subsystems

---

## Implementation Phases

### Phase 1: Basic REPL Foundation (Week 1-2)

**Goal**: Minimal viable REPL with single-line input, workspace persistence, and core commands.

**Deliverables**:
- Single-line input (no multi-line yet)
- Persistent workspace table for variables
- Basic value display using `coercetostring()`
- Core commands: `/exit`, `/help`, `/clear`, `/vars`
- System root auto-loading with priority order
- Simple prompt: `[root]> `
- Basic error handling (show error, don't crash)

**Files to Create**:
- `frontier-cli/repl.h` - Main REPL loop interface
- `frontier-cli/repl.c` - Main REPL loop implementation
- `frontier-cli/repl_eval.h` - Evaluator interface
- `frontier-cli/repl_eval.c` - Workspace management and script execution
- `frontier-cli/repl_output.h` - Output formatting interface
- `frontier-cli/repl_output.c` - Value display logic
- `frontier-cli/repl_commands.h` - Command processor interface
- `frontier-cli/repl_commands.c` - Command implementations

**Files to Modify**:
- `frontier-cli/main.c` - Add REPL mode detection and initialization
- `frontier-cli/cli_parser.h` - Add REPL-specific options
- `frontier-cli/cli_parser.c` - Parse REPL flags

**Success Criteria**:
```bash
$ ./frontier-cli
[root]> workspace.x = 42
42
[root]> workspace.x * 2
84
[root]> /vars
workspace.x = 42
[root]> /clear
Workspace cleared
[root]> /exit
```

---

### Phase 2: Multi-line Input and History (Week 3)

**Goal**: Backslash continuation, persistent command history, readline integration.

**Deliverables**:
- Backslash (`\`) continuation for multi-line input
- Persistent history in `~/.frontier/history`
- Readline/libedit integration (up/down arrows, Ctrl-R search)
- Improved value formatting for complex types (tables, binaries)
- Better error messages with line numbers for multi-line scripts

**Files to Create**:
- `frontier-cli/repl_input.h` - Input handling interface
- `frontier-cli/repl_input.c` - Multi-line handling, readline integration

**Files to Modify**:
- `frontier-cli/repl.c` - Use new input handler
- `frontier-cli/repl_output.c` - Enhanced value formatting

**Success Criteria**:
```bash
$ ./frontier-cli
[root]> local(sum = 0); \
      > for i = 1 to 10 { \
      >   sum = sum + i \
      > }; \
      > return sum
55
[root]> <up arrow shows previous multi-line command>
```

---

### Phase 3: File Operations and Enhanced Commands (Week 4)

**Goal**: Execute UserTalk files, load scripts into workspace, stdin batch mode.

**Deliverables**:
- `-f <file>` flag to execute UserTalk file in batch mode
- `/load <file>` command to execute file in REPL context
- Stdin redirection support: `frontier-cli < script.ut`
- `/save <file>` command to save workspace to file (future consideration)
- Better table display (show structure, not just "[table]")

**Files to Modify**:
- `frontier-cli/cli_parser.c` - Add `-f` flag
- `frontier-cli/main.c` - Detect stdin redirect, route to batch executor
- `frontier-cli/repl_commands.c` - Add `/load` command
- `frontier-cli/repl_output.c` - Improve table formatting

**Success Criteria**:
```bash
$ cat test.ut
workspace.greeting = "Hello, REPL!"
workspace.answer = 42

$ ./frontier-cli -f test.ut
42

$ ./frontier-cli < test.ut
42

$ ./frontier-cli
[root]> /load test.ut
Loaded test.ut
[root]> workspace.greeting
"Hello, REPL!"
```

---

### Phase 4: Server Mode Preparation (Week 5-6)

**Goal**: Abstract I/O layer to enable future network protocol support.

**Deliverables**:
- I/O abstraction layer (`repl_io.h`) with terminal and network implementations
- Protocol buffer design for remote REPL (JSON or MessagePack)
- Refactor REPL loop to be I/O-agnostic
- Network server stub (listen on TCP socket, accept connections, route to REPL evaluator)
- Authentication and authorization framework (disabled by default, opt-in)

**Files to Create**:
- `frontier-cli/repl_io.h` - Abstract I/O interface
- `frontier-cli/repl_io_terminal.c` - Terminal implementation
- `frontier-cli/repl_io_network.c` - Network protocol implementation (stub)
- `frontier-cli/repl_server.c` - TCP server for remote REPL
- `docs/REPL_NETWORK_PROTOCOL.md` - Protocol specification

**Files to Modify**:
- `frontier-cli/repl.c` - Use abstract I/O layer
- `frontier-cli/main.c` - Add `--server` mode

**Success Criteria**:
```bash
# Terminal mode (unchanged)
$ ./frontier-cli
[root]> 1+1
2

# Server mode (new)
$ ./frontier-cli --server --port 8080
Frontier REPL server listening on port 8080

# Client (netcat for testing)
$ nc localhost 8080
{"jsonrpc":"2.0","method":"eval","params":{"code":"1+1"},"id":1}
{"jsonrpc":"2.0","result":"2","id":1}
```

---

## Technical Design

### Main REPL Loop (repl.c)

```c
// repl.h
#ifndef REPL_H
#define REPL_H

#include "../Common/headers/frontier.h"

typedef struct {
    boolean interactive;           // Interactive mode (vs batch)
    boolean running;               // REPL loop active
    hdlhashtable workspace;        // Persistent workspace table
    hdlhashtable context_table;    // Current context (for prompt, target.get)
    bigstring context_path;        // Dot-path to context (e.g., "workspace.foo")
    char* history_file;            // Path to command history (~/.frontier/history)
} repl_state_t;

// Initialize REPL state
boolean repl_init(repl_state_t* state);

// Run REPL loop (blocking)
void repl_run(repl_state_t* state);

// Cleanup REPL state
void repl_cleanup(repl_state_t* state);

#endif // REPL_H
```

```c
// repl.c - Main loop pseudocode
void repl_run(repl_state_t* state) {
    char* input = NULL;

    // Print welcome message
    repl_output_welcome();

    while (state->running) {
        // Render prompt based on current context
        repl_output_prompt(state->context_path);

        // Read input (handles multi-line continuation in Phase 2)
        if (!repl_input_readline(state, &input)) {
            // EOF (Ctrl-D) or error
            state->running = false;
            break;
        }

        // Skip empty lines
        if (input == NULL || strlen(input) == 0) {
            free(input);
            continue;
        }

        // Check for special command (/exit, /help, etc.)
        if (input[0] == '/') {
            if (!repl_command_execute(state, input)) {
                // Command requested exit or error
                if (strcmp(input, "/exit") == 0) {
                    state->running = false;
                }
            }
            free(input);
            continue;
        }

        // Compile and execute UserTalk
        tyvaluerecord result;
        bigstring error;
        if (repl_eval_execute(state, input, &result, error)) {
            // Success - display result
            repl_output_value(&result);
            disposevaluerecord(result, false);
        } else {
            // Error - display error message
            repl_output_error(error);
        }

        free(input);
    }

    repl_output_goodbye();
}
```

---

### Workspace Table Management (repl_eval.c)

The workspace table is the key to persistent state across REPL evaluations. It lives in the system root's table hierarchy and persists variables declared at the REPL.

**Design Decision**: Use a dedicated `workspace` table rather than polluting root or system tables.

**Workspace Lifecycle**:
1. **Initialization**: Create `system.repl.workspace` table on REPL startup
2. **Evaluation**: Set `currenthashtable` to workspace before script execution
3. **Persistence**: Variables declared in REPL scripts are stored in workspace
4. **Cleanup**: On `/clear`, dispose all workspace entries (or dispose and recreate table)

```c
// repl_eval.h
#ifndef REPL_EVAL_H
#define REPL_EVAL_H

#include "../Common/headers/frontier.h"
#include "repl.h"

// Initialize workspace table (called once on REPL startup)
boolean repl_eval_init_workspace(repl_state_t* state);

// Execute UserTalk code in workspace context
// Returns true on success, false on error (error message in bserror)
boolean repl_eval_execute(repl_state_t* state, const char* code,
                          tyvaluerecord* result, bigstring bserror);

// Clear all workspace variables
boolean repl_eval_clear_workspace(repl_state_t* state);

// Enumerate workspace variables (for /vars command)
typedef boolean (*repl_var_visitor)(bigstring name, tyvaluerecord* val, void* refcon);
boolean repl_eval_visit_workspace(repl_state_t* state, repl_var_visitor visitor, void* refcon);

#endif // REPL_EVAL_H
```

```c
// repl_eval.c - Implementation
boolean repl_eval_init_workspace(repl_state_t* state) {
    // Ensure system.repl exists
    hdlhashtable repl_table = nil;
    bigstring bs_repl;
    copyctopstring("repl", bs_repl);

    if (systemtable == nil) {
        log_error(LOG_COMP_GENERAL, "System table not loaded - cannot create workspace");
        return false;
    }

    // Find or create system.repl
    if (!findnamedtable(systemtable, bs_repl, &repl_table)) {
        if (!tablenewsubtable(systemtable, bs_repl, &repl_table)) {
            log_error(LOG_COMP_GENERAL, "Failed to create system.repl table");
            return false;
        }
    }

    // Create system.repl.workspace
    hdlhashtable workspace = nil;
    bigstring bs_workspace;
    copyctopstring("workspace", bs_workspace);

    if (!findnamedtable(repl_table, bs_workspace, &workspace)) {
        if (!tablenewsubtable(repl_table, bs_workspace, &workspace)) {
            log_error(LOG_COMP_GENERAL, "Failed to create workspace table");
            return false;
        }
    }

    state->workspace = workspace;
    state->context_table = workspace;  // Default context is workspace
    copystring(bs_workspace, state->context_path);

    return true;
}

boolean repl_eval_execute(repl_state_t* state, const char* code,
                         tyvaluerecord* result, bigstring bserror) {
    // Convert code to Handle
    size_t code_len = strlen(code);
    Handle htext = NewHandle(code_len);
    if (htext == nil) {
        copyctopstring("Out of memory", bserror);
        return false;
    }

    HLock(htext);
    memcpy(*htext, code, code_len);
    HUnlock(htext);

    // Compile UserTalk
    hdltreenode hcode = nil;
    if (!langcompiletext(htext, false, &hcode)) {
        // Compilation error - get error string
        langgeterrorstring(bserror);
        DisposeHandle(htext);
        return false;
    }

    // Set workspace as current context
    hdlhashtable saved_current = currenthashtable;
    currenthashtable = state->workspace;

    // Execute in workspace context
    bigstring empty_name;
    setemptystring(empty_name);
    setnilvalue(result);

    boolean ok = langrunscriptcode(state->workspace, empty_name, hcode,
                                   nil, nil, result);

    // Restore previous context
    currenthashtable = saved_current;

    if (!ok) {
        // Runtime error
        langgeterrorstring(bserror);
        langdisposecodetree(hcode);
        DisposeHandle(htext);
        return false;
    }

    // Success
    langdisposecodetree(hcode);
    DisposeHandle(htext);
    return true;
}

boolean repl_eval_clear_workspace(repl_state_t* state) {
    if (state->workspace == nil) {
        return false;
    }

    // Dispose all entries in workspace table
    hashclearhashtable(state->workspace);

    return true;
}

boolean repl_eval_visit_workspace(repl_state_t* state, repl_var_visitor visitor, void* refcon) {
    if (state->workspace == nil || visitor == nil) {
        return false;
    }

    // Iterate workspace hash table
    hdlhashnode nomad = (**state->workspace).hashfirstnode;

    while (nomad != nil) {
        bigstring name;
        tyvaluerecord val;

        gethashkey(nomad, name);
        hashgetvaluerecord(nomad, &val);

        if (!(*visitor)(name, &val, refcon)) {
            break;  // Visitor requested stop
        }

        nomad = (**nomad).hashlink;
    }

    return true;
}
```

---

### Value Display (repl_output.c)

The REPL must display evaluation results in a human-readable format. Frontier's existing `coercetostring()` function handles most value types, but tables, binaries, and complex types need special handling.

**Design Decision**: Use existing `coercetostring()` for Phase 1, enhance in Phase 2.

```c
// repl_output.h
#ifndef REPL_OUTPUT_H
#define REPL_OUTPUT_H

#include "../Common/headers/frontier.h"
#include "repl.h"

// Display welcome message
void repl_output_welcome(void);

// Display goodbye message
void repl_output_goodbye(void);

// Display prompt (e.g., "[root]> ")
void repl_output_prompt(bigstring context_path);

// Display evaluation result
void repl_output_value(tyvaluerecord* val);

// Display error message
void repl_output_error(bigstring bserror);

// Display table summary (for /vars command)
void repl_output_table_summary(hdlhashtable htable);

#endif // REPL_OUTPUT_H
```

```c
// repl_output.c - Implementation
void repl_output_welcome(void) {
    printf("Frontier REPL - Interactive UserTalk Environment\n");
    printf("Type /help for commands, /exit to quit\n\n");
}

void repl_output_goodbye(void) {
    printf("\nGoodbye!\n");
}

void repl_output_prompt(bigstring context_path) {
    char path_str[256];
    copyctopstring(stringbaseaddress(context_path), path_str);
    printf("[%s]> ", path_str);
    fflush(stdout);
}

void repl_output_value(tyvaluerecord* val) {
    // Special handling for nil (don't print anything)
    if (val->valuetype == novaluetype) {
        return;
    }

    // Special handling for tables (show summary, not full dump)
    if (val->valuetype == externalvaluetype) {
        hdlexternalvariable hv = (hdlexternalvariable)val->data.externalvalue;
        if (hv != nil && (**hv).variabletype == vartable) {
            printf("[table with %ld items]\n", hashtablecount((hdlhashtable)(**hv).variabledata));
            return;
        }
    }

    // Use Frontier's built-in coercetostring for everything else
    tyvaluerecord val_copy = *val;  // Don't mutate original
    if (!coercetostring(&val_copy)) {
        printf("[unable to display value of type %d]\n", val->valuetype);
        return;
    }

    // Extract and print string
    bigstring bs;
    copyheapstring(val_copy.data.stringvalue, bs);

    // Print value (add quotes for string types)
    if (val->valuetype == stringvaluetype) {
        printf("\"%.*s\"\n", (int)stringlength(bs), stringbaseaddress(bs));
    } else {
        printf("%.*s\n", (int)stringlength(bs), stringbaseaddress(bs));
    }

    disposevaluerecord(val_copy, false);
}

void repl_output_error(bigstring bserror) {
    printf("Error: %.*s\n", (int)stringlength(bserror), stringbaseaddress(bserror));
}

void repl_output_table_summary(hdlhashtable htable) {
    if (htable == nil) {
        printf("(empty)\n");
        return;
    }

    long count = hashtablecount(htable);
    if (count == 0) {
        printf("(empty)\n");
        return;
    }

    printf("Workspace variables (%ld):\n", count);

    // Iterate and display
    hdlhashnode nomad = (**htable).hashfirstnode;
    while (nomad != nil) {
        bigstring name;
        tyvaluerecord val;

        gethashkey(nomad, name);
        hashgetvaluerecord(nomad, &val);

        // Print name
        printf("  %.*s = ", (int)stringlength(name), stringbaseaddress(name));

        // Print value summary
        switch (val.valuetype) {
            case novaluetype:
                printf("nil");
                break;
            case booleanvaluetype:
                printf("%s", val.data.flvalue ? "true" : "false");
                break;
            case longvaluetype:
                printf("%ld", val.data.longvalue);
                break;
            case stringvaluetype: {
                bigstring bs;
                copyheapstring(val.data.stringvalue, bs);
                printf("\"%.*s\"", (int)stringlength(bs), stringbaseaddress(bs));
                break;
            }
            case externalvaluetype:
                printf("[external]");
                break;
            default: {
                // Use coercetostring for other types
                tyvaluerecord val_copy = val;
                if (coercetostring(&val_copy)) {
                    bigstring bs;
                    copyheapstring(val_copy.data.stringvalue, bs);
                    printf("%.*s", (int)stringlength(bs), stringbaseaddress(bs));
                    disposevaluerecord(val_copy, false);
                } else {
                    printf("[type %d]", val.valuetype);
                }
                break;
            }
        }
        printf("\n");

        nomad = (**nomad).hashlink;
    }
}
```

---

### Special Commands (repl_commands.c)

Commands use `/` prefix to avoid conflicts with UserTalk syntax (leading `/` is invalid in UserTalk, so it's a safe namespace).

```c
// repl_commands.h
#ifndef REPL_COMMANDS_H
#define REPL_COMMANDS_H

#include "../Common/headers/frontier.h"
#include "repl.h"

// Execute a REPL command
// Returns false if command is /exit or fatal error
boolean repl_command_execute(repl_state_t* state, const char* command);

#endif // REPL_COMMANDS_H
```

```c
// repl_commands.c - Implementation
boolean repl_command_execute(repl_state_t* state, const char* command) {
    if (command == nil || command[0] != '/') {
        return true;  // Not a command
    }

    // Skip leading '/'
    const char* cmd = command + 1;

    // /exit - Exit REPL
    if (strcmp(cmd, "exit") == 0) {
        return false;
    }

    // /help - Show available commands
    if (strcmp(cmd, "help") == 0) {
        printf("Available commands:\n");
        printf("  /exit          Exit the REPL\n");
        printf("  /help          Show this help message\n");
        printf("  /clear         Clear workspace variables\n");
        printf("  /vars          Show workspace variables\n");
        printf("\n");
        printf("Multi-line input:\n");
        printf("  Use backslash (\\) at end of line to continue on next line\n");
        printf("\n");
        printf("Examples:\n");
        printf("  workspace.x = 42\n");
        printf("  workspace.x * 2\n");
        printf("  local(sum = 0); \\\n");
        printf("    for i = 1 to 10 { sum = sum + i }; \\\n");
        printf("    return sum\n");
        return true;
    }

    // /clear - Clear workspace
    if (strcmp(cmd, "clear") == 0) {
        if (repl_eval_clear_workspace(state)) {
            printf("Workspace cleared\n");
        } else {
            printf("Error: Failed to clear workspace\n");
        }
        return true;
    }

    // /vars - Show workspace variables
    if (strcmp(cmd, "vars") == 0) {
        repl_output_table_summary(state->workspace);
        return true;
    }

    // /load <file> - Load and execute UserTalk file (Phase 3)
    if (strncmp(cmd, "load ", 5) == 0) {
        const char* filename = cmd + 5;
        printf("Error: /load not yet implemented (Phase 3)\n");
        // TODO: Implement in Phase 3
        return true;
    }

    // Unknown command
    printf("Unknown command: /%s\n", cmd);
    printf("Type /help for available commands\n");
    return true;
}
```

---

### System Root Initialization

The REPL should automatically load the system root database with the following priority:

1. `--system-root <path>` command line flag (highest priority)
2. `FRONTIER_SYSTEM_ROOT` environment variable
3. `./Frontier.root` or `./frontier.root` (case-insensitive search in cwd)
4. `Frontier.root` in same directory as executable (matches legacy behavior)
5. No system root loaded (continue without - silent fallback)

**Migration behavior**: Like `db.open()`, look for v7 migrated root first (`.root7` suffix), fall back to plain `.root` file if not present, and auto-migrate if needed.

```c
// In main.c - System root auto-loading
static char* find_system_root(void) {
    static char path_buf[1024];

    // 1. Check --system-root flag (already handled by cli_parser)
    if (g_cli_options.system_root != NULL) {
        return g_cli_options.system_root;
    }

    // 2. Check FRONTIER_SYSTEM_ROOT environment variable
    const char* env_root = getenv("FRONTIER_SYSTEM_ROOT");
    if (env_root != NULL && strlen(env_root) > 0) {
        // Check if file exists (try .root7 first, then .root)
        snprintf(path_buf, sizeof(path_buf), "%s.root7", env_root);
        if (access(path_buf, R_OK) == 0) {
            return path_buf;
        }
        snprintf(path_buf, sizeof(path_buf), "%s", env_root);
        if (access(path_buf, R_OK) == 0) {
            return path_buf;
        }
        snprintf(path_buf, sizeof(path_buf), "%s.root", env_root);
        if (access(path_buf, R_OK) == 0) {
            return path_buf;
        }
    }

    // 3. Check ./Frontier.root or ./frontier.root in cwd
    const char* cwd_candidates[] = {
        "Frontier.root7", "Frontier.root",
        "frontier.root7", "frontier.root",
        NULL
    };
    for (int i = 0; cwd_candidates[i] != NULL; i++) {
        if (access(cwd_candidates[i], R_OK) == 0) {
            return (char*)cwd_candidates[i];
        }
    }

    // 4. Check Frontier.root in executable directory
    char exe_path[1024];
    if (realpath(g_argv0, exe_path) != NULL) {
        char* last_slash = strrchr(exe_path, '/');
        if (last_slash != NULL) {
            *last_slash = '\0';  // Truncate to directory
            snprintf(path_buf, sizeof(path_buf), "%s/Frontier.root7", exe_path);
            if (access(path_buf, R_OK) == 0) {
                return path_buf;
            }
            snprintf(path_buf, sizeof(path_buf), "%s/Frontier.root", exe_path);
            if (access(path_buf, R_OK) == 0) {
                return path_buf;
            }
        }
    }

    // 5. No system root found - return NULL (silent fallback)
    return NULL;
}
```

---

### Multi-line Input Handling (Phase 2)

Multi-line input uses backslash continuation (like shell). Simple, predictable, explicit.

```c
// repl_input.h (Phase 2)
#ifndef REPL_INPUT_H
#define REPL_INPUT_H

#include "../Common/headers/frontier.h"
#include "repl.h"

// Initialize input subsystem (readline, history)
boolean repl_input_init(repl_state_t* state);

// Read a line (or multi-line with backslash continuation)
// Returns true on success, false on EOF or error
// Caller must free *input when done
boolean repl_input_readline(repl_state_t* state, char** input);

// Cleanup input subsystem
void repl_input_cleanup(repl_state_t* state);

#endif // REPL_INPUT_H
```

```c
// repl_input.c - Multi-line handling
boolean repl_input_readline(repl_state_t* state, char** input) {
    // Use readline (or libedit on macOS) for line editing
    #ifdef HAVE_READLINE
        char* prompt = "> ";  // Use repl_output_prompt for initial line
        char* line = readline(prompt);

        if (line == NULL) {
            // EOF (Ctrl-D)
            *input = NULL;
            return false;
        }

        // Check for backslash continuation
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\\') {
            // Multi-line mode - accumulate lines
            size_t total_len = len - 1;  // Remove backslash
            char* buffer = malloc(total_len + 1);
            memcpy(buffer, line, len - 1);
            buffer[total_len] = '\0';
            free(line);

            while (true) {
                // Read continuation line
                line = readline("      > ");
                if (line == NULL) {
                    // EOF during multi-line - return partial input
                    *input = buffer;
                    return true;
                }

                len = strlen(line);
                boolean has_continuation = (len > 0 && line[len - 1] == '\\');
                size_t line_len = has_continuation ? len - 1 : len;

                // Reallocate buffer and append
                buffer = realloc(buffer, total_len + line_len + 2);  // +2 for newline + null
                buffer[total_len] = '\n';
                memcpy(buffer + total_len + 1, line, line_len);
                total_len += line_len + 1;
                buffer[total_len] = '\0';
                free(line);

                if (!has_continuation) {
                    break;  // Done with multi-line input
                }
            }

            // Add to history
            if (strlen(buffer) > 0) {
                add_history(buffer);
            }

            *input = buffer;
            return true;
        }

        // Single-line input - add to history
        if (strlen(line) > 0) {
            add_history(line);
        }

        *input = line;
        return true;
    #else
        // Fallback: Use fgets without readline
        char line_buf[8192];
        if (fgets(line_buf, sizeof(line_buf), stdin) == NULL) {
            *input = NULL;
            return false;
        }

        // Remove trailing newline
        size_t len = strlen(line_buf);
        if (len > 0 && line_buf[len - 1] == '\n') {
            line_buf[len - 1] = '\0';
            len--;
        }

        // Check for backslash continuation (similar logic as above)
        // ... (implement fallback multi-line handling)

        *input = strdup(line_buf);
        return true;
    #endif
}
```

---

### Error Handling

**Design Principle**: Errors should be informative but not crash the REPL.

**Error Types**:
1. **Compilation errors** - Syntax errors, show line/column if available
2. **Runtime errors** - Division by zero, undefined variable, type mismatch
3. **System errors** - Out of memory, database corruption
4. **Command errors** - Unknown command, invalid arguments

**Error Display Strategy**:
```
Error: Can't find a sub-table named "foo"
  in: workspace.foo.bar
       ^~~~~~~~~~~~~~~

Error: Syntax error at line 2, column 5
  1> local(x = 5);
  2> x +
         ^
  3> return x

Error: /load requires a filename
  Usage: /load <filename>
```

**Implementation**:
- Use `langgeterrorstring()` for compilation/runtime errors (existing mechanism)
- Display line numbers for multi-line input (track line offsets during accumulation)
- For command errors, print usage hint

---

## Command Line Interface

### Mode Detection Logic

```c
// In main.c - Determine interactive vs batch mode
typedef enum {
    MODE_BATCH,        // Execute script and exit
    MODE_INTERACTIVE,  // REPL loop
    MODE_SERVER        // Network server (Phase 4)
} execution_mode_t;

execution_mode_t detect_execution_mode(int argc, char* argv[]) {
    // Force modes
    if (has_flag("--interactive")) {
        return MODE_INTERACTIVE;
    }
    if (has_flag("--batch")) {
        return MODE_BATCH;
    }
    if (has_flag("--server")) {
        return MODE_SERVER;
    }

    // Batch triggers
    if (has_flag("-e") || has_flag("--execute")) {
        return MODE_BATCH;
    }
    if (has_flag("-f") || has_flag("--file")) {
        return MODE_BATCH;
    }
    if (!isatty(STDIN_FILENO)) {
        // Stdin is redirected (e.g., frontier-cli < script.ut)
        return MODE_BATCH;
    }

    // Default to interactive if TTY present
    if (isatty(STDIN_FILENO) && isatty(STDOUT_FILENO)) {
        return MODE_INTERACTIVE;
    }

    // No TTY and no batch flags - error
    fprintf(stderr, "Error: No execution mode specified\n");
    fprintf(stderr, "Use -e for inline script, -f for file, or run interactively with a TTY\n");
    exit(1);
}
```

### Command Line Flags

**Existing flags (preserved)**:
- `-e, --execute SCRIPT` - Execute inline UserTalk script (batch mode)
- `--system-root PATH` - Load system root database
- `--upgrade-system-root` - Upgrade system root to v7 format
- `--output-json` - Output results in JSON format (batch mode only)
- `-v, --verbose` - Verbose output
- `--debug` - Debug mode
- `-h, --help` - Show help message
- `--version` - Show version information

**New flags (Phase 1-3)**:
- `--interactive` - Force interactive mode (even without TTY, for testing)
- `--batch` - Force batch mode (even with TTY)
- `--no-system-root` - Don't auto-load system root (explicit opt-out)
- `-f, --file FILENAME` - Execute UserTalk file (batch mode, Phase 3)

**New flags (Phase 4)**:
- `--server` - Run as network server
- `--port PORT` - Server port (default: 8080)
- `--bind ADDRESS` - Server bind address (default: 127.0.0.1)

### Environment Variables

**Existing**:
- `FRONTIER_LOG_LEVEL` - Set log level (TRACE, DEBUG, INFO, WARN, ERROR)
- `FRONTIER_LOG_COMPONENT` - Filter logs by component
- `FRONTIER_HEADLESS_RUN_STARTUP` - Run system.startup scripts (default: skip)

**New**:
- `FRONTIER_SYSTEM_ROOT` - Path to system root database (priority 2 in lookup)
- `FRONTIER_REPL_HISTORY` - Path to command history file (default: `~/.frontier/history`)

### Help Text

```
Frontier CLI - Command Line Interface for UserTalk Script Execution
Version 1.0.0 (2026-01-12)

Usage: frontier-cli [OPTIONS] [SCRIPT_FILE]

Execution Modes:
  Interactive Mode (default with TTY):
    frontier-cli                            # Start REPL
    frontier-cli --system-root db.root      # REPL with custom system root

  Batch Execution:
    frontier-cli -e "1+1"                   # Execute inline script
    frontier-cli -f script.ut               # Execute script file
    frontier-cli < script.ut                # Read from stdin

Options:
  -e, --execute SCRIPT     Execute inline UserTalk script (batch mode)
  -f, --file FILENAME      Execute UserTalk file (batch mode)
  --system-root PATH       Load system root database before execution
  --no-system-root         Don't auto-load system root database
  --upgrade-system-root    Upgrade system root to v7 format (use with --system-root)
  --output-json            Output results in JSON format (batch mode only)
  --interactive            Force interactive mode (even without TTY)
  --batch                  Force batch mode (even with TTY)
  -v, --verbose            Verbose output
  --debug                  Debug mode
  -h, --help               Show this help message
  --version                Show version information

Environment Variables:
  FRONTIER_SYSTEM_ROOT              Path to system root database
  FRONTIER_REPL_HISTORY             Command history file (default: ~/.frontier/history)
  FRONTIER_LOG_LEVEL                Log level (TRACE, DEBUG, INFO, WARN, ERROR)
  FRONTIER_LOG_COMPONENT            Filter logs by component (DB, HASH, LANG, etc.)
  FRONTIER_HEADLESS_RUN_STARTUP     Run system.startup scripts (default: skip)

Interactive Mode Commands:
  /exit          Exit the REPL
  /help          Show available commands
  /clear         Clear workspace variables
  /vars          Show workspace variables
  /load <file>   Execute UserTalk file in REPL context (Phase 3)

Multi-line Input:
  Use backslash (\) at end of line to continue on next line
  Example:
    [root]> local(sum = 0); \
          > for i = 1 to 10 { sum = sum + i }; \
          > return sum

Examples:
  # Start REPL with default system root
  frontier-cli

  # REPL with custom system root
  frontier-cli --system-root databases/Frontier-v6.root

  # Batch execution
  frontier-cli -e "workspace.x = 42; workspace.x * 2"
  frontier-cli -f myscript.ut
  cat myscript.ut | frontier-cli

  # Upgrade database
  frontier-cli --system-root old.root --upgrade-system-root

For detailed documentation, see: docs/REPL_INTERACTIVE_MODE_DESIGN.md
```

---

## Special Commands

### Phase 1 Commands

| Command | Description | Implementation Priority |
|---------|-------------|------------------------|
| `/exit` | Exit REPL | P0 (required) |
| `/help` | Show available commands and usage | P0 (required) |
| `/clear` | Clear workspace variables | P0 (required) |
| `/vars` | Show workspace variables with values | P0 (required) |

### Phase 2 Commands

No new commands in Phase 2 (focus on multi-line input and history).

### Phase 3 Commands

| Command | Description | Implementation Priority |
|---------|-------------|------------------------|
| `/load <file>` | Execute UserTalk file in REPL context | P1 (Phase 3) |
| `/save <file>` | Save workspace to UserTalk file | P2 (future) |
| `/pwd` | Show current working directory | P2 (future) |
| `/cd <path>` | Change current working directory | P2 (future) |

### Phase 4 Commands

No new terminal commands in Phase 4 (focus on server mode abstraction).

### Future Commands (Post-Phase 4)

| Command | Description | Implementation Priority |
|---------|-------------|------------------------|
| `/context <path>` | Change current context (for prompt and target.get) | P2 (context switching) |
| `/reset` | Reset runtime (reload system root, clear workspace) | P3 (advanced) |
| `/history` | Show command history | P3 (convenience) |
| `/debug on\|off` | Toggle debug logging | P3 (convenience) |
| `/trace on\|off` | Toggle trace logging | P3 (convenience) |
| `/bench <code>` | Benchmark UserTalk code execution | P4 (performance testing) |

---

## Testing Strategy

### Automated Testing

**Challenge**: Interactive mode is inherently interactive - how do we test it automatically?

**Solution**: Stdin simulation with expect-like assertions.

**Test Framework**:
```bash
# test_repl.sh - REPL test harness
#!/bin/bash

run_repl_test() {
    local test_name=$1
    local input_file=$2
    local expected_output=$3

    echo "Running test: $test_name"

    # Run frontier-cli with stdin from input file
    actual_output=$(cat "$input_file" | ./frontier-cli --batch 2>&1)

    # Compare output (ignore prompt lines)
    diff <(echo "$expected_output") <(echo "$actual_output" | grep -v "^\[") > /dev/null

    if [ $? -eq 0 ]; then
        echo "  ✓ PASSED"
        return 0
    else
        echo "  ✗ FAILED"
        echo "  Expected: $expected_output"
        echo "  Got:      $actual_output"
        return 1
    fi
}

# Test 1: Basic arithmetic
cat > /tmp/test_repl_1.ut <<EOF
1+1
/exit
EOF
run_repl_test "basic_arithmetic" "/tmp/test_repl_1.ut" "2"

# Test 2: Workspace persistence
cat > /tmp/test_repl_2.ut <<EOF
workspace.x = 42
workspace.x * 2
/exit
EOF
run_repl_test "workspace_persistence" "/tmp/test_repl_2.ut" "42\n84"

# Test 3: Multi-line input
cat > /tmp/test_repl_3.ut <<EOF
local(sum = 0); \
for i = 1 to 10 { sum = sum + i }; \
return sum
/exit
EOF
run_repl_test "multiline_input" "/tmp/test_repl_3.ut" "55"
```

**Integration with Existing Test Suite**:
- Add `tests/repl_tests.c` for unit tests of REPL components
- Add `tests/integration/repl/` directory for scripted REPL tests
- Add `make test-repl` target to Makefile

**Unit Test Coverage**:
- `test_repl_workspace_lifecycle()` - Create, use, clear workspace
- `test_repl_eval_execute()` - Compile and execute code
- `test_repl_commands()` - Each `/command` individually
- `test_repl_output_formatting()` - Value display for each type
- `test_repl_multiline_accumulation()` - Backslash continuation parsing

---

### Manual Testing Scenarios

**Scenario 1: Basic REPL usage**
```bash
$ ./frontier-cli
Frontier REPL - Interactive UserTalk Environment
Type /help for commands, /exit to quit

[root]> 1+1
2
[root]> "Hello, " + "world!"
"Hello, world!"
[root]> /exit
Goodbye!
```

**Scenario 2: Workspace persistence**
```bash
[root]> workspace.x = 42
42
[root]> workspace.y = workspace.x * 2
84
[root]> workspace.x + workspace.y
126
[root]> /vars
Workspace variables (2):
  x = 42
  y = 84
[root]> /clear
Workspace cleared
[root]> /vars
(empty)
```

**Scenario 3: Multi-line input**
```bash
[root]> local(sum = 0); \
      > for i = 1 to 10 { \
      >   sum = sum + i \
      > }; \
      > return sum
55
```

**Scenario 4: Error recovery**
```bash
[root]> workspace.x = 1 / 0
Error: Division by zero
[root]> workspace.x = 42
42
[root]> workspace.y
Error: Can't find a variable named "y"
```

**Scenario 5: System root auto-loading**
```bash
# With Frontier.root in current directory
$ ls
Frontier.root  frontier-cli

$ ./frontier-cli
Frontier REPL - Interactive UserTalk Environment
Type /help for commands, /exit to quit

[root]> sizeOf(system)
45
[root]> system.paths.frontier
"/Users/jake/Frontier.root"
```

**Scenario 6: Multi-session state isolation**
```bash
# Terminal 1
$ ./frontier-cli
[root]> workspace.x = 100
100

# Terminal 2
$ ./frontier-cli
[root]> workspace.x
Error: Can't find a variable named "x"
# (workspace is per-session, not shared across REPL instances)
```

---

## Future Considerations

### Server Mode (Phase 4)

**Design Goal**: Enable remote REPL access over network with minimal changes to evaluation engine.

**Architecture**:
```
┌───────────────────────┐
│  REPL Client (CLI)    │
│  - Terminal I/O       │
│  - Local display      │
└───────────┬───────────┘
            │ TCP/TLS
┌───────────▼───────────┐
│  REPL Server          │
│  - Accept connections │
│  - Authentication     │
│  - Session management │
└───────────┬───────────┘
            │
┌───────────▼───────────┐
│  REPL Evaluator       │
│  - Same as terminal   │
│  - No I/O dependencies│
└───────────────────────┘
```

**Protocol Design** (JSON-RPC 2.0):
```json
// Request
{
  "jsonrpc": "2.0",
  "method": "eval",
  "params": {
    "code": "workspace.x = 42"
  },
  "id": 1
}

// Success response
{
  "jsonrpc": "2.0",
  "result": {
    "value": "42",
    "type": "long"
  },
  "id": 1
}

// Error response
{
  "jsonrpc": "2.0",
  "error": {
    "code": -32000,
    "message": "Can't find a variable named \"y\""
  },
  "id": 1
}
```

**Authentication**: Optional token-based auth (disabled by default for local development).

**Session Management**: Each client connection gets its own workspace table (isolated state).

**Security Considerations**:
- Bind to `127.0.0.1` by default (localhost only)
- Require explicit `--bind 0.0.0.0` to expose to network
- Rate limiting and request size limits
- Consider TLS for production use

---

### Context Switching

**Design Goal**: Allow users to change the "current context" for evaluations and prompt display.

**Example**:
```bash
[root]> /context system.compiler
[system.compiler]> language.version
"Frontier 1.0"
[system.compiler]> /context root
[root]>
```

**Implementation**:
- Update `repl_state_t.context_table` and `repl_state_t.context_path`
- Set `currenthashtable` to context table before evaluation
- `target.get()` returns context path as address
- Prompt displays context path

**Challenge**: How to navigate back up tree? Use `..` notation or `/cd` command?

---

### Tab Completion

**Design Goal**: Auto-complete variable names, table paths, and commands.

**Implementation**:
- Integrate with readline's completion API (`rl_attempted_completion_function`)
- On Tab press, enumerate workspace variables and system table paths
- Complete `/commands` with list of available commands

**Example**:
```bash
[root]> works<TAB>
[root]> workspace.
[root]> workspace.x<TAB>
[root]> workspace.x  # (completed if x exists)
[root]> /cl<TAB>
[root]> /clear
```

---

### External Editor Integration

**Design Goal**: Allow editing multi-line scripts in external editor (like bash `set -o vi`).

**Implementation**:
- Readline supports this via `edit-and-execute-command` (Ctrl-X Ctrl-E)
- Configure `EDITOR` environment variable to `$EDITOR` or `vim` by default
- On Ctrl-X Ctrl-E, open temp file in editor, load result on exit

**Example**:
```bash
[root]> <Ctrl-X Ctrl-E>
# Editor opens with empty file
# User writes multi-line script, saves, exits
[root]> <script executes automatically>
```

---

### Debugging Integration

**Design Goal**: Integrate with UserTalk debugger (step, breakpoints, inspect stack).

**Challenge**: Frontier's debugger is GUI-based (Mac app). Headless debugging requires text-based alternative.

**Approach** (future work):
- Add `/debug on` command to enable step-by-step execution
- Display current line, stack trace, local variables
- Commands: `/step`, `/continue`, `/break <line>`, `/inspect <var>`

---

### Collaborative REPL (Frontier 2.0 Vision)

**Design Goal**: Multi-user REPL sessions editing shared ODB.

**Architecture**:
- REPL server supports multiple concurrent connections
- Each connection gets isolated workspace, but can access shared ODB
- CRDT-based conflict resolution for concurrent edits
- Real-time updates broadcast to all connected clients

**Out of Scope**: This is post-1.0 work aligned with collaborative ODB vision.

---

## Migration Path

### Backward Compatibility

**Guaranteed**:
- All existing batch modes continue to work unchanged
- `frontier-cli -e "code"` - Same behavior
- `frontier-cli --system-root db.root -e "code"` - Same behavior
- Environment variables preserved
- Exit codes preserved (0 = success, 1 = error)

**New Behavior**:
- `frontier-cli` (no args, with TTY) - Enters REPL (previously showed help)
- `frontier-cli < script.ut` - Batch execution (previously not supported)

**Documentation Updates**:
- Update `docs/CLI_USAGE_GUIDE.md` with REPL section
- Update `docs/TESTING_GUIDE.md` with REPL testing patterns
- Add `docs/REPL_TUTORIAL.md` for new users

---

### Deprecation Strategy

**Nothing Deprecated**: All existing functionality remains available.

**Future Consideration** (post-1.0):
- May add `--no-repl` flag to force old behavior (show help instead of entering REPL)
- May deprecate `--hydrate-system-root` in favor of automatic hydration

---

### User Communication

**Release Notes** (Phase 1):
```
Frontier CLI 1.1.0 - Interactive REPL Mode
===========================================

NEW: Interactive REPL
- Run `frontier-cli` without arguments to enter interactive mode
- Persistent workspace for variables across evaluations
- Multi-line input with backslash continuation
- Special commands: /exit, /help, /clear, /vars
- Command history saved to ~/.frontier/history
- Automatic system root loading (Frontier.root in current directory)

ENHANCED: Batch Execution
- New `-f <file>` flag to execute UserTalk files
- Stdin redirection support: `frontier-cli < script.ut`
- Better error messages with line numbers

MIGRATION: No breaking changes
- All existing scripts and workflows continue to work
- REPL is opt-in (only when running without arguments)

See docs/REPL_TUTORIAL.md for getting started guide.
```

---

## Implementation Checklist

### Phase 1: Basic REPL (Weeks 1-2)

- [ ] Create `frontier-cli/repl.h` and `frontier-cli/repl.c`
- [ ] Create `frontier-cli/repl_eval.h` and `frontier-cli/repl_eval.c`
- [ ] Create `frontier-cli/repl_output.h` and `frontier-cli/repl_output.c`
- [ ] Create `frontier-cli/repl_commands.h` and `frontier-cli/repl_commands.c`
- [ ] Modify `frontier-cli/main.c` for mode detection and REPL initialization
- [ ] Modify `frontier-cli/cli_parser.c` for new flags (`--interactive`, `--no-system-root`)
- [ ] Implement workspace table creation in system.repl.workspace
- [ ] Implement basic value display using `coercetostring()`
- [ ] Implement commands: `/exit`, `/help`, `/clear`, `/vars`
- [ ] Implement system root auto-loading with priority order
- [ ] Add unit tests in `tests/repl_tests.c`
- [ ] Add integration tests in `tests/integration/repl/`
- [ ] Update `docs/CLI_USAGE_GUIDE.md`
- [ ] Create `docs/REPL_TUTORIAL.md`

### Phase 2: Multi-line and History (Week 3)

- [ ] Create `frontier-cli/repl_input.h` and `frontier-cli/repl_input.c`
- [ ] Integrate readline/libedit for line editing
- [ ] Implement backslash continuation for multi-line input
- [ ] Implement persistent history in `~/.frontier/history`
- [ ] Enhance value formatting for tables and complex types
- [ ] Add tests for multi-line input parsing
- [ ] Update documentation with multi-line examples

### Phase 3: File Operations (Week 4)

- [ ] Add `-f <file>` flag to cli_parser.c
- [ ] Implement stdin redirection detection and batch execution
- [ ] Implement `/load <file>` command
- [ ] Improve table display (structure summary instead of "[table]")
- [ ] Add file operation tests
- [ ] Update documentation with file execution examples

### Phase 4: Server Mode Prep (Weeks 5-6)

- [ ] Create `frontier-cli/repl_io.h` - Abstract I/O interface
- [ ] Create `frontier-cli/repl_io_terminal.c` - Terminal implementation
- [ ] Create `frontier-cli/repl_io_network.c` - Network stub
- [ ] Create `frontier-cli/repl_server.c` - TCP server
- [ ] Design JSON-RPC protocol (or MessagePack)
- [ ] Create `docs/REPL_NETWORK_PROTOCOL.md`
- [ ] Refactor REPL loop to use abstract I/O layer
- [ ] Add `--server` flag and server mode to main.c
- [ ] Implement basic authentication framework (disabled by default)
- [ ] Add server tests (local socket, basic requests)
- [ ] Update documentation with server mode guide

---

## Appendices

### Appendix A: Research Findings

**1. Value Display Primitive**

Frontier uses `coercetostring()` in `Common/source/langvalue.c` (lines 3046+) to convert values to strings. This function handles:
- Basic types: boolean, char, int, long, double, date
- Special types: OSType, direction, point, rect, RGB, pattern
- Heap types: string (already string), external (address or summary)

**Key Insight**: We can reuse `coercetostring()` for most value display. Special handling needed for:
- Tables: Show `[table with N items]` instead of full dump
- Binaries: Show `[binary, N bytes]`
- Nil: Don't print anything (like Python REPL)

**2. Target Context Handling**

`target.get()` in `Common/source/langverbs.c` (line 1454) returns the current target as an address value. In GUI Frontier, this is the frontmost window's variable. In headless mode, we need to:
- Define "current target" as REPL context table (default: workspace)
- Return address to context table when `target.get()` is called
- Allow `/context <path>` to change target (future)

**Key Insight**: `langgettarget()` and `langsettarget()` are the low-level primitives. REPL should maintain its own target state in `repl_state_t.context_table`.

**3. Readline Integration**

Frontier doesn't currently use readline. File operations use `headless_readline()` for line-oriented file reading, but this is unrelated to terminal input.

**Key Insight**: We need to add readline/libedit dependency. On macOS, prefer libedit (ships with OS). On Linux, prefer readline (GPL-compatible).

**Build Integration**:
```makefile
# In frontier-cli/Makefile
LIBS += -lreadline  # or -ledit on macOS
```

**Autodetection**:
```c
#ifdef HAVE_READLINE
    #include <readline/readline.h>
    #include <readline/history.h>
#else
    // Fallback to fgets
#endif
```

---

### Appendix B: Alternative Designs Considered

**Alternative 1: Implicit Multi-line (Like Python)**

**Rejected**: Python's REPL uses implicit continuation (unclosed brackets, trailing operators). This requires complex parsing and is error-prone. Explicit backslash is simpler and more predictable.

**Alternative 2: Global Workspace (Like Node.js)**

**Rejected**: Node.js REPL pollutes global scope with REPL variables. Frontier has a rich table system - using a dedicated `workspace` table is cleaner and allows inspection (`/vars`), clearing (`/clear`), and future export (`/save`).

**Alternative 3: Session Persistence (Save to Disk)**

**Rejected for Phase 1**: Saving workspace to disk between REPL sessions adds complexity (file format, versioning, corruption handling). Phase 1 uses ephemeral workspace (cleared on exit). Future enhancement: `/save workspace.ut` to export workspace.

**Alternative 4: Inline Command Mode (Like IPython `!`)**

**Rejected**: IPython uses `!cmd` for shell commands. Frontier doesn't need shell integration (file operations are UserTalk verbs). Using `/` for REPL commands is cleaner and less ambiguous.

---

### Appendix C: Glossary

**REPL**: Read-Eval-Print Loop - Interactive programming environment
**Workspace**: Persistent table for REPL variables (`system.repl.workspace`)
**Context**: Current table scope for evaluations and prompt display
**Batch Mode**: Non-interactive execution (script file or inline code)
**Continuation**: Multi-line input using backslash at line end
**Readline**: GNU library for line editing and history (or libedit on macOS)
**TTY**: Teletype - Terminal device (used for interactive detection)
**JSON-RPC**: JSON Remote Procedure Call - Protocol for server mode

---

### Appendix D: References

**Internal Documentation**:
- `docs/CLI_USAGE_GUIDE.md` - Existing CLI documentation
- `docs/TESTING_GUIDE.md` - Testing patterns and practices
- `docs/VERB_IMPLEMENTATION_GUIDE.md` - Kernel verb implementation
- `planning/CRDT_FOUNDATION_ROADMAP.md` - Collaborative ODB vision

**Codebase References**:
- `frontier-cli/main.c` - CLI entry point and initialization
- `frontier-cli/cli_parser.c` - Command line argument parsing
- `portable/cli_executor.c` - Batch execution engine
- `Common/source/langvalue.c` - Value system and `coercetostring()`
- `Common/source/langverbs.c` - `target.get()` and `target.set()`
- `Common/source/lang.c` - `langcompiletext()` and `langrunscriptcode()`

**External References**:
- GNU Readline: https://tiswww.case.edu/php/chet/readline/rltop.html
- Libedit (BSD): https://thrysoee.dk/editline/
- JSON-RPC 2.0: https://www.jsonrpc.org/specification

---

**Document History**:
- 2026-01-12: Initial design document (Phase 1-4 specification)

---

**Approval Status**: Draft - Pending TPM/CTO Review

---

**End of Document**
