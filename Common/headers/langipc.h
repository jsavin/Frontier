
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

#define langipcinclude


#ifndef langinclude

	#include "lang.h"

#endif

#ifndef __APPLEEVENTS__
#if !defined(FRONTIER_PORTABLE) && !defined(FRONTIER_HEADLESS)
    /* Only include classic AppleEvents for non-portable builds */
    #include <AppleEvents.h>
#endif
/* For portable/headless: Types are defined in osincludes_portable.h */
#endif

/*types*/

typedef short tyipcmessageflags;


enum { /*ipcmessageflags*/
	
	normalmsg = 0,
	
	noreplymsg = 0x01,
	
	transactionmsg = 0x02,
	
	microsoftmsg = 0x04,
	
	systemmsg = 0x08
	};


/*globals*/


	extern typrocessid langipcself;



/*prototypes*/

extern boolean langipcerrorroutine (bigstring, ptrvoid); /*langipc.c*/

extern boolean setdescriptorvalue (AEDesc, tyvaluerecord *);

extern boolean valuetodescriptor (tyvaluerecord *, AEDesc *);

#ifdef landinclude

extern boolean langipcbuildparamlist (hdltreenode, hdlverbrecord, hdltreenode *);

extern boolean langipcpushparam (tyvaluerecord *, typaramkeyword, hdlverbrecord);

#endif

extern boolean langipcfindapptable (OSType , boolean, hdlhashtable *, bigstring);

extern boolean langipcbrowsenetwork (hdltreenode, tyvaluerecord *);

extern boolean langipcsettimeout (hdltreenode, tyvaluerecord *);

extern boolean langipcsettransactionid (hdltreenode, tyvaluerecord *);

extern boolean langipcsetinteractionlevel (hdltreenode, tyvaluerecord *);

extern boolean langipcgeteventattr (hdltreenode, tyvaluerecord *);

extern boolean langipccoerceappleitem (hdltreenode, tyvaluerecord *);

extern boolean langipcapprunning (hdltreenode, tyvaluerecord *);

extern boolean langipcgetaddressvalue (hdltreenode, tyvaluerecord *);

extern void binarytodesc (Handle, AEDesc *);

extern boolean langipcconvertoplist (const tyvaluerecord *, AEDesc *);

extern boolean langipcconvertaelist (const AEDesc *, tyvaluerecord *);

extern boolean langipcputlistitem (hdltreenode, tyvaluerecord *);

extern boolean langipcgetlistitem (hdltreenode, tyvaluerecord *);

extern boolean langipccountlistitems (hdltreenode, tyvaluerecord *);

extern boolean newselfaddressedevent (AEEventID id, AppleEvent *event);

extern boolean langipcmessage (hdltreenode, tyipcmessageflags, tyvaluerecord *);

extern boolean langipccomplexmessage (hdltreenode, tyvaluerecord *);

extern boolean langipctablemessage (hdltreenode, tyvaluerecord *);

extern boolean langipcbuildsubroutineevent (AppleEvent *, bigstring, hdltreenode);

extern boolean langipchandlercall (hdltreenode, bigstring, hdltreenode, tyvaluerecord *);

extern boolean langipckernelfunction (hdlhashtable, bigstring, hdltreenode, tyvaluerecord *);

extern boolean langipcshowmenunode (long);

extern boolean langipcnoop (void);

extern boolean langipcstart (void);

extern void langipcshutdown (void);

extern boolean langipcinit (void);


extern boolean langipcgetmenuhandle (OSType, short, Handle *); /*langipcmenus.c*/


extern boolean langipcgetitemlangtext (long, short, short, Handle *, long *);

extern boolean langipccheckformulas (long);

extern void langipcdisposemenuarray (long, Handle);


extern boolean langipcrunitem (long, short, short, long *);

extern boolean langipckillscript (long);

extern boolean langipcgetmenuarray (long, short, boolean, Handle *);

extern boolean langipcmenustartup (void);

extern boolean langipcmenushutdown (void);

extern boolean langipcsymbolchanged (hdlhashtable, const bigstring, boolean);

extern boolean langipcsymbolinserted (hdlhashtable, const bigstring);

extern boolean langipcsymboldeleted (hdlhashtable, const bigstring);

extern boolean langipcmenuinit (void);

