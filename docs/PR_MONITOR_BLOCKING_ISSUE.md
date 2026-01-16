# PR Monitor Blocking Issue - Known Problem

## Problem

When using the pull-request agent to create PRs and monitor for review feedback, the session freezes and becomes unresponsive. This occurs because:

1. The pull-request agent invokes `monitor_pr_review.sh` in the **foreground**
2. The monitor script polls for reviews every 5 seconds for up to **900 seconds (15 minutes)**
3. If no reviews arrive, it blocks for the full 15 minutes with no escape route
4. The session remains frozen - Ctrl-C doesn't work because the process is buried in sub-shells
5. Terminal must be force-killed

This has occurred multiple times in single sessions when creating multiple PRs.

## Why This Happens

The monitor script is designed to:
- Wait for bot reviews to arrive (which legitimately takes a few minutes)
- Poll every 5 seconds and display reviews as they come in
- Exit after 2 minutes of inactivity following review detection
- OR timeout after 900 seconds of total monitoring

However, if a PR gets created but no reviews are submitted (e.g., PR is still being reviewed by bots), the agent blocks for the full 15 minutes.

## Workaround - Until Pull-Request Agent is Fixed

### Option A: Manual Background Monitoring (Recommended)

After the pull-request agent creates your PR:

```bash
# In a SEPARATE terminal (don't wait for this in same terminal)
cd /Users/jake/dev/jsavin/Frontier
./tools/monitor_pr_review_bg.sh <PR_NUMBER>

# Watch the log in your original terminal
tail -f tests/tmp/pr_monitor_<PR_NUMBER>.log
```

This returns immediately and runs monitoring in the background.

### Option B: Kill Frozen Session

If your session freezes:

```bash
# In another terminal
pkill -f "monitor_pr_review.sh"
# OR find specific process
ps aux | grep monitor_pr_review
kill -9 <PID>
```

## Recommended Feedback to Anthropic

File issue at https://github.com/anthropics/claude-code/issues:

**Title**: `pull-request agent blocks on PR monitoring - causes session freeze`

**Description**:
The pull-request agent invokes `monitor_pr_review.sh` in the foreground, causing session freeze for up to 15 minutes when monitoring for PR reviews. This is particularly problematic when creating multiple PRs in sequence.

**Suggested Fix**:
The agent should invoke the monitor script in background:
```bash
./tools/monitor_pr_review.sh <PR_NUMBER> &
# Returns immediately instead of blocking
```

---

## Root Cause Analysis

**File**: `tools/monitor_pr_review.sh` lines 131-139

The exit logic requires either:
1. Timeout after 900 seconds, OR
2. Reviews detected + 2-minute cooldown

If reviews never arrive, case 2 never triggers and it blocks for the full 15 minutes.

A proper fix would require making the agent invoke the script non-blocking, which is outside the scope of this script.
