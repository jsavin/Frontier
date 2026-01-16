#!/bin/bash

# Monitor PR for new bot review comments and reviews (DETERMINISTIC VERSION)
# Usage: ./monitor_pr_review.sh <pr_number> [timeout_seconds]
#
# Three-phase design for reliable bot monitoring:
#   Phase 1: WAIT (30s) - Give bots time to pick up commit and start analyzing
#   Phase 2: POLL (until review detected or timeout) - Check for incoming reviews
#   Phase 3: COOLDOWN (30s after last review) - Wait for any final reviews
#
# Default timeout: 900 seconds (15 minutes)
# Fixed polling: 15-second intervals (reduces API calls & log spam)
# Clean exit: Logs reason for exit (timeout/cooldown/no-reviews)

# Configuration
PR_NUMBER="${1:-}"
TIMEOUT="${2:-900}"
WAIT_FOR_BOTS=30     # Phase 1: Initial wait for bots to pick up commit
POLL_INTERVAL=15     # Poll every 15s (not too aggressive)
COOLDOWN_AFTER_REVIEW=30  # Phase 3: How long to wait after last review

if [ -z "$PR_NUMBER" ]; then
    echo "Usage: $0 <pr_number> [timeout_seconds]"
    exit 1
fi

# Single consistent timestamp reference
START_TIME=$(date +%s)

# Fetch initial state once
echo "[$(date)] Starting PR #$PR_NUMBER review monitor (timeout: ${TIMEOUT}s)"
INITIAL_COMMENTS=$(gh pr view "$PR_NUMBER" --json comments --jq '.comments | length' 2>/dev/null || echo "0")
INITIAL_REVIEWS=$(gh pr view "$PR_NUMBER" --json reviews --jq '.reviews | length' 2>/dev/null || echo "0")

echo "[$(date)] Initial state: $INITIAL_COMMENTS comments, $INITIAL_REVIEWS reviews"

# Check for merge conflicts (blocks bot reviews)
echo "[$(date)] Checking for merge conflicts..."
MERGEABLE=$(gh pr view "$PR_NUMBER" --json mergeable --jq '.mergeable' 2>/dev/null)
if [ "$MERGEABLE" = "CONFLICTING" ]; then
    echo "[$(date)] ❌ MERGE CONFLICT DETECTED - Cannot proceed with bot review"
    echo "[$(date)] Resolve conflict and rebase:"
    echo "[$(date)]   git fetch origin && git rebase origin/develop"
    echo "[$(date)]   git push -f origin <branch-name>"
    exit 1
fi

if [ "$MERGEABLE" != "MERGEABLE" ]; then
    echo "[$(date)] ⚠️  Merge status: $MERGEABLE (expected: MERGEABLE)"
fi

# ============================================================================
# PHASE 1: Wait for bots to pick up the commit and start analyzing
# ============================================================================
echo "[$(date)] PHASE 1: Waiting ${WAIT_FOR_BOTS}s for bots to pick up commit..."
sleep $WAIT_FOR_BOTS

# ============================================================================
# PHASE 2: Poll for reviews until one appears or timeout
# ============================================================================
echo "[$(date)] PHASE 2: Polling for reviews..."

LAST_COMMENT_COUNT=$INITIAL_COMMENTS
LAST_REVIEW_COUNT=$INITIAL_REVIEWS
REVIEWS_DETECTED=0
LAST_NEW_REVIEW_TIME=0
FIRST_REVIEW_DETECTED_TIME=0

while true; do
    CURRENT_TIME=$(date +%s)
    ELAPSED=$((CURRENT_TIME - START_TIME))

    # Check overall timeout
    if [ $ELAPSED -gt $TIMEOUT ]; then
        echo "[$(date)] TIMEOUT: Overall monitoring limit (${TIMEOUT}s) exceeded"
        echo "[$(date)] Reviews detected: $REVIEWS_DETECTED"
        exit 0
    fi

    # Fetch current state
    CURRENT_COMMENTS=$(gh pr view "$PR_NUMBER" --json comments --jq '.comments | length' 2>/dev/null || echo "$LAST_COMMENT_COUNT")
    CURRENT_REVIEWS=$(gh pr view "$PR_NUMBER" --json reviews --jq '.reviews | length' 2>/dev/null || echo "$LAST_REVIEW_COUNT")

    # Check for new comments (bot reviews as comments)
    if [ "$CURRENT_COMMENTS" -gt "$LAST_COMMENT_COUNT" ]; then
        for ((i=$LAST_COMMENT_COUNT; i<$CURRENT_COMMENTS; i++)); do
            COMMENT=$(gh pr view "$PR_NUMBER" --json comments --jq ".comments[$i]" 2>/dev/null)
            if [ -n "$COMMENT" ]; then
                AUTHOR=$(echo "$COMMENT" | jq -r '.author.login')
                CREATED=$(echo "$COMMENT" | jq -r '.createdAt')
                BODY=$(echo "$COMMENT" | jq -r '.body')

                echo ""
                echo "=========================================="
                echo "NEW REVIEW COMMENT (${AUTHOR}) at ${CREATED}"
                echo "=========================================="
                echo "$BODY"
                echo "=========================================="
                echo ""

                REVIEWS_DETECTED=$((REVIEWS_DETECTED + 1))
                LAST_NEW_REVIEW_TIME=$CURRENT_TIME
                if [ $FIRST_REVIEW_DETECTED_TIME -eq 0 ]; then
                    FIRST_REVIEW_DETECTED_TIME=$CURRENT_TIME
                fi
            fi
        done
        LAST_COMMENT_COUNT=$CURRENT_COMMENTS
    fi

    # Check for new reviews (GitHub review system)
    if [ "$CURRENT_REVIEWS" -gt "$LAST_REVIEW_COUNT" ]; then
        for ((i=$LAST_REVIEW_COUNT; i<$CURRENT_REVIEWS; i++)); do
            REVIEW=$(gh pr view "$PR_NUMBER" --json reviews --jq ".reviews[$i]" 2>/dev/null)
            if [ -n "$REVIEW" ]; then
                AUTHOR=$(echo "$REVIEW" | jq -r '.author.login')
                STATE=$(echo "$REVIEW" | jq -r '.state')
                SUBMITTED=$(echo "$REVIEW" | jq -r '.submittedAt')
                BODY=$(echo "$REVIEW" | jq -r '.body')

                echo ""
                echo "=========================================="
                echo "NEW REVIEW (${AUTHOR}, ${STATE}) at ${SUBMITTED}"
                echo "=========================================="
                echo "$BODY"
                echo "=========================================="
                echo ""

                REVIEWS_DETECTED=$((REVIEWS_DETECTED + 1))
                LAST_NEW_REVIEW_TIME=$CURRENT_TIME
                if [ $FIRST_REVIEW_DETECTED_TIME -eq 0 ]; then
                    FIRST_REVIEW_DETECTED_TIME=$CURRENT_TIME
                fi
            fi
        done
        LAST_REVIEW_COUNT=$CURRENT_REVIEWS
    fi

    # ========================================================================
    # PHASE 3: Cooldown logic - Exit if no new reviews after detection
    # ========================================================================
    if [ $FIRST_REVIEW_DETECTED_TIME -gt 0 ]; then
        TIME_SINCE_LAST_REVIEW=$((CURRENT_TIME - LAST_NEW_REVIEW_TIME))
        if [ $TIME_SINCE_LAST_REVIEW -gt $COOLDOWN_AFTER_REVIEW ]; then
            echo "[$(date)] COOLDOWN: No new reviews for ${COOLDOWN_AFTER_REVIEW}s (detected: $REVIEWS_DETECTED)"
            exit 0
        fi
    fi

    # Status log (every poll interval)
    printf "[$(date)] Polling... (%3ds elapsed) | Comments: %d | Reviews: %d | Detected: %d\n" $ELAPSED $CURRENT_COMMENTS $CURRENT_REVIEWS $REVIEWS_DETECTED

    sleep $POLL_INTERVAL
done
