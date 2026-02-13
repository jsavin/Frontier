/*
 * headless_window_verbs.c - Window processor verbs for headless mode
 *
 * Originally auto-generated, now manually maintained for headless support.
 * Implements window verbs with reasonable defaults for headless operation.
 *
 * Key stubs that return defaults (not errors):
 * - window.getSize: returns 400x500 (reasonable default), returns true
 * - window.getPosition: returns 100,100 (reasonable default), returns true
 * - window.setSize/setPosition: accept params but return false (no-op indicator)
 * - window.getTitle: returns object's full dot-path, returns the path string
 * - window.setTitle: accepts params but returns false (no-op indicator)
 * - window.show/hide: consume title param, return true (no-op)
 * - window.isVisible: consumes title param, returns false (nothing visible)
 *
 * Note on setter return values: Setter verbs (setSize, setPosition, setTitle)
 * return false in headless mode to indicate the operation was a no-op. This
 * allows callers to detect headless mode if needed, while still allowing
 * scripts to run without errors.
 *
 * This allows UserTalk glue scripts (like op.outlineToXml) to work in headless
 * mode. Per OPML 2.0 spec, window-related metadata (expansionState, scrollState,
 * windowTop/Left/Bottom/Right) are all optional, so using defaults is valid.
 */

#include "frontier.h"
#include "standard.h"

#include <errno.h>
#include <limits.h>
#include <string.h>
#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"
#include "odbinternal.h"
#include "logging.h"

/* From file_portable.c — declared locally because #include "file.h"
 * pulls in definitions that cause startup crashes in headless mode.
 * These functions are linked from file_portable.o. */
extern const char *headless_fnum_path(hdlfilenum);
extern boolean filespectopath(const ptrfilespec, bigstring);

/* Convert a pascal bigstring to a null-terminated C string.
 * Truncates to destsize-1 if the string is longer. */
static void pstrtocstr(bigstring bs, char *dest, size_t destsize) {
    short len = stringlength(bs);
    if (len >= (short)destsize) len = (short)destsize - 1;
    memcpy(dest, stringbaseaddress(bs), len);
    dest[len] = '\0';
}

/* Token enum for all verbs in the window processor */
enum {
    winv_isopen = 0,
    winv_open = 1,
    winv_isfront = 2,
    winv_bringtofront = 3,
    winv_sendtoback = 4,
    winv_frontmost = 5,
    winv_next = 6,
    winv_isvisible = 7,
    winv_show = 8,
    winv_hide = 9,
    winv_close = 10,
    winv_update = 11,
    winv_ismenuscript = 12,
    winv_getposition = 13,
    winv_setposition = 14,
    winv_getsize = 15,
    winv_setsize = 16,
    winv_zoom = 17,
    winv_runselection = 18,
    winv_scroll = 19,
    winv_msg = 20,
    winv_dbstats = 21,
    winv_quickscript = 22,
    winv_ismodified = 23,
    winv_setmodified = 24,
    winv_gettitle = 25,
    winv_settitle = 26,
    winv_about = 27,
    winv_getfile = 28,
    winv_isreadonly = 29,
    winv_setquickscript = 30
};

static boolean window_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    (void)hparam1;
    (void)vreturned;
    switch(token) {
        case winv_isopen: {
            /* window.isOpen(x) — check if a database "window" is open.
             *
             * Legacy Frontier tries two resolution paths:
             *   Path A: Treat param as an address. If it resolves to the root
             *           table of any opened database, return true.
             *   Path B: If address resolution fails, treat as a string file path.
             *           Check if it matches the file path of the system root
             *           or any opened guest database.
             *
             * In headless mode there are no actual windows, but root tables of
             * opened databases are considered "always-open hidden windows."
             * This allows Frontier.openDataFile() to detect already-opened DBs. */

            flnextparamislast = true;

            /* Path A: try address resolution */
            {
                tyvaluerecord addrval;

                disablelangerror();
                boolean fladdrparam = getaddressparam(hparam1, 1, &addrval);
                enablelangerror();

                if (fladdrparam) {
                    hdlhashtable htable;
                    bigstring bsname;

                    if (getaddressvalue(addrval, &htable, bsname)) {
                        /* htable is the parent table of the addressed node.
                         * Only the root table of an opened database is considered
                         * "open" in headless mode — sub-tables like @system are not.
                         *
                         * htable == nil: address IS the root table (@root)
                         * htable == filewindowtable: address is a guest DB root */
                        if (htable == nil)
                            return setbooleanvalue(true, vreturned);

                        if (filewindowtable != nil && htable == filewindowtable)
                            return setbooleanvalue(true, vreturned);
                    }

                    /* Address resolved but not a database root — no editor window */
                    return setbooleanvalue(false, vreturned);
                }
            }

            /* Path B: fall back to string (file path) */
            {
                bigstring bspath;

                if (!getstringvalue(hparam1, 1, bspath))
                    return false;

                char inputpath[PATH_MAX];
                pstrtocstr(bspath, inputpath, sizeof(inputpath));

                /* Resolve to absolute path for reliable comparison.
                 * If the path doesn't exist or is inaccessible, raise a
                 * script-level error rather than silently returning false. */
                char resolvedinput[PATH_MAX];
                if (realpath(inputpath, resolvedinput) == NULL) {
                    int saved_errno = errno;

                    log_debug(LOG_COMP_LANG, "window.isOpen: realpath failed for \"%s\": %s",
                              inputpath, strerror(saved_errno));

                    bigstring bserrmsg;
                    bigstring bsinputpath;
                    copyctopstring(inputpath, bsinputpath);

                    if (saved_errno == ENOENT) {
                        copystring(BIGSTRING("\x14" "Can't check window \""), bserrmsg);
                        pushstring(bsinputpath, bserrmsg);
                        pushstring(BIGSTRING("\x16" "\": file does not exist"), bserrmsg);
                    }
                    else if (saved_errno == EACCES) {
                        copystring(BIGSTRING("\x14" "Can't check window \""), bserrmsg);
                        pushstring(bsinputpath, bserrmsg);
                        pushstring(BIGSTRING("\x14" "\": permission denied"), bserrmsg);
                    }
                    else {
                        copystring(BIGSTRING("\x14" "Can't check window \""), bserrmsg);
                        pushstring(bsinputpath, bserrmsg);
                        pushstring(BIGSTRING("\x19" "\": path resolution failed"), bserrmsg);
                    }

                    langerrormessage(bserrmsg);
                    return false;
                }

                /* Check system root database file path */
                if (databasedata != nil) {
                    const char *sysroot = headless_fnum_path(
                        (hdlfilenum)(**databasedata).fnumdatabase);

                    if (sysroot != nil) {
                        char resolvedsys[PATH_MAX];
                        if (realpath(sysroot, resolvedsys) != NULL
                            && strcmp(resolvedinput, resolvedsys) == 0)
                            return setbooleanvalue(true, vreturned);
                    }
                }

                /* Check guest database file paths */
                if (hodblist != nil) {
                    hdlodbrecord hodb;

                    for (hodb = (**hodblist).hnext;
                         hodb != nil && hodb != hodblist;
                         hodb = (**hodb).hnext) {

                        /* Convert guest DB filespec to C string for realpath() */
                        bigstring bsguest;
                        if (filespectopath(&(**hodb).fs, bsguest)) {
                            char guestpath[PATH_MAX];
                            pstrtocstr(bsguest, guestpath, sizeof(guestpath));

                            char resolvedguest[PATH_MAX];
                            if (realpath(guestpath, resolvedguest) != NULL
                                && strcmp(resolvedinput, resolvedguest) == 0)
                                return setbooleanvalue(true, vreturned);
                        }
                    }
                }

                /* No match — not open */
                return setbooleanvalue(false, vreturned);
            }
        }
        case winv_open:
            /* Verb: window.open - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case winv_isfront:
            /* Verb: window.isfront - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case winv_bringtofront:
            /* Verb: window.bringtofront - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case winv_sendtoback:
            /* Verb: window.sendtoback - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case winv_frontmost:
            /* window.frontmost - no-op in headless mode, returns empty string */
            return setstringvalue(BIGSTRING("\x00"), vreturned);
        case winv_next:
            /* Verb: window.next - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case winv_isvisible: {
            /* window.isVisible(title) - always false in headless mode */
            bigstring bstitle;

            flnextparamislast = true;
            if (!getstringvalue(hparam1, 1, bstitle))
                return false;

            return setbooleanvalue(false, vreturned);
        }
        case winv_show:
        case winv_hide: {
            /* window.show/hide(title) - no-op in headless mode */
            bigstring bstitle;

            flnextparamislast = true;
            if (!getstringvalue(hparam1, 1, bstitle))
                return false;

            return setbooleanvalue(true, vreturned);
        }
        case winv_close:
            /* Verb: window.close - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case winv_update:
            /* window.update - no-op in headless mode (no GUI to update) */
            setbooleanvalue(true, vreturned);
            return true;
        case winv_ismenuscript:
            /* Verb: window.ismenuscript - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case winv_getposition: {
            /* window.getPosition(title, horizAddr, vertAddr)
             * In headless mode, return reasonable default position (100, 100).
             * This allows UserTalk scripts that call window.getPosition to work. */
            bigstring bstitle;

            if (!getstringvalue(hparam1, 1, bstitle))
                return false;

            if (!langsetlongvarparam(hparam1, 2, 100))  /* horiz = 100 */
                return false;

            flnextparamislast = true;

            if (!langsetlongvarparam(hparam1, 3, 100))  /* vert = 100 */
                return false;

            return setbooleanvalue(true, vreturned);
        }
        case winv_setposition: {
            /* window.setPosition(title, horiz, vert)
             * In headless mode, accept but ignore position changes.
             * Returns false so callers know the position wasn't actually set. */
            bigstring bstitle;
            long horiz, vert;

            if (!getstringvalue(hparam1, 1, bstitle))
                return false;

            if (!getlongvalue(hparam1, 2, &horiz))
                return false;

            flnextparamislast = true;

            if (!getlongvalue(hparam1, 3, &vert))
                return false;

            /* Return false - no window to position in headless mode */
            return setbooleanvalue(false, vreturned);
        }
        case winv_getsize: {
            /* window.getSize(title, widthAddr, heightAddr)
             * In headless mode, return reasonable default size (400, 500).
             * This allows UserTalk scripts that call window.getSize to work. */
            bigstring bstitle;

            if (!getstringvalue(hparam1, 1, bstitle))
                return false;

            if (!langsetlongvarparam(hparam1, 2, 400))  /* width = 400 */
                return false;

            flnextparamislast = true;

            if (!langsetlongvarparam(hparam1, 3, 500))  /* height = 500 */
                return false;

            return setbooleanvalue(true, vreturned);
        }
        case winv_setsize: {
            /* window.setSize(title, width, height)
             * In headless mode, accept but ignore size changes.
             * Returns false so callers know the size wasn't actually set. */
            bigstring bstitle;
            long width, height;

            if (!getstringvalue(hparam1, 1, bstitle))
                return false;

            if (!getlongvalue(hparam1, 2, &width))
                return false;

            flnextparamislast = true;

            if (!getlongvalue(hparam1, 3, &height))
                return false;

            /* Return false - no window to resize in headless mode */
            return setbooleanvalue(false, vreturned);
        }
        case winv_zoom:
            /* Verb: window.zoom - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case winv_runselection:
            /* Verb: window.runselection - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case winv_scroll:
            /* Verb: window.scroll - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case winv_msg:
            /* window.msg - no-op in headless mode (no status bar) */
            setbooleanvalue(true, vreturned);
            return true;
        case winv_dbstats:
            /* window.dbstats - error stub */
            if (bserror)
                copystring(BIGSTRING("\pCan't use window verbs because GUI is not available in headless mode"), bserror);
            return false;
        case winv_quickscript:
            /* Verb: window.quickscript - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case winv_ismodified:
            /* Verb: window.ismodified - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case winv_setmodified:
            /* Verb: window.setmodified - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case winv_gettitle: {
            /* window.getTitle(adr)
             * In headless mode, return the full dot-path to the object.
             * This allows UserTalk scripts that call window.getTitle to work.
             * Per OPML 2.0 spec, window title is optional metadata. */
            tyvaluerecord val;
            hdlhashtable htable;
            bigstring bspath;

            setemptystring(bspath);

            flnextparamislast = true;

            if (!getaddressparam(hparam1, 1, &val))
                return false;

            if (!getaddressvalue(val, &htable, bspath))
                return false;

            (void)htable;  /* htable retrieved but unused - we only need the path */

            /* Return the full path as the "window title" */
            return setstringvalue(bspath, vreturned);
        }
        case winv_settitle: {
            /* window.setTitle(adr, title)
             * In headless mode, return false so callers know the title
             * wasn't actually set (no window exists to rename). */
            tyvaluerecord val;
            bigstring bstitle;

            if (!getaddressparam(hparam1, 1, &val))
                return false;

            flnextparamislast = true;

            if (!getstringvalue(hparam1, 2, bstitle))
                return false;

            /* Return false - no window to rename in headless mode */
            return setbooleanvalue(false, vreturned);
        }
        case winv_about:
            /* window.about - no-op in headless mode (no GUI to display About window) */
            setbooleanvalue(true, vreturned);
            return true;
        case winv_getfile:
            /* Verb: window.getfile - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case winv_isreadonly:
            /* Verb: window.isreadonly - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case winv_setquickscript:
            /* Verb: window.setquickscript - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean windowinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pwindow"), bsname);

    if (!newfunctionprocessor(bsname, &window_valueproc, false, &htable))
        return false;

    pushhashtable(htable);

    #define ADD_VERB(name, tok) do { \
        bigstring bs; \
        copystring(name, bs); \
        if (!langaddkeyword(bs, tok)) { \
            pophashtable(); \
            return false; \
        } \
    } while(0)

    ADD_VERB(BIGSTRING("\pisopen"), winv_isopen);
    ADD_VERB(BIGSTRING("\popen"), winv_open);
    ADD_VERB(BIGSTRING("\pisfront"), winv_isfront);
    ADD_VERB(BIGSTRING("\pbringtofront"), winv_bringtofront);
    ADD_VERB(BIGSTRING("\psendtoback"), winv_sendtoback);
    ADD_VERB(BIGSTRING("\pfrontmost"), winv_frontmost);
    ADD_VERB(BIGSTRING("\pnext"), winv_next);
    ADD_VERB(BIGSTRING("\pisvisible"), winv_isvisible);
    ADD_VERB(BIGSTRING("\pshow"), winv_show);
    ADD_VERB(BIGSTRING("\phide"), winv_hide);
    ADD_VERB(BIGSTRING("\pclose"), winv_close);
    ADD_VERB(BIGSTRING("\pupdate"), winv_update);
    ADD_VERB(BIGSTRING("\pismenuscript"), winv_ismenuscript);
    ADD_VERB(BIGSTRING("\pgetposition"), winv_getposition);
    ADD_VERB(BIGSTRING("\psetposition"), winv_setposition);
    ADD_VERB(BIGSTRING("\pgetsize"), winv_getsize);
    ADD_VERB(BIGSTRING("\psetsize"), winv_setsize);
    ADD_VERB(BIGSTRING("\pzoom"), winv_zoom);
    ADD_VERB(BIGSTRING("\prunselection"), winv_runselection);
    ADD_VERB(BIGSTRING("\pscroll"), winv_scroll);
    ADD_VERB(BIGSTRING("\pmsg"), winv_msg);
    ADD_VERB(BIGSTRING("\pdbstats"), winv_dbstats);
    ADD_VERB(BIGSTRING("\pquickscript"), winv_quickscript);
    ADD_VERB(BIGSTRING("\pismodified"), winv_ismodified);
    ADD_VERB(BIGSTRING("\psetmodified"), winv_setmodified);
    ADD_VERB(BIGSTRING("\pgettitle"), winv_gettitle);
    ADD_VERB(BIGSTRING("\psettitle"), winv_settitle);
    ADD_VERB(BIGSTRING("\pabout"), winv_about);
    ADD_VERB(BIGSTRING("\pgetfile"), winv_getfile);
    ADD_VERB(BIGSTRING("\pisreadonly"), winv_isreadonly);
    ADD_VERB(BIGSTRING("\psetquickscript"), winv_setquickscript);

    #undef ADD_VERB

    pophashtable();
    return true;
}
