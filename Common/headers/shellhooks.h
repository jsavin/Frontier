
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

#ifndef shellhooksinclude
#define shellhooksinclude

#ifndef processinclude
	#include "process.h"
#endif


/*types*/

typedef boolean (*menuhookcallback) (short, short);

typedef boolean (*eventhookcallback) (EventRecord *, WindowPtr);

typedef boolean (*errorhookcallback) (bigstring);

typedef boolean (*scraphookcallback) (Handle);

typedef boolean (*memoryhookcallback) (long *);

typedef boolean (*wakeuphookcallback) (struct tythreadglobals **);


/*globals*/

#define maxerrorhooks 5

extern short cterrorhooks;

extern errorhookcallback errorhooks [maxerrorhooks];


/*prototypes*/

extern boolean shellpushkeyboardhook (callback);

extern boolean shellcallkeyboardhooks (void);

extern boolean shellpushdirtyhook (callback);

extern boolean shellcalldirtyhooks (void);

extern boolean shellpushmenuhook (menuhookcallback);

extern boolean shellcallmenuhooks (short, short);

extern boolean shellpusheventhook (eventhookcallback);

extern boolean shellpopeventhook (void);

extern boolean shellcalleventhooks (EventRecord *, WindowPtr);

extern boolean shellpusherrorhook (errorhookcallback);

extern boolean shellpoperrorhook (void);

extern boolean shellcallerrorhooks (bigstring);

extern boolean shellpushscraphook (scraphookcallback);

extern boolean shellcallscraphooks (Handle);

extern boolean shellpushmemoryhook (memoryhookcallback);

extern boolean shellcallmemoryhooks (long *);

extern boolean shellpushfilehook (callback);

extern boolean shellcallfilehooks (void);

extern boolean shellpushwakeuphook (wakeuphookcallback);

extern boolean shellcallwakeuphooks (hdlprocessthread);


#endif /*shellhooksinclude*/

