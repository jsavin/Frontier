/*
	SPDX-License-Identifier: MIT

	Copyright (c) 2026 Frontier contributors

	diff_roots.c -- recursive value-level equality walker for two ODB roots.

	See diff_roots.h for the design commitments. Implementation notes that are
	not obvious from the header:

	- Traversal buffers each table and sorts by raw name bytes before matching.
	  hfirstsort is NOT alphabetical in this build: sorted insertion consults
	  langcallbacks.comparenodescallback (langhash.c:1464) and headless startup
	  wires that to cb_noop_compare, which always returns 0 (langstartup.c:81,
	  :1172), so nodes append in INSERTION order. Sorting is what makes matching
	  correct across roots with different insertion histories, and what makes
	  output deterministic.

	- Names are compared as RAW BYTES (memcmp over the Pascal string), never
	  case-folded and never newline-normalized. Storage is case-preserving, and
	  Virgin.root contains sibling names that differ only by control byte.

	- Externals ('scpt', 'optx', 'tabl', 'mbar', ...) are NOT distinct
	  valuetypes: they are all externalvaluetype, discriminated by the
	  tyexternalid on the external variable (lang.h:266-271).

	- Script and outline payloads are compared as oppackoutline() bytes. That
	  is the canonical serialized form, it encodes text AND node structure (so
	  the #881 single-LF-node class and the #890 comment-structure class both
	  surface), and a determinism spike proved it byte-stable across
	  independent loads of all three shipped roots.

	  NOT langexternalpack(): its outline branch calls dbassignhandle_context
	  (opverbs.c:1494) and writes disk blocks. That is a save path.

	- Menubars and wptext are compared at the STORED level, never materialized.
	  For mbar the headless stub menuverbinmemory_context
	  (stubs/headless_menu_stubs.c:618) sets flinmemory = true WITHOUT loading,
	  so variabledata is still a raw dbaddress and dereferencing it segfaults --
	  flinmemory is not trustworthy per-type in this build. For wptext,
	  force-materializing exhausts memory on a full-root walk (db_format.c:1694).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "frontier.h"
#include "standard.h"
#include "shell_api.h"
#include "db.h"
#include "dbinternal.h"
#include "lang.h"
#include "langexternal.h"
#include "langinternal.h"
#include "op.h"
#include "oplist.h"
#include "opverbs.h"
#include "tablestructure.h"
#include "db_format.h"
#include "file.h"
#include "strings.h"
#include "memory.h"
/*
	ut_pct_encode_segment is the lossless name codec shared with the ut-sync
	layer. NOTE for whoever retires ut-sync (Step 8 of the root-build plan):
	--diff-roots depends on this function. Lift it into a shared module rather
	than deleting it with the rest of that layer.
*/
#include "ut_sync.h"

#include "diff_roots.h"

#define maxwalkdepth 64

/*
	Worst-case encoded segment: a 255-byte Pascal name whose every byte escapes
	to three characters, plus the NUL.
*/
#define maxencodedsegment (255 * 3 + 1)


typedef struct tyrootside {

	const char *path;
	hdlfilenum fnum;
	hdldatabaserecord database;
	Handle hrootvariable;
	hdlhashtable roottable;
	db_context context;
	boolean flopen;

	} tyrootside;


typedef struct tywalkstate {

	const tydiffoptions *options;
	FILE *out;
	long ctfindings;

	} tywalkstate;


const char *diff_roots_kindname (tydiffkind kind) {

	static const char *names [ctdiffkinds] = {
		"only-in-a",
		"only-in-b",
		"type",
		"payload",
		"unsupported-type",
		"unreadable"
		};

	if ((kind < 0) || (kind >= ctdiffkinds))
		return ("unknown");

	return (names [kind]);
	} /*diff_roots_kindname*/


boolean diff_roots_appendsegment (char *path, size_t pathsz, const char *name, size_t namelen) {

	char encoded [maxencodedsegment];
	size_t used;
	size_t need;

	if ((path == NULL) || (pathsz == 0))
		return (false);

	if (!ut_pct_encode_segment (name, namelen, encoded, sizeof (encoded)))
		return (false);

	used = strlen (path);

	/*One byte for the '.' separator when this is not the first segment.*/
	need = used + ((used > 0) ? 1 : 0) + strlen (encoded) + 1;

	if (need > pathsz)
		return (false); /*caller reports; never a silent skip*/

	if (used > 0) {
		path [used] = '.';
		memcpy (path + used + 1, encoded, strlen (encoded) + 1);
		}
	else
		memcpy (path, encoded, strlen (encoded) + 1);

	return (true);
	} /*diff_roots_appendsegment*/


/*
	Emit an ODB path to a stream with high-bit bytes escaped.

	THE ONLY WAY a path reaches human-readable output. Every emission site must
	route through here -- an earlier version escaped in the findings reporter
	but printed the raw path in an audit note, which is the same class of
	inconsistency in a smaller place.

	Escaping happens at the REPORT layer only. The shared name codec
	(ut_pct_encode_segment) passes >= 0x80 through untouched, and that is
	correct: it is a lossless bijection used for filesystem paths, and MacRoman
	bytes must survive it byte-for-byte (#880). But a terminal renders those
	bytes through whatever locale it happens to have, so raw emission is
	ambiguous to read and unsafe to paste into a bug report. Findings handed to
	a callback still carry the raw bytes.
*/
static void emitpath (FILE *out, const char *path) {

	const unsigned char *p;

	if (path == NULL)
		return;

	for (p = (const unsigned char *) path; *p != '\0'; p++) {

		if (*p >= 0x80)
			fprintf (out, "%%%02X", (unsigned) *p);
		else
			fputc ((int) *p, out);
		}
	} /*emitpath*/


static void report (tywalkstate *state, tydiffkind kind, const char *path,
                    const char *typea, const char *typeb,
                    long sizea, long sizeb, long firstdiff, const char *detail) {

	tydifffinding finding;

	finding.kind = kind;
	finding.path = path;
	finding.typea = (typea != NULL) ? typea : "-";
	finding.typeb = (typeb != NULL) ? typeb : "-";
	finding.sizea = sizea;
	finding.sizeb = sizeb;
	finding.firstdiff = firstdiff;
	finding.detail = detail;

	state->ctfindings++;

	if (state->options->callback != NULL)
		(*state->options->callback) (&finding, state->options->refcon);

	if (state->options->flquiet)
		return;

	fprintf (state->out, "%-16s ", diff_roots_kindname (kind));

	emitpath (state->out, path);

	if ((kind == diffkind_type) || (kind == diffkind_unsupported))
		fprintf (state->out, "  A=%s B=%s", finding.typea, finding.typeb);
	else if (kind == diffkind_payload) {
		fprintf (state->out, "  type=%s A=%ldbytes B=%ldbytes", finding.typea, sizea, sizeb);

		if (firstdiff >= 0)
			fprintf (state->out, " first-diff-offset=%ld", firstdiff);
		}
	else if ((kind == diffkind_only_in_a) || (kind == diffkind_only_in_b))
		fprintf (state->out, "  type=%s",
		         (kind == diffkind_only_in_a) ? finding.typea : finding.typeb);

	if (detail != NULL)
		fprintf (state->out, "  (%s)", detail);

	fprintf (state->out, "\n");
	} /*report*/


/*
	Symbolic type name for a value. For externals this is the external's kind,
	which is what the census calls 'scpt'/'optx'/'tabl'/'mbar'; for everything
	else it is the scalar type. Never returns nil.
*/
static const char *typename_for (const tyvaluerecord *val) {

	if (val == NULL)
		return ("-");

	if ((*val).valuetype == externalvaluetype) {

		hdlexternalvariable hv = (hdlexternalvariable) (*val).data.externalvalue;

		if (hv == nil)
			return ("exte-nil");

		switch ((**hv).id) {

			case idoutlineprocessor:
				return ((**hv).flscript ? "scpt" : "optx");

			case idscriptprocessor:
				return ("scpt");

			case idtableprocessor:
				return ("tabl");

			case idmenuprocessor:
				return ("mbar");

			case idwordprocessor:
				return ("wptx");

			case idpictprocessor:
				return ("pict");

			default:
				return ("exte");
			}
		}

	switch ((*val).valuetype) {

		case novaluetype:		return ("????");
		case charvaluetype:		return ("char");
		case intvaluetype:		return ("shor");
		case longvaluetype:		return ("long");
		case oldstringvaluetype:	/*unpacks into the same field as stringvaluetype*/
		case stringvaluetype:		return ("TEXT");
		case binaryvaluetype:		return ("data");
		case booleanvaluetype:		return ("bool");
		case tokenvaluetype:		return ("tokn");
		case datevaluetype:		return ("date");
		case addressvaluetype:		return ("addr");
		case doublevaluetype:		return ("doub");
		case directionvaluetype:	return ("dir ");
		case ostypevaluetype:		return ("type");
		case enumvaluetype:		return ("enum");
		case pointvaluetype:		return ("QDpt");
		case rectvaluetype:		return ("qdrt");
		case rgbvaluetype:		return ("cRGB");
		case patternvaluetype:		return ("patt");
		case objspecvaluetype:		return ("obj ");
		case filespecvaluetype:		return ("fspc");
		case aliasvaluetype:		return ("alis");
		case listvaluetype:		return ("list");
		case recordvaluetype:		return ("reco");
		case codevaluetype:		return ("code");
		case passwordvaluetype:		return ("pass");
		case singlevaluetype:		return ("sing");
		case fixedvaluetype:		return ("fixd");

		default:
			return ("unkn");
		}
	} /*typename_for*/


/*
	Raw payload bytes for the scalar types whose data lives in a Handle. All of
	these alias the same Handle slot in tyvaluedata, which is why langgetvalsize
	reads them all through data.binaryvalue.

	Returns false when this type is not handle-backed.
*/
static boolean handlebytes (const tyvaluerecord *val, const unsigned char **pbytes, long *psize) {

	Handle h;

	/*
	An fldiskval value has NOT been faulted in: data.binaryvalue still holds a
	dbaddress, not a pointer, and dereferencing it segfaults. Callers resolve
	those separately (resolvediskvalue) rather than resolving here, because the
	kernel's resolver dirties the owning table as a side effect
	(langhash.c:2211) and this walker must not dirty either root.
	*/
	if ((*val).fldiskval)
		return (false);

	switch ((*val).valuetype) {

		case oldstringvaluetype:
		case stringvaluetype:
		case binaryvaluetype:
		case passwordvaluetype:
		case patternvaluetype:
		case rgbvaluetype:
		case rectvaluetype:
		case objspecvaluetype:
		case aliasvaluetype:
		case doublevaluetype:
		case addressvaluetype:
			break;

		default:
			return (false);
		}

	h = (Handle) (*val).data.binaryvalue;

	if ((h == nil) || (*h == nil)) {
		*pbytes = NULL;
		*psize = 0;
		return (true); /*empty is a legitimate, comparable state*/
		}

	*pbytes = (const unsigned char *) *h;
	*psize = gethandlesize (h);

	return (true);
	} /*handlebytes*/


/*
	Read the stored bytes of an fldiskval value WITHOUT mutating anything.

	The kernel's own resolver (hashresolvevalue_context, langhash.c:2196) does
	the same dbrefhandle call, but then writes the handle back into the node and
	sets (**htable).fldirty = true "because we released the disk value". A
	read-only walker must not do either, so this stops at the read. The caller
	disposes the handle.
*/
static boolean resolvediskvalue (tyrootside *side, const tyvaluerecord *val, Handle *h) {

	hdldatabaserecord saved = databasedata;
	boolean fl;

	*h = nil;

	if (!(*val).fldiskval)
		return (false);

	/*
	Park the global on this side for the duration of the read. dbrefhandle_context
	takes an explicit context, but the layers beneath it still consult
	databasedata, and with two roots open the global may be pointing at the
	other one. Callee-saves, matching the convention documented at
	langexternal.c:975-979.
	*/
	databasedata = side->database;

	fl = dbrefhandle_context (&side->context, (*val).data.diskvalue, h);

	databasedata = saved;

	return (fl);
	} /*resolvediskvalue*/


/*
	Where an unmaterialized external's stored block actually lives.

	oldaddress is NOT the answer on its own. For a value that has never been
	materialized, langnewexternalvariable initializes oldaddress to nildbaddress
	(langexternal.c:2932, :2997) and puts the stored address in variabledata --
	menuverbunpack (menuverbs.c:501-509) is the concrete path: it reads the
	address off disk and passes it as newmenuvariable's variabledata argument.

	An earlier version of this walker read only oldaddress. For every
	never-materialized menubar that was nil on BOTH sides, which combined with a
	nil-means-empty-handle rule to make every mbar and wptext compare EQUAL
	regardless of content. The comparison was vacuous, and self-diff plus a
	cross-era diff both "passed" it because neither had a fixture where a
	menubar actually differed.

	The fallback below mirrors what the menu stub itself does
	(headless_menu_stubs.c:551-553).

	Returns nildbaddress only when the value genuinely has no stored block; the
	caller must treat that as reportable, never as equality.

	KNOWN-LOUD: the returned address is used as-is, with no normalization
	(dbrefhandle_context's underlying read does none). If a stored address were
	somehow interior or stale, the read fails and the caller emits an
	`unreadable` finding naming the path -- a visible, attributable failure
	rather than a silent wrong answer. That is the intended behavior, not an
	unhandled case.
*/
static dbaddress storedaddressof (hdlexternalvariable hv) {

	dbaddress adr;

	if (hv == nil)
		return (nildbaddress);

	adr = (**hv).oldaddress;

	if ((adr == nildbaddress) && !(**hv).flinmemory)
		adr = (dbaddress) (**hv).variabledata;

	return (adr);
	} /*storedaddressof*/


/*
	Read the raw stored block at `adr` from one side, without materializing the
	value it belongs to. Used for the types that must not be brought into memory
	(see compareexternal). The caller disposes the handle.

	A nil address returns FALSE rather than an empty handle. Treating "no
	address" as "empty content" is what made the mbar comparison silently
	vacuous; the caller reports it instead.
*/
static boolean readstoredbytes (tyrootside *side, dbaddress adr, Handle *h) {

	hdldatabaserecord saved = databasedata;
	boolean fl;

	*h = nil;

	if (adr == nildbaddress)
		return (false);

	databasedata = side->database;	/*callee-saves; see resolvediskvalue*/

	fl = dbrefhandle_context (&side->context, adr, h);

	databasedata = saved;

	return (fl);
	} /*readstoredbytes*/


/*
	Inline scalars: compared by value, not by dereference. Returns false when
	this type is not an inline scalar.
*/
static boolean inlineequal (const tyvaluerecord *a, const tyvaluerecord *b, boolean *pequal) {

	switch ((*a).valuetype) {

		case novaluetype:
			/*Nothing is stored for these (langpack.c: "nothing to pack").*/
			*pequal = true;
			return (true);

		case booleanvaluetype:
			*pequal = ((*a).data.flvalue == (*b).data.flvalue);
			return (true);

		case charvaluetype:
			*pequal = ((*a).data.chvalue == (*b).data.chvalue);
			return (true);

		case intvaluetype:
			*pequal = ((*a).data.intvalue == (*b).data.intvalue);
			return (true);

		case longvaluetype:
		case datevaluetype:
		case fixedvaluetype:
			*pequal = ((*a).data.longvalue == (*b).data.longvalue);
			return (true);

		case ostypevaluetype:
			*pequal = ((*a).data.ostypevalue == (*b).data.ostypevalue);
			return (true);

		case enumvaluetype:
			*pequal = ((*a).data.enumvalue == (*b).data.enumvalue);
			return (true);

		case directionvaluetype:
			*pequal = ((*a).data.dirvalue == (*b).data.dirvalue);
			return (true);

		case tokenvaluetype:
			*pequal = ((*a).data.tokenvalue == (*b).data.tokenvalue);
			return (true);

		case singlevaluetype:
			/*
			memcmp, not ==. This is a STORED-BYTES comparison, and float
			equality is the wrong predicate for it twice over: NaN != NaN would
			report two identical stored values as different, and +0.0 == -0.0
			would report two different stored values as identical. Comparing
			the bytes answers the question actually being asked.
			*/
			*pequal = (memcmp (&(*a).data.singlevalue, &(*b).data.singlevalue,
			                   sizeof ((*a).data.singlevalue)) == 0);
			return (true);

		case pointvaluetype:
			*pequal = (memcmp (&(*a).data.pointvalue, &(*b).data.pointvalue,
			                   sizeof ((*a).data.pointvalue)) == 0);
			return (true);

		default:
			return (false);
		}
	} /*inlineequal*/


/*
	Packed bytes for an outline-backed external (script or outline). The caller
	disposes the handle. Menubars do NOT come here -- see the file header.
*/
static boolean packoutlinebytes (hdlexternalvariable hv, Handle *hpacked) {

	hdloutlinerecord ho;

	*hpacked = nil;

	if (!(**hv).flinmemory)
		return (false);

	ho = (hdloutlinerecord) (**hv).variabledata;

	if (ho == nil)
		return (false);

	/*oppackoutline APPENDS to a non-nil handle, so it must start nil.*/
	return (oppackoutline (ho, hpacked));
	} /*packoutlinebytes*/


/*
	The packed-outline header carries SAVE METADATA, not content, and those
	fields must be excluded from an equality basis or every value in a re-saved
	root reports as different.

	The regions are NOT hardcoded here. They come from
	op_packed_header_volatile_regions() (op.h / oppack_v7.c), derived with
	offsetof()/sizeof() on the real struct. That indirection exists because the
	first version of this walker DID hardcode them and got them wrong by +6.

	The cause was THIS UNIT'S OWN throwaway offset probe (a scratch program
	written while designing the walker, never committed): it declared a copy of
	typortablediskheader WITHOUT #pragma pack(2), so it reported 24/32/40/48
	with a 1080-byte header, while the real packed layout is 18/26/34 with 1068
	bytes. The _Static_assert at oppack_v7.c:154 pins 1068 and would have caught
	it immediately had the probe's own output been checked against it --
	measuring instruments need their own invariant checks, the same lesson this
	walker exists to enforce one level up. Two silent failures resulted -- timecreated
	(18-23) was left UNSKIPPED, producing false diffs whenever creation stamps
	differ, and fltextmode (38-39) was WRONGLY skipped, letting real content
	compare equal. Deriving from the struct makes that class of error
	impossible.

	Excluded, all with demonstrated volatility:

	  timecreated   -- per-value creation stamp
	  timelastsave  -- rewritten on every save
	  ctsaves       -- INCREMENTED by oppack itself (oppack_v7.c), so it differs
	                   even between two packs of one resident value

	NOT excluded: outlinesignature. An earlier version skipped it on the
	assumption it was volatile. Dumping real packed values from Virgin.root
	shows it is the constant 'LAND' throughout, and excluding a field with no
	demonstrated volatility is precisely the silent-false-equality failure this
	whole exclusion mechanism must avoid. If a signature difference ever appears
	it should surface as a finding and be attributed, not pre-swallowed.

	Empirical note on the original 3,527 spurious findings: 3,495 first differed
	at offset 37 and 27 at offset 49. Both are ctsaves -- offset 37 is its last
	byte in a bare packed outline (34..37), and offset 49 is its last byte
	inside a packed LIST, displaced by the 12-byte list header (12+34=46..49).
	The earlier attribution of those to timelastsave and outlinesignature was
	wrong; the DATA was right, the field names were not.

	timecreated is excluded too: it is per-VALUE provenance metadata that the
	root-build plan preserves in the manifest (decision 9.4), so it is exactly
	the kind of thing that legitimately differs between a built root and its
	ancestor without the CONTENT differing.

	CONSEQUENCE, stated plainly so it is not later mistaken for a hole:
	excluding timecreated/timelastsave makes this tool BLIND TO CREATE/MOD-DATE
	DRIFT BY DESIGN. That is correct for Step 1 drift detection -- a re-saved
	root with identical content must compare equal, or the acceptance gate is
	useless. Dates are covered long-term by plan decision 9.4: the exporter
	preserves per-value dates in the source manifests, so the round-trip law
	checks them AS MANIFEST TEXT, not as packed bytes. Content equality here,
	date equality there; neither check subsumes the other.

	Packed LISTS inherit these same fields, because oppacklist emits a list
	header followed by a packed outline (oplist.c:708-711). The displacement is
	READ FROM THE LIST HEADER'S OWN recordsize field (oplist.c:88, "number of
	bytes in this header") rather than hardcoded -- see listoutlinebase() -- so
	the walker cannot silently drift out of agreement with the format if
	tydisklistrecord changes shape.

	NOTE: this makes --diff-roots a CONTENT comparison. A dedicated metadata
	comparison is a separate tool if one is ever wanted.
*/
#define maxheaderregions 8

static typackedheaderregion g_headerregions [maxheaderregions];
static long g_ctheaderregions = 0;
static long g_headerbytes = 0;
static boolean g_headerregionsready = false;


/*
	Load the volatile-region table once, from oppack_v7.c via offsetof/sizeof on
	the real struct. Hardcoding these offsets is how the first version of this
	walker got them wrong by +6: this unit's own scratch offset probe declared
	the struct WITHOUT #pragma pack(2) and so reported 24/32/40/48 with a
	1080-byte header, when the real packed layout is 18/26/34 with 1068 bytes
	(the _Static_assert at oppack_v7.c:154 is the invariant that would have
	caught it).

	Returns false if the table cannot be obtained, in which case the caller
	compares the whole buffer verbatim -- over-reporting, never under-reporting.

	The static cache is not thread-safe. That is fine here and nowhere else:
	this mode runs in main.c's early-exit band, before any runtime or thread
	registry exists, single-threaded by construction. Anything reusing this in
	a threaded context must not reuse the cache.
*/
static boolean loadheaderregions (void) {

	if (g_headerregionsready)
		return (g_ctheaderregions > 0);

	g_headerregionsready = true;

	g_ctheaderregions = maxheaderregions;

	if (!op_packed_header_volatile_regions (g_headerregions, &g_ctheaderregions)) {
		g_ctheaderregions = 0;
		return (false);
		}

	g_headerbytes = op_packed_header_size ();

	return (g_ctheaderregions > 0);
	} /*loadheaderregions*/


static boolean inskipregion (long offset) {

	long i;

	for (i = 0; i < g_ctheaderregions; i++) {

		const typackedheaderregion *r = &g_headerregions [i];

		if ((offset >= r->offset) && (offset < (r->offset + r->length)))
			return (true);
		}

	return (false);
	} /*inskipregion*/


/*
	Compare two packed buffers, ignoring the volatile fields of a packed-outline
	header that begins at `outlinebase`.

	`outlinebase` is 0 for a packed outline, and the size of the list header for
	a packed list -- oppacklist emits `tydisklistrecord` followed by the packed
	outline (oplist.c:708-711), so a list inherits the same volatile fields at a
	fixed displacement.

	Returns the offset of the first CONTENT difference, or -1 when equal.
*/
static long outlinecontentdifference (const unsigned char *a, long cta,
                                      const unsigned char *b, long ctb,
                                      long outlinebase) {

	long i;
	boolean haveregions = loadheaderregions ();

	if (cta != ctb)
		return ((cta < ctb) ? cta : ctb); /*length difference is a content difference*/

	for (i = 0; i < cta; i++) {

		long relative;

		if (a [i] == b [i])
			continue;

		relative = i - outlinebase;

		if (haveregions
		        && (relative >= 0) && (relative < g_headerbytes)
		        && inskipregion (relative))
			continue;

		return (i);
		}

	return (-1);
	} /*outlinecontentdifference*/


/*
	Byte offset of the packed outline inside a packed list.

	Read from the list header itself rather than hardcoded: tydisklistrecord's
	first field is `recordsize` -- "number of bytes in this header" (oplist.c:88)
	-- written as a big-endian short by oppacklist (oplist.c:694). Deriving it
	means the walker cannot drift from the format if that struct changes.

	Returns -1 if the buffer is too small or the size is implausible, in which
	case the caller compares the whole buffer verbatim.
*/
static long listoutlinebase (const unsigned char *packed, long ct) {

	long recordsize;

	if ((packed == NULL) || (ct < 2))
		return (-1);

	recordsize = ((long) packed [0] << 8) | (long) packed [1];

	if ((recordsize < 4) || (recordsize > ct))
		return (-1);

	return (recordsize);
	} /*listoutlinebase*/


static long firstdifference (const unsigned char *a, const unsigned char *b, long ct) {

	long i;

	for (i = 0; i < ct; i++)
		if (a [i] != b [i])
			return (i);

	return (-1);
	} /*firstdifference*/


static void comparepayload (tywalkstate *state, const char *path,
                            tyrootside *sidea, tyrootside *sideb,
                            const tyvaluerecord *vala, const tyvaluerecord *valb);


static void walkpair (tywalkstate *state, const char *path,
                      tyrootside *sidea, tyrootside *sideb,
                      hdlhashtable tablea, hdlhashtable tableb, int depth);


/*
	Materialize an external read-only, if it is the kind we are willing to
	materialize. Returns false when the value cannot be brought into memory --
	the caller reports that rather than skipping.
*/
static boolean materialize (tyrootside *side, hdlexternalvariable hv) {

	if ((**hv).flinmemory)
		return (true);

	return (ensure_external_in_memory (&side->context, hv));
	} /*materialize*/


static void compareexternal (tywalkstate *state, const char *path,
                             tyrootside *sidea, tyrootside *sideb,
                             const tyvaluerecord *vala, const tyvaluerecord *valb,
                             int depth) {

	hdlexternalvariable hva = (hdlexternalvariable) (*vala).data.externalvalue;
	hdlexternalvariable hvb = (hdlexternalvariable) (*valb).data.externalvalue;
	tyexternalid extid;

	if ((hva == nil) || (hvb == nil)) {
		report (state, diffkind_unreadable, path, typename_for (vala), typename_for (valb),
		        -1, -1, -1, "nil external variable");
		return;
		}

	if ((**hva).id != (**hvb).id) {
		report (state, diffkind_type, path, typename_for (vala), typename_for (valb),
		        -1, -1, -1, NULL);
		return;
		}

	extid = (**hva).id;

	switch (extid) {

		case idtableprocessor: {

			if (!materialize (sidea, hva) || !materialize (sideb, hvb)) {
				report (state, diffkind_unreadable, path, "tabl", "tabl", -1, -1, -1,
				        "subtable could not be loaded");
				return;
				}

			walkpair (state, path, sidea, sideb,
			          (hdlhashtable) (**hva).variabledata,
			          (hdlhashtable) (**hvb).variabledata,
			          depth + 1);
			return;
			}

		case idoutlineprocessor:
		case idscriptprocessor: {

			Handle hpackeda = nil;
			Handle hpackedb = nil;
			long sizea, sizeb;

			if (!materialize (sidea, hva) || !materialize (sideb, hvb)) {
				report (state, diffkind_unreadable, path,
				        typename_for (vala), typename_for (valb), -1, -1, -1,
				        "outline could not be loaded");
				return;
				}

			if (!packoutlinebytes (hva, &hpackeda) || !packoutlinebytes (hvb, &hpackedb)) {

				if (hpackeda != nil)
					disposehandle (hpackeda);

				if (hpackedb != nil)
					disposehandle (hpackedb);

				report (state, diffkind_unreadable, path,
				        typename_for (vala), typename_for (valb), -1, -1, -1,
				        "outline could not be packed");
				return;
				}

			sizea = gethandlesize (hpackeda);
			sizeb = gethandlesize (hpackedb);

			{
			long offset = outlinecontentdifference ((const unsigned char *) *hpackeda, sizea,
			                                        (const unsigned char *) *hpackedb, sizeb,
			                                        0);

			if (offset >= 0)
				report (state, diffkind_payload, path,
				        typename_for (vala), typename_for (valb),
				        sizea, sizeb, offset, NULL);
			}

			disposehandle (hpackeda);
			disposehandle (hpackedb);
			return;
			}

		case idmenuprocessor:
		case idwordprocessor: {

			/*
			Compared at the STORED level: the raw disk bytes at each side's
			external address, never materialized.

			Stored bytes are the ODB's ground truth, and the round-trip law
			requires the assembler to reproduce them anyway, so this is the
			right basis for these types rather than a concession.

			The two types are here for DIFFERENT reasons, and the reason-codes
			below are kept distinct on purpose so a later reader does not merge
			them:

			  mbar  -- cannot be materialized headless AT ALL. The stub
			           menuverbinmemory_context (stubs/headless_menu_stubs.c:618)
			           sets flinmemory = true WITHOUT loading, leaving
			           variabledata a raw dbaddress; dereferencing it segfaults.
			           Filed as #896.

			  wptx  -- CAN be materialized, but must not be during a full-root
			           walk: force-materializing wptext externals exhausts
			           memory and hangs for minutes (db_format.c:1694).
			*/
			const char *reason = (extid == idmenuprocessor)
			        ? "mbar: not materializable headless (#896); compared at stored level"
			        : "wptx: materialization hazard (db_format.c:1694); compared at stored level";

			Handle ha = nil;
			Handle hb = nil;
			dbaddress adra = storedaddressof (hva);
			dbaddress adrb = storedaddressof (hvb);
			boolean oka;
			boolean okb;

			/*
			NEVER-STORED ON BOTH SIDES.

			With the old nil-means-empty-handle rule removed (it is what made
			the mbar comparison vacuous), a value with no stored block on
			EITHER side would otherwise be reported unreadable -- which would
			break the self-diff law, since a root compared against itself would
			produce findings.

			Two values that are both genuinely never-stored ARE equal: there is
			no content on either side to differ. So report no difference, but
			emit an auditable note naming the path, so the case can be reviewed
			by a human without the exit code lying in either direction. The
			note goes to stderr, keeping the findings stream on stdout clean
			for diffing.

			The shipped roots contain no such values (every mbar and wptx
			resolves through the variabledata fallback), but the tool has to be
			correct in general, not just on today's inputs.
			*/
			if ((adra == nildbaddress) && (adrb == nildbaddress)) {

				if (!state->options->flquiet) {
					fprintf (stderr, "note: ");
					emitpath (stderr, path);	/*escaped, like every other path emission*/
					fprintf (stderr, " never stored on either side (%s); treated as equal\n",
					         typename_for (vala));
					}

				return;
				}

			oka = readstoredbytes (sidea, adra, &ha);
			okb = readstoredbytes (sideb, adrb, &hb);

			if (!oka || !okb) {

				if (ha != nil)
					disposehandle (ha);

				if (hb != nil)
					disposehandle (hb);

				/*
				One side has a stored block the other lacks, or a read failed.
				Either way this is a real finding, not an equality: reporting
				it is what keeps a missing value from passing silently.
				*/
				report (state, diffkind_unreadable, path,
				        typename_for (vala), typename_for (valb), -1, -1, -1,
				        reason);
				return;
				}

			{
			long sizea = gethandlesize (ha);
			long sizeb = gethandlesize (hb);

			if ((sizea != sizeb)
			        || ((sizea > 0)
			            && (memcmp (*ha, *hb, (size_t) sizea) != 0)))
				report (state, diffkind_payload, path,
				        typename_for (vala), typename_for (valb), sizea, sizeb,
				        (sizea == sizeb)
				                ? firstdifference ((const unsigned char *) *ha,
				                                   (const unsigned char *) *hb, sizea)
				                : -1,
				        reason);
			}

			disposehandle (ha);
			disposehandle (hb);
			return;
			}

		default:
			/*
			Reached only by external kinds that do not occur in any shipped
			root: the 2026-08-11 census records no pict (and no filespec, alias
			or code scalars) across Virgin.root, mainResponder.root or
			manila.root. Rather than write comparison code that cannot be
			exercised -- and so cannot be trusted when it finally is -- these
			fail loudly with the path, which is the documented contract. The
			first root that carries one turns into a red build naming exactly
			what to implement.
			*/
			report (state, diffkind_unsupported, path,
			        typename_for (vala), typename_for (valb), -1, -1, -1,
			        "unhandled external kind (not present in any shipped root)");
			return;
		}
	} /*compareexternal*/


static void comparepayload (tywalkstate *state, const char *path,
                            tyrootside *sidea, tyrootside *sideb,
                            const tyvaluerecord *vala, const tyvaluerecord *valb) {

	boolean equal = false;
	const unsigned char *bytesa = NULL;
	const unsigned char *bytesb = NULL;
	long sizea = 0;
	long sizeb = 0;

	/*
	Unresolved disk values first: their payload is still on disk, so they must
	be read through the owning side's context rather than dereferenced.
	*/
	if ((*vala).fldiskval || (*valb).fldiskval) {

		Handle ha = nil;
		Handle hb = nil;
		boolean oka = true;
		boolean okb = true;

		if ((*vala).fldiskval)
			oka = resolvediskvalue (sidea, vala, &ha);

		if ((*valb).fldiskval)
			okb = resolvediskvalue (sideb, valb, &hb);

		if (!oka || !okb) {

			if (ha != nil)
				disposehandle (ha);

			if (hb != nil)
				disposehandle (hb);

			report (state, diffkind_unreadable, path,
			        typename_for (vala), typename_for (valb), -1, -1, -1,
			        "stored value could not be read");
			return;
			}

		/*
		A value may be on disk on one side and resident on the other -- that is
		a residency difference, not a content difference, so compare bytes
		either way.
		*/
		if (ha != nil) {
			bytesa = (const unsigned char *) *ha;
			sizea = gethandlesize (ha);
			}
		else if (!handlebytes (vala, &bytesa, &sizea)) {
			disposehandle (hb);
			report (state, diffkind_unsupported, path,
			        typename_for (vala), typename_for (valb), -1, -1, -1,
			        "mixed residency for a non-handle type");
			return;
			}

		if (hb != nil) {
			bytesb = (const unsigned char *) *hb;
			sizeb = gethandlesize (hb);
			}
		else if (!handlebytes (valb, &bytesb, &sizeb)) {
			disposehandle (ha);
			report (state, diffkind_unsupported, path,
			        typename_for (vala), typename_for (valb), -1, -1, -1,
			        "mixed residency for a non-handle type");
			return;
			}

		if ((sizea != sizeb)
		        || ((sizea > 0) && (memcmp (bytesa, bytesb, (size_t) sizea) != 0)))
			report (state, diffkind_payload, path,
			        typename_for (vala), typename_for (valb), sizea, sizeb,
			        (sizea == sizeb) ? firstdifference (bytesa, bytesb, sizea) : -1,
			        NULL);

		if (ha != nil)
			disposehandle (ha);

		if (hb != nil)
			disposehandle (hb);

		return;
		}

	if (inlineequal (vala, valb, &equal)) {

		if (!equal)
			report (state, diffkind_payload, path,
			        typename_for (vala), typename_for (valb), -1, -1, -1, NULL);

		return;
		}

	if (handlebytes (vala, &bytesa, &sizea) && handlebytes (valb, &bytesb, &sizeb)) {

		if (sizea != sizeb) {
			report (state, diffkind_payload, path,
			        typename_for (vala), typename_for (valb), sizea, sizeb, -1, NULL);
			return;
			}

		if ((sizea > 0) && (memcmp (bytesa, bytesb, (size_t) sizea) != 0))
			report (state, diffkind_payload, path,
			        typename_for (vala), typename_for (valb), sizea, sizeb,
			        firstdifference (bytesa, bytesb, sizea), NULL);

		return;
		}

	if (((*vala).valuetype == listvaluetype) || ((*vala).valuetype == recordvaluetype)) {

		/*
		Lists and records serialize through oppacklist -- the same call the
		ODB's own scalar packer uses (langhash.c:3204-3207). This is what keeps
		#887 out of the picture: nothing here coerces to string.
		*/
		Handle hpackeda = nil;
		Handle hpackedb = nil;

		if (!oppacklist ((hdllistrecord) (*vala).data.listvalue, &hpackeda)
		        || !oppacklist ((hdllistrecord) (*valb).data.listvalue, &hpackedb)) {

			if (hpackeda != nil)
				disposehandle (hpackeda);

			if (hpackedb != nil)
				disposehandle (hpackedb);

			report (state, diffkind_unreadable, path,
			        typename_for (vala), typename_for (valb), -1, -1, -1,
			        "list could not be packed");
			return;
			}

		sizea = gethandlesize (hpackeda);
		sizeb = gethandlesize (hpackedb);

		{
		/*
		A packed list is a list header followed by a packed outline, so it
		carries the same volatile save-metadata fields at a displacement.
		*/
		long basea = listoutlinebase ((const unsigned char *) *hpackeda, sizea);
		long baseb = listoutlinebase ((const unsigned char *) *hpackedb, sizeb);
		long offset;

		if ((basea >= 0) && (basea == baseb))
			offset = outlinecontentdifference ((const unsigned char *) *hpackeda, sizea,
			                                   (const unsigned char *) *hpackedb, sizeb,
			                                   basea);
		else if (sizea != sizeb)
			offset = (sizea < sizeb) ? sizea : sizeb;
		else
			offset = firstdifference ((const unsigned char *) *hpackeda,
			                          (const unsigned char *) *hpackedb, sizea);

		if (offset >= 0)
			report (state, diffkind_payload, path,
			        typename_for (vala), typename_for (valb), sizea, sizeb, offset, NULL);
		}

		disposehandle (hpackeda);
		disposehandle (hpackedb);
		return;
		}

	/*Loud fail: a type we do not know how to compare is a finding, not a skip.*/
	report (state, diffkind_unsupported, path,
	        typename_for (vala), typename_for (valb), -1, -1, -1, NULL);
	} /*comparepayload*/


static void comparevalue (tywalkstate *state, const char *path,
                          tyrootside *sidea, tyrootside *sideb,
                          const tyvaluerecord *vala, const tyvaluerecord *valb,
                          int depth) {

	/*
	oldstringvaluetype (4) and stringvaluetype (12) unpack into the same field
	(langpack.c:654-660), so treat them as the same type rather than emitting a
	phantom TYPE finding on a cross-era comparison.
	*/
	tyvaluetype ta = (*vala).valuetype;
	tyvaluetype tb = (*valb).valuetype;

	if (ta == oldstringvaluetype)
		ta = stringvaluetype;

	if (tb == oldstringvaluetype)
		tb = stringvaluetype;

	if (ta != tb) {
		report (state, diffkind_type, path, typename_for (vala), typename_for (valb),
		        -1, -1, -1, NULL);
		return;
		}

	if (ta == externalvaluetype) {
		compareexternal (state, path, sidea, sideb, vala, valb, depth);
		return;
		}

	comparepayload (state, path, sidea, sideb, vala, valb);
	} /*comparevalue*/


/*
	Compare two raw Pascal names as bytes. Returns <0, 0, >0 like memcmp.
*/
static int comparenames (const unsigned char *bsa, const unsigned char *bsb) {

	size_t lena = (size_t) bsa [0];
	size_t lenb = (size_t) bsb [0];
	size_t shared = (lena < lenb) ? lena : lenb;
	int order = memcmp (bsa + 1, bsb + 1, shared);

	if (order != 0)
		return (order);

	if (lena == lenb)
		return (0);

	return ((lena < lenb) ? -1 : 1);
	} /*comparenames*/


/*
	Exact buffer size for `parent` + '.' + one encoded segment + NUL.

	The buffer GROWS with the path instead of sitting at a fixed cap. A diff
	tool must never fail to NAME a path, and a fixed cap silently converts a
	deep path into an unreportable one -- the failure mode both existing ODB
	walkers have (main.c:2429, langhash.c:297). Worst case per segment is three
	characters per raw byte, when every byte percent-escapes.

	maxdiffpath survives only as a sanity ceiling for absurd input, not as the
	working size.
*/
static size_t pathbufsize (const char *parent, size_t namelen) {

	size_t need = strlen (parent) + 1 /*separator*/ + (namelen * 3) + 1 /*NUL*/;

	if (need < 64)
		need = 64;

	if (need > (size_t) maxdiffpath)
		need = (size_t) maxdiffpath;

	return (need);
	} /*pathbufsize*/


/*
	One table entry, buffered so a table can be sorted by name before matching.
	`name` is a COPY of the node's Pascal key -- gethashkey writes into a
	caller-supplied bigstring, so the bytes must be captured rather than
	pointed at.
*/
typedef struct tynodeentry {

	bigstring name;
	hdlhashnode node;

	} tynodeentry;


static int comparenodeentries (const void *a, const void *b) {

	return (comparenames (((const tynodeentry *) a)->name,
	                      ((const tynodeentry *) b)->name));
	} /*comparenodeentries*/


/*
	Buffer every node of a table and sort by raw name bytes.

	Sorting is what makes matching correct AND output deterministic: hfirstsort
	is insertion order in this build (see walkpair), so neither property comes
	for free. Sorting by the same byte comparison used for matching keeps the
	two consistent.

	Returns false only on allocation failure. An empty table yields ct 0 and a
	NULL array, which the caller handles as "nothing on this side".

	Assumes names within one table are unique, which the ODB enforces (a
	hashtable cannot hold two entries under the same key). If duplicates could
	occur, qsort's unspecified ordering among equal keys would make the pairing
	of A-side to B-side entries arbitrary; the merge below would still pair
	them one-for-one, but which-with-which would not be meaningful.
*/
static boolean collectnodes (hdlhashtable htable, tynodeentry **pentries, long *pct) {

	hdlhashnode nomad;
	long ct = 0;
	long i = 0;
	tynodeentry *entries;

	*pentries = NULL;
	*pct = 0;

	if (htable == nil)
		return (true);

	for (nomad = (**htable).hfirstsort; nomad != nil; nomad = (**nomad).sortedlink)
		ct++;

	if (ct == 0)
		return (true);

	entries = (tynodeentry *) malloc ((size_t) ct * sizeof (tynodeentry));

	if (entries == NULL)
		return (false);

	for (nomad = (**htable).hfirstsort; (nomad != nil) && (i < ct);
	     nomad = (**nomad).sortedlink) {

		gethashkey (nomad, entries [i].name);
		entries [i].node = nomad;
		i++;
		}

	qsort (entries, (size_t) i, sizeof (tynodeentry), comparenodeentries);

	*pentries = entries;
	*pct = i;

	return (true);
	} /*collectnodes*/


static void reportsubtree (tywalkstate *state, tyrootside *side, const char *parentpath,
                           hdlhashnode node, tydiffkind kind, int depth);


/*
	Report an entire present-on-one-side-only subtree, so a missing table is
	not reported as a single line that hides thousands of values.
*/
static void reportonlyside (tywalkstate *state, tyrootside *side, const char *parentpath,
                            const unsigned char *bsname, const tyvaluerecord *val,
                            tydiffkind kind, hdlhashnode node, int depth) {

	/*
	Path buffers are HEAP allocated, not stack. maxdiffpath is deliberately
	generous (a diff tool must never fail to name a path), and both this
	function and walkpair recurse to maxwalkdepth -- an 8 KB stack buffer per
	frame overflows the 8 MB default stack well before that depth. Learned the
	hard way: the first build segfaulted here on manila.root.
	*/
	size_t pathsz = pathbufsize (parentpath, (size_t) bsname [0]);
	char *path = (char *) malloc (pathsz);

	if (path == NULL) {
		report (state, diffkind_unreadable, parentpath, "-", "-", -1, -1, -1,
		        "out of memory building path");
		return;
		}

	snprintf (path, pathsz, "%s", parentpath);

	if (!diff_roots_appendsegment (path, pathsz,
	                               (const char *) &bsname [1], (size_t) bsname [0])) {
		report (state, diffkind_unreadable, parentpath, "-", "-", -1, -1, -1,
		        "path too long to report");
		free (path);
		return;
		}

	report (state, kind, path,
	        (kind == diffkind_only_in_a) ? typename_for (val) : "-",
	        (kind == diffkind_only_in_b) ? typename_for (val) : "-",
	        -1, -1, -1, NULL);

	reportsubtree (state, side, path, node, kind, depth);

	free (path);
	} /*reportonlyside*/


static void reportsubtree (tywalkstate *state, tyrootside *side, const char *parentpath,
                           hdlhashnode node, tydiffkind kind, int depth) {

	tyvaluerecord *val;
	hdlexternalvariable hv;
	hdlhashtable child;
	hdlhashnode nomad;

	if (depth >= maxwalkdepth)
		return;

	val = &(**node).val;

	if ((*val).valuetype != externalvaluetype)
		return;

	hv = (hdlexternalvariable) (*val).data.externalvalue;

	if ((hv == nil) || ((**hv).id != idtableprocessor))
		return;

	if (!(**hv).flinmemory) {

		/*
		Materialize rather than stop. Returning here would silently truncate
		the report: the table itself was named, but none of the values beneath
		it would be -- so a present-on-one-side-only subtree would look far
		smaller than it is. That is the same blind spot the .ut exporter has
		(main.c:2444), and a diff tool cannot afford it.
		*/
		if (!ensure_external_in_memory (&side->context, hv)) {
			report (state, diffkind_unreadable, parentpath, "tabl", "tabl", -1, -1, -1,
			        "subtree could not be loaded to enumerate its contents");
			return;
			}
		}

	child = (hdlhashtable) (**hv).variabledata;

	if (child == nil)
		return;

	for (nomad = (**child).hfirstsort; nomad != nil; nomad = (**nomad).sortedlink) {

		bigstring bsname;

		gethashkey (nomad, bsname);

		reportonlyside (state, side, parentpath, bsname, &(**nomad).val, kind, nomad, depth + 1);
		}
	} /*reportsubtree*/


static void walkpair (tywalkstate *state, const char *path,
                      tyrootside *sidea, tyrootside *sideb,
                      hdlhashtable tablea, hdlhashtable tableb, int depth) {

	if (depth >= maxwalkdepth) {
		report (state, diffkind_unreadable, path, "tabl", "tabl", -1, -1, -1,
		        "maximum walk depth exceeded");
		return;
		}

	if ((tablea == nil) || (tableb == nil)) {
		report (state, diffkind_unreadable, path, "tabl", "tabl", -1, -1, -1,
		        "nil subtable");
		return;
		}

	/*
	NAME-KEYED MATCHING, not a positional merge over hfirstsort.

	hfirstsort is NOT alphabetical in this build. Sorted insertion consults
	langcallbacks.comparenodescallback (langhash.c:1464), and the headless
	startup wires that to cb_noop_compare, which always returns 0
	(langstartup.c:81, :1172) -- so every node appends and the list is in
	INSERTION order.

	An earlier version merged the two lists positionally on that assumption.
	Two roots holding identical values inserted in different orders produced
	four spurious findings for three values, each reported as BOTH only-in-A
	and only-in-B, while their payloads were never compared at all. That is the
	exact hazard: not just noise, but masked payload comparisons.

	So: buffer both sides, sort each by raw name bytes, then merge. Sorting
	makes the output deterministic regardless of how either root happens to
	store its nodes, which is what a drift check needs.
	*/
	{
	tynodeentry *entriesa = NULL;
	tynodeentry *entriesb = NULL;
	long cta = 0;
	long ctb = 0;
	long ia = 0;
	long ib = 0;

	if (!collectnodes (tablea, &entriesa, &cta) || !collectnodes (tableb, &entriesb, &ctb)) {

		free (entriesa);
		free (entriesb);

		report (state, diffkind_unreadable, path, "tabl", "tabl", -1, -1, -1,
		        "out of memory collecting table entries");
		return;
		}

	while ((ia < cta) || (ib < ctb)) {

		int order;

		if (ia >= cta)
			order = 1;
		else if (ib >= ctb)
			order = -1;
		else
			order = comparenames (entriesa [ia].name, entriesb [ib].name);

		if (order < 0) {
			reportonlyside (state, sidea, path, entriesa [ia].name,
			                &(**(entriesa [ia].node)).val, diffkind_only_in_a,
			                entriesa [ia].node, depth);
			ia++;
			continue;
			}

		if (order > 0) {
			reportonlyside (state, sideb, path, entriesb [ib].name,
			                &(**(entriesb [ib].node)).val, diffkind_only_in_b,
			                entriesb [ib].node, depth);
			ib++;
			continue;
			}

		{
		/*Heap, not stack -- see the note in reportonlyside.*/
		size_t childsz = pathbufsize (path, (size_t) entriesa [ia].name [0]);
		char *childpath = (char *) malloc (childsz);

		if (childpath == NULL) {
			free (entriesa);
			free (entriesb);
			report (state, diffkind_unreadable, path, "-", "-", -1, -1, -1,
			        "out of memory building path");
			return;
			}

		snprintf (childpath, childsz, "%s", path);

		if (!diff_roots_appendsegment (childpath, childsz,
		                               (const char *) &entriesa [ia].name [1],
		                               (size_t) entriesa [ia].name [0]))
			report (state, diffkind_unreadable, path, "-", "-", -1, -1, -1,
			        "path too long to report");
		else
			comparevalue (state, childpath, sidea, sideb,
			              &(**(entriesa [ia].node)).val,
			              &(**(entriesb [ib].node)).val, depth);

		free (childpath);
		}

		ia++;
		ib++;
		}

	free (entriesa);
	free (entriesb);
	}
	} /*walkpair*/


static void closeside (tyrootside *side) {

	if (!side->flopen)
		return;

	/*
	Full teardown, in this order:

	  databasedata = side  -- dbclose/dbdispose operate on the global, so it
	                          must name THIS side while two roots are open.
	  dbclose()            -- flushes the header only (db.c:4823); it does NOT
	                          free the record or close the file, so stopping
	                          here leaks both.
	  dbdispose()          -- frees the databaserecord and sets databasedata to
	                          nil (db.c:4530-4542).
	  closefile()          -- releases the file number itself.

	Ordering matters and is not cosmetic: dbzeroreleasestack's comment
	(db.c:4500-4510) records a real bug where a guard restored databasedata and
	dbdispose then freed it, leaving a dangling global that segfaulted at exit.
	So the guard exit must happen AFTER this, never around it -- see
	diff_roots_compare, which calls closeside for both sides before
	odb_guard_exit.
	*/
	databasedata = side->database;

	dbclose ();

	dbdispose (); /*frees the record and nils databasedata*/

	if (side->fnum != 0)
		closefile (side->fnum);

	side->flopen = false;
	side->fnum = 0;
	side->database = nil;
	side->hrootvariable = nil;
	side->roottable = nil;
	} /*closeside*/


static boolean openside (tyrootside *side, const char *path, FILE *err) {

	bigstring bspath;
	tyfilespec fs;
	dbaddress rootaddress = nildbaddress;

	side->path = path;
	side->fnum = 0;
	side->database = nil;
	side->hrootvariable = nil;
	side->roottable = nil;
	side->flopen = false;

	if (!copyctopstring (path, bspath)) {
		fprintf (err, "diff-roots: path too long: %s\n", path);
		return (false);
		}

	if (!pathtofilespec (bspath, &fs)) {
		fprintf (err, "diff-roots: cannot resolve path: %s\n", path);
		return (false);
		}

	/*
	Read-only at both layers. dbwrite refuses writes to a flreadonly database
	(db.c:715-728), and read-only open also skips the auto-migration that a
	read-write open would perform on a v6 file (dbverbs.c:741).
	*/
	if (!openfile (&fs, &side->fnum, true)) {
		fprintf (err, "diff-roots: cannot open: %s\n", path);
		return (false);
		}

	if (!dbopenfile (side->fnum, true)) {
		fprintf (err, "diff-roots: not a readable database: %s\n", path);
		closefile (side->fnum);
		return (false);
		}

	side->flopen = true;
	side->database = databasedata;

	db_context_init (&side->context);
	side->context.database = side->database;
	side->context.mode = db_format_mode_current ();

	if ((**side->database).versionnumber < 7) {
		/*
		v6 stores a Cancoon record in views[0] whose adrroottable points at the
		root table; v7 stores the root table address directly
		(odbengine.c:566-570). Rather than silently misinterpret one as the
		other, refuse.
		*/
		fprintf (err, "diff-roots: %s is not a v7 database (version %d); "
		         "migrate it first\n", path, (int) (**side->database).versionnumber);
		closeside (side);
		return (false);
		}

	dbgetview (cancoonview, &rootaddress);

	if (rootaddress == nildbaddress) {
		fprintf (err, "diff-roots: %s has no root table\n", path);
		closeside (side);
		return (false);
		}

	if (!tableloadsystemtable (rootaddress, &side->hrootvariable, &side->roottable, false)) {
		fprintf (err, "diff-roots: cannot load root table of %s\n", path);
		closeside (side);
		return (false);
		}

	return (true);
	} /*openside*/


boolean diff_roots_compare (const char *patha, const char *pathb,
                            const tydiffoptions *options, long *ctfindings) {

	tydiffoptions defaults;
	tyrootside sidea;
	tyrootside sideb;
	tywalkstate state;
	odb_context_guard guard;
	FILE *err = stderr;
	boolean ok = false;
	char rootpath [maxdiffpath];

	if (ctfindings != NULL)
		*ctfindings = 0;

	/*
	MODE PRECONDITION -- checked FIRST, before any input validation, because it
	is a statement about whether this mode may run at all, not about whether
	its arguments are good.

	ut-sync's import hook rewrites outlines, sets dirty flags, and writes
	.ut-sync-state: all writes, under a mode that promises neither root is
	touched. cli_validate_options rejects the combination on the CLI path, but
	this function is callable directly (the unit tests do exactly that), and
	FRONTIER_UT_SYNC_DIR turns ut-sync on from the environment with no flag on
	the command line at all.

	Checked via the env var rather than by calling back into the CLI layer:
	this module must not depend on main.c, and the env var is precisely the
	source that needs no argument to be present.

	Ordering it first also makes the refusal OBSERVABLE and therefore testable.
	Behind the argument checks, every input a test can supply is rejected for
	some other reason first, so a test could not tell this guard from a
	file-not-found -- verified: with the guard stubbed out, such a test still
	passed. First position plus the distinct message below means a test can
	assert on this specific refusal.
	*/
	{
	const char *utsyncenv = getenv ("FRONTIER_UT_SYNC_DIR");

	if ((utsyncenv != NULL) && (utsyncenv [0] != '\0')) {
		fprintf (err, "%s\n", diff_roots_utsyncrefusal);
		return (false);
		}
	}

	if ((patha == NULL) || (pathb == NULL))
		return (false);

	if (options == NULL) {
		memset (&defaults, 0, sizeof (defaults));
		options = &defaults;
		}

	memset (&state, 0, sizeof (state));
	state.options = options;
	state.out = (options->out != NULL) ? options->out : stdout;
	state.ctfindings = 0;

	if (!db_format_prepare_runtime ()) {
		fprintf (err, "diff-roots: database runtime failed to initialize\n");
		return (false);
		}

	/*
	DEFENSE IN DEPTH, not a load-bearing fix for a demonstrated corruption.

	An earlier version of this comment claimed that opening a second database
	clears the table-structure globals. An audit disproved that: dbopenfile
	touches only databasedata. cleartablestructureglobals is called from
	cancoon.c:601 and the compaction/migration path in db_format.c, neither of
	which this mode enters. The hazard db.h:285-289 documents is real for the
	guest-DB open path; it is simply not reached from here.

	The guard is kept anyway because this mode holds two roots open at once and
	sets databasedata directly, so restoring the caller's context on every exit
	path costs nothing and removes a whole class of future coupling. Keeping it
	for an honest reason is better than keeping it for an invented one.

	Note it does NOT save the db_format mode globals (g_mode_state). That is
	inert here: the walker is v7-only and never switches modes, and closeside's
	dbdispose pops the mode pushed by that side's dbopenfile.
	*/
	odb_guard_enter (&guard);

	memset (&sidea, 0, sizeof (sidea));
	memset (&sideb, 0, sizeof (sideb));

	if (!openside (&sidea, patha, err))
		goto exit;

	if (!openside (&sideb, pathb, err))
		goto exit;

	rootpath [0] = '\0';

	/*
	Value access reads the databasedata global, so each side's walk must run
	with that global pointing at that side. walkpair touches both sides, so it
	sets the global per access via the side's db_context where the API allows
	it, and leaves databasedata parked on side A otherwise.
	*/
	databasedata = sidea.database;

	walkpair (&state, rootpath, &sidea, &sideb, sidea.roottable, sideb.roottable, 0);

	ok = true;

exit:

	closeside (&sideb);
	closeside (&sidea);

	odb_guard_exit (&guard);

	if (ctfindings != NULL)
		*ctfindings = state.ctfindings;

	return (ok);
	} /*diff_roots_compare*/
