# Frontier GUI IDE with AI Integration

**Status**: Vision Document
**Date**: 2026-01-13
**Context**: Modern GUI successor to classic Frontier IDE

## Historical Context

### What Made Classic Frontier Special

**The Original Vision (1992)**:
- Unified environment: outline editor, script editor, object database browser
- Live scripting: Modify code while it's running
- Cross-app automation: AppleScript/OSA integration
- Everything in one place: Data + code + tools

**What Was Lost**:
- Classic Mac only (never truly worked on Windows)
- Single-user only (no collaboration)
- Closed ecosystem (can't integrate with modern tools)
- Aging UI paradigms

**What We Can Restore**:
- Cross-app automation (now via MCP instead of OSA)
- Unified environment (but modernized)
- Live scripting (but with AI assistance)
- Everything in one place (but collaborative)

---

## The Modern GUI IDE Vision

### Core Principle: "AI-Native IDE for Structured Data"

**Not a code editor** (VS Code exists)
**Not just an outline editor** (Workflowy exists)
**Not just a database GUI** (TablePlus exists)

**Instead**: A unified workspace where AI helps you build with structured data and automation

---

## Key Features

### 1. AI-Powered Outline Editor

**Classic Frontier**: Manual outline organization
**Modern Frontier**: AI suggests structure

**Example workflow**:
```
You: [Paste 50 research paper abstracts]
AI: "I see 5 themes. Should I organize by:
     A) Topic (ML, NLP, Vision)
     B) Year (2024, 2025, 2026)
     C) Methodology (Supervised, Unsupervised, RL)"
You: "A, then sort each by year"
AI: [Reorganizes instantly]
```

**AI Features**:
- **Smart summarization**: Collapse outlines to key points
- **Auto-categorization**: Drag items → AI suggests categories
- **Semantic search**: "Find all items about neural architecture"
- **Duplicate detection**: "These 3 items say the same thing"

---

### 2. AI Script Assistant (Copilot-Style)

**Classic Frontier**: Write UserTalk by hand
**Modern Frontier**: AI writes it with you

**Inline autocomplete**:
```usertalk
// You type:
email.on

// AI suggests:
email.onReceive(@handler) {
  // Extract PDF attachments
  local(pdfs = email.getAttachments(msg, "pdf"));

  // For each PDF, extract invoice data
  for pdf in pdfs {
    local(data = pdf.extractInvoice(pdf));
    odb.insert(@accounting.invoices, data);
  }
}
```

**AI code generation**:
- Right-click script → "Explain this code"
- Select script → "Add error handling"
- Comment: `// TODO: Add rate limiting` → AI implements it
- Natural language: "Create a script that backs up my research outline daily"

**AI debugging**:
- Runtime error → "Why did this fail?"
- AI: "Line 15 assumes msg.body is a string, but it's nil. Add: `if msg.body == nil { return }`"
- Slow script → "Why is this slow?"
- AI: "You're calling db.query() in a loop (line 22). Hoist it outside: [shows refactored code]"

---

### 3. Visual ODB Browser with AI Queries

**Classic Frontier**: Browse tables manually
**Modern Frontier**: AI helps you find what you need

**Natural language queries**:
```
You: "Show me all projects from last quarter that went over budget"
AI: [Generates ODB query, shows results in table view]

You: "Which customers haven't ordered in 6 months?"
AI: [Generates query with date math, shows results]

You: "Create a report of my top 10 expenses by category"
AI: [Generates outline-style report with charts]
```

**AI schema assistance**:
- Adding new table → AI suggests structure based on existing data
- Import CSV → AI detects column types and suggests ODB schema
- Inconsistent data → AI flags: "These 12 entries have mismatched types"

---

### 4. Cross-App Automation Builder (Modern OSA)

**Classic Frontier**: AppleScript integration (Mac only)
**Modern Frontier**: MCP integration (cross-platform)

**Visual workflow builder**:
```
┌─────────────────────────────────────────────────┐
│  When: Email arrives with "invoice" in subject  │
│  ↓                                               │
│  Then: Extract PDF attachment                   │
│  ↓                                               │
│  Then: Parse invoice data (AI)                  │
│  ↓                                               │
│  Then: Save to accounting.invoices ODB table    │
│  ↓                                               │
│  Then: Post summary to Slack #accounting        │
└─────────────────────────────────────────────────┘
```

**Behind the scenes**: UserTalk + MCP
**User sees**: Visual blocks (Zapier-style)
**Advanced users**: Click "Show Code" → edit UserTalk directly

**AI assistance**:
- "Add step: If amount > $1000, email CFO"
- AI inserts conditional block with email tool
- "Make this more robust"
- AI adds error handling, retries, logging

---

### 5. Collaborative Editing (The New Frontier)

**Classic Frontier**: Single user, one Mac
**Modern Frontier**: Multiple users, real-time sync

**Collaborative outline editing**:
- Google Docs-style cursors showing who's editing what
- Live updates as others type
- Conflict resolution (CRDT-based)
- Comments/annotations on outline items

**Collaborative scripting**:
- Multiple people can edit same script (conflict resolution)
- Code review inline (like GitHub PR comments)
- "Watch mode" - see what others are changing live

**Shared ODB**:
- Multiple users accessing same database
- Fine-grained permissions (read/write per table)
- Audit log (who changed what, when)

**AI-mediated collaboration**:
- "Merge Alice's changes with mine (prefer mine for conflicts)"
- "Summarize what Bob changed in last hour"
- "Review this code before I merge it" → AI does code review

---

### 6. Live Preview/Testing

**Classic Frontier**: Run script, see output in message window
**Modern Frontier**: Interactive preview pane

**For outlines**:
- Preview rendered HTML/markdown
- Preview as blog post (with styles)
- Preview as presentation slides

**For scripts**:
- Watch mode: See variables update in real-time as script runs
- Breakpoints + AI debugging: "Why is totalCost wrong?"
- Time-travel debugging: Step backward through execution

**For automations**:
- Test mode: Simulate email arriving, see workflow run
- Dry run: "Show me what this would do, don't actually do it"
- Replay: "Re-run this automation with different input"

---

## UI/UX Design Principles

### 1. Progressive Disclosure

**Novice users**: Visual workflow builder, AI chat interface
**Power users**: REPL, direct ODB editing, UserTalk scripts
**Everyone**: Can grow from novice → power user naturally

### 2. Context-Aware AI

**AI knows what you're looking at**:
- Outline focused → AI suggests organization improvements
- Script focused → AI provides autocomplete
- ODB focused → AI helps with queries
- Automation focused → AI suggests optimizations

### 3. Everything is Scriptable

**Classic Frontier's strength**: The IDE itself was scriptable
**Modern Frontier**: AI makes this accessible

**Example**:
```
You: "Add a menu command to export this outline as PDF"
AI: "I'll create that for you. Where should it save?"
You: "Ask me each time"
AI: [Generates UserTalk script, adds to menus table]
```

### 4. Local-First, Cloud-Optional

**Data stays on your machine** (MCP provides AI access)
**Cloud sync optional** (for collaboration)
**Self-hosted option** (run your own server)

---

## Technology Stack

### Frontend

**Framework**: Electron (cross-platform) or Tauri (lighter)
**Why**: Works on macOS, Windows, Linux
**Alternative**: Native Swift (macOS) + Qt (Windows/Linux)

**UI Library**: React or Svelte
**Why**: Modern, component-based, good AI tooling

**Editor**: Monaco (VS Code's editor) or CodeMirror
**Why**: Syntax highlighting, autocomplete integration

**Outline View**: Custom React component
**Why**: Need full control for collaborative cursors, drag-drop

### Backend

**Core**: frontier-cli (C runtime)
**API**: REST API (Phase 4) + MCP server
**Database**: ODB (Frontier's native format)

**Collaboration**: CRDT (Yjs or Automerge)
**Why**: Conflict-free replicated data types for real-time sync

### AI Integration

**Autocomplete**: Anthropic API (Claude) or OpenAI (GPT-4)
**Chat assistant**: Same as autocomplete
**Local models**: Option to run Llama/Mistral locally (privacy)

**MCP Client**: Built into Electron app
**Why**: AI can access local files, ODB, automations

---

## Development Phases

### Phase 1: Foundation (3 months)

**Goal**: Basic IDE with outline editor + script editor

**Deliverables**:
- Electron app shell
- ODB browser (read-only)
- Outline editor (basic)
- UserTalk script editor (Monaco)
- Connect to frontier-cli backend

**Not yet**: AI, collaboration, automation builder

---

### Phase 2: AI Integration (2 months)

**Goal**: AI-powered autocomplete and chat assistant

**Deliverables**:
- Anthropic API integration
- Inline autocomplete in script editor
- Chat panel ("Ask AI")
- Code explanation/generation
- Natural language ODB queries

---

### Phase 3: Automation Builder (2 months)

**Goal**: Visual workflow builder using MCP

**Deliverables**:
- Drag-drop workflow canvas
- MCP tool palette (file ops, ODB, email, Slack, etc.)
- Test mode (dry run workflows)
- Generated UserTalk (show code)

---

### Phase 4: Collaboration (3 months)

**Goal**: Real-time collaborative editing

**Deliverables**:
- CRDT integration (Yjs)
- Multiplayer outline editing
- Multiplayer script editing
- Presence indicators (cursors, typing)
- Comments/annotations

---

### Phase 5: Polish (2 months)

**Goal**: Production-ready release

**Deliverables**:
- Performance optimization
- Theme/customization
- Keyboard shortcuts
- Documentation
- Installer packages

**Total**: 12 months (1 year)

---

## Cross-Platform Automation (Modern OSA)

### The OSA Legacy

**What Frontier Did (Mac)**:
- AppleScript integration
- Control any Mac app (Mail, Finder, Safari, etc.)
- Record actions → generate scripts
- System-wide scripting

**Why It Never Worked on Windows**:
- Windows had no equivalent to AppleScript
- COM automation was complex, inconsistent
- No unified scripting architecture

### The MCP Opportunity

**MCP as Modern OSA**:
- Cross-platform (works on Mac, Windows, Linux)
- Standard protocol (unlike AppleScript/COM)
- App-agnostic (any app can expose MCP tools)
- AI-native (LLMs understand MCP)

**How It Works**:

**On macOS**:
```usertalk
// Open URL in Safari
app.safari.openURL("https://example.com")

// Behind the scenes: MCP call to Safari MCP server
// (which wraps AppleScript/JXA)
```

**On Windows**:
```usertalk
// Same API!
app.edge.openURL("https://example.com")

// Behind the scenes: MCP call to Edge MCP server
// (which wraps PowerShell/COM)
```

**On Linux**:
```usertalk
// Same API!
app.firefox.openURL("https://example.com")

// Behind the scenes: MCP call to Firefox MCP server
// (which wraps DBus)
```

### Building the Connector Ecosystem

**Phase 1**: Core OS integration
- macOS: Wrap AppleScript/JXA in MCP server
- Windows: Wrap PowerShell/COM in MCP server
- Linux: Wrap DBus in MCP server

**Phase 2**: Popular apps
- Browser automation (Chrome, Firefox, Safari, Edge)
- Email clients (Mail, Outlook, Thunderbird)
- Office apps (Word, Excel, PowerPoint, LibreOffice)
- Development tools (VS Code, Terminal, Git)

**Phase 3**: Community connectors
- Open protocol → anyone can build MCP servers
- Marketplace for connectors (like Zapier integrations)
- AI can learn new connectors dynamically

---

## Visual Design Concepts

### Layout

```
┌─────────────────────────────────────────────────────────┐
│  Frontier IDE                                     [•••]  │
├─────────────────────────────────────────────────────────┤
│  [📁 File] [🔧 Edit] [▶️  Run] [🤖 AI] [👥 Share]      │
├──────────┬──────────────────────────────┬───────────────┤
│          │                              │               │
│  ODB     │  Outline Editor              │  AI Assistant │
│  Browser │  ┌────────────────────────┐  │               │
│          │  │ • Research Papers      │  │  💬 Ask me    │
│  system  │  │   • ML (15)            │  │  anything...  │
│  ├─verbs │  │   • NLP (8)            │  │               │
│  ├─paths │  │   • Vision (12)        │  │  Suggestions: │
│  workspace│  │ • Project Ideas       │  │  • Organize   │
│  ├─notes │  │   • Phase 1           │  │    outline     │
│  ├─tasks │  │   • Phase 2           │  │  • Generate   │
│  research │  │ • Code Snippets       │  │    summary    │
│  ├─papers│  └────────────────────────┘  │  • Find       │
│  ├─notes │                              │    duplicates  │
│          │  [Script Editor]             │               │
│          │  ```                         │               │
│          │  on processPapers() {        │               │
│          │    // AI autocomplete here   │               │
│          │  }                           │               │
│          │  ```                         │               │
│          │                              │               │
└──────────┴──────────────────────────────┴───────────────┘
```

### Theme: Modern + Clean

**Inspiration**: VS Code, Notion, Linear
**Not**: Cluttered, enterprise software, 1990s Mac

**Colors**:
- Light mode: Clean whites, soft grays
- Dark mode: True blacks, subtle highlights
- Accent: Purple (AI features), Blue (links/nav)

---

## Competitive Positioning

### vs VS Code + Extensions

**VS Code**: Amazing code editor, extensible
**Frontier GUI**: Structured data editor with scripting

**Use VS Code for**: Programming languages (Python, JS, Rust)
**Use Frontier for**: Outlines + automation + data management

**Can coexist**: Edit UserTalk in VS Code if you prefer!

### vs Notion

**Notion**: Great for outlines, wikis, docs
**Frontier GUI**: Outlines + scripting + automation + local-first

**Migration path**: Import Notion workspace → Frontier ODB

### vs Zapier GUI

**Zapier**: Visual workflow builder (cloud only)
**Frontier GUI**: Visual builder + local access + AI code generation

**Use Zapier for**: Cloud-to-cloud integrations (if you must)
**Use Frontier for**: Local workflows + custom logic + owned data

---

## Go-to-Market Strategy

### Phase 1: CLI + MCP (Kickstarter)

**Target**: Developers, Claude Desktop users
**Deliverable**: frontier-cli with MCP server
**Timeline**: 6 months (August 2026)

### Phase 2: GUI IDE Alpha (Invite-Only)

**Target**: Early adopters from Kickstarter
**Deliverable**: Electron app (outline + script editor + AI)
**Timeline**: +6 months (February 2027)

### Phase 3: GUI IDE Beta (Public)

**Target**: Knowledge workers, automation enthusiasts
**Deliverable**: Collaboration features, automation builder
**Timeline**: +6 months (August 2027)

### Phase 4: GUI IDE 1.0 (Launch)

**Target**: General public, teams, enterprises
**Deliverable**: Polish, marketplace, mobile companion
**Timeline**: +3 months (November 2027)

---

## Success Metrics

### GUI IDE Adoption

**Year 1**:
- 5K active GUI users
- 50K automations created in GUI
- 1K community-contributed MCP connectors

**Year 2**:
- 50K active GUI users
- 500K automations created
- 10K paid teams

**Year 3**:
- 500K active GUI users
- Standard tool in productivity stacks
- Acquisition interest

---

## Related Documents

- [AI Automation Platform Vision](AI_AUTOMATION_PLATFORM_VISION.md) - CLI/MCP vision
- [MCP Integration Architecture](MCP_INTEGRATION_ARCHITECTURE.md) - Technical foundation
- [Kickstarter Campaign](KICKSTARTER_CAMPAIGN.md) - Go-to-market for CLI

---

**Last Updated**: 2026-01-13
**Status**: Vision document - follows CLI/MCP launch
**Timeline**: GUI development starts Q3 2026 (after CLI ships)
