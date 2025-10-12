# Frontier CLI Recipes

Status
- State: In Progress
- Phase: 1–2
- Last Updated: 2025-09-29
- Notes: Practical examples for common CLI tasks.

Related Docs
- frontier-cli/README.md
- planning/DEVELOPER_QUICKSTART_HEADLESS.md

Change Log
- 2025-09-29: Initial set of recipes.

Run a script file
```bash
./frontier-cli path/to/script.usertalk
```

Run inline code
```bash
./frontier-cli -e "local(x=5); x * 2"
```

Open database and run a query
```bash
./frontier-cli -d databases/Frontier.root -q "db.getValue('system.version')"
```

Create a new database
```bash
./frontier-cli -d databases/new.root -q "db.new()"
```

Migrate a database (where supported)
```bash
./frontier-cli -d databases/legacy.root -m migrate
```

Start HTTP server (headless)
```bash
./frontier-cli --server --port 8080
```

Start WebSocket server (headless)
```bash
./frontier-cli --websocket --port 8081
```

Common errors and tips
- Ensure headless builds do not link UI frameworks; see planning/no_ui_linkage_policy.md.
- For tests, prefer running `make -C tests` and the test binaries directly.
- When a verb requires UI, expect a predictable error or no-op in headless mode.

