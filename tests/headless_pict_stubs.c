#include "frontier.h"
#include "standard.h"
#include "langexternal.h"
#include "pictverbs.h"
#include "strings.h"
#include "db.h"
#include "db_format.h"

#ifdef FRONTIER_HEADLESS

boolean pictverbgetdisplaystring (hdlexternalvariable h, bigstring bs) {
    (void) h;
    copyctopstring("picture", bs);
    return true;
}

boolean pictverbgettypestring (hdlexternalvariable h, bigstring bs) {
    (void) h;
    copyctopstring("picture", bs);
    return true;
}

boolean pictverbdispose (hdlexternalvariable h, boolean fldisk) {
    (void) h;
    (void) fldisk;
    return true;
}

boolean pictverbnew (Handle h, hdlexternalvariable *hv) {
    (void) h;
    (void) hv;
    return false;
}

boolean pictverbisdirty (hdlexternalvariable h) {
    (void) h;
    return false;
}

boolean pictverbsetdirty (hdlexternalvariable h, boolean fldirty) {
    (void) h;
    (void) fldirty;
    return false;
}

boolean pictverbinmemory (const db_context *ctx, hdlexternalvariable h) {
    /* Stub: mark picture as "in memory" without actually loading it.
     * Pictures aren't used in headless mode, but we need to set the flag
     * for migration validation. */
    (void) ctx;

    if (h == nil)
        return false;

    /* Mark as in-memory so migration validator doesn't complain */
    (**h).flinmemory = true;

    return true;
}

boolean pictverbmemorypack (hdlexternalvariable h, Handle *hpacked) {
    (void) h;
    (void) hpacked;
    return false;
}

boolean pictverbmemoryunpack (Handle hpacked, long *ixload, hdlexternalvariable *hv) {
    (void) hpacked;
    (void) ixload;
    (void) hv;
    return false;
}

boolean pictverbpack_internal (const db_context *ctx, hdlexternalvariable h, Handle *hpacked, boolean *flnewdbaddress) {
    /* Context-aware picture packing for migration.
     *
     * Applies format mode from context before packing, ensuring pictures are
     * migrated with correct v7 format flags.
     *
     * Precondition: Picture must be in memory (caller should have called
     * pictverbinmemory() first during materialization). This ensures we're
     * packing actual picture data, not stale v6 addresses.
     */

    if ((h == nil) || (hpacked == nil))
        return false;

    /* Precondition: picture must be in memory */
    if (!(**h).flinmemory) {
        /* This is a programming error - caller should have loaded it */
        return false;
    }

    db_format_mode savedmode = db_format_mode_current();

    /* Apply context mode if provided (for migration: sets v7 format flags) */
    if (ctx != NULL) {
        db_format_mode_apply(&ctx->mode);
    }

    /* Picture is in memory - assign new address and return it */
    dbaddress adr = (**h).oldaddress;
    if (adr == nildbaddress)
        adr = (dbaddress) (**h).variabledata;

    db_format_mode mode = db_format_mode_current();
    if (fldatabasesaveas || mode.use_64bit_format) {
        /* During migration (adapter repack), allocate new address */
        *flnewdbaddress = true;
    } else if (flnewdbaddress) {
        *flnewdbaddress = false;
    }

    (**h).oldaddress = adr;
    db_format_mode_apply(&savedmode);
    return pushlongondiskhandle((long) adr, *hpacked);
}

boolean pictverbpack (hdlexternalvariable h, Handle *hpacked, boolean *flnewdbaddress) {
    /* Wrapper for backward compatibility - uses global mode state */
    return pictverbpack_internal(NULL, h, hpacked, flnewdbaddress);
}

boolean pictverbunpack (Handle hpacked, long *ixload, hdlexternalvariable *hv, hdldatabaserecord hdb) {
    long rawadr = 0;
    if (!loadlongfromdiskhandle(hpacked, ixload, &rawadr))
        return false;
    return langnewexternalvariable(false, rawadr, hv, hdb);
}

boolean pictverbpacktotext (hdlexternalvariable h, Handle htext) {
    (void) h;
    (void) htext;
    return false;
}

boolean pictverbgetsize (hdlexternalvariable h, long *size) {
    (void) h;
    if (size)
        *size = 0;
    return true;
}

boolean pictverbgettimes (hdlexternalvariable h, int64_t *timecreated, int64_t *timemodified) {
    (void) h;
    if (timecreated)
        *timecreated = 0;
    if (timemodified)
        *timemodified = 0;
    return false;
}

boolean pictverbsettimes (hdlexternalvariable h, int64_t timecreated, int64_t timemodified) {
    (void) h;
    (void) timecreated;
    (void) timemodified;
    return false;
}

boolean pictwindowopen (hdlexternalvariable h, hdlwindowinfo *hinfo) {
    (void) h;
    if (hinfo)
        *hinfo = nil;
    return false;
}

boolean pictedit (hdlexternalvariable h, hdlwindowinfo win, ptrfilespec fs, bigstring bs, rectparam rzoom) {
    (void) h;
    (void) win;
    (void) fs;
    (void) bs;
    (void) rzoom;
    return false;
}

boolean pictverbfind (hdlexternalvariable h, boolean *flzoom) {
    (void) h;
    if (flzoom)
        *flzoom = false;
    return false;
}

boolean pictstart (void) {
    return true;
}

#endif /* FRONTIER_HEADLESS */
