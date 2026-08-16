/*
	SPDX-License-Identifier: MIT

	Copyright (c) 2026 Frontier contributors

	Determinism spike for --diff-roots (root-build Step 1).

	Question this answers: is oppackoutline() a usable EQUALITY BASIS for
	script/outline/menubar values? It is only usable if packing the SAME stored
	value across two INDEPENDENT loads of the same file yields identical bytes.
	Packing twice within one load is NOT sufficient evidence -- volatile
	in-memory-only fields (cursor, scroll position, expansion state) would be
	stable across two packs of one resident value while differing across two
	separate loads.

	So: open read-only -> walk -> pack every scpt/optx/mbar -> hash the packed
	bytes -> tear the database all the way down -> reopen from scratch -> repeat
	-> compare. Any per-path delta names a volatile field and disqualifies the
	approach (falling back to a node walk over headstring/headlevel).

	Also asserts the file itself is byte-identical after the walk (md5 is checked
	by the driver script), and that packing does not flip the outline record's
	dirty flags.

	Build: see tests/Makefile target diff_roots_spike.
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
#include "menueditor.h"	/* tymenurecord.menuoutline -- a menubar contains an outline */
#include "tablestructure.h"
#include "db_format.h"
#include "file.h"
#include "strings.h"
#include "memory.h"
#include "ut_sync.h"	/* ut_pct_encode_segment -- the same lossless name codec
			   the walker will use for path reporting */

#define maxpathdepth 64
#define maxrecords 20000

typedef struct tyspikerecord {
	char path [512];
	unsigned long hash;
	long packedsize;
	tyexternalid extid;
	} tyspikerecord;

static tyspikerecord records [maxrecords];
static long ctrecords = 0;
static long ctpackfailures = 0;
static long ctdirtyflips = 0;

/*
	Read context for the root currently being walked. Set once per load, passed
	explicitly to ensure_external_in_memory so materialization never depends on
	the databasedata global -- the same discipline the walker proper needs when
	it holds two roots open at once.
*/
static db_context g_walkcontext;

/*
	FNV-1a. We only need to detect difference, not resist collision -- and a
	hash keeps the spike's memory flat across ~3,500 scripts.
*/
static unsigned long hashbytes (const unsigned char *p, long ct) {

	unsigned long h = 2166136261UL;
	long i;

	for (i = 0; i < ct; i++) {
		h ^= (unsigned long) p [i];
		h *= 16777619UL;
		}

	return (h);
	} /*hashbytes*/


static void appendsegment (char *path, size_t pathsz, const unsigned char *bsname) {

	/*
	Percent-encode via the same codec the walker will use, so the spike's paths
	are directly comparable to the walker's output. Names legitimately contain
	tab and raw CR/LF (verified: siblings under ...aim.code.util.repl are named
	with the single bytes 0x0A/0x0B/0x0C/0x0D), so raw concatenation would be
	ambiguous.
	*/

	char encoded [766]; /*255 * 3 + 1 -- max encoded Pascal name*/
	size_t len;

	if (!ut_pct_encode_segment ((const char *) &bsname [1], (size_t) bsname [0],
	                            encoded, sizeof (encoded)))
		strcpy (encoded, "%3CENCODE-OVERFLOW%3E");

	len = strlen (path);

	if (len > 0)
		snprintf (path + len, pathsz - len, ".%s", encoded);
	else
		snprintf (path, pathsz, "%s", encoded);
	} /*appendsegment*/


static void packone (hdlexternalvariable hv, tyexternalid extid, const char *path) {

	hdloutlinerecord ho;
	Handle hpacked = nil;
	boolean fldirtybefore, fldirtyviewbefore;

	if (!(**hv).flinmemory) /*handled by the caller; nothing resident to pack*/
		return;

	if (extid == idmenuprocessor) {
		/*
		MENUBARS CANNOT BE PACKED IN A HEADLESS BUILD. Two separate reasons,
		both verified:

		1. A menubar is not an outline -- variabledata would be an hdlmenurecord
		   (menueditor.h:142), whose content outline is (**hm).menuoutline
		   (menueditor.h:144).

		2. More fundamentally: the headless menu stub
		   menuverbinmemory_context (frontier-cli/stubs/headless_menu_stubs.c:618)
		   sets flinmemory = true WITHOUT LOADING ANYTHING -- its own comment
		   says "Menu externals are not materialized during headless migration.
		   Mark as in memory to skip actual loading, but keep the address for
		   packing." So variabledata is still a raw dbaddress, and dereferencing
		   it as any record segfaults. flinmemory is a LIE for mbar values in
		   this build.

		This is why mainResponder.root's `menu` crashed the walk, and it is
		almost certainly why the census reports size -1 for that same value.

		The walker proper must therefore compare mbar values at the STORED
		level (raw disk bytes at the external's address) rather than by
		materializing them -- reported honestly, never silently skipped.
		*/
		printf ("MBAR-NOT-COMPARABLE-HEADLESS\t%s\n", path);
		return;
		}

	ho = (hdloutlinerecord) (**hv).variabledata;

	if (ho == nil) {
		printf ("NOOUTLINE\t%s\textid=%d\n", path, (int) extid);
		return;
		}

	fldirtybefore = (**ho).fldirty;
	fldirtyviewbefore = (**ho).fldirtyview;

	/*
	oppackoutline APPENDS when handed a non-nil handle (see oppack's header
	comment), so hpacked must start nil.
	*/
	if (!oppackoutline (ho, &hpacked)) {
		ctpackfailures++;
		printf ("PACKFAIL\t%s\n", path);
		return;
		}

	if (((**ho).fldirty != fldirtybefore) || ((**ho).fldirtyview != fldirtyviewbefore)) {
		ctdirtyflips++;
		printf ("DIRTYFLIP\t%s\tdirty %d->%d view %d->%d\n", path,
		        (int) fldirtybefore, (int) (**ho).fldirty,
		        (int) fldirtyviewbefore, (int) (**ho).fldirtyview);
		}

	if (ctrecords < maxrecords) {
		tyspikerecord *r = &records [ctrecords++];

		snprintf (r->path, sizeof (r->path), "%s", path);
		r->packedsize = gethandlesize (hpacked);
		r->hash = hashbytes ((const unsigned char *) *hpacked, r->packedsize);
		r->extid = extid;
		}

	disposehandle (hpacked);
	} /*packone*/


static void walktable (hdlhashtable htable, char *path, size_t pathsz, int depth) {

	hdlhashnode nomad;

	if ((htable == nil) || (depth > maxpathdepth))
		return;

	for (nomad = (**htable).hfirstsort; nomad != nil; nomad = (**nomad).sortedlink) {

		tyvaluerecord *val = &(**nomad).val;
		bigstring bsname;
		char childpath [512];
		hdlexternalvariable hv;
		tyexternalid extid;

		gethashkey (nomad, bsname);

		snprintf (childpath, sizeof (childpath), "%s", path);
		appendsegment (childpath, sizeof (childpath), bsname);

		if (getenv ("SPIKE_TRACE") != NULL) {
			fprintf (stderr, "TRACE %s type=%d\n", childpath, (int) (*val).valuetype);
			fflush (stderr);
			}

		if ((*val).valuetype != externalvaluetype)
			continue;

		hv = (hdlexternalvariable) (*val).data.externalvalue;

		if (hv == nil)
			continue;

		extid = (**hv).id;

		if (extid == idtableprocessor) {

			/*
			Subtables are NOT resident after tableloadsystemtable -- they fault
			in on demand. The .ut export walk (main.c:2444) simply skips
			non-resident subtables, which for a diff tool would silently omit
			whole subtrees. So materialize read-only instead. tableverbinmemory
			only flips flinmemory (a residency flag); it sets no dirty flag and
			writes nothing, and the database is open flreadonly regardless.
			*/
			if (!(**hv).flinmemory) {

				if (!ensure_external_in_memory (&g_walkcontext, hv)) {
					printf ("LOADFAIL\t%s\ttable\n", childpath);
					continue;
					}
				}

			walktable ((hdlhashtable) (**hv).variabledata, childpath, pathsz, depth + 1);

			continue;
			}

		/*
		The spike measures exactly the three types the walker intends to compare
		via packed bytes. wptext (idwordprocessor) is deliberately NOT touched:
		force-materializing those exhausts memory on a full-root walk
		(db_format.c:1694).
		*/
		if (extid == idmenuprocessor) {
			/*
			Do NOT materialize: the headless stub fakes flinmemory without
			loading (see packone). Report and move on.
			*/
			printf ("MBAR-NOT-COMPARABLE-HEADLESS\t%s\n", childpath);
			continue;
			}

		if ((extid == idscriptprocessor) || (extid == idoutlineprocessor)) {

			if (!(**hv).flinmemory) {

				if (getenv ("SPIKE_TRACE") != NULL) {
					fprintf (stderr, "  materialize extid=%d %s\n", (int) extid, childpath);
					fflush (stderr);
					}

				if (!ensure_external_in_memory (&g_walkcontext, hv)) {
					printf ("LOADFAIL\t%s\textid=%d\n", childpath, (int) extid);
					continue;
					}
				}

			if (getenv ("SPIKE_TRACE") != NULL) {
				fprintf (stderr, "  pack extid=%d %s\n", (int) extid, childpath);
				fflush (stderr);
				}

			packone (hv, extid, childpath);
			}
		}
	} /*walktable*/


static boolean loadandpack (const char *cpath) {

	bigstring bspath;
	tyfilespec fs;
	hdlfilenum fnum = 0;
	dbaddress rootaddress = nildbaddress;
	Handle hrootvariable = nil;
	hdlhashtable hroot = nil;
	char path [512];

	if (!copyctopstring (cpath, bspath)) {
		fprintf (stderr, "spike: path too long: %s\n", cpath);
		return (false);
		}

	if (!pathtofilespec (bspath, &fs)) {
		fprintf (stderr, "spike: pathtofilespec failed for %s\n", cpath);
		return (false);
		}

	/*Both opens read-only: the file must be byte-identical afterwards.*/
	if (!openfile (&fs, &fnum, true)) {
		fprintf (stderr, "spike: openfile failed for %s\n", cpath);
		return (false);
		}

	if (!dbopenfile (fnum, true)) {
		fprintf (stderr, "spike: dbopenfile failed for %s\n", cpath);
		closefile (fnum);
		return (false);
		}

	db_context_init (&g_walkcontext);
	g_walkcontext.database = databasedata;
	g_walkcontext.mode = db_format_mode_current ();

	dbgetview (cancoonview, &rootaddress);

	if (rootaddress == nildbaddress) {
		fprintf (stderr, "spike: dbgetview returned nil root address\n");
		dbclose ();
		return (false);
		}

	/*
	NO Cancoon indirection for v7.

	odbengine.c:566-570 states the rule: for v6 databases views[0] points at a
	Cancoon record that in turn holds adrroottable, but for v7 databases
	views[0] IS the root table address (or nildbaddress when empty). All three
	shipped roots are v7 (header starts 00 07), so the address dbgetview just
	returned is already what tableloadsystemtable wants.

	Reading it as a Cancoon record instead produced 0x4fe0000 -- a non-block
	address that dbnormalizeaddress correctly rejected.

	A v6 input would need the Cancoon path; this spike does not accept one, and
	the walker proper will reject non-v7 roots explicitly rather than
	misinterpret them.
	*/

	if (!tableloadsystemtable (rootaddress, &hrootvariable, &hroot, false)) {
		fprintf (stderr, "spike: tableloadsystemtable failed\n");
		dbclose ();
		return (false);
		}

	path [0] = '\0';

	{
	long ctnodes = 0;
	hdlhashnode n;

	for (n = (**hroot).hfirstsort; n != nil; n = (**n).sortedlink)
		ctnodes++;

	fprintf (stderr, "spike: root table adr=0x%llx has %ld top-level nodes\n",
	         (unsigned long long) rootaddress, ctnodes);
	}

	walktable (hroot, path, sizeof (path), 0);

	dbclose ();

	return (true);
	} /*loadandpack*/


int main (int argc, char **argv) {

	const char *dbpath;
	long ctfirst;
	long i;
	long ctmismatch = 0;
	static tyspikerecord firstpass [maxrecords];

	if (argc < 2) {
		fprintf (stderr, "usage: diff_roots_spike <path-to.root>\n");
		return (2);
		}

	dbpath = argv [1];

	if (!db_format_prepare_runtime ()) {
		fprintf (stderr, "spike: db_format_prepare_runtime failed\n");
		return (2);
		}

	/*PASS 1*/
	ctrecords = 0;

	if (!loadandpack (dbpath))
		return (2);

	ctfirst = ctrecords;
	memcpy (firstpass, records, (size_t) ctfirst * sizeof (tyspikerecord));

	printf ("PASS1\tvalues=%ld\tpackfailures=%ld\tdirtyflips=%ld\n",
	        ctfirst, ctpackfailures, ctdirtyflips);

	/*
	PASS 2 -- a completely independent load. dbclose() ran at the end of pass 1,
	so nothing from pass 1's outline records survives into this pass. This is
	what makes the comparison meaningful.
	*/
	ctrecords = 0;
	ctpackfailures = 0;

	if (!loadandpack (dbpath))
		return (2);

	printf ("PASS2\tvalues=%ld\tpackfailures=%ld\tdirtyflips=%ld\n",
	        ctrecords, ctpackfailures, ctdirtyflips);

	if (ctrecords != ctfirst) {
		printf ("VERDICT\tRED\tvalue count differs between loads: %ld vs %ld\n",
		        ctfirst, ctrecords);
		return (1);
		}

	for (i = 0; i < ctfirst; i++) {

		if (strcmp (firstpass [i].path, records [i].path) != 0) {
			printf ("ORDERDIFF\t%ld\t%s\t%s\n", i, firstpass [i].path, records [i].path);
			ctmismatch++;
			continue;
			}

		if ((firstpass [i].hash != records [i].hash)
		        || (firstpass [i].packedsize != records [i].packedsize)) {
			printf ("BYTEDIFF\t%s\tsize %ld vs %ld\thash %lu vs %lu\n",
			        firstpass [i].path,
			        firstpass [i].packedsize, records [i].packedsize,
			        firstpass [i].hash, records [i].hash);
			ctmismatch++;
			}
		}

	printf ("COMPARED\t%ld\tmismatches=%ld\n", ctfirst, ctmismatch);

	if ((ctmismatch == 0) && (ctdirtyflips == 0)) {
		printf ("VERDICT\tGREEN\toppackoutline is stable across independent loads\n");
		return (0);
		}

	printf ("VERDICT\tRED\tmismatches=%ld dirtyflips=%ld\n", ctmismatch, ctdirtyflips);

	return (1);
	} /*main*/
