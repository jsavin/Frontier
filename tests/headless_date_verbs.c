#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the date processor */
enum {
    datv_now = 0,
    datv_set = 1,
    datv_sleepfor = 2,
    datv_ticks = 3,
    datv_milliseconds = 4,
    datv_waitseconds = 5,
    datv_waitsixtieths = 6,
    datv_date = 7,
    datv_get = 8,
    datv_set = 9,
    datv_abbrevstring = 10,
    datv_dayofweek = 11,
    datv_daysinmonth = 12,
    datv_daystring = 13,
    datv_firstofmonth = 14,
    datv_lastofmonth = 15,
    datv_longstring = 16,
    datv_nextmonth = 17,
    datv_nextweek = 18,
    datv_nextyear = 19,
    datv_prevmonth = 20,
    datv_prevweek = 21,
    datv_prevyear = 22,
    datv_shortstring = 23,
    datv_tomorrow = 24,
    datv_weeksinmonth = 25,
    datv_yesterday = 26,
    datv_getcurrenttimezone = 27,
    datv_netstandardstring = 28,
    datv_monthtostring = 29
};

static boolean date_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case datv_now:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_set:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_sleepfor:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_ticks:
            /* Verb #3 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_milliseconds:
            /* Verb #4 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_waitseconds:
            /* Verb #5 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_waitsixtieths:
            /* Verb #6 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_date:
            /* Verb #7 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_get:
            /* Verb #8 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_set:
            /* Verb #9 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_abbrevstring:
            /* Verb #10 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_dayofweek:
            /* Verb #11 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_daysinmonth:
            /* Verb #12 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_daystring:
            /* Verb #13 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_firstofmonth:
            /* Verb #14 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_lastofmonth:
            /* Verb #15 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_longstring:
            /* Verb #16 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_nextmonth:
            /* Verb #17 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_nextweek:
            /* Verb #18 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_nextyear:
            /* Verb #19 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_prevmonth:
            /* Verb #20 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_prevweek:
            /* Verb #21 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_prevyear:
            /* Verb #22 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_shortstring:
            /* Verb #23 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_tomorrow:
            /* Verb #24 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_weeksinmonth:
            /* Verb #25 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_yesterday:
            /* Verb #26 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_getcurrenttimezone:
            /* Verb #27 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_netstandardstring:
            /* Verb #28 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_monthtostring:
            /* Verb #29 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean dateinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pdate"), bsname);

    if (!newfunctionprocessor(bsname, &date_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pnow"), datv_now);
    ADD_VERB(BIGSTRING("\pset"), datv_set);
    ADD_VERB(BIGSTRING("\psleepfor"), datv_sleepfor);
    ADD_VERB(BIGSTRING("\pticks"), datv_ticks);
    ADD_VERB(BIGSTRING("\pmilliseconds"), datv_milliseconds);
    ADD_VERB(BIGSTRING("\pwaitseconds"), datv_waitseconds);
    ADD_VERB(BIGSTRING("\pwaitsixtieths"), datv_waitsixtieths);
    ADD_VERB(BIGSTRING("\pdate"), datv_date);
    ADD_VERB(BIGSTRING("\pget"), datv_get);
    ADD_VERB(BIGSTRING("\pset"), datv_set);
    ADD_VERB(BIGSTRING("\pabbrevstring"), datv_abbrevstring);
    ADD_VERB(BIGSTRING("\pdayofweek"), datv_dayofweek);
    ADD_VERB(BIGSTRING("\pdaysinmonth"), datv_daysinmonth);
    ADD_VERB(BIGSTRING("\pdaystring"), datv_daystring);
    ADD_VERB(BIGSTRING("\pfirstofmonth"), datv_firstofmonth);
    ADD_VERB(BIGSTRING("\plastofmonth"), datv_lastofmonth);
    ADD_VERB(BIGSTRING("\plongstring"), datv_longstring);
    ADD_VERB(BIGSTRING("\pnextmonth"), datv_nextmonth);
    ADD_VERB(BIGSTRING("\pnextweek"), datv_nextweek);
    ADD_VERB(BIGSTRING("\pnextyear"), datv_nextyear);
    ADD_VERB(BIGSTRING("\pprevmonth"), datv_prevmonth);
    ADD_VERB(BIGSTRING("\pprevweek"), datv_prevweek);
    ADD_VERB(BIGSTRING("\pprevyear"), datv_prevyear);
    ADD_VERB(BIGSTRING("\pshortstring"), datv_shortstring);
    ADD_VERB(BIGSTRING("\ptomorrow"), datv_tomorrow);
    ADD_VERB(BIGSTRING("\pweeksinmonth"), datv_weeksinmonth);
    ADD_VERB(BIGSTRING("\pyesterday"), datv_yesterday);
    ADD_VERB(BIGSTRING("\pgetcurrenttimezone"), datv_getcurrenttimezone);
    ADD_VERB(BIGSTRING("\pnetstandardstring"), datv_netstandardstring);
    ADD_VERB(BIGSTRING("\pmonthtostring"), datv_monthtostring);

    #undef ADD_VERB

    pophashtable();
    return true;
}
