#!/usr/bin/env bash
set -euo pipefail

# Regenerate langparser.c (and optionally langparser.h) from langparser.y using Bison 3.x
# This script does NOT overwrite files by default — it emits to tmp/ and shows a diff.
# Usage examples:
#   scripts/gen_langparser.sh              # dry-run, outputs to tmp/parser/
#   scripts/gen_langparser.sh --apply      # overwrite Common/source/langparser.c (and header if requested)
# Options:
#   --apply-header  also overwrite Common/headers/langparser.h with generated header

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
YFILE="$ROOT_DIR/Common/source/langparser.y"
OUT_C="$ROOT_DIR/Common/source/langparser.c"
OUT_H="$ROOT_DIR/Common/headers/langparser.h"
TMPDIR="$ROOT_DIR/tmp/parser"
APPLY=0
APPLY_HEADER=0

for arg in "$@"; do
  case "$arg" in
    --apply) APPLY=1 ;;
    --apply-header) APPLY_HEADER=1 ;;
    *) echo "Unknown option: $arg" >&2; exit 2 ;;
  esac
done

mkdir -p "$TMPDIR"

if ! command -v bison >/dev/null 2>&1; then
  echo "Error: bison not found in PATH" >&2
  exit 1
fi

echo "bison version: $(bison --version | head -1)"

# Generate to tmp
GEN_C="$TMPDIR/langparser.c"
GEN_H="$TMPDIR/langparser.h"

set -x
bison -y -o "$GEN_C" --defines="$GEN_H" "$YFILE"
set +x

echo
echo "Generated:" 
echo "  $GEN_C"
echo "  $GEN_H"

echo
echo "Diff (C):"
echo "----------------------------------------"
diff -u "$OUT_C" "$GEN_C" || true
echo "----------------------------------------"

echo
echo "Diff (H):"
echo "----------------------------------------"
diff -u "$OUT_H" "$GEN_H" || true
echo "----------------------------------------"

if [[ "$APPLY" -eq 1 ]]; then
  echo
  echo "Applying regenerated C to $OUT_C"
  cp "$GEN_C" "$OUT_C"
  if [[ "$APPLY_HEADER" -eq 1 ]]; then
    echo "Applying regenerated H to $OUT_H"
    cp "$GEN_H" "$OUT_H"
  else
    echo "(Header not applied; pass --apply-header to also overwrite)"
  fi
fi

echo
echo "Done. Review diffs above."

