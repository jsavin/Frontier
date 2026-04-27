# Frontier Quick Reference

Bash commands for common Frontier development tasks. Pulled out of `CLAUDE.md` to keep that file lean — agents read this on demand.

## Build

```bash
# Build the CLI (universal binary for arm64 + x86_64)
make -C frontier-cli
```

## Test

```bash
# Full unit test suite
./tools/run_headless_tests.sh

# Integration tests (Python/YAML-based verb tests)
cd tests && make test-integration

# All tests (unit + integration)
cd tests && make test-all
```

## Database

```bash
# Migration v6 → v7 (in-place, with .v6.root backup)
./frontier-cli/frontier-cli --migrate databases/Frontier.root
```

v7 databases use `.root` extension (same as v6). `.root7` is recognized for backward compatibility but deprecated.

## Verb Coverage

```bash
# Coverage report
cd tools/kernelverbs_parser && python3 cli.py report

# Export integration tests to OPML
python3 tools/export_tests_to_opml.py
# Output: reports/integration_tests.opml
```

## Git / PR

```bash
# Install git hooks
./tools/install_git_hooks.sh

# Create PR (after pushing branch)
gh pr create --title "..." --body "..." --base develop

# Background PR monitor (auto-backgrounds itself)
./tools/monitor_pr_review.sh <PR_NUMBER>

# Watch monitor log
tail -f tests/tmp/pr_monitor_<PR_NUMBER>.log
```

## Test Output Directory Structure

- `tests/tmp/unit/` — C unit test outputs
- `tests/tmp/integration/` — Integration test outputs
- `tests/tmp/migration/` — Migration test artifacts
- `tests/tmp/results/` — Test logs and CLI runtime artifacts

For test scratch files, `/tmp` or `tests/tmp/` both work.
