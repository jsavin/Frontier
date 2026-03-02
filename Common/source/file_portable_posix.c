/* file_portable_posix.c - POSIX implementation of portable file path operations */

#include "frontier.h"
#include "standard.h"
#include "strings.h"
#include "file_portable.h"

#ifdef __MACH__
#define PATH_SEP '/'
#define PATH_SEP_STR "/"
#else
#define PATH_SEP '/'
#define PATH_SEP_STR "/"
#endif

char portable_getpathsep(void) {
	return PATH_SEP;
}

boolean portable_folderfrompath(const bigstring bspath, bigstring bsfolder) {
	/*
	 * Extract folder portion from path by finding last path separator
	 * and returning everything up to and INCLUDING the separator.
	 *
	 * Per original Mac implementation (fileops.m):
	 * "return all the characters to the left of the colon, and the colon"
	 *
	 * Examples:
	 *   /Users/test/file.txt -> /Users/test/
	 *   /Users/test/         -> /Users/
	 *   file.txt             -> (empty)
	 *   /                    -> /
	 */

	short pathlen = stringlength(bspath);
	short i;
	short lastsep = -1;

	/* Empty path returns empty folder */
	if (pathlen == 0) {
		setemptystring(bsfolder);
		return true;
	}

	/* Find last path separator */
	for (i = pathlen; i >= 1; i--) {
		if (bspath[i] == PATH_SEP) {
			lastsep = i;
			break;
		}
	}

	/* No separator found - no folder component */
	if (lastsep == -1) {
		setemptystring(bsfolder);
		return true;
	}

	/* Root path "/" - return "/" */
	if (lastsep == 1 && pathlen == 1) {
		copystring(BIGSTRING("\p/"), bsfolder);
		return true;
	}

	/* Path ends with separator - find previous separator */
	if (lastsep == pathlen) {
		/* Look for previous separator */
		for (i = lastsep - 1; i >= 1; i--) {
			if (bspath[i] == PATH_SEP) {
				lastsep = i;
				break;
			}
		}

		/* If we found root, return it */
		if (lastsep == 1) {
			copystring(BIGSTRING("\p/"), bsfolder);
			return true;
		}
	}

	/* Copy everything up to and INCLUDING the last separator */
	if (lastsep > 0) {
		copystring(bspath, bsfolder);
		setstringlength(bsfolder, lastsep);  /* Include the separator */
		return true;
	}

	setemptystring(bsfolder);
	return true;
}

boolean portable_filefrompath(const bigstring bspath, bigstring bsfile) {
	/*
	 * Extract filename portion from path by finding last path separator
	 * and returning everything after it.
	 *
	 * For directory paths (ending with separator), strips the trailing
	 * separator, extracts the directory name, then re-appends the separator.
	 * This matches legacy Frontier behavior where file.fileFromPath on a
	 * folder path returns the folder name with a trailing separator.
	 *
	 * Examples:
	 *   "/path/to/file.txt" -> "file.txt"
	 *   "/path/to/folder/"  -> "folder/"
	 *   "/path/to/folder"   -> "folder"
	 */

	short pathlen = stringlength(bspath);
	short effectivelen;
	boolean flfolder = false;
	short i;
	short lastsep = 0;

	/* Empty path returns empty filename */
	if (pathlen == 0) {
		setemptystring(bsfile);
		return true;
	}

	effectivelen = pathlen;

	/* If path ends with separator, strip it and remember it's a folder */
	if (bspath[pathlen] == PATH_SEP) {
		flfolder = true;
		effectivelen = pathlen - 1;
	}

	/* Find last path separator in the effective path */
	for (i = effectivelen; i >= 1; i--) {
		if (bspath[i] == PATH_SEP) {
			lastsep = i;
			break;
		}
	}

	/* Copy everything after last separator */
	if (lastsep > 0) {
		short filelen = effectivelen - lastsep;
		short j;

		setstringlength(bsfile, filelen);
		for (j = 1; j <= filelen; j++) {
			bsfile[j] = bspath[lastsep + j];
		}
	}
	else {
		/* No separator - entire (effective) path is filename */
		copystring(bspath, bsfile);
		if (flfolder)
			setstringlength(bsfile, effectivelen);
	}

	/* Re-add trailing separator for folder paths */
	if (flfolder)
		pushchar(PATH_SEP, bsfile);

	return true;
}

boolean portable_splitpath(const bigstring bspath, bigstring bsfolder,
                          bigstring bsfile) {
	if (!portable_folderfrompath(bspath, bsfolder))
		return false;
	if (!portable_filefrompath(bspath, bsfile))
		return false;
	return true;
}
