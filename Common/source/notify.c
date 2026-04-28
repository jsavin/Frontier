
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

#include "standard.h"

#include "notify.h"

boolean notifyuser (bigstring bsmessage) {
    
    /*
     use the Notification Manager to ask the user to bring our app to the front.
     
     note that we must allocate the record in the heap because multi-threading
     makes stack addresses non-persistent.
     
     1/18/93 dmb: langbackgroundtask now takes flresting parameter; don't set global
     
     6/9/93 dmb: don't ignore the result of the background callbacks
     
     2.1b5 dmb: if we're in the main thread, need to do same as if yield is disabled.
     
     7.0d6 PBS: In Pike, the header of the dialog should not read UserLand Frontier,
     it should be UserLand [Whatever]. At this writing, [Whatever] is still undefined,
     so we'll go with Whatever for the moment.
     */
    
    SInt16 itemhit = 0;
    OSErr err = noErr;
    
    err = StandardAlert (kAlertNoteAlert, bsmessage, nil, nil, &itemhit);
    
    return (err == noErr);
    
} /*notifyuser*/






