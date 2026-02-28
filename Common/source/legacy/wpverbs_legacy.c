/* 2025-11-27 Codex: Legacy wrappers for WP pack/unpack paths. */

#include "frontier.h"
#include "standard.h"

#include "wpverbs.h"

boolean wpverbmemorypack_legacy (hdlexternalvariable h, Handle *hpacked) {
    return wpverbmemorypack(h, hpacked);
}

boolean wpverbmemoryunpack_legacy (Handle hpacked, long *ixload, hdlexternalvariable *h) {
    return wpverbmemoryunpack(hpacked, ixload, h);
}

boolean wpverbpack_legacy (hdlexternalvariable h, Handle *hpacked, boolean *flnewdbaddress) {
    return wpverbpack(h, hpacked, flnewdbaddress);
}

boolean wpverbunpack_legacy (Handle hpacked, long *ixload, hdlexternalvariable *h) {
    return wpverbunpack(hpacked, ixload, h, databasedata);
}

boolean wpverbpacktotext_legacy (hdlexternalvariable h, Handle htext) {
    return wpverbpacktotext(h, htext);
}
