#include "../../portable/file_portable.h"
#include "../../portable/strings_portable.h"
#include "../../portable/standard.h"

/* Simple file copy using portable file APIs. */
boolean test_copy_file(const char *src, const char *dst) {
    bigstring bssrc, bsdst; tyfilespec fssrc, fsdst;
    copyctopstring(src, bssrc);
    copyctopstring(dst, bsdst);
    if (!pathtofilespec(bssrc, &fssrc) || !pathtofilespec(bsdst, &fsdst))
        return false;
    return filecopy(&fssrc, &fsdst);
}

/* Remove file if present. */
void test_remove_if_exists(const char *path) {
    if (path == NULL)
        return;
    bigstring bs; tyfilespec fs;
    copyctopstring(path, bs);
    if (pathtofilespec(bs, &fs))
        filedelete(&fs);
}
