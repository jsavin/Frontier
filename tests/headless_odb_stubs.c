#include <stdio.h>

#include "frontier.h"
#include "standard.h"
#include "dialogs.h"
#include "odbinternal.h"
#include "tablestructure.h"
#include "db_format.h"
#include "db.h"

/* Minimal headless stubs to satisfy ODB engine dependencies in tests */

boolean alertdialog (bigstring s) { (void)s; return true; }

WindowPtr shellfindfilewindow (const ptrfilespec fs) { (void)fs; return (WindowPtr)0; }

boolean shellsave (WindowPtr w) { (void)w; return true; }

typedef struct tythreadglobalsdummy {} *hdlthreadglobals;

hdlthreadglobals getcurrentthreadglobals (void) { return (hdlthreadglobals)0; }

void copythreadglobals (hdlthreadglobals ht) { (void)ht; }

void swapinthreadglobals (hdlthreadglobals ht) { (void)ht; }

boolean tablevalidate (hdlhashtable ht, boolean fl) { (void)ht; (void)fl; return true; }

/* provided by headless_file_portable.c */
extern const char* headless_fnum_path(hdlfilenum fnum);

boolean db_migrate_reopen_if_legacy(odbref *podb) {
    (void)podb;
    db_format_mode mode = db_format_mode_current();
    if (mode.use_64bit_format)
        return true;

    if (databasedata == nil)
        return false;

    hdlfilenum fnum = (hdlfilenum) ((**databasedata).fnumdatabase);
    const char *path = headless_fnum_path(fnum);
    if (!path)
        return false;

    if (!migrate_32bit_to_64bit(path))
        return false;
    char migrated_path[1024];
    if (!db_format_last_backup_path(migrated_path, sizeof migrated_path))
        return false;
    remove(path);
    if (rename(migrated_path, path) != 0)
        return false;

    mode.use_64bit_format = true;
    db_format_mode_apply(&mode);
    return true;
}
