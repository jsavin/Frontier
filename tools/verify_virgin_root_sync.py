#!/usr/bin/env python3
"""verify_virgin_root_sync.py — detect drift between databases/Virgin.root
and usertalk_scripts/Frontier.root/**/*.ut.

Both artifacts are tracked in git. The .ut corpus exists so git/PR reviewers
have something diffable for changes that actually land in the binary
Virgin.root. Without a verifier, the two drift undetected (issue #675).

Mode of operation
-----------------
- default (no args): incremental. Reads staged .ut paths via
  `git diff --cached`. Exits 0 if no .ut files are staged.
- --full: walk the entire usertalk_scripts/Frontier.root/ tree.
- --paths FILE [FILE ...]: verify the named .ut files explicitly.
- --corpus-root PATH: override the default corpus root prefix for path
  translation. Used by the negative test to sandbox a synthetic .ut.
- --system-root PATH: the .root database to read (default databases/Virgin.root).
- --cli PATH: the frontier-cli binary (default frontier-cli/frontier-cli).

Path translation
----------------
A filesystem path `<corpus_root>/a/b/c.ut` maps to ODB path `a.b.c`.
Lexical; no escaping. Sampling confirmed this matches the existing corpus
layout uniformly.

Normalization
-------------
The kernel's string() coercion of a script returns:
  - MacRoman bytes (high-bit chars NOT UTF-8-encoded — the protocol JSON
    output is in fact NOT valid UTF-8 if the script contains high-bit chars)
  - CR (0x0d) line endings
  - 0xC7 (MacRoman `«`) at the start of every single-line comment
  - 0xC8 (MacRoman `»`) at the close of any block comment

The .ut corpus has been converted to:
  - UTF-8 throughout
  - LF (0x0a) line endings
  - `//` for single-line comments
  - block-comment `«»` removed during conversion (no surviving `«»` in any .ut)

Verifier applies these transforms to kernel output before byte-comparing
against .ut content:
  1. Decode raw kernel bytes as MacRoman → UTF-8.
  2. Substitute UTF-8 `«` («) → `//`.
  3. Substitute UTF-8 `»` (») → empty.
  4. CR → LF.
  5. Strip a single trailing LF if present (.ut files have no trailing newline).

Exit codes
----------
  0  in sync
  1  drift detected
  2  infrastructure error (cli missing, root missing, protocol parse error)
"""

from __future__ import annotations

import argparse
import difflib
import json
import os
import re
import selectors
import subprocess
import sys
from pathlib import Path
from typing import Iterable


# Per-request read timeout (seconds). The kernel normally responds in
# milliseconds; this bounds a hung subprocess so the pre-commit hook can't
# wedge the user's terminal. Overridable via FRONTIER_VERIFIER_TIMEOUT env.
DEFAULT_READ_TIMEOUT_S = float(os.environ.get("FRONTIER_VERIFIER_TIMEOUT", "30"))


# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_CORPUS_ROOT = REPO_ROOT / "usertalk_scripts" / "Frontier.root"
DEFAULT_SYSTEM_ROOT = REPO_ROOT / "databases" / "Virgin.root"
DEFAULT_CLI = REPO_ROOT / "frontier-cli" / "frontier-cli"


# ---------------------------------------------------------------------------
# Protocol session
# ---------------------------------------------------------------------------

class ProtocolSession:
    """Long-lived frontier-cli --protocol session.

    One process for the lifetime of the verifier run; many script/eval
    requests over stdin/stdout. The kernel writes its responses as
    JSON-ish lines with raw MacRoman bytes embedded (NOT UTF-8) when a
    script body contains high-bit chars, so we MUST NOT json.loads() the
    raw line. Instead we extract the "value" field with a regex against
    raw bytes.
    """

    # Match: ..."value":"<captured>","type":"string"...
    # Non-greedy capture with DOTALL; we trust the kernel's escaping of
    # embedded backslashes and quotes.
    VALUE_RE = re.compile(rb'"value":"(.*?)","type":"string"', re.DOTALL)
    SUCCESS_RE = re.compile(rb'"success":(true|false)')
    ID_RE = re.compile(rb'"id":(\d+)')

    def __init__(self, cli_path: Path, system_root: Path):
        self.cli_path = cli_path
        self.system_root = system_root
        self.proc: subprocess.Popen | None = None
        self.next_id = 1
        # Persistent read buffer so a read1() chunk that crosses a
        # newline boundary doesn't discard the next response's prefix.
        self._read_buf = bytearray()

    def __enter__(self) -> "ProtocolSession":
        self.proc = subprocess.Popen(
            [
                str(self.cli_path),
                "--lock-opened-roots",
                "--protocol",
                "--skip-startup",
                "--system-root",
                str(self.system_root),
            ],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            bufsize=0,
        )
        return self

    def _readline_with_timeout(self, timeout_s: float) -> bytes:
        """Read one line from the subprocess stdout, bounded by timeout.

        Uses a persistent self._read_buf so that a read1() chunk which
        contains MORE than one line keeps the surplus for the next call.
        Returns empty bytes if the subprocess closed stdout. Raises
        RuntimeError if the timeout elapses before a complete line
        arrives — verify_files treats this as infra error (exit 2).
        """
        import time
        assert self.proc is not None and self.proc.stdout is not None

        # First, check if a complete line is already buffered from a
        # prior over-read.
        nl = self._read_buf.find(b"\n")
        if nl >= 0:
            line = bytes(self._read_buf[: nl + 1])
            del self._read_buf[: nl + 1]
            return line

        sel = selectors.DefaultSelector()
        sel.register(self.proc.stdout, selectors.EVENT_READ)
        deadline_ns = int(timeout_s * 1_000_000_000)
        start = time.monotonic_ns()
        try:
            while True:
                remaining_ns = deadline_ns - (time.monotonic_ns() - start)
                if remaining_ns <= 0:
                    try:
                        self.proc.kill()
                    except OSError:
                        pass
                    raise RuntimeError(
                        f"protocol response timeout after {timeout_s}s "
                        f"(set FRONTIER_VERIFIER_TIMEOUT to override)"
                    )
                events = sel.select(timeout=remaining_ns / 1_000_000_000)
                if not events:
                    continue
                # Use os.read on the fd directly: bufsize=0 Popen gives
                # FileIO, which lacks read1; os.read returns whatever's
                # available without blocking once selectors says ready.
                try:
                    chunk = os.read(self.proc.stdout.fileno(), 65536)
                except OSError:
                    chunk = b""
                if not chunk:
                    # EOF. Return whatever's in the buffer (caller treats
                    # empty as "session ended").
                    out = bytes(self._read_buf)
                    self._read_buf.clear()
                    return out
                self._read_buf.extend(chunk)
                nl = self._read_buf.find(b"\n")
                if nl >= 0:
                    line = bytes(self._read_buf[: nl + 1])
                    del self._read_buf[: nl + 1]
                    return line
        finally:
            sel.close()

    def __exit__(self, exc_type, exc, tb) -> None:
        if self.proc is None:
            return
        try:
            shutdown_id = self.next_id
            self.next_id += 1
            req = f'{{"op":"shutdown","id":{shutdown_id}}}\n'.encode("ascii")
            try:
                self.proc.stdin.write(req)
                self.proc.stdin.flush()
            except (BrokenPipeError, OSError):
                pass
            try:
                self.proc.stdin.close()
            except OSError:
                pass
            try:
                self.proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait()
        finally:
            self.proc = None

    def get_script_body(self, odb_path: str) -> tuple[bool, bytes, str]:
        """Send `string(<odb_path>)`. Returns (success, raw_value_bytes, error_message).

        On success: raw_value_bytes is the kernel's JSON-escaped string value
        as RAW bytes (NOT decoded), containing MacRoman + escape sequences.
        On failure: error_message holds the error text from the protocol response.
        """
        assert self.proc is not None
        req_id = self.next_id
        self.next_id += 1
        expr = f"string({odb_path})"
        # JSON-escape the expression. odb_path is lexically restricted to
        # [a-zA-Z0-9._] so no escaping is actually needed, but be safe.
        expr_json = json.dumps(expr)
        req = f'{{"op":"script/eval","id":{req_id},"params":{{"expression":{expr_json}}}}}\n'.encode("ascii")
        self.proc.stdin.write(req)
        self.proc.stdin.flush()

        # Read one response line, bounded by DEFAULT_READ_TIMEOUT_S so a
        # kernel stall can't wedge the pre-commit hook. The kernel
        # normally responds in milliseconds; the timeout exists for
        # adversarial / pathological inputs (issue #675 P1).
        line = self._readline_with_timeout(DEFAULT_READ_TIMEOUT_S)
        if not line:
            stderr = self.proc.stderr.read().decode("utf-8", errors="replace") if self.proc.stderr else ""
            raise RuntimeError(f"protocol session ended unexpectedly. stderr: {stderr}")

        # Strip trailing newline.
        line = line.rstrip(b"\r\n")

        # Match success flag.
        m_succ = self.SUCCESS_RE.search(line)
        success = m_succ is not None and m_succ.group(1) == b"true"

        if not success:
            # Best-effort error extraction. The error message field is ASCII,
            # so a UTF-8 decode is safe.
            try:
                parsed = json.loads(line.decode("utf-8", errors="replace"))
                err = parsed.get("error", {})
                msg = err.get("message", "unknown error")
            except json.JSONDecodeError:
                msg = "unparseable error response"
            return (False, b"", msg)

        m_val = self.VALUE_RE.search(line)
        if not m_val:
            # success=true but no string value → script address doesn't
            # name a script (probably a non-script ODB node, or address
            # itself stringified). Treat as drift signal: caller decides.
            return (True, b"", "no string value in response")

        return (True, m_val.group(1), "")


# ---------------------------------------------------------------------------
# Normalization
# ---------------------------------------------------------------------------

def normalize_kernel_body(raw: bytes) -> bytes:
    """Apply the kernel-canonical → .ut-canonical transform.

    Input: raw bytes from the JSON "value" field (still JSON-escaped,
    still in MacRoman encoding).

    Output: bytes that should byte-compare equal to a correctly-exported
    .ut file.
    """
    # 1. Decode JSON escapes that the kernel emits: \r, \t, \n, \", \\.
    #    Do this BEFORE charset conversion so the resulting bytes are pure
    #    MacRoman + ASCII control chars.
    out = bytearray()
    i = 0
    n = len(raw)
    while i < n:
        b = raw[i]
        if b == 0x5C and i + 1 < n:  # backslash
            nxt = raw[i + 1]
            if nxt == ord("r"):
                out.append(0x0D)
                i += 2
                continue
            if nxt == ord("n"):
                out.append(0x0A)
                i += 2
                continue
            if nxt == ord("t"):
                out.append(0x09)
                i += 2
                continue
            if nxt == ord('"'):
                out.append(ord('"'))
                i += 2
                continue
            if nxt == ord("\\"):
                out.append(ord("\\"))
                i += 2
                continue
            if nxt == ord("/"):
                out.append(ord("/"))
                i += 2
                continue
            if nxt == ord("u") and i + 5 < n:
                # \uXXXX 4-hex sequence
                hex4 = raw[i + 2 : i + 6]
                try:
                    cp = int(hex4.decode("ascii"), 16)
                    out.extend(chr(cp).encode("utf-8"))
                    i += 6
                    continue
                except (ValueError, UnicodeError):
                    pass
            # Unknown escape — keep literal backslash, advance one byte.
            out.append(b)
            i += 1
            continue
        out.append(b)
        i += 1

    # 2. MacRoman → UTF-8. The bytearray now has CR/LF/TAB as raw bytes
    #    and high-bit chars as MacRoman. Decode and re-encode as UTF-8.
    try:
        decoded = bytes(out).decode("mac_roman")
    except UnicodeDecodeError:
        # mac_roman covers all 256 bytes; this shouldn't trigger. If it does,
        # fall back to latin-1 (also 1:1 byte-to-codepoint, never errors).
        decoded = bytes(out).decode("latin-1")

    # 3. Substitute the kernel's single-line-comment marker. After MacRoman
    #    decoding, 0xC7/0xC8 became `«` / `»` (the UTF-8 sequences are
    #    « / »).
    decoded = decoded.replace("«", "//")
    decoded = decoded.replace("»", "")

    # 4. CR → LF.
    decoded = decoded.replace("\r", "\n")

    # 5. Strip outline structure markers around pure-comment subtrees.
    #
    # The kernel emits, for a subtree of nothing but comments:
    #
    #   \tABOVE
    #   \t {//comment
    #   \t\t {//nested
    #   \t\t\t}};//deepest comment
    #
    # The .ut form has the same comments with no braces:
    #
    #   \tABOVE
    #   \t//comment
    #   \t\t//nested
    #   \t\t\t//deepest comment
    #
    # `script.newScriptObject` strips these markers via langstripstructuremarkers
    # before install (PR #621 territory), so a bare-comment .ut round-trips
    # to itself after install + re-export. For the verifier to match the on-
    # disk form against kernel-emitted form, apply the same stripping here.
    #
    # Strip a leading brace sequence immediately before a comment marker.
    # The kernel emits, for a subtree of all-comments, lines like:
    #
    #   \t {//comment        opening brace, level 1
    #   \t\t {//inner        opening brace, level 2
    #   \t\t\t}};//deep      pair of closing braces + semicolon, level 3
    #
    # The .ut form has bare `\t//comment` / `\t\t//inner` / `\t\t\t//deep`.
    # script.newScriptObject calls langstripstructuremarkers on install,
    # which strips exactly these tokens — so a bare-comment .ut round-trips
    # cleanly to itself. The verifier mirrors that stripping here.
    #
    # Pattern: anchor at start-of-line; preserve the indentation prefix;
    # then strip an optional space, any run of `{` or `}` (one or more),
    # an optional `;`, and an optional trailing space — but only when
    # followed by `//`.
    decoded = re.sub(r"(^|\n)([ \t]*?)( ?[{}]+;? ?)(?=//)", r"\1\2", decoded)

    # 6. Strip a single trailing LF if present (.ut files have no trailing
    #    newline). Kernel output also has none normally, but if string()
    #    ever appends one we want to absorb it.
    if decoded.endswith("\n"):
        decoded = decoded[:-1]

    return decoded.encode("utf-8")


# ---------------------------------------------------------------------------
# Path translation
# ---------------------------------------------------------------------------

# UserTalk identifiers: leading letter or underscore, then letters/digits/underscores.
# Anything else in a path segment (e.g., `#filters`, `13`, ` foo bar`) must be
# bracket-quoted: `parent.["unusual segment"].child`.
IDENT_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")


def _path_within(candidate: Path, root: Path) -> bool:
    """Return True if `candidate` is `root` itself or under `root`.

    Path.is_relative_to() exists from Python 3.9 but the implementation
    differs slightly across versions; use commonpath for stability.
    """
    try:
        return os.path.commonpath([str(candidate), str(root)]) == str(root)
    except ValueError:
        # Different drives on Windows (not applicable here, but be safe).
        return False


def _quote_segment(seg: str) -> str:
    """Return seg as a valid ODB-path segment, bracket-quoting if needed.

    Rejects segments containing control bytes (NUL through US, plus DEL)
    or newlines — these can't safely round-trip and are not expected in
    any legitimate corpus path. Defensive against pathological filesystem
    state (POSIX permits these bytes in filenames, even though no real
    corpus path uses them).

    Note: `[` and `]` ARE permitted in segments — the corpus uses literal
    brackets as part of escape encodings in some paths (e.g. the
    `[colon]` / `[slash]` segments under xml.rss.moduleDrivers).
    Bracket-quoting still works in UserTalk for these.
    """
    if IDENT_RE.match(seg):
        return seg
    if any(ord(c) < 0x20 or c == "\x7f" for c in seg):
        raise ValueError(f"unrepresentable ODB path segment: {seg!r}")
    # Bracket-quoted form: ["seg"]. Escape embedded quotes / backslashes.
    escaped = seg.replace("\\", "\\\\").replace('"', '\\"')
    return f'["{escaped}"]'


def fs_to_odb_path(fs_path: Path, corpus_root: Path) -> str:
    """Translate filesystem .ut path to dotted ODB path.

    Segments that aren't valid UserTalk identifiers (e.g., `#filters`) are
    bracket-quoted: parent.["#filters"].child. The first segment is special:
    it's the root table name, joined with `.` to the rest.
    """
    fs_path = fs_path.resolve()
    corpus_root = corpus_root.resolve()
    rel = fs_path.relative_to(corpus_root)
    if rel.suffix != ".ut":
        raise ValueError(f"expected .ut suffix on {fs_path}, got {rel.suffix}")
    parts = list(rel.with_suffix("").parts)
    if not parts:
        raise ValueError(f"empty path after stripping .ut: {fs_path}")
    # The root segment is always an identifier (system, suites, etc.).
    out = parts[0]
    for seg in parts[1:]:
        q = _quote_segment(seg)
        if q.startswith("["):
            out += q
        else:
            out += "." + q
    return out


# ---------------------------------------------------------------------------
# Verification loop
# ---------------------------------------------------------------------------

class DriftRecord:
    __slots__ = ("path", "odb_path", "reason", "diff")

    def __init__(self, path: Path, odb_path: str, reason: str, diff: str = ""):
        self.path = path
        self.odb_path = odb_path
        self.reason = reason
        self.diff = diff


def verify_files(
    paths: Iterable[Path],
    corpus_root: Path,
    cli_path: Path,
    system_root: Path,
    show_diff: bool = True,
    rewrite: bool = False,
) -> list[DriftRecord]:
    """Verify each .ut path against Virgin.root. Returns list of drift records.

    If `rewrite` is True, drifted .ut files are OVERWRITTEN with the
    kernel-canonical form (after normalization). The drift records are
    still returned so the caller can report what was changed.
    """
    drifts: list[DriftRecord] = []
    with ProtocolSession(cli_path, system_root) as sess:
        for ut_path in paths:
            try:
                odb_path = fs_to_odb_path(ut_path, corpus_root)
            except ValueError as e:
                drifts.append(DriftRecord(ut_path, "", f"path translation: {e}"))
                continue

            try:
                ok, raw_value, err = sess.get_script_body(odb_path)
            except RuntimeError as e:
                drifts.append(DriftRecord(ut_path, odb_path, f"protocol error: {e}"))
                continue

            if not ok:
                # Script doesn't exist at that ODB path → drift: .ut on disk,
                # nothing in Virgin.root.
                drifts.append(
                    DriftRecord(
                        ut_path,
                        odb_path,
                        f"path not found in Virgin.root: {err}",
                    )
                )
                continue

            if not raw_value:
                # success=true but no string value — non-script node.
                # Skip with note; the .ut probably wraps a non-script
                # object we don't know how to render canonically.
                drifts.append(
                    DriftRecord(
                        ut_path,
                        odb_path,
                        "non-script node (success=true but no value)",
                    )
                )
                continue

            kernel_bytes = normalize_kernel_body(raw_value)
            try:
                disk_bytes = ut_path.read_bytes()
            except OSError as e:
                drifts.append(DriftRecord(ut_path, odb_path, f"read error: {e}"))
                continue

            # Symmetric trailing-LF strip: .ut files in the corpus are
            # inconsistent (some have a trailing newline, some don't);
            # kernel output never does. Normalize both sides to "no
            # trailing LF" before comparing.
            disk_bytes_cmp = disk_bytes[:-1] if disk_bytes.endswith(b"\n") else disk_bytes

            if kernel_bytes != disk_bytes_cmp:
                diff_str = ""
                if show_diff:
                    try:
                        k = kernel_bytes.decode("utf-8", errors="replace").splitlines(keepends=True)
                        d = disk_bytes_cmp.decode("utf-8", errors="replace").splitlines(keepends=True)
                        diff_str = "".join(
                            difflib.unified_diff(
                                k,
                                d,
                                fromfile=f"kernel:{odb_path}",
                                tofile=f"disk:{ut_path}",
                                n=2,
                            )
                        )
                    except Exception as e:
                        diff_str = f"(diff render error: {e})"
                reason = "content drift"
                if rewrite:
                    # Containment check: --rewrite is a generic file-overwrite
                    # primitive if the caller controls both --paths and
                    # --corpus-root. Refuse to write outside the resolved
                    # corpus root. (CWE-22 / CWE-73 mitigation.)
                    try:
                        resolved = ut_path.resolve()
                        corpus_resolved = corpus_root.resolve()
                        if not _path_within(resolved, corpus_resolved):
                            reason = "content drift (rewrite refused: outside corpus root)"
                        else:
                            ut_path.write_bytes(kernel_bytes)
                            reason = "content drift (rewritten)"
                    except OSError as e:
                        reason = f"content drift (rewrite failed: {e})"
                drifts.append(
                    DriftRecord(
                        ut_path,
                        odb_path,
                        reason,
                        diff_str,
                    )
                )
    return drifts


# ---------------------------------------------------------------------------
# Mode handlers
# ---------------------------------------------------------------------------

# ODB scope filter: skip paths under these top-level keys. These hold
# user personalization / runtime state, not pre-startup canonical content.
SKIP_PREFIXES = ("user.", "scratchpad.", "workspace.", "system.misc.runtime.")


def in_scope(odb_path: str) -> bool:
    for skip in SKIP_PREFIXES:
        if odb_path == skip[:-1] or odb_path.startswith(skip):
            return False
    return True


def collect_full_corpus(corpus_root: Path) -> list[Path]:
    paths: list[Path] = []
    for p in corpus_root.rglob("*.ut"):
        if not p.is_file():
            continue
        try:
            odb = fs_to_odb_path(p, corpus_root)
        except ValueError:
            continue
        if in_scope(odb):
            paths.append(p)
    paths.sort()
    return paths


def collect_staged(corpus_root: Path) -> list[Path]:
    """Return staged .ut paths under the corpus_root."""
    result = subprocess.run(
        ["git", "diff", "--cached", "--name-only", "--diff-filter=ACMR"],
        capture_output=True,
        text=True,
        cwd=REPO_ROOT,
    )
    if result.returncode != 0:
        return []
    paths: list[Path] = []
    # corpus_root must be relative to REPO_ROOT for git's output to match.
    try:
        prefix = str(corpus_root.resolve().relative_to(REPO_ROOT.resolve())) + os.sep
    except ValueError:
        # corpus_root is outside the repo (e.g., sandbox). Return [] to be safe.
        return []
    for line in result.stdout.splitlines():
        if not line.endswith(".ut"):
            continue
        if not line.startswith(prefix):
            continue
        p = REPO_ROOT / line
        if p.is_file():
            paths.append(p)
    return paths


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(
        description="Verify .ut corpus matches scripts in Virgin.root.",
    )
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument("--full", action="store_true", help="Walk entire .ut corpus.")
    mode.add_argument(
        "--paths",
        nargs="+",
        metavar="PATH",
        help="Verify specific .ut file(s).",
    )
    ap.add_argument(
        "--corpus-root",
        type=Path,
        default=DEFAULT_CORPUS_ROOT,
        help=f"Corpus root (default: {DEFAULT_CORPUS_ROOT}).",
    )
    ap.add_argument(
        "--system-root",
        type=Path,
        default=DEFAULT_SYSTEM_ROOT,
        help=f"ODB system root (default: {DEFAULT_SYSTEM_ROOT}).",
    )
    ap.add_argument(
        "--cli",
        type=Path,
        default=DEFAULT_CLI,
        help=f"frontier-cli binary (default: {DEFAULT_CLI}).",
    )
    ap.add_argument(
        "--no-diff",
        action="store_true",
        help="Suppress unified-diff output on drift (show paths only).",
    )
    ap.add_argument(
        "--quiet",
        action="store_true",
        help="Only print drift; suppress progress.",
    )
    ap.add_argument(
        "--rewrite",
        action="store_true",
        help=(
            "DANGER: overwrite drifted .ut files with the kernel-canonical "
            "form. Use this to re-sync a known-stale .ut corpus to match "
            "edits made directly in Virgin.root. Always inspect the resulting "
            "git diff before committing. Refuses to write outside the "
            "resolved --corpus-root for safety."
        ),
    )
    args = ap.parse_args(argv)

    # Sanity: cli + root must exist.
    if not args.cli.exists():
        print(f"ERROR: frontier-cli not found at {args.cli}", file=sys.stderr)
        print("Build it first: make -C frontier-cli", file=sys.stderr)
        return 2
    if not args.system_root.exists():
        print(f"ERROR: system root not found at {args.system_root}", file=sys.stderr)
        return 2

    # Determine path set.
    if args.full:
        if not args.quiet:
            print(f"Walking corpus: {args.corpus_root}", file=sys.stderr)
        paths = collect_full_corpus(args.corpus_root)
        if not args.quiet:
            print(f"  {len(paths)} in-scope .ut files", file=sys.stderr)
    elif args.paths:
        paths = [Path(p) for p in args.paths]
    else:
        # Incremental: read staged .ut files.
        paths = collect_staged(args.corpus_root)
        if not paths:
            if not args.quiet:
                print("No staged .ut files; nothing to verify.", file=sys.stderr)
            return 0
        if not args.quiet:
            print(f"Incremental: {len(paths)} staged .ut file(s)", file=sys.stderr)

    if not paths:
        return 0

    try:
        drifts = verify_files(
            paths,
            corpus_root=args.corpus_root,
            cli_path=args.cli,
            system_root=args.system_root,
            show_diff=not args.no_diff,
            rewrite=args.rewrite,
        )
    except RuntimeError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 2

    if not drifts:
        if not args.quiet:
            print(f"OK: all {len(paths)} file(s) match Virgin.root.", file=sys.stderr)
        return 0

    print(
        f"DRIFT: {len(drifts)} of {len(paths)} file(s) diverge from Virgin.root.",
        file=sys.stderr,
    )
    for d in drifts:
        print(file=sys.stderr)
        print(f"  {d.path}", file=sys.stderr)
        print(f"  ODB path: {d.odb_path or '(unmappable)'}", file=sys.stderr)
        print(f"  Reason:   {d.reason}", file=sys.stderr)
        if d.diff:
            print(file=sys.stderr)
            for line in d.diff.splitlines():
                print(f"    {line}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
