/* 2025-11-27 Codex: Legacy wrappers for 32-bit external pack/unpack paths. */

#include "frontier.h"
#include "standard.h"

#include "langexternal.h"

boolean langexternalpack_legacy (hdlexternalhandle h, Handle *hpacked, boolean *flnewdbaddress) {
    return langexternalpack(h, hpacked, flnewdbaddress);
}

boolean langexternalunpack_legacy (Handle hpacked, hdlexternalhandle *h) {
    return langexternalunpack(hpacked, h);
}

boolean langexternalpacktotext_legacy (hdlexternalhandle h, Handle htext) {
    return langexternalpacktotext(h, htext);
}
