#!/bin/bash
#
# convert_ut_encoding.sh
#
# Converts all .ut files in usertalk_scripts/Frontier.root from:
#   - Classic Mac OS line endings (CR only) to Unix line endings (LF)
#   - Windows-1252 character encoding to UTF-8
#
# The .ut files were originally exported on Windows, so non-ASCII characters
# use Windows-1252 encoding (e.g., 0x93/0x94 = curly quotes, 0xAB/0xBB = «/»
# block comment delimiters). Line endings are classic Mac CR-only (\r).
#
# Usage:
#   ./tools/convert_ut_encoding.sh [--dry-run]
#
# Options:
#   --dry-run   Show what would be converted without modifying files

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
TARGET_DIR="$ROOT_DIR/usertalk_scripts/Frontier.root"

DRY_RUN=false
if [[ "${1:-}" == "--dry-run" ]]; then
    DRY_RUN=true
fi

if [[ ! -d "$TARGET_DIR" ]]; then
    echo "Error: Directory not found: $TARGET_DIR" >&2
    exit 1
fi

if ! command -v iconv &>/dev/null; then
    echo "Error: iconv not found." >&2
    exit 1
fi

total=0
converted=0
already_ok=0
errors=0

echo "Scanning .ut files in: $TARGET_DIR"
echo "Source encoding: Windows-1252 (CP1252)"
echo "Target encoding: UTF-8 with Unix (LF) line endings"
echo ""

while IFS= read -r -d '' file; do
    total=$((total + 1))

    # Convert Windows-1252 to UTF-8, then CR line endings to LF.
    #
    # This is safe to run unconditionally:
    #   - iconv is idempotent on pure ASCII (valid in both encodings)
    #   - tr '\r' '\n' converts CR->LF; harmless if already LF
    #   - Files that are already ASCII+LF remain unchanged

    if [[ "$DRY_RUN" == true ]]; then
        converted_content=$(iconv -f WINDOWS-1252 -t UTF-8 "$file" | tr '\r' '\n') || {
            echo "  ERROR: Failed to convert: $file"
            errors=$((errors + 1))
            continue
        }
        original_content=$(cat "$file")
        if [[ "$converted_content" != "$original_content" ]]; then
            echo "  WOULD CONVERT: $file"
            converted=$((converted + 1))
        else
            already_ok=$((already_ok + 1))
        fi
    else
        tmpfile="${file}.tmp.$$"
        if iconv -f WINDOWS-1252 -t UTF-8 "$file" | tr '\r' '\n' > "$tmpfile"; then
            mv "$tmpfile" "$file"
            converted=$((converted + 1))
        else
            echo "  ERROR: Failed to convert: $file" >&2
            rm -f "$tmpfile"
            errors=$((errors + 1))
        fi
    fi
done < <(find "$TARGET_DIR" -name '*.ut' -type f -print0)

echo ""
echo "=== Conversion Summary ==="
echo "Total .ut files found: $total"
if [[ "$DRY_RUN" == true ]]; then
    echo "Files that would change: $converted"
    echo "Files already correct:   $already_ok"
else
    echo "Files converted:         $converted"
fi
echo "Errors:                  $errors"

if [[ "$DRY_RUN" == true ]]; then
    echo ""
    echo "(Dry run - no files were modified)"
fi

if [[ $errors -gt 0 ]]; then
    exit 1
fi
