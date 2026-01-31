
/*	$Id$    */

/******************************************************************************

    UserLand Frontier(tm) -- High performance Web content management,
    object database, system-level and Internet scripting environment,
    including source code editing and debugging.

    Copyright (C) 1992-2004 UserLand Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

******************************************************************************/

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
