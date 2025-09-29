#include "frontier.h"
#include "strings.h"
#include "langregexp.h"

#ifdef FRONTIER_HEADLESS

static void setunsupportedmessage(bigstring bsmsg) {
    if (bsmsg)
        copyctopstring("regular expressions are not available in headless mode", bsmsg);
}

boolean regexpcompile(const char *pattern, int options, bigstring bsmsg, Handle *hout) {
    (void)pattern;
    (void)options;
    if (hout)
        *hout = nil;
    setunsupportedmessage(bsmsg);
    return false;
}

boolean regexpcheckreplacement(Handle hcp, const char *replacement, int len) {
    (void)hcp;
    (void)replacement;
    (void)len;
    return false;
}

boolean regexpnewovector(Handle hcp, Handle *hovec) {
    (void)hcp;
    if (hovec)
        *hovec = nil;
    return false;
}

boolean regexptextsearch(byte *text, long len, long *offset, long *matchlen) {
    (void)text;
    (void)len;
    if (offset)
        *offset = 0;
    if (matchlen)
        *matchlen = 0;
    return false;
}

boolean regexpinitverbs(void) {
    return true;
}

#endif /* FRONTIER_HEADLESS */
