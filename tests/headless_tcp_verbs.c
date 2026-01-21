/*
 * headless_tcp_verbs.c - Tcp processor verbs
 *
 * Phase 1A: Core TCP socket operations (blocking I/O, client-side)
 * Phase 1B: DNS and address operations
 * Phase 2: Buffered I/O and timeouts
 * Phase 3: Server operations (listen/accept with threading)
 *
 * Phase 1A/1B verbs are implemented in C (Common/source/tcpverbs.c).
 * Some advanced verbs may be implemented in UserTalk.
 */

#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"
#include "tcpverbs.h"

/* Token enum for all verbs in the tcp processor */
enum {
    tcpv_addressdecode = 0,
    tcpv_addressencode = 1,
    tcpv_addresstoname = 2,
    tcpv_nametoaddress = 3,
    tcpv_myaddress = 4,
    tcpv_abortstream = 5,
    tcpv_closestream = 6,
    tcpv_closelisten = 7,
    tcpv_openaddrstream = 8,
    tcpv_opennamestream = 9,
    tcpv_readstream = 10,
    tcpv_writestream = 11,
    tcpv_listenstream = 12,
    tcpv_statusstream = 13,
    tcpv_getpeeraddress = 14,
    tcpv_getpeerport = 15,
    tcpv_writestringtostream = 16,
    tcpv_writefiletostream = 17,
    tcpv_readstreamuntil = 18,
    tcpv_readstreambytes = 19,
    tcpv_readstreamuntilclosed = 20,
    tcpv_getstats = 21,
    tcpv_countconnections = 22
};

static boolean tcp_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    hdltreenode hp1 = hparam1;
    tyvaluerecord *v = vreturned;

    setbooleanvalue(false, v);  /* Default return value */

    switch(token) {
        case tcpv_addressdecode: {
            /* Verb #0: tcp.addressDecode(addr) -> ipString */
            long addr;
            bigstring ip_string;

            flnextparamislast = true;
            if (!getlongvalue(hp1, 1, &addr))
                return false;

            if (!tcp_address_decode(addr, ip_string))
                return false;

            return setstringvalue(ip_string, v);
        }

        case tcpv_addressencode: {
            /* Verb #1: tcp.addressEncode(ipString) -> addr */
            bigstring ip_string;
            long addr;

            flnextparamislast = true;
            if (!getstringvalue(hp1, 1, ip_string))
                return false;

            if (!tcp_address_encode(ip_string, &addr))
                return false;

            return setlongvalue(addr, v);
        }

        case tcpv_addresstoname: {
            /* Verb #2: tcp.addressToName(addr) -> hostname */
            long addr;
            bigstring hostname;

            flnextparamislast = true;
            if (!getlongvalue(hp1, 1, &addr))
                return false;

            if (!tcp_address_to_name(addr, hostname))
                return false;

            return setstringvalue(hostname, v);
        }

        case tcpv_nametoaddress: {
            /* Verb #3: tcp.nameToAddress(hostname) -> addr */
            bigstring hostname;
            long addr;

            flnextparamislast = true;
            if (!getstringvalue(hp1, 1, hostname))
                return false;

            if (!tcp_name_to_address(hostname, &addr))
                return false;

            return setlongvalue(addr, v);
        }

        case tcpv_myaddress:
            /* Verb #4: tcp.myaddress - Not yet implemented (Phase 2) */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;

        case tcpv_abortstream: {
            /* Verb #5: tcp.abortStream(stream) -> true */
            long stream_id;

            flnextparamislast = true;
            if (!getlongvalue(hp1, 1, &stream_id))
                return false;

            if (!tcp_abort_stream(stream_id))
                return false;

            return setbooleanvalue(true, v);
        }

        case tcpv_closestream: {
            /* Verb #6: tcp.closeStream(stream) -> true */
            long stream_id;

            flnextparamislast = true;
            if (!getlongvalue(hp1, 1, &stream_id))
                return false;

            if (!tcp_close_stream(stream_id))
                return false;

            return setbooleanvalue(true, v);
        }

        case tcpv_closelisten: {
            /* Verb #7: tcp.closeListen(listenID) -> true */
            long listen_id;

            flnextparamislast = true;
            if (!getlongvalue(hp1, 1, &listen_id))
                return false;

            if (!tcp_close_listen(listen_id))
                return false;

            return setbooleanvalue(true, v);
        }

        case tcpv_openaddrstream: {
            /* Verb #8: tcp.openAddrStream(addr, port) -> streamID */
            long addr, port;
            long stream_id;

            if (!getlongvalue(hp1, 1, &addr))
                return false;

            flnextparamislast = true;
            if (!getlongvalue(hp1, 2, &port))
                return false;

            if (!tcp_open_stream_addr(addr, port, &stream_id))
                return false;

            return setlongvalue(stream_id, v);
        }

        case tcpv_opennamestream: {
            /* Verb #9: tcp.openNameStream(hostname, port) -> streamID */
            bigstring hostname;
            long port;
            long stream_id;

            if (!getstringvalue(hp1, 1, hostname))
                return false;

            flnextparamislast = true;
            if (!getlongvalue(hp1, 2, &port))
                return false;

            if (!tcp_open_stream_name(hostname, port, &stream_id))
                return false;

            return setlongvalue(stream_id, v);
        }

        case tcpv_readstream: {
            /* Verb #10: tcp.readStream(stream, bytes) -> data */
            long stream_id, bytes_to_read;
            Handle hdata;

            if (!getlongvalue(hp1, 1, &stream_id))
                return false;

            flnextparamislast = true;
            if (!getlongvalue(hp1, 2, &bytes_to_read))
                return false;

            if (!tcp_read_stream(stream_id, bytes_to_read, &hdata))
                return false;

            return setheapvalue(hdata, binaryvaluetype, v);
        }

        case tcpv_writestream: {
            /* Verb #11: tcp.writeStream(stream, data) -> true */
            long stream_id;
            Handle hdata;

            if (!getlongvalue(hp1, 1, &stream_id))
                return false;

            flnextparamislast = true;
            if (!getexempttextvalue(hp1, 2, &hdata))
                return false;

            if (!tcp_write_stream(stream_id, hdata))
                return false;

            return setbooleanvalue(true, v);
        }

        case tcpv_listenstream: {
            /* Verb #12: tcp.listenStream(port, depth, callback, refcon, addr) -> listenID */
            long port, depth, refcon, bind_addr;
            tyvaluerecord vcallback;
            hdlhashtable callback_htable;
            bigstring callback_name;
            long listen_id;

            /* Extract parameters */
            if (!getlongvalue(hp1, 1, &port))
                return false;

            if (!getlongvalue(hp1, 2, &depth))
                return false;

            /* Get callback address parameter */
            if (!getaddressparam(hp1, 3, &vcallback))
                return false;

            /* Extract hash table and script name from address */
            if (!getaddressvalue(vcallback, &callback_htable, callback_name)) {
                if (bserror) copystring(BIGSTRING("\pCan't resolve callback address"), bserror);
                return false;
            }

            if (!getlongvalue(hp1, 4, &refcon))
                return false;

            flnextparamislast = true;
            if (!getlongvalue(hp1, 5, &bind_addr))
                return false;

            /* Call implementation */
            if (!tcp_listen_stream(port, depth, callback_htable, callback_name, refcon, bind_addr, &listen_id))
                return false;

            return setlongvalue(listen_id, v);
        }

        case tcpv_statusstream:
            /* Verb #13: tcp.statusstream - Not yet implemented (Phase 2) */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;

        case tcpv_getpeeraddress:
            /* Verb #14: tcp.getpeeraddress - Not yet implemented (Phase 2) */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;

        case tcpv_getpeerport:
            /* Verb #15: tcp.getpeerport - Not yet implemented (Phase 2) */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;

        case tcpv_writestringtostream:
            /* Verb #16: tcp.writestringtostream - Not yet implemented (Phase 2) */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;

        case tcpv_writefiletostream:
            /* Verb #17: tcp.writefiletostream - Not yet implemented (Phase 2) */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;

        case tcpv_readstreamuntil:
            /* Verb #18: tcp.readstreamuntil - Not yet implemented (Phase 2) */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;

        case tcpv_readstreambytes:
            /* Verb #19: tcp.readstreambytes - Not yet implemented (Phase 2) */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;

        case tcpv_readstreamuntilclosed:
            /* Verb #20: tcp.readstreamuntilclosed - Not yet implemented (Phase 2) */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;

        case tcpv_getstats:
            /* Verb #21: tcp.getstats - Not yet implemented (Phase 2) */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;

        case tcpv_countconnections: {
            /* Verb #22: tcp.countConnections() -> count */
            long count;

            count = tcp_count_connections();

            return setlongvalue(count, v);
        }

        default:
            return false;
    }
}

boolean tcpinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    /* Initialize TCP context (sockets, mutexes, etc.) */
    if (!tcp_init_context())
        return false;

    copystring(BIGSTRING("\ptcp"), bsname);

    if (!newfunctionprocessor(bsname, &tcp_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\paddressdecode"), tcpv_addressdecode);
    ADD_VERB(BIGSTRING("\paddressencode"), tcpv_addressencode);
    ADD_VERB(BIGSTRING("\paddresstoname"), tcpv_addresstoname);
    ADD_VERB(BIGSTRING("\pnametoaddress"), tcpv_nametoaddress);
    ADD_VERB(BIGSTRING("\pmyaddress"), tcpv_myaddress);
    ADD_VERB(BIGSTRING("\pabortstream"), tcpv_abortstream);
    ADD_VERB(BIGSTRING("\pclosestream"), tcpv_closestream);
    ADD_VERB(BIGSTRING("\pcloselisten"), tcpv_closelisten);
    ADD_VERB(BIGSTRING("\popenaddrstream"), tcpv_openaddrstream);
    ADD_VERB(BIGSTRING("\popennamestream"), tcpv_opennamestream);
    ADD_VERB(BIGSTRING("\preadstream"), tcpv_readstream);
    ADD_VERB(BIGSTRING("\pwritestream"), tcpv_writestream);
    ADD_VERB(BIGSTRING("\plistenstream"), tcpv_listenstream);
    ADD_VERB(BIGSTRING("\pstatusstream"), tcpv_statusstream);
    ADD_VERB(BIGSTRING("\pgetpeeraddress"), tcpv_getpeeraddress);
    ADD_VERB(BIGSTRING("\pgetpeerport"), tcpv_getpeerport);
    ADD_VERB(BIGSTRING("\pwritestringtostream"), tcpv_writestringtostream);
    ADD_VERB(BIGSTRING("\pwritefiletostream"), tcpv_writefiletostream);
    ADD_VERB(BIGSTRING("\preadstreamuntil"), tcpv_readstreamuntil);
    ADD_VERB(BIGSTRING("\preadstreambytes"), tcpv_readstreambytes);
    ADD_VERB(BIGSTRING("\preadstreamuntilclosed"), tcpv_readstreamuntilclosed);
    ADD_VERB(BIGSTRING("\pgetstats"), tcpv_getstats);
    ADD_VERB(BIGSTRING("\pcountconnections"), tcpv_countconnections);

    #undef ADD_VERB

    pophashtable();
    return true;
}
