#ifndef TEST_MIGRATION_PORTABLE_H
#define TEST_MIGRATION_PORTABLE_H

#include "../../portable/file_portable.h"
#include "../../portable/strings_portable.h"

boolean tm_pathtofilespec(const char *path, tyfilespec *fs);
boolean tm_openfile(const tyfilespec *fs, hdlfilenum *fnum, boolean readonly);
boolean tm_opennewfile(const tyfilespec *fs, OSType creator, OSType filetype, hdlfilenum *fnum);
boolean tm_closefile(hdlfilenum fnum);
boolean tm_fileread(hdlfilenum fnum, long ctbytes, void *buf);
boolean tm_filewrite(hdlfilenum fnum, long ctbytes, const void *buf);
boolean tm_filegeteof(hdlfilenum fnum, long *eof);
boolean tm_filecopy_path(const char *src, const char *dst);
void tm_remove_if_exists(const char *path);

#endif /* TEST_MIGRATION_PORTABLE_H */
