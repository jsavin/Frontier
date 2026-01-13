# Frontier AI: Your Personal Automation Co-Pilot

**Status**: Vision Document
**Date**: 2026-01-13
**Owner**: Product Strategy

## Executive Summary

Frontier AI is an AI-powered automation platform that bridges the gap between local workflows and cloud APIs. Unlike Zapier (cloud-only) or GitHub Copilot (code-only), Frontier combines:

- **Local automation** - Access your files, apps, and scripts
- **Cloud integration** - Connect to remote APIs via MCP protocol
- **AI code generation** - Natural language → working UserTalk scripts
- **Structured data** - ODB as the automation state store

**Target Market**: Claude Desktop power users, knowledge workers, developers wanting personal automation

**Competitive Edge**: Only platform that gives Claude (or any AI) access to local workflows via MCP protocol

---

## The Problem

### Current State Pain Points

**Zapier/Make.com**:
- ❌ Cloud APIs only - can't touch local files or apps
- ❌ $600+/year for decent tier
- ❌ GUI workflow builders (inflexible, hard to version control)
- ❌ No AI assistance

**Claude Desktop**:
- ✅ AI-powered assistance
- ✅ MCP protocol support
- ❌ Limited to approved MCP servers
- ❌ No local workflow automation out of the box

**Custom Scripts**:
- ✅ Full control, local access
- ❌ No AI assistance
- ❌ Brittle (breaks when APIs change)
- ❌ Hard to maintain

**GitHub Copilot/Cursor**:
- ✅ AI code generation
- ❌ Only helps write code, doesn't run it
- ❌ No automation orchestration

---

## The Solution

### Frontier AI = Local Workflows + MCP + AI Code Generation

**Three Core Capabilities**:

1. **Frontier as MCP Server** - Expose local workflows to AI
   - UserTalk verbs → MCP tools
   - ODB tables → MCP resources
   - File system, processes, local apps all accessible
   - Claude Desktop can orchestrate your entire digital life

2. **Frontier as MCP Client** - Connect to cloud APIs
   - Use existing MCP servers (Slack, GitHub, email, etc.)
   - UserTalk scripts can call remote tools
   - Best of both worlds: local power + cloud reach

3. **AI Code Generation** - Natural language → automation
   - Chat with AI: "When emails arrive with invoices..."
   - AI writes UserTalk script
   - Test, refine, deploy - all via conversation
   - REPL autocomplete for manual scripting

---

## User Workflows

### Example 1: Email Invoice Processing

**User asks Claude Desktop**:
> "When emails arrive with PDF invoices, extract the data, save to my accounting database, and notify my bookkeeper on Slack"

**What happens** (via MCP):
```
Claude Desktop (MCP client)
  ↓ calls "email.watch" tool
Frontier MCP Server
  ↓ UserTalk: email.onReceive(@handler)
  ↓ Email arrives, handler triggered
  ↓ calls "pdf.extract" tool
Frontier MCP Server
  ↓ calls "odb.insert" tool
Frontier MCP Server (save to accounting.root)
  ↓ calls "slack.post" tool
Slack MCP Server (remote)
  ↓ Success
Claude: "Done! Watching for invoice emails."
```

**Time saved**: 2 min/invoice × 50 invoices/month = 100 min/month

### Example 2: Git Commit Digest

**User asks Claude**:
> "Give me a daily digest of my Git commits, grouped by project"

**What happens**:
```
Claude → frontier.git.log (last 24h)
      → frontier.odb.query (project mapping)
      → frontier.outline.create (formatted digest)
      → frontier.email.send (to user)
      → frontier.schedule.daily (repeat tomorrow)
```

**Result**: Every morning, email with categorized commit summary

### Example 3: Research Paper Workflow

**User asks Claude**:
> "When I save a PDF to ~/Research, extract key points, add to my research outline, and find related papers"

**What happens**:
```
Claude → frontier.fs.watch ("~/Research")
      → frontier.pdf.parse (extract text)
      → claude.summarize (AI generates key points)
      → frontier.outline.insert (@research.papers)
      → semantic_scholar.search (via MCP)
      → frontier.outline.insert (related papers)
```

**Time saved**: 15 min/paper × 10 papers/month = 150 min/month

---

## Product Architecture

### Component Stack

```
┌─────────────────────────────────────────────────┐
│  AI Clients (Claude Desktop, VS Code, etc.)     │
│  - MCP client protocol                           │
│  - Natural language interface                    │
└─────────────────────────────────────────────────┘
                    ↓ MCP protocol
┌─────────────────────────────────────────────────┐
│  Frontier MCP Server (frontier-cli --mcp-server) │
│  - Expose UserTalk verbs as MCP tools            │
│  - Expose ODB as MCP resources                   │
│  - Security/permissions layer                    │
└─────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────┐
│  Frontier Runtime                                │
│  - UserTalk execution engine                     │
│  - ODB (structured data store)                   │
│  - Local integrations (files, apps, processes)   │
│  - Remote MCP client (connect to other servers)  │
└─────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────┐
│  Local System                                    │
│  - File system (watch, read, write)              │
│  - Applications (macOS/Windows/Linux apps)       │
│  - Processes (run scripts, scheduled jobs)       │
└─────────────────────────────────────────────────┘
```

### Key Technical Decisions

**1. MCP as Primary Protocol**
- Standard protocol (Anthropic spec)
- Works with Claude Desktop today
- Future-proof (other AI clients will adopt)
- No custom protocol maintenance

**2. UserTalk as Automation Language**
- Simpler than JavaScript/Python for non-programmers
- AI can generate it easily (well-defined syntax)
- Integrated with ODB (no ORM needed)
- Proven for 30+ years

**3. ODB as State Store**
- Structured data (not just files)
- Collaborative (multiple clients)
- Scriptable (UserTalk native)
- Versioned (built-in history)

---

## Market Positioning

### Target Audiences

**Primary: Claude Desktop Power Users** (20K-50K users)
- Already using Claude Desktop
- Hit limitations with local workflows
- Technical enough to appreciate code generation
- Willing to pay for productivity

**Secondary: Knowledge Workers** (broader market)
- Drowning in repetitive tasks
- Not programmers but willing to learn
- Email/document/research heavy workflows
- Monthly subscription fatigue

**Tertiary: Developer Teams** (future)
- Shared automation workflows
- Team ODB for config/state
- Custom internal tools
- Enterprise licensing

### Competitive Matrix

|                      | Frontier AI | Zapier | n8n | Claude Desktop | Scripts |
|----------------------|-------------|--------|-----|----------------|---------|
| Local file access    | ✅          | ❌     | ❌  | ⚠️ (limited)   | ✅      |
| Cloud API access     | ✅          | ✅     | ✅  | ⚠️ (limited)   | ⚠️      |
| AI code generation   | ✅          | ❌     | ❌  | ✅             | ⚠️      |
| MCP protocol native  | ✅          | ❌     | ❌  | ✅             | ❌      |
| Structured data      | ✅          | ❌     | ⚠️  | ❌             | ❌      |
| Self-hosted          | ✅          | ❌     | ✅  | N/A            | ✅      |
| Price (annual)       | $99         | $600+  | Free| $20/month      | Free    |

### Unique Value Propositions

1. **"Claude can finally automate your local workflows"**
   - Only MCP server that exposes local system access
   - Turn any UserTalk script into a Claude tool
   - Bridge gap between AI and your computer

2. **"Zapier + AI, for 1/6th the price"**
   - Cloud APIs + local access
   - AI writes the automation code
   - Self-hosted option (no vendor lock)

3. **"Your workflows, your data, your server"**
   - No cloud vendor required
   - ODB on your machine
   - Export to any format

---

## Kickstarter Campaign Strategy

### Campaign Positioning

**Tagline**: "Your local workflows, now Claude-native"

**Pitch**:
"Claude Desktop is powerful but can't touch your computer. Zapier connects cloud APIs but costs $600/year and has no AI. Frontier AI gives you both: local automation + cloud APIs + AI code generation, for $99/year."

**Campaign Goal**: $50K (fund 6 months development)

### Reward Tiers

| Tier | Price | Rewards | Target |
|------|-------|---------|--------|
| Early Bird | $49 | First year included, AI autocomplete | 200 backers |
| Standard | $99 | Lifetime desktop license + 1 year hosted AI | 300 backers |
| Pro | $199 | Lifetime + unlimited AI + priority support | 100 backers |
| Team | $499 | Team (5 users), shared automations | 20 backers |
| Enterprise | $2000 | Custom connectors + white-label | 5 backers |

**Revenue**: (200×$49) + (300×$99) + (100×$199) + (20×$499) + (5×$2000) = $79K

### Stretch Goals

- **$100K**: Mobile companion app (trigger automations on mobile)
- **$200K**: Visual workflow builder (for non-coders)
- **$500K**: AI marketplace (share/sell automation scripts)

### Demo Video Script (90 seconds)

**Scene 1** (15 sec): The Problem
- Show overflowing inbox
- Manual downloading attachments
- Copy-pasting data
- Updating spreadsheets
- "I waste 2 hours/day on this"

**Scene 2** (15 sec): Current Solutions Fall Short
- Zapier: "Can't access local files" ❌
- Claude Desktop: "Can't automate workflows" ❌
- Custom scripts: "Break constantly, no AI help" ❌

**Scene 3** (45 sec): Frontier AI Solution
- Install Frontier: `brew install frontier-cli`
- Configure Claude Desktop (show config.json)
- Chat with Claude: "Automate my invoice workflow"
- Claude: "I'll create that. Testing... works! Enable?"
- Show automation running: Email → Extract → Save → Notify
- "That's 2 hours/day back. What will YOU automate?"

**Scene 4** (15 sec): Call to Action
- "Join 500+ early backers"
- "Starting at $49"
- "Ship date: August 2026"
- Link to Kickstarter

---

## Development Roadmap

### Phase 1: MCP Server Foundation (2-3 months)
**Goal**: Frontier as MCP server, basic tools, Claude Desktop integration

**Deliverables**:
- MCP server implementation (stdio transport)
- Core tools: file ops, ODB access, process management
- Claude Desktop config guide
- 10 example workflows

**Success Metric**: Claude Desktop can read/write local ODB via Frontier

### Phase 2: Tool Library Expansion (1-2 months)
**Goal**: Rich tool library for common workflows

**Deliverables**:
- File watchers (trigger on file changes)
- Email integration (IMAP/SMTP)
- PDF/document parsing
- Custom tool registration (UserTalk → MCP tool)

**Success Metric**: 50 pre-built tools, 5 user-contributed tools

### Phase 3: MCP Client + Connectors (2-3 months)
**Goal**: Connect to remote MCP servers, Zapier parity

**Deliverables**:
- MCP client in UserTalk
- Pre-built connectors: Slack, GitHub, Google APIs
- OAuth flow handling
- Rate limiting, retry logic

**Success Metric**: UserTalk can call any public MCP server

### Phase 4: AI Autocomplete (1-2 months)
**Goal**: REPL autocomplete, AI-assisted scripting

**Deliverables**:
- Anthropic API integration
- Context-aware suggestions in REPL
- Code explanation/debugging
- Pattern learning

**Success Metric**: 80% of autocomplete suggestions accepted

### Phase 5: Polish + Launch (1 month)
**Goal**: Production-ready, Kickstarter fulfillment

**Deliverables**:
- Security hardening
- Documentation
- Installer packages (macOS/Linux/Windows)
- Kickstarter backer delivery

**Success Metric**: 95% backer satisfaction

---

## Success Metrics

### Launch Metrics (Month 1)
- 500+ Kickstarter backers
- $50K+ funding
- 1000+ email signups

### Early Adoption (Months 2-6)
- 2000+ active users
- 500+ custom automations created
- 50+ contributed MCP tools
- 90% retention rate

### Growth (Months 7-12)
- 10K+ active users
- $500K ARR
- 1000+ automations in marketplace
- Partnerships with MCP server providers

### Long-term (Year 2+)
- 100K+ users
- $5M ARR
- Standard tool in "AI agent stack"
- Acquisition interest from automation platforms

---

## Risk Analysis

### Technical Risks

**MCP Protocol Changes**
- Risk: Anthropic changes MCP spec
- Mitigation: Track spec changes, maintain backward compat layer

**AI API Costs**
- Risk: Anthropic API pricing increases
- Mitigation: Support multiple providers (OpenAI, local models), user BYO API key option

**Security/Permissions**
- Risk: MCP gives AI too much local access
- Mitigation: Fine-grained permission system, audit logs, sandbox mode

### Market Risks

**Claude Desktop Adoption**
- Risk: Claude Desktop usage stagnates
- Mitigation: Support other MCP clients (VS Code, custom), standalone value prop

**Zapier Adds MCP**
- Risk: Zapier implements MCP protocol
- Mitigation: Local access advantage, self-hosted option, pricing edge

**Open Source Competition**
- Risk: Someone builds open source equivalent
- Mitigation: Focus on UX, AI quality, hosted service, support

---

## Next Steps

1. **Validate with Users** (Week 1-2)
   - Survey Claude Desktop community
   - Interview 20 potential users
   - Refine value prop based on feedback

2. **Build MCP Prototype** (Week 3-6)
   - Implement basic MCP server
   - 5 core tools (file read/write, ODB access, process run)
   - Test with Claude Desktop

3. **Create Demo** (Week 7-8)
   - Record 90-second video
   - Build 3 example workflows
   - Write Kickstarter page copy

4. **Soft Launch** (Week 9-10)
   - Share with early access list
   - Gather feedback
   - Refine messaging

5. **Kickstarter Launch** (Week 11)
   - Launch campaign
   - PR push (Hacker News, Reddit, Twitter)
   - Daily updates to backers

---

## Related Documents

- [MCP Integration Architecture](MCP_INTEGRATION_ARCHITECTURE.md) - Technical design
- [Kickstarter Campaign Plan](KICKSTARTER_CAMPAIGN.md) - Campaign details
- [Demo Scenarios](DEMO_SCENARIOS.md) - Example workflows
- [ADR-007: REST API](../architectural_decision_records/ADR-007-rest-api-via-frontier-web-server.md) - Foundation work

---

**Last Updated**: 2026-01-13
**Status**: Vision document - ready for validation
**Next Review**: After user interviews (Week 2)
