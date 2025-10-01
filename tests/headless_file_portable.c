#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "frontier.h"
#include "standard.h"
#include "file.h"

/* Simple headless portable file layer for tests */

typedef struct {
    FILE *fp;
    char path[4096];
} fnum_entry;

#define MAX_FNUM 256
static fnum_entry ftable[MAX_FNUM];

static hdlfilenum alloc_fnum(void) {
    for (int i = 1; i < MAX_FNUM; i++) if (ftable[i].fp == NULL) return (hdlfilenum)i;
    return 0;
}

static FILE* fp_from(hdlfilenum fnum) {
    if (fnum <= 0 || fnum >= MAX_FNUM) return NULL;
    return ftable[fnum].fp;
}

/* Convert bigstring <-> C */
static void bs_to_c(const bigstring bs, char *out, size_t outsz) {
    size_t len = stringlength(bs);
    if (len >= outsz) len = outsz - 1;
    memcpy(out, stringbaseaddress(bs), len);
    out[len] = '\0';
}

static void c_to_bs(const char *s, bigstring bs) {
    size_t len = strlen(s);
    if (len > lenbigstring) len = lenbigstring;
    bs[0] = (unsigned char)len;
    memcpy(&bs[1], s, len);
}

/* Basic error and path helpers */
boolean oserror (OSErr err) { (void)err; return false; }
boolean pathtofilespec (bigstring bspath, ptrfilespec fs) {
    if (!fs) return false;
    if (isemptystring(bspath)) return false;
    /* Store the POSIX path in the portable fs->name field (UTF-16 per PortableUniStr255) */
    const unsigned char *src = (const unsigned char *) bspath;
    unsigned int len = src[0];
    if (len > 255) len = 255;
    fs->name.length = (unsigned short) len;
    for (unsigned int i = 0; i < len; i++) {
        /* simple widening: ASCII/UTF-8 byte to UTF-16 code unit */
        fs->name.unicode[i] = (unsigned short) src[1 + i];
    }
    fs->flags.flvolume = false;
    return true;
}

/* Path conversion handled elsewhere in headless stubs */

/* Open/Close */
boolean openfile (const ptrfilespec fs, hdlfilenum *pfnum, boolean flreadonly) {
    if (!pfnum || !fs) return false;
    /* Reconstruct path from fs->name (PortableUniStr255). */
    char path[4096];
    unsigned int len = fs->name.length;
    if (len > sizeof(path) - 1) len = (unsigned int)sizeof(path) - 1;
    for (unsigned int i = 0; i < len; i++) {
        unsigned short u = fs->name.unicode[i];
        path[i] = (char)(u & 0xFF); /* ASCII subset */
    }
    path[len] = '\0';

    const char *mode = flreadonly ? "rb" : "rb+";
    FILE *fp = fopen(path, mode);
    if (!fp && !flreadonly) fp = fopen(path, "rb");
    if (!fp) return false;
    hdlfilenum fnum = alloc_fnum();
    if (!fnum) { fclose(fp); return false; }
    ftable[fnum].fp = fp;
    strncpy(ftable[fnum].path, path, sizeof(ftable[fnum].path) - 1);
    ftable[fnum].path[sizeof(ftable[fnum].path) - 1] = '\0';
    *pfnum = fnum;
    return true;
}

boolean closefile (hdlfilenum fnum) {
    FILE *fp = fp_from(fnum);
    if (!fp) return false;
    fclose(fp);
    ftable[fnum].fp = NULL;
    return true;
}

/* Position/size */
boolean filesetposition (hdlfilenum fnum, long pos) {
    FILE *fp = fp_from(fnum);
    if (!fp) return false;
    int r = fseeko(fp, (off_t)pos, SEEK_SET);
    return r == 0;
}

boolean filegeteof (hdlfilenum fnum, long *position) {
    FILE *fp = fp_from(fnum);
    if (!fp || !position) return false;
    off_t cur = ftello(fp);
    if (cur < 0) return false;
    if (fseeko(fp, 0, SEEK_END) != 0) return false;
    off_t end = ftello(fp);
    if (end < 0) return false;
    *position = (long)end;
    return fseeko(fp, cur, SEEK_SET) == 0;
}

boolean fileseteof (hdlfilenum fnum, long size) {
    FILE *fp = fp_from(fnum);
    if (!fp) return false;
    int fd = fileno(fp);
    return ftruncate(fd, (off_t)size) == 0;
}

long filegetsize (hdlfilenum fnum) {
    long eof = 0;
    if (!filegeteof(fnum, &eof)) return -1;
    return eof;
}

/* Read/Write */
boolean filewrite (hdlfilenum fnum, long ctbytes, void *pdata) {
    FILE *fp = fp_from(fnum);
    if (!fp) return false;
    return fwrite(pdata, 1, (size_t)ctbytes, fp) == (size_t)ctbytes;
}

boolean fileread (hdlfilenum fnum, long ctbytes, void *pdata) {
    FILE *fp = fp_from(fnum);
    if (!fp) return false;
    return fread(pdata, 1, (size_t)ctbytes, fp) == (size_t)ctbytes;
}

/* Unused stubs for this test */
boolean opennewfile (ptrfilespec fs, OSType a, OSType b, hdlfilenum *p) { (void)fs; (void)a; (void)b; (void)p; return false; }
boolean filereaddata (hdlfilenum f, long a, long *b, void *c) { (void)f;(void)a;(void)b;(void)c; return false; }
boolean filegetchar (hdlfilenum f, char *ch) { (void)f; (void)ch; return false; }
boolean fileputchar (hdlfilenum f, char ch) { (void)f; (void)ch; return false; }
boolean filewritehandle (hdlfilenum f, Handle h) { (void)f;(void)h; return false; }
boolean filereadhandle (hdlfilenum f, Handle *h) { (void)f;(void)h; return false; }
boolean flushvolumechanges (const ptrfilespec fs, hdlfilenum f) { (void)fs;(void)f; return true; }

/* Helper to expose path for a given file number (used by headless stubs) */
const char* headless_fnum_path(hdlfilenum fnum) {
    FILE *fp = fp_from(fnum);
    if (!fp) return NULL;
    return ftable[fnum].path[0] ? ftable[fnum].path : NULL;
}
