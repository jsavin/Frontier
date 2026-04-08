#include "frontier.h"
#include "standard.h"
#include "search.h"

#ifdef FRONTIER_HEADLESS

tysearchparameters searchparams;

void startnewsearch (boolean flwrap, boolean flreset) {
    (void) flwrap;
    (void) flreset;
}

boolean startingtosearch (long refcon) {
    (void) refcon;
    return false;
}

boolean searchshouldwrap (long refcon) {
    (void) refcon;
    return false;
}

boolean searchshouldcontinue (long refcon) {
    (void) refcon;
    return false;
}

void endcurrentsearch (void) {
}

boolean initsearch (void) {
    return true;
}

boolean getsearchparams (void) {
    return false;
}

boolean setsearchparams (void) {
    return false;
}

#endif /* FRONTIER_HEADLESS */
