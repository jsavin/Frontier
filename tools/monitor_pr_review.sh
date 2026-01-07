#!/bin/bash

# Monitor PR for new bot review comments and reviews
# Usage: ./monitor_pr_review.sh <pr_number> [timeout_seconds]
# Default timeout: 900 seconds (15 minutes)
# Detects both comment-based reviews and GitHub review system reviews
# Continues monitoring until all review workflows complete or timeout

PR_NUMBER="${1:-162}"
TIMEOUT="${2:-900}"
POLL_INTERVAL=5

echo "[$(date)] Starting PR #$PR_NUMBER review monitor (timeout: ${TIMEOUT}s)"
echo "[$(date)] Getting initial counts..."

# Get the initial counts for both comments and reviews
INITIAL_COMMENTS=$(gh pr view "$PR_NUMBER" --json comments --jq '.comments | length' 2>/dev/null)
INITIAL_REVIEWS=$(gh pr view "$PR_NUMBER" --json reviews --jq '.reviews | length' 2>/dev/null)

if [ -z "$INITIAL_COMMENTS" ] || [ "$INITIAL_COMMENTS" -eq 0 ]; then
    INITIAL_COMMENTS=0
fi

if [ -z "$INITIAL_REVIEWS" ] || [ "$INITIAL_REVIEWS" -eq 0 ]; then
    INITIAL_REVIEWS=0
fi

echo "[$(date)] Initial comment count: $INITIAL_COMMENTS"
echo "[$(date)] Initial review count: $INITIAL_REVIEWS"
echo "[$(date)] Monitoring for new comments and reviews (will display all new reviews)..."
echo ""

START_TIME=$(date +%s)
LAST_COMMENT_COUNT=$INITIAL_COMMENTS
LAST_REVIEW_COUNT=$INITIAL_REVIEWS
REVIEWS_DETECTED=0

while true; do
    CURRENT_TIME=$(date +%s)
    ELAPSED=$((CURRENT_TIME - START_TIME))

    if [ $ELAPSED -gt $TIMEOUT ]; then
        echo "[$(date)] TIMEOUT: Monitoring complete after ${TIMEOUT}s"
        echo "Total reviews detected: $REVIEWS_DETECTED"
        exit 0
    fi

    # Get current counts
    CURRENT_COMMENTS=$(gh pr view "$PR_NUMBER" --json comments --jq '.comments | length' 2>/dev/null)
    CURRENT_REVIEWS=$(gh pr view "$PR_NUMBER" --json reviews --jq '.reviews | length' 2>/dev/null)

    if [ -z "$CURRENT_COMMENTS" ]; then
        CURRENT_COMMENTS=$LAST_COMMENT_COUNT
    fi

    if [ -z "$CURRENT_REVIEWS" ]; then
        CURRENT_REVIEWS=$LAST_REVIEW_COUNT
    fi

    # Check for new comment
    if [ "$CURRENT_COMMENTS" -gt "$LAST_COMMENT_COUNT" ]; then
        # Get all new comments since last check
        for ((i=$LAST_COMMENT_COUNT; i<$CURRENT_COMMENTS; i++)); do
            LATEST=$(gh pr view "$PR_NUMBER" --json comments --jq ".comments[$i]" 2>/dev/null)

            if [ -n "$LATEST" ]; then
                COMMENT_ID=$(echo "$LATEST" | jq -r '.id')
                AUTHOR=$(echo "$LATEST" | jq -r '.author.login')
                CREATED_AT=$(echo "$LATEST" | jq -r '.createdAt')
                BODY=$(echo "$LATEST" | jq -r '.body')

                echo "=========================================="
                echo "NEW BOT REVIEW DETECTED (Comment)"
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
                echo ""

                REVIEWS_DETECTED=$((REVIEWS_DETECTED + 1))
            fi
        done
        LAST_COMMENT_COUNT=$CURRENT_COMMENTS
    fi

    # Check for new review (GitHub review system)
    if [ "$CURRENT_REVIEWS" -gt "$LAST_REVIEW_COUNT" ]; then
        # Get all new reviews since last check
        for ((i=$LAST_REVIEW_COUNT; i<$CURRENT_REVIEWS; i++)); do
            LATEST=$(gh pr view "$PR_NUMBER" --json reviews --jq ".reviews[$i]" 2>/dev/null)

            if [ -n "$LATEST" ]; then
                REVIEW_ID=$(echo "$LATEST" | jq -r '.id')
                AUTHOR=$(echo "$LATEST" | jq -r '.author.login')
                STATE=$(echo "$LATEST" | jq -r '.state')
                SUBMITTED_AT=$(echo "$LATEST" | jq -r '.submittedAt')
                BODY=$(echo "$LATEST" | jq -r '.body')

                echo "=========================================="
                echo "NEW BOT REVIEW DETECTED (Review System)"
                echo "=========================================="
                echo "Review ID: $REVIEW_ID"
                echo "Author: $AUTHOR"
                echo "State: $STATE"
                echo "Submitted: $SUBMITTED_AT"
                echo "Elapsed time: ${ELAPSED}s"
                echo "=========================================="
                echo ""
                echo "$BODY"
                echo ""
                echo "=========================================="
                echo ""

                REVIEWS_DETECTED=$((REVIEWS_DETECTED + 1))
            fi
        done
        LAST_REVIEW_COUNT=$CURRENT_REVIEWS
    fi

    echo "[$(date)] Monitoring... ($ELAPSED/${TIMEOUT}s) Comments: $CURRENT_COMMENTS Reviews: $CURRENT_REVIEWS Detected: $REVIEWS_DETECTED"
    sleep $POLL_INTERVAL
done
