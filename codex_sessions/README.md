Codex session logs live on the dedicated `codex-sessions` branch so they do not
clutter `develop`.

## Getting the logs locally

```sh
git fetch origin codex-sessions
git worktree add ../Frontier-codex-sessions codex-sessions   # once
```

This creates a sibling directory (`../Frontier-codex-sessions`) containing the
`codex-sessions` branch. All transcripts sit under
`../Frontier-codex-sessions/codex_sessions/`.

## Adding new sessions

1. Drop new log files into `../Frontier-codex-sessions/codex_sessions/`.
2. From that directory:

   ```sh
   git add codex_sessions
   git commit -m "Add Codex session logs through <date>"
   git push
   ```

## Reading/reviewing logs

- See the worktree path above, or browse directly on GitHub:
  <https://github.com/jsavin/Frontier/tree/codex-sessions/codex_sessions>

## Why a separate branch?

- Keeps large log files out of `develop` history.
- Makes it easy to clone or fetch only when needed.
- Keeps the main working tree clean while still versioning transcripts.
