# Codex Session Transcripts

This branch (`codex-sessions`) stores archived Codex CLI session logs. Each file
is a raw transcript (timestamped in its filename) and can be consumed with any
text editor.

## Updating the archive

```sh
# From the codex-sessions worktree (../Frontier-codex-sessions)
# Drop new transcripts into codex_sessions/

cd ../Frontier-codex-sessions
cp /path/to/new/codex_session_YYYYMMDD_nn.txt codex_sessions/
git add codex_sessions
git commit -m "Add Codex session logs through <date>"
git push
```

## Viewing logs

Browse in GitHub: https://github.com/jsavin/Frontier/tree/codex-sessions/codex_sessions

Or clone/fetch locally:

```sh
git fetch origin codex-sessions
git worktree add ../Frontier-codex-sessions codex-sessions   # once
```

Logs remain out of `develop` to keep the main history slim while preserving full
text transcripts for reference.
