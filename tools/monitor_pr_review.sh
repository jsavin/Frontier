#!/bin/bash

# Monitor PR for new bot review comments
# Usage: ./monitor_pr_review.sh <pr_number> [timeout_seconds]
# Default timeout: 600 seconds (10 minutes)
# Only detects comments created AFTER the script starts

PR_NUMBER="${1:-162}"
TIMEOUT="${2:-600}"
POLL_INTERVAL=5

echo "[$(date)] Starting PR #$PR_NUMBER review monitor (timeout: ${TIMEOUT}s)"
echo "[$(date)] Getting initial comment count..."

# Get the initial number of comments to detect new ones
INITIAL_COUNT=$(gh pr view "$PR_NUMBER" --json comments --jq '.comments | length' 2>/dev/null)

if [ -z "$INITIAL_COUNT" ] || [ "$INITIAL_COUNT" -eq 0 ]; then
    echo "[$(date)] No initial comments found"
    INITIAL_COUNT=0
fi

echo "[$(date)] Initial comment count: $INITIAL_COUNT"
echo "[$(date)] Monitoring for new comments..."
echo ""

START_TIME=$(date +%s)

while true; do
    CURRENT_TIME=$(date +%s)
    ELAPSED=$((CURRENT_TIME - START_TIME))

    if [ $ELAPSED -gt $TIMEOUT ]; then
        echo "[$(date)] TIMEOUT: No new review found after ${TIMEOUT}s"
        exit 1
    fi

    # Get current number of comments
    CURRENT_COUNT=$(gh pr view "$PR_NUMBER" --json comments --jq '.comments | length' 2>/dev/null)

    if [ -z "$CURRENT_COUNT" ]; then
        echo "[$(date)] Error querying PR comments"
        sleep $POLL_INTERVAL
        continue
    fi

    # Check if there's a new comment
    if [ "$CURRENT_COUNT" -gt "$INITIAL_COUNT" ]; then
        # Get the latest comment
        LATEST=$(gh pr view "$PR_NUMBER" --json comments --jq '.comments[-1]' 2>/dev/null)

        if [ -z "$LATEST" ]; then
            sleep $POLL_INTERVAL
            continue
        fi

        # Extract comment info
        COMMENT_ID=$(echo "$LATEST" | jq -r '.id')
        AUTHOR=$(echo "$LATEST" | jq -r '.author.login')
        CREATED_AT=$(echo "$LATEST" | jq -r '.createdAt')
        BODY=$(echo "$LATEST" | jq -r '.body')

        echo "=========================================="
        echo "NEW BOT REVIEW DETECTED"
        echo "=========================================="
        echo "Comment ID: $COMMENT_ID"
        echo "Author: $AUTHOR"
        echo "Created: $CREATED_AT"
        echo "Elapsed time: ${ELAPSED}s"
        echo "=========================================="
        echo ""
        echo "$BODY"
        echo ""
        echo "=========================================="
        exit 0
    fi

    echo "[$(date)] Waiting for new review... ($ELAPSED/${TIMEOUT}s) Current comments: $CURRENT_COUNT"
    sleep $POLL_INTERVAL
done
