#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "tablestructure.h"
#include "file.h"

/* Headless file verbs EFP registering a minimal subset: open/close/readLine/read/write,
   setPosition/getPosition, setEndOfFile/getEndOfFile, endOfFile. */

enum {
    fv_open = 1,
    fv_close,
    fv_readLine,
    fv_read,
    fv_write,
    fv_setPosition,
    fv_getPosition,
    fv_setEndOfFile,
    fv_getEndOfFile,
    fv_endOfFile
};

/* bring in headless_readline from the portable file adapter */
extern long headless_readline(hdlfilenum fnum, char *buf, long bufsz);

static boolean fv_valueproc(short token, hdltreenode hparam1, tyvaluerecord *vreturned, bigstring bserror) {
    (void)bserror;
    register tyvaluerecord *v = vreturned;
    setbooleanvalue(false, v);

    tyfilespec fs; clearbytes(&fs, sizeof fs);
    switch (token) {
        case fv_open: {
            tyvaluerecord v1; if (!getparamvalue(hparam1, 1, &v1)) return false;
            if (!coercetofilespec(&v1)) return false;
            fs = **v1.data.filespecvalue;
            (*v).data.flvalue = fifopenfile(&fs, 0L);
            return true;
        }
        case fv_close: {
            tyvaluerecord v1; if (!getparamvalue(hparam1, 1, &v1)) return false;
            if (!coercetofilespec(&v1)) return false;
            fs = **v1.data.filespecvalue;
            (*v).data.flvalue = fifclosefile(&fs);
            return true;
        }
        case fv_endOfFile: {
            tyvaluerecord v1; if (!getparamvalue(hparam1, 1, &v1)) return false;
            if (!coercetofilespec(&v1)) return false;
            fs = **v1.data.filespecvalue;
            (*v).data.flvalue = fifendoffile(&fs);
            return true;
        }
        case fv_getEndOfFile: {
            long eof = 0;
            tyvaluerecord v1; if (!getparamvalue(hparam1, 1, &v1)) return false;
            if (!coercetofilespec(&v1)) return false;
            fs = **v1.data.filespecvalue;
            if (!fifgetendoffile(&fs, &eof)) return false;
            return setlongvalue(eof, v);
        }
        case fv_setEndOfFile: {
            long eof = 0;
            tyvaluerecord v1; if (!getparamvalue(hparam1, 1, &v1)) return false;
            if (!coercetofilespec(&v1)) return false;
            fs = **v1.data.filespecvalue;
            flnextparamislast = true;
            if (!getlongvalue(hparam1, 2, &eof)) return false;
            (*v).data.flvalue = fifsetendoffile(&fs, eof);
            return true;
        }
        case fv_setPosition: {
            long pos = 0;
            tyvaluerecord v1; if (!getparamvalue(hparam1, 1, &v1)) return false;
            if (!coercetofilespec(&v1)) return false;
            fs = **v1.data.filespecvalue;
            flnextparamislast = true;
            if (!getlongvalue(hparam1, 2, &pos)) return false;
            (*v).data.flvalue = fifsetposition(&fs, pos);
            return true;
        }
        case fv_getPosition: {
            long pos = 0;
            tyvaluerecord v1; if (!getparamvalue(hparam1, 1, &v1)) return false;
            if (!coercetofilespec(&v1)) return false;
            fs = **v1.data.filespecvalue;
            if (!fifgetposition(&fs, &pos)) return false;
            return setlongvalue(pos, v);
        }
        case fv_readLine: {
            Handle line = nil;
            tyvaluerecord v1; if (!getparamvalue(hparam1, 1, &v1)) return false;
            if (!coercetofilespec(&v1)) return false;
            fs = **v1.data.filespecvalue;
            /* Use existing buffered implementation for correctness */
            if (!fifreadline(&fs, &line)) return false;
            return setheapvalue(line, stringvaluetype, v);
        }
        case fv_read: {
            long ct = 0; Handle h = nil;
            tyvaluerecord v1; if (!getparamvalue(hparam1, 1, &v1)) return false;
            if (!coercetofilespec(&v1)) return false;
            fs = **v1.data.filespecvalue;
            flnextparamislast = true;
            if (!getlongvalue(hparam1, 2, &ct)) return false;
            if (!fifreadhandle(&fs, ct, &h)) return true; /* returns nil/false */
            if ((ct < longinfinity) && (ct != gethandlesize(h))) { disposehandle(h); return true; }
            return setbinaryvalue(h, '\?\?\?\?', v);
        }
        case fv_write: {
            Handle hdata = nil;
            tyvaluerecord v1; if (!getparamvalue(hparam1, 1, &v1)) return false;
            if (!coercetofilespec(&v1)) return false;
            fs = **v1.data.filespecvalue;
            flnextparamislast = true;
            if (!getreadonlytextvalue(hparam1, 2, &hdata)) return false;
            (*v).data.flvalue = fifwritehandle(&fs, hdata);
            return true;
        }
    }
    return false;
}

boolean fileinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname; copyctopstring("file", bsname);
    if (!newfunctionprocessor(bsname, &fv_valueproc, false, &htable))
        return false;
    pushhashtable(htable);
    #define ADD_VERB(name, tok) do { bigstring bs; copyctopstring(name, bs); if (!langaddkeyword(bs, tok)) { pophashtable(); return false; } } while(0)
    ADD_VERB("open", fv_open);
    ADD_VERB("close", fv_close);
    ADD_VERB("readLine", fv_readLine);
    ADD_VERB("read", fv_read);
    ADD_VERB("write", fv_write);
    ADD_VERB("setPosition", fv_setPosition);
    ADD_VERB("getPosition", fv_getPosition);
    ADD_VERB("setEndOfFile", fv_setEndOfFile);
    ADD_VERB("getEndOfFile", fv_getEndOfFile);
    ADD_VERB("endOfFile", fv_endOfFile);
    #undef ADD_VERB
    pophashtable();
    return true;
}
