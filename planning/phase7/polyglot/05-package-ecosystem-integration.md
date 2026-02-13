# Package Ecosystem Integration

Status
- State: Draft
- Phase: Phase 7 — Polyglot Scripting
- Last Updated: 2026-02-13
- Owner: Jake
- Notes: Specification only. Resolves package management questions deferred in specs 00–04. No code changes.

Related Docs
- `00-polyglot-vision.md` — Vision & Architecture Overview
- `01-language-interface-spec.md` — Abstract Language Interface
- `02-javascript-integration.md` — JavaScript Integration (module resolution, bare specifiers)
- `03-python-integration.md` — Python Integration (pip ecosystem, virtualenv questions)
- `04-compiled-extensions.md` — Compiled Extensions (extension paths, future registry)
- `planning/phase6/CRDT_FOUNDATION_ROADMAP.md` — Phase 6 (prerequisite)
- `planning/phase4/threading/README.md` — Threading & GIL model (ADR-014)
- `Common/headers/langexternal.h` — External value type system

Change Log
- 2026-02-13: Initialized document.

## Overview

Specs 00–04 establish how JavaScript, Python, and compiled extensions integrate with Frontier's runtime, but explicitly defer package management. This spec resolves those open questions:

- **From 00**: "How do npm/pip packages interact with ODB-stored scripts? Can `node_modules` or virtualenvs be ODB-rooted?"
- **From 02**: "Can `node_modules` be stored in the ODB? Or is filesystem-only more practical?"
- **From 03**: "Should Frontier manage its own virtualenv? Can `pip install` be triggered from Frontier? Should there be a `requirements.txt` equivalent in the ODB?"
- **From 04**: "Should there be a package registry for Frontier extensions?"

The answer is a design that puts the ODB at the center of the developer experience while transparently bridging to npm/pip ecosystems on the filesystem.

**Core ideas:**

1. **Filesystem mounts** — A new ODB table type that is actually a view into a filesystem directory. This bridges the two worlds: developers see packages in the ODB namespace without caring that they live on disk.

2. **The `#packages` convention** — A language-agnostic table hierarchy (`#packages.npm`, `#packages.pip`, `#packages.frontier`) for declaring and discovering dependencies.

3. **Two tiers** — Tier 1 (ODB-managed) for developers who want Frontier to handle everything, and Tier 2 (external environment) for developers with existing npm/pip workflows.

4. **Auto-detection** — When a developer with an existing `node_modules/` or virtualenv opens a database, Frontier detects and uses it with zero configuration.

**Two developer personas:**

- **Suite-structured apps**: A developer building `myAppSuite` declares dependencies in `myAppSuite.#packages.npm.dependencies` and Frontier manages installation.
- **Ad-hoc scripting**: A developer writing scripts at the database root uses a well-known `#packages` table at the root level.

## Decisions

### D1: Packages Live on the Filesystem; the ODB Declares and Discovers Them

npm and pip packages are filesystem artifacts — compiled native extensions, deeply nested `node_modules/` trees, symlinked virtualenvs. Storing these byte-for-byte in the ODB would be impractical and fragile. Instead:

- The **ODB** stores the dependency manifest (what to install, version constraints, configuration).
- The **filesystem** stores the installed packages (the actual `node_modules/` or `site-packages/` directory).
- **Filesystem mounts** (D2) bridge the gap, making filesystem packages visible in the ODB namespace.

This follows the pattern of other embeddable systems: Blender stores a reference to its bundled Python, not a copy of every pip package. PostgreSQL extensions live on disk, declared in `pg_catalog`.

### D2: Filesystem Mounts Bridge the ODB Namespace to Package Directories

A filesystem mount is a new ODB table type that transparently maps to a directory on disk. When a script navigates to `myAppSuite.#packages.npm.express`, the ODB resolves that path through the mount to the filesystem.

Full design in [Filesystem Mounts](#filesystem-mounts--a-new-odb-concept) below.

### D3: Installation Uses Native Tools as Subprocesses

Frontier does not reimplement npm or pip. Instead:

- `frontier.packages.install("express", "npm")` generates an ephemeral `package.json`, invokes `npm install` as a subprocess, and updates the mount.
- `frontier.packages.install("requests", "pip")` generates a `requirements.txt`, invokes `pip install`, and updates the mount.

This ensures compatibility with the full ecosystem (private registries, native extensions, lockfile formats) without maintaining a parallel implementation.

### D4: Default Is Auto-Detection; Explicit Configuration as Override

When opening a database, Frontier scans for existing package environments:

1. `node_modules/` adjacent to or near the `.root7` file
2. `.venv/` or `venv/` adjacent to or near the `.root7` file
3. Active `VIRTUAL_ENV` environment variable

If found, Frontier creates implicit mounts. No configuration needed. Developers can override with explicit configuration when auto-detection finds the wrong environment or when they want non-standard paths.

### D5: The `frontier` Module Is Virtual, with Installable Type Stubs

As established in specs 02 and 03, the `frontier` module is injected at runtime — it is not installed via npm or pip. For editor support (autocompletion, type checking), Frontier provides:

- `@anthropic/frontier-types` (npm) — TypeScript declarations
- `frontier-stubs` (pip) — Python type stubs

These are standard packages that can be installed in development environments. They contain only type information, no runtime code.

### D6: The Design Accommodates a Future Frontier Package System

The `#packages` convention is language-agnostic by design. `#packages.npm` and `#packages.pip` are peers alongside a reserved `#packages.frontier` table. When Frontier develops its own package ecosystem (evolving from suite sharing toward formal versioned packages), it slots in naturally without redesigning the infrastructure.

## Filesystem Mounts — A New ODB Concept

### What It Looks Like

From a developer's perspective:

```
myAppSuite
  #packages
    npm        ← mount of /path/to/node_modules
      express
        index.js
        package.json
      lodash
        lodash.js
    pip        ← mount of /path/to/.venv/lib/python3.12/site-packages
      requests
        __init__.py
      flask
        __init__.py
    frontier   ← reserved for future Frontier/UserTalk packages
```

The developer interacts with `myAppSuite.#packages.npm.express` as if it were a normal ODB path. Under the hood, Frontier resolves this to a filesystem read.

### Mount Declaration

A mount is declared as metadata on an ODB table node. The table itself is a special external value type (building on the `externalvaluetype` pattern from `langexternal.h`):

```c
// Conceptual — not final API
typedef struct {
    char        filesystem_path[1024];  // e.g., "/path/to/node_modules"
    tyvaluedata mount_flags;            // read-only, read-write, cached, etc.
    long        cache_ttl_seconds;      // directory listing cache lifetime
    long        last_scan_timestamp;    // last directory scan time
} ty_filesystem_mount;
```

The mount metadata lives in the ODB. The mounted content does not.

### Mount Semantics: Read-Only vs Read-Write

Two approaches, each with trade-offs:

**Read-only mounts (recommended default):**
- ODB operations can read/enumerate the mounted directory
- Modifications go through Frontier's package verbs (`frontier.packages.install()`) which invoke npm/pip
- Prevents accidental corruption of `node_modules/` through ODB table operations
- Simpler to implement and reason about

**Read-write mounts (advanced/optional):**
- ODB write operations map to filesystem writes
- Useful for development workflows where direct file manipulation is desired
- Risk: ODB operations could corrupt package directory structure
- Requires careful mapping of ODB semantics to filesystem semantics

**Recommendation:** Default to read-only mounts. Read-write mounts can be a future option for power users, gated behind explicit configuration.

### How the Engine's Module Loader Interacts with Mounts

When a JS or Python engine resolves an import:

1. **JS bare specifier** (`import "express"`): The JS engine's module loader checks the suite's `#packages.npm` mount first, then the database-level `#packages.npm` mount. The mount resolves the specifier to a filesystem path (`/path/to/node_modules/express/index.js`), which the engine loads normally.

2. **Python import** (`import requests`): The mount's filesystem path is added to `sys.path` at engine initialization. Python's standard import machinery handles resolution. No custom importer needed.

This means the mount primarily serves two purposes:
- **Discovery**: Making package contents visible in the ODB namespace (for browsing, search, tooling)
- **Configuration**: Telling the engine where to find packages

### Performance Considerations

Directory scanning (enumerating `node_modules/` with thousands of entries) can be expensive. Mitigations:

- **Lazy enumeration**: Don't scan the mounted directory until a script or user explicitly navigates into it
- **Cached directory listings**: Cache the top-level directory listing with a configurable TTL (default: 60 seconds)
- **Shallow mounts**: By default, only enumerate the first level (package names). Deeper enumeration happens on demand.
- **Background refresh**: Update the cache asynchronously when the TTL expires, serving stale data briefly rather than blocking

### Relationship to Existing Patterns

Filesystem mounts build on the existing `externalvaluetype` pattern in Frontier's ODB. External value types already allow ODB nodes to represent data that doesn't fit the standard value model. A mount extends this: the external value is "the contents of a directory."

The mount concept is analogous to:
- Unix mount points (a directory that transparently redirects to another filesystem)
- Docker volume mounts (host directories visible inside a container)
- Blender's asset browser (filesystem directories browsable through the application UI)

## Package Configuration in the ODB

### Suite-Structured Apps

For applications organized as suites (the primary Frontier development pattern):

```
myAppSuite
  #packages
    npm
      dependencies    → {"express": "^4.18", "lodash": "^4.17"}
      devDependencies → {"jest": "^29.0"}
      mount           → (filesystem mount to node_modules)
    pip
      dependencies    → ["requests>=2.28", "flask>=3.0"]
      devDependencies → ["pytest>=7.0"]
      mount           → (filesystem mount to site-packages)
    policy            → {isolation: "shared"}
    frontier          → (reserved for future Frontier packages)
```

The `dependencies` and `devDependencies` tables mirror npm/pip conventions. The `mount` node is the filesystem mount pointing to the installed packages. The `policy` table controls isolation behavior.

### Ad-Hoc Scripting (No Suite)

For scripts at the database root level without suite structure, three options were considered:

**Option A: Root-level `#packages` table (recommended)**

```
#packages
  npm
    dependencies → {"express": "^4.18"}
    mount        → (filesystem mount)
  pip
    dependencies → ["requests>=2.28"]
    mount        → (filesystem mount)
```

Pros: Consistent with suite pattern (same table name, just at root). Discoverable. The `#` prefix signals "system/config" by convention.

Cons: Occupies a root-level name (mitigated by `#` prefix convention).

**Option B: `user.packages` table**

```
user
  packages
    npm
      dependencies → {...}
```

Pros: Follows Frontier's `user.*` convention for per-user data.

Cons: `user.*` implies user-specific, but packages are database-scoped. Confusing semantics.

**Option C: Database-level config table**

```
config
  packages
    npm
      dependencies → {...}
```

Pros: Clear separation of configuration from content.

Cons: No existing `config` convention in Frontier. Introduces a new top-level name.

**Recommendation:** Option A. The `#packages` table at root level is consistent with the suite-structured pattern, uses the `#` prefix to signal configuration/system tables, and is immediately discoverable by tools.

### The `#` Prefix Convention

The `#` character in table names signals "system/configuration" — not user content. This convention is new but intentional:

- `#packages` — Package configuration and mounts
- Future: `#config`, `#metadata`, `#hooks` as the convention develops

The `#` prefix avoids collisions with the reserved `system.*` namespace (which belongs to `Frontier.root`) and the `user.*` namespace (which is per-user, not per-database).

### Isolation Policy

The `policy` table controls how packages are shared:

| Policy | Meaning | Use Case |
|--------|---------|----------|
| `shared` | All suites in the database share one `node_modules` / virtualenv | Simple apps, small databases |
| `isolated` | Each suite gets its own package directory | Large databases, conflicting dependency versions |

Default: `shared` (simpler, less disk usage). Isolation creates per-suite directories under the cache path.

## Tier 1: ODB-Managed Packages (Default)

Developers interact with packages entirely through Frontier. The ODB is the source of truth for what's installed.

### Kernel Verbs

```javascript
// JavaScript API (via frontier global)
frontier.packages.install("express", {language: "npm"});
frontier.packages.install("requests", {language: "pip"});
frontier.packages.uninstall("lodash", {language: "npm"});
frontier.packages.list({language: "npm"});           // → [{name, version, ...}]
frontier.packages.update("express", {language: "npm"});
frontier.packages.update({language: "pip"});          // update all pip packages

// UserTalk API
frontier.packages.install("express", "npm")
frontier.packages.uninstall("lodash", "npm")
frontier.packages.list("npm")
frontier.packages.update("pip")                       // update all pip packages
```

```python
# Python API (via frontier module)
frontier.packages.install("requests", language="pip")
frontier.packages.uninstall("flask", language="pip")
frontier.packages.list(language="pip")
frontier.packages.update(language="pip")
```

### CLI Interface

```bash
# Install packages
frontier-cli --db myapp.root7 packages npm install express lodash
frontier-cli --db myapp.root7 packages pip install requests flask

# List installed packages
frontier-cli --db myapp.root7 packages npm list
frontier-cli --db myapp.root7 packages pip list

# Update packages
frontier-cli --db myapp.root7 packages npm update express
frontier-cli --db myapp.root7 packages pip update

# Install from manifest (restore from ODB)
frontier-cli --db myapp.root7 packages npm restore
frontier-cli --db myapp.root7 packages pip restore

# Suite-specific operations
frontier-cli --db myapp.root7 packages npm install express --suite myAppSuite
```

### Under the Hood

When `frontier.packages.install("express", "npm")` is called:

1. **Read manifest**: Load `#packages.npm.dependencies` from the current suite (or root `#packages`)
2. **Add entry**: Insert `"express": "latest"` (or specified version) into the dependencies table
3. **Generate ephemeral file**: Write a temporary `package.json` from the manifest
4. **Invoke npm**: Run `npm install --prefix <cache-path>` as a subprocess
5. **Update mount**: Ensure the `#packages.npm.mount` points to the resulting `node_modules/`
6. **Clean up**: Remove the temporary `package.json`

The pip flow is analogous, using `pip install -r <temp-requirements.txt> --target <cache-path>`.

### Filesystem Cache

Installed packages live on the filesystem in a managed cache:

```
~/.frontier/packages/
  <db-hash>/                    # per-database (shared policy)
    npm/
      node_modules/
    pip/
      site-packages/
  <db-hash>/<suite-name>/      # per-suite (isolated policy)
    npm/
      node_modules/
    pip/
      site-packages/
```

The `<db-hash>` is derived from the database file path, ensuring different databases don't collide. This location is configurable:

- Database-level: `#packages.policy.cachePath` overrides the default
- Global: `~/.frontier/config` can set a different root

### Portability

The key advantage of ODB-managed packages: the `.root7` file is portable.

1. Copy `myapp.root7` to another machine
2. Open in Frontier
3. Frontier reads `#packages.npm.dependencies` and `#packages.pip.dependencies`
4. Frontier runs `npm install` and `pip install` to populate the local cache
5. Mounts are created automatically
6. Ready to use

This is analogous to cloning a repo with `package.json` — you run `npm install` and you're ready.

## Tier 2: External Environment (Auto-Detect + Link)

For developers with existing npm/pip workflows who don't want Frontier managing their packages.

### Auto-Detection

When Frontier opens a database, it scans for existing package environments:

**Search order:**

1. **Adjacent to database**: `./node_modules/`, `./.venv/`, `./venv/`
2. **Parent directories**: Walk up to 3 levels looking for `node_modules/` or `.venv/`
3. **Environment variables**: `VIRTUAL_ENV`, `NODE_PATH`
4. **Common patterns**: Check for `.nvmrc`, `pyproject.toml`, `package.json` in parent directories to infer project root

**When found:**

- Create an implicit mount (not persisted to ODB unless the user saves)
- Log the detection: `"Auto-detected node_modules at /path/to/project/node_modules"`
- The mount is read-only by default

**When not found:**

- No mount created
- Bare imports will fail with a clear error: `"Cannot resolve 'express'. No npm packages configured. Use frontier.packages.install() or link an existing node_modules."`

### Explicit Link

For non-standard directory layouts:

```javascript
// Link an existing node_modules
frontier.packages.npm.link("/path/to/my-project/node_modules");

// Link an existing virtualenv
frontier.packages.pip.link("/path/to/my-project/.venv");
```

Linking creates a persistent mount in the ODB that points to the specified directory. The linked directory is used as-is — Frontier does not manage its contents.

### Mixed Mode

Per-language configuration is independent. A developer can use:

- Tier 1 (ODB-managed) for pip packages
- Tier 2 (external link) for npm packages

This supports real-world workflows where one ecosystem is tightly managed and the other is handled by existing tooling (e.g., a frontend project with its own `package.json` but backend scripts using Frontier-managed pip packages).

### Use Cases

- **Existing projects**: Developer has a Node.js project with `package.json`. Opens a `.root7` in the project directory. Frontier auto-detects `node_modules/` — imports work immediately.
- **CI/CD**: Build scripts install packages via `npm ci` before running Frontier scripts. Frontier auto-detects the `node_modules/` created by CI.
- **Monorepos**: Link to a specific workspace's `node_modules/` rather than the root.
- **Team workflows**: Shared database with a linked `node_modules/` on a network mount. Each team member sees the same packages.

## Manifest Depth Analysis

How much dependency information should the ODB store?

### Thin Manifest

The ODB stores only top-level dependencies and version constraints. Transitive resolution (the full dependency tree, exact versions, integrity hashes) lives on the filesystem in standard lockfiles (`package-lock.json`, `requirements.txt` with pinned versions).

```
#packages.npm.dependencies → {"express": "^4.18", "lodash": "^4.17"}
```

**Pros:**
- Simple — the ODB manifest is small and human-readable
- Standard — lockfiles are well-understood by the npm/pip ecosystems
- Tool-compatible — `npm audit`, `pip check`, etc. work on the filesystem lockfiles

**Cons:**
- Not fully self-contained — copying just the `.root7` gets you the top-level constraints, but `npm install` may resolve to different transitive versions on a different machine/date
- Two sources of truth — the ODB manifest and the filesystem lockfile can drift

### Rich Manifest

The ODB stores the full resolved dependency tree, including exact versions and integrity hashes. The ODB IS the lockfile.

```
#packages.npm.dependencies → {"express": "^4.18"}
#packages.npm.resolved → {
  "express": {"version": "4.18.2", "integrity": "sha512-..."},
  "accepts": {"version": "1.3.8", "integrity": "sha512-..."},
  ... (hundreds of entries for a typical project)
}
```

**Pros:**
- Fully portable — copying the `.root7` guarantees reproducible installs
- Single source of truth — no drift between ODB and filesystem
- Auditable — the full dependency tree is inspectable through the ODB

**Cons:**
- Large — a typical Express app has ~60 transitive dependencies; a real-world project can have hundreds
- Maintenance burden — every `npm install` must update the ODB resolved tree
- Duplicates npm/pip's own lockfile format without clear benefit

### Recommendation

**Default to thin manifest.** Most developers are familiar with `package-lock.json` and `requirements.txt` as lockfiles. The thin manifest keeps the ODB clean and lets the filesystem tooling handle resolution.

**Offer rich manifest as an option** for users who need fully portable databases (e.g., distributing a database to users who may not have npm/pip installed). Enable via `#packages.policy.manifest = "rich"`.

When rich manifest is enabled, `frontier.packages.install()` captures the full resolved tree and stores it in the ODB. `frontier.packages.restore()` uses the resolved tree for deterministic installation.

## Package Resolution

### JavaScript

When a JS engine encounters a bare specifier (`import "express"`):

1. **Suite `#packages.npm` mount** — If executing within a suite context, check the suite's npm mount first
2. **Database `#packages.npm` mount** — Check the root-level npm mount
3. **Global cache** — Check `~/.frontier/packages/global/npm/node_modules/` (if configured)
4. **Error** — `"Cannot resolve module 'express'. No npm packages found."`

For relative specifiers (`import "./helpers"`), resolution follows the ODB-relative rules from spec 02. For `file:` specifiers, resolution is direct filesystem access.

### Python

When a Python engine encounters `import requests`:

1. **Suite virtualenv** — If executing within a suite context, the suite's `site-packages/` is first in `sys.path`
2. **Database virtualenv** — The root-level `#packages.pip` mount's path is in `sys.path`
3. **Global cache** — `~/.frontier/packages/global/pip/site-packages/` (if configured)
4. **System Python** — If `#packages.policy.allowSystemPackages` is `true` (default: `false`), fall through to the system Python's `site-packages`
5. **ImportError** — `"No module named 'requests'. Use frontier.packages.install('requests', language='pip') to install."`

### Per-Execution Isolation

When multiple scripts run concurrently (under the GIL), they may need different package resolution contexts (e.g., two suites with different versions of the same package). This is handled by:

- **JS**: Each engine instance gets its own module loader configuration pointing to the correct mount
- **Python**: Thread-local `sys.path` modification at script entry, restored at exit (via a context manager in the Python integration layer)

This aligns with the threading model from ADR-014: the GIL ensures only one script executes at a time, so `sys.path` modifications are safe as long as they're restored.

## The `frontier` Virtual Package + Type Stubs

### Runtime Injection

The `frontier` module (JS global object / Python module) is injected by Frontier's engine integration layer at initialization, as specified in specs 02 and 03. It is never installed via npm or pip.

### Type Stubs for Editor Support

Developers using VS Code, PyCharm, or other editors need type information for autocompletion:

**For JavaScript/TypeScript:**
```bash
npm install --save-dev @anthropic/frontier-types
```

Provides TypeScript declaration files (`.d.ts`) for the `frontier` global object, all kernel verb namespaces, and value types.

**For Python:**
```bash
pip install frontier-stubs
```

Provides PEP 561 type stubs for the `frontier` module, including all kernel verb namespaces and value types.

### Auto-Generation

Type stubs are generated from Frontier's verb table registrations (`kernel_verbs_headless.c`). A build tool reads the verb registry and produces:

- TypeScript declarations mapping each verb namespace and function signature
- Python stub files with proper type annotations

This ensures stubs stay in sync with the runtime API as verbs are added or modified.

## The Future `frontier` Package Ecosystem

### Reserved Namespace

`#packages.frontier` is reserved for future Frontier-native packages. The infrastructure is designed so that when Frontier develops its own package system, it slots in as a peer alongside npm and pip:

```
myAppSuite
  #packages
    npm       ← managed by npm
    pip       ← managed by pip
    frontier  ← managed by Frontier's future package system
```

### Evolution Path

1. **Suite sharing (current)**: Developers share suites by copying ODB tables or exporting OPML
2. **Versioned suites**: Add version metadata to suites, enabling version-constraint-based imports
3. **Registry**: A Frontier package registry (akin to npm registry) for publishing and discovering suites
4. **Dependency resolution**: The `frontier.packages.install("my-package", "frontier")` verb installs from the registry

### Design Constraints

The current spec avoids over-specifying the Frontier package system. Key constraints that the current design preserves:

- The `#packages.frontier` table uses the same `dependencies` / `mount` structure as npm/pip
- The resolution order (suite → database → global → error) works for Frontier packages too
- The CLI pattern (`frontier-cli packages frontier install my-package`) extends naturally
- Isolation policy applies uniformly across all languages

## Private Packages and Registries

### npm Private Registries

Developers using private npm registries (GitHub Packages, Artifactory, Verdaccio) configure access via:

1. **Environment-based** (recommended): `.npmrc` in the user's home directory or project root. Frontier's subprocess invocation of `npm install` inherits this configuration.
2. **Database-based**: `#packages.npm.registry` specifies a custom registry URL. Frontier generates the appropriate `.npmrc` when invoking npm.

### pip Private Indexes

Similarly, private pip indexes are configured via:

1. **Environment-based**: `pip.conf` or `PIP_INDEX_URL` environment variable.
2. **Database-based**: `#packages.pip.indexUrl` specifies a custom index.

### Credential Handling

Credentials for private registries are NOT stored in the ODB. They remain in the environment (`.npmrc`, `pip.conf`, environment variables, keyring). The ODB stores only the registry URL, not authentication tokens.

## Developer Workflows

### Workflow A: Pure ODB, No External Packages

A developer writes UserTalk, JavaScript, or Python scripts that only use Frontier's built-in kernel verbs. No npm/pip packages needed.

- No `#packages` table exists
- The `frontier` module/global is available (runtime-injected)
- Imports resolve only within the ODB

### Workflow B: ODB-Managed Packages (Tier 1)

A developer building a web scraping tool in Python:

```python
# 1. Install packages (from Frontier REPL or script)
frontier.packages.install("requests", language="pip")
frontier.packages.install("beautifulsoup4", language="pip")

# 2. Use them in a script
import requests
from bs4 import BeautifulSoup

page = requests.get("https://example.com")
soup = BeautifulSoup(page.content, "html.parser")
frontier.odb.set("scraping.results.title", soup.title.string)
```

Behind the scenes:
- `#packages.pip.dependencies` now contains `["requests", "beautifulsoup4"]`
- `~/.frontier/packages/<db-hash>/pip/site-packages/` contains the installed packages
- `#packages.pip.mount` points to that directory

### Workflow C: Existing Project (Tier 2)

A developer with an existing Express.js project:

```
my-project/
  package.json
  node_modules/
  src/
  data.root7        ← Frontier database
```

1. Opens `data.root7` in Frontier
2. Frontier auto-detects `../node_modules/` (one level up)
3. An implicit mount is created
4. Scripts can immediately `import "express"` and use it

No configuration, no `#packages` table needed.

### Workflow D: Team/Shared Database

A team sharing a database with standardized dependencies:

1. Team lead creates `#packages.npm.dependencies` with the team's standard packages
2. Each developer clones the database (or accesses a shared copy)
3. On first open, each developer runs `frontier-cli --db shared.root7 packages npm restore`
4. Each machine gets its own local `node_modules/` in `~/.frontier/packages/`, but the dependency manifest is shared through the ODB

## Prior Art

| System | Package Approach | Lesson for Frontier |
|--------|-----------------|---------------------|
| **Blender** | Bundled Python with `pip install --target`. Scripts can import from system Python or Blender's bundled packages. | Managed environment with escape hatch to system. Similar to Tier 1 + Tier 2 model. |
| **PostgreSQL** | Extensions installed on disk (`CREATE EXTENSION`), declared in `pg_catalog`. | ODB declares, filesystem stores. Direct inspiration for the mount concept. |
| **Deno** | URL-based imports, cached locally. `deno.json` for import maps. No `node_modules` by default. | Shows that a centralized cache works well. The "import map" concept is similar to ODB manifest. |
| **Jupyter** | Uses the kernel's Python environment. `%pip install` magic for in-notebook installation. | In-environment installation verb is natural. Auto-detection of existing environment is essential. |
| **Electron** | Full Node.js with `node_modules`. App bundles its own packages. | Per-app isolation is the default. Analogous to per-suite isolation. |
| **Neovim** | Lua packages via `luarocks` or bundled. Plugin managers (lazy.nvim) handle installation. | Two-tier model works: managed (lazy.nvim) and manual (luarocks) coexist. |
| **Godot** | GDExtension for native code. Asset Library for Godot-specific packages. External packages (pip, npm) not directly supported. | Shows value of a first-party package system alongside external ecosystems. Validates the `frontier` package reservation. |

## Open Questions

1. **Mount performance at scale**: How does a mount of a large `node_modules/` (1000+ packages) perform for ODB enumeration? Need to profile lazy enumeration + caching approach.

2. **Lock file strategy**: Should Frontier generate `package-lock.json` / `requirements.txt` in the cache directory for reproducibility, even in thin manifest mode? Leaning yes, but needs validation.

3. **Global packages**: Should there be a Frontier-wide shared package cache that all databases can opt into (`#packages.policy.useGlobalCache = true`)? Reduces disk usage but introduces version conflict risk.

4. **Native extensions**: npm packages with native addons (node-gyp) and pip packages with C extensions require build tools. Should Frontier detect and warn, or attempt to install build dependencies?

5. **Offline mode**: For fully air-gapped environments, should Frontier support pre-populated cache directories? The mount concept supports this (just point to a pre-populated directory), but the workflow needs documentation.

6. **Virtual mount depth**: Should mounts support nested mounts (a mounted directory containing another mount point)? Initial answer: no, keep it simple.

7. **Database size impact**: Rich manifests for large projects could add significant size to the `.root7` file. Need benchmarks for databases with 500+ resolved dependencies.

8. **Concurrent access**: When two Frontier instances open the same database and both try to install packages, how is the filesystem cache coordinated? File locking? Advisory locks?

## Next Steps

1. **Prototype filesystem mount type** — Implement a minimal read-only mount that maps an ODB path to a filesystem directory (requires new `externalvaluetype` variant)
2. **Prototype `#packages` table** — Define the ODB schema for dependency manifests and test with manual table creation
3. **Auto-detection spike** — Implement the scanning algorithm for `node_modules/` and `.venv/` detection
4. **JS module loader integration** — Extend the module resolution from spec 02 to resolve through mounts
5. **Python sys.path integration** — Extend the Python integration from spec 03 to include mount paths
6. **CLI `packages` subcommand** — Implement the CLI interface for install/restore/list operations
7. **Type stub generator** — Build the tool that generates TypeScript declarations and Python stubs from verb registrations
