#include "test_migration_portable.h"
#include "../../portable/standard.h"
#include "../../portable/strings_portable.h"

boolean tm_pathtofilespec(const char *path, tyfilespec *fs) {
    bigstring bs;
    copyctopstring(path, bs);
    return pathtofilespec(bs, fs);
}

boolean tm_openfile(const tyfilespec *fs, hdlfilenum *fnum, boolean readonly) {
    return openfile(fs, fnum, readonly);
}

boolean tm_opennewfile(const tyfilespec *fs, OSType creator, OSType filetype, hdlfilenum *fnum) {
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
    copyctopstring(src, bssrc);
    copyctopstring(dst, bsdst);
    if (!pathtofilespec(bssrc, &fssrc) || !pathtofilespec(bsdst, &fsdst))
        return false;
    return filecopy(&fssrc, &fsdst);
}

void tm_remove_if_exists(const char *path) {
    if (path == NULL)
        return;
    bigstring bs; tyfilespec fs;
    copyctopstring(path, bs);
    if (pathtofilespec(bs, &fs))
        filedelete(&fs);
}
#include "../../Common/headers/strings.h"
