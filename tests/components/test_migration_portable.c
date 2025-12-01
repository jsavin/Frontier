/* 2025-11-30 Codex: Use only the portable file helpers and implement local copy/delete helpers. */

#include "test_migration_portable.h"
#include "../../Common/headers/strings.h"
#include <stdio.h>

boolean tm_pathtofilespec(const char *path, tyfilespec *fs) {
    bigstring bs;
    copyctopstring(path, bs);
    return pathtofilespec(bs, fs);
}

boolean tm_openfile(tyfilespec *fs, hdlfilenum *fnum, boolean readonly) {
    return openfile(fs, fnum, readonly);
}

boolean tm_opennewfile(tyfilespec *fs, OSType creator, OSType filetype, hdlfilenum *fnum) {
    return opennewfile(fs, creator, filetype, fnum);
}

boolean tm_closefile(hdlfilenum fnum) {
    return closefile(fnum);
}

boolean tm_fileread(hdlfilenum fnum, long ctbytes, void *buf) {
    return fileread(fnum, ctbytes, buf);
}

boolean tm_filewrite(hdlfilenum fnum, long ctbytes, const void *buf) {
    return filewrite(fnum, ctbytes, (void *) buf);
}

boolean tm_filegeteof(hdlfilenum fnum, long *eof) {
    return filegeteof(fnum, eof);
}

boolean tm_filecopy_path(const char *src, const char *dst) {
    bigstring bssrc, bsdst; tyfilespec fssrc, fsdst;
    hdlfilenum srcf = 0, dstf = 0;
    copyctopstring(src, bssrc);
    copyctopstring(dst, bsdst);
    if (!pathtofilespec(bssrc, &fssrc) || !pathtofilespec(bsdst, &fsdst))
        return false;
    if (!openfile(&fssrc, &srcf, true))
        return false;
    boolean ok = opennewfile(&fsdst, 'LAND', 'ROOT', &dstf);
    if (ok) {
        long eof = 0;
        ok = filegeteof(srcf, &eof);
        char buffer[4096];
        while (ok && eof > 0) {
            long chunk = eof > (long) sizeof buffer ? (long) sizeof buffer : eof;
            ok = fileread(srcf, chunk, buffer) && filewrite(dstf, chunk, buffer);
            eof -= chunk;
        }
    }
    if (srcf != 0)
        closefile(srcf);
    if (dstf != 0)
        closefile(dstf);
    if (!ok)
        tm_remove_if_exists(dst);
    return ok;
}

void tm_remove_if_exists(const char *path) {
    if (path == NULL)
        return;
    remove(path);
}
