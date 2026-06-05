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

/* Buffer size for file copy/move operations (128KB) */
#define FILE_COPY_BUFFER_SIZE (128 * 1024)

/* Threshold for treating count as "infinity" in file.read(path, infinity).
 * Any count >= 2GB effectively means "read the rest of the file".
 * This handles both UserTalk's 'infinity' constant and large explicit values. */
#define USERTALK_INFINITY_THRESHOLD (2LL * 1024 * 1024 * 1024)

typedef struct {
	FILE *fp;
	char path[4096];    /* Store path for path-based lookup (Frontier semantics) */
	boolean inuse;
	int refcount;       /* Reference count for thread-safe FILE* access */
} filehandle;

static filehandle filetable[MAX_OPEN_FILES];
static boolean g_cleanup_registered = false;
static pthread_mutex_t filetable_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Normalize path for comparison.
 * macOS: case-insensitive (HFS+/APFS default), convert to lowercase
 * Linux: case-sensitive, preserve original case
 * Returns true if path fit in buffer, false if truncated.
 */
static boolean normalize_path(const char *src, char *dst, size_t dstsize) {
	size_t i;
	for (i = 0; i < dstsize - 1 && src[i]; i++) {
#ifdef __APPLE__
		dst[i] = tolower((unsigned char)src[i]);
#else
		dst[i] = src[i];  /* Preserve case on case-sensitive filesystems */
#endif
	}
	dst[i] = '\0';

	/* Check if source was truncated */
	if (src[i] != '\0') {
		log_warn(LOG_COMP_LANG, "normalize_path: path truncated (len > %zu)", dstsize - 1);
		return false;
	}
	return true;
}

/*
 * Open a file by path and register it in the file table.
 * Returns true if file was opened successfully, false otherwise.
 * If file is already open, just returns true (Frontier behavior).
 *
 * Thread safety: We reserve the slot (inuse=true, fp=NULL) before doing I/O,
 * preventing TOCTOU races where another thread could steal our slot.
 */
static boolean open_file_by_path(const char *path) {
	int i;
	int reserved_slot = -1;
	FILE *fp;
	char normalized[4096];

	normalize_path(path, normalized, sizeof(normalized));

	pthread_mutex_lock(&filetable_mutex);

	/* Check if file is already open */
	for (i = 0; i < MAX_OPEN_FILES; i++) {
		if (filetable[i].inuse && filetable[i].fp != NULL) {
			char existing_normalized[4096];
			normalize_path(filetable[i].path, existing_normalized, sizeof(existing_normalized));
			if (strcmp(existing_normalized, normalized) == 0) {
				pthread_mutex_unlock(&filetable_mutex);
				return true;  /* Already open */
			}
		}
	}

	/* Find free slot and RESERVE it before doing I/O */
	for (i = 0; i < MAX_OPEN_FILES; i++) {
		if (!filetable[i].inuse) {
			/* Reserve the slot: mark inuse but with NULL fp */
			filetable[i].inuse = true;
			filetable[i].fp = NULL;
			filetable[i].refcount = 0;
			strncpy(filetable[i].path, path, sizeof(filetable[i].path) - 1);
			filetable[i].path[sizeof(filetable[i].path) - 1] = '\0';
			reserved_slot = i;
			break;
		}
	}

	pthread_mutex_unlock(&filetable_mutex);

	if (reserved_slot < 0) {
		return false;  /* No free slots */
	}

	/* Open file outside of lock to avoid blocking I/O under mutex */
	fp = fopen(path, "r+b");
	if (!fp) {
		/* Try read-only if r+b fails */
		fp = fopen(path, "rb");
	}

	pthread_mutex_lock(&filetable_mutex);

	if (!fp) {
		/* fopen failed - release our reserved slot */
		filetable[reserved_slot].inuse = false;
		filetable[reserved_slot].path[0] = '\0';
		pthread_mutex_unlock(&filetable_mutex);
		return false;
	}

	/* Install the FILE* in our reserved slot */
	filetable[reserved_slot].fp = fp;

	pthread_mutex_unlock(&filetable_mutex);
	return true;
}

/*
 * Get FILE* for a path. Returns NULL if file is not open.
 * Caller must call release_file_by_path() when done.
 */
static FILE* get_file_by_path(const char *path) {
	int i;
	FILE *result = NULL;
	char normalized[4096];

	normalize_path(path, normalized, sizeof(normalized));

	pthread_mutex_lock(&filetable_mutex);

	for (i = 0; i < MAX_OPEN_FILES; i++) {
		/* Skip reserved but not-yet-opened slots (fp=NULL during fopen) */
		if (filetable[i].inuse && filetable[i].fp != NULL) {
			char existing_normalized[4096];
			normalize_path(filetable[i].path, existing_normalized, sizeof(existing_normalized));
			if (strcmp(existing_normalized, normalized) == 0) {
				result = filetable[i].fp;
				filetable[i].refcount++;
				break;
			}
		}
	}

	pthread_mutex_unlock(&filetable_mutex);
	return result;
}

/*
 * Release reference to file obtained via get_file_by_path()
 */
static void release_file_by_path(const char *path) {
	int i;
	char normalized[4096];

	normalize_path(path, normalized, sizeof(normalized));

	pthread_mutex_lock(&filetable_mutex);

	for (i = 0; i < MAX_OPEN_FILES; i++) {
		if (filetable[i].inuse) {
			char existing_normalized[4096];
			normalize_path(filetable[i].path, existing_normalized, sizeof(existing_normalized));
			if (strcmp(existing_normalized, normalized) == 0) {
				if (filetable[i].refcount > 0)
					filetable[i].refcount--;
				break;
			}
		}
	}

	pthread_mutex_unlock(&filetable_mutex);
}

/*
 * Close a file by path. Returns true if closed, false if not found.
 * Skips reserved-but-not-opened slots (fp=NULL during fopen in another thread).
 */
static boolean close_file_by_path(const char *path) {
	int i;
	FILE *fp_to_close = NULL;
	char normalized[4096];

	normalize_path(path, normalized, sizeof(normalized));

	pthread_mutex_lock(&filetable_mutex);

	for (i = 0; i < MAX_OPEN_FILES; i++) {
		/* Skip reserved but not-yet-opened slots (fp=NULL during fopen) */
		if (filetable[i].inuse && filetable[i].fp != NULL) {
			char existing_normalized[4096];
			normalize_path(filetable[i].path, existing_normalized, sizeof(existing_normalized));
			if (strcmp(existing_normalized, normalized) == 0) {
				if (filetable[i].refcount == 0) {
					fp_to_close = filetable[i].fp;
					filetable[i].fp = NULL;
					filetable[i].inuse = false;
					filetable[i].path[0] = '\0';
				}
				break;
			}
		}
	}

	pthread_mutex_unlock(&filetable_mutex);

	if (fp_to_close) {
		fclose(fp_to_close);
		return true;
	}

	return (i < MAX_OPEN_FILES);  /* Return true if found (even if couldn't close due to refs) */
}

/* Legacy refnum-based functions removed - path-based API is now the only supported API */

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
		if (filetable[i].inuse && filetable[i].fp != NULL) {
			log_warn(LOG_COMP_LANG, "cleanup_file_handles: closing leaked file path='%s'",
			         filetable[i].path);

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

	/*
	 * Shared path buffers - hoisted above switch to avoid ~450KB stack frame.
	 *
	 * Without this, each case declares its own char path[4096] etc., and the
	 * compiler allocates stack space for ALL case-local variables simultaneously
	 * at function entry. Since UserTalk expressions like file.exists(file.folderFromPath(x))
	 * cause recursive entry, two frames exceed the 512KB pthread stack limit.
	 *
	 * path  - primary path buffer (source path in two-path operations)
	 * path2 - secondary path buffer (dest path in copy/move/rename)
	 *
	 * WARNING: These buffers are reused across all cases. Each case may
	 * overwrite their contents. Do not add fallthrough or goto between
	 * cases without considering buffer reuse.
	 */
	char path[4096];
	char path2[4096];
	struct stat st;

	switch (token) {

		/* Tier 1: Critical file operations (20 verbs) - Phase 2 */

		case fileexistsfunc: {
			/* Check if file or folder exists */
			tyfilespec fs;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			if (!filespec_to_cstring(&fs, path, sizeof(path)))
				return false;

			boolean exists = (stat(path, &st) == 0);
			return setbooleanvalue(exists, vreturned);
		}

		case fileisfolderfunc: {
			/* Check if path is a folder/directory
			 * Per original Mac behavior: throws error if path doesn't exist,
			 * returns false if path exists but is not a folder.
			 * This is required for file.sureFolder() to work correctly.
			 */
			tyfilespec fs;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			if (!filespec_to_cstring(&fs, path, sizeof(path)))
				return false;

			if (stat(path, &st) != 0) {
				/* Path doesn't exist - throw error (not return false) */
				/* This matches original Mac behavior where filegetinfo fails */
				bigstring bserrmsg;
				char errbuf[512];
				snprintf(errbuf, sizeof(errbuf), "Can't find a file named \"%s\".", path);
				/* copyctopstring clips to 255 bytes on very long paths.  This is
				 * intentional here: we are already on the error path (stat failed),
				 * and a truncated error message is acceptable -- the caller still
				 * sees a meaningful "Can't find a file named ..." prefix.  No
				 * secondary error handling is added to avoid nesting error logic
				 * inside an error reporter. (#712 audit) */
				copyctopstring(errbuf, bserrmsg);
				langerrormessage(bserrmsg);
				return false;
			}

			return setbooleanvalue(S_ISDIR(st.st_mode), vreturned);
		}

		case fileisvolumefunc: {
			/* Check if path is a mount point (volume root) */
			tyfilespec fs;
			struct stat parent_st;

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
			snprintf(path2, sizeof(path2), "%s/..", path);

			/* Get stat info for parent directory */
			if (stat(path2, &parent_st) != 0)
				return setbooleanvalue(false, vreturned);

			/* If device ID differs from parent, this is a mount point */
			boolean is_mountpoint = (st.st_dev != parent_st.st_dev);

			return setbooleanvalue(is_mountpoint, vreturned);
		}
		case filesizefunc: {
			/* Return file size in bytes */
			tyfilespec fs;

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
			bigstring bsresolved;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			if (!filespec_to_cstring(&fs, path, sizeof(path)))
				return false;

			/* Use realpath() to resolve to absolute path */
			if (realpath(path, path2) == NULL) {
				/* If realpath fails (file doesn't exist), just return the original path */
				if (!filespectopath(&fs, bsresolved))
					return false;
			} else {
				/* Convert C string to bigstring */
				size_t len = strlen(path2);
				if (len > 255)
					len = 255;
				bsresolved[0] = (unsigned char)len;
				memcpy(&bsresolved[1], path2, len);
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

			if (!getfilespecvalue(hparam1, 1, &fsold))
				return false;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 2, &fsnew))
				return false;

			if (!filespec_to_cstring(&fsold, path, sizeof(path)))
				return false;

			if (!filespec_to_cstring(&fsnew, path2, sizeof(path2)))
				return false;

			/* Use rename() system call */
			if (rename(path, path2) != 0) {
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
			FILE *fpsrc = NULL, *fpdest = NULL;
			char *copybuf = NULL;
			size_t bytes_read;
			boolean success = false;

			if (!getfilespecvalue(hparam1, 1, &fssrc))
				return false;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 2, &fsdest))
				return false;

			if (!filespec_to_cstring(&fssrc, path, sizeof(path)))
				return false;

			if (!filespec_to_cstring(&fsdest, path2, sizeof(path2)))
				return false;

			/* Check source exists and get permissions */
			if (stat(path, &st) != 0) {
				copyctopstring("Source file not found", bserror);
				return false;
			}

			/* Heap-allocate copy buffer (128KB) to avoid stack overflow */
			copybuf = malloc(FILE_COPY_BUFFER_SIZE);
			if (!copybuf) {
				copyctopstring("Out of memory", bserror);
				return false;
			}

			/* Open source for reading */
			fpsrc = fopen(path, "rb");
			if (!fpsrc) {
				free(copybuf);
				copyctopstring("Can't open source file", bserror);
				return false;
			}

			/* Open destination for writing */
			fpdest = fopen(path2, "wb");
			if (!fpdest) {
				fclose(fpsrc);
				free(copybuf);
				copyctopstring("Can't create destination file", bserror);
				return false;
			}

			/* Copy data in chunks */
			while ((bytes_read = fread(copybuf, 1, FILE_COPY_BUFFER_SIZE, fpsrc)) > 0) {
				if (fwrite(copybuf, 1, bytes_read, fpdest) != bytes_read) {
					copyctopstring("Write error during copy", bserror);
					goto copy_cleanup;
				}
			}

			/* Check for read error */
			if (ferror(fpsrc)) {
				copyctopstring("Read error during copy", bserror);
				goto copy_cleanup;
			}

			success = true;

		copy_cleanup:
			if (fpsrc)
				fclose(fpsrc);
			if (fpdest)
				fclose(fpdest);
			free(copybuf);

			/* Preserve permissions on success */
			if (success) {
				if (chmod(path2, st.st_mode & 0777) != 0) {
					log_warn(LOG_COMP_LANG, "file.copy: chmod failed for %s (errno=%d)",
					        path2, errno);
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

			if (!getfilespecvalue(hparam1, 1, &fssrc))
				return false;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 2, &fsdest))
				return false;

			if (!filespec_to_cstring(&fssrc, path, sizeof(path)))
				return false;

			if (!filespec_to_cstring(&fsdest, path2, sizeof(path2)))
				return false;

			/* Check source exists */
			if (stat(path, &st) != 0) {
				copyctopstring("Source file not found", bserror);
				return false;
			}

			/* Try rename() first (efficient for same volume) */
			if (rename(path, path2) == 0) {
				return setbooleanvalue(true, vreturned);
			}

			/* If cross-volume (EXDEV), fall back to copy+delete */
			if (errno == EXDEV) {
				FILE *fpsrc = NULL, *fpdest = NULL;
				char *copybuf = NULL;
				size_t bytes_read;

				/* Heap-allocate copy buffer (128KB) to avoid stack overflow */
				copybuf = malloc(FILE_COPY_BUFFER_SIZE);
				if (!copybuf) {
					copyctopstring("Out of memory", bserror);
					return false;
				}

				/* Open source for reading */
				fpsrc = fopen(path, "rb");
				if (!fpsrc) {
					free(copybuf);
					copyctopstring("Can't open source file", bserror);
					return false;
				}

				/* Open destination for writing */
				fpdest = fopen(path2, "wb");
				if (!fpdest) {
					fclose(fpsrc);
					free(copybuf);
					copyctopstring("Can't create destination file", bserror);
					return false;
				}

				/* Copy data */
				while ((bytes_read = fread(copybuf, 1, FILE_COPY_BUFFER_SIZE, fpsrc)) > 0) {
					if (fwrite(copybuf, 1, bytes_read, fpdest) != bytes_read) {
						copyctopstring("Write error during move", bserror);
						fclose(fpsrc);
						fclose(fpdest);
						free(copybuf);
						return false;
					}
				}

				/* Check for read error BEFORE closing */
				if (ferror(fpsrc)) {
					fclose(fpsrc);
					fclose(fpdest);
					free(copybuf);
					copyctopstring("Read error during move", bserror);
					return false;
				}

				fclose(fpsrc);
				fclose(fpdest);
				free(copybuf);

				/* Preserve permissions */
				if (chmod(path2, st.st_mode & 0777) != 0) {
					log_warn(LOG_COMP_LANG, "file.move: chmod failed for %s (errno=%d)",
					        path2, errno);
					/* Continue - copy succeeded even if permission preservation failed */
				}

				/* Delete source on success */
				if (unlink(path) != 0) {
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
					snprintf(path2, sizeof(path2), "%s/Desktop", home);
					folderpath = path2;
					break;

				case 'docs':  /* Documents */
					snprintf(path2, sizeof(path2), "%s/Documents", home);
					folderpath = path2;
					break;

				case 'temp':  /* Temporary Items */
					folderpath = "./tmp";  /* Relative to cwd (sandbox-safe) */
					break;

				case 'pref':  /* Preferences */
					#ifdef __APPLE__
						snprintf(path2, sizeof(path2), "%s/Library/Preferences", home);
					#else
						snprintf(path2, sizeof(path2), "%s/.config", home);
					#endif
					folderpath = path2;
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
						snprintf(path2, sizeof(path2), "%s/Library/Fonts", home);
					#else
						snprintf(path2, sizeof(path2), "%s/.fonts", home);
					#endif
					folderpath = path2;
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
			/* file.open(path) - Open file for subsequent read/write operations
			 * Per docs/usertalk/docserver/file/open.txt:
			 * - Opens file by path
			 * - Returns true if file opened successfully, false otherwise
			 * - File can then be read/written using file.read(path, count) etc. */
			tyfilespec fs;
			boolean success;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			if (!filespec_to_cstring(&fs, path, sizeof(path)))
				return false;

			/* Open file and register in path-based table */
			success = open_file_by_path(path);

			return setbooleanvalue(success, vreturned);
		}

		case closefilefunc: {
			/* file.close(path) - Close file by path
			 * Per docs/usertalk/docserver/file/close.txt:
			 * - Closes file previously opened with file.open(path)
			 * - Returns true if closed successfully */
			tyfilespec fs;
			boolean success;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			if (!filespec_to_cstring(&fs, path, sizeof(path)))
				return false;

			success = close_file_by_path(path);

			return setbooleanvalue(success, vreturned);
		}

		case endoffilefunc: {
			/* file.endOfFile(path) - Check if at end of file */
			tyfilespec fs;
			FILE *fp;
			boolean iseof;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			if (!filespec_to_cstring(&fs, path, sizeof(path)))
				return false;

			fp = get_file_by_path(path);
			if (!fp) {
				copyctopstring("File not open - call file.open first", bserror);
				return false;
			}

			iseof = (feof(fp) != 0);
			release_file_by_path(path);
			return setbooleanvalue(iseof, vreturned);
		}

		case setendoffilefunc: {
			/* file.setEndOfFile(path) - Truncate file at current position */
			tyfilespec fs;
			FILE *fp;
			long pos;
			int fd;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			if (!filespec_to_cstring(&fs, path, sizeof(path)))
				return false;

			fp = get_file_by_path(path);
			if (!fp) {
				copyctopstring("File not open - call file.open first", bserror);
				return false;
			}

			pos = ftell(fp);
			if (pos < 0) {
				release_file_by_path(path);
				copyctopstring("Can't get file position", bserror);
				return false;
			}

			fd = fileno(fp);
			if (ftruncate(fd, pos) != 0) {
				release_file_by_path(path);
				copyctopstring("Can't truncate file", bserror);
				return false;
			}

			release_file_by_path(path);
			return setbooleanvalue(true, vreturned);
		}

		case getendoffilefunc: {
			/* file.getEndOfFile(path) - Get file size */
			tyfilespec fs;
			FILE *fp;
			long current, size;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			if (!filespec_to_cstring(&fs, path, sizeof(path)))
				return false;

			fp = get_file_by_path(path);
			if (!fp) {
				copyctopstring("File not open - call file.open first", bserror);
				return false;
			}

			current = ftell(fp);
			if (current < 0) {
				release_file_by_path(path);
				copyctopstring("Can't get current file position", bserror);
				return false;
			}

			if (fseek(fp, 0, SEEK_END) != 0) {
				release_file_by_path(path);
				copyctopstring("Can't seek to end of file", bserror);
				return false;
			}

			size = ftell(fp);
			if (size < 0) {
				release_file_by_path(path);
				copyctopstring("Can't get file size", bserror);
				return false;
			}

			if (fseek(fp, current, SEEK_SET) != 0) {
				release_file_by_path(path);
				copyctopstring("Can't restore file position", bserror);
				return false;
			}

			release_file_by_path(path);
			return setlongvalue(size, vreturned);
		}

		case setpositionfunc: {
			/* file.setPosition(path, position) - Set file position */
			tyfilespec fs;
			long position;
			FILE *fp;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			flnextparamislast = true;

			if (!getlongvalue(hparam1, 2, &position))
				return false;

			if (!filespec_to_cstring(&fs, path, sizeof(path)))
				return false;

			fp = get_file_by_path(path);
			if (!fp) {
				copyctopstring("File not open - call file.open first", bserror);
				return false;
			}

			if (fseek(fp, position, SEEK_SET) != 0) {
				release_file_by_path(path);
				copyctopstring("Can't set file position", bserror);
				return false;
			}

			release_file_by_path(path);
			return setbooleanvalue(true, vreturned);
		}

		case getpositionfunc: {
			/* file.getPosition(path) - Get current file position */
			tyfilespec fs;
			FILE *fp;
			long position;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			if (!filespec_to_cstring(&fs, path, sizeof(path)))
				return false;

			fp = get_file_by_path(path);
			if (!fp) {
				copyctopstring("File not open - call file.open first", bserror);
				return false;
			}

			position = ftell(fp);
			if (position < 0) {
				release_file_by_path(path);
				copyctopstring("Can't get file position", bserror);
				return false;
			}

			release_file_by_path(path);
			return setlongvalue(position, vreturned);
		}

		case readlinefunc: {
			/* file.readLine(path) - Read a line from file */
			tyfilespec fs;
			FILE *fp;
			bigstring bsline;
			int ch;
			int len = 0;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			if (!filespec_to_cstring(&fs, path, sizeof(path)))
				return false;

			fp = get_file_by_path(path);
			if (!fp) {
				copyctopstring("File not open - call file.open first", bserror);
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

			release_file_by_path(path);
			return setstringvalue(bsline, vreturned);
		}

		case writelinefunc: {
			/* file.writeLine(path, line) - Write a line to file */
			tyfilespec fs;
			FILE *fp;
			bigstring bsline;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			flnextparamislast = true;

			if (!getstringvalue(hparam1, 2, bsline))
				return false;

			if (!filespec_to_cstring(&fs, path, sizeof(path)))
				return false;

			fp = get_file_by_path(path);
			if (!fp) {
				copyctopstring("File not open - call file.open first", bserror);
				return false;
			}

			/* Write string */
			if (bsline[0] > 0) {
				if (fwrite(&bsline[1], 1, bsline[0], fp) != bsline[0]) {
					release_file_by_path(path);
					copyctopstring("Write error", bserror);
					return false;
				}
			}

			/* Write newline */
			if (fputc('\n', fp) == EOF) {
				release_file_by_path(path);
				copyctopstring("Write error", bserror);
				return false;
			}

			release_file_by_path(path);
			return setbooleanvalue(true, vreturned);
		}

		case readfunc: {
			/* file.read(path, count) - Read bytes from file
			 * Per docs/usertalk/docserver/file/read.txt:
			 * - Takes path (not refnum!) and byte count
			 * - File must be opened with file.open() first
			 * - Maintains file position for sequential reads
			 * - If count is infinity, reads all remaining bytes
			 * - Returns binary data */
			tyfilespec fs;
			long long count;  /* Use long long to handle infinity (LLONG_MAX) */
			FILE *fp;
			Handle hdata;
			unsigned char *buffer;
			size_t bytesread;
			long current_pos, file_size, bytes_to_read;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			flnextparamislast = true;

			/* Get count as long long to handle infinity (0x7FFFFFFFFFFFFFFF) */
			tyvaluerecord countval;
			if (!getparamvalue(hparam1, 2, &countval))
				return false;

			if (!coercetolong(&countval))
				return false;

			count = countval.data.longvalue;

			if (!filespec_to_cstring(&fs, path, sizeof(path)))
				return false;

			/* Look up file in path-based table (must be opened with file.open first) */
			fp = get_file_by_path(path);
			if (!fp) {
				copyctopstring("File not open - call file.open first", bserror);
				return false;
			}

			if (count <= 0) {
				release_file_by_path(path);
				return setstringvalue(BIGSTRING("\x00"), vreturned);
			}

			/* Handle infinity (or very large count) - read remaining bytes in file */
			if (count >= USERTALK_INFINITY_THRESHOLD) {
				current_pos = ftell(fp);
				if (current_pos < 0) {
					release_file_by_path(path);
					copyctopstring("Can't get file position", bserror);
					return false;
				}
				fseek(fp, 0, SEEK_END);
				file_size = ftell(fp);
				fseek(fp, current_pos, SEEK_SET);
				bytes_to_read = file_size - current_pos;
			} else {
				bytes_to_read = (long)count;
			}

			if (bytes_to_read <= 0) {
				release_file_by_path(path);
				return setstringvalue(BIGSTRING("\x00"), vreturned);
			}

			/* Safety check - limit to reasonable size (500MB) */
			if (bytes_to_read > 500 * 1024 * 1024) {
				release_file_by_path(path);
				copyctopstring("File too large (exceeds 500MB limit)", bserror);
				return false;
			}

			/* Allocate handle for data */
			if (!newhandle(bytes_to_read, &hdata)) {
				release_file_by_path(path);
				copyctopstring("Out of memory", bserror);
				return false;
			}

			lockhandle(hdata);
			buffer = (unsigned char *)*hdata;
			bytesread = fread(buffer, 1, bytes_to_read, fp);
			unlockhandle(hdata);

			if (bytesread == 0) {
				release_file_by_path(path);
				disposehandle(hdata);
				return setstringvalue(BIGSTRING("\x00"), vreturned);
			}

			/* Resize handle if we read less than requested */
			if (bytesread < (size_t)bytes_to_read) {
				sethandlesize(hdata, bytesread);
			}

			release_file_by_path(path);
			return setbinaryvalue(hdata, bytesread, vreturned);
		}

		case writefunc: {
			/* file.write(path, data) - Write bytes to file
			 * Per docs/usertalk/docserver/file/write.txt:
			 * - Takes path (not refnum!) and data to write
			 * - File must be opened with file.open() first
			 * - Writes at current file position
			 * - Returns true on success */
			tyfilespec fs;
			Handle hdata;
			FILE *fp;
			size_t datasize;
			size_t written;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			flnextparamislast = true;

			if (!getreadonlytextvalue(hparam1, 2, &hdata))
				return false;

			if (!filespec_to_cstring(&fs, path, sizeof(path)))
				return false;

			/* Look up file in path-based table (must be opened with file.open first) */
			fp = get_file_by_path(path);
			if (!fp) {
				copyctopstring("File not open - call file.open first", bserror);
				return false;
			}

			datasize = gethandlesize(hdata);
			if (datasize > 0) {
				lockhandle(hdata);
				written = fwrite(*hdata, 1, datasize, fp);
				unlockhandle(hdata);

				if (written != datasize) {
					release_file_by_path(path);
					copyctopstring("Write error", bserror);
					return false;
				}
			}

			release_file_by_path(path);
			return setbooleanvalue(true, vreturned);
		}

		case readwholefilefunc: {
			/* Read entire file into string/binary */
			tyfilespec fs;
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

			if (bytesread != (size_t) filesize) {
				disposehandle(hdata);
				copyctopstring("Read error", bserror);
				return false;
			}

			return setbinaryvalue(hdata, filesize, vreturned);
		}

		case writewholefilefunc: {
			/* Write entire string or binary data to file */
			tyfilespec fs;
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
				if (fwrite(buffer, 1, datasize, fp) != (size_t) datasize) {
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
			FILE *fp1 = NULL, *fp2 = NULL;
			int ch1, ch2;
			boolean equal = true;

			if (!getfilespecvalue(hparam1, 1, &fs1))
				return false;

			flnextparamislast = true;

			if (!getfilespecvalue(hparam1, 2, &fs2))
				return false;

			if (!filespec_to_cstring(&fs1, path, sizeof(path)))
				return false;

			if (!filespec_to_cstring(&fs2, path2, sizeof(path2)))
				return false;

			fp1 = fopen(path, "rb");
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
		case setfilecreatorfunc: {
			/* Legacy HFS type/creator codes — no-op on modern systems.
			   Called by file.writeWholeFile when type/creator params are provided.
			   On macOS, UTIs and file extensions have replaced type/creator codes. */
			tyfilespec fs;
			bigstring bsval;

			if (!getfilespecvalue(hparam1, 1, &fs))
				return false;

			flnextparamislast = true;

			if (!getstringvalue(hparam1, 2, bsval))
				return false;

			return setbooleanvalue(true, vreturned);
		}

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
					/* In non-interactive mode, return false as the dialog result
					 * (same as user clicking Cancel), not a verb error. This lets
					 * scripts handle it gracefully via if/try. */
					setbooleanvalue (false, vreturned);
					return true;
				}

				return portable_file_dialog_verb(token, hparam1, vreturned, bserror);
			#else
				/* Return false as the dialog result (user cancelled), not a verb error */
				setbooleanvalue (false, vreturned);
				return true;
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

		case filefindappfunc: {
			/* In headless mode, app searching isn't meaningful.
			 * Return empty string to allow scripts to continue gracefully.
			 * Callers typically check for empty result anyway. */
			bigstring bsresult;

			flnextparamislast = true;

			/* Consume the parameter (creator code) but ignore it */
			if (!getstringvalue(hparam1, 1, bsresult))
				return false;

			/* Return empty string - app not found */
			setemptystring(bsresult);
			return setstringvalue(bsresult, vreturned);
		}

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
	extern file_dialog_result file_dialog_get_file(const char *, const char *, const char *);
	extern file_dialog_result file_dialog_put_file(const char *, const char *);
	extern file_dialog_result file_dialog_get_folder(const char *, const char *);
	extern file_dialog_result file_dialog_get_disk(const char *);

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

	/* Extract variable name to store result (parameter 2).
	   For getFileDialog, there's an additional type parameter (param 3),
	   matching the GUI's filedialogverb() signature. */
	if (token != sfgetfilefunc)
		flnextparamislast = true;

	if (!getvarparam(hparam1, 2, &htable, bsvarname)) {
		return false;
	}

	/* For getFileDialog: extract the file type parameter (param 3)
	   for browser extension filtering. */
	bigstring bstype = {0};
	if (token == sfgetfilefunc) {
		flnextparamislast = true;
		if (!getstringvalue(hparam1, 3, bstype)) {
			return false;
		}
	}

	/* Get starting path from current variable value (if exists) */
	char start_path[1024] = {0};
	if (hashtablelookup(htable, bsvarname, &val, &hnode)) {
		tyvaluerecord valcopy;
		if (copyvaluerecord(val, &valcopy)) {
			disablelangerror();
			if (coercetofilespec(&valcopy)) {
				filespec_to_cstring(*valcopy.data.filespecvalue, start_path, sizeof(start_path));
			}
			enablelangerror();
		}
	}

	/* Call appropriate file dialog function */
	file_dialog_result result;
	result.success = false;
	result.path[0] = '\0';

	/* Convert prompt and type to C strings for dialog functions */
	char csprompt[256];
	copyptocstring(bsprompt, csprompt);

	char cstype[64] = {0};
	if (bstype[0] > 0) {
		copyptocstring(bstype, cstype);
	}

	switch (token) {
		case sfgetfilefunc:
			result = file_dialog_get_file(csprompt,
			                              start_path[0] ? start_path : NULL,
			                              cstype[0] ? cstype : NULL);
			break;

		case sfputfilefunc:
			result = file_dialog_put_file(csprompt,
			                              start_path[0] ? start_path : NULL);
			break;

		case sfgetfolderfunc:
			result = file_dialog_get_folder(csprompt,
			                                start_path[0] ? start_path : NULL);
			break;

		case sfgetdiskfunc:
			result = file_dialog_get_disk(csprompt);
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
