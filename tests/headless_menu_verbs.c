/*
 * headless_menu_verbs.c - Menu processor verbs for headless mode
 *
 * Most menu verbs are GUI-only operations (building menubars, adding submenus,
 * etc.) that have no meaningful effect in headless mode. These are implemented
 * as safe no-ops that return true so scripts don't fail.
 *
 * Implemented as no-ops:
 *   - menu.buildmenubar, clearmenubar, isinstalled, install, remove
 *   - menu.addSubMenu, addMenuCommand, deleteSubMenu, deleteMenuCommand
 *   - menu.getScript (returns empty string), setScript
 *   - menu.getCommandKey (returns empty char), setCommandKey
 *
 * Kept as stub (requires window):
 *   - menu.zoomScript
 */

#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"
#include "logging.h"

/* Token enum for all verbs in the menu processor */
enum {
    menv_zoomscript = 0,
    menv_buildmenubar = 1,
    menv_clearmenubar = 2,
    menv_isinstalled = 3,
    menv_install = 4,
    menv_remove = 5,
    menv_getscript = 6,
    menv_setscript = 7,
    menv_addmenucommand = 8,
    menv_deletemenucommand = 9,
    menv_addsubmenu = 10,
    menv_deletesubmenu = 11,
    menv_getcommandkey = 12,
    menv_setcommandkey = 13
};

static boolean menu_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    (void)bserror;

    switch(token) {
        case menv_zoomscript:
            /* menu.zoomScript - requires menu editor window, keep as stub */
            if (bserror) copystring(PSTRING("\017", "not implemented"), bserror);
            return false;

        case menv_buildmenubar:
            /* menu.buildmenubar - no-op in headless mode (no GUI menubar) */
            return setbooleanvalue(true, vreturned);

        case menv_clearmenubar:
            /* menu.clearMenuBar - no-op in headless mode */
            return setbooleanvalue(true, vreturned);

        case menv_isinstalled: {
            /* menu.isInstalled(@adr) - no menus installed in headless mode */
            hdlhashtable htable;
            bigstring bs;

            flnextparamislast = true;

            if (!getvarparam(hparam1, 1, &htable, bs))
                return false;

            return setbooleanvalue(false, vreturned);
        }

        case menv_install:
            /* menu.install - no-op in headless mode */
            return setbooleanvalue(true, vreturned);

        case menv_remove:
            /* menu.remove - no-op in headless mode (no menu bar) */
            return setbooleanvalue(true, vreturned);

        case menv_getscript:
            /* menu.getScript - return empty string in headless mode */
            return setstringvalue(BIGSTRING("\p"), vreturned);

        case menv_setscript:
            /* menu.setScript - no-op in headless mode */
            return setbooleanvalue(true, vreturned);

        case menv_addmenucommand:
            /* menu.addMenuCommand - no-op in headless mode (no menu bar) */
            return setbooleanvalue(true, vreturned);

        case menv_deletemenucommand:
            /* menu.deleteMenuCommand - no-op in headless mode */
            return setbooleanvalue(true, vreturned);

        case menv_addsubmenu:
            /* menu.addSubMenu - no-op in headless mode (no menu bar) */
            return setbooleanvalue(true, vreturned);

        case menv_deletesubmenu:
            /* menu.deleteSubMenu - no-op in headless mode */
            return setbooleanvalue(true, vreturned);

        case menv_getcommandkey:
            /* menu.getCommandKey - return empty char in headless mode */
            return setcharvalue('\0', vreturned);

        case menv_setcommandkey:
            /* menu.setCommandKey - no-op in headless mode */
            return setbooleanvalue(true, vreturned);

        default:
            return false;
    }
}

boolean menuinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(PSTRING("\004", "menu"), bsname);

    if (!newfunctionprocessor(bsname, &menu_valueproc, false, &htable))
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

    ADD_VERB(PSTRING("\012", "zoomscript"), menv_zoomscript);
    ADD_VERB(PSTRING("\014", "buildmenubar"), menv_buildmenubar);
    ADD_VERB(PSTRING("\014", "clearmenubar"), menv_clearmenubar);
    ADD_VERB(PSTRING("\013", "isinstalled"), menv_isinstalled);
    ADD_VERB(PSTRING("\007", "install"), menv_install);
    ADD_VERB(PSTRING("\006", "remove"), menv_remove);
    ADD_VERB(PSTRING("\011", "getscript"), menv_getscript);
    ADD_VERB(PSTRING("\011", "setscript"), menv_setscript);
    ADD_VERB(PSTRING("\016", "addmenucommand"), menv_addmenucommand);
    ADD_VERB(PSTRING("\021", "deletemenucommand"), menv_deletemenucommand);
    ADD_VERB(PSTRING("\012", "addsubmenu"), menv_addsubmenu);
    ADD_VERB(PSTRING("\015", "deletesubmenu"), menv_deletesubmenu);
    ADD_VERB(PSTRING("\015", "getcommandkey"), menv_getcommandkey);
    ADD_VERB(PSTRING("\015", "setcommandkey"), menv_setcommandkey);

    #undef ADD_VERB

    pophashtable();
    return true;
}
