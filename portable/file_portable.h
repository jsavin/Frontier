#ifndef PORTABLE_FILE_PORTABLE_H
#define PORTABLE_FILE_PORTABLE_H

#include "file.h"

const char *headless_fnum_path(hdlfilenum fnum);
boolean headless_reopen_fnum(hdlfilenum fnum, const char *path, boolean flreadonly);

#endif /* PORTABLE_FILE_PORTABLE_H */
