#!/usr/bin/env bash
# Run all configured Frontier test binaries, building each first.
# Canonical runner used by local_scripts wrapper(s).
#
# Filtering:
#   Set RUN_PATTERN (extended regex) to run only matching targets.
#   Examples:
#     RUN_PATTERN='^runtime_tests$'   ./tests/run_all_tests.sh
#     RUN_PATTERN='64bit'             ./tests/run_all_tests.sh
#     RUN_PATTERN='^(core_tests|handle_tests)$' ./tests/run_all_tests.sh

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

discover_targets() {
  local make_out
  if make_out=$(make -C "$REPO_DIR/tests" -pRrq : 2>/dev/null); then
    # Try to extract ALL_TESTS resolved value from make's database
    local all_line
    all_line=$(printf "%s\n" "$make_out" | awk '/^ALL_TESTS[[:space:]]*=/{print substr($0,index($0,"=" )+1)}' | tail -n1)
    # Tokenize
    if [[ -n "$all_line" ]]; then
      for t in $all_line; do
        [[ -n "$t" ]] && echo "$t"
      done | sort -u
      return 0
    fi
  fi

  # Fallback: parse Makefile variable blocks and explicit target rules
  local mf="$REPO_DIR/tests/Makefile"
  if [[ -f "$mf" ]]; then
    awk '
      BEGIN{collect=0}
      /^USERTALK_OBJECT_TESTS[[:space:]]*=/{collect=1; next}
      /^TEST_EXECUTABLES[[:space:]]*=/{collect=1; next}
      collect==1{
        # gather items until an empty line or a non-indented line without backslash continuation
        for (i=1;i<=NF;i++) {
          gsub(/\\/,"",$i);
          if ($i!="\\") print $i;
        }
        if ($0 !~ /\\[[:space:]]*$/) collect=0;
        next
      }
      # Also capture explicit rules that build tests
      /^[a-zA-Z0-9_.-]+:[[:space:]].*(\$\(TEST_FRAMEWORK\)|\$\(FRONTIER_SOURCES\)|\$\(LANG_RUNTIME_SOURCES\))/ {
        split($1, a, ":"); print a[1];
      }
    ' "$mf" | sort -u
  fi
}

TARGETS=()
_tmp_targets_file="$(mktemp -t frontier_targets.XXXXXX)"
discover_targets > "$_tmp_targets_file" || true
while IFS= read -r _t; do
  [[ -n "$_t" ]] && TARGETS+=("$_t")
done < "$_tmp_targets_file"
rm -f "$_tmp_targets_file"

# Optional filter: only run targets whose names match RUN_PATTERN (extended regex)
if [[ -n "${RUN_PATTERN:-}" ]]; then
  _filtered=()
  for _t in "${TARGETS[@]}"; do
    if printf "%s\n" "$_t" | grep -E -q -- "$RUN_PATTERN"; then
      _filtered+=("$_t")
    fi
  done
  TARGETS=("${_filtered[@]}")
fi

printf "Running tests from %s\n" "$REPO_DIR"
if [[ -n "${RUN_PATTERN:-}" ]]; then
  printf "RUN_PATTERN=%s\n" "$RUN_PATTERN"
fi
printf "Discovered %d target(s)\n" "${#TARGETS[@]}"
printf "Targets to run:\n"
for t in "${TARGETS[@]}"; do
  printf "  - %s\n" "$t"
done
printf "\n"

if [[ ${#TARGETS[@]} -eq 0 ]]; then
  echo "No test targets to run." >&2
  exit 1
fi

FAILED=0

for TARGET in "${TARGETS[@]}"; do
  echo "=== $TARGET ==="
  echo "> Build"
  ( cd "$REPO_DIR/tests" && make "$TARGET" ) 2>&1
  BUILD_EXIT=$?
  echo "BUILD_EXIT:$BUILD_EXIT"
  if [[ $BUILD_EXIT -ne 0 ]]; then
    echo "RESULT:$TARGET:BUILD_FAILED"
    FAILED=$((FAILED+1))
    echo ""
    continue
  fi
  echo "> Run"
  ( cd "$REPO_DIR" && "./tests/$TARGET" ) 2>&1
  RUN_EXIT=$?
  echo "RUN_EXIT:$RUN_EXIT"
  if [[ $RUN_EXIT -ne 0 ]]; then
    echo "RESULT:$TARGET:RUN_FAILED"
    FAILED=$((FAILED+1))
  else
    echo "RESULT:$TARGET:OK"
  fi
  echo ""
done

echo "Summary: FAILURES=$FAILED"
exit $FAILED
