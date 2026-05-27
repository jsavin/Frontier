#!/bin/bash
#
# edit_virgin_root.sh — stage-and-confirm wrapper for editing Virgin.root
# via frontier-cli's protocol mode.
#
# Problem (issue #644): the documented ODB-edit workflow points
# `frontier-cli --protocol` directly at `databases/Virgin.root`. A forgotten
# session, a runaway script, or a mid-write kill can leave the canonical
# .root file corrupted locally with no automatic recovery. PR #643's agent
# hit this in practice — Virgin.root grew from 20MB to 31MB due to a leaked
# session.
#
# This wrapper interposes a staging step:
#
#   1. Compute md5 of databases/Virgin.root.
#   2. Copy it to a unique stage dir under $TMPDIR (default /tmp on macOS).
#   3. Spawn frontier-cli --protocol against the staged copy (stdin/stdout
#      passed through so the operator can interact normally).
#   4. After the session exits, compute the staged copy's new md5 + size.
#   5. Print a diff banner (before/after md5, size delta).
#   6. If unchanged, discard the staged copy and exit.
#   7. Otherwise prompt: "Promote changes to databases/Virgin.root? [y/N]"
#        - yes: atomically replace canonical, then rm -rf the temp.
#        - no:  rename staged copy to .bak-<timestamp> for inspection
#               and tell the operator where it is.
#
# Flags:
#   --help        Print usage and exit 0.
#   --dry-run     Stage the copy but do NOT spawn frontier-cli. Useful for
#                 testing the wrapper itself.
#   --read-only   Pass --lock-opened-roots to the spawned frontier-cli.
#                 Lets operators do read-only inspection without the
#                 wrapper's promotion ceremony. (Equivalent to running the
#                 CLI directly in inspection mode, but staged for
#                 consistency.) Kept as --read-only on the wrapper for
#                 operator-facing clarity; --lock-opened-roots is the
#                 underlying CLI flag.
#
# On Ctrl-C during the editor session the staged temp dir is left in place
# for inspection.

set -euo pipefail

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

# The wrapper resolves paths relative to the *current working directory*,
# not relative to the script's own location. This is intentional: it forces
# operators to invoke the wrapper from the Frontier project root, which
# guards against accidentally running a stale copy of the wrapper against
# the wrong checkout. If `databases/Virgin.root` doesn't exist relative to
# cwd we bail with a clear error.
PROJECT_ROOT="$(pwd)"
CLI="$PROJECT_ROOT/frontier-cli/frontier-cli"
CANONICAL_ROOT="$PROJECT_ROOT/databases/Virgin.root"

DRY_RUN=0
READ_ONLY=0

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

usage() {
    cat <<EOF
Usage: tools/edit_virgin_root.sh [options]

Stage databases/Virgin.root to a unique stage dir under \$TMPDIR (default
/tmp on macOS), spawn frontier-cli's protocol-mode editor against the
staged copy, then prompt before promoting any changes back to the
canonical file.

Options:
  --help        Show this help and exit.
  --dry-run     Stage the copy but do not spawn frontier-cli. Cleans up the
                staged temp dir on exit. Use this to smoke-test the wrapper.
  --read-only   Spawn frontier-cli with --lock-opened-roots. The session
                evaluates mutations in memory but the on-exit save is
                suppressed, and this flag also disables the promotion
                prompt at the end since the staged copy cannot have been
                changed on disk.

Examples:
  tools/edit_virgin_root.sh
      Interactive: stage, edit, confirm-and-promote.

  tools/edit_virgin_root.sh --read-only
      Stage and inspect without any write path.

  tools/edit_virgin_root.sh --dry-run
      Stage only, do not spawn frontier-cli (useful for tests).
EOF
}

err() {
    echo "edit_virgin_root.sh: error: $*" >&2
}

md5_of() {
    # Cross-platform md5; prefer md5sum (Linux + portable) then fall back to
    # macOS's md5 -q. Frontier's dev platform is macOS but agents on CI may
    # differ.
    if command -v md5sum >/dev/null 2>&1; then
        md5sum "$1" | cut -d' ' -f1
    elif command -v md5 >/dev/null 2>&1; then
        md5 -q "$1"
    else
        err "neither md5sum nor md5 available"
        exit 2
    fi
}

size_of() {
    # Cross-platform byte size. stat differs between BSD (macOS) and GNU.
    if stat -f%z "$1" >/dev/null 2>&1; then
        stat -f%z "$1"
    else
        stat -c%s "$1"
    fi
}

# ---------------------------------------------------------------------------
# Argv parsing
# ---------------------------------------------------------------------------

while [ $# -gt 0 ]; do
    case "$1" in
        --help|-h)
            usage
            exit 0
            ;;
        --dry-run)
            DRY_RUN=1
            shift
            ;;
        --read-only)
            READ_ONLY=1
            shift
            ;;
        *)
            err "unknown argument: $1"
            echo >&2
            usage >&2
            exit 2
            ;;
    esac
done

# ---------------------------------------------------------------------------
# Preconditions
# ---------------------------------------------------------------------------

if [ ! -f "$CANONICAL_ROOT" ]; then
    err "databases/Virgin.root not found at $CANONICAL_ROOT"
    err "run this wrapper from the Frontier project root (or its tools/ dir)"
    exit 2
fi

if [ ! -x "$CLI" ]; then
    err "frontier-cli not found or not executable at $CLI"
    err "build it first: make -C frontier-cli"
    exit 2
fi

# ---------------------------------------------------------------------------
# Stage
# ---------------------------------------------------------------------------

BEFORE_MD5="$(md5_of "$CANONICAL_ROOT")"
BEFORE_SIZE="$(size_of "$CANONICAL_ROOT")"

# Unpredictable suffix via mktemp -d closes symlink-hijack on /tmp.
# Prefix kept human-readable for debugging.
STAGE_DIR="$(mktemp -d -t "frontier-edit-${BEFORE_MD5:0:12}-XXXXXXXX")" || {
    err "mktemp -d failed"
    exit 2
}
STAGED_ROOT="$STAGE_DIR/Virgin.root"

# EXIT trap for cleanup. Sets STAGE_PRESERVE=1 to skip removal on the
# "rejected promotion" and "--read-only changed" (lock-bypass) forensic branches.
STAGE_PRESERVE=0
cleanup_stage() {
    if [ "$STAGE_PRESERVE" -eq 0 ] && [ -n "${STAGE_DIR:-}" ] && [ -d "$STAGE_DIR" ]; then
        rm -rf "$STAGE_DIR"
    fi
    # Sweep any orphan .promoting.$$ file in the canonical's directory in
    # case a signal hit between cp and mv during promotion.
    canonical_dir="$(dirname "$CANONICAL_ROOT")"
    [ -d "$canonical_dir" ] && find "$canonical_dir" -maxdepth 1 -name "*.promoting.$$" -delete 2>/dev/null || true
}
trap cleanup_stage EXIT

cp "$CANONICAL_ROOT" "$STAGED_ROOT"
# Belt-and-suspenders: verify what we wrote is a regular file.
if [ ! -f "$STAGED_ROOT" ] || [ -L "$STAGED_ROOT" ]; then
    err "staged path is not a regular file: $STAGED_ROOT"
    exit 3
fi

# Verify the copy is byte-identical to the source — defense against a
# truncated cp on a full /tmp.
STAGED_MD5="$(md5_of "$STAGED_ROOT")"
if [ "$STAGED_MD5" != "$BEFORE_MD5" ]; then
    err "staged copy md5 ($STAGED_MD5) does not match source ($BEFORE_MD5)"
    err "leaving stage dir for inspection: $STAGE_DIR"
    STAGE_PRESERVE=1
    exit 3
fi

echo "Staged Virgin.root for editing:"
echo "  source: $CANONICAL_ROOT"
echo "  staged: $STAGED_ROOT"
echo "  size:   $BEFORE_SIZE bytes"
echo "  md5:    $BEFORE_MD5"
echo

# ---------------------------------------------------------------------------
# Dry run — bail before spawning frontier-cli
# ---------------------------------------------------------------------------

if [ "$DRY_RUN" -eq 1 ]; then
    echo "(dry-run) skipping frontier-cli; cleaning up staged copy."
    exit 0
fi

# ---------------------------------------------------------------------------
# Spawn frontier-cli
# ---------------------------------------------------------------------------

CLI_ARGS=(--protocol --skip-startup --system-root "$STAGED_ROOT")
if [ "$READ_ONLY" -eq 1 ]; then
    CLI_ARGS=(--lock-opened-roots "${CLI_ARGS[@]}")
fi

if [ "$READ_ONLY" -eq 1 ]; then
    echo "Spawning frontier-cli in --read-only mode (--lock-opened-roots)."
else
    echo "Spawning frontier-cli (default-RW) against the staged copy."
fi
echo "Command: $CLI ${CLI_ARGS[*]}"
echo

# Don't let `set -e` kill us if the CLI exits non-zero — we want to surface
# the diff banner regardless of the CLI's exit status.
CLI_RC=0
"$CLI" "${CLI_ARGS[@]}" || CLI_RC=$?

echo
echo "frontier-cli exited with status $CLI_RC."

# ---------------------------------------------------------------------------
# Diff banner
# ---------------------------------------------------------------------------

AFTER_MD5="$(md5_of "$STAGED_ROOT")"
AFTER_SIZE="$(size_of "$STAGED_ROOT")"
SIZE_DELTA=$((AFTER_SIZE - BEFORE_SIZE))

echo
echo "============================================="
echo "Stage diff:"
echo "  before md5:  $BEFORE_MD5"
echo "  after md5:   $AFTER_MD5"
echo "  before size: $BEFORE_SIZE bytes"
echo "  after size:  $AFTER_SIZE bytes"
echo "  delta:       $SIZE_DELTA bytes"
echo "============================================="
echo

if [ "$BEFORE_MD5" = "$AFTER_MD5" ]; then
    echo "No changes — discarding staged copy."
    exit 0
fi

if [ "$READ_ONLY" -eq 1 ]; then
    # This shouldn't happen — --lock-opened-roots means the CLI shouldn't
    # write to disk — but if it does, treat it as a hard error and preserve
    # the staged copy for forensics.
    err "staged copy changed despite --read-only (--lock-opened-roots) — refusing to promote"
    err "staged copy left at: $STAGED_ROOT"
    STAGE_PRESERVE=1
    exit 4
fi

# ---------------------------------------------------------------------------
# Promotion prompt
# ---------------------------------------------------------------------------

# Default N: a stray Enter must not promote.
read -r -p "Promote changes to databases/Virgin.root? [y/N] " response
case "$response" in
    y|Y|yes|YES|Yes)
        # Race check: someone else may have written to the canonical while
        # we held our staged copy. Refuse to clobber their work.
        CURRENT_MD5="$(md5_of "$CANONICAL_ROOT")"
        if [ "$CURRENT_MD5" != "$BEFORE_MD5" ]; then
            err "$CANONICAL_ROOT changed externally during this session"
            err "  expected md5: $BEFORE_MD5"
            err "  current md5:  $CURRENT_MD5"
            err "  refusing to promote — another process may be writing"
            err "  staged copy preserved at: $STAGED_ROOT"
            STAGE_PRESERVE=1
            exit 5
        fi
        # Atomic on same filesystem: write to .new, then rename. A Ctrl-C
        # mid-cp leaves the old Virgin.root intact; only the rename swaps it.
        canonical_tmp="$CANONICAL_ROOT.promoting.$$"
        cp "$STAGED_ROOT" "$canonical_tmp"
        sync
        mv "$canonical_tmp" "$CANONICAL_ROOT"
        echo "Promoted staged copy -> $CANONICAL_ROOT"
        ;;
    *)
        # Preserve the staged copy under a .bak path so the operator can
        # inspect or recover from it later.
        ts="$(date +%Y%m%d-%H%M%S)"
        bak_path="$STAGE_DIR/Virgin.root.bak-$ts"
        mv "$STAGED_ROOT" "$bak_path"
        echo "Changes NOT promoted."
        echo "Staged copy preserved at: $bak_path"
        STAGE_PRESERVE=1
        ;;
esac
