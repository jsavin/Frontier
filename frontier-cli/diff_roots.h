/*
	SPDX-License-Identifier: MIT

	Copyright (c) 2026 Frontier contributors

	diff_roots.h -- recursive value-level equality walker for two ODB roots.

	This is the acceptance instrument for the root-build-from-source plan
	(planning/ROOT_BUILD_FROM_SOURCE_PLAN.md, Step 1). Every later step is
	verified with it: the exporter/assembler round-trip law
	(diff(build(export(R)), R) == empty) and the CI drift check both reduce to
	"run this and require an empty diff".

	Design commitments, each one load-bearing:

	- PURE C VALUE ACCESS. No interpreter, no string() coercion (string() on a
	  `list` value HARD-ABORTS through try, #887), no hashgetvaluestring
	  (deparses and would false-diff). Payloads are compared as stored bytes.

	- BOTH ROOTS OPEN READ-ONLY. Neither file is written on disk, and the
	  walker sets no dirty flag itself. dbwrite() refuses writes to a
	  flreadonly database (db.c:715-728) and dbflushheader skips them
	  (db.c:855), so the on-disk guarantee is enforced below this layer too.

	  Precisely: the walker DOES bring values into memory (subtables and
	  outlines must be resident to be compared) and materialization sets
	  residency flags. What it never does is set a dirty flag, write a block,
	  or let anything it calls do so -- disk-value reads deliberately stop at
	  dbrefhandle_context rather than using the kernel resolver, which would
	  mark the owning table dirty as a side effect (langhash.c:2211).

	- NAMES ARE BYTES. ODB names legitimately contain tabs and raw CR/LF --
	  Virgin.root has sibling values named with the single bytes 0x0A/0x0B/
	  0x0C/0x0D under system.verbs.builtins.tcp.im.builtinDrivers.aim.code.
	  util.repl. Those two collide the moment names are newline-normalized, so
	  names are compared as raw bytes and only ESCAPED for reporting.

	- LOUD FAIL, NEVER SILENT SKIP. Any value the walker cannot compare
	  produces a per-path finding and a non-empty diff. "Lost artifacts"
	  become red builds by construction.
*/

#ifndef diffrootsinclude
#define diffrootsinclude

#include <stdio.h>

#include "frontier.h"

/*
	Maximum reported path length.

	Generous on purpose: a diff tool must never fail to NAME a path. The
	existing ODB walkers cap paths at 512 bytes and skip on overflow
	(frontier-cli/main.c:2429, Common/source/langhash.c:297); for a report that
	feeds an acceptance gate, a skipped path is a correctness hole, so overflow
	here is both rare and reported rather than silent.
*/
#define maxdiffpath 8192


/*
	Finding kinds, in the order the reporter prints their names.
*/
typedef enum tydiffkind {

	diffkind_only_in_a,		/*path present in A, absent in B*/
	diffkind_only_in_b,		/*path present in B, absent in A*/
	diffkind_type,			/*same path, different type*/
	diffkind_payload,		/*same path and type, different bytes*/
	diffkind_unsupported,		/*walker cannot compare this type -- loud fail*/
	diffkind_unreadable,		/*value could not be read/materialized on one side*/

	ctdiffkinds
	} tydiffkind;


/*
	One finding. `path` is the full ODB path with every segment percent-encoded
	(see ut_pct_encode_segment), so a '.' in the path is ALWAYS a separator and
	control bytes are unambiguous.

	sizea/sizeb are payload byte counts, or -1 where not applicable. firstdiff
	is the offset of the first differing byte for diffkind_payload, else -1.
	The finding never carries payload CONTENT: dumping bytes would both bloat
	the report and risk re-encoding MacRoman (#880).
*/
typedef struct tydifffinding {

	tydiffkind kind;
	const char *path;
	const char *typea;	/*4-char type tag, or a symbolic name; never nil*/
	const char *typeb;
	long sizea;
	long sizeb;
	long firstdiff;
	const char *detail;	/*optional extra context; may be nil*/

	} tydifffinding;


typedef void (*tydiffcallback) (const tydifffinding *finding, void *refcon);


/*
	Options controlling a walk. Zero-initialize for defaults.
*/
typedef struct tydiffoptions {

	FILE *out;		/*findings stream; nil means stdout*/
	boolean flquiet;	/*count findings without printing them*/
	tydiffcallback callback;	/*optional per-finding hook (tests use this)*/
	void *refcon;

	} tydiffoptions;


/*
	diff_roots_compare -- open both roots read-only, walk them, report every
	difference.

	Returns true if the walk COMPLETED (whether or not differences were found);
	returns false only on an operational failure (cannot open, cannot reach a
	root table, unsupported format). *ctfindings receives the number of
	findings; a completed walk with zero findings is the empty-diff case.

	Both paths must name v7 databases. A v6 root is rejected rather than
	misinterpreted: in v6 the database's views[0] points at a Cancoon record,
	while in v7 it IS the root table address (odbengine.c:566-570).
*/
extern boolean diff_roots_compare (const char *patha, const char *pathb,
                                   const tydiffoptions *options,
                                   long *ctfindings);


/*
	diff_roots_kindname -- stable lowercase-hyphenated name for a finding kind,
	as it appears in report output ("only-in-a", "payload", ...). Returns
	"unknown" for an out-of-range kind rather than indexing off the end.
*/
extern const char *diff_roots_kindname (tydiffkind kind);


/*
	diff_roots_appendsegment -- append one raw ODB name to a dotted path,
	percent-encoding it first.

	`path` is a NUL-terminated buffer of `pathsz` bytes holding the parent path
	("" for the root). `name`/`namelen` are the RAW name bytes (which may
	include tab, CR, LF, NUL, or high-bit MacRoman). Returns true on success.

	Returns false, leaving `path` unmodified, if the result would not fit --
	callers must treat that as a reportable condition, never as a reason to
	skip a value silently.
*/
extern boolean diff_roots_appendsegment (char *path, size_t pathsz,
                                         const char *name, size_t namelen);

#endif
