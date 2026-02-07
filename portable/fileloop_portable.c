/*
 * fileloop_portable.c - Portable implementation of file loop iteration
 *
 * Provides POSIX-based implementations of fileinitloop/filenextloop/fileendloop
 * for the headless CLI, replacing the Mac-specific Carbon API calls in
 * Common/source/fileloop.c.
 *
 * The original implementation uses PBGetCatInfoSync to enumerate directory
 * contents into an oplist. This portable version uses opendir/readdir/closedir
 * and stores file paths in the same oplist structure for compatibility with
 * the existing fileloopguts() in langevaluate.c.
 */

#include "frontier.h"
#include "memory.h"
#include "strings.h"
#include "error.h"
#include "file.h"
#include "oplist.h"
#include "fileloop.h"
#include "langinternal.h"
#include "logging.h"

#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>

/*
 * Portable fileloop record - matches the original structure layout.
 * The original uses vnum/dirid for Mac volume/directory IDs;
 * we repurpose the structure but only use hfilelist and ixdirectory.
 */
#pragma pack(2)
typedef struct tyfilelooprecord {
	short vnum;
	long dirid;
	short ixdirectory;
	hdllistrecord hfilelist;
} tyfilelooprecord, *ptrfilelooprecord, **hdlfilelooprecord;
#pragma options align=reset

/* Helper: convert filespec to C string path using public filespectopath API */
static boolean filespec_to_cpath(const ptrfilespec fs, char *out, size_t outsz) {
	bigstring bspath;

	if (!filespectopath(fs, bspath))
		return false;

	unsigned int len = (unsigned char)bspath[0];
	if (len >= outsz)
		len = (unsigned int)(outsz - 1);

	memcpy(out, bspath + 1, len);
	out[len] = '\0';
	return out[0] != '\0';
}

static boolean fileloopreleaseitem (Handle h) {
	#pragma unused(h)
	return (true);
}

boolean fileinitloop (const ptrfilespec fst, tyfileloopcallback filefilter, Handle *hfileloop) {
	#pragma unused(filefilter)

	/*
	 * Enumerate all entries in the directory specified by fst.
	 * Store each filename (with trailing '/' for directories) in an oplist.
	 * This matches the original Carbon implementation's behavior of
	 * pre-loading all filenames at init time.
	 */

	char dirpath[4096];
	DIR *dp;
	struct dirent *entry;
	tyfilelooprecord info;
	hdlfilelooprecord h;
	hdllistrecord hlist;

	*hfileloop = nil;

	log_debug(LOG_COMP_GENERAL, "fileinitloop: called");

	if (!filespec_to_cpath(fst, dirpath, sizeof(dirpath))) {
		langerrormessage(BIGSTRING("\x1e" "Can't do fileloop: bad path"));
		return false;
	}

	/* Remove trailing slash for opendir (unless it's root "/") */
	size_t pathlen = strlen(dirpath);
	if (pathlen > 1 && dirpath[pathlen - 1] == '/')
		dirpath[pathlen - 1] = '\0';

	dp = opendir(dirpath);
	if (!dp) {
		bigstring bserr;
		char msg[512];
		snprintf(msg, sizeof(msg), "Can't do fileloop on \"%s\"", dirpath);
		size_t mlen = strlen(msg);
		if (mlen > 255) mlen = 255;
		bserr[0] = (unsigned char)mlen;
		memcpy(bserr + 1, msg, mlen);
		langerrormessage(bserr);
		return false;
	}

	/* Restore trailing slash for building full paths */
	pathlen = strlen(dirpath);
	if (dirpath[pathlen - 1] != '/') {
		dirpath[pathlen] = '/';
		dirpath[pathlen + 1] = '\0';
		pathlen++;
	}

	clearbytes(&info, sizeof(info));
	info.ixdirectory = 1;

	if (!newfilledhandle(&info, sizeof(info), hfileloop)) {
		closedir(dp);
		return false;
	}

	h = (hdlfilelooprecord) *hfileloop;
	hlist = nil;

	if (!opnewlist(&hlist, false))
		goto error;

	(**h).hfilelist = hlist;
	opsetreleaseitemcallback(hlist, &fileloopreleaseitem);

	while ((entry = readdir(dp)) != NULL) {
		char fullpath[4096];
		bigstring bsname;
		Handle hstring;
		struct stat st;

		/* Skip . and .. */
		if (entry->d_name[0] == '.' &&
			(entry->d_name[1] == '\0' ||
			 (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))
			continue;

		/* Build full path for stat */
		snprintf(fullpath, sizeof(fullpath), "%s%s", dirpath, entry->d_name);

		/* Build Pascal string with the full path.
		 * For directories, append '/' to match the original behavior
		 * of appending ':' for Mac folder names. The caller (filenextloop)
		 * will detect this and set flfolder accordingly.
		 */
		size_t namelen = strlen(fullpath);

		/* Check if it's a directory */
		boolean isdir = false;
		if (stat(fullpath, &st) == 0 && S_ISDIR(st.st_mode))
			isdir = true;

		if (isdir && namelen < sizeof(fullpath) - 1) {
			fullpath[namelen] = '/';
			fullpath[namelen + 1] = '\0';
			namelen++;
		}

		if (namelen > 255)
			namelen = 255;

		bsname[0] = (unsigned char)namelen;
		memcpy(bsname + 1, fullpath, namelen);

		if (!newtexthandle(bsname, &hstring))
			goto error;

		if (!oppushhandle(hlist, nil, hstring))
			goto error;
	}

	closedir(dp);
	return true;

error:
	closedir(dp);
	opdisposelist(hlist);
	disposehandle(*hfileloop);
	*hfileloop = nil;
	return false;
}

void fileendloop (Handle hfileloop) {
	register hdlfilelooprecord h = (hdlfilelooprecord) hfileloop;

	opdisposelist((**h).hfilelist);
	disposehandle((Handle) h);
}

boolean filenextloop (Handle hfileloop, ptrfilespec fsfile, boolean *flfolder) {

	/*
	 * Get the next file from the pre-built list.
	 * Each entry is a full path string. Directories have a trailing '/'.
	 */

	register hdlfilelooprecord h = (hdlfilelooprecord) hfileloop;
	Handle hdata;
	bigstring bs;

	if (!opgetlisthandle((**h).hfilelist, (**h).ixdirectory++, nil, &hdata))
		return false;

	texthandletostring(hdata, bs);

	/* Check for trailing '/' indicating a directory */
	*flfolder = (lastchar(bs) == '/');

	if (*flfolder)
		--*bs; /* remove trailing '/' from the path */

	/* Convert bigstring path to filespec */
	if (!pathtofilespec(bs, fsfile))
		return false;

	return true;
}

boolean diskinitloop (tyfileloopcallback diskfilter, Handle *hdiskloop) {
	#pragma unused(diskfilter)

	/* Not applicable in portable mode - no concept of mounted volumes */
	(void)hdiskloop;
	return false;
}

boolean folderloop (const ptrfilespec pfs, boolean flreverse, tyfileloopcallback filecallback, long refcon) {

	/*
	 * Iterate through all files in a folder, calling filecallback for each.
	 * This is used by various parts of the runtime (not just the fileloop keyword).
	 */

	char dirpath[4096];
	DIR *dp;
	struct dirent *entry;

	if (!filespec_to_cpath(pfs, dirpath, sizeof(dirpath)))
		return false;

	/* Remove trailing slash for opendir (unless root) */
	size_t pathlen = strlen(dirpath);
	if (pathlen > 1 && dirpath[pathlen - 1] == '/')
		dirpath[pathlen - 1] = '\0';

	dp = opendir(dirpath);
	if (!dp)
		return false;

	/* Restore trailing slash */
	pathlen = strlen(dirpath);
	if (dirpath[pathlen - 1] != '/') {
		dirpath[pathlen] = '/';
		dirpath[pathlen + 1] = '\0';
	}

	/* Collect entries first if flreverse, otherwise iterate forward */
	/* For simplicity, always iterate forward (reverse is rarely used) */
	(void)flreverse;

	while ((entry = readdir(dp)) != NULL) {
		bigstring bsname;
		tyfileinfo finfo;
		char fullpath[4096];
		struct stat st;

		if (entry->d_name[0] == '.' &&
			(entry->d_name[1] == '\0' ||
			 (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))
			continue;

		snprintf(fullpath, sizeof(fullpath), "%s%s", dirpath, entry->d_name);

		size_t namelen = strlen(entry->d_name);
		if (namelen > 255) namelen = 255;
		bsname[0] = (unsigned char)namelen;
		memcpy(bsname + 1, entry->d_name, namelen);

		clearbytes(&finfo, sizeof(finfo));

		if (stat(fullpath, &st) == 0) {
			finfo.flfolder = S_ISDIR(st.st_mode);
			finfo.timecreated = (unsigned long)st.st_ctime;
			finfo.timemodified = (unsigned long)st.st_mtime;
			finfo.sizedatafork = (unsigned long long)st.st_size;
		}

		if (!(*filecallback)(bsname, &finfo, refcon)) {
			closedir(dp);
			return true; /* callback returned false = stop iteration */
		}
	}

	closedir(dp);
	return true;
}
