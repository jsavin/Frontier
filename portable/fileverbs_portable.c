/*
 * fileverbs_portable.c - Portable file verb implementations for headless mode
 *
 * Implements 86 file verbs using POSIX APIs instead of Mac-specific CoreFoundation.
 * This file is linked in headless builds instead of Common/source/fileverbs.c.
 *
 * Implementation status:
 * - Phase 1: Infrastructure and stubs (this file)
 * - Phase 2: Tier 1 critical operations (20 verbs) - TODO
 * - Phase 3: Tier 2 file I/O (14 verbs) - TODO
 * - Phase 4: Tier 3 optional features (16 verbs) - TODO
 * - Phase 5: Tier 4 Mac-specific stubs (36 verbs) - STUBBED
 *
 * Created: 2026-01-01 - Portable file verb dispatcher
 */

#include "frontier.h"
#include "standard.h"

#include "error.h"
#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "file.h"
#include "file_portable.h"
#include "file_working_dir.h"
#include "kernelverbdefs.h"
#include "resources.h"
#include "logging.h"

#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

/* Token enum - MUST match tyfiletoken in fileverbs.c and headless_file_verbs.c */
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
	writewholefilefunc = 76,
	getpathcharfunc = 77,
	volumefreespacedoublefunc = 78,
	volumesizedoublefunc = 79,
	getmp3infofunc = 80,
	readwholefilefunc = 81,
	getlabelindexfunc = 82,
	setlabelindexfunc = 83,
	getlabelnamesfunc = 84,
	getposixpathfunc = 85
};


/*
 * Main dispatcher for portable file verb implementations
 */
boolean portable_filefunctionvalue(short token, hdltreenode hparam1,
                                  tyvaluerecord *vreturned, bigstring bserror) {
	log_debug(LOG_COMP_LANG, "portable_filefunctionvalue: token=%d", token);

	switch (token) {

		/* Tier 1: Critical file operations (20 verbs) - TODO Phase 2 */

		case fileexistsfunc:
		case fileisfolderfunc:
		case fileisvolumefunc:
		case filedeletefunc:
		case filerenamefunc:
		case filecopyfunc:
		case filemovefunc:
		case filesizefunc:
		case filecreatedfunc:
		case filemodifiedfunc:
		case filefullpathfunc:
		case filegetpathfunc:
		case filesetpathfunc:
		case filefrompathfunc:
		case folderfrompathfunc:
		case newfunc:
		case newfolderfunc:
		case getsystempathfunc:
		case getspecialpathfunc:
		case getpathcharfunc:
			getstringlist(langerrorlist, unimplementedverberror, bserror);
			return false;

		/* Tier 2: File I/O operations (14 verbs) - TODO Phase 3 */

		case openfilefunc:
		case closefilefunc:
		case endoffilefunc:
		case setendoffilefunc:
		case getendoffilefunc:
		case setpositionfunc:
		case getpositionfunc:
		case readlinefunc:
		case writelinefunc:
		case readfunc:
		case writefunc:
		case readwholefilefunc:
		case writewholefilefunc:
		case comparefunc:
		case countlinesfunc:
		case findinfilefunc:
			getstringlist(langerrorlist, unimplementedverberror, bserror);
			return false;

		/* Tier 3: Optional features (16 verbs) - TODO Phase 4 */

		case filetypefunc:
		case filecreatorfunc:
		case setfiletypefunc:
		case setfilecreatorfunc:
		case setfilecreatedfunc:
		case setfilemodifiedfunc:
		case fileislockedfunc:
		case filelockfunc:
		case fileunlockfunc:
		case filecopydataforkfunc:
		case fileisvisiblefunc:
		case filesetvisiblefunc:
		case fileisbusyfunc:
		case filehasbundlefunc:
		case filesetbundlefunc:
		case getposixpathfunc:
			getstringlist(langerrorlist, unimplementedverberror, bserror);
			return false;

		/* Tier 4: Mac-specific UI/metadata (36 verbs) - STUBBED (not implementable in headless) */

		case sfgetfilefunc:
		case sfputfilefunc:
		case sfgetfolderfunc:
		case sfgetdiskfunc:
			copyctopstring("File dialogs not supported in headless mode - use explicit paths", bserror);
			return false;

		case filegeticonposfunc:
		case fileseticonposfunc:
			copyctopstring("Icon positions are Mac-only - not supported in headless mode", bserror);
			return false;

		case filegetlabelfunc:
		case filesetlabelfunc:
		case getlabelindexfunc:
		case setlabelindexfunc:
		case getlabelnamesfunc:
			copyctopstring("Finder labels are Mac-only - not supported in headless mode", bserror);
			return false;

		case getshortversionfunc:
		case setshortversionfunc:
		case getlongversionfunc:
		case setlongversionfunc:
			copyctopstring("Version resources are Mac-only - not supported in headless mode", bserror);
			return false;

		case newaliasfunc:
		case fileisaliasfunc:
		case filefollowaliasfunc:
			copyctopstring("Mac aliases not supported in headless mode - use symlinks", bserror);
			return false;

		case filegetcommentfunc:
		case filesetcommentfunc:
			copyctopstring("File comments are Mac-only - not supported in headless mode", bserror);
			return false;

		case filefindappfunc:
			copyctopstring("Application search not implemented in headless mode", bserror);
			return false;

		case filecopyresourceforkfunc:
			copyctopstring("Resource forks are obsolete (Mac OS 9) - not supported", bserror);
			return false;

		case volumeejectfunc:
		case volumeisejectablefunc:
		case unmountvolumefunc:
		case mountservervolumefunc:
			copyctopstring("Volume mount/eject operations not supported in headless mode", bserror);
			return false;

		case volumefreespacefunc:
		case volumesizefunc:
		case volumeblocksizefunc:
		case volumefreespacedoublefunc:
		case volumesizedoublefunc:
			copyctopstring("Volume operations not implemented in headless mode", bserror);
			return false;

		case filesonvolumefunc:
		case foldersonvolumefunc:
			copyctopstring("Volume enumeration not supported in headless mode", bserror);
			return false;

		case getmp3infofunc:
			copyctopstring("MP3 parsing not implemented in headless mode", bserror);
			return false;

		default:
			copyctopstring("Unknown file verb", bserror);
			return false;
	}
}
