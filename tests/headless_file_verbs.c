/*
 * headless_file_verbs.c - File processor verbs for headless mode
 *
 * This file implements cross-platform file verbs for headless mode.
 * Most verbs return "not implemented" - only cross-platform essentials are implemented.
 *
 * @IMPLEMENTED:
 *   - file.type() - extension-based type detection
 *   - file.creator() - returns spaces (no creator codes on non-Mac)
 *   - file.hasbundle() - suffix-based bundle detection
 *   - file.isvisible() - always returns true
 *   - file.setvisible() - returns false (not supported)
 *
 * Created: 2026-01-01 - File verb bindings (Phase 3)
 */

#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"
#include "file.h"
#include "logging.h"

#include <unistd.h>  /* for unlink() */

/* Token enum matching fileverbs.c */
typedef enum tyfiletoken {
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
	fileisbusyfunc = 45,
	filehasbundlefunc = 46,
	filesetbundlefunc = 47,
	fileisaliasfunc = 48,
	fileisvisiblefunc = 49,
	filesetvisiblefunc = 50,
	filefollowaliasfunc = 51,
	filemovefunc = 52,
	volumeejectfunc = 53,
	volumeisejectablefunc = 54,
	volumefreespacefunc = 55,
	volumesizefunc = 56,
	volumeblocksizefunc = 57,
	filesonvolumefunc = 58,
	foldersonvolumefunc = 59,
	unmountvolumefunc = 60,
	mountservervolumefunc = 61,
	filefindinfilefunc = 62,
	filecountlinesfunc = 63,
	fileopenfunc = 64,
	fileclosefunc = 65,
	fileendoffunc = 66,
	filesetendoffunc = 67,
	filegetendoffunc = 68,
	filesetpositionfunc = 69,
	filegetpositionfunc = 70,
	filereadlinefunc = 71,
	filewritelinefunc = 72,
	filereadfunc = 73,
	filewritefunc = 74,
	filecomparefunc = 75,
	writewholefilefunc = 76,
	getpathcharfunc = 77,
	volumefreespacedoublefunc = 78,
	volumesizedoublefunc = 79,
	filegetmp3infofunc = 80,
	readwholefilefunc = 81,
	getlabelindexfunc = 82,
	setlabelindexfunc = 83,
	getlabelnamesfunc = 84,
	getposixpathfunc = 85,

	filv_count
} tyfiletoken;

/* Compile-time verification */
#define EXPECTED_FILE_VERB_COUNT 86
_Static_assert(filv_count == EXPECTED_FILE_VERB_COUNT,
               "Token enum out of sync with fileverbs.c");

/* Helper: get filespec parameter */
static boolean getpathvalue(hdltreenode hparam1, short pnum, ptrfilespec fspath) {
	tyvaluerecord v;

	if (!getparamvalue(hparam1, pnum, &v))
		return false;

	if (!coercetofilespec(&v))
		return false;

	*fspath = **v.data.filespecvalue;

	return true;
}

/* Helper: get filename from filespec */
extern boolean getfsfile(const ptrfilespec fs, bigstring bs);

/* External file operations from file_portable.c and fileops.c */
extern boolean pathtofilespec(bigstring bspath, ptrfilespec fs);
extern boolean opennewfile(ptrfilespec, OSType, OSType, hdlfilenum *);
extern boolean closefile(hdlfilenum);
extern boolean fileexists(const ptrfilespec, boolean *);

/* Helper: create new file (wrapper around opennewfile + closefile) */
boolean newfile(const ptrfilespec fs, OSType creator, OSType filetype) {
	hdlfilenum fnum;

	if (!opennewfile((ptrfilespec)fs, creator, filetype, &fnum))
		return false;

	return closefile(fnum);
}

/* Helper: delete file (POSIX implementation for headless mode) */
boolean deletefile(const ptrfilespec fs) {
	bigstring bspath;
	char cpath[256];

	if (!filespectopath(fs, bspath))
		return false;

	copyptocstring(bspath, cpath);

	if (unlink(cpath) != 0)
		return false;

	return true;
}

static boolean file_valueproc(short token, hdltreenode hparam1,
                                tyvaluerecord *vreturned,
                                bigstring bserror) {
	tyfilespec fs;
	bigstring bs, bsext;
	OSType type;
	short extlen;

	switch(token) {
		case filefrompathfunc: {
			/* Create filespec from path string */
			bigstring bspath;
			tyvaluerecord val;

			flnextparamislast = true;

			if (!getstringvalue(hparam1, 1, bspath))
				return false;

			if (!pathtofilespec(bspath, &fs))
				return false;

			if (!setfilespecvalue(&fs, &val))
				return false;

			*vreturned = val;
			return true;
		}

		case newfunc: {
			/* Create new file */
			flnextparamislast = true;

			if (!getpathvalue(hparam1, 1, &fs))
				return false;

			if (!newfile(&fs, 0, 0))  /* No creator/type codes */
				return false;

			(*vreturned).data.flvalue = true;
			return true;
		}

		case filedeletefunc: {
			/* Delete file */
			flnextparamislast = true;

			if (!getpathvalue(hparam1, 1, &fs))
				return false;

			if (!deletefile(&fs))
				return false;

			(*vreturned).data.flvalue = true;
			return true;
		}

		case fileexistsfunc: {
			/* Check if file exists */
			boolean flfolder;

			flnextparamislast = true;

			if (!getpathvalue(hparam1, 1, &fs))
				return false;

			if (!fileexists(&fs, &flfolder))
				return false;

			(*vreturned).data.flvalue = true;
			return true;
		}

		case filetypefunc: {
			/* Cross-platform: extension-based type detection */
			flnextparamislast = true;

			if (!getpathvalue(hparam1, 1, &fs))
				return false;

			/* Get filename */
			getfsfile(&fs, bs);

			/* Extract extension (after last '.') */
			lastword(bs, '.', bsext);

			/* Check if extension was found (if bsext == bs, no '.' was found) */
			if (stringlength(bsext) == 0 || equalstrings(bs, bsext)) {
				/* No extension */
				type = 0x3F3F3F3F;  /* '????' */
				return setostypevalue(type, vreturned);
			}

			extlen = stringlength(bsext);

			if (extlen <= 4) {
				/* Short extension: return as OSType */
				stringtoostype(bsext, &type);
				return setostypevalue(type, vreturned);
			} else {
				/* Long extension: return as string */
				return setstringvalue(bsext, vreturned);
			}
		}

		case filecreatorfunc: {
			/* Cross-platform: always return 4 spaces */
			flnextparamislast = true;

			if (!getpathvalue(hparam1, 1, &fs))
				return false;

			type = 0x20202020;  /* '    ' - 4 spaces */
			return setostypevalue(type, vreturned);
		}

		case filehasbundlefunc: {
			/* Cross-platform: check for bundle suffix */
			flnextparamislast = true;

			if (!getpathvalue(hparam1, 1, &fs))
				return false;

			/* Get full path */
			if (!filespectopath(&fs, bs))
				return false;

			/* Extract extension */
			if (!lastword(bs, '.', bsext)) {
				(*vreturned).data.flvalue = false;
				return true;
			}

			/* Check if it's a bundle suffix */
			(*vreturned).data.flvalue = (
				equalstrings(bsext, BIGSTRING("\003app")) ||
				equalstrings(bsext, BIGSTRING("\006bundle")) ||
				equalstrings(bsext, BIGSTRING("\011framework")) ||
				equalstrings(bsext, BIGSTRING("\006plugin")) ||
				equalstrings(bsext, BIGSTRING("\004kext"))
			);

			return true;
		}

		case fileisvisiblefunc: {
			/* Cross-platform: always return true */
			flnextparamislast = true;

			if (!getpathvalue(hparam1, 1, &fs))
				return false;

			(*vreturned).data.flvalue = true;
			return true;
		}

		case filesetvisiblefunc: {
			/* Cross-platform: return false (not supported) */
			boolean flvisible;

			if (!getpathvalue(hparam1, 1, &fs))
				return false;

			flnextparamislast = true;

			if (!getbooleanvalue(hparam1, 2, &flvisible))
				return false;

			(*vreturned).data.flvalue = false;
			return true;
		}

		case setfiletypefunc:
		case setfilecreatorfunc:
		case filesetbundlefunc:
		case filecopyresourceforkfunc:
		case newaliasfunc:
		case filefollowaliasfunc:
		case fileisaliasfunc:
		case filegeticonposfunc:
		case fileseticonposfunc:
		case getshortversionfunc:
		case setshortversionfunc:
		case getlongversionfunc:
		case setlongversionfunc:
		case filegetcommentfunc:
		case filesetcommentfunc:
		case filegetlabelfunc:
		case filesetlabelfunc:
		case sfgetfilefunc:
		case sfputfilefunc:
		case sfgetfolderfunc:
		case sfgetdiskfunc:
		case filefindappfunc:
			/* Mac-only or not implemented */
			if (bserror)
				copystring(BIGSTRING("\pnot implemented"), bserror);
			return false;

		default:
			/* All other verbs not implemented */
			if (bserror)
				copystring(BIGSTRING("\pnot implemented"), bserror);
			return false;
	}
}

/* Exported callback */
boolean headless_file_verbs_callback(short token, hdltreenode hparam1,
                                      tyvaluerecord *vreturned, bigstring bserror) {
	return file_valueproc(token, hparam1, vreturned, bserror);
}

/* Initialization function */
boolean fileinitverbs(void) {
	hdlhashtable htable = nil;
	bigstring bsname;

	extern boolean newfunctionprocessor(bigstring, langvaluecallback, boolean, hdlhashtable*);
	extern boolean langaddkeyword(bigstring, short);
	extern boolean pushhashtable(hdlhashtable);
	extern boolean pophashtable(void);

	log_debug(LOG_COMP_LANG, "fileinitverbs: registering headless file processor");

	copystring(BIGSTRING("\004file"), bsname);

	if (!newfunctionprocessor(bsname, &headless_file_verbs_callback, true, &htable))
		return false;

	pushhashtable(htable);

	#define ADD_VERB(name, tok) do { \
		bigstring bs; \
		copystring(name, bs); \
		if (!langaddkeyword(bs, tok)) { \
			pophashtable(); \
			return false; \
		} \
	} while(0)

	ADD_VERB(BIGSTRING("\007created"), filecreatedfunc);
	ADD_VERB(BIGSTRING("\010modified"), filemodifiedfunc);
	ADD_VERB(BIGSTRING("\004type"), filetypefunc);
	ADD_VERB(BIGSTRING("\007creator"), filecreatorfunc);
	ADD_VERB(BIGSTRING("\012setcreated"), setfilecreatedfunc);
	ADD_VERB(BIGSTRING("\013setmodified"), setfilemodifiedfunc);
	ADD_VERB(BIGSTRING("\007settype"), setfiletypefunc);
	ADD_VERB(BIGSTRING("\012setcreator"), setfilecreatorfunc);
	ADD_VERB(BIGSTRING("\010isfolder"), fileisfolderfunc);
	ADD_VERB(BIGSTRING("\010isvolume"), fileisvolumefunc);
	ADD_VERB(BIGSTRING("\010islocked"), fileislockedfunc);
	ADD_VERB(BIGSTRING("\004lock"), filelockfunc);
	ADD_VERB(BIGSTRING("\006unlock"), fileunlockfunc);
	ADD_VERB(BIGSTRING("\004copy"), filecopyfunc);
	ADD_VERB(BIGSTRING("\014copydatafork"), filecopydataforkfunc);
	ADD_VERB(BIGSTRING("\020copyresourcefork"), filecopyresourceforkfunc);
	ADD_VERB(BIGSTRING("\006delete"), filedeletefunc);
	ADD_VERB(BIGSTRING("\006rename"), filerenamefunc);
	ADD_VERB(BIGSTRING("\006exists"), fileexistsfunc);
	ADD_VERB(BIGSTRING("\004size"), filesizefunc);
	ADD_VERB(BIGSTRING("\010fullpath"), filefullpathfunc);
	ADD_VERB(BIGSTRING("\007getpath"), filegetpathfunc);
	ADD_VERB(BIGSTRING("\007setpath"), filesetpathfunc);
	ADD_VERB(BIGSTRING("\014filefrompath"), filefrompathfunc);
	ADD_VERB(BIGSTRING("\016folderfrompath"), folderfrompathfunc);
	ADD_VERB(BIGSTRING("\022getsystemfolderpath"), getsystempathfunc);
	ADD_VERB(BIGSTRING("\023getspecialfolderpath"), getspecialpathfunc);
	ADD_VERB(BIGSTRING("\003new"), newfunc);
	ADD_VERB(BIGSTRING("\011newfolder"), newfolderfunc);
	ADD_VERB(BIGSTRING("\010newalias"), newaliasfunc);
	ADD_VERB(BIGSTRING("\016getfiledialog"), sfgetfilefunc);
	ADD_VERB(BIGSTRING("\016putfiledialog"), sfputfilefunc);
	ADD_VERB(BIGSTRING("\020getfolderdialog"), sfgetfolderfunc);
	ADD_VERB(BIGSTRING("\016getdiskdialog"), sfgetdiskfunc);
	ADD_VERB(BIGSTRING("\013geticonpos"), filegeticonposfunc);
	ADD_VERB(BIGSTRING("\013seticonpos"), fileseticonposfunc);
	ADD_VERB(BIGSTRING("\012getversion"), getshortversionfunc);
	ADD_VERB(BIGSTRING("\012setversion"), setshortversionfunc);
	ADD_VERB(BIGSTRING("\016getfullversion"), getlongversionfunc);
	ADD_VERB(BIGSTRING("\016setfullversion"), setlongversionfunc);
	ADD_VERB(BIGSTRING("\012getcomment"), filegetcommentfunc);
	ADD_VERB(BIGSTRING("\012setcomment"), filesetcommentfunc);
	ADD_VERB(BIGSTRING("\010getlabel"), filegetlabelfunc);
	ADD_VERB(BIGSTRING("\010setlabel"), filesetlabelfunc);
	ADD_VERB(BIGSTRING("\017findapplication"), filefindappfunc);
	ADD_VERB(BIGSTRING("\006isbusy"), fileisbusyfunc);
	ADD_VERB(BIGSTRING("\011hasbundle"), filehasbundlefunc);
	ADD_VERB(BIGSTRING("\011setbundle"), filesetbundlefunc);
	ADD_VERB(BIGSTRING("\007isalias"), fileisaliasfunc);
	ADD_VERB(BIGSTRING("\011isvisible"), fileisvisiblefunc);
	ADD_VERB(BIGSTRING("\012setvisible"), filesetvisiblefunc);
	ADD_VERB(BIGSTRING("\013followalias"), filefollowaliasfunc);
	ADD_VERB(BIGSTRING("\004move"), filemovefunc);
	ADD_VERB(BIGSTRING("\005eject"), volumeejectfunc);
	ADD_VERB(BIGSTRING("\014isejectable"), volumeisejectablefunc);
	ADD_VERB(BIGSTRING("\020freespaceonvolume"), volumefreespacefunc);
	ADD_VERB(BIGSTRING("\012volumesize"), volumesizefunc);
	ADD_VERB(BIGSTRING("\017volumeblocksize"), volumeblocksizefunc);
	ADD_VERB(BIGSTRING("\016filesonvolume"), filesonvolumefunc);
	ADD_VERB(BIGSTRING("\020foldersonvolume"), foldersonvolumefunc);
	ADD_VERB(BIGSTRING("\015unmountvolume"), unmountvolumefunc);
	ADD_VERB(BIGSTRING("\021mountservervolume"), mountservervolumefunc);
	ADD_VERB(BIGSTRING("\012findinfile"), filefindinfilefunc);
	ADD_VERB(BIGSTRING("\012countlines"), filecountlinesfunc);
	ADD_VERB(BIGSTRING("\004open"), fileopenfunc);
	ADD_VERB(BIGSTRING("\005close"), fileclosefunc);
	ADD_VERB(BIGSTRING("\011endoffile"), fileendoffunc);
	ADD_VERB(BIGSTRING("\014setendoffile"), filesetendoffunc);
	ADD_VERB(BIGSTRING("\014getendoffile"), filegetendoffunc);
	ADD_VERB(BIGSTRING("\013setposition"), filesetpositionfunc);
	ADD_VERB(BIGSTRING("\013getposition"), filegetpositionfunc);
	ADD_VERB(BIGSTRING("\010readline"), filereadlinefunc);
	ADD_VERB(BIGSTRING("\011writeline"), filewritelinefunc);
	ADD_VERB(BIGSTRING("\004read"), filereadfunc);
	ADD_VERB(BIGSTRING("\005write"), filewritefunc);
	ADD_VERB(BIGSTRING("\007compare"), filecomparefunc);
	ADD_VERB(BIGSTRING("\016writewholefile"), writewholefilefunc);
	ADD_VERB(BIGSTRING("\013getpathchar"), getpathcharfunc);
	ADD_VERB(BIGSTRING("\026freespaceonvolumedouble"), volumefreespacedoublefunc);
	ADD_VERB(BIGSTRING("\020volumesizedouble"), volumesizedoublefunc);
	ADD_VERB(BIGSTRING("\012getmp3info"), filegetmp3infofunc);
	ADD_VERB(BIGSTRING("\016readwholefile"), readwholefilefunc);
	ADD_VERB(BIGSTRING("\015getLabelIndex"), getlabelindexfunc);
	ADD_VERB(BIGSTRING("\015setLabelIndex"), setlabelindexfunc);
	ADD_VERB(BIGSTRING("\015getLabelNames"), getlabelnamesfunc);
	ADD_VERB(BIGSTRING("\014getPosixPath"), getposixpathfunc);

	pophashtable();

	log_debug(LOG_COMP_LANG, "fileinitverbs: file processor registered successfully");

	return true;
}
