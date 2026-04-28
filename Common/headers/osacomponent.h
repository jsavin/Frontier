
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

#ifndef osacomponentinclude
#define osacomponentinclude

#ifdef FRONTIER_PORTABLE
    /* Skip Component Manager & OSA in portable core */
    #if !defined(OS_PORTABLE_HAS_COMPONENT_TYPES)
        typedef void* Component;
        typedef void* ComponentInstance;
    #endif
    #if !defined(OS_PORTABLE_HAS_APPLEEVENT)
        typedef void* AppleEvent;
    #endif
#else
    #ifndef __COMPONENTS__
        #if !defined(FRONTIER_HEADLESS)
        #include <Components.h>
        #endif
    #endif
    #ifndef __OSA__
        #if !defined(FRONTIER_HEADLESS)
        #include <OSA.h>
        #endif
    #endif
#endif

/*globals*/

extern Component osacomponent;


/*prototypes*/

extern boolean havecomponentmanager (void);

extern boolean osagetcode (Handle, OSType, boolean, tyvaluerecord *);

extern boolean osagetsource (const tyvaluerecord *, OSType *, tyvaluerecord *);

extern boolean isosascriptnode (hdltreenode, tyvaluerecord *);

extern ComponentInstance getosaserver (OSType);

extern boolean evaluateosascript (const tyvaluerecord *, hdltreenode, bigstring, tyvaluerecord *);

extern boolean evaluateosascriptevent (const tyvaluerecord *, const AppleEvent *, AppleEvent *);

extern boolean osacomponentstart (void);

extern boolean osacomponentverifyshutdown (void);

extern void osacomponentshutdown (void);


#endif



