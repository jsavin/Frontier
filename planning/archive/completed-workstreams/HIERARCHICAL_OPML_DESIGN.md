# Hierarchical OPML Structure for Integration Test Reports

## Problem Statement

The current integration test OPML export generates a single monolithic file that:
- Grows to 13,999+ lines with each test suite run
- Causes frequent merge conflicts when multiple developers add/modify tests
- Makes symlink pointers fragile (the symlink target itself becomes a conflict point)
- Treats the entire test hierarchy as one indivisible unit

## Solution: Multi-Level OPML Hierarchy with Transclusion

Instead of one large file, generate a hierarchy of small, focused OPML files that use OPML's native transclusion feature (`type="link"` + `url` attributes) to compose the full structure dynamically.

## Architecture

### File Structure

```
reports/
├── integration_tests.opml                    (canonical URL, top-level manifest)
├── integration_tests_db.opml                 (category file)
├── integration_tests_file.opml               (category file)
├── integration_tests_lang.opml               (category file)
├── integration_tests_table.opml              (category file)
├── integration_tests_dialog.opml             (category file)
├── integration_tests_outlineops.opml         (category file)
└── ... (other category files as needed)
```

### File Hierarchy in OPML

**`reports/integration_tests.opml`** (Top-level manifest):
```xml
<opml version="2.0">
  <head>
    <title>Frontier Integration Test Reports</title>
    <dateCreated>Fri, 17 Jan 2026 22:10:00 GMT</dateCreated>
  </head>
  <body>
    <outline text="Frontier Integration Test Categories">
      <outline text="Database Tests"
               type="link"
               url="integration_tests_db.opml" />
      <outline text="File Tests"
               type="link"
               url="integration_tests_file.opml" />
      <outline text="Language Tests"
               type="link"
               url="integration_tests_lang.opml" />
      <outline text="Table Tests"
               type="link"
               url="integration_tests_table.opml" />
      <!-- more categories -->
    </outline>
  </body>
</opml>
```

**`reports/integration_tests_db.opml`** (Category file):
```xml
<opml version="2.0">
  <head>
    <title>Database Integration Tests</title>
    <dateCreated>Fri, 17 Jan 2026 22:10:00 GMT</dateCreated>
  </head>
  <body>
    <outline text="Database Tests">
      <outline text="db.new - creates new database"
               type="test"
               status="PASS"
               created="Fri, 17 Jan 2026 22:10:00 GMT" />
      <outline text="db.open - opens existing database"
               type="test"
               status="PASS"
               created="Fri, 17 Jan 2026 22:10:00 GMT" />
      <!-- more db tests -->
    </outline>
  </body>
</opml>
```

When opened in Drummer or another OPML-aware editor:
- The top-level file displays category links
- Clicking/expanding a link loads the category file and shows its contents
- The hierarchy is navigable and explorable without downloading a 13,999-line file

## Benefits

### 1. **Eliminates Merge Conflicts**
- Adding `db.*` tests modifies only `integration_tests_db.opml`
- Adding `file.*` tests modifies only `integration_tests_file.opml`
- No conflicts between unrelated test categories
- Top-level manifest is stable and rarely changes

### 2. **Atomic Changes**
- Each test category is its own self-contained OPML file
- Changes are scoped to affected categories
- Git history is cleaner (smaller, focused diffs)
- Easier to review test changes in isolation

### 3. **Scalability**
- Large test suites don't create unwieldy files
- Files stay small and performant
- Easy to add new test categories without bloating existing files

### 4. **Native OPML Features**
- Uses standard transclusion (`type="link"`) supported by all modern OPML editors
- Works with Drummer, OPML Editor, and other tools that support this pattern
- No custom extensions or non-standard attributes

### 5. **Stable Canonical URL**
- `reports/integration_tests.opml` is the one true endpoint
- Can be symlinked, bookmarked, or shared without worrying about what it points to
- No more symlink merge conflicts

## Implementation

### Generator Changes (`tools/export_tests_to_opml.py`)

The export script needs to be refactored to:

1. **Parse tests by category** from YAML files
   - Extract category from test filename (e.g., `db_verbs.yaml` → `db`)
   - Group tests by category

2. **Generate category files**
   - For each category, create `integration_tests_{category}.opml`
   - Include test details (name, status, created time)
   - Use `type="test"` for individual test items

3. **Generate top-level manifest**
   - Create `integration_tests.opml` with links to all categories
   - Use `type="link"` + `url` for transclusion
   - Include metadata (title, dateCreated, etc.)

4. **Smart incremental updates** (optional)
   - Only regenerate category files that changed
   - Update top-level manifest timestamp
   - Preserve history if desired

### File Structure Patterns

**Naming:**
- Top-level: `integration_tests.opml`
- Categories: `integration_tests_{category}.opml` (e.g., `integration_tests_db.opml`)

**Attributes:**
- `type="link"` - for transclusion references
- `type="test"` - for individual test results
- `url` - for link targets (relative paths work)
- `created` - timestamp of when test was created/modified
- `status` - test pass/fail status (if applicable)

## Transition Plan

### Phase 1: Design & Prototyping (This document)
- Finalize OPML attribute specifications
- Prototype generator changes
- Test with Drummer to verify transclusion works

### Phase 2: Implementation
- Update `export_tests_to_opml.py` to generate hierarchical structure
- Add `.gitignore` rules as needed
- Test on develop branch with new test additions

### Phase 3: Integration
- Switch CI/CD to use new generator
- Verify no merge conflicts with concurrent test additions
- Document the new structure for developers

## Related Issues

- **PR #321**: Triggered this design (remove bloated monolithic OPML)
- **docserver original design**: Similar hierarchy approach for documentation
- **JakeShare.opml**: Reference implementation using `type="link"` transclusion

## Questions & Decisions Needed

1. **History retention**: Do we keep all generated OPML files in git, or only current ones?
2. **Relative vs absolute URLs**: Use relative paths (`integration_tests_db.opml`) or absolute URLs?
3. **Granularity beyond categories**: Should we go deeper (e.g., individual test files as OPML)?
4. **Generator invocation**: Manual, automatic on test changes, or CI/CD only?

## References

- OPML Spec: http://opml.org/spec2.opml
- Drummer (supports transclusion): https://drummer.land/
- OPML Editor: http://www.opmlEditor.org/
