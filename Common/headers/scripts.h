
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

#define scriptsinclude

#ifndef opinclude

	#include "op.h"

#endif


#define typeLAND 'LAND'


/*prototypes*/

extern boolean scriptbuildtree (Handle, long, hdltreenode *);

extern boolean scriptrunstartupscripts (void);

extern boolean scriptrunsuspendscripts (void);

extern boolean scriptrunresumescripts (void);

extern long specialoneshotscriptsrunning (void);

//extern boolean scriptinstallscripts (hdlhashtable);

extern boolean scriptinstallagent (hdlhashnode);

extern boolean scriptremoveagent (hdlhashnode);

extern boolean loadsystemscripts (void);

extern boolean runshutdownscripts (void);

extern boolean scriptinmenubar (void);

extern boolean scriptdebugger (hdltreenode);

extern boolean scriptkilled (void);

extern boolean scriptpushsourcecode (hdlhashtable, hdlhashnode, bigstring);

extern boolean scriptpopsourcecode (void);

extern boolean scriptpushtable (hdlhashtable *);

extern boolean scriptpoptable (hdlhashtable);

extern boolean scriptgetdebuggingcontext (hdlhashtable *);

extern void scriptunlockdebuggingcontext (void);

extern boolean scriptzoomwindow (Rect, Rect, hdlheadrecord, WindowPtr *);

extern void scriptsetcallbacks (hdloutlinerecord);

extern boolean scriptsetdata (WindowPtr, hdlheadrecord, hdloutlinerecord);

extern boolean scriptbackgroundtask (boolean);

extern boolean scriptgettypename (long, bigstring);

extern boolean scriptgetnametype (bigstring, long *);

extern boolean scriptstart (void);

extern boolean initscripts (void);




