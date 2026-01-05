#!/bin/bash
# Check for potentially problematic datetime type usage
#
# This script scans C source files for datetime-related type issues that could
# cause 32-bit truncation on some platforms. It checks for:
# 1. 'long' or 'unsigned long' used with timestamp field names
# 2. int32_t/uint32_t used with timestamp fields (needs review)
# 3. Function parameters using 'long' for date/time values
#
# Whitelisted files (Mac GUI only, not used in headless):
# - Common/headers/claybrowser.h
# - portable/shelltypes_portable.h
#
# Exit codes:
#   0 - No critical issues found
#   1 - Critical issues found (blocks merge)

ERRORS=0
WARNINGS=0

# Files to skip (Mac GUI only or legacy disk formats)
WHITELIST=(
    "Common/headers/claybrowser.h"      # Mac GUI file browser
    "Common/source/claybrowserexpand.c" # Mac GUI file browser
    "portable/shelltypes_portable.h"    # Mac GUI shell types
    "portable/wptext_runtime.c"         # Contains legacy wp_diskheader (Mac disk format)
)

echo "Checking for datetime type issues..."

FILES=$(find Common/source Common/headers portable -name "*.c" -o -name "*.h" 2>/dev/null | grep -v legacy)

if [ -z "$FILES" ]; then
    echo "⚠️  Warning: No source files found. Are you in the project root?"
    exit 0
fi

# Check if file is whitelisted
is_whitelisted() {
    local file="$1"
    for wl in "${WHITELIST[@]}"; do
        if [ "$file" = "$wl" ]; then
            return 0
        fi
    done
    return 1
}

for FILE in $FILES; do
    # Skip whitelisted files
    if is_whitelisted "$FILE"; then
        continue
    fi
    # Pattern 1: long/unsigned long with timestamp field names
    # Catches: long timecreated, unsigned long timemodified, etc.
    # Excludes: byte swap function calls, tick counters
    MATCHES=$(grep -n "\(unsigned \)\?long.*\(timecreated\|timemodified\|timelastsave\)" "$FILE" 2>/dev/null | \
       grep -v "tick\|double.*click\|keyboard" | \
       grep -v "conditionallongswap\|conditionallonglongswap")

    if [ -n "$MATCHES" ]; then
        echo "❌ $FILE: Found 'long' with timestamp field name"
        echo "$MATCHES" | sed 's/^/   /'
        ERRORS=$((ERRORS + 1))
    fi

    # Pattern 2: int32_t/uint32_t with timestamp field names
    # These need manual review: disk format (OK) vs in-memory (BAD)
    # Excludes: byte swap function calls, lines with legacy-disk-format comment
    MATCHES=$(grep -n "\(u\)\?int32_t.*\(timecreated\|timemodified\|timelastsave\)" "$FILE" 2>/dev/null | \
       grep -v "conditionallongswap\|host_to_disk\|disk_to_host" | \
       grep -v "legacy-disk-format")

    if [ -n "$MATCHES" ]; then
        echo "⚠️  $FILE: Found int32_t/uint32_t with timestamp field (review: disk format OK, in-memory BAD)"
        echo "$MATCHES" | sed 's/^/   /'
        WARNINGS=$((WARNINGS + 1))
    fi

    # Pattern 3: Function parameters using 'long' for date/time
    # Catches: boolean func(long localdate, ...) or boolean func(unsigned long time, ...)
    MATCHES=$(grep -n "boolean.*(\(unsigned \)\?long \(local\)\?date\|boolean.*(\(unsigned \)\?long.*time" "$FILE" 2>/dev/null | \
       grep -v "tick\|timeout\|timeslice")

    if [ -n "$MATCHES" ]; then
        echo "❌ $FILE: Found 'long' in date/time function parameter"
        echo "$MATCHES" | sed 's/^/   /'
        ERRORS=$((ERRORS + 1))
    fi
done

echo ""
if [ $ERRORS -gt 0 ]; then
    echo "❌ Found $ERRORS critical datetime type issue(s)"
    echo "   Use int64_t or frontier_time_t for timestamps"
    echo ""
    echo "   See docs/frontier_time_t_standard.md for guidance"
fi

if [ $WARNINGS -gt 0 ]; then
    echo "⚠️  Found $WARNINGS potential issue(s) needing manual review"
    echo "   uint32_t is OK for disk formats, BAD for in-memory structures"
    echo "   See planning/phase3/datetime_handling_audit.md for details"
fi

if [ $ERRORS -gt 0 ]; then
    exit 1
else
    echo "✅ No critical datetime type issues detected"
    exit 0
fi
