
/*	$Id$    */

/*
    SPDX-License-Identifier: MIT

    Copyright (c) 1992-2004 UserLand Software, Inc.
    Copyright (c) 2025-present Frontier contributors

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#ifndef langdllinclude
#define langdllinclude

#ifndef FDLLCALL_H
#include "FDllCall.h"
#endif


Handle xCALLBACK extfrontierReAlloc (Handle h, long sz);
Handle xCALLBACK extfrontierAlloc (long sz);
char * xCALLBACK extfrontierLock (Handle h);
void xCALLBACK extfrontierFree (Handle h);
long xCALLBACK extfrontierSize (Handle h);
void xCALLBACK extfrontierUnlock (Handle h);

odbRef xCALLBACK extOdbGetCurrentRoot (void);
odbBool xCALLBACK extOdbNewFile (hdlfilenum);
odbBool xCALLBACK extOdbOpenFile (hdlfilenum, odbRef *odb);
odbBool xCALLBACK extOdbSaveFile (odbRef odb);
odbBool xCALLBACK extOdbCloseFile (odbRef odb);
odbBool xCALLBACK extOdbDefined (odbRef odb, odbString bspath);
odbBool xCALLBACK extOdbDelete (odbRef odb, odbString bspath);
odbBool xCALLBACK extOdbGetType (odbRef odb, odbString bspath, OSType *type);
odbBool xCALLBACK extOdbCountItems (odbRef odb, odbString bspath, long *count);
odbBool xCALLBACK extOdbGetNthItem (odbRef odb, odbString bspath, long n, odbString bsname);
odbBool xCALLBACK extOdbGetValue (odbRef odb, odbString bspath, odbValueRecord *value);
odbBool xCALLBACK extOdbSetValue (odbRef odb, odbString bspath, odbValueRecord *value);
odbBool xCALLBACK extOdbNewTable (odbRef odb, odbString bspath);
odbBool xCALLBACK extOdbGetModDate (odbRef odb, odbString bspath, unsigned long *date);
void xCALLBACK extOdbDisposeValue (odbRef odb, odbValueRecord *value);
void xCALLBACK extOdbGetError (odbString bs);

odbBool xCALLBACK extDoScript (char * script, long len, odbValueRecord *value);
odbBool xCALLBACK extDoScriptText (char * script, long len, Handle * text);

odbBool xCALLBACK extOdbNewListValue (odbRef odb, odbValueRecord *valueList, odbBool flRecord);
odbBool xCALLBACK extOdbGetListCount (odbRef odb, odbValueRecord *valueList, long * cnt);

// 2006-04-04 - kw --- removed parameter names
// odbBool xCALLBACK extOdbDeleteListValue (odbRef odb, odbValueRecord *valueList, long index, char * recordname);
odbBool xCALLBACK extOdbDeleteListValue (odbRef, odbValueRecord *, long, char *);

// odbBool xCALLBACK extOdbSetListValue (odbRef odb, odbValueRecord *valueList, long index, char * recordname, odbValueRecord *valueData);
odbBool xCALLBACK extOdbSetListValue (odbRef, odbValueRecord *, long, char *, odbValueRecord *);
// odbBool xCALLBACK extOdbGetListValue (odbRef odb, odbValueRecord *valueList, long index, char * recordname, odbValueRecord *valueReturn);
odbBool xCALLBACK extOdbGetListValue (odbRef, odbValueRecord *, long, char *, odbValueRecord *);

odbBool xCALLBACK extOdbAddListValue (odbRef odb, odbValueRecord *valueList, char * recordname, odbValueRecord *valueData);

odbBool xCALLBACK extInvoke (bigstring bsscriptname, void * pDispParams, odbValueRecord * retval, boolean *flfoundhandler, unsigned int * errarg);
odbBool xCALLBACK extCoerce (odbValueRecord * odbval, odbValueType newtype);

odbBool xCALLBACK extCallScript (odbString bspath, odbValueRecord *vparams, odbValueRecord *value); /* 2002-10-13 AR */
odbBool xCALLBACK extCallScriptText (odbString bspath, odbValueRecord *vparams, Handle * text); /* 2002-10-13 AR */

odbBool xCALLBACK extThreadYield (void); /* 2003-04-22 AR */
odbBool xCALLBACK extThreadSleep (long sleepticks); /* 2003-04-22 AR */

extern void dllinitverbs (void); /*2004-11-29 aradke*/

extern void fillcalltable (XDLLProcTable *);

extern boolean dllisloadedverb (hdltreenode hparam1, tyvaluerecord *vreturned);

extern boolean dllloadverb (hdltreenode hparam1, tyvaluerecord *vreturned);

extern boolean dllunloadverb (hdltreenode hparam1, tyvaluerecord *vreturned);

extern boolean dllcallverb (hdltreenode hparam1, tyvaluerecord *vreturned);

#endif

