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
#include "tableverbs.h"
#include "langexternal.h"
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

            /* Extract the parameter value once, read-only.
             * We must NOT use getaddressparam() here because it calls
             * coercetoaddress() in-place on the tree node's value record.
             * If coercion fails (e.g. string that isn't a valid address),
             * setexemptaddressvalue() corrupts the value type via initvalue()
             * before checking allocation success — permanently damaging the
             * code tree node for subsequent calls. */
            tyvaluerecord paramval;

            if (!getreadonlyparamvalue(hparam1, 1, &paramval))
                return false;

            /* Path A: try address resolution (on a copy to avoid corruption) */
            {
                tyvaluerecord addrval;

                if (copyvaluerecord(paramval, &addrval)) {

                    disablelangerror();
                    boolean fladdrparam = coercetoaddress(&addrval);
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

                    /* addrval was copyvaluerecord'd from a stringvalue param.
                     * coercetoaddress failed, so addrval still holds the copied
                     * string handle (valuetype unchanged on failure). Release it. */
                    releaseheaptmp((Handle) addrval.data.stringvalue);
                }
            }

            /* Path B: fall back to string (file path).
             * Coerce the param to string using a defensive copy to avoid
             * modifying the tree node's value record. */
            {
                bigstring bspath;

                if (paramval.valuetype == stringvaluetype) {
                    pullstringvalue(&paramval, bspath);
                }
                else {
                    tyvaluerecord strval;
                    if (!copyvaluerecord(paramval, &strval))
                        return false;
                    if (!coercetostring(&strval)) {
                        releaseheaptmp((Handle) strval.data.stringvalue);
                        return false;
                    }
                    pullstringvalue(&strval, bspath);
                    releaseheaptmp((Handle) strval.data.stringvalue);
                }

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
            if (bserror) copystring(PSTRING("\017", "not implemented"), bserror);
            return false;
        case winv_isfront:
            /* Verb: window.isfront - not yet implemented */
            if (bserror) copystring(PSTRING("\017", "not implemented"), bserror);
            return false;
        case winv_bringtofront:
            /* Verb: window.bringtofront - not yet implemented */
            if (bserror) copystring(PSTRING("\017", "not implemented"), bserror);
            return false;
        case winv_sendtoback:
            /* Verb: window.sendtoback - not yet implemented */
            if (bserror) copystring(PSTRING("\017", "not implemented"), bserror);
            return false;
        case winv_frontmost:
            /* window.frontmost - no-op in headless mode, returns empty string */
            return setstringvalue(BIGSTRING("\x00"), vreturned);
        case winv_next:
            /* Verb: window.next - not yet implemented */
            if (bserror) copystring(PSTRING("\017", "not implemented"), bserror);
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
            /* Headless: no windows to close, return false (no window closed). */
            return setbooleanvalue(false, vreturned);
        case winv_update:
            /* window.update - no-op in headless mode (no GUI to update) */
            setbooleanvalue(true, vreturned);
            return true;
        case winv_ismenuscript:
            /* Verb: window.ismenuscript - not yet implemented */
            if (bserror) copystring(PSTRING("\017", "not implemented"), bserror);
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
            if (bserror) copystring(PSTRING("\017", "not implemented"), bserror);
            return false;
        case winv_runselection:
            /* Verb: window.runselection - not yet implemented */
            if (bserror) copystring(PSTRING("\017", "not implemented"), bserror);
            return false;
        case winv_scroll:
            /* Verb: window.scroll - not yet implemented */
            if (bserror) copystring(PSTRING("\017", "not implemented"), bserror);
            return false;
        case winv_msg:
            /* window.msg - no-op in headless mode (no status bar) */
            setbooleanvalue(true, vreturned);
            return true;
        case winv_dbstats:
            /* window.dbstats - error stub */
            if (bserror)
                copystring(PSTRING("\104", "Can't use window verbs because GUI is not available in headless mode"), bserror);
            return false;
        case winv_quickscript:
            /* Headless: QuickScript window can't be opened, return false (no error). */
            return setbooleanvalue(false, vreturned);
        case winv_ismodified:
            /* Verb: window.ismodified - not yet implemented */
            if (bserror) copystring(PSTRING("\017", "not implemented"), bserror);
            return false;
        case winv_setmodified:
            /* Verb: window.setmodified - not yet implemented */
            if (bserror) copystring(PSTRING("\017", "not implemented"), bserror);
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
        case winv_getfile: {
            /* window.getFile(adr) — return the database file path for an address.
             * Determines which .root database file contains the object at the
             * given address and returns its file path as a string. */
            tyvaluerecord val;
            hdlhashtable htable;
            bigstring bsname;
            hdldatabaserecord hdb = nil;
            boolean fl;

            flnextparamislast = true;

            if (!getaddressparam(hparam1, 1, &val))
                return false;

            fl = getaddressvalue(val, &htable, bsname);

            /* val is an address value on the tmp stack — cleaned up automatically
             * when the current statement finishes (no manual dispose needed). */

            if (!fl)
                return false;

            if (htable == nil) {
                /* htable == nil means an unresolved single identifier (e.g., @root,
                 * @manila). Check filewindowtable first for guest DB roots, then
                 * fall back to roottable for the system root database. */
                hdlhashnode hnode;

                if (hashtablelookupnode(filewindowtable, bsname, &hnode)) {
                    if ((**hnode).val.valuetype == externalvaluetype)
                        hdb = langexternalgetdatabase((hdlexternalvariable) (**hnode).val.data.externalvalue);
                }

                if (hdb == nil && equalstrings(bsname, nameroottable))
                    hdb = tablegetdatabase(roottable);
            }
            else if (htable == filewindowtable) {
                /* Guest DB root — look up the node to get its external variable */
                hdlhashnode hnode;

                if (hashtablelookupnode(filewindowtable, bsname, &hnode)) {
                    if ((**hnode).val.valuetype == externalvaluetype)
                        hdb = langexternalgetdatabase((hdlexternalvariable) (**hnode).val.data.externalvalue);
                }
            }
            else {
                /* Address is inside a table — get DB from the table's refcon */
                hdb = tablegetdatabase(htable);
            }

            if (hdb != nil) {
                const char *path = headless_fnum_path((hdlfilenum)((**hdb).fnumdatabase));

                if (path != nil) {
                    size_t pathlen = strlen(path);

                    if (pathlen > 255) {
                        log_warn(LOG_COMP_LANG, "window.getFile: path is %zu bytes, exceeds bigstring limit of 255", pathlen);

                        if (bserror)
                            copystring(PSTRING("\065", "Can't get file path because it exceeds 255 characters"), bserror);

                        return false;
                    }

                    bigstring bspath;

                    copyctopstring(path, bspath);

                    return setstringvalue(bspath, vreturned);
                }
            }

            /* No database found (e.g., local variable) — return empty string */
            return setstringvalue(PSTRING("\000", ""), vreturned);
        }
        case winv_isreadonly:
            /* Verb: window.isreadonly - not yet implemented */
            if (bserror) copystring(PSTRING("\017", "not implemented"), bserror);
            return false;
        case winv_setquickscript:
            /* Verb: window.setquickscript - not yet implemented */
            if (bserror) copystring(PSTRING("\017", "not implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean windowinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(PSTRING("\006", "window"), bsname);

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

    ADD_VERB(PSTRING("\006", "isopen"), winv_isopen);
    ADD_VERB(PSTRING("\004", "open"), winv_open);
    ADD_VERB(PSTRING("\007", "isfront"), winv_isfront);
    ADD_VERB(PSTRING("\014", "bringtofront"), winv_bringtofront);
    ADD_VERB(PSTRING("\012", "sendtoback"), winv_sendtoback);
    ADD_VERB(PSTRING("\011", "frontmost"), winv_frontmost);
    ADD_VERB(PSTRING("\004", "next"), winv_next);
    ADD_VERB(PSTRING("\011", "isvisible"), winv_isvisible);
    ADD_VERB(PSTRING("\004", "show"), winv_show);
    ADD_VERB(PSTRING("\004", "hide"), winv_hide);
    ADD_VERB(PSTRING("\005", "close"), winv_close);
    ADD_VERB(PSTRING("\006", "update"), winv_update);
    ADD_VERB(PSTRING("\014", "ismenuscript"), winv_ismenuscript);
    ADD_VERB(PSTRING("\013", "getposition"), winv_getposition);
    ADD_VERB(PSTRING("\013", "setposition"), winv_setposition);
    ADD_VERB(PSTRING("\007", "getsize"), winv_getsize);
    ADD_VERB(PSTRING("\007", "setsize"), winv_setsize);
    ADD_VERB(PSTRING("\004", "zoom"), winv_zoom);
    ADD_VERB(PSTRING("\014", "runselection"), winv_runselection);
    ADD_VERB(PSTRING("\006", "scroll"), winv_scroll);
    ADD_VERB(PSTRING("\003", "msg"), winv_msg);
    ADD_VERB(PSTRING("\007", "dbstats"), winv_dbstats);
    ADD_VERB(PSTRING("\013", "quickscript"), winv_quickscript);
    ADD_VERB(PSTRING("\012", "ismodified"), winv_ismodified);
    ADD_VERB(PSTRING("\013", "setmodified"), winv_setmodified);
    ADD_VERB(PSTRING("\010", "gettitle"), winv_gettitle);
    ADD_VERB(PSTRING("\010", "settitle"), winv_settitle);
    ADD_VERB(PSTRING("\005", "about"), winv_about);
    ADD_VERB(PSTRING("\007", "getfile"), winv_getfile);
    ADD_VERB(PSTRING("\012", "isreadonly"), winv_isreadonly);
    ADD_VERB(PSTRING("\016", "setquickscript"), winv_setquickscript);

    #undef ADD_VERB

    pophashtable();
    return true;
}
