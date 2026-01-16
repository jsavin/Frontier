#!/bin/bash

# Non-blocking PR review monitor
# Runs monitor_pr_review.sh in background and returns immediately
# Usage: ./monitor_pr_review_bg.sh <pr_number> [timeout_seconds]
#
# Output is logged to: tests/tmp/pr_monitor_<pr_number>.log
# Check log with: tail -f tests/tmp/pr_monitor_<pr_number>.log

PR_NUMBER="${1:-}"
TIMEOUT="${2:-900}"

if [ -z "$PR_NUMBER" ]; then
    echo "Usage: $0 <pr_number> [timeout_seconds]"
    exit 1
fi

# Ensure log directory exists
mkdir -p tests/tmp
LOG_FILE="tests/tmp/pr_monitor_${PR_NUMBER}.log"

echo "[$(date)] Starting background PR monitor for #$PR_NUMBER (timeout: ${TIMEOUT}s)"
echo "[$(date)] Output will be logged to: $LOG_FILE"
echo ""

# Run monitor in background, redirecting output to log file
nohup ./tools/monitor_pr_review.sh "$PR_NUMBER" "$TIMEOUT" >> "$LOG_FILE" 2>&1 &
MONITOR_PID=$!

echo "[$(date)] Monitor started with PID $MONITOR_PID"
echo "[$(date)] Watch with: tail -f $LOG_FILE"
echo "[$(date)] Kill with: kill $MONITOR_PID"
echo ""

exit 0
