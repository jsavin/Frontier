#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"
#include "file.h"
#include "file_portable.h"

/* Headless file verbs EFP registering file I/O and path manipulation verbs.
 * Token IDs match kernelverbs.rc (EFP 1007) for proper verb lookup. */

enum {
    fv_created = 0,
    fv_modified = 1,
    fv_type = 2,
    fv_creator = 3,
    fv_setcreated = 4,
    fv_setmodified = 5,
    fv_settype = 6,
    fv_setcreator = 7,
    fv_isfolder = 8,
    fv_isvolume = 9,
    fv_islocked = 10,
    fv_lock = 11,
    fv_unlock = 12,
    fv_copy = 13,
    fv_copydatafork = 14,
    fv_copyresourcefork = 15,
    fv_delete = 16,
    fv_rename = 17,
    fv_exists = 18,
    fv_size = 19,
    fv_fullpath = 20,
    fv_getpath = 21,
    fv_setpath = 22,
    fv_filefrompath = 23,
    fv_folderfrompath = 24,
    fv_getsystemfolderpath = 25,
    fv_getspecialfolderpath = 26,
    fv_new = 27,
    fv_newfolder = 28,
    fv_newalias = 29,
    fv_getfiledialog = 30,
    fv_putfiledialog = 31,
    fv_getfolderdialog = 32,
    fv_getdiskdialog = 33,
    fv_geticonpos = 34,
    fv_seticonpos = 35,
    fv_getversion = 36,
    fv_setversion = 37,
    fv_getfullversion = 38,
    fv_setfullversion = 39,
    fv_getcomment = 40,
    fv_setcomment = 41,
    fv_getlabel = 42,
    fv_setlabel = 43,
    fv_findapplication = 44,
    fv_isbusy = 45,
    fv_hasbundle = 46,
    fv_setbundle = 47,
    fv_isalias = 48,
    fv_isvisible = 49,
    fv_setvisible = 50,
    fv_followalias = 51,
    fv_move = 52,
    fv_eject = 53,
    fv_isejectable = 54,
    fv_freespaceonvolume = 55,
    fv_volumesize = 56,
    fv_volumeblocksize = 57,
    fv_filesonvolume = 58,
    fv_foldersonvolume = 59,
    fv_unmountvolume = 60,
    fv_mountservervolume = 61,
    fv_findinfile = 62,
    fv_countlines = 63,
    fv_open = 64,
    fv_close = 65,
    fv_endoffile = 66,
    fv_setendoffile = 67,
    fv_getendoffile = 68,
    fv_setposition = 69,
    fv_getposition = 70,
    fv_readline = 71,
    fv_writeline = 72,
    fv_read = 73,
    fv_write = 74,
    fv_compare = 75,
    fv_writewholefile = 76,
    fv_getpathchar = 77,
    fv_freespaceonvolumedouble = 78,
    fv_volumesizedouble = 79,
    fv_getmp3info = 80,
    fv_readwholefile = 81,
    fv_getLabelIndex = 82,
    fv_setLabelIndex = 83,
    fv_getLabelNames = 84,
    fv_getPosixPath = 85
};

/* bring in headless_readline from the portable file adapter */
extern long headless_readline(hdlfilenum fnum, char *buf, long bufsz);

static boolean fv_valueproc(short token, hdltreenode hparam1, tyvaluerecord *vreturned, bigstring bserror) {
    register tyvaluerecord *v = vreturned;

    setbooleanvalue(false, v);

    tyfilespec fs; clearbytes(&fs, sizeof fs);
    switch (token) {
        case fv_filefrompath: {
            bigstring bspath, bsfile;
            flnextparamislast = true;
            if (!getstringvalue(hparam1, 1, bspath))
                return false;
            if (!portable_filefrompath(bspath, bsfile))
                return false;
            return setstringvalue(bsfile, v);
        }
        case fv_folderfrompath: {
            bigstring bspath, bsfolder;
            flnextparamislast = true;
            if (!getstringvalue(hparam1, 1, bspath))
                return false;
            if (!portable_folderfrompath(bspath, bsfolder))
                return false;
            return setstringvalue(bsfolder, v);
        }
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
        case fv_endoffile: {
            tyvaluerecord v1; if (!getparamvalue(hparam1, 1, &v1)) return false;
            if (!coercetofilespec(&v1)) return false;
            fs = **v1.data.filespecvalue;
            (*v).data.flvalue = fifendoffile(&fs);
            return true;
        }
        case fv_getendoffile: {
            long eof = 0;
            tyvaluerecord v1; if (!getparamvalue(hparam1, 1, &v1)) return false;
            if (!coercetofilespec(&v1)) return false;
            fs = **v1.data.filespecvalue;
            if (!fifgetendoffile(&fs, &eof)) return false;
            return setlongvalue(eof, v);
        }
        case fv_setendoffile: {
            long eof = 0;
            tyvaluerecord v1; if (!getparamvalue(hparam1, 1, &v1)) return false;
            if (!coercetofilespec(&v1)) return false;
            fs = **v1.data.filespecvalue;
            flnextparamislast = true;
            if (!getlongvalue(hparam1, 2, &eof)) return false;
            (*v).data.flvalue = fifsetendoffile(&fs, eof);
            return true;
        }
        case fv_setposition: {
            long pos = 0;
            tyvaluerecord v1; if (!getparamvalue(hparam1, 1, &v1)) return false;
            if (!coercetofilespec(&v1)) return false;
            fs = **v1.data.filespecvalue;
            flnextparamislast = true;
            if (!getlongvalue(hparam1, 2, &pos)) return false;
            (*v).data.flvalue = fifsetposition(&fs, pos);
            return true;
        }
        case fv_getposition: {
            long pos = 0;
            tyvaluerecord v1; if (!getparamvalue(hparam1, 1, &v1)) return false;
            if (!coercetofilespec(&v1)) return false;
            fs = **v1.data.filespecvalue;
            if (!fifgetposition(&fs, &pos)) return false;
            return setlongvalue(pos, v);
        }
        case fv_readline: {
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
        default:
            /* All other file verbs not yet implemented in headless */
            copystring(BIGSTRING("\pfile verb not implemented in headless mode"), bserror);
            return false;
    }
    return false;
}

boolean fileinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pfile"), bsname);

    if (!newfunctionprocessor(bsname, &fv_valueproc, false, &htable))
        return false;

    pushhashtable(htable);

    /* Register all 86 verbs as defined in kernelverbs.rc (EFP 1007) */
    #define ADD_VERB(name, tok) do { \
        bigstring bs; \
        copystring(name, bs); \
        if (!langaddkeyword(bs, tok)) { \
            pophashtable(); \
            return false; \
        } \
    } while(0)

    ADD_VERB(BIGSTRING("\pcreated"), fv_created);
    ADD_VERB(BIGSTRING("\pmodified"), fv_modified);
    ADD_VERB(BIGSTRING("\ptype"), fv_type);
    ADD_VERB(BIGSTRING("\pcreator"), fv_creator);
    ADD_VERB(BIGSTRING("\psetcreated"), fv_setcreated);
    ADD_VERB(BIGSTRING("\psetmodified"), fv_setmodified);
    ADD_VERB(BIGSTRING("\psettype"), fv_settype);
    ADD_VERB(BIGSTRING("\psetcreator"), fv_setcreator);
    ADD_VERB(BIGSTRING("\pisfolder"), fv_isfolder);
    ADD_VERB(BIGSTRING("\pisvolume"), fv_isvolume);
    ADD_VERB(BIGSTRING("\pislocked"), fv_islocked);
    ADD_VERB(BIGSTRING("\plock"), fv_lock);
    ADD_VERB(BIGSTRING("\punlock"), fv_unlock);
    ADD_VERB(BIGSTRING("\pcopy"), fv_copy);
    ADD_VERB(BIGSTRING("\pcopydatafork"), fv_copydatafork);
    ADD_VERB(BIGSTRING("\pcopyresourcefork"), fv_copyresourcefork);
    ADD_VERB(BIGSTRING("\pdelete"), fv_delete);
    ADD_VERB(BIGSTRING("\prename"), fv_rename);
    ADD_VERB(BIGSTRING("\pexists"), fv_exists);
    ADD_VERB(BIGSTRING("\psize"), fv_size);
    ADD_VERB(BIGSTRING("\pfullpath"), fv_fullpath);
    ADD_VERB(BIGSTRING("\pgetpath"), fv_getpath);
    ADD_VERB(BIGSTRING("\psetpath"), fv_setpath);
    ADD_VERB(BIGSTRING("\pfilefrompath"), fv_filefrompath);
    ADD_VERB(BIGSTRING("\pfolderfrompath"), fv_folderfrompath);
    ADD_VERB(BIGSTRING("\pgetsystemfolderpath"), fv_getsystemfolderpath);
    ADD_VERB(BIGSTRING("\pgetspecialfolderpath"), fv_getspecialfolderpath);
    ADD_VERB(BIGSTRING("\pnew"), fv_new);
    ADD_VERB(BIGSTRING("\pnewfolder"), fv_newfolder);
    ADD_VERB(BIGSTRING("\pnewalias"), fv_newalias);
    ADD_VERB(BIGSTRING("\pgetfiledialog"), fv_getfiledialog);
    ADD_VERB(BIGSTRING("\pputfiledialog"), fv_putfiledialog);
    ADD_VERB(BIGSTRING("\pgetfolderdialog"), fv_getfolderdialog);
    ADD_VERB(BIGSTRING("\pgetdiskdialog"), fv_getdiskdialog);
    ADD_VERB(BIGSTRING("\pgeticonpos"), fv_geticonpos);
    ADD_VERB(BIGSTRING("\pseticonpos"), fv_seticonpos);
    ADD_VERB(BIGSTRING("\pgetversion"), fv_getversion);
    ADD_VERB(BIGSTRING("\psetversion"), fv_setversion);
    ADD_VERB(BIGSTRING("\pgetfullversion"), fv_getfullversion);
    ADD_VERB(BIGSTRING("\psetfullversion"), fv_setfullversion);
    ADD_VERB(BIGSTRING("\pgetcomment"), fv_getcomment);
    ADD_VERB(BIGSTRING("\psetcomment"), fv_setcomment);
    ADD_VERB(BIGSTRING("\pgetlabel"), fv_getlabel);
    ADD_VERB(BIGSTRING("\psetlabel"), fv_setlabel);
    ADD_VERB(BIGSTRING("\pfindapplication"), fv_findapplication);
    ADD_VERB(BIGSTRING("\pisbusy"), fv_isbusy);
    ADD_VERB(BIGSTRING("\phasbundle"), fv_hasbundle);
    ADD_VERB(BIGSTRING("\psetbundle"), fv_setbundle);
    ADD_VERB(BIGSTRING("\pisalias"), fv_isalias);
    ADD_VERB(BIGSTRING("\pisvisible"), fv_isvisible);
    ADD_VERB(BIGSTRING("\psetvisible"), fv_setvisible);
    ADD_VERB(BIGSTRING("\pfollowlias"), fv_followalias);
    ADD_VERB(BIGSTRING("\pmove"), fv_move);
    ADD_VERB(BIGSTRING("\peject"), fv_eject);
    ADD_VERB(BIGSTRING("\pisejectable"), fv_isejectable);
    ADD_VERB(BIGSTRING("\pfreespaceonvolume"), fv_freespaceonvolume);
    ADD_VERB(BIGSTRING("\pvolumesize"), fv_volumesize);
    ADD_VERB(BIGSTRING("\pvolumeblocksize"), fv_volumeblocksize);
    ADD_VERB(BIGSTRING("\pfilesonvolume"), fv_filesonvolume);
    ADD_VERB(BIGSTRING("\pfoldersonvolume"), fv_foldersonvolume);
    ADD_VERB(BIGSTRING("\punmountvolume"), fv_unmountvolume);
    ADD_VERB(BIGSTRING("\pmountservervolume"), fv_mountservervolume);
    ADD_VERB(BIGSTRING("\pfindinfile"), fv_findinfile);
    ADD_VERB(BIGSTRING("\pcountlines"), fv_countlines);
    ADD_VERB(BIGSTRING("\popen"), fv_open);
    ADD_VERB(BIGSTRING("\pclose"), fv_close);
    ADD_VERB(BIGSTRING("\pendoffile"), fv_endoffile);
    ADD_VERB(BIGSTRING("\psetendoffile"), fv_setendoffile);
    ADD_VERB(BIGSTRING("\pgetendoffile"), fv_getendoffile);
    ADD_VERB(BIGSTRING("\psetposition"), fv_setposition);
    ADD_VERB(BIGSTRING("\pgetposition"), fv_getposition);
    ADD_VERB(BIGSTRING("\preadline"), fv_readline);
    ADD_VERB(BIGSTRING("\pwriteline"), fv_writeline);
    ADD_VERB(BIGSTRING("\pread"), fv_read);
    ADD_VERB(BIGSTRING("\pwrite"), fv_write);
    ADD_VERB(BIGSTRING("\pcompare"), fv_compare);
    ADD_VERB(BIGSTRING("\pwritewholefile"), fv_writewholefile);
    ADD_VERB(BIGSTRING("\pgetpathchar"), fv_getpathchar);
    ADD_VERB(BIGSTRING("\pfreespaceonvolumedouble"), fv_freespaceonvolumedouble);
    ADD_VERB(BIGSTRING("\pvolumesizedouble"), fv_volumesizedouble);
    ADD_VERB(BIGSTRING("\pgetmp3info"), fv_getmp3info);
    ADD_VERB(BIGSTRING("\preadwholefile"), fv_readwholefile);
    ADD_VERB(BIGSTRING("\pgetlabelindex"), fv_getLabelIndex);
    ADD_VERB(BIGSTRING("\psetlabelindex"), fv_setLabelIndex);
    ADD_VERB(BIGSTRING("\pgetlabelnames"), fv_getLabelNames);
    ADD_VERB(BIGSTRING("\pgetposixpath"), fv_getPosixPath);

    #undef ADD_VERB

    pophashtable();
    return true;
}
