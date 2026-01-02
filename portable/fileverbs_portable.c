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
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <pthread.h>

/* Forward declarations from file_portable_posix.c */
extern boolean portable_filefrompath(const bigstring bspath, bigstring bsfile);
extern boolean portable_folderfrompath(const bigstring bspath, bigstring bsfolder);

/* Frontier epoch offset: seconds between 1904 and 1970 */
#define FRONTIER_EPOCH_OFFSET 2082844800LL

/*
 * Helper function: Convert Unix time_t to Frontier seconds (since 1904)
 * Protects against Y2038+ overflow when converting 64-bit time_t to uint32_t.
 */
static inline uint32_t timet_to_frontierseconds(time_t unixtime) {
	/* Check for overflow before addition (Y2038 safety) */
	if (unixtime > (INT64_MAX - FRONTIER_EPOCH_OFFSET)) {
		log_warn(LOG_COMP_LANG, "Date overflow in timet_to_frontierseconds: time_t=%lld", (long long)unixtime);
		return UINT32_MAX;  /* Return max valid Frontier date */
	}
	return (uint32_t)(unixtime + FRONTIER_EPOCH_OFFSET);
}

/*
 * Helper function: Convert filespec to C string path
 */
static boolean filespec_to_cstring(const ptrfilespec fs, char *path, size_t pathsize) {
	bigstring bspath;

	if (!filespectopath(fs, bspath))
		return false;

	if (bspath[0] == 0) {
		path[0] = '\0';
		return false;
	}

	size_t len = bspath[0];
	if (len >= pathsize)
		len = pathsize - 1;

	memcpy(path, &bspath[1], len);
	path[len] = '\0';
	return true;
}

/* File handle table for open files */
#define MAX_OPEN_FILES 64

/* Maximum file size for readwholefile() - 500MB limit prevents OOM on huge files */
#define MAX_READWHOLEFILE_SIZE (500 * 1024 * 1024)

typedef struct {
	FILE *fp;
	short refnum;
	boolean inuse;
} filehandle;

static filehandle filetable[MAX_OPEN_FILES];
static boolean g_cleanup_registered = false;
static pthread_mutex_t filetable_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Allocate a file handle and return refnum.
 * Uses slot index + 1 as refnum for O(1) allocation and lookup.
 */
static short allocate_filehandle(FILE *fp) {
	int i;
	short result = 0;

	pthread_mutex_lock(&filetable_mutex);

	/* Find free slot and use slot index + 1 as refnum */
	for (i = 0; i < MAX_OPEN_FILES; i++) {
		if (!filetable[i].inuse) {
			filetable[i].fp = fp;
			filetable[i].refnum = i + 1;  /* Slot 0 → refnum 1, slot 1 → refnum 2, etc. */
			filetable[i].inuse = true;
			result = filetable[i].refnum;
			break;
		}
	}

	pthread_mutex_unlock(&filetable_mutex);
	return result; /* Returns refnum or 0 if no free handles */
}

/*
 * Get FILE* from refnum (O(1) direct lookup)
 */
static FILE* get_filepointer(short refnum) {
	FILE *result = NULL;

	/* Validate refnum range */
	if (refnum < 1 || refnum > MAX_OPEN_FILES) {
		return NULL;
	}

	pthread_mutex_lock(&filetable_mutex);

	/* Direct O(1) lookup using refnum - 1 as index */
	int i = refnum - 1;
	if (filetable[i].inuse && filetable[i].refnum == refnum) {
		result = filetable[i].fp;
	}

	pthread_mutex_unlock(&filetable_mutex);
	return result;
}

/*
 * Release file handle (O(1) direct lookup)
 * Defensively closes FILE* to prevent leaks even if caller forgets to close.
 */
static boolean release_filehandle(short refnum) {
	boolean result = false;
	FILE *fp_to_close = NULL;

	/* Validate refnum range */
	if (refnum < 1 || refnum > MAX_OPEN_FILES) {
		return false;
	}

	pthread_mutex_lock(&filetable_mutex);

	/* Direct O(1) lookup using refnum - 1 as index */
	int i = refnum - 1;
	if (filetable[i].inuse && filetable[i].refnum == refnum) {
		fp_to_close = filetable[i].fp;  /* Save FILE* before clearing */
		filetable[i].inuse = false;
		filetable[i].fp = NULL;
		filetable[i].refnum = 0;
		result = true;
	}

	pthread_mutex_unlock(&filetable_mutex);

	/* Close outside mutex to avoid blocking I/O under lock */
	if (fp_to_close) {
		fclose(fp_to_close);
	}

	return result;
}

/*
 * Cleanup function registered with atexit() to close any leaked file handles.
 * This ensures:
 * 1. Buffers are flushed on normal exit (prevents data loss)
 * 2. Leaked handles are logged for debugging
 * 3. Resources are properly released
 */
static void cleanup_file_handles(void) {
	FILE *fps_to_close[MAX_OPEN_FILES];
	int count = 0;
	int i;

	/* Phase 1: Collect FILE* pointers under mutex */
	pthread_mutex_lock(&filetable_mutex);
	for (i = 0; i < MAX_OPEN_FILES; i++) {
		if (filetable[i].inuse) {
			log_debug(LOG_COMP_LANG, "cleanup_file_handles: closing refnum=%d (fp=%p)",
			         filetable[i].refnum, (void*)filetable[i].fp);

			fps_to_close[count++] = filetable[i].fp;
			filetable[i].inuse = false;
			filetable[i].fp = NULL;
		}
	}
	pthread_mutex_unlock(&filetable_mutex);

	/* Phase 2: Close handles without holding mutex (avoid blocking I/O under lock) */
	for (i = 0; i < count; i++) {
		fclose(fps_to_close[i]);  /* Flushes buffers, releases locks */
	}

	if (count > 0) {
		log_warn(LOG_COMP_LANG, "cleanup_file_handles: closed %d leaked file handle(s)",
		        count);
	}
}

/*
 * Initialize file handle cleanup - must be called once at startup.
 * This registers the cleanup hook with atexit() in a thread-safe manner.
 * Called from main() before any threads are spawned.
 */
void init_file_handle_cleanup(void) {
	/* Ensure this is only called once, before any threads are spawned */
	assert(!g_cleanup_registered && "init_file_handle_cleanup called multiple times");

	atexit(cleanup_file_handles);
	g_cleanup_registered = true;
	log_debug(LOG_COMP_LANG, "File handle cleanup registered with atexit()");
}

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

	/* Reset flnextparamislast to ensure clean state for each verb call.
	 * This is critical because flnextparamislast is a global variable that persists
	 * across function calls. Without this reset, verbs that set flnextparamislast=true
	 * but don't consume all parameters (like file.open with 1 param) will leave it
	 * set to true, causing the next 2-parameter verb to fail with "too many parameters". */
	flnextparamislast = false;

	switch (token) {

		/* Tier 1: Critical file operations (20 verbs) - Phase 2 */

		case fileexistsfunc: {
			/* Check if file or folder exists */
			tyfilespec fs;
			char path[4096];
			struct stat st;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			if (!filespec_to_cstring(&fs, path, sizeof(path)))
				return false;

			boolean exists = (stat(path, &st) == 0);
			return setbooleanvalue(exists, vreturned);
		}

		case fileisfolderfunc: {
			/* Check if path is a folder/directory */
			tyfilespec fs;
			char path[4096];
			struct stat st;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			if (!filespec_to_cstring(&fs, path, sizeof(path)))
				return false;

			if (stat(path, &st) != 0)
				return setbooleanvalue(false, vreturned);

			return setbooleanvalue(S_ISDIR(st.st_mode), vreturned);
		}

		case fileisvolumefunc: {
			/* In headless mode, we don't have Mac-style volumes - always return false */
			tyfilespec fs;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			return setbooleanvalue(false, vreturned);
		}
		case filesizefunc: {
			/* Return file size in bytes */
			tyfilespec fs;
			char path[4096];
			struct stat st;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			if (!filespec_to_cstring(&fs, path, sizeof(path)))
				return false;

			if (stat(path, &st) != 0) {
				copyctopstring("File not found", bserror);
				return false;
			}

			return setlongvalue(st.st_size, vreturned);
		}

		case filecreatedfunc: {
			/* Return file creation date as Frontier date (seconds since 1904) */
			tyfilespec fs;
			char path[4096];
			struct stat st;
			uint32_t frontierseconds;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			if (!filespec_to_cstring(&fs, path, sizeof(path)))
				return false;

			if (stat(path, &st) != 0) {
				copyctopstring("File not found", bserror);
				return false;
			}

			/* Convert Unix timestamp to Frontier date (seconds since Jan 1, 1904) */
			#ifdef __APPLE__
				/* macOS has st_birthtime for true creation time */
				frontierseconds = timet_to_frontierseconds(st.st_birthtime);
			#else
				/* Other platforms: use ctime (inode change time) as fallback */
				frontierseconds = timet_to_frontierseconds(st.st_ctime);
			#endif

			return setdatevalue(frontierseconds, vreturned);
		}

		case filemodifiedfunc: {
			/* Return file modification date as Frontier date */
			tyfilespec fs;
			char path[4096];
			struct stat st;
			uint32_t frontierseconds;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			if (!filespec_to_cstring(&fs, path, sizeof(path)))
				return false;

			if (stat(path, &st) != 0) {
				copyctopstring("File not found", bserror);
				return false;
			}

			/* Convert Unix timestamp to Frontier date */
			frontierseconds = timet_to_frontierseconds(st.st_mtime);
			return setdatevalue(frontierseconds, vreturned);
		}

		case filefullpathfunc: {
			/* Convert relative path to absolute path using realpath() */
			tyfilespec fs;
			char path[4096];
			char resolved[4096];
			bigstring bsresolved;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			if (!filespec_to_cstring(&fs, path, sizeof(path)))
				return false;

			/* Use realpath() to resolve to absolute path */
			if (realpath(path, resolved) == NULL) {
				/* If realpath fails (file doesn't exist), just return the original path */
				if (!filespectopath(&fs, bsresolved))
					return false;
			} else {
				/* Convert C string to bigstring */
				size_t len = strlen(resolved);
				if (len > 255)
					len = 255;
				bsresolved[0] = (unsigned char)len;
				memcpy(&bsresolved[1], resolved, len);
			}

			/* Convert bigstring path back to filespec */
			tyfilespec fsresolved;
			if (!pathtofilespec(bsresolved, &fsresolved))
				return false;

			return setfilespecvalue(&fsresolved, vreturned);
		}

		case filegetpathfunc: {
			/* Return current working directory */
			tyfilespec fs;

			if (!langcheckparamcount(hparam1, 0))
				return false;

			if (!filegetdefaultpath(&fs))
				return false;

			return setfilespecvalue(&fs, vreturned);
		}

		case filesetpathfunc: {
			/* Set current working directory */
			tyfilespec fs;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			if (!filesetdefaultpath(&fs))
				return false;

			return setbooleanvalue(true, vreturned);
		}

		case filefrompathfunc: {
			/* Extract filename from path */
			bigstring bspath, bsfile;

			flnextparamislast = true;

			if (!getstringvalue(hparam1, 1, bspath))
				return false;

			if (!portable_filefrompath(bspath, bsfile))
				return false;

			return setstringvalue(bsfile, vreturned);
		}

		case folderfrompathfunc: {
			/* Extract folder path from full path */
			bigstring bspath, bsfolder;

			flnextparamislast = true;

			if (!getstringvalue(hparam1, 1, bspath))
				return false;

			if (!portable_folderfrompath(bspath, bsfolder))
				return false;

			return setstringvalue(bsfolder, vreturned);
		}

		case newfunc: {
			/* Create new empty file */
			tyfilespec fs;
			char path[4096];
			FILE *fp;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			if (!filespec_to_cstring(&fs, path, sizeof(path)))
				return false;

			/* Create empty file */
			fp = fopen(path, "wb");
			if (!fp) {
				copyctopstring("Can't create file", bserror);
				return false;
			}

			fclose(fp);
			return setbooleanvalue(true, vreturned);
		}

		case newfolderfunc: {
			/* Create new directory */
			tyfilespec fs;
			char path[4096];

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			if (!filespec_to_cstring(&fs, path, sizeof(path)))
				return false;

			/* Create directory with standard permissions (0755) */
			if (mkdir(path, 0755) != 0) {
				if (errno == EEXIST) {
					copyctopstring("Folder already exists", bserror);
				} else {
					copyctopstring("Can't create folder", bserror);
				}
				return false;
			}

			return setbooleanvalue(true, vreturned);
		}

		case filedeletefunc: {
			/* Delete file or folder */
			tyfilespec fs;
			char path[4096];
			struct stat st;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			if (!filespec_to_cstring(&fs, path, sizeof(path)))
				return false;

			/* Check if path exists and whether it's a directory */
			if (stat(path, &st) != 0) {
				copyctopstring("File not found", bserror);
				return false;
			}

			/* Use rmdir for directories, unlink for files */
			if (S_ISDIR(st.st_mode)) {
				if (rmdir(path) != 0) {
					if (errno == ENOTEMPTY) {
						copyctopstring("Folder not empty", bserror);
					} else {
						copyctopstring("Can't delete folder", bserror);
					}
					return false;
				}
			} else {
				if (unlink(path) != 0) {
					copyctopstring("Can't delete file", bserror);
					return false;
				}
			}

			return setbooleanvalue(true, vreturned);
		}

		case filerenamefunc: {
			/* Rename file or folder */
			tyfilespec fsold, fsnew;
			char oldpath[4096], newpath[4096];

			if (!getfilespecvalue(hparam1, 1, &fsold))
				return false;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 2, &fsnew))
				return false;

			if (!filespec_to_cstring(&fsold, oldpath, sizeof(oldpath)))
				return false;

			if (!filespec_to_cstring(&fsnew, newpath, sizeof(newpath)))
				return false;

			/* Use rename() system call */
			if (rename(oldpath, newpath) != 0) {
				if (errno == ENOENT) {
					copyctopstring("File not found", bserror);
				} else if (errno == EEXIST || errno == ENOTEMPTY) {
					copyctopstring("Destination already exists", bserror);
				} else if (errno == EXDEV) {
					copyctopstring("Can't rename across volumes", bserror);
				} else {
					copyctopstring("Can't rename file", bserror);
				}
				return false;
			}

			return setbooleanvalue(true, vreturned);
		}

		case filecopyfunc: {
			/* Copy file from source to destination */
			tyfilespec fssrc, fsdest;
			char srcpath[4096], destpath[4096];
			FILE *fpsrc = NULL, *fpdest = NULL;
			char buffer[8192];
			size_t bytes_read;
			struct stat st;
			boolean success = false;

			if (!getfilespecvalue(hparam1, 1, &fssrc))
				return false;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 2, &fsdest))
				return false;

			if (!filespec_to_cstring(&fssrc, srcpath, sizeof(srcpath)))
				return false;

			if (!filespec_to_cstring(&fsdest, destpath, sizeof(destpath)))
				return false;

			/* Check source exists and get permissions */
			if (stat(srcpath, &st) != 0) {
				copyctopstring("Source file not found", bserror);
				return false;
			}

			/* Open source for reading */
			fpsrc = fopen(srcpath, "rb");
			if (!fpsrc) {
				copyctopstring("Can't open source file", bserror);
				return false;
			}

			/* Open destination for writing */
			fpdest = fopen(destpath, "wb");
			if (!fpdest) {
				fclose(fpsrc);
				copyctopstring("Can't create destination file", bserror);
				return false;
			}

			/* Copy data in chunks */
			while ((bytes_read = fread(buffer, 1, sizeof(buffer), fpsrc)) > 0) {
				if (fwrite(buffer, 1, bytes_read, fpdest) != bytes_read) {
					copyctopstring("Write error during copy", bserror);
					goto cleanup;
				}
			}

			/* Check for read error */
			if (ferror(fpsrc)) {
				copyctopstring("Read error during copy", bserror);
				goto cleanup;
			}

			success = true;

		cleanup:
			if (fpsrc)
				fclose(fpsrc);
			if (fpdest)
				fclose(fpdest);

			/* Preserve permissions on success */
			if (success) {
				if (chmod(destpath, st.st_mode & 0777) != 0) {
					log_warn(LOG_COMP_LANG, "file.copy: chmod failed for %s (errno=%d)",
					        destpath, errno);
					/* Continue - copy succeeded even if permission preservation failed */
				}
			}

			if (!success)
				return false;

			return setbooleanvalue(true, vreturned);
		}

		case filemovefunc: {
			/* Move file from source to destination */
			tyfilespec fssrc, fsdest;
			char srcpath[4096], destpath[4096];
			struct stat st;

			if (!getfilespecvalue(hparam1, 1, &fssrc))
				return false;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 2, &fsdest))
				return false;

			if (!filespec_to_cstring(&fssrc, srcpath, sizeof(srcpath)))
				return false;

			if (!filespec_to_cstring(&fsdest, destpath, sizeof(destpath)))
				return false;

			/* Check source exists */
			if (stat(srcpath, &st) != 0) {
				copyctopstring("Source file not found", bserror);
				return false;
			}

			/* Try rename() first (efficient for same volume) */
			if (rename(srcpath, destpath) == 0) {
				return setbooleanvalue(true, vreturned);
			}

			/* If cross-volume (EXDEV), fall back to copy+delete */
			if (errno == EXDEV) {
				FILE *fpsrc = NULL, *fpdest = NULL;
				char buffer[8192];
				size_t bytes_read;

				/* Open source for reading */
				fpsrc = fopen(srcpath, "rb");
				if (!fpsrc) {
					copyctopstring("Can't open source file", bserror);
					return false;
				}

				/* Open destination for writing */
				fpdest = fopen(destpath, "wb");
				if (!fpdest) {
					fclose(fpsrc);
					copyctopstring("Can't create destination file", bserror);
					return false;
				}

				/* Copy data */
				while ((bytes_read = fread(buffer, 1, sizeof(buffer), fpsrc)) > 0) {
					if (fwrite(buffer, 1, bytes_read, fpdest) != bytes_read) {
						copyctopstring("Write error during move", bserror);
						fclose(fpsrc);
						fclose(fpdest);
						return false;
					}
				}

				/* Check for read error BEFORE closing */
				if (ferror(fpsrc)) {
					fclose(fpsrc);
					fclose(fpdest);
					copyctopstring("Read error during move", bserror);
					return false;
				}

				fclose(fpsrc);
				fclose(fpdest);

				/* Preserve permissions */
				if (chmod(destpath, st.st_mode & 0777) != 0) {
					log_warn(LOG_COMP_LANG, "file.move: chmod failed for %s (errno=%d)",
					        destpath, errno);
					/* Continue - copy succeeded even if permission preservation failed */
				}

				/* Delete source on success */
				if (unlink(srcpath) != 0) {
					copyctopstring("Can't delete source after copy", bserror);
					return false;
				}

				return setbooleanvalue(true, vreturned);
			}

			/* Other rename errors */
			if (errno == ENOENT) {
				copyctopstring("File not found", bserror);
			} else if (errno == EEXIST || errno == ENOTEMPTY) {
				copyctopstring("Destination already exists", bserror);
			} else {
				copyctopstring("Can't move file", bserror);
			}

			return false;
		}

		case getsystempathfunc:
		case getspecialpathfunc: {
			/* Get system or special folder path - map OSType codes to Unix paths */
			OSType foldertype;
			const char *folderpath = NULL;
			char resolved[4096];
			const char *home;
			bigstring bspath;
			tyfilespec fs;

			flnextparamislast = true;

			if (!getostypevalue(hparam1, 1, &foldertype))
				return false;

			/* Get home directory for user-specific paths */
			home = getenv("HOME");
			if (!home)
				home = "/tmp";

			/* Map OSType codes to Unix paths */
			switch (foldertype) {
				case 'desk':  /* Desktop */
					snprintf(resolved, sizeof(resolved), "%s/Desktop", home);
					folderpath = resolved;
					break;

				case 'docs':  /* Documents */
					snprintf(resolved, sizeof(resolved), "%s/Documents", home);
					folderpath = resolved;
					break;

				case 'temp':  /* Temporary Items */
					folderpath = "/tmp";
					break;

				case 'pref':  /* Preferences */
					#ifdef __APPLE__
						snprintf(resolved, sizeof(resolved), "%s/Library/Preferences", home);
					#else
						snprintf(resolved, sizeof(resolved), "%s/.config", home);
					#endif
					folderpath = resolved;
					break;

				case 'home':  /* Home directory */
					folderpath = home;
					break;

				case 'strt':  /* Startup (system boot directory) */
					#ifdef __APPLE__
						folderpath = "/Library/StartupItems";
					#else
						folderpath = "/etc/init.d";
					#endif
					break;

				case 'apps':  /* Applications */
					#ifdef __APPLE__
						folderpath = "/Applications";
					#else
						folderpath = "/usr/bin";
					#endif
					break;

				case 'font':  /* Fonts */
					#ifdef __APPLE__
						snprintf(resolved, sizeof(resolved), "%s/Library/Fonts", home);
					#else
						snprintf(resolved, sizeof(resolved), "%s/.fonts", home);
					#endif
					folderpath = resolved;
					break;

				default:
					/* Unknown folder type - return home directory as fallback */
					folderpath = home;
					break;
			}

			/* Convert C string to bigstring */
			size_t len = strlen(folderpath);
			if (len > 255)
				len = 255;
			bspath[0] = (unsigned char)len;
			memcpy(&bspath[1], folderpath, len);

			/* Convert to filespec and return */
			if (!pathtofilespec(bspath, &fs))
				return false;

			return setfilespecvalue(&fs, vreturned);
		}

		case getpathcharfunc: {
			/* Return '/' as the path separator character */
			if (!langcheckparamcount(hparam1, 0))
				return false;

			return setstringvalue(BIGSTRING("\x01/"), vreturned);
		}

		/* Tier 2: File I/O operations (14 verbs) */

		case openfilefunc: {
			/* Open file and return refnum */
			tyfilespec fs;
			char path[4096];
			char mode[4] = "r+b"; /* Default: read/write binary */
			boolean flreadonly = false;
			FILE *fp;
			short refnum;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			/* Optional second parameter: readonly flag */
			if (langgetparamcount(hparam1) >= 2) {
				flnextparamislast = true;
				if (!getbooleanvalue(hparam1, 2, &flreadonly))
					return false;
			} else {
				flnextparamislast = true;
			}

			if (!filespec_to_cstring(&fs, path, sizeof(path)))
				return false;

			/* Set mode based on readonly flag */
			if (flreadonly) {
				fp = fopen(path, "rb");
			} else {
				/* Try r+b first, fall back to w+b if file doesn't exist */
				fp = fopen(path, "r+b");
				if (!fp) {
					fp = fopen(path, "w+b");
				}
			}

			if (!fp) {
				copyctopstring("Can't open file", bserror);
				return false;
			}

			refnum = allocate_filehandle(fp);
			if (refnum == 0) {
				fclose(fp);
				copyctopstring("Too many open files", bserror);
				return false;
			}

			return setlongvalue(refnum, vreturned);
		}

		case closefilefunc: {
			/* Close file by refnum (release_filehandle handles fclose) */
			long refnum;

			flnextparamislast = true;

			if (!getlongvalue(hparam1, 1, &refnum))
				return false;

			if (!release_filehandle((short)refnum)) {
				copyctopstring("Invalid file refnum", bserror);
				return false;
			}

			return setbooleanvalue(true, vreturned);
		}

		case endoffilefunc: {
			/* Check if at end of file */
			long refnum;
			FILE *fp;

			flnextparamislast = true;

			if (!getlongvalue(hparam1, 1, &refnum))
				return false;

			fp = get_filepointer((short)refnum);
			if (!fp) {
				copyctopstring("Invalid file refnum", bserror);
				return false;
			}

			return setbooleanvalue(feof(fp) != 0, vreturned);
		}

		case setendoffilefunc: {
			/* Truncate file at current position */
			long refnum;
			FILE *fp;
			long pos;
			int fd;

			flnextparamislast = true;

			if (!getlongvalue(hparam1, 1, &refnum))
				return false;

			fp = get_filepointer((short)refnum);
			if (!fp) {
				copyctopstring("Invalid file refnum", bserror);
				return false;
			}

			pos = ftell(fp);
			if (pos < 0) {
				copyctopstring("Can't get file position", bserror);
				return false;
			}

			fd = fileno(fp);
			if (ftruncate(fd, pos) != 0) {
				copyctopstring("Can't truncate file", bserror);
				return false;
			}

			return setbooleanvalue(true, vreturned);
		}

		case getendoffilefunc: {
			/* Get file size */
			long refnum;
			FILE *fp;
			long current, size;

			flnextparamislast = true;

			if (!getlongvalue(hparam1, 1, &refnum))
				return false;

			fp = get_filepointer((short)refnum);
			if (!fp) {
				copyctopstring("Invalid file refnum", bserror);
				return false;
			}

			current = ftell(fp);
			if (current < 0) {
				copyctopstring("Can't get current file position", bserror);
				return false;
			}

			if (fseek(fp, 0, SEEK_END) != 0) {
				copyctopstring("Can't seek to end of file", bserror);
				return false;
			}

			size = ftell(fp);
			if (size < 0) {
				copyctopstring("Can't get file size", bserror);
				return false;
			}

			if (fseek(fp, current, SEEK_SET) != 0) {
				copyctopstring("Can't restore file position", bserror);
				return false;
			}

			return setlongvalue(size, vreturned);
		}

		case setpositionfunc: {
			/* Set file position */
			long refnum, position;
			FILE *fp;

			if (!getlongvalue(hparam1, 1, &refnum))
				return false;

			flnextparamislast = true;

			if (!getlongvalue(hparam1, 2, &position))
				return false;

			fp = get_filepointer((short)refnum);
			if (!fp) {
				copyctopstring("Invalid file refnum", bserror);
				return false;
			}

			if (fseek(fp, position, SEEK_SET) != 0) {
				copyctopstring("Can't set file position", bserror);
				return false;
			}

			return setbooleanvalue(true, vreturned);
		}

		case getpositionfunc: {
			/* Get current file position */
			long refnum;
			FILE *fp;
			long position;

			flnextparamislast = true;

			if (!getlongvalue(hparam1, 1, &refnum))
				return false;

			fp = get_filepointer((short)refnum);
			if (!fp) {
				copyctopstring("Invalid file refnum", bserror);
				return false;
			}

			position = ftell(fp);
			if (position < 0) {
				copyctopstring("Can't get file position", bserror);
				return false;
			}

			return setlongvalue(position, vreturned);
		}

		case readlinefunc: {
			/* Read a line from file */
			long refnum;
			FILE *fp;
			bigstring bsline;
			int ch;
			int len = 0;

			flnextparamislast = true;

			if (!getlongvalue(hparam1, 1, &refnum))
				return false;

			fp = get_filepointer((short)refnum);
			if (!fp) {
				copyctopstring("Invalid file refnum", bserror);
				return false;
			}

			/* Read until newline or EOF (pre-increment ensures max 255 bytes) */
			while (len < 255 && (ch = fgetc(fp)) != EOF && ch != '\n' && ch != '\r') {
				bsline[++len] = (unsigned char)ch;  /* Pre-increment: writes to indices 1-255 */
			}

			/* Handle CR/LF combinations */
			if (ch == '\r') {
				int next = fgetc(fp);
				if (next != '\n' && next != EOF) {
					ungetc(next, fp);
				}
			}

			bsline[0] = (unsigned char)len;

			return setstringvalue(bsline, vreturned);
		}

		case writelinefunc: {
			/* Write a line to file */
			long refnum;
			FILE *fp;
			bigstring bsline;

			if (!getlongvalue(hparam1, 1, &refnum))
				return false;

			flnextparamislast = true;

			if (!getstringvalue(hparam1, 2, bsline))
				return false;

			fp = get_filepointer((short)refnum);
			if (!fp) {
				copyctopstring("Invalid file refnum", bserror);
				return false;
			}

			/* Write string */
			if (bsline[0] > 0) {
				if (fwrite(&bsline[1], 1, bsline[0], fp) != bsline[0]) {
					copyctopstring("Write error", bserror);
					return false;
				}
			}

			/* Write newline */
			if (fputc('\n', fp) == EOF) {
				copyctopstring("Write error", bserror);
				return false;
			}

			return setbooleanvalue(true, vreturned);
		}

		case readfunc: {
			/* Read bytes from file */
			long refnum, count;
			FILE *fp;
			Handle hdata;
			unsigned char *buffer;
			size_t bytesread;

			if (!getlongvalue(hparam1, 1, &refnum))
				return false;

			flnextparamislast = true;

			if (!getlongvalue(hparam1, 2, &count))
				return false;

			fp = get_filepointer((short)refnum);
			if (!fp) {
				copyctopstring("Invalid file refnum", bserror);
				return false;
			}

			if (count <= 0) {
				return setstringvalue(BIGSTRING("\x00"), vreturned);
			}

			/* For small reads (<=255 bytes), return as string */
			if (count <= 255) {
				bigstring bs;
				bytesread = fread(&bs[1], 1, count, fp);
				bs[0] = (unsigned char)bytesread;
				return setstringvalue(bs, vreturned);
			}

			/* For larger reads, return as binary */
			if (!newhandle(count, &hdata)) {
				copyctopstring("Out of memory", bserror);
				return false;
			}

			lockhandle(hdata);
			buffer = (unsigned char *)*hdata;
			bytesread = fread(buffer, 1, count, fp);
			unlockhandle(hdata);

			if (bytesread == 0) {
				disposehandle(hdata);
				return setstringvalue(BIGSTRING("\x00"), vreturned);
			}

			return setbinaryvalue(hdata, bytesread, vreturned);
		}

		case writefunc: {
			/* Write bytes to file */
			long refnum;
			FILE *fp;
			bigstring bs;

			if (!getlongvalue(hparam1, 1, &refnum))
				return false;

			flnextparamislast = true;

			if (!getstringvalue(hparam1, 2, bs))
				return false;

			fp = get_filepointer((short)refnum);
			if (!fp) {
				copyctopstring("Invalid file refnum", bserror);
				return false;
			}

			/* Write string data */
			if (bs[0] > 0) {
				if (fwrite(&bs[1], 1, bs[0], fp) != bs[0]) {
					copyctopstring("Write error", bserror);
					return false;
				}
			}

			return setlongvalue(bs[0], vreturned);
		}

		case readwholefilefunc: {
			/* Read entire file into string/binary */
			tyfilespec fs;
			char path[4096];
			FILE *fp = NULL;
			long filesize;
			Handle hdata;
			unsigned char *buffer;
			bigstring bsdata;
			size_t bytesread;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			if (!filespec_to_cstring(&fs, path, sizeof(path)))
				return false;

			fp = fopen(path, "rb");
			if (!fp) {
				copyctopstring("Can't open file", bserror);
				return false;
			}

			/* Get file size */
			if (fseek(fp, 0, SEEK_END) != 0) {
				fclose(fp);
				copyctopstring("Can't seek to end of file", bserror);
				return false;
			}

			filesize = ftell(fp);
			if (filesize < 0) {
				fclose(fp);
				copyctopstring("Can't get file size", bserror);
				return false;
			}

			if (fseek(fp, 0, SEEK_SET) != 0) {
				fclose(fp);
				copyctopstring("Can't seek to beginning of file", bserror);
				return false;
			}

			/* Empty file */
			if (filesize == 0) {
				fclose(fp);
				return setstringvalue(BIGSTRING("\x00"), vreturned);
			}

			/* Check file size limit (500MB) */
			if (filesize > MAX_READWHOLEFILE_SIZE) {
				fclose(fp);
				copyctopstring("File too large (exceeds 500MB limit)", bserror);
				return false;
			}

			/* Small file - return as string */
			if (filesize <= 255) {
				bytesread = fread(&bsdata[1], 1, filesize, fp);
				fclose(fp);
				bsdata[0] = (unsigned char)bytesread;
				return setstringvalue(bsdata, vreturned);
			}

			/* Large file - return as binary */
			if (!newhandle(filesize, &hdata)) {
				fclose(fp);
				copyctopstring("Out of memory", bserror);
				return false;
			}

			lockhandle(hdata);
			buffer = (unsigned char *)*hdata;
			bytesread = fread(buffer, 1, filesize, fp);
			unlockhandle(hdata);
			fclose(fp);

			if (bytesread != filesize) {
				disposehandle(hdata);
				copyctopstring("Read error", bserror);
				return false;
			}

			return setbinaryvalue(hdata, filesize, vreturned);
		}

		case writewholefilefunc: {
			/* Write entire string or binary data to file */
			tyfilespec fs;
			char path[4096];
			FILE *fp = NULL;
			Handle hdata = NULL;
			long datasize;
			unsigned char *buffer;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			flnextparamislast = true;

			if (!getexempttextvalue(hparam1, 2, &hdata))
				return false;

			if (!filespec_to_cstring(&fs, path, sizeof(path))) {
				disposehandle(hdata);
				return false;
			}

			fp = fopen(path, "wb");
			if (!fp) {
				disposehandle(hdata);
				copyctopstring("Can't create file", bserror);
				return false;
			}

			/* Write handle data */
			datasize = gethandlesize(hdata);
			if (datasize > 0) {
				lockhandle(hdata);
				buffer = (unsigned char *)*hdata;
				if (fwrite(buffer, 1, datasize, fp) != datasize) {
					unlockhandle(hdata);
					disposehandle(hdata);
					fclose(fp);
					copyctopstring("Write error", bserror);
					return false;
				}
				unlockhandle(hdata);
			}

			disposehandle(hdata);
			fclose(fp);
			return setbooleanvalue(true, vreturned);
		}

		case comparefunc: {
			/* Compare two files byte-by-byte - returns true if identical */
			tyfilespec fs1, fs2;
			char path1[4096], path2[4096];
			FILE *fp1 = NULL, *fp2 = NULL;
			int ch1, ch2;
			boolean equal = true;

			if (!getfilespecvalue(hparam1, 1, &fs1))
				return false;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 2, &fs2))
				return false;

			if (!filespec_to_cstring(&fs1, path1, sizeof(path1)))
				return false;

			if (!filespec_to_cstring(&fs2, path2, sizeof(path2)))
				return false;

			fp1 = fopen(path1, "rb");
			if (!fp1) {
				copyctopstring("Can't open first file", bserror);
				return false;
			}

			fp2 = fopen(path2, "rb");
			if (!fp2) {
				fclose(fp1);
				copyctopstring("Can't open second file", bserror);
				return false;
			}

			/* Compare byte by byte */
			while ((ch1 = fgetc(fp1)) != EOF) {
				ch2 = fgetc(fp2);
				if (ch1 != ch2) {
					equal = false;
					break;
				}
			}

			/* Check if second file has more data */
			if (equal && fgetc(fp2) != EOF) {
				equal = false;
			}

			fclose(fp1);
			fclose(fp2);

			return setbooleanvalue(equal, vreturned);
		}

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
