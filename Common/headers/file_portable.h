/* file_portable.h - Platform-independent file path operations */

#ifndef __FILE_PORTABLE_H__
#define __FILE_PORTABLE_H__

#include "frontier.h"

/*
 * Platform-independent file path operations.
 *
 * These functions handle path parsing and manipulation
 * for different platforms (POSIX, Windows).
 */

/* Extract folder/directory from path */
boolean portable_folderfrompath(const bigstring bspath, bigstring bsfolder);

/* Extract filename from path */
boolean portable_filefrompath(const bigstring bspath, bigstring bsfile);

/* Get path separator for current platform */
char portable_getpathsep(void);

/* Split path into components */
boolean portable_splitpath(const bigstring bspath, bigstring bsfolder,
                          bigstring bsfile);

#endif /* __FILE_PORTABLE_H__ */
