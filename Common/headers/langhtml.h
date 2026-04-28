
/*	$Id$    */

/*
    SPDX-License-Identifier: MIT

    Copyright (c) 1992-2004 UserLand Software, Inc.
    Copyright (c) 2025-2026 Frontier contributors

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

#define langhtmlinclude

boolean processhtmlmacrosverb (hdltreenode hparam1, tyvaluerecord *vreturned);

boolean urldecodeverb (hdltreenode hparam1, tyvaluerecord *vreturned);

boolean urlencodeverb (hdltreenode hparam1, tyvaluerecord *vreturned);

boolean parseargsverb (hdltreenode hparam1, tyvaluerecord *vreturned);

boolean iso8859encodeverb (hdltreenode hparam1, tyvaluerecord *vreturned);

boolean getgifheightwidthverb (hdltreenode hparam1, tyvaluerecord *vreturned);

boolean getjpegheightwidthverb (hdltreenode hparam1, tyvaluerecord *vreturned);

boolean expandurlsverb (hdltreenode hparam1, tyvaluerecord *vreturned);

boolean htmlneutermacrosverb (hdltreenode hparam1, tyvaluerecord *vreturned);

boolean htmlneutertagsverb (hdltreenode hparam1, tyvaluerecord *vreturned);

/* Phase 2 HTML verbs */
boolean buildpagetableverb (hdltreenode hp1, tyvaluerecord *v);

boolean getprefverb (hdltreenode hp1, tyvaluerecord *v);

boolean rundirectiveverb (hdltreenode hp1, tyvaluerecord *v);

boolean rundirectivesverb (hdltreenode hp1, tyvaluerecord *v);

boolean runoutlinedirectivesverb (hdltreenode hp1, tyvaluerecord *v);

boolean cleanforexportverb (hdltreenode hp1, tyvaluerecord *v);

boolean glossarypatcherverb (hdltreenode hp1, tyvaluerecord *v);

boolean traversalskipverb (hdltreenode hp1, tyvaluerecord *v);

boolean getpagetableaddressverb (hdltreenode hp1, tyvaluerecord *v);

boolean stripmarkupverb (hdltreenode hp1, tyvaluerecord *v);

/* inetd verbs - exposed for headless mode */
boolean inetdsupervisor (long stream, long refcon, tyvaluerecord *vreturn);

/* webserver verbs - exposed for headless mode */
boolean webserverserver (tyaddress *pta, Handle hrequest, tyvaluerecord *vreturn);
boolean webserverdispatch (tyaddress *pta, tyvaluerecord *vreturn);
boolean webserverparseheaders (Handle htext, hdlhashtable hheadertable, Handle *hptr);
boolean webserverparsecookies (hdlhashtable hparamtable, tyvaluerecord *vreturn);
boolean webserverbuildresponse (bigstring bscode, hdlhashtable hheaderstable, Handle hbody, tyvaluerecord *vreturn);
boolean webserverbuilderrorpage (Handle hshort, Handle hlong, Handle *hpage);
boolean webservergetserverstring (tyvaluerecord *vreturn);
