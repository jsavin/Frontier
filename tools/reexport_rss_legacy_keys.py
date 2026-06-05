#!/usr/bin/env python3
"""reexport_rss_legacy_keys.py — one-shot tool to rewrite the 7 legacy
RSS module-driver `.ut` files (the `[colon][slash]` bracket-token paths
under `system.verbs.builtins.xml.rss.moduleDrivers`) into the post-#698
percent-encoded scheme.

This closes the second half of issue #700. The bracket-token directories
predate the lossless percent-encoding fix that shipped in #699; they're
not produced by any current code path, and `verify_virgin_root_sync.sh
--full` can't resolve them back to their real ODB keys (they fail with
"path not found" because the FS-to-ODB translator splits them on `.`
and `/`).

What this tool does
-------------------
1. Opens a protocol session against `databases/Virgin.root` in
   `--lock-opened-roots` mode (read-only).
2. For each affected ODB path (the 7 leaves under the 3 URL keys),
   fetches the script body via `string()` + normalizes it the same way
   the verifier does (MacRoman -> UTF-8, CR -> LF, comment-marker
   substitution, structure-marker strip).
3. Fetches `timeModified()` for each script, converts the Mac-epoch
   value to Unix epoch.
4. Writes the body to its new percent-encoded path; stamps the file's
   mtime to match the ODB timestamp so the import-side LWW compare
   starts converged (per docs/usertalk/UT_SYNC_WORKFLOW.md).
5. Removes the legacy `[colon][slash]` directory subtrees.
6. Prints a summary; verifies with the existing verifier.

The tool is idempotent: if the new paths already exist and the legacy
ones don't, running it again is a no-op (the source ODB keys are still
fetched fresh).

Run from the repo root: `./tools/reexport_rss_legacy_keys.py`.
"""

from __future__ import annotations

import os
import shutil
import sys
from pathlib import Path

# Reuse the verifier's session + normalization.
REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO_ROOT / "tools"))
import verify_virgin_root_sync as v  # noqa: E402

CORPUS_ROOT = REPO_ROOT / "usertalk_scripts" / "Frontier.root"
SYSTEM_ROOT = REPO_ROOT / "databases" / "Virgin.root"
CLI_PATH = REPO_ROOT / "frontier-cli" / "frontier-cli"

# Mac epoch (Jan 1 1904 UTC) is this many seconds before the Unix epoch.
# Mirrors UT_MAC_TO_UNIX_EPOCH_OFFSET in frontier-cli/ut_sync.c.
MAC_TO_UNIX_EPOCH_OFFSET = 2082844800


# The legacy [colon][slash] directories to delete (top-level URL-key
# placeholders). All seven affected .ut files live under these three roots.
LEGACY_DIRS = [
    CORPUS_ROOT / "system/verbs/builtins/xml/rss/moduleDrivers" / d
    for d in (
        "http[colon][slash][slash]backend",
        "http[colon][slash][slash]purl",
        "http[colon][slash][slash]webns",
    )
]


# The 7 leaves, expressed as (real ODB sub-path under
# `system.verbs.builtins.xml.rss.moduleDrivers["<URL>"].<rest>`).
# The path is split into (url_key, *trailing_segments). The trailing
# segments are valid UserTalk identifiers (subElementOfChannel, init,
# any, blink, blogRoll, mySubscriptions, subElementOfItem) so they map
# 1:1 to ODB keys and to flat directory components on disk.
LEAVES = [
    # (url_key, [trailing segments to the leaf])
    ("http://backend.userland.com/blogChannelModule", ["init"]),
    ("http://backend.userland.com/blogChannelModule", ["subElementOfChannel", "blink"]),
    ("http://backend.userland.com/blogChannelModule", ["subElementOfChannel", "blogRoll"]),
    ("http://backend.userland.com/blogChannelModule", ["subElementOfChannel", "mySubscriptions"]),
    ("http://purl.org/rss/1.0/modules/content/", ["subElementOfItem", "any"]),
    ("http://webns.net/mvcb/", ["init"]),
    ("http://webns.net/mvcb/", ["subElementOfChannel", "any"]),
]


def pct_encode_segment(raw: str) -> str:
    """Mirror of `ut_pct_encode_segment` (frontier-cli/ut_sync.c:706)."""
    out = []
    for i, c in enumerate(raw):
        b = ord(c)
        escape = (
            b == 0x25  # %
            or b == 0x2E  # .
            or b == 0x2F  # /
            or b == 0x3A  # :
            or b == 0x22  # "
            or b == 0x5C  # \
            or b < 0x20
            or b == 0x7F
            or (i == 0 and b == 0x2D)  # leading -
        )
        if escape:
            out.append("%%%02X" % b)
        else:
            out.append(chr(b))
    return "".join(out)


def odb_path_for(url_key: str, trail: list[str]) -> str:
    """Construct the bracket-quoted ODB path for protocol calls."""
    # url_key needs bracket-quoting; trailing segments are identifiers.
    base = 'system.verbs.builtins.xml.rss.moduleDrivers.["' + url_key + '"]'
    return base + "".join("." + seg for seg in trail)


def fs_path_for(url_key: str, trail: list[str]) -> Path:
    """Construct the on-disk %XX-encoded .ut path."""
    encoded_key = pct_encode_segment(url_key)
    return CORPUS_ROOT.joinpath(
        "system/verbs/builtins/xml/rss/moduleDrivers",
        encoded_key,
        *trail[:-1],
        trail[-1] + ".ut",
    )


def fetch_time_modified(sess: v.ProtocolSession, odb_path: str) -> int:
    """Return the Unix epoch seconds for the script's timeModified."""
    assert sess.proc is not None
    req_id = sess.next_id
    sess.next_id += 1
    # timeModified takes an address (its handler signature names the
    # param adrFoo), not a value. See memory
    # reference_usertalk_verb_param_convention_adr_prefix.
    expr = f"long(timeModified(@{odb_path}))"
    import json as _json
    expr_json = _json.dumps(expr)
    req = f'{{"op":"script/eval","id":{req_id},"params":{{"expression":{expr_json}}}}}\n'.encode("ascii")
    sess.proc.stdin.write(req)
    sess.proc.stdin.flush()
    line = sess._readline_with_timeout(v.DEFAULT_READ_TIMEOUT_S).rstrip(b"\r\n")
    m = sess.SUCCESS_RE.search(line)
    if not (m and m.group(1) == b"true"):
        raise RuntimeError(f"timeModified probe failed for {odb_path}: {line!r}")
    # Pull the integer out of `"value":"3116941870"`.
    import re as _re
    val_m = _re.search(rb'"value":"(\d+)"', line)
    if not val_m:
        raise RuntimeError(f"timeModified probe returned no value for {odb_path}: {line!r}")
    mac_seconds = int(val_m.group(1))
    return mac_seconds - MAC_TO_UNIX_EPOCH_OFFSET


def main() -> int:
    if not CLI_PATH.exists():
        print(f"ERROR: frontier-cli not found at {CLI_PATH}", file=sys.stderr)
        print("Build it first: make -C frontier-cli", file=sys.stderr)
        return 2
    if not SYSTEM_ROOT.exists():
        print(f"ERROR: system root not found at {SYSTEM_ROOT}", file=sys.stderr)
        return 2

    written = 0
    skipped = 0
    with v.ProtocolSession(CLI_PATH, SYSTEM_ROOT) as sess:
        for url_key, trail in LEAVES:
            odb_path = odb_path_for(url_key, trail)
            fs_path = fs_path_for(url_key, trail)

            # Confirm the node exists and is a script.
            defined, ostype, err = sess.probe_node(odb_path)
            if not defined or ostype != "scpt":
                print(
                    f"  SKIP {url_key}/{'/'.join(trail)}: "
                    f"defined={defined} typeof={ostype} err={err}",
                    file=sys.stderr,
                )
                skipped += 1
                continue

            ok, raw_value, err = sess.get_script_body(odb_path)
            if not ok or not raw_value:
                print(
                    f"  SKIP {url_key}/{'/'.join(trail)}: "
                    f"body fetch failed ({err!r})",
                    file=sys.stderr,
                )
                skipped += 1
                continue

            body = v.normalize_kernel_body(raw_value)
            unix_mtime = fetch_time_modified(sess, odb_path)

            fs_path.parent.mkdir(parents=True, exist_ok=True)
            fs_path.write_bytes(body)
            os.utime(fs_path, (unix_mtime, unix_mtime))
            print(
                f"  WROTE {fs_path.relative_to(REPO_ROOT)} "
                f"({len(body)} bytes, mtime={unix_mtime})"
            )
            written += 1

    # Delete the legacy directory trees.
    removed = 0
    for d in LEGACY_DIRS:
        if d.exists():
            shutil.rmtree(d)
            print(f"  REMOVED {d.relative_to(REPO_ROOT)}")
            removed += 1
        else:
            print(f"  (already gone) {d.relative_to(REPO_ROOT)}")

    print(
        f"\nSummary: wrote {written}, skipped {skipped}, "
        f"removed {removed} legacy dir(s).",
        file=sys.stderr,
    )
    return 0 if (written == len(LEAVES) and skipped == 0) else 1


if __name__ == "__main__":
    sys.exit(main())
