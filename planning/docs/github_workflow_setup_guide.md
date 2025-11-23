## GitHub Integrations for UserLanders / Frontier

Status
- State: Reference
- Phase: Tooling
- Last Updated: 2025-11-20
- Owner: Codex
- Notes: Living guide for GitHub/CI integrations; update when we adopt new tooling.

### Reliability & Safety

- **Dependabot (built-in, free)**: Auto-PRs for vulnerable or outdated dependencies.\
  *Setup:* Repo → Settings → Code security & analysis → Enable Dependabot alerts & updates.
- **Renovate Bot (free for OSS)**: Smarter dependency updates (batching, schedules).\
  *Setup:* Install Renovate app → add `renovate.json`.
- **CodeQL (built-in, free for public)**: Security & static analysis on PRs.\
  *Setup:* Repo → Security → Code scanning → Set up CodeQL.
- **UptimeRobot / Better Stack (free tier)**: Monitor any docs/demo/status URLs; alert to Discord.\
  *Setup:* Create monitor → send alerts to Discord webhook.

### Code Quality (C/C++)

- **clang-tidy + cppcheck (free)**: Static analysis for C/C++ code.\
  *Setup:* Add a GitHub Actions job to run both and comment on diffs.
- **Sanitizers (ASan/UBSan/TSan)**: Detect memory, UB, or threading issues in CI.\
  *Setup:* Build with `-fsanitize=address,undefined` or `-fsanitize=thread`.
- **SonarCloud (free for OSS)**: Deep code analysis and trend metrics.\
  *Setup:* Sign in with GitHub → create project → add SonarCloud GitHub Action.
- **Include-What-You-Use (free)**: Reduces include bloat.\
  *Setup:* Optional nightly workflow that reports suggestions.

### Testing & Coverage

- **ctest + GitHub Actions (free)**: Cross-platform builds & tests (Ubuntu/macOS).\
  *Setup:* Matrix build with `cmake .. && cmake --build . && ctest`.
- **Codecov (free for OSS)**: Coverage tracking & PR badges.\
  *Setup:* Upload via `codecov/codecov-action` after running tests.

### Release Automation

- **Release Drafter (free)**: Auto-generates changelog drafts from labels.\
  *Setup:* Add `release-drafter.yml` + workflow.
- **GitHub Release Artifacts**: Attach compiled binaries for macOS/Linux.\
  *Setup:* On tag → build → `actions/upload-release-asset`.
- **Homebrew Tap (optional)**: Publish a Homebrew formula on release.\
  *Setup:* Once binaries stabilize.

### Repo Hygiene & Workflow

- **CODEOWNERS (built-in)**: Auto-request reviewers for key paths.\
  *Setup:* Add `CODEOWNERS` file.
- **Issue Forms (built-in)**: Structured bug/feature templates.\
  *Setup:* `.github/ISSUE_TEMPLATE/*.yml`.
- **PR Labeler (free)**: Auto-label PRs by file paths.\
  *Setup:* `actions/labeler` + `labeler.yml`.
- **Stale Bot (free)**: Auto-close inactive issues (with long grace period).\
  *Setup:* `actions/stale` + config file.
- **CLA Assistant (free for public)**: Collect Contributor License Agreements.\
  *Setup:* Install app → configure CLA.

### Discord Integration Ideas

- **CI Failure Alerts:** Only failed builds post to a private `#alerts` channel.
- **Release Notifications:** Post release embeds to `#announcements`.
- **PR Threads:** Auto-create a Discord thread per PR summary for discussion.

### Monitoring (no‑CI options)

**Option A — UptimeRobot / Better Stack → Discord (fastest)**

1. Create a monitor for your health endpoint (e.g., `https://SERVER/health`).
2. Add a **Discord webhook** as the notification target.
3. Route to `#alerts` (private) and keep success notifications off.

**Option B — Scheduled GitHub Action (runs every 5 min)** Add a small workflow to ping your server and alert on failures. This is independent of CI and free for public repos.

```yaml
# .github/workflows/monitor.yml
name: Server Monitor
on:
  schedule: [{ cron: "*/5 * * * *" }]
  workflow_dispatch:
jobs:
  check:
    runs-on: ubuntu-latest
    steps:
      - name: Ping health
        id: ping
        env:
          HEALTH_URL: https://your.server/health
        run: |
          set -e
          if ! curl -fsS --max-time 10 "$HEALTH_URL" >/dev/null; then
            echo "ok=false" >> $GITHUB_OUTPUT
          else
            echo "ok=true" >> $GITHUB_OUTPUT
          fi

      - name: Notify Discord on failure
        if: steps.ping.outputs.ok != 'true'
        env:
          DISCORD_WEBHOOK: ${{ secrets.DISCORD_WEBHOOK_MONITOR }}
          HEALTH_URL: https://your.server/health
        run: |
          payload='{"embeds":[{"title":"Server DOWN","description":"'"$HEALTH_URL"'","timestamp":"'$(date -u +%Y-%m-%dT%H:%M:%SZ)'"}]}'
          curl -s -X POST -H "Content-Type: application/json" -d "$payload" "$DISCORD_WEBHOOK" >/dev/null

      - name: Open or comment on issue
        if: steps.ping.outputs.ok != 'true'
        uses: actions/github-script@v7
        with:
          github-token: ${{ secrets.GITHUB_TOKEN }}
          script: |
            const title = `Server down: https://your.server/health`;
            const { data } = await github.search.issuesAndPullRequests({
              q: `repo:${context.repo.owner}/${context.repo.repo} is:issue is:open in:title "${title}"`
            });
            if (data.items.length) {
              await github.rest.issues.createComment({
                owner: context.repo.owner,
                repo: context.repo.repo,
                issue_number: data.items[0].number,
                body: `Another failure at ${new Date().toISOString()}.`
              });
            } else {
              await github.rest.issues.create({
                owner: context.repo.owner,
                repo: context.repo.repo,
                title,
                labels: ['infra','bug'],
                body: `Automated check failed at ${new Date().toISOString()}.`
              });
            }
```

*Secrets needed:* `DISCORD_WEBHOOK_MONITOR` (channel webhook URL). You can later add an **UP** message when service recovers if you want.

**Option C — Tiny server-side cron script (no GitHub minutes)** Run a local script that posts to Discord and opens/comments a GitHub Issue on failure.

```bash
#!/usr/bin/env bash
set -euo pipefail
HEALTH_URL="https://your.server/health"
DISCORD_WEBHOOK="https://discord.com/api/webhooks/..."
GITHUB_REPO="jsavin/Frontier"
GITHUB_TOKEN="ghp_..."  # fine-scoped PAT (Issues: write)
TTL=10
if ! curl -fsS --max-time "$TTL" "$HEALTH_URL" >/dev/null; then
  curl -s -X POST -H 'Content-Type: application/json' \
    -d '{"embeds":[{"title":"Server DOWN","description":"'$HEALTH_URL'"}]}' "$DISCORD_WEBHOOK" >/dev/null
  title="Server down: $HEALTH_URL"
  existing=$(curl -s -H "Authorization: Bearer $GITHUB_TOKEN" \
    "https://api.github.com/search/issues?q=repo:$GITHUB_REPO+is:issue+is:open+in:title+$(printf %s "$title" | sed 's/ /+/g')" | jq -r '.items[0].number // empty')
  if [ -n "$existing" ]; then
    curl -s -X POST -H "Authorization: Bearer $GITHUB_TOKEN" -H 'Content-Type: application/json' \
      -d '{"body":"Another failure at '$(date -u +%Y-%m-%dT%H:%M:%SZ)'."}' \
      "https://api.github.com/repos/$GITHUB_REPO/issues/$existing/comments" >/dev/null
  else
    curl -s -X POST -H "Authorization: Bearer $GITHUB_TOKEN" -H 'Content-Type: application/json' \
      -d '{"title":"'$title'","body":"Automated check failed.","labels":["infra","bug"]}' \
      "https://api.github.com/repos/$GITHUB_REPO/issues" >/dev/null
  fi
fi
```

Cron example: `*/5 * * * * /usr/local/bin/monitor.sh`

### Static Analysis (local, no‑CI)

Run these locally any time—useful even before enabling CI.

```bash
# Generate compile_commands.json (CMake project)
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# cppcheck (sane defaults for C)
cppcheck --enable=warning,performance,portability,style \
         --std=c11 --force --inline-suppr --project=build/compile_commands.json

# clang-tidy on tracked C files (C-friendly checks)
ln -sf build/compile_commands.json .
clang-tidy -p . \
  -checks='clang-analyzer-*,bugprone-*,cert-*,misc-*,readability-*,-modernize-*' \
  $(git ls-files '*.c' '*.h')
```

If you don’t use CMake, generate `compile_commands.json` with **Bear** or **intercept-build**.

### Reviewdog Static Analysis (commented out for later)

When you're ready to enable automatic clang-tidy/cppcheck reviews on PRs, copy this file to `.github/workflows/reviewdog.yml` and remove the leading `#` from each line. (For a C project, we explicitly \*\*exclude \*\*\`\` checks.)

```yaml
# name: Static Analysis (reviewdog)
# on:
#   pull_request:
#     paths: ["**/*.c", "**/*.h"]
# jobs:
#   cppcheck:
#     runs-on: ubuntu-latest
#     steps:
#       - uses: actions/checkout@v4
#       - run: sudo apt-get update && sudo apt-get install -y cppcheck
#       - name: cppcheck via reviewdog
#         uses: reviewdog/action-cppcheck@v2
#         with:
#           github_token: ${{ secrets.GITHUB_TOKEN }}
#           reporter: github-pr-review
#           level: warning
#           cppcheck_flags: >
#             --enable=warning,performance,portability,style
#             --std=c11 --inline-suppr
#   clang-tidy:
#     runs-on: ubuntu-latest
#     steps:
#       - uses: actions/checkout@v4
#       - run: sudo apt-get update && sudo apt-get install -y clang clang-tidy cmake
#       - run: cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
#       - run: ln -sf build/compile_commands.json .
#       - name: clang-tidy via reviewdog (changed files only)
#         uses: reviewdog/action-clang-tidy@v3
#         with:
#           github_token: ${{ secrets.GITHUB_TOKEN }}
#           reporter: github-pr-review
#           filter_mode: diff_context
#           clang_tidy_flags: >
#             -checks=clang-analyzer-*,bugprone-*,cert-*,misc-*,readability-*,-modernize-*
```

### Recommended Startup Order

1. CI (build + test + sanitizers)
2. CodeQL scanning
3. Release Drafter + PR Labeler
4. clang-tidy / cppcheck
5. Codecov (when tests are ready)
6. Dependabot or Renovate (pick one)

### Example CI & Tool Workflows

**1) Build + clang-tidy + Sanitizers + Tests**

```yaml
# .github/workflows/ci.yml
name: CI
on: [push, pull_request]
jobs:
  build-test:
    strategy:
      matrix:
        os: [ubuntu-latest, macos-latest]
        cc: [clang]
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4
      - name: Configure
        run: |
          cmake -S . -B build -DCMAKE_C_COMPILER=${{ matrix.cc }} -DCMAKE_C_FLAGS="-fsanitize=address,undefined -O1 -fno-omit-frame-pointer" -DCMAKE_BUILD_TYPE=Debug
      - name: Build
        run: cmake --build build --parallel
      - name: clang-tidy
        run: |
          if command -v clang-tidy >/dev/null; then
            cmake -S . -B build-tidy -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
            clang-tidy -p build-tidy $(git ls-files '*.c' '*.cc' '*.cpp') || true
          fi
      - name: Test
        run: ctest --test-dir build --output-on-failure
```

**2) CodeQL**

```yaml
# .github/workflows/codeql.yml
name: CodeQL
on:
  push: {branches: [main]}
  pull_request: {branches: [main]}
  schedule: [{cron: '0 3 * * 1'}]
jobs:
  analyze:
    uses: github/codeql-action/.github/workflows/analyze.yml@v3
    with:
      languages: cpp
```

**3) Release Drafter**

```yaml
# .github/workflows/release-drafter.yml
name: Release Drafter
on:
  push: {branches: [main]}
  pull_request:
    types: [opened, reopened, synchronize, closed, labeled, unlabeled]
  release:
    types: [published]
jobs:
  update:
    runs-on: ubuntu-latest
    steps:
      - uses: release-drafter/release-drafter@v6
        with: {config-name: release-drafter.yml}
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

**4) Release Drafter Config**

```yaml
# .github/release-drafter.yml
name-template: "v$RESOLVED_VERSION"
tag-template: "v$RESOLVED_VERSION"
categories:
  - title: "🚀 Features"
    labels: ["feature", "enhancement"]
  - title: "🐛 Fixes"
    labels: ["bug", "fix"]
  - title: "🧰 Maintenance"
    labels: ["chore", "deps"]
change-template: "- $TITLE (#$NUMBER) @$AUTHOR"
```

**5) Codecov**

```yaml
- name: Upload Coverage
  uses: codecov/codecov-action@v4
  with:
    fail_ci_if_error: true
```
