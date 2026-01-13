# MCP Integration Architecture

**Status**: Technical Design
**Date**: 2026-01-13
**Dependencies**: ADR-007 (REST API), Phase 4 (Networking)

## Overview

Frontier's MCP (Model Context Protocol) integration provides bidirectional AI integration:

1. **Frontier as MCP Server** - Expose local workflows to AI clients (Claude Desktop, etc.)
2. **Frontier as MCP Client** - Connect to remote MCP servers from UserTalk scripts

**Protocol**: [MCP Specification](https://spec.modelcontextprotocol.io/) (Anthropic)

---

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│  AI Clients (Claude Desktop, VS Code + MCP, custom apps)    │
│  - Send MCP requests (list tools, call tools, get resources)│
│  - Receive MCP responses (tool results, resource content)   │
└─────────────────────────────────────────────────────────────┘
                           ↓
                    MCP Protocol (stdio)
                           ↓
┌─────────────────────────────────────────────────────────────┐
│  Frontier MCP Server (frontier-cli --mcp-server)             │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ MCP Protocol Handler                                   │  │
│  │ - Parse JSON-RPC requests                             │  │
│  │ - Route to tool/resource handlers                     │  │
│  │ - Serialize responses                                 │  │
│  └───────────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ Tool Registry                                          │  │
│  │ - Built-in tools (file ops, ODB, processes)           │  │
│  │ - Custom tools (user-defined UserTalk scripts)        │  │
│  │ - Remote tools (proxy to other MCP servers)           │  │
│  └───────────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ Resource Provider                                      │  │
│  │ - ODB tables as MCP resources                         │  │
│  │ - File system as resources                            │  │
│  │ - Dynamic resource generation                         │  │
│  └───────────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ Security Layer                                         │  │
│  │ - Permission checks (what can AI access?)             │  │
│  │ - Audit logging (what did AI do?)                     │  │
│  │ - Sandbox mode (read-only, safe commands only)        │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────┐
│  Frontier Runtime                                            │
│  - Execute UserTalk code                                     │
│  - Access ODB (read/write tables)                           │
│  - Call system verbs (file.*, string.*, etc.)               │
│  - MCP Client (connect to remote servers)                   │
└─────────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────┐
│  Local System                                                │
│  - File system (read, write, watch)                          │
│  - Processes (run scripts, scheduled tasks)                  │
│  - Applications (macOS AppleScript, Windows PowerShell, etc.)│
└─────────────────────────────────────────────────────────────┘
```

---

## MCP Server Implementation

### 1. Protocol Transport

**stdio-based transport** (Claude Desktop standard):
- frontier-cli reads JSON-RPC from stdin
- Writes JSON-RPC responses to stdout
- stderr for error logging (not protocol messages)

**Example configuration** (`~/.config/claude/config.json`):
```json
{
  "mcpServers": {
    "frontier": {
      "command": "/usr/local/bin/frontier-cli",
      "args": ["--mcp-server", "--system-root", "~/Frontier.root"],
      "env": {
        "FRONTIER_MCP_PERMISSIONS": "~/.frontier/mcp_permissions.json"
      }
    }
  }
}
```

### 2. MCP Message Types

**Client → Server Requests**:

```json
// List available tools
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "tools/list",
  "params": {}
}

// Call a tool
{
  "jsonrpc": "2.0",
  "id": 2,
  "method": "tools/call",
  "params": {
    "name": "odb_insert",
    "arguments": {
      "path": "research.papers",
      "data": {"title": "MCP Paper", "year": 2026}
    }
  }
}

// Get resource content
{
  "jsonrpc": "2.0",
  "id": 3,
  "method": "resources/read",
  "params": {
    "uri": "odb://system.paths"
  }
}
```

**Server → Client Responses**:

```json
// Tool list response
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "tools": [
      {
        "name": "odb_insert",
        "description": "Insert data into ODB table",
        "inputSchema": {
          "type": "object",
          "properties": {
            "path": {"type": "string", "description": "ODB table path"},
            "data": {"type": "object", "description": "Data to insert"}
          },
          "required": ["path", "data"]
        }
      }
    ]
  }
}

// Tool call response
{
  "jsonrpc": "2.0",
  "id": 2,
  "result": {
    "content": [
      {
        "type": "text",
        "text": "Inserted 1 item into research.papers"
      }
    ]
  }
}
```

### 3. Built-in Tool Categories

**File Operations** (`file_*` tools):
- `file_read` - Read file content
- `file_write` - Write file content
- `file_list` - List directory contents
- `file_watch` - Watch for file changes (register callback)
- `file_delete` - Delete file/directory

**ODB Operations** (`odb_*` tools):
- `odb_get` - Get value from ODB path
- `odb_set` - Set value at ODB path
- `odb_insert` - Insert item into table
- `odb_delete` - Delete item from table
- `odb_query` - Query ODB with filter

**Process Operations** (`process_*` tools):
- `process_run` - Run shell command
- `process_schedule` - Schedule recurring task
- `process_list` - List running processes

**Application Integration** (`app_*` tools):
- `app_applescript` - Run AppleScript (macOS)
- `app_powershell` - Run PowerShell (Windows)
- `app_dbus` - Call DBus method (Linux)

**Outline Operations** (`outline_*` tools):
- `outline_create` - Create new outline
- `outline_insert` - Insert headline
- `outline_get` - Get outline content
- `outline_export` - Export to OPML/JSON

### 4. Custom Tool Registration

Users can define custom tools in UserTalk:

**UserTalk script** (`system.verbs.mcp.tools.myCustomTool`):
```usertalk
on tool_myCustomTool(paramsTable) {
  // Tool implementation in UserTalk
  local(input = paramsTable.input);
  local(result = string.upper(input));

  return {
    "content": [{
      "type": "text",
      "text": "Uppercased: " + result
    }]
  }
}
```

**Tool metadata** (`system.verbs.mcp.metadata.myCustomTool`):
```usertalk
{
  "name": "myCustomTool",
  "description": "Convert text to uppercase",
  "inputSchema": {
    "type": "object",
    "properties": {
      "input": {"type": "string"}
    }
  }
}
```

**Discovery**: MCP server scans `system.verbs.mcp.tools.*` at startup

---

## MCP Client Implementation

### 1. UserTalk API for Calling Remote MCP Tools

**Connect to remote MCP server**:
```usertalk
// Connect to Slack MCP server
local(slackServer = mcp.connect("slack", {
  "command": "/usr/local/bin/slack-mcp-server",
  "args": ["--token", system.env.SLACK_TOKEN]
}));
```

**Call remote tool**:
```usertalk
// Post message to Slack via MCP
local(result = mcp.call(slackServer, "post_message", {
  "channel": "#general",
  "text": "Automation completed!"
}));

if result.success {
  msg("Posted to Slack")
} else {
  error("Slack post failed: " + result.error)
}
```

**List available tools**:
```usertalk
local(tools = mcp.listTools(slackServer));
for tool in tools {
  msg(tool.name + ": " + tool.description)
}
```

### 2. Pre-built Connector Library

**Slack Connector** (`system.verbs.mcp.connectors.slack`):
```usertalk
on slack_post(channel, text) {
  local(server = mcp.getOrConnect("slack"));
  return mcp.call(server, "post_message", {
    "channel": channel,
    "text": text
  })
}

on slack_listen(channel, handler) {
  local(server = mcp.getOrConnect("slack"));
  return mcp.subscribe(server, "channel_messages", {
    "channel": channel,
    "callback": handler
  })
}
```

**GitHub Connector** (`system.verbs.mcp.connectors.github`):
```usertalk
on github_createIssue(repo, title, body) {
  local(server = mcp.getOrConnect("github"));
  return mcp.call(server, "create_issue", {
    "repo": repo,
    "title": title,
    "body": body
  })
}

on github_listCommits(repo, since) {
  local(server = mcp.getOrConnect("github"));
  return mcp.call(server, "list_commits", {
    "repo": repo,
    "since": since
  })
}
```

---

## Resource Providers

### 1. ODB Resources

**URI Scheme**: `odb://path.to.table`

**Example resources**:
- `odb://system.paths` - System paths table
- `odb://workspace` - Current workspace
- `odb://research.papers` - User's research database

**Resource metadata**:
```json
{
  "uri": "odb://research.papers",
  "name": "Research Papers Database",
  "mimeType": "application/x-frontier-odb",
  "description": "Collection of research papers with metadata"
}
```

**Resource content** (read):
```json
{
  "contents": [
    {
      "uri": "odb://research.papers",
      "mimeType": "application/json",
      "text": "{\"items\": [{\"title\": \"MCP Paper\", \"year\": 2026}]}"
    }
  ]
}
```

### 2. File System Resources

**URI Scheme**: `file:///absolute/path`

**Example resources**:
- `file:///Users/jake/Documents` - Directory listing
- `file:///Users/jake/notes.txt` - File content

**Dynamic resource templates**:
- `file:///{workspace}/*` - All files in workspace directory
- `file:///~/Research/*.pdf` - All PDFs in Research folder

---

## Security Model

### 1. Permission System

**Permission file** (`~/.frontier/mcp_permissions.json`):
```json
{
  "default": "deny",
  "rules": [
    {
      "tool": "file_*",
      "allow": ["~/Documents/*", "~/Research/*"],
      "deny": ["~/.ssh/*", "~/.aws/*"]
    },
    {
      "tool": "odb_*",
      "allow": ["workspace", "research.*", "projects.*"],
      "deny": ["system.verbs.*"]
    },
    {
      "tool": "process_run",
      "allow": ["git", "python3", "node"],
      "deny": ["rm", "sudo", "curl"]
    }
  ]
}
```

**Permission check flow**:
1. AI requests tool call
2. MCP server checks permission rules
3. If denied: return error "Permission denied: tool X on path Y"
4. If allowed: execute tool, log action

### 2. Audit Logging

**Audit log** (`~/.frontier/mcp_audit.log`):
```
2026-01-13T10:30:15Z [ALLOWED] tool=odb_insert path=research.papers user=claude
2026-01-13T10:30:20Z [DENIED]  tool=file_delete path=~/.ssh/id_rsa user=claude
2026-01-13T10:30:25Z [ALLOWED] tool=process_run cmd=git args=[status] user=claude
```

**Audit query API**:
```usertalk
// Get all denied actions from last hour
local(denied = mcp.audit.query({
  "action": "DENIED",
  "since": date.now() - (60 * 60)
}));
```

### 3. Sandbox Mode

**Read-only mode** (`--mcp-server --sandbox`):
- Only allow read operations (file_read, odb_get)
- Deny write operations (file_write, odb_set, process_run)
- Useful for demos, untrusted AI clients

**Safe commands whitelist**:
- Allow: `git status`, `ls`, `cat`, `grep`
- Deny: `rm`, `sudo`, `curl`, `wget`, `ssh`

---

## Implementation Phases

### Phase 1: Basic MCP Server (4 weeks)

**Week 1-2: Protocol Implementation**
- [ ] JSON-RPC parser/serializer
- [ ] stdio transport (read from stdin, write to stdout)
- [ ] Message routing (tools/list, tools/call, resources/read)
- [ ] Error handling

**Week 3: Core Tools**
- [ ] `file_read`, `file_write`, `file_list`
- [ ] `odb_get`, `odb_set`, `odb_insert`
- [ ] `process_run`

**Week 4: Claude Desktop Integration**
- [ ] Test with Claude Desktop
- [ ] Configuration guide
- [ ] Example workflows
- [ ] Debug logging

**Deliverable**: Claude can read/write local ODB via MCP

### Phase 2: Tool Library Expansion (3 weeks)

**Week 1: Advanced File Tools**
- [ ] `file_watch` - Watch for changes
- [ ] `file_search` - Search file contents
- [ ] `file_sync` - Sync directories

**Week 2: Advanced ODB Tools**
- [ ] `odb_query` - Filter/search ODB
- [ ] `odb_export` - Export to JSON/CSV
- [ ] `odb_backup` - Backup table

**Week 3: Process & App Tools**
- [ ] `process_schedule` - Cron-like scheduling
- [ ] `app_applescript` - macOS automation
- [ ] `app_powershell` - Windows automation

**Deliverable**: 30+ built-in tools covering common workflows

### Phase 3: Custom Tools & Resources (2 weeks)

**Week 1: Custom Tool API**
- [ ] Tool registration from UserTalk
- [ ] Metadata schema validation
- [ ] Discovery mechanism
- [ ] Example custom tools

**Week 2: Resource Providers**
- [ ] ODB resource provider
- [ ] File system resource provider
- [ ] Resource templates
- [ ] Resource metadata

**Deliverable**: Users can define custom MCP tools in UserTalk

### Phase 4: MCP Client (3 weeks)

**Week 1: Client Protocol**
- [ ] `mcp.connect()` API
- [ ] `mcp.call()` API
- [ ] `mcp.listTools()` API
- [ ] Connection management

**Week 2: Pre-built Connectors**
- [ ] Slack connector
- [ ] GitHub connector
- [ ] Email connector (SMTP/IMAP)

**Week 3: OAuth & Auth**
- [ ] OAuth flow handling
- [ ] Token storage
- [ ] Token refresh

**Deliverable**: UserTalk can call any public MCP server

### Phase 5: Security & Polish (2 weeks)

**Week 1: Security**
- [ ] Permission system
- [ ] Audit logging
- [ ] Sandbox mode

**Week 2: Production Readiness**
- [ ] Error messages
- [ ] Documentation
- [ ] Performance testing
- [ ] Security audit

**Deliverable**: Production-ready MCP server

---

## Testing Strategy

### Unit Tests

**C layer tests** (`tests/headless_mcp_tests.c`):
- JSON-RPC parsing/serialization
- Tool registry lookup
- Permission checking
- Resource resolution

**UserTalk tests** (`tests/integration/mcp_tests.yaml`):
```yaml
tests:
  - name: "MCP tool call - odb_insert"
    script: |
      local(result = mcp.test_tool("odb_insert", {
        "path": "test.items",
        "data": {"name": "Test"}
      }));
      return result.success
    expected_success: true

  - name: "MCP permission denied"
    script: |
      local(result = mcp.test_tool("file_delete", {
        "path": "~/.ssh/id_rsa"
      }));
      return result.error contains "Permission denied"
    expected_success: true
```

### Integration Tests

**Claude Desktop end-to-end**:
1. Start frontier-cli --mcp-server
2. Send MCP request via stdin
3. Verify response on stdout
4. Check audit log

**Multi-tool workflows**:
1. file_read → odb_insert → process_run → file_write
2. Verify each step works
3. Check atomic rollback on failure

### Performance Tests

**Throughput**:
- 100 tool calls/second target
- Measure latency (p50, p95, p99)
- Memory usage under load

**Concurrent clients**:
- 10 Claude Desktop instances
- All calling Frontier MCP server
- Verify no race conditions

---

## Reference Implementation

### frontier-cli MCP Server Mode

**Command line**:
```bash
frontier-cli --mcp-server \
  --system-root ~/Frontier.root \
  --permissions ~/.frontier/mcp_permissions.json \
  --audit-log ~/.frontier/mcp_audit.log
```

**Startup sequence**:
1. Load system root ODB
2. Parse permissions file
3. Register built-in tools
4. Discover custom tools in system.verbs.mcp.tools.*
5. Enter stdio read loop
6. Process MCP requests indefinitely

**Example tool implementation** (C):
```c
// File: frontier-cli/mcp_tools.c

static boolean mcp_tool_odb_insert(tyvaluerecord *params, tyvaluerecord *result) {
    // Extract path and data from params
    bigstring path;
    if (!hashtablelookup(params, BIGSTRING("path"), &vpath, NULL))
        return false;
    pullstringvalue(&vpath, path);

    // Get data to insert
    tyvaluerecord vdata;
    if (!hashtablelookup(params, BIGSTRING("data"), &vdata, NULL))
        return false;

    // Check permissions
    if (!mcp_check_permission("odb_insert", path))
        return mcp_error("Permission denied", result);

    // Insert into ODB
    hdlhashtable htable;
    if (!odb_get_table(path, &htable))
        return mcp_error("Table not found", result);

    if (!hashtableinsert(htable, &vdata))
        return mcp_error("Insert failed", result);

    // Log audit entry
    mcp_audit_log("ALLOWED", "odb_insert", path);

    // Return success
    return mcp_success("Inserted 1 item", result);
}
```

---

## Related Documents

- [AI Automation Platform Vision](AI_AUTOMATION_PLATFORM_VISION.md) - Product context
- [Kickstarter Campaign Plan](KICKSTARTER_CAMPAIGN.md) - Go-to-market strategy
- [ADR-007: REST API](../architectural_decision_records/ADR-007-rest-api-via-frontier-web-server.md) - Related architecture
- [MCP Specification](https://spec.modelcontextprotocol.io/) - Protocol spec

---

**Last Updated**: 2026-01-13
**Status**: Technical design - ready for prototyping
**Next Steps**: Phase 1 implementation (4 weeks)
