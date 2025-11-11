#include "osincludes_portable.h"
#include "standard.h"
#include "langexternal.h"
#include "memory.h"
#include "db.h"
#include "strings.h"
#include "byteorder.h"
#include "wpverbs.h"

#ifdef FRONTIER_HEADLESS

#include <stdint.h>
#include <stdlib.h>

typedef struct {
    short top;
    short left;
    short bottom;
    short right;
} wp_diskrect;

typedef struct {
    short versionnumber;
    long timecreated;
    long timelastsave;
    long ctsaves;
    char fontname[32];
    short fontsize;
    short fontstyle;
    long maxpos;
    short unused[4];
    short flags;
    wp_diskrect windowrect;
    long varlistsize;
    long buffersize;
    char waste[52];
} wp_diskheader;

#define WP_FLAG_ONELINE 0x8000
#define WP_FLAG_RULERON 0x4000

typedef struct wp_portable_state {
    dbaddress address;
    long timecreated;
    long timelastsave;
    long ctsaves;
    boolean header_valid;
    boolean dirty;
} wp_portable_state;

static long headless_wp_sel_start = 0;
static long headless_wp_sel_end = 0;

static wp_portable_state *wp_portable_state_alloc(void) {
    wp_portable_state *state = (wp_portable_state *)calloc(1, sizeof(wp_portable_state));
    return state;
}

static void wp_portable_state_free(wp_portable_state *state) {
    if (state != NULL)
        free(state);
}

static wp_portable_state *wp_portable_state_from_external(hdlexternalvariable hv) {
    return (wp_portable_state *)(intptr_t)(**hv).variabledata;
}

static void wp_portable_state_attach(hdlexternalvariable hv, wp_portable_state *state, dbaddress adr) {
    (**hv).flinmemory = false;
    (**hv).flpacked = true;
    (**hv).variabledata = (long)(intptr_t)state;
    (**hv).oldaddress = adr;
    state->address = adr;
}

static boolean wp_portable_state_load_header(hdlexternalvariable hv, wp_portable_state *state) {
    if (state == NULL)
        return false;
    if (state->header_valid)
        return true;

    Handle hpacked = nil;
    if (!langexternalrefdata(hv, &hpacked))
        return false;

    wp_diskheader header;
    long size = gethandlesize(hpacked);
    long ix = size - (long)sizeof(header);
    boolean ok = false;

    if (ix >= 0 && loadfromhandle(hpacked, &ix, sizeof(header), &header)) {
        state->timecreated = conditionallongswap(header.timecreated);
        state->timelastsave = conditionallongswap(header.timelastsave);
        state->ctsaves = conditionallongswap(header.ctsaves);
        state->header_valid = true;
        ok = true;
    }

    disposehandle(hpacked);
    return ok;
}

static boolean wp_portable_new_external(boolean flinmemory, boolean flpacked, wp_portable_state *state, hdlexternalvariable *out) {
    hdlexternalvariable hv = nil;
    if (!langnewexternalvariable(flinmemory, (long)(intptr_t)state, &hv))
        return false;
    (**hv).flpacked = flpacked;
    *out = hv;
    return true;
}

static wp_portable_state *wp_portable_state_require(hdlexternalvariable hv) {
    wp_portable_state *state = wp_portable_state_from_external(hv);
    if (state == NULL) {
        state = wp_portable_state_alloc();
        if (state != NULL) {
            wp_portable_state_attach(hv, state, (**hv).oldaddress);
        }
    }
    return state;
}

boolean wpverbgetdisplaystring(hdlexternalvariable h, bigstring bs) {
    (void)h;
    copyctopstring("wp", bs);
    return true;
}

boolean wpverbgettypestring(hdlexternalvariable h, bigstring bs) {
    (void)h;
    copyctopstring("wp", bs);
    return true;
}

boolean wpverbdispose(hdlexternalvariable h, boolean fldisk) {
    (void)fldisk;
    wp_portable_state *state = wp_portable_state_from_external(h);
    wp_portable_state_free(state);
    (**h).variabledata = 0;
    return true;
}

boolean wpverbisdirty(hdlexternalvariable h) {
    wp_portable_state *state = wp_portable_state_from_external(h);
    return state != NULL && state->dirty;
}

boolean wpverbsetdirty(hdlexternalvariable h, boolean fldirty) {
    wp_portable_state *state = wp_portable_state_from_external(h);
    if (state == NULL)
        return false;
    state->dirty = fldirty;
    return true;
}

boolean wpverbnew(Handle h, hdlexternalvariable *hv) {
    (void)h;
    (void)hv;
    return false;
}

boolean wpverbmemorypack(hdlexternalvariable h, Handle *hpacked) {
    (void)h;
    (void)hpacked;
    return false;
}

boolean wpverbmemoryunpack(Handle hpacked, long *ixload, hdlexternalvariable *h) {
    (void)hpacked;
    (void)ixload;
    (void)h;
    return false;
}

boolean wpverbpack(hdlexternalvariable hv, Handle *hpacked, boolean *flnewdbaddress) {
    wp_portable_state *state = wp_portable_state_from_external(hv);
    if (state == NULL || hpacked == NULL || *hpacked == nil)
        return false;

    if (flnewdbaddress)
        *flnewdbaddress = false;

    return pushlongondiskhandle((long)state->address, *hpacked);
}

boolean wpverbunpack(Handle hpacked, long *ixload, hdlexternalvariable *h) {
    long rawadr = 0;

    if (!loadlongfromdiskhandle(hpacked, ixload, &rawadr))
        return false;

    wp_portable_state *state = wp_portable_state_alloc();
    if (state == NULL)
        return false;

    if (!wp_portable_new_external(false, true, state, h)) {
        wp_portable_state_free(state);
        return false;
    }

    wp_portable_state_attach(*h, state, (dbaddress)rawadr);
    return true;
}

boolean wpverbinmemory(hdlexternalvariable h) {
    return wp_portable_state_from_external(h) != NULL;
}

boolean wpverbpacktotext(hdlexternalvariable h, Handle htext) {
    (void)h;
    (void)htext;
    return false;
}

boolean wpverbgetsize(hdlexternalvariable h, long *size) {
    if (size == NULL)
        return false;

    Handle data = nil;
    if (!langexternalrefdata(h, &data))
        return false;

    *size = gethandlesize(data);
    disposehandle(data);
    return true;
}

boolean wpverbgettimes(hdlexternalvariable h, long *timecreated, long *timemodified) {
    wp_portable_state *state = wp_portable_state_require(h);
    if (state == NULL)
        return false;

    if (!wp_portable_state_load_header(h, state))
        return false;

    if (timecreated)
        *timecreated = state->timecreated;
    if (timemodified)
        *timemodified = state->timelastsave;
    return true;
}

boolean wpverbsettimes(hdlexternalvariable h, long timecreated, long timemodified) {
    wp_portable_state *state = wp_portable_state_require(h);
    if (state == NULL)
        return false;
    state->timecreated = timecreated;
    state->timelastsave = timemodified;
    state->header_valid = true;
    state->dirty = true;
    return true;
}

boolean wpwindowopen(hdlexternalvariable h, hdlwindowinfo *hinfo) {
    (void)h;
    if (hinfo)
        *hinfo = nil;
    return false;
}

boolean wpedit(hdlexternalvariable h, hdlwindowinfo win, ptrfilespec fs, bigstring bs, rectparam rzoom) {
    (void)h;
    (void)win;
    (void)fs;
    (void)bs;
    (void)rzoom;
    return false;
}

boolean wpverbfind(hdlexternalvariable h, boolean *flzoom) {
    (void)h;
    if (flzoom)
        *flzoom = false;
    return false;
}

boolean wpstart(void) {
    return true;
}

boolean wpgetselection(long *startsel, long *endsel) {
    if (startsel)
        *startsel = headless_wp_sel_start;
    if (endsel)
        *endsel = headless_wp_sel_end;
    return true;
}

boolean wpsetselection(long startsel, long endsel) {
    headless_wp_sel_start = startsel;
    headless_wp_sel_end = endsel;
    return true;
}

#endif /* FRONTIER_HEADLESS */
