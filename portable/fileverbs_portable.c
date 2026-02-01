/*
 * fileverbs_portable.c - Portable file verb implementations for headless mode
 *
 * Implements 86 file verbs using POSIX APIs instead of Mac-specific CoreFoundation.
 * This file is linked in headless builds instead of Common/source/fileverbs.c.
 *
 * Implementation status (34/86 verbs = 40% coverage):
 * - Phase 1: Infrastructure and stubs - COMPLETE ✅
 * - Phase 2: Tier 1 critical operations (20 verbs) - COMPLETE ✅
 * - Phase 2: Tier 2 file I/O (14 verbs) - COMPLETE ✅
 * - Phase 3: Tier 3 optional features (16 verbs) - TODO
 * - Phase 4: Tier 4 Mac-specific stubs (36 verbs) - STUBBED
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
#include <sys/statvfs.h>
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
 * Returns frontier_time_t (64-bit) per docs/frontier_time_t_standard.md
 */
static inline frontier_time_t timet_to_frontierseconds(time_t unixtime) {
	return (frontier_time_t)(unixtime + FRONTIER_EPOCH_OFFSET);
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
	int refcount;  /* Reference count for thread-safe FILE* access */
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
			filetable[i].refcount = 0;  /* No active users yet */
			result = filetable[i].refnum;
			break;
		}
	}

	pthread_mutex_unlock(&filetable_mutex);
	return result; /* Returns refnum or 0 if no free handles */
}

/*
 * Get FILE* from refnum with reference counting (O(1) direct lookup)
 * MUST call release_filepointer() when done to avoid leaking references.
 * This prevents race condition where FILE* is closed by another thread.
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
		filetable[i].refcount++;  /* Increment under lock to prevent close */
	}

	pthread_mutex_unlock(&filetable_mutex);
	return result;
}

/*
 * Release reference to FILE* obtained from get_filepointer()
 * MUST be called after using FILE* to allow cleanup.
 * If this is the last reference and handle was marked for close, performs cleanup.
 */
static void release_filepointer(short refnum) {
	FILE *fp_to_close = NULL;

	/* Validate refnum range */
	if (refnum < 1 || refnum > MAX_OPEN_FILES) {
		return;
	}

	pthread_mutex_lock(&filetable_mutex);

	int i = refnum - 1;
	if (filetable[i].refnum == refnum && filetable[i].refcount > 0) {
		filetable[i].refcount--;

		/* If this was the last reference and handle is marked for close, clean up */
		if (filetable[i].refcount == 0 && !filetable[i].inuse) {
			fp_to_close = filetable[i].fp;
			filetable[i].fp = NULL;
			filetable[i].refnum = 0;
		}
	}

	pthread_mutex_unlock(&filetable_mutex);

	/* Close outside mutex to avoid blocking I/O under lock */
	if (fp_to_close) {
		fclose(fp_to_close);
	}
}

/*
 * Release file handle (O(1) direct lookup)
 * Only closes FILE* when refcount == 0 (no active users).
 * Prevents race condition where FILE* is in use by another thread.
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
		if (filetable[i].refcount == 0) {
			/* No active users - safe to close immediately */
			fp_to_close = filetable[i].fp;
			filetable[i].inuse = false;
			filetable[i].fp = NULL;
			filetable[i].refnum = 0;
		} else {
			/* Active users - mark for close but don't close yet */
			filetable[i].inuse = false;  /* Prevent new references */
			/* FILE* will be closed when last reference is released */
		}
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
			/* Check if path is a mount point (volume root) */
			tyfilespec fs;
			char path[4096];
			struct stat st, parent_st;
			char parent_path[4096];

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			if (!filespec_to_cstring(&fs, path, sizeof(path))) {
				copyctopstring("Invalid file path", bserror);
				return false;
			}

			/* Root directory is always a volume */
			if (strcmp(path, "/") == 0)
				return setbooleanvalue(true, vreturned);

			/* Get stat info for the path */
			if (stat(path, &st) != 0)
				return setbooleanvalue(false, vreturned);

			/* Not a directory = not a volume */
			if (!S_ISDIR(st.st_mode))
				return setbooleanvalue(false, vreturned);

			/* Get parent directory path */
			snprintf(parent_path, sizeof(parent_path), "%s/..", path);

			/* Get stat info for parent directory */
			if (stat(parent_path, &parent_st) != 0)
				return setbooleanvalue(false, vreturned);

			/* If device ID differs from parent, this is a mount point */
			boolean is_mountpoint = (st.st_dev != parent_st.st_dev);

			return setbooleanvalue(is_mountpoint, vreturned);
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
			frontier_time_t frontierseconds;

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
			frontier_time_t frontierseconds;

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
				home = ".";  /* Fallback to current directory (sandbox-safe) */

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
					folderpath = "./tmp";  /* Relative to cwd (sandbox-safe) */
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

			(void)mode;  /* Used in conditional logic below */

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
			boolean iseof;

			flnextparamislast = true;

			if (!getlongvalue(hparam1, 1, &refnum))
				return false;

			fp = get_filepointer((short)refnum);
			if (!fp) {
				copyctopstring("Invalid file refnum", bserror);
				return false;
			}

			iseof = (feof(fp) != 0);
			release_filepointer((short)refnum);
			return setbooleanvalue(iseof, vreturned);
		}

		case setendoffilefunc: {
			/* Truncate file at current position */
			long refnum;
			FILE *fp;
			long pos;
			int fd;
			boolean success;

			(void)success;  /* Used for error handling flow */

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
				release_filepointer((short)refnum);
				copyctopstring("Can't get file position", bserror);
				return false;
			}

			fd = fileno(fp);
			if (ftruncate(fd, pos) != 0) {
				release_filepointer((short)refnum);
				copyctopstring("Can't truncate file", bserror);
				return false;
			}

			release_filepointer((short)refnum);
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
				release_filepointer((short)refnum);
				copyctopstring("Can't get current file position", bserror);
				return false;
			}

			if (fseek(fp, 0, SEEK_END) != 0) {
				release_filepointer((short)refnum);
				copyctopstring("Can't seek to end of file", bserror);
				return false;
			}

			size = ftell(fp);
			if (size < 0) {
				release_filepointer((short)refnum);
				copyctopstring("Can't get file size", bserror);
				return false;
			}

			if (fseek(fp, current, SEEK_SET) != 0) {
				release_filepointer((short)refnum);
				copyctopstring("Can't restore file position", bserror);
				return false;
			}

			release_filepointer((short)refnum);
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
				release_filepointer((short)refnum);
				copyctopstring("Can't set file position", bserror);
				return false;
			}

			release_filepointer((short)refnum);
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
				release_filepointer((short)refnum);
				copyctopstring("Can't get file position", bserror);
				return false;
			}

			release_filepointer((short)refnum);
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

			release_filepointer((short)refnum);
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
					release_filepointer((short)refnum);
					copyctopstring("Write error", bserror);
					return false;
				}
			}

			/* Write newline */
			if (fputc('\n', fp) == EOF) {
				release_filepointer((short)refnum);
				copyctopstring("Write error", bserror);
				return false;
			}

			release_filepointer((short)refnum);
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
				release_filepointer((short)refnum);
				return setstringvalue(BIGSTRING("\x00"), vreturned);
			}

			/* For small reads (<=255 bytes), return as string */
			if (count <= 255) {
				bigstring bs;
				bytesread = fread(&bs[1], 1, count, fp);
				bs[0] = (unsigned char)bytesread;
				release_filepointer((short)refnum);
				return setstringvalue(bs, vreturned);
			}

			/* For larger reads, return as binary */
			if (!newhandle(count, &hdata)) {
				release_filepointer((short)refnum);
				copyctopstring("Out of memory", bserror);
				return false;
			}

			lockhandle(hdata);
			buffer = (unsigned char *)*hdata;
			bytesread = fread(buffer, 1, count, fp);
			unlockhandle(hdata);

			if (bytesread == 0) {
				disposehandle(hdata);
				release_filepointer((short)refnum);
				return setstringvalue(BIGSTRING("\x00"), vreturned);
			}

			release_filepointer((short)refnum);
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
					release_filepointer((short)refnum);
					copyctopstring("Write error", bserror);
					return false;
				}
			}

			release_filepointer((short)refnum);
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

	case filetypefunc: {
		/* Portable implementation for headless mode:
		 * Return file extension with dot prefix (e.g., ".txt")
		 * No extension: Return empty string
		 */
		tyfilespec fs;
		char path[4096];
		bigstring bspath, bsfilename, bsext, bsresult;
		short i, lastslash, lastdot;

		(void)bsext;  /* Reserved for future extension parsing */

		flnextparamislast = true;

		if (!getfilespecvalue(hparam1, 1, &fs))
			return false;

		/* Convert filespec to full path string */
		if (!filespec_to_cstring(&fs, path, sizeof(path))) {
			setemptystring(bsresult);
			return setstringvalue(bsresult, vreturned);
		}

		/* Convert C string to bigstring */
		size_t pathlen = strlen(path);
		if (pathlen > 255) pathlen = 255;
		bspath[0] = (unsigned char)pathlen;
		memcpy(&bspath[1], path, pathlen);

		/* Find last path separator ('/' or ':') */
		lastslash = -1;
		for (i = 1; i <= bspath[0]; i++) {
			if (bspath[i] == '/' || bspath[i] == ':')
				lastslash = i;
		}

		/* Extract filename (everything after last separator) */
		if (lastslash >= 0) {
			short filenamelen = bspath[0] - lastslash;
			bsfilename[0] = filenamelen;
			memcpy(&bsfilename[1], &bspath[lastslash + 1], filenamelen);
		} else {
			copystring(bspath, bsfilename);
		}

		/* Find last dot in filename */
		lastdot = -1;
		for (i = 1; i <= bsfilename[0]; i++) {
			if (bsfilename[i] == '.')
				lastdot = i;
		}

		/* If no dot found, return empty string */
		if (lastdot < 0 || lastdot == bsfilename[0]) {
			setemptystring(bsresult);
			return setstringvalue(bsresult, vreturned);
		}

		/* Extract extension and prepend dot */
		short extlen = bsfilename[0] - lastdot;
		bsresult[0] = extlen + 1;  /* +1 for the dot */
		bsresult[1] = '.';
		memcpy(&bsresult[2], &bsfilename[lastdot + 1], extlen);

		/* Windows port compatibility: return string4Type for ≤4 chars, stringType for >4 */
		if (bsresult[0] <= 4) {
			/* Extension fits in 4 bytes - use string4Type with space padding */
			bigstring bs4;
			short i;

			/* Copy extension */
			for (i = 0; i < bsresult[0]; i++) {
				bs4[i] = bsresult[i + 1];
			}

			/* Pad with spaces to 4 bytes */
			for (; i < 4; i++) {
				bs4[i] = ' ';
			}

			/* Pack as OSType (4 bytes) */
			OSType typecode = (((unsigned long)bs4[0]) << 24) |
			                  (((unsigned long)bs4[1]) << 16) |
			                  (((unsigned long)bs4[2]) << 8) |
			                  ((unsigned long)bs4[3]);

			return setostypevalue(typecode, vreturned);
		} else {
			/* Extension >4 chars - return as string */
			return setstringvalue(bsresult, vreturned);
		}
	}

	case filecreatorfunc: {
		/* Portable implementation for headless mode:
		 * Creator codes are Mac-specific - return empty string on all platforms
		 */
		tyfilespec fs;
		bigstring bsempty;

		flnextparamislast = true;

		if (!getfilespecvalue(hparam1, 1, &fs))
			return false;

		/* Return empty string (no creator code concept in portable mode) */
		setemptystring(bsempty);
		return setstringvalue(bsempty, vreturned);
	}

		case fileislockedfunc: {
			/* file.islocked - Platform-specific file locking not implemented
			 *
			 * See file.lock comments for rationale.
			 */
			flnextparamislast = true;

			if (bserror)
				copyctopstring("file.islocked is not implemented on this platform", bserror);

			return false;
		}

		case filelockfunc: {
			/* file.lock - Platform-specific file locking not implemented
			 *
			 * Classic Frontier used Mac-specific file system flags (kFSNodeLockedMask).
			 * Cross-platform file locking is complex and varies by OS:
			 * - macOS: chflags with UF_IMMUTABLE (requires elevated privileges)
			 * - Linux: flock/fcntl (different semantics, process-scoped)
			 * - Windows: LockFile (different API entirely)
			 *
			 * This verb is rarely used. Defer proper cross-platform implementation.
			 */
			flnextparamislast = true;

			if (bserror)
				copyctopstring("file.lock is not implemented on this platform", bserror);

			return false;
		}

		case fileunlockfunc: {
			/* file.unlock - Platform-specific file locking not implemented
			 *
			 * See file.lock comments for rationale.
			 */
			flnextparamislast = true;

			if (bserror)
				copyctopstring("file.unlock is not implemented on this platform", bserror);

			return false;
		}

		case setfilecreatedfunc: {
			/* Set file creation time (macOS only, returns false on Linux) */
			tyfilespec fs;
			frontier_time_t created;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			flnextparamislast = true;

			if (!getdatevalue(hparam1, 2, &created))
				return false;

			boolean result = setfilecreated(&fs, created);
			return setbooleanvalue(result, vreturned);
		}

		case setfilemodifiedfunc: {
			/* Set file modification time */
			tyfilespec fs;
			frontier_time_t modified;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			flnextparamislast = true;

			if (!getdatevalue(hparam1, 2, &modified))
				return false;

			boolean result = setfilemodified(&fs, modified);
			return setbooleanvalue(result, vreturned);
		}

		case setfiletypefunc:
		case setfilecreatorfunc:
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
		case sfgetdiskfunc: {
			#ifdef FRONTIER_HEADLESS
				/* Phase 2B: Interactive file dialogs in headless mode */
				extern boolean isInteractiveMode(void);
				extern boolean portable_file_dialog_verb(short token, hdltreenode hparam1,
				                                         tyvaluerecord *vreturned, bigstring bserror);

				if (!isInteractiveMode()) {
					copyctopstring("File dialogs require interactive mode (TTY) - use explicit paths in batch mode", bserror);
					return false;
				}

				return portable_file_dialog_verb(token, hparam1, vreturned, bserror);
			#else
				copyctopstring("File dialogs not supported in headless mode - use explicit paths", bserror);
				return false;
			#endif
		}

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

		case volumefreespacefunc: {
			/* Return free space available to non-root users (as long, may overflow) */
			tyfilespec fs;
			char path[4096];
			struct statvfs vfs;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			if (!filespec_to_cstring(&fs, path, sizeof(path))) {
				copyctopstring("Invalid file path", bserror);
				return false;
			}

			if (statvfs(path, &vfs) != 0) {
				copyctopstring("Unable to get volume information", bserror);
				return false;
			}

			/* Free space = available blocks * fragment size */
			unsigned long long free_size = (unsigned long long)vfs.f_bavail * vfs.f_frsize;

			/* Return as long (may overflow for large free space) */
			return setlongvalue((long)free_size, vreturned);
		}

		case volumesizefunc: {
			/* Return total volume size in bytes (as long, may overflow for large volumes) */
			tyfilespec fs;
			char path[4096];
			struct statvfs vfs;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			if (!filespec_to_cstring(&fs, path, sizeof(path))) {
				copyctopstring("Invalid file path", bserror);
				return false;
			}

			if (statvfs(path, &vfs) != 0) {
				copyctopstring("Unable to get volume information", bserror);
				return false;
			}

			/* Total size = total blocks * fragment size */
			unsigned long long total_size = (unsigned long long)vfs.f_blocks * vfs.f_frsize;

			/* Return as long (may overflow for volumes > 2GB) */
			return setlongvalue((long)total_size, vreturned);
		}

		case volumesizedoublefunc: {
			/* Return total volume size in bytes (as double, handles large volumes) */
			tyfilespec fs;
			char path[4096];
			struct statvfs vfs;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			if (!filespec_to_cstring(&fs, path, sizeof(path))) {
				copyctopstring("Invalid file path", bserror);
				return false;
			}

			if (statvfs(path, &vfs) != 0) {
				copyctopstring("Unable to get volume information", bserror);
				return false;
			}

			/* Total size = total blocks * fragment size */
			double total_size = (double)vfs.f_blocks * vfs.f_frsize;

			return setdoublevalue(total_size, vreturned);
		}

		case volumefreespacedoublefunc: {
			/* Return free space available to non-root users (as double) */
			tyfilespec fs;
			char path[4096];
			struct statvfs vfs;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			if (!filespec_to_cstring(&fs, path, sizeof(path))) {
				copyctopstring("Invalid file path", bserror);
				return false;
			}

			if (statvfs(path, &vfs) != 0) {
				copyctopstring("Unable to get volume information", bserror);
				return false;
			}

			/* Free space available to non-root = available blocks * fragment size */
			double free_space = (double)vfs.f_bavail * vfs.f_frsize;

			return setdoublevalue(free_space, vreturned);
		}

		case volumeblocksizefunc: {
			/* Return volume block size in bytes */
			tyfilespec fs;
			char path[4096];
			struct statvfs vfs;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			if (!filespec_to_cstring(&fs, path, sizeof(path))) {
				copyctopstring("Invalid file path", bserror);
				return false;
			}

			if (statvfs(path, &vfs) != 0) {
				copyctopstring("Unable to get volume information", bserror);
				return false;
			}

			/* Return preferred block size for I/O operations */
			return setlongvalue((long)vfs.f_bsize, vreturned);
		}

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

#ifdef FRONTIER_HEADLESS
/*
 * portable_file_dialog_verb - Interactive file dialog implementation
 *
 * Phase 2B.4: File verb integration layer
 *
 * Implements file.getFileDialog, putFileDialog, getFolderDialog, getDiskDialog
 * for interactive mode. Bridges UserTalk calling convention to file_dialog.c.
 */
boolean portable_file_dialog_verb(short token, hdltreenode hparam1,
                                  tyvaluerecord *vreturned, bigstring bserror) {
	/* Result structure matching file_dialog.h */
	typedef struct {
		char path[1024];
		boolean success;
	} file_dialog_result;

	/* Forward declarations to avoid including file_dialog.h */
	extern file_dialog_result file_dialog_get_file(const char *);
	extern file_dialog_result file_dialog_put_file(const char *);
	extern file_dialog_result file_dialog_get_folder(const char *);
	extern file_dialog_result file_dialog_get_disk(void);

	bigstring bsprompt;
	bigstring bsvarname;
	hdlhashtable htable;
	tyvaluerecord val;
	tyfilespec fs;
	hdlhashnode hnode;

	/* Extract prompt (parameter 1) */
	if (!getstringvalue(hparam1, 1, bsprompt)) {
		return false;
	}

	/* Extract variable name to store result (parameter 2) */
	flnextparamislast = true;
	if (!getvarparam(hparam1, 2, &htable, bsvarname)) {
		return false;
	}

	/* Get starting path from current variable value (if exists) */
	char start_path[1024] = {0};
	if (hashtablelookup(htable, bsvarname, &val, &hnode)) {
		tyvaluerecord valcopy;
		if (copyvaluerecord(val, &valcopy)) {
			disablelangerror();
			if (coercetofilespec(&valcopy)) {
				filespec_to_cstring(&valcopy.data.filespecvalue, start_path, sizeof(start_path));
			}
			enablelangerror();
		}
	}

	/* Call appropriate file dialog function */
	file_dialog_result result;
	result.success = false;
	result.path[0] = '\0';

	switch (token) {
		case sfgetfilefunc:
			result = file_dialog_get_file(start_path[0] ? start_path : NULL);
			break;

		case sfputfilefunc:
			result = file_dialog_put_file(start_path[0] ? start_path : NULL);
			break;

		case sfgetfolderfunc:
			result = file_dialog_get_folder(start_path[0] ? start_path : NULL);
			break;

		case sfgetdiskfunc:
			result = file_dialog_get_disk();
			break;

		default:
			copyctopstring("Unknown file dialog verb", bserror);
			return false;
	}

	/* User cancelled */
	if (!result.success) {
		setbooleanvalue(false, vreturned);
		return true;
	}

	/* Convert result path to filespec */
	bigstring bspath;
	/* Copy C string to Pascal string manually */
	size_t len = strlen(result.path);
	if (len > 255)
		len = 255;
	memcpy(&bspath[1], result.path, len);
	bspath[0] = (unsigned char)len;

	if (!pathtofilespec(bspath, &fs)) {
		copyctopstring("Failed to convert path to filespec", bserror);
		return false;
	}

	if (!setfilespecvalue(&fs, &val)) {
		return false;
	}

	/* Store result in variable */
	pushhashtable(htable);
	boolean fl = langsetsymbolval(bsvarname, val);
	pophashtable();

	if (!fl) {
		return false;
	}

	exemptfromtmpstack(&val);

	/* Return true to indicate user selected a file */
	setbooleanvalue(true, vreturned);
	return true;
}
#endif /* FRONTIER_HEADLESS */
