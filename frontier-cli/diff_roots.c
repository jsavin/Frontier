/*
	SPDX-License-Identifier: MIT

	Copyright (c) 2026 Frontier contributors

	diff_roots.c -- recursive value-level equality walker for two ODB roots.

	See diff_roots.h for the design commitments. Implementation notes that are
	not obvious from the header:

	- Traversal is hfirstsort/sortedlink, which is the ODB's own stored
	  alphabetical order (lang.h:436,474). Walking two roots in that order lets
	  the comparison be a single sorted merge with no sorting pass, no
	  allocation, and deterministic output.

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

	fprintf (state->out, "%-16s %s", diff_roots_kindname (kind), path);

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
	Read the raw stored block at `adr` from one side, without materializing the
	value it belongs to. Used for the types that must not be brought into memory
	(see compareexternal). The caller disposes the handle.

	A nil address is a legitimate "nothing stored" state and yields an empty
	handle so two nil-address values compare equal rather than both failing.
*/
static boolean readstoredbytes (tyrootside *side, dbaddress adr, Handle *h) {

	hdldatabaserecord saved = databasedata;
	boolean fl;

	*h = nil;

	if (adr == nildbaddress)
		return (newemptyhandle (h));

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
			*pequal = ((*a).data.singlevalue == (*b).data.singlevalue);
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
	Volatile regions of the packed-outline header (oppack_v7.c:115-152), given
	as [offset, offset+length). These carry SAVE METADATA, not content, and they
	must be excluded from an equality basis or every value in a re-saved root
	reports as different.

	Verified empirically: comparing a root against a copy that had been opened
	and saved once produced 3,527 spurious payload findings, 3,495 of them
	differing first at offset 37 (inside timelastsave) and 27 at offset 49
	(inside outlinesignature).

	  timecreated       offset 24, 8 bytes   -- creation stamp, per-value
	  timelastsave      offset 32, 8 bytes   -- rewritten on every save
	  ctsaves           offset 40, 4 bytes   -- INCREMENTED by oppack itself
	                                            (oppack_v7.c:622), so it differs
	                                            even between two packs
	  outlinesignature  offset 48, 4 bytes   -- caller-defined cookie

	timecreated is excluded too: it is per-VALUE provenance metadata that the
	root-build plan preserves in the manifest (decision 9.4), so it is exactly
	the kind of thing that legitimately differs between a built root and its
	ancestor without the CONTENT differing.

	NOTE: this makes --diff-roots a CONTENT comparison. A dedicated metadata
	comparison is a separate tool if one is ever wanted.
*/
typedef struct tyskipregion {
	long offset;
	long length;
	} tyskipregion;

static const tyskipregion outlineheaderskips [] = {
	{24, 8},	/*timecreated*/
	{32, 8},	/*timelastsave*/
	{40, 4},	/*ctsaves*/
	{48, 4}		/*outlinesignature*/
	};

#define ctoutlineheaderskips ((long) (sizeof (outlineheaderskips) / sizeof (outlineheaderskips [0])))

/*Total packed-outline header size (_Static_assert at oppack_v7.c:154).*/
#define outlineheaderbytes 1068


static boolean inskipregion (long offset) {

	long i;

	for (i = 0; i < ctoutlineheaderskips; i++) {

		const tyskipregion *r = &outlineheaderskips [i];

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

	if (cta != ctb)
		return ((cta < ctb) ? cta : ctb); /*length difference is a content difference*/

	for (i = 0; i < cta; i++) {

		long relative;

		if (a [i] == b [i])
			continue;

		relative = i - outlinebase;

		if ((relative >= 0) && (relative < outlineheaderbytes) && inskipregion (relative))
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
			boolean oka;
			boolean okb;

			oka = readstoredbytes (sidea, (**hva).oldaddress, &ha);
			okb = readstoredbytes (sideb, (**hvb).oldaddress, &hb);

			if (!oka || !okb) {

				if (ha != nil)
					disposehandle (ha);

				if (hb != nil)
					disposehandle (hb);

				/*
				Could not read one or both stored blocks. Report rather than
				assume equality -- an unreadable value is a finding.
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
			report (state, diffkind_unsupported, path,
			        typename_for (vala), typename_for (valb), -1, -1, -1,
			        "unhandled external kind");
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


static void reportsubtree (tywalkstate *state, const char *parentpath,
                           hdlhashnode node, tydiffkind kind, int depth);


/*
	Report an entire present-on-one-side-only subtree, so a missing table is
	not reported as a single line that hides thousands of values.
*/
static void reportonlyside (tywalkstate *state, const char *parentpath,
                            const unsigned char *bsname, const tyvaluerecord *val,
                            tydiffkind kind, hdlhashnode node, int depth) {

	/*
	Path buffers are HEAP allocated, not stack. maxdiffpath is deliberately
	generous (a diff tool must never fail to name a path), and both this
	function and walkpair recurse to maxwalkdepth -- an 8 KB stack buffer per
	frame overflows the 8 MB default stack well before that depth. Learned the
	hard way: the first build segfaulted here on manila.root.
	*/
	char *path = (char *) malloc (maxdiffpath);

	if (path == NULL) {
		report (state, diffkind_unreadable, parentpath, "-", "-", -1, -1, -1,
		        "out of memory building path");
		return;
		}

	snprintf (path, maxdiffpath, "%s", parentpath);

	if (!diff_roots_appendsegment (path, maxdiffpath,
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

	reportsubtree (state, path, node, kind, depth);

	free (path);
	} /*reportonlyside*/


static void reportsubtree (tywalkstate *state, const char *parentpath,
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

	if (!(**hv).flinmemory)
		return; /*not loaded on this side; the table itself was already reported*/

	child = (hdlhashtable) (**hv).variabledata;

	if (child == nil)
		return;

	for (nomad = (**child).hfirstsort; nomad != nil; nomad = (**nomad).sortedlink) {

		bigstring bsname;

		gethashkey (nomad, bsname);

		reportonlyside (state, parentpath, bsname, &(**nomad).val, kind, nomad, depth + 1);
		}
	} /*reportsubtree*/


static void walkpair (tywalkstate *state, const char *path,
                      tyrootside *sidea, tyrootside *sideb,
                      hdlhashtable tablea, hdlhashtable tableb, int depth) {

	hdlhashnode nodea;
	hdlhashnode nodeb;

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

	nodea = (**tablea).hfirstsort;
	nodeb = (**tableb).hfirstsort;

	/*
	Sorted merge. Both sides are in the ODB's own stored order, so a single
	lockstep pass yields every finding in deterministic order with no sorting
	and no allocation.
	*/
	while ((nodea != nil) || (nodeb != nil)) {

		bigstring bsa;
		bigstring bsb;
		int order;

		if (nodea == nil)
			order = 1;
		else if (nodeb == nil)
			order = -1;
		else {
			gethashkey (nodea, bsa);
			gethashkey (nodeb, bsb);
			order = comparenames (bsa, bsb);
			}

		if (order < 0) {
			gethashkey (nodea, bsa);
			reportonlyside (state, path, bsa, &(**nodea).val, diffkind_only_in_a,
			                nodea, depth);
			nodea = (**nodea).sortedlink;
			continue;
			}

		if (order > 0) {
			gethashkey (nodeb, bsb);
			reportonlyside (state, path, bsb, &(**nodeb).val, diffkind_only_in_b,
			                nodeb, depth);
			nodeb = (**nodeb).sortedlink;
			continue;
			}

		{
		/*Heap, not stack -- see the note in reportonlyside.*/
		char *childpath = (char *) malloc (maxdiffpath);

		if (childpath == NULL) {
			report (state, diffkind_unreadable, path, "-", "-", -1, -1, -1,
			        "out of memory building path");
			return;
			}

		gethashkey (nodea, bsa);

		snprintf (childpath, maxdiffpath, "%s", path);

		if (!diff_roots_appendsegment (childpath, maxdiffpath,
		                               (const char *) &bsa [1], (size_t) bsa [0]))
			report (state, diffkind_unreadable, path, "-", "-", -1, -1, -1,
			        "path too long to report");
		else
			comparevalue (state, childpath, sidea, sideb,
			              &(**nodea).val, &(**nodeb).val, depth);

		free (childpath);
		}

		nodea = (**nodea).sortedlink;
		nodeb = (**nodeb).sortedlink;
		}
	} /*walkpair*/


static void closeside (tyrootside *side) {

	if (!side->flopen)
		return;

	/*
	Point the database globals back at this side before closing, so dbclose
	tears down the right database when two are open.
	*/
	databasedata = side->database;

	dbclose ();

	side->flopen = false;
	side->database = nil;
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
	Opening a second database clears the table-structure globals, which would
	destroy the first root's state. The guard saves and restores all of them --
	this is the documented hazard at db.h:285-289.
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
