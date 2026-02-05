# WPText Editor Specification

| | |
|---|---|
| **Version** | 0.1.0 |
| **Status** | Draft |
| **Last Updated** | 2026-02-04 |

## Change History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1.0 | 2026-02-04 | Jake Savin, Claude | Initial draft |

---

## Overview

The wptext (word processor text) editor is Frontier's rich text editor for creating and editing styled text documents. Unlike the outline-based editors (script, outline, menu), the wptext editor uses **off-the-shelf RTF editing components** appropriate to each platform.

WPText documents are stored in the ODB as **RTF (Rich Text Format) with UTF-8 encoding**. The editor works in whatever native format the platform component uses, but serializes to/from RTF for storage.

**Key design decisions:**
- **RTF is the canonical storage format** - The ODB stores RTF. Editors convert to/from RTF for storage.
- **Platform-specific implementations** - Different platforms use different RTF/rich text components.
- **Wrap, don't build** - Use existing RTF editing libraries, not a custom editor.
- **Feature parity over consistency** - Platform-native editors may have minor differences; that's acceptable.

---

## Visual Structure

### Window Layout

```
+-------------------------------------------------------------+
|  user.docs.readme                                     _ [] X |
+-------------------------------------------------------------+
| [B] [I] [U] [S] | Font [v] | Size [v] | [A] [Aa] | [link]   |
+-------------------------------------------------------------+
|                                                             |
|  Welcome to Frontier                                        |
|  ==================                                         |
|                                                             |
|  This is a **rich text** document with _formatting_.        |
|                                                             |
|  Features:                                                  |
|  * Bold, italic, underline                                  |
|  * Multiple fonts and sizes                                 |
|  * Links to websites                                        |
|                                                             |
|  Visit https://example.com for more info.                   |
|                                                             |
+-------------------------------------------------------------+
| Words: 42 | Characters: 256                        Line 5   |
+-------------------------------------------------------------+
```

### Components

- **Title bar:** Shows the dot-path to the document being edited (e.g., `user.docs.readme`)
- **Toolbar:** Formatting controls (bold, italic, underline, strikethrough, font, size, colors, links)
- **Document area:** Scrollable rich text editing surface
- **Status bar:** Word count, character count, cursor position

### Toolbar Components

| Button | Icon | Description |
|--------|------|-------------|
| **B** | Bold | Toggle bold on selection |
| **I** | Italic | Toggle italic on selection |
| **U** | Underline | Toggle underline on selection |
| **S** | Strikethrough | Toggle strikethrough on selection |
| **Font** | Dropdown | Select font family |
| **Size** | Dropdown | Select font size |
| **A** | Text color | Pick text foreground color |
| **Aa** | Background | Pick text background/highlight color |
| **Link** | Chain icon | Insert or edit hyperlink |

---

## Features

### MVP Features (Phase 1)

#### Text Formatting

| Feature | Shortcut (macOS) | Shortcut (Win/Linux) |
|---------|------------------|----------------------|
| Bold | Cmd+B | Ctrl+B |
| Italic | Cmd+I | Ctrl+I |
| Underline | Cmd+U | Ctrl+U |
| Strikethrough | Cmd+Shift+X | Ctrl+Shift+X |

#### Font Selection

- **Font family dropdown** - Lists available system fonts
- **Font size dropdown** - Common sizes (8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 26, 28, 36, 48, 72)
- **Custom size entry** - Type a specific size in the dropdown
- Keyboard shortcuts:
  - **Increase font size:** Cmd+Shift+> / Ctrl+Shift+>
  - **Decrease font size:** Cmd+Shift+< / Ctrl+Shift+<

#### Colors

- **Text color picker** - Select foreground color for text
- **Background/highlight color picker** - Select background color for text
- Standard color palette plus custom color option

#### Links

- **URL links** - Clickable links that open in default browser
- **Link insertion** - Select text, press Cmd+K / Ctrl+K, enter URL
- **Link editing** - Click on link, press Cmd+K to modify or remove
- **Link display** - Links shown underlined, in link color (typically blue)
- **Click behavior** - Single click follows link, Cmd+Click to edit

#### Lists

- **Bulleted lists** - Unordered lists with bullet markers
- **Numbered lists** - Ordered lists with number markers
- **Indent list** - Tab increases list nesting
- **Outdent list** - Shift+Tab decreases list nesting

### Phase 2 Features

| Feature | Description |
|---------|-------------|
| Tables | Insert and edit tables (rows, columns, cell content) |
| Paragraph alignment | Left, center, right, justify |
| Line spacing | Single, 1.5, double, custom spacing |
| Indentation | Paragraph indentation (first line, hanging) |

### Phase 3 Features

| Feature | Description |
|---------|-------------|
| Embedded images | Insert images from file or clipboard |
| Styles/headings | H1, H2, H3, etc. with predefined styles |
| Find and replace | Search and replace text within document |
| Spell check | Integrated spell checking (platform-dependent) |

### Phase 4 Features

| Feature | Description |
|---------|-------------|
| ODB links | Links to other ODB objects (`frontier://path.to.object`) |
| Print/export | Print preview, PDF export |
| Collaborative editing | Real-time multi-user editing |

---

## Platform Implementation Recommendations

### Web (Browser-based GUI)

Web editors work in HTML internally. RTF conversion is required for storage.

| Option | Pros | Cons |
|--------|------|------|
| **Quill** | Clean API, lightweight, good for embedding | RTF conversion needs library |
| **TinyMCE** | Mature, full-featured, good RTF plugins | Heavy, commercial for some features |
| **Tiptap** | Built on ProseMirror, simpler API, modern | RTF conversion challenges |
| **ProseMirror** | Highly customizable, good architecture | Steeper learning curve |
| **CKEditor 5** | Modern, good collaboration features | Complex licensing |

**Recommendation:** Quill or Tiptap for simplicity. TinyMCE if full RTF fidelity is critical.

**RTF Conversion Libraries:**
- `rtf.js` - JavaScript RTF parser
- `html-to-rtf` / `rtf-to-html` npm packages
- Server-side conversion via frontier-cli (preferred for fidelity)

### macOS (Swift/AppKit)

| Option | Pros | Cons |
|--------|------|------|
| **NSTextView** | Native, built-in RTF support, free | Older API |
| **TextKit 2** | Modern, powerful, good RTF support | macOS 12+ only |
| **WKWebView + web editor** | Consistent with web UI | Adds complexity |

**Recommendation:** NSTextView - native RTF read/write, works out of the box.

**NSTextView RTF API:**
```swift
// Read RTF
let rtfData = Data(contentsOf: url)
let attrString = NSAttributedString(rtf: rtfData, documentAttributes: nil)
textView.textStorage?.setAttributedString(attrString)

// Write RTF
let rtfData = textView.attributedString().rtf(from: NSRange(...))
```

### Windows

| Option | Pros | Cons |
|--------|------|------|
| **RichTextBox (WinForms)** | Built-in RTF, simple API | Older technology |
| **RichEditBox (WinUI)** | Modern, RTF support | WinUI only |
| **WebView2 + web editor** | Consistent with web UI | Adds complexity |

**Recommendation:** RichTextBox for simplicity. WebView2 + web editor for cross-platform consistency.

### Linux

| Option | Pros | Cons |
|--------|------|------|
| **Qt QTextEdit** | Good RTF support, cross-platform | Requires Qt |
| **WebKitGTK + web editor** | Consistent with web UI | WebKit dependency |
| **GtkSourceView** | GTK native | More code-oriented |

**Recommendation:** Qt QTextEdit if using Qt. WebKitGTK + web editor for GTK apps.

### Cross-Platform Strategy

Three approaches, in order of recommended priority:

1. **Web-first** - Build web editor, embed via WebView on native platforms
   - Most consistent UI across platforms
   - Single codebase for editor logic
   - RTF conversion handled server-side

2. **Native-first** - Use native rich text components on each platform
   - Best native feel
   - RTF support varies by platform
   - Minor feature/behavior differences acceptable

3. **Hybrid** - Web editor for complex documents, native for simple editing
   - Balance of consistency and native feel
   - More complex to implement

---

## RTF Storage Details

### Format

- **Standard:** RTF 1.x specification
- **Encoding:** UTF-8 for text content within RTF
- **MIME type:** `application/rtf` or `text/rtf`

### ODB Storage

WPText is stored as a binary external in the ODB:
- Type code: `'TEXT'` with wptext subtype
- Value: Raw RTF data

### v7 Database Note

In v7 databases, wptext stores **RTF with UTF-8 encoding**. Font and style information is preserved within the RTF structure (this is the exception to the v7 rule about not storing font/style info - RTF inherently contains this information).

### Example RTF

```rtf
{\rtf1\ansi\deff0 {\fonttbl{\f0 Helvetica;}}
{\colortbl;\red0\green0\blue0;\red0\green0\blue255;}
\f0\fs24 Welcome to Frontier\par
\par
This is a {\b rich text} document with {\i formatting}.\par
}
```

---

## Protocol Operations

### wptext/get - Get Document Content

Retrieves the wptext document as RTF.

**HTTP:**
```http
POST /api/wptext/get
Content-Type: application/json

{"path": "user.docs.readme"}
```

**WebSocket:**
```json
{
  "op": "wptext/get",
  "id": 1,
  "params": {
    "path": "user.docs.readme"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Dot-path to wptext object |
| `format` | string | No | Output format: `"rtf"` (default), `"html"`, `"text"` |

**Response:**
```json
{
  "id": 1,
  "result": {
    "path": "user.docs.readme",
    "content": "{\\rtf1\\ansi\\deff0 {\\fonttbl{\\f0 Helvetica;}}...",
    "format": "rtf",
    "encoding": "utf-8",
    "modified": "2026-02-04T12:00:00Z"
  }
}
```

**Format options:**
- `"rtf"` - Raw RTF (canonical format)
- `"html"` - Converted to HTML (for web editors)
- `"text"` - Plain text (stripped of formatting)

### wptext/set - Update Document Content

Updates the wptext document.

**HTTP:**
```http
POST /api/wptext/set
Content-Type: application/json

{
  "path": "user.docs.readme",
  "content": "{\\rtf1\\ansi\\deff0...",
  "format": "rtf"
}
```

**WebSocket:**
```json
{
  "op": "wptext/set",
  "id": 2,
  "params": {
    "path": "user.docs.readme",
    "content": "{\\rtf1\\ansi\\deff0...",
    "format": "rtf"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Dot-path to wptext object |
| `content` | string | Yes | Document content |
| `format` | string | No | Input format: `"rtf"` (default), `"html"` |

**Response:**
```json
{
  "id": 2,
  "result": {
    "path": "user.docs.readme",
    "modified": "2026-02-04T12:05:00Z"
  }
}
```

**Note:** If `format` is `"html"`, the server converts to RTF before storing.

### wptext/getPlainText - Get Plain Text

Extracts plain text from the document (useful for search indexing, preview).

**HTTP:**
```http
POST /api/wptext/getPlainText
Content-Type: application/json

{"path": "user.docs.readme"}
```

**WebSocket:**
```json
{
  "op": "wptext/getPlainText",
  "id": 3,
  "params": {
    "path": "user.docs.readme"
  }
}
```

**Response:**
```json
{
  "id": 3,
  "result": {
    "path": "user.docs.readme",
    "text": "Welcome to Frontier\n\nThis is a rich text document with formatting.\n\nFeatures:\n..."
  }
}
```

### wptext/convert - Convert Between Formats

Converts document content between RTF and HTML without storing.

**WebSocket:**
```json
{
  "op": "wptext/convert",
  "id": 4,
  "params": {
    "content": "<p><b>Hello</b> World</p>",
    "from": "html",
    "to": "rtf"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `content` | string | Yes | Source content |
| `from` | string | Yes | Source format: `"rtf"` or `"html"` |
| `to` | string | Yes | Target format: `"rtf"` or `"html"` |

**Response:**
```json
{
  "id": 4,
  "result": {
    "content": "{\\rtf1\\ansi...}",
    "format": "rtf"
  }
}
```

### Events (Server to Client)

#### wptext/updated - Document Changed

Notifies clients when a wptext document is modified by another client or script.

```json
{
  "event": "wptext/updated",
  "subscriptionId": "sub_xyz789",
  "data": {
    "path": "user.docs.readme",
    "modified": "2026-02-04T12:10:00Z",
    "changedBy": "user:alice"
  }
}
```

---

## Keyboard Shortcuts

### Formatting

| Action | macOS | Windows/Linux |
|--------|-------|---------------|
| Bold | Cmd+B | Ctrl+B |
| Italic | Cmd+I | Ctrl+I |
| Underline | Cmd+U | Ctrl+U |
| Strikethrough | Cmd+Shift+X | Ctrl+Shift+X |
| Remove formatting | Cmd+\ | Ctrl+\ |
| Increase font size | Cmd+Shift+> | Ctrl+Shift+> |
| Decrease font size | Cmd+Shift+< | Ctrl+Shift+< |

### Lists

| Action | macOS | Windows/Linux |
|--------|-------|---------------|
| Bulleted list | Cmd+Shift+8 | Ctrl+Shift+8 |
| Numbered list | Cmd+Shift+7 | Ctrl+Shift+7 |
| Indent list item | Tab | Tab |
| Outdent list item | Shift+Tab | Shift+Tab |

### Links

| Action | macOS | Windows/Linux |
|--------|-------|---------------|
| Insert/edit link | Cmd+K | Ctrl+K |
| Remove link | Cmd+Shift+K | Ctrl+Shift+K |

### Editing

| Action | macOS | Windows/Linux |
|--------|-------|---------------|
| Save | Cmd+S | Ctrl+S |
| Undo | Cmd+Z | Ctrl+Z |
| Redo | Cmd+Shift+Z | Ctrl+Y |
| Cut | Cmd+X | Ctrl+X |
| Copy | Cmd+C | Ctrl+C |
| Paste | Cmd+V | Ctrl+V |
| Paste without formatting | Cmd+Shift+V | Ctrl+Shift+V |
| Select all | Cmd+A | Ctrl+A |
| Find | Cmd+F | Ctrl+F |
| Find and replace | Cmd+H | Ctrl+H |

### Navigation

| Action | macOS | Windows/Linux |
|--------|-------|---------------|
| Go to beginning | Cmd+Up | Ctrl+Home |
| Go to end | Cmd+Down | Ctrl+End |
| Go to line start | Cmd+Left | Home |
| Go to line end | Cmd+Right | End |

---

## Saving

### Explicit Save

- **Cmd+S / Ctrl+S** saves the document to the ODB
- Window title shows dirty indicator (*) when unsaved changes exist
- Closing window prompts to save if dirty

### Auto-Save (Future)

- Configurable auto-save interval (e.g., every 30 seconds)
- Saves after idle period with unsaved changes
- Visual indicator when auto-save occurs

---

## Phasing

| Phase | Features |
|-------|----------|
| **MVP** | Basic formatting (B/I/U/S), font/size selection, text/background colors, links, bulleted/numbered lists |
| **Phase 2** | Tables (insert, edit cells, resize), paragraph alignment, line spacing |
| **Phase 3** | Embedded images, styles/headings (H1-H6), find and replace, spell check |
| **Phase 4** | ODB links with protocol handler, print/PDF export, collaborative editing |

---

## Open Questions

1. **RTF fidelity:** How important is round-trip fidelity? If a user edits in an HTML-based editor, converts to RTF, then back to HTML, will all formatting be preserved? What's our tolerance for formatting loss?

2. **HTML as alternative storage:** Should we support HTML as an alternative to RTF for web-first workflows? Or always convert to RTF for storage?

3. **Server-side vs client-side conversion:** Should the server (frontier-cli) provide HTML/RTF conversion endpoints, or should clients handle conversion themselves? Server-side is more consistent but adds latency.

4. **ODB link protocol:** What should the URL scheme be for ODB links? `frontier://path.to.object`? How to handle cross-database links?

5. **Image storage:** When we add images, where are they stored?
   - Embedded in RTF (increases document size)
   - Separate ODB entries with references
   - External file references

6. **Diff/merge:** How to handle concurrent edits in collaborative mode? RTF doesn't diff well. Consider CRDT for rich text (e.g., Yjs, Automerge).

7. **Search indexing:** How to index wptext content for ODB search? Extract plain text on save? Index on demand?

8. **Maximum document size:** Any limits on document size? Large documents with many images may need streaming or chunked loading.

9. **Print/export:** PDF export priority? Native print dialog or custom preview?

10. **Accessibility:** Screen reader support for rich text content? Keyboard navigation in toolbar? ARIA labels?

11. **Copy/paste fidelity:** When pasting from external apps (Word, web pages), how much formatting to preserve? Offer "paste as plain text" option?

12. **Undo history:** How much undo history to maintain? Persisted across save/reload? Undo after close/reopen?

---

## Related Documents

- [`ARCHITECTURE.md`](./ARCHITECTURE.md) - Overall GUI architecture
- [`PROTOCOL.md`](./PROTOCOL.md) - JSON protocol specification
- [`TABLE_BROWSER.md`](./TABLE_BROWSER.md) - Table browser (for viewing wptext in ODB context)
- [`SCRIPT_EDITOR.md`](./SCRIPT_EDITOR.md) - Script editor (contrast: outline-based vs rich text)
- [`OUTLINE_EDITOR.md`](./OUTLINE_EDITOR.md) - Outline editor (contrast: structured vs free-form)
