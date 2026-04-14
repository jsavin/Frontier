#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#ifdef __APPLE__
#include <sys/attr.h>
#endif

#include "frontier.h"
#include "standard.h"
#include "file.h"
#include "memory.h"
#include "strings.h"
#include "file_portable.h"
#include "file_working_dir.h"
#include "logging.h"
#include "timedate.h"

/*
 * Portable/headless file layer that backs the classic Frontier file API with
 * stdio. This is sufficient for CLI/tests that use FRONTIER_HEADLESS or
 * FRONTIER_PORTABLE builds.
 *
 * THREADING MODEL:
 * - Global file descriptor table protected by ftable_mutex
 * - Reference counting pattern prevents use-after-free during concurrent I/O
 * - closefile() returns false if file has active references (refcount > 0)
 * - Caller responsible for synchronization - must not close file with active I/O
 * - Edge case: If thread crashes while holding reference, file stays open
 *   (acceptable for process lifetime; cleanup required only at shutdown)
 *
 * LOGGING: Uses LOG_COMP_DB since file layer is part of database persistence.
 * No LOG_COMP_FILE exists - file operations are semantically database operations.
 */

typedef struct {
    FILE *fp;
    char path[4096];
    int refcount;  /* Number of threads with active I/O operations on this file.
                    * closefile() fails if refcount > 0 to prevent use-after-free. */
} fnum_entry;

#define PORTABLE_MAX_FNUM 256
static fnum_entry ftable[PORTABLE_MAX_FNUM];
static boolean ftable_initialized = false;
static pthread_mutex_t ftable_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * ftable_init_impl - Internal initialization function for pthread_once
 * Guarantees single-invocation initialization even under concurrent access.
 */
static void ftable_init_impl(void) {
    memset(ftable, 0, sizeof(ftable));
    ftable_initialized = true;
}

/*
 * ensure_ftable_initialized - Initialize file descriptor table on first use
 *
 * Uses pthread_once() to guarantee thread-safe, single-invocation initialization.
 * Even if multiple threads call this concurrently, the table is initialized exactly once.
 *
 * Addresses launch blocker (CLAUDE.md: "Global Mutable State - CRITICAL FOR LAUNCH").
 * Issue #323 originally tracked this; now implemented.
 */
static pthread_once_t ftable_once = PTHREAD_ONCE_INIT;

static void ensure_ftable_initialized(void) {
    pthread_once(&ftable_once, ftable_init_impl);
}

static hdlfilenum alloc_fnum(void) {
    ensure_ftable_initialized();
    pthread_mutex_lock(&ftable_mutex);

    for (int i = 1; i < PORTABLE_MAX_FNUM; ++i) {
        if (ftable[i].fp == NULL) {
            pthread_mutex_unlock(&ftable_mutex);
            return (hdlfilenum) i;
        }
    }

    pthread_mutex_unlock(&ftable_mutex);
    return 0;
}

static fnum_entry *entry_from(hdlfilenum fnum) {
    if (fnum <= 0 || fnum >= PORTABLE_MAX_FNUM)
        return NULL;
    return &ftable[fnum];
}

__attribute__((unused))
static FILE *fp_from(hdlfilenum fnum) {
    fnum_entry *slot = entry_from(fnum);
    if (!slot)
        return NULL;
    return slot->fp;
}

static void fsname_to_path(const tyfsname *name, char *out, size_t outsz) {
    if (!name || !out || outsz == 0) {
        if (out && outsz)
            out[0] = '\0';
        return;
    }
    size_t len = name->length;
    if (len >= outsz)
        len = outsz - 1;
    for (size_t i = 0; i < len; ++i)
        out[i] = (char) (name->unicode[i] & 0xFF);
    out[len] = '\0';
}

static void path_to_fsname(const char *path, tyfsnameptr name) {
    if (!path || !name) {
        return;
    }
    size_t len = strlen(path);
    if (len > 255)
        len = 255;
    name->length = (UInt16) len;
    for (size_t i = 0; i < len; ++i)
        name->unicode[i] = (UInt16) (unsigned char) path[i];
}

static boolean path_from_filespec(const ptrfilespec fs, char *out, size_t outsz) {
    if (!fs || !out)
        return false;
    fsname_to_path(&fs->name, out, outsz);
    return out[0] != '\0';
}

boolean pathtofilespec(bigstring bspath, ptrfilespec fs) {
    if (!fs || isemptystring(bspath))
        return false;
    const unsigned char *src = (const unsigned char *) bspath;
    unsigned int len = src[0];
    if (len > 255)
        len = 255;
    fs->name.length = (UInt16) len;
    for (unsigned int i = 0; i < len; ++i)
        fs->name.unicode[i] = (UInt16) src[1 + i];
    fs->flags.flvolume = false;
    memset(&fs->ref, 0, sizeof fs->ref);
    return true;
}

boolean filespectopath(const ptrfilespec fs, bigstring bs) {
    if (!fs)
        return false;
    unsigned int len = fs->name.length;
    if (len > 255)
        len = 255;
    bs[0] = (unsigned char) len;
    for (unsigned int i = 0; i < len; ++i)
        bs[1 + i] = (unsigned char) (fs->name.unicode[i] & 0xFF);
    return true;
}

boolean openfile(const ptrfilespec fs, hdlfilenum *pfnum, boolean flreadonly) {
    if (!fs || !pfnum)
        return false;
    char path[4096];
    if (!path_from_filespec(fs, path, sizeof path))
        return false;
    const char *mode = flreadonly ? "rb" : "rb+";
    FILE *fp = fopen(path, mode);
    if (!fp && !flreadonly)
        fp = fopen(path, "rb");
    if (!fp)
        return false;
    hdlfilenum fnum = alloc_fnum();
    if (!fnum) {
        fclose(fp);
        return false;
    }

    /* Protect slot assignment with mutex to prevent race with other threads
     * reading/writing the same slot concurrently */
    pthread_mutex_lock(&ftable_mutex);
    fnum_entry *slot = entry_from(fnum);
    if (slot && slot->fp == NULL) {
        slot->fp = fp;
        slot->refcount = 0;  /* Initialize refcount for new entry */
        strncpy(slot->path, path, sizeof slot->path - 1);
        slot->path[sizeof slot->path - 1] = '\0';
        *pfnum = fnum;
        pthread_mutex_unlock(&ftable_mutex);

        log_trace(LOG_COMP_DB, "openfile fnum=%d path=%s mode=%s flreadonly=%d",
                  (int)fnum, path, mode, (int)flreadonly);

        return true;
    }
    pthread_mutex_unlock(&ftable_mutex);

    /* Slot was already taken by another thread - close our handle and fail */
    fclose(fp);
    return false;
}

boolean opennewfile(ptrfilespec fs, OSType creator, OSType filetype, hdlfilenum *pfnum) {
    (void) creator;
    (void) filetype;
    if (!fs || !pfnum)
        return false;
    char path[4096];
    if (!path_from_filespec(fs, path, sizeof path))
        return false;
    FILE *fp = fopen(path, "wb+");
    if (!fp)
        return false;
    hdlfilenum fnum = alloc_fnum();
    if (!fnum) {
        fclose(fp);
        return false;
    }

    /* Protect slot assignment with mutex to prevent race with other threads
     * reading/writing the same slot concurrently */
    pthread_mutex_lock(&ftable_mutex);
    fnum_entry *slot = entry_from(fnum);
    if (slot && slot->fp == NULL) {
        slot->fp = fp;
        slot->refcount = 0;  /* Initialize refcount for new entry */
        strncpy(slot->path, path, sizeof slot->path - 1);
        slot->path[sizeof slot->path - 1] = '\0';
        path_to_fsname(path, &fs->name);
        *pfnum = fnum;
        pthread_mutex_unlock(&ftable_mutex);

        log_trace(LOG_COMP_DB, "opennewfile fnum=%d path=%s creator/filetype ignored",
                  (int)fnum, path);

        return true;
    }
    pthread_mutex_unlock(&ftable_mutex);

    /* Slot was already taken by another thread - close our handle and fail */
    fclose(fp);
    return false;
}

boolean closefile(hdlfilenum fnum) {
    pthread_mutex_lock(&ftable_mutex);

    fnum_entry *slot = entry_from(fnum);
    if (!slot || !slot->fp) {
        pthread_mutex_unlock(&ftable_mutex);
        return false;
    }

    /* CRITICAL: Check refcount before closing.
     * If refcount > 0, another thread has active I/O on this file.
     * Closing now would cause use-after-free for that thread.
     * Caller must synchronize to ensure no I/O is in progress. */
    if (slot->refcount > 0) {
        log_warn(LOG_COMP_DB,
                 "closefile fnum=%d failed: %d threads have active I/O (refcount=%d)",
                 (int)fnum, slot->refcount, slot->refcount);
        pthread_mutex_unlock(&ftable_mutex);
        return false;
    }

    fclose(slot->fp);
    slot->fp = NULL;
    slot->path[0] = '\0';
    slot->refcount = 0;

    pthread_mutex_unlock(&ftable_mutex);

    log_trace(LOG_COMP_DB, "closefile fnum=%d", (int)fnum);

    return true;
}

boolean filesetposition(hdlfilenum fnum, long pos) {
    pthread_mutex_lock(&ftable_mutex);
    fnum_entry *slot = entry_from(fnum);
    if (!slot || !slot->fp) {
        pthread_mutex_unlock(&ftable_mutex);
        return false;
    }
    FILE *fp = slot->fp;
    slot->refcount++;
    pthread_mutex_unlock(&ftable_mutex);

    boolean result = fseeko(fp, (off_t) pos, SEEK_SET) == 0;

    pthread_mutex_lock(&ftable_mutex);
    /* Decrement refcount if slot is valid and has active references */
    if (slot && slot->refcount > 0)
        slot->refcount--;
    pthread_mutex_unlock(&ftable_mutex);

    return result;
}

boolean filegetposition(hdlfilenum fnum, long *ppos) {
    if (!ppos)
        return false;

    pthread_mutex_lock(&ftable_mutex);
    fnum_entry *slot = entry_from(fnum);
    if (!slot || !slot->fp) {
        pthread_mutex_unlock(&ftable_mutex);
        return false;
    }
    FILE *fp = slot->fp;
    slot->refcount++;
    pthread_mutex_unlock(&ftable_mutex);

    off_t cur = ftello(fp);

    pthread_mutex_lock(&ftable_mutex);
    /* Decrement refcount if slot is valid and has active references */
    if (slot && slot->refcount > 0)
        slot->refcount--;
    pthread_mutex_unlock(&ftable_mutex);

    if (cur < 0)
        return false;
    *ppos = (long) cur;
    return true;
}

boolean filegeteof(hdlfilenum fnum, long *ppos) {
    if (!ppos)
        return false;

    pthread_mutex_lock(&ftable_mutex);
    fnum_entry *slot = entry_from(fnum);
    if (!slot || !slot->fp) {
        pthread_mutex_unlock(&ftable_mutex);
        return false;
    }
    FILE *fp = slot->fp;
    slot->refcount++;
    pthread_mutex_unlock(&ftable_mutex);

    off_t cur = ftello(fp);
    if (cur < 0) {
        pthread_mutex_lock(&ftable_mutex);
        /* Decrement refcount if slot is valid and has active references */
        if (slot && slot->refcount > 0 && slot->fp == fp)
            slot->refcount--;
        pthread_mutex_unlock(&ftable_mutex);
        return false;
    }

    if (fseeko(fp, 0, SEEK_END) != 0) {
        pthread_mutex_lock(&ftable_mutex);
        /* Decrement refcount if slot is valid and has active references */
        if (slot && slot->refcount > 0 && slot->fp == fp)
            slot->refcount--;
        pthread_mutex_unlock(&ftable_mutex);
        return false;
    }

    off_t end = ftello(fp);
    if (end < 0) {
        pthread_mutex_lock(&ftable_mutex);
        /* Decrement refcount if slot is valid and has active references */
        if (slot && slot->refcount > 0 && slot->fp == fp)
            slot->refcount--;
        pthread_mutex_unlock(&ftable_mutex);
        return false;
    }

    if (fseeko(fp, cur, SEEK_SET) != 0) {
        pthread_mutex_lock(&ftable_mutex);
        /* Decrement refcount if slot is valid and has active references */
        if (slot && slot->refcount > 0 && slot->fp == fp)
            slot->refcount--;
        pthread_mutex_unlock(&ftable_mutex);
        return false;
    }

    pthread_mutex_lock(&ftable_mutex);
    /* Decrement refcount if slot is valid and has active references */
    if (slot && slot->refcount > 0)
        slot->refcount--;
    pthread_mutex_unlock(&ftable_mutex);

    *ppos = (long) end;
    return true;
}

boolean fileseteof(hdlfilenum fnum, long size) {
    pthread_mutex_lock(&ftable_mutex);
    fnum_entry *slot = entry_from(fnum);
    if (!slot || !slot->fp) {
        pthread_mutex_unlock(&ftable_mutex);
        return false;
    }
    FILE *fp = slot->fp;
    int fd = fileno(fp);
    slot->refcount++;
    pthread_mutex_unlock(&ftable_mutex);

    boolean result = ftruncate(fd, (off_t) size) == 0;

    pthread_mutex_lock(&ftable_mutex);
    /* Decrement refcount if slot is valid and has active references */
    if (slot && slot->refcount > 0)
        slot->refcount--;
    pthread_mutex_unlock(&ftable_mutex);

    return result;
}

boolean filewrite(hdlfilenum fnum, long ctbytes, void *pdata) {
    /* REFERENCE COUNTING PATTERN: Increment refcount while accessing file
     * to prevent closefile() from closing the FILE* mid-operation.
     *
     * 1. Lock ftable_mutex
     * 2. Get FILE* pointer and increment refcount
     * 3. RELEASE mutex
     * 4. Perform fwrite()
     * 5. Lock mutex, decrement refcount, release
     *
     * This ensures closefile() will fail if any thread has active I/O (refcount > 0).
     */
    pthread_mutex_lock(&ftable_mutex);

    fnum_entry *slot = entry_from(fnum);
    if (!slot || !slot->fp) {
        pthread_mutex_unlock(&ftable_mutex);
        return false;
    }

    FILE *fp = slot->fp;
    slot->refcount++;  /* Increment refcount while we have reference */

    pthread_mutex_unlock(&ftable_mutex);

    /* Perform I/O without holding mutex - allows concurrent access to other files */
    boolean result = fwrite(pdata, 1, (size_t) ctbytes, fp) == (size_t) ctbytes;

    /* Release reference */
    pthread_mutex_lock(&ftable_mutex);
    /* Decrement refcount if slot is valid and has active references */
    if (slot && slot->refcount > 0)
        slot->refcount--;
    pthread_mutex_unlock(&ftable_mutex);

    return result;
}

boolean fileread(hdlfilenum fnum, long ctbytes, void *pdata) {
    /* REFERENCE COUNTING PATTERN: See filewrite() for implementation details.
     * Increments refcount during I/O to prevent concurrent closefile().
     */
    pthread_mutex_lock(&ftable_mutex);

    fnum_entry *slot = entry_from(fnum);
    if (!slot || !slot->fp) {
        pthread_mutex_unlock(&ftable_mutex);
        return false;
    }

    FILE *fp = slot->fp;
    slot->refcount++;

    pthread_mutex_unlock(&ftable_mutex);

    boolean result = fread(pdata, 1, (size_t) ctbytes, fp) == (size_t) ctbytes;

    pthread_mutex_lock(&ftable_mutex);
    /* Decrement refcount if slot is valid and has active references */
    if (slot && slot->refcount > 0)
        slot->refcount--;
    pthread_mutex_unlock(&ftable_mutex);

    return result;
}

boolean filereaddata(hdlfilenum fnum, long ctread, long *pctactual, void *pbuf) {
    if (!pctactual)
        return false;

    pthread_mutex_lock(&ftable_mutex);

    fnum_entry *slot = entry_from(fnum);
    if (!slot || !slot->fp) {
        pthread_mutex_unlock(&ftable_mutex);
        return false;
    }

    FILE *fp = slot->fp;
    slot->refcount++;

    pthread_mutex_unlock(&ftable_mutex);

    size_t n = fread(pbuf, 1, (size_t) ctread, fp);
    *pctactual = (long) n;

    pthread_mutex_lock(&ftable_mutex);
    /* Decrement refcount if slot is valid and has active references */
    if (slot && slot->refcount > 0)
        slot->refcount--;
    pthread_mutex_unlock(&ftable_mutex);

    return true;
}

long filegetsize(hdlfilenum fnum) {
    long eof = 0;
    if (!filegeteof(fnum, &eof))
        return -1;
    return eof;
}

boolean fileputchar(hdlfilenum fnum, char ch) {
    pthread_mutex_lock(&ftable_mutex);
    fnum_entry *slot = entry_from(fnum);
    if (!slot || !slot->fp) {
        pthread_mutex_unlock(&ftable_mutex);
        return false;
    }
    FILE *fp = slot->fp;
    slot->refcount++;
    pthread_mutex_unlock(&ftable_mutex);

    boolean result = fputc((unsigned char) ch, fp) != EOF;

    pthread_mutex_lock(&ftable_mutex);
    /* Decrement refcount if slot is valid and has active references */
    if (slot && slot->refcount > 0)
        slot->refcount--;
    pthread_mutex_unlock(&ftable_mutex);

    return result;
}

boolean filegetchar(hdlfilenum fnum, char *ch) {
    if (!ch)
        return false;

    pthread_mutex_lock(&ftable_mutex);
    fnum_entry *slot = entry_from(fnum);
    if (!slot || !slot->fp) {
        pthread_mutex_unlock(&ftable_mutex);
        return false;
    }
    FILE *fp = slot->fp;
    slot->refcount++;
    pthread_mutex_unlock(&ftable_mutex);

    int c = fgetc(fp);

    pthread_mutex_lock(&ftable_mutex);
    /* Decrement refcount if slot is valid and has active references */
    if (slot && slot->refcount > 0)
        slot->refcount--;
    pthread_mutex_unlock(&ftable_mutex);

    if (c == EOF)
        return false;
    *ch = (char) c;
    return true;
}

boolean filewritehandle(hdlfilenum fnum, Handle h) {
    if (!h)
        return false;
    return filewrite(fnum, gethandlesize(h), *h);
}

boolean filereadhandle(hdlfilenum fnum, Handle *h) {
    if (!h)
        return false;
    long size = filegetsize(fnum);
    if (size < 0)
        return false;
    if (!newhandle(size, h))
        return false;
    if (!fileread(fnum, size, **h)) {
        disposehandle(*h);
        *h = nil;
        return false;
    }
    return true;
}

boolean flushvolumechanges(const ptrfilespec fs, hdlfilenum fnum) {
    (void) fs;
    (void) fnum;
    return true;
}

boolean largefilebuffer(Handle *hbuffer) {
    if (!hbuffer)
        return false;
    long sz = 32 * 1024;
    return newhandle(sz, hbuffer);
}

boolean fileexists(const ptrfilespec fs, boolean *flfolder) {
    bigstring bspath;
    char cpath[PATH_MAX];  /* Use PATH_MAX instead of hardcoded 512 */
    struct stat st;

    if (!fs)
        return false;

    if (flfolder)
        *flfolder = false;

    /* Convert filespec to path */
    if (!filespectopath(fs, bspath))
        return false;

    /* Convert bigstring to C string */
    copyptocstring(bspath, cpath);

    /* Check if file exists using stat */
    if (stat(cpath, &st) != 0)
        return false;

    /* Set folder flag if it's a directory */
    if (flfolder)
        *flfolder = S_ISDIR(st.st_mode);

    return true;
}

boolean fileisfolder(const ptrfilespec fs, boolean *out) {
    if (out)
        *out = false;
    (void) fs;
    return true;
}

boolean fileisvolume(const ptrfilespec fs) {
    (void) fs;
    return false;
}

boolean equalfilespecs(const ptrfilespec a, const ptrfilespec b) {
    if (!a || !b)
        return false;
    if (a->name.length != b->name.length)
        return false;
    for (unsigned int i = 0; i < a->name.length; ++i) {
        if (a->name.unicode[i] != b->name.unicode[i])
            return false;
    }
    return true;
}

boolean getfsfile(const ptrfilespec pfs, bigstring name) {
    if (!pfs) {
        setemptystring(name);
        return false;
    }
    unsigned int len = pfs->name.length;
    if (len > lenbigstring)
        len = lenbigstring;
    name[0] = (unsigned char) len;
    for (unsigned int i = 0; i < len; ++i)
        name[1 + i] = (unsigned char) (pfs->name.unicode[i] & 0xFF);
    return true;
}

long headless_readline(hdlfilenum fnum, char *buf, long bufsz) {
    if (bufsz <= 0)
        return -1;

    pthread_mutex_lock(&ftable_mutex);
    fnum_entry *slot = entry_from(fnum);
    if (!slot || !slot->fp) {
        pthread_mutex_unlock(&ftable_mutex);
        return -1;
    }
    FILE *fp = slot->fp;
    slot->refcount++;
    pthread_mutex_unlock(&ftable_mutex);

    long n = 0;
    int c = EOF;
    while (1) {
        c = fgetc(fp);
        if (c == EOF)
            break;
        if (c == '\n')
            break;
        if (c == '\r') {
            int next = fgetc(fp);
            if (next != '\n' && next != EOF)
                ungetc(next, fp);
            break;
        }
        if (n < bufsz - 1)
            buf[n++] = (char) c;
    }

    pthread_mutex_lock(&ftable_mutex);
    /* Decrement refcount if slot is valid and has active references */
    if (slot && slot->refcount > 0)
        slot->refcount--;
    pthread_mutex_unlock(&ftable_mutex);

    buf[(n < bufsz) ? n : (bufsz - 1)] = '\0';
    if (c == EOF && n == 0)
        return 0;
    return n;
}

const char *headless_fnum_path(hdlfilenum fnum) {
    /* Note: This function returns a pointer into the ftable, so caller must
     * understand the pointer may become invalid if another thread calls
     * closefile() or headless_reopen_fnum(). For true thread-safety of the
     * returned string, consider having the caller copy it. */
    pthread_mutex_lock(&ftable_mutex);

    fnum_entry *slot = entry_from(fnum);
    const char *result = (slot && slot->fp && slot->path[0]) ? slot->path : NULL;

    pthread_mutex_unlock(&ftable_mutex);
    return result;
}

boolean headless_reopen_fnum(hdlfilenum fnum, const char *path, boolean flreadonly) {
    if (!path || path[0] == '\0')
        return false;

    const char *mode = flreadonly ? "rb" : "rb+";
    FILE *fp = fopen(path, mode);
    if (!fp && !flreadonly)
        fp = fopen(path, "rb");
    if (!fp)
        return false;

    /* Protect ftable modification with mutex */
    pthread_mutex_lock(&ftable_mutex);

    fnum_entry *slot = entry_from(fnum);
    if (!slot) {
        pthread_mutex_unlock(&ftable_mutex);
        fclose(fp);
        return false;
    }

    if (slot->fp) {
        fclose(slot->fp);
        slot->fp = NULL;
    }

    slot->fp = fp;
    slot->refcount = 0;  /* Reset refcount when reopening file */
    strncpy(slot->path, path, sizeof slot->path - 1);
    slot->path[sizeof slot->path - 1] = '\0';

    pthread_mutex_unlock(&ftable_mutex);
    return true;
}

/*
 * filegetdefaultpath - Portable implementation for headless mode
 *
 * Returns the thread-local working directory as a filespec.
 * This replaces the Mac-specific implementation in filepath.c.
 */
boolean filegetdefaultpath(ptrfilespec fs) {
    bigstring bspath;

    if (!fs) {
        log_error(LOG_COMP_FILE, "filegetdefaultpath: NULL fs parameter");
        return false;
    }

    /* Get thread-local working directory */
    if (!get_thread_working_dir(bspath)) {
        log_error(LOG_COMP_FILE, "filegetdefaultpath: get_thread_working_dir failed");
        return false;
    }

    /* Convert bigstring path to filespec */
    if (!pathtofilespec(bspath, fs)) {
        log_error(LOG_COMP_FILE, "filegetdefaultpath: pathtofilespec failed for path len=%d", (int)bspath[0]);
        return false;
    }

    log_debug(LOG_COMP_FILE, "filegetdefaultpath: SUCCESS path len=%d", (int)bspath[0]);
    return true;
}

/*
 * filesetdefaultpath - Portable implementation for headless mode
 *
 * Sets the thread-local working directory from a filespec.
 * This replaces the Mac-specific implementation in filepath.c.
 */
boolean filesetdefaultpath(const ptrfilespec fs) {
    bigstring bspath;

    if (!fs) {
        log_error(LOG_COMP_FILE, "filesetdefaultpath: NULL fs parameter");
        return false;
    }

    /* Convert filespec to bigstring path */
    if (!filespectopath(fs, bspath)) {
        log_error(LOG_COMP_FILE, "filesetdefaultpath: filespectopath failed");
        return false;
    }

    /* Set thread-local working directory */
    if (!set_thread_working_dir(bspath)) {
        log_error(LOG_COMP_FILE, "filesetdefaultpath: set_thread_working_dir failed for path len=%d", (int)bspath[0]);
        return false;
    }

    log_debug(LOG_COMP_FILE, "filesetdefaultpath: SUCCESS path len=%d", (int)bspath[0]);
    return true;
}

/*
 * setfilemodified - Portable implementation for headless mode
 *
 * Sets the modification time of a file. This is fully portable across
 * POSIX systems (macOS, Linux, BSD, etc.).
 *
 * Uses utimensat() which is part of POSIX.1-2008.
 */
boolean setfilemodified(const ptrfilespec fs, const long when) {
    char path[4096];
    struct timespec times[2];
    struct stat st;

    if (!fs) {
        log_error(LOG_COMP_FILE, "setfilemodified: NULL fs parameter");
        return false;
    }

    /* Convert filespec to path */
    if (!path_from_filespec(fs, path, sizeof(path))) {
        log_error(LOG_COMP_FILE, "setfilemodified: path_from_filespec failed");
        return false;
    }

    /* Get current file times first (to preserve access time) */
    if (stat(path, &st) != 0) {
        log_error(LOG_COMP_FILE, "setfilemodified: stat failed for %s: %s", path, strerror(errno));
        return false;
    }

    /* Set access time to UTIME_OMIT to preserve it */
    times[0].tv_sec = 0;
    times[0].tv_nsec = UTIME_OMIT;

    /* Set modification time to the specified value
     * Note: 'when' is in Frontier time (seconds since 1904-01-01)
     * Convert to Unix time (seconds since 1970-01-01)
     */
    int64_t unix_time = (int64_t)when - FRONTIER_EPOCH_TO_UNIX_OFFSET;
    times[1].tv_sec = (time_t)unix_time;
    times[1].tv_nsec = 0;

    /* Use utimensat with AT_FDCWD to operate on path */
    if (utimensat(AT_FDCWD, path, times, 0) != 0) {
        log_error(LOG_COMP_FILE, "setfilemodified: utimensat failed for %s: %s", path, strerror(errno));
        return false;
    }

    log_debug(LOG_COMP_FILE, "setfilemodified: SUCCESS for %s, time=%ld", path, when);
    return true;
}

/*
 * setfilecreated - Portable implementation for headless mode
 *
 * Sets the creation time of a file. Platform support varies:
 * - macOS: Supports creation time via setattrlist()
 * - Linux: Most filesystems don't support setting creation time (birth time is read-only)
 * - Windows: Supports creation time via SetFileTime()
 *
 * For maximum portability, this implementation:
 * - On macOS: Uses setattrlist() to set creation time
 * - On other platforms: Returns error (creation time is typically read-only)
 */
boolean setfilecreated(const ptrfilespec fs, const long when) {
    char path[4096];

    if (!fs) {
        log_error(LOG_COMP_FILE, "setfilecreated: NULL fs parameter");
        return false;
    }

    /* Convert filespec to path */
    if (!path_from_filespec(fs, path, sizeof(path))) {
        log_error(LOG_COMP_FILE, "setfilecreated: path_from_filespec failed");
        return false;
    }

#ifdef __APPLE__
    /* macOS: Use setattrlist() to set creation time */
    struct attrlist attrList;
    struct {
        struct timespec creationTime;
    } attrBuf;

    /* Convert Frontier time to Unix time */
    int64_t unix_time = (int64_t)when - FRONTIER_EPOCH_TO_UNIX_OFFSET;

    /* Set up attribute list */
    memset(&attrList, 0, sizeof(attrList));
    attrList.bitmapcount = ATTR_BIT_MAP_COUNT;
    attrList.commonattr = ATTR_CMN_CRTIME;

    /* Set creation time */
    attrBuf.creationTime.tv_sec = (time_t)unix_time;
    attrBuf.creationTime.tv_nsec = 0;

    if (setattrlist(path, &attrList, &attrBuf, sizeof(attrBuf), 0) != 0) {
        log_error(LOG_COMP_FILE, "setfilecreated: setattrlist failed for %s: %s", path, strerror(errno));
        return false;
    }

    log_debug(LOG_COMP_FILE, "setfilecreated: SUCCESS for %s, time=%ld", path, when);
    return true;

#else
    /* Linux/other platforms: Setting creation time is not supported on most filesystems */
    log_warn(LOG_COMP_FILE, "setfilecreated: Not supported on this platform (%s)", path);

    /* Return false to indicate operation not supported
     * This matches the behavior of Mac Frontier when operations aren't available
     */
    return false;
#endif
}
