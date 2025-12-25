#include "frontier.h"
#include "standard.h"

#include <stdio.h>
#include <string.h>

#include "langexternal.h"
#include "menuverbs.h"
#include "strings.h"
#include "db.h"
#include "db_format.h"
#include "dbinternal.h"
#include "memory.h"
#include "logging.h"

#ifdef FRONTIER_HEADLESS

boolean menuverbgetdisplaystring (hdlexternalvariable h, bigstring bs) {
    (void) h; copyctopstring("menu", bs); return true;
}
boolean menuverbgettypestring (hdlexternalvariable h, bigstring bs) {
    (void) h; copyctopstring("menu", bs); return true;
}
boolean menuverbsetdirty (hdlexternalvariable h, boolean fldirty) { (void)h;(void)fldirty; return false; }
boolean menuverbmemorypack (hdlexternalvariable h, Handle *hp) { (void)h; if (hp) *hp = nil; return false; }
boolean menuverbmemoryunpack (Handle hpacked, long *ixload, hdlexternalvariable *h) { (void)hpacked;(void)ixload;(void)h; return false; }
static boolean headless_menu_dup_block(dbaddress source, dbaddress *dest) {
    dbaddress normalized = source;
    long payload_offset = 0;
    Handle hpayload = nil;
    if (dest == NULL) {
        log_error(LOG_COMP_DB, "[headless] menu dup missing dest pointer");
        return false;
    }
    *dest = nildbaddress;
    if (dbnormalizeaddress(&normalized)) {
        if (normalized != source) {
            dbaddress data_start = normalized + sizeheader;
            if (source > data_start)
                payload_offset = (long) (source - data_start);
            source = normalized;
        }
    } else {
        log_error(LOG_COMP_DB, "[headless] menu dbnormalizeaddress failed adr=0x%llx",
                (unsigned long long) source);
    }

    if (!dbrefhandle(source, &hpayload)) {
        log_error(LOG_COMP_DB, "[headless] dbrefhandle failed for menu adr=0x%llx",
                (unsigned long long) source);
        return false;
    }

    long hsize = gethandlesize(hpayload);
    hdldatabaserecord hdest = nil;
    (void) dbgetdestinationdatabase(&hdest);
    log_debug(LOG_COMP_DB,
            "[headless] menu dup source=0x%llx hsize=%ld saveas=%d current=%p dest=%p",
            (unsigned long long) source,
            hsize,
            (int) fldatabasesaveas,
            (void *) databasedata,
            (void *) hdest);

    if (payload_offset > 0) {
        long hsize = gethandlesize(hpayload);
        if (payload_offset < hsize)
            pullfromhandle(hpayload, 0, payload_offset, nil);
    }

    if (!dbassignhandle(hpayload, dest)) {
        log_error(LOG_COMP_DB, "[headless] dbassignhandle failed for menu adr=0x%llx",
                (unsigned long long) *dest);
        disposehandle(hpayload);
        return false;
    }

    disposehandle(hpayload);
    return true;
}

boolean menuverbpack_internal (const db_context *ctx, hdlexternalvariable h, Handle *hp, boolean *flnew) {
    /* Context-aware menu packing for migration.
     * During headless migration, menu externals are preserved as database references
     * without materializing their data. We just pack the v6 address as-is. */

    (void)ctx;  /* Context not needed for address passthrough */

    if ((h == nil) || (hp == nil))
        return false;

    dbaddress adr = (**h).oldaddress;
    if (adr == nildbaddress)
        adr = (dbaddress) (**h).variabledata;
    if (adr == nildbaddress) {
        if (flnew)
            *flnew = false;
        return false;
    }

    /* During migration, mark as new address (will be allocated in destination) */
    if (flnew)
        *flnew = true;

    (**h).oldaddress = adr;
    return pushlongondiskhandle((long) adr, *hp);
}

boolean menuverbpack (hdlexternalvariable h, Handle *hp, boolean *flnew) {
    /* Wrapper for backward compatibility - uses global mode state */
    return menuverbpack_internal(NULL, h, hp, flnew);
}

boolean menuverbunpack (Handle hpacked, long *ixload, hdlexternalvariable *h) {
    long rawadr = 0;
    if (!loadlongfromdiskhandle(hpacked, ixload, &rawadr))
        return false;
    return langnewexternalvariable(false, rawadr, h);
}
boolean menuverbpacktotext (hdlexternalvariable h, Handle htext) { (void)h;(void)htext; return false; }
boolean menuverbgetsize (hdlexternalvariable h, long *size) { (void)h; if (size) *size=0; return true; }
boolean menuverbgettimes (hdlexternalvariable h, int64_t *tc, int64_t *tm) { (void)h; if (tc) *tc=0; if (tm) *tm=0; return false; }
boolean menuverbsettimes (hdlexternalvariable h, int64_t tc, int64_t tm) { (void)h;(void)tc;(void)tm; return false; }
boolean menuverbfindusedblocks (hdlexternalvariable h, bigstring bspath) { (void)h; if (bspath) setemptystring(bspath); return false; }
boolean menuverbfind (hdlexternalvariable h, boolean *flzoom) { (void)h; if (flzoom) *flzoom=false; return false; }
boolean menuverbdispose (hdlexternalvariable h, boolean fldisk) { (void)h;(void)fldisk; return true; }
boolean menuverbnew (Handle hdata, hdlexternalvariable *hv) { (void)hdata;(void)hv; return false; }
boolean menuverbinmemory_context (const db_context *ctx, hdlexternalvariable hvariable) {
    /* Menu externals are not materialized during headless migration.
       Mark as "in memory" to skip actual loading, but keep the address for packing. */
    (void)ctx;
    (**hvariable).flinmemory = true;
    return true;
}

#endif /* FRONTIER_HEADLESS */
