# PR Monitor Blocking Issue - RESOLVED ✅

## Problem (Previously)

The pull-request agent would invoke `monitor_pr_review.sh` in the foreground, causing session freeze for up to 15 minutes. The session became unresponsive and could not be interrupted.

## Solution (Implemented)

`monitor_pr_review.sh` now **auto-backgrounds itself** using environment variable detection.

### How It Works

When `monitor_pr_review.sh` is invoked:

1. **First call (foreground)**:
   - Checks `MONITOR_PR_REVIEW_BACKGROUNDED` environment variable
   - If not set, re-execs itself in background with `nohup` and sets the marker variable
   - Parent process returns immediately with PID information
   - Returns to user's prompt

2. **Background execution**:
   - Child process continues with monitoring logic
   - Output logged to `tests/tmp/pr_monitor_<PR_NUMBER>.log`
   - Runs independently - no blocking

**Key Result**: No matter how the script is invoked (by agent, manually, or in scripts), it **always returns immediately** and runs in the background.

## Usage

```bash
# Start background monitor (returns immediately)
./tools/monitor_pr_review.sh <PR_NUMBER>

# Watch the output asynchronously
tail -f tests/tmp/pr_monitor_<PR_NUMBER>.log

# Kill if needed
kill <PID>  # PID shown when monitor starts
```

## Why This is Deterministic

The auto-background mechanism uses:
- **Environment variable marker** (`MONITOR_PR_REVIEW_BACKGROUNDED`)
- **`nohup` + process backgrounding** for true background execution
- **Shell re-execution** to ensure child inherits marker and continues normally

This means:
- ✅ No race conditions
- ✅ Works with `pull-request` agent
- ✅ Works with manual invocation
- ✅ Works in scripts without explicit `&`
- ✅ Impossible to accidentally block

## Removed

- `tools/monitor_pr_review_bg.sh` - No longer needed, consolidated into single canonical script
- Previous workaround documentation - No longer applicable
