/* 2025-11-27 Codex: Legacy wrappers for outline/script pack/unpack paths. */

#include "frontier.h"
#include "standard.h"

#include "langexternal.h"
#include "opverbs.h"

boolean opverbmemorypack_legacy (hdlexternalvariable h, Handle *hpacked) {
    return opverbmemorypack(h, hpacked);
}

boolean opverbmemoryunpack_legacy (Handle hpacked, long *ixload, hdlexternalvariable *h) {
    return opverbmemoryunpack(hpacked, ixload, h);
}

boolean opverbscriptmemoryunpack_legacy (Handle hpacked, long *ixload, hdlexternalvariable *h) {
    return opverbscriptmemoryunpack(hpacked, ixload, h);
}

boolean opverbpack_legacy (hdlexternalvariable h, Handle *hpacked, boolean *flnewdbaddress) {
    return opverbpack(h, hpacked, flnewdbaddress);
}

boolean opverbunpack_legacy (Handle hpacked, long *ixload, hdlexternalvariable *h) {
    return opverbunpack(hpacked, ixload, h);
}

boolean opverbscriptunpack_legacy (Handle hpacked, long *ixload, hdlexternalvariable *h) {
    return opverbscriptunpack(hpacked, ixload, h);
}

boolean opverbpacktotext_legacy (hdlexternalvariable h, Handle htext) {
    return opverbpacktotext(h, htext);
}
