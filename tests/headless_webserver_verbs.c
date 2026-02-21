/*
 * headless_webserver_verbs.c - Webserver processor verbs
 *
 * Wires up webserver.* kernel verbs to their implementations in langhtml.c.
 * These verbs are part of Frontier's kernelized webserver.
 */

#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "langexternal.h"
#include "tablestructure.h"
#include "tableverbs.h"
#include "langhtml.h"
#include "logging.h"

/* Token enum for all verbs in the webserver processor */
enum {
    webv_server = 0,
    webv_dispatch = 1,
    webv_parseheaders = 2,
    webv_parsecookies = 3,
    webv_buildresponse = 4,
    webv_builderrorpage = 5,
    webv_getserverstring = 6
};

static boolean webserver_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
#pragma unused(bserror)
    switch(token) {
        case webv_server: {
            /* webserver.server(adrparamtable, httpRequest=nil) */
            tyaddress adrparamtable;
            Handle hrequest = nil;

            if (!getvarparam(hparam1, 1, &adrparamtable.ht, adrparamtable.bs))
                return false;

            if (langgetparamcount(hparam1) > 1) {
                flnextparamislast = true;
                if (!getreadonlytextvalue(hparam1, 2, &hrequest))
                    return false;
            }

            return webserverserver(&adrparamtable, hrequest, vreturned);
        }

        case webv_dispatch: {
            /* webserver.dispatch(adrparamtable) */
            tyaddress adrparamtable;

            flnextparamislast = true;

            if (!getvarparam(hparam1, 1, &adrparamtable.ht, adrparamtable.bs))
                return false;

            return webserverdispatch(&adrparamtable, vreturned);
        }

        case webv_parseheaders: {
            /* webserver.parseheaders(response, adrheadertable) -> string */
            Handle response, h;
            hdlhashtable htable, hheadertable;
            bigstring bs;
            hdlhashnode hnode;

            (void)hnode;  /* Reserved for future node operations */

            if (!getreadonlytextvalue(hparam1, 1, &response))
                return false;

            flnextparamislast = true;

            if (!getvarparam(hparam1, 2, &htable, bs))
                return false;

            if (!langsuretablevalue(htable, bs, &hheadertable))
                return false;

            if (!webserverparseheaders(response, hheadertable, &h))
                return false;

            return setheapvalue(h, stringvaluetype, vreturned);
        }

        case webv_parsecookies: {
            /* webserver.parsecookies(headertable) */
            hdlhashtable ht;

            flnextparamislast = true;

            if (!gettablevalue(hparam1, 1, &ht))
                return false;

            return webserverparsecookies(ht, vreturned);
        }

        case webv_buildresponse: {
            /* webserver.buildresponse(code, adrheadertable=nil, responsebody=nil) */
            hdlhashtable hheadertable = nil;
            hdlhashtable hparenttable = nil;
            bigstring bscode, bstablename;
            tyvaluerecord vtable, vresponse, vadrtable;
            short ctconsumed = 1;
            short ctpositional = 1;
            hdlhashnode hnode;

            if (!getstringvalue(hparam1, 1, bscode))
                return false;

            initvalue(&vadrtable, addressvaluetype);

            if (!getoptionalparamvalue(hparam1, &ctconsumed, &ctpositional, PSTRING("\016", "adrHeaderTable"), &vadrtable))
                return false;

            if (vadrtable.data.addressvalue != nil) {
                if (!getaddressvalue(vadrtable, &hparenttable, bstablename))
                    return false;

                if (hparenttable != nil) {
                    if (!langhashtablelookup(hparenttable, bstablename, &vtable, &hnode))
                        return false;

                    if (!langexternalvaltotable(vtable, &hheadertable, hnode))
                        return false;
                }
            }

            initvalue(&vresponse, stringvaluetype);

            flnextparamislast = true;

            if (!getoptionalparamvalue(hparam1, &ctconsumed, &ctpositional, PSTRING("\014", "responseBody"), &vresponse))
                return false;

            return webserverbuildresponse(bscode, hheadertable, vresponse.data.stringvalue, vreturned);
        }

        case webv_builderrorpage: {
            /* webserver.builderrorpage(shortmsg, longmsg) -> string */
            Handle hshort, hlong, h;

            if (!getreadonlytextvalue(hparam1, 1, &hshort))
                return false;

            flnextparamislast = true;

            if (!getreadonlytextvalue(hparam1, 2, &hlong))
                return false;

            if (!webserverbuilderrorpage(hshort, hlong, &h))
                return false;

            return setheapvalue(h, stringvaluetype, vreturned);
        }

        case webv_getserverstring: {
            /* webserver.getserverstring() -> string */
            if (!langcheckparamcount(hparam1, 0))
                return false;

            return webservergetserverstring(vreturned);
        }

        default:
            return false;
    }
}

boolean webserverinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    log_debug(LOG_COMP_LANG, "webserverinitverbs: registering webserver EFP");

    copystring(PSTRING("\011", "webserver"), bsname);

    if (!newfunctionprocessor(bsname, &webserver_valueproc, false, &htable)) {
        log_error(LOG_COMP_LANG, "webserverinitverbs: newfunctionprocessor failed");
        return false;
    }

    pushhashtable(htable);

    #define ADD_VERB(name, tok) do { \
        bigstring bs; \
        copystring(name, bs); \
        if (!langaddkeyword(bs, tok)) { \
            pophashtable(); \
            return false; \
        } \
    } while(0)

    ADD_VERB(PSTRING("\006", "server"), webv_server);
    ADD_VERB(PSTRING("\010", "dispatch"), webv_dispatch);
    ADD_VERB(PSTRING("\014", "parseheaders"), webv_parseheaders);
    ADD_VERB(PSTRING("\014", "parsecookies"), webv_parsecookies);
    ADD_VERB(PSTRING("\015", "buildresponse"), webv_buildresponse);
    ADD_VERB(PSTRING("\016", "builderrorpage"), webv_builderrorpage);
    ADD_VERB(PSTRING("\017", "getserverstring"), webv_getserverstring);

    #undef ADD_VERB

    pophashtable();
    return true;
}
