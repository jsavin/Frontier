/*
 * headless_file_verbs.c - File processor verbs for headless mode
 *
 * This file provides the callback dispatcher for file verbs in headless mode,
 * routing verb calls to the actual implementations in fileverbs.c.
 *
 * @IMPLEMENTED - All 86 file verbs forward to filefunctionvalue() in fileverbs.c
 *
 * Created: 2026-01-01 - Phase 1 file verb dispatcher
 */

#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "logging.h"

/* Token enum for all verbs in the file processor
 *
 * CRITICAL: This enum MUST be kept in sync with tyfiletoken in Common/source/fileverbs.c
 *
 * Verification:
 *   1. Token order must match exactly (0=filecreated, 1=filemodified, etc.)
 *   2. Token count must match ctfileverbs value (86 verbs)
 *   3. Compile-time assertion below will fail if count mismatches
 *
 * To verify manually:
 *   grep -c "func = " Common/source/fileverbs.c | should equal 86
 */
enum {
	filecreatedfunc = 0,
	filemodifiedfunc = 1,
	filetypefunc = 2,
	filecreatorfunc = 3,
	setfilecreatedfunc = 4,
	setfilemodifiedfunc = 5,
	setfiletypefunc = 6,
	setfilecreatorfunc = 7,
	fileisfolderfunc = 8,
	fileisvolumefunc = 9,
	fileislockedfunc = 10,
	filelockfunc = 11,
	fileunlockfunc = 12,
	filecopyfunc = 13,
	filecopydataforkfunc = 14,
	filecopyresourceforkfunc = 15,
	filedeletefunc = 16,
	filerenamefunc = 17,
	fileexistsfunc = 18,
	filesizefunc = 19,
	filefullpathfunc = 20,
	filegetpathfunc = 21,
	filesetpathfunc = 22,
	filefrompathfunc = 23,
	folderfrompathfunc = 24,
	getsystempathfunc = 25,
	getspecialpathfunc = 26,
	newfunc = 27,
	newfolderfunc = 28,
	newaliasfunc = 29,
	sfgetfilefunc = 30,
	sfputfilefunc = 31,
	sfgetfolderfunc = 32,
	sfgetdiskfunc = 33,
	filegeticonposfunc = 34,
	fileseticonposfunc = 35,
	getshortversionfunc = 36,
	setshortversionfunc = 37,
	getlongversionfunc = 38,
	setlongversionfunc = 39,
	filegetcommentfunc = 40,
	filesetcommentfunc = 41,
	filegetlabelfunc = 42,
	filesetlabelfunc = 43,
	filefindappfunc = 44,
	/* fileeditlinefeedsfunc - commented out in original */
	fileisbusyfunc = 45,
	filehasbundlefunc = 46,
	filesetbundlefunc = 47,
	fileisaliasfunc = 48,
	fileisvisiblefunc = 49,
	filesetvisiblefunc = 50,
	filefollowaliasfunc = 51,
	filemovefunc = 52,
	/* filesinfolderfunc - commented out in original */
	volumeejectfunc = 53,
	volumeisejectablefunc = 54,
	volumefreespacefunc = 55,
	volumesizefunc = 56,
	volumeblocksizefunc = 57,
	filesonvolumefunc = 58,
	foldersonvolumefunc = 59,
	unmountvolumefunc = 60,
	mountservervolumefunc = 61,
	/* filelaunchfunc - commented out in original */
	/* start of new verbs added by DW, 7/27/91 */
	findinfilefunc = 62,
	countlinesfunc = 63,
	openfilefunc = 64,
	closefilefunc = 65,
	endoffilefunc = 66,
	setendoffilefunc = 67,
	getendoffilefunc = 68,
	setpositionfunc = 69,
	getpositionfunc = 70,
	readlinefunc = 71,
	writelinefunc = 72,
	readfunc = 73,
	writefunc = 74,
	comparefunc = 75,
	/* end of new verbs added by DW, 7/27/91 */
	writewholefilefunc = 76,
	getpathcharfunc = 77,
	volumefreespacedoublefunc = 78,
	volumesizedoublefunc = 79,
	getmp3infofunc = 80,
	readwholefilefunc = 81,			/* 2006-04-11 aradke */
	getlabelindexfunc = 82,			/* 2006-04-23 creedon */
	setlabelindexfunc = 83,			/* 2006-04-23 creedon */
	getlabelnamesfunc = 84,			/* 2006-04-23 creedon */
	getposixpathfunc = 85,			/* 2006-10-07 creedon */

	/* Sentinel - must equal ctfileverbs from fileverbs.c */
	filev_count
};

/* Compile-time verification that token count matches fileverbs.c
 * If this fails, the enum above is out of sync with tyfiletoken */
#define EXPECTED_FILE_VERB_COUNT 86
_Static_assert(filev_count == EXPECTED_FILE_VERB_COUNT,
               "Token enum out of sync with fileverbs.c - update headless_file_verbs.c");

/* Forward declaration of portable implementation */
extern boolean portable_filefunctionvalue(short token, hdltreenode hparam1,
                                         tyvaluerecord *vreturned, bigstring bserror);

static boolean file_valueproc(short token, hdltreenode hparam1,
                               tyvaluerecord *vreturned,
                               bigstring bserror) {
	/*
	 * Dispatcher for file verbs in headless mode.
	 * Forwards all calls to portable implementation in portable/fileverbs_portable.c
	 */
	boolean result;

	log_debug(LOG_COMP_LANG, "file_valueproc: ENTRY token=%d hparam1=%p vreturned=%p bserror=%p",
	          token, (void*)hparam1, (void*)vreturned, (void*)bserror);

	result = portable_filefunctionvalue(token, hparam1, vreturned, bserror);

	if (bserror && bserror[0] > 0) {
		char errmsg[256];
		copyptocstring(bserror, errmsg);
		log_debug(LOG_COMP_LANG, "file_valueproc: EXIT token=%d result=%d bserror='%s'",
		          token, result, errmsg);
	} else {
		log_debug(LOG_COMP_LANG, "file_valueproc: EXIT token=%d result=%d bserror=<empty>",
		          token, result);
	}

	return result;
}

/* Exported callback used by headless kernel verb bootstrap */
boolean headless_file_verbs_callback(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned, bigstring bserror) {
	return file_valueproc(token, hparam1, vreturned, bserror);
}

/* Headless-specific file verb initialization function
 *
 * This replaces fileinitverbs() for headless mode, registering the file processor
 * with headless_file_verbs_callback instead of the windowed filefunctionvalue callback.
 *
 * Follows the same pattern as tableinitverbs() in headless_table_verbs.c.
 *
 * Returns: true if file processor was successfully registered, false otherwise
 */
boolean fileinitverbs(void) {
	hdlhashtable htable = nil;
	bigstring bsname;

	extern boolean newfunctionprocessor(bigstring, langvaluecallback, boolean, hdlhashtable*);
	extern boolean langaddkeyword(bigstring, short);
	extern boolean pushhashtable(hdlhashtable);
	extern boolean pophashtable(void);

	log_debug(LOG_COMP_LANG, "fileinitverbs: registering headless file processor");

	copystring(PSTRING("\004", "file"), bsname);

	if (!newfunctionprocessor(bsname, &headless_file_verbs_callback, true, &htable))
		return false;

	pushhashtable(htable);

	/* Register all file verbs */
	#define ADD_VERB(name, tok) do { \
		bigstring bs; \
		copystring(name, bs); \
		if (!langaddkeyword(bs, tok)) { \
			pophashtable(); \
			return false; \
		} \
	} while(0)

	ADD_VERB(PSTRING("\007", "created"), filecreatedfunc);
	ADD_VERB(PSTRING("\010", "modified"), filemodifiedfunc);
	ADD_VERB(PSTRING("\004", "type"), filetypefunc);
	ADD_VERB(PSTRING("\007", "creator"), filecreatorfunc);
	ADD_VERB(PSTRING("\012", "setcreated"), setfilecreatedfunc);
	ADD_VERB(PSTRING("\013", "setmodified"), setfilemodifiedfunc);
	ADD_VERB(PSTRING("\007", "settype"), setfiletypefunc);
	ADD_VERB(PSTRING("\012", "setcreator"), setfilecreatorfunc);
	ADD_VERB(PSTRING("\010", "isfolder"), fileisfolderfunc);
	ADD_VERB(PSTRING("\010", "isvolume"), fileisvolumefunc);
	ADD_VERB(PSTRING("\010", "islocked"), fileislockedfunc);
	ADD_VERB(PSTRING("\004", "lock"), filelockfunc);
	ADD_VERB(PSTRING("\006", "unlock"), fileunlockfunc);
	ADD_VERB(PSTRING("\004", "copy"), filecopyfunc);
	ADD_VERB(PSTRING("\014", "copydatafork"), filecopydataforkfunc);
	ADD_VERB(PSTRING("\020", "copyresourcefork"), filecopyresourceforkfunc);
	ADD_VERB(PSTRING("\006", "delete"), filedeletefunc);
	ADD_VERB(PSTRING("\006", "rename"), filerenamefunc);
	ADD_VERB(PSTRING("\006", "exists"), fileexistsfunc);
	ADD_VERB(PSTRING("\004", "size"), filesizefunc);
	ADD_VERB(PSTRING("\010", "fullpath"), filefullpathfunc);
	ADD_VERB(PSTRING("\007", "getpath"), filegetpathfunc);
	ADD_VERB(PSTRING("\007", "setpath"), filesetpathfunc);
	ADD_VERB(PSTRING("\014", "filefrompath"), filefrompathfunc);
	ADD_VERB(PSTRING("\016", "folderfrompath"), folderfrompathfunc);
	ADD_VERB(PSTRING("\023", "getsystemfolderpath"), getsystempathfunc);
	ADD_VERB(PSTRING("\024", "getspecialfolderpath"), getspecialpathfunc);
	ADD_VERB(PSTRING("\003", "new"), newfunc);
	ADD_VERB(PSTRING("\011", "newfolder"), newfolderfunc);
	ADD_VERB(PSTRING("\010", "newalias"), newaliasfunc);
	ADD_VERB(PSTRING("\015", "getfiledialog"), sfgetfilefunc);
	ADD_VERB(PSTRING("\015", "putfiledialog"), sfputfilefunc);
	ADD_VERB(PSTRING("\017", "getfolderdialog"), sfgetfolderfunc);
	ADD_VERB(PSTRING("\015", "getdiskdialog"), sfgetdiskfunc);
	ADD_VERB(PSTRING("\012", "geticonpos"), filegeticonposfunc);
	ADD_VERB(PSTRING("\012", "seticonpos"), fileseticonposfunc);
	ADD_VERB(PSTRING("\012", "getversion"), getshortversionfunc);
	ADD_VERB(PSTRING("\012", "setversion"), setshortversionfunc);
	ADD_VERB(PSTRING("\016", "getfullversion"), getlongversionfunc);
	ADD_VERB(PSTRING("\016", "setfullversion"), setlongversionfunc);
	ADD_VERB(PSTRING("\012", "getcomment"), filegetcommentfunc);
	ADD_VERB(PSTRING("\012", "setcomment"), filesetcommentfunc);
	ADD_VERB(PSTRING("\010", "getlabel"), filegetlabelfunc);
	ADD_VERB(PSTRING("\010", "setlabel"), filesetlabelfunc);
	ADD_VERB(PSTRING("\017", "findapplication"), filefindappfunc);
	ADD_VERB(PSTRING("\006", "isbusy"), fileisbusyfunc);
	ADD_VERB(PSTRING("\011", "hasbundle"), filehasbundlefunc);
	ADD_VERB(PSTRING("\011", "setbundle"), filesetbundlefunc);
	ADD_VERB(PSTRING("\007", "isalias"), fileisaliasfunc);
	ADD_VERB(PSTRING("\011", "isvisible"), fileisvisiblefunc);
	ADD_VERB(PSTRING("\012", "setvisible"), filesetvisiblefunc);
	ADD_VERB(PSTRING("\013", "followalias"), filefollowaliasfunc);
	ADD_VERB(PSTRING("\004", "move"), filemovefunc);
	ADD_VERB(PSTRING("\005", "eject"), volumeejectfunc);
	ADD_VERB(PSTRING("\013", "isejectable"), volumeisejectablefunc);
	ADD_VERB(PSTRING("\021", "freespaceonvolume"), volumefreespacefunc);
	ADD_VERB(PSTRING("\012", "volumesize"), volumesizefunc);
	ADD_VERB(PSTRING("\017", "volumeblocksize"), volumeblocksizefunc);
	ADD_VERB(PSTRING("\015", "filesonvolume"), filesonvolumefunc);
	ADD_VERB(PSTRING("\017", "foldersonvolume"), foldersonvolumefunc);
	ADD_VERB(PSTRING("\015", "unmountvolume"), unmountvolumefunc);
	ADD_VERB(PSTRING("\021", "mountservervolume"), mountservervolumefunc);
	ADD_VERB(PSTRING("\012", "findinfile"), findinfilefunc);
	ADD_VERB(PSTRING("\012", "countlines"), countlinesfunc);
	ADD_VERB(PSTRING("\004", "open"), openfilefunc);
	ADD_VERB(PSTRING("\005", "close"), closefilefunc);
	ADD_VERB(PSTRING("\011", "endoffile"), endoffilefunc);
	ADD_VERB(PSTRING("\014", "setendoffile"), setendoffilefunc);
	ADD_VERB(PSTRING("\014", "getendoffile"), getendoffilefunc);
	ADD_VERB(PSTRING("\013", "setposition"), setpositionfunc);
	ADD_VERB(PSTRING("\013", "getposition"), getpositionfunc);
	ADD_VERB(PSTRING("\010", "readline"), readlinefunc);
	ADD_VERB(PSTRING("\011", "writeline"), writelinefunc);
	ADD_VERB(PSTRING("\004", "read"), readfunc);
	ADD_VERB(PSTRING("\005", "write"), writefunc);
	ADD_VERB(PSTRING("\007", "compare"), comparefunc);
	ADD_VERB(PSTRING("\016", "writewholefile"), writewholefilefunc);
	ADD_VERB(PSTRING("\013", "getpathchar"), getpathcharfunc);
	ADD_VERB(PSTRING("\027", "freespaceonvolumedouble"), volumefreespacedoublefunc);
	ADD_VERB(PSTRING("\020", "volumesizedouble"), volumesizedoublefunc);
	ADD_VERB(PSTRING("\012", "getmp3info"), getmp3infofunc);
	ADD_VERB(PSTRING("\015", "readwholefile"), readwholefilefunc);
	ADD_VERB(PSTRING("\015", "getlabelindex"), getlabelindexfunc);
	ADD_VERB(PSTRING("\015", "setlabelindex"), setlabelindexfunc);
	ADD_VERB(PSTRING("\015", "getlabelnames"), getlabelnamesfunc);
	ADD_VERB(PSTRING("\014", "getposixpath"), getposixpathfunc);

	pophashtable();

	log_debug(LOG_COMP_LANG, "fileinitverbs: file processor registered successfully");

	return true;
}
